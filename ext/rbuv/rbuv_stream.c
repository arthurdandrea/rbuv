#include "rbuv_stream.h"

struct rbuv_stream_s {
  uv_stream_t *uv_handle;
  VALUE cb_on_close;
  VALUE cb_on_connection;
  VALUE cb_on_read;
  VALUE requests;
};
typedef struct rbuv_stream_s rbuv_stream_t;

typedef struct {
  uv_stream_t *uv_stream;
  int status;
} rbuv_stream_on_connection_arg_t;

typedef struct {
  uv_stream_t *uv_stream;
  ssize_t nread;
  char *base;
} rbuv_stream_on_read_arg_t;

typedef struct {
  uv_write_t *uv_req;
  int status;
} rbuv_stream_on_write_arg_t;

VALUE cRbuvStream;

/* Private methods */
static void rbuv_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
static void rbuv_stream_on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t* buf);
static void rbuv_stream_on_read_no_gvl(rbuv_stream_on_read_arg_t *arg);
static void rbuv_stream_on_write(uv_write_t *req, int status);
static void rbuv_stream_on_write_no_gvl(rbuv_stream_on_write_arg_t *arg);
static void rbuv_stream_on_connection(uv_stream_t *uv_stream, int status);
static void rbuv_stream_on_connection_no_gvl(rbuv_stream_on_connection_arg_t *arg);
static void rbuv_ary_delete_same_object(VALUE ary, VALUE obj);

/* @overload listen(backlog)
 *   Listen for incomining connections
 *
 *   @param backlog [Number]
 *   @yield callback
 *   @yieldparam stream [self]
 *   @yieldparam error [Rbuv::Error, nil]
 *   @return [self] itself
 */
static VALUE rbuv_stream_listen(VALUE self, VALUE backlog) {
  rbuv_stream_t *rbuv_server;
  int uv_backlog;

  rb_need_block();
  Data_Get_Handle_Struct(self, rbuv_stream_t, rbuv_server);
  uv_backlog = FIX2INT(backlog);

  RBUV_DEBUG_LOG_DETAIL("self: %s, backlog: %d, rbuv_server: %p, "
                        "uv_handle: %p, rbuv_stream_on_connection: %p",
                        RSTRING_PTR(rb_inspect(self)),
                        uv_backlog,
                        rbuv_server,
                        rbuv_server->uv_handle,
                        rbuv_stream_on_connection);
  RBUV_CHECK_UV_RETURN(uv_listen(rbuv_server->uv_handle, uv_backlog, rbuv_stream_on_connection));
  rbuv_server->cb_on_connection = rb_block_proc();

  return self;
}

/*
 * @overload accept(client)
 * This call is used in conjunction with {#listen} to accept incoming
 * connections. Call {#accept} after the {#listen} block is called to accept
 * the connection.
 *
 * When the {#listen} block is called it is guaranteed that +accept+ will
 * complete successfully the first time. If you attempt to use it more than
 * once, it may fail. It is suggested to only call {#accept} once per
 * {#listen} block call.
 *
 * @param client [Rbuv::Stream] a fresh stream object to be associated with the
 *   accepted connection
 * @return [self] itself
 */
static VALUE rbuv_stream_accept(VALUE self, VALUE client) {
  rbuv_stream_t *rbuv_server;
  rbuv_stream_t *rbuv_client;

  Data_Get_Handle_Struct(self, rbuv_stream_t, rbuv_server);
  Data_Get_Handle_Struct(client, rbuv_stream_t, rbuv_client);

  RBUV_CHECK_UV_RETURN(uv_accept(rbuv_server->uv_handle, rbuv_client->uv_handle));

  return self;
}

/*
 * Check if this stream is readable.
 * @return [Boolean] +true+ if this stream is readable, +false+ otherwise
 */
static VALUE rbuv_stream_is_readable(VALUE self) {
  rbuv_stream_t *rbuv_stream;

  Data_Get_Handle_Struct(self, rbuv_stream_t, rbuv_stream);

  return uv_is_readable(rbuv_stream->uv_handle) ? Qtrue : Qfalse;
}

/*
 * Check if this stream is writable.
 * @return [Boolean] +true+ if this stream is writable, +false+ otherwise
 */
static VALUE rbuv_stream_is_writable(VALUE self) {
  rbuv_stream_t *rbuv_stream;

  Data_Get_Handle_Struct(self, rbuv_stream_t, rbuv_stream);

  return uv_is_writable(rbuv_stream->uv_handle) ? Qtrue : Qfalse;
}

/*
 * @overload shutdown
 *   Shutdown the outgoing (write) side of a duplex stream. It waits for
 *   pending write requests to complete.
 *   @yield The block is called after shutdown is complete.
 *   @return [self] itself
 *   @todo Implement it
 */
static VALUE rbuv_stream_shutdown(VALUE self) {
  rbuv_stream_t *rbuv_stream;

  Data_Get_Handle_Struct(self, rbuv_stream_t, rbuv_stream);

  rb_raise(rb_eNotImpError, __func__);

  return self;
}

/*
 * Read data from an incoming stream.
 *
 * @yield The block will be called made several times until there is no more
 *   data to read or {#read_stop} is called. When we've reached EOF, +error+
 *   will be set to an instance of +EOFError+.
 * @yieldparam data [String, nil] the readed data or +nil+ if the operation has
 *   not succeded
 * @yieldparam error [Rbuv::Error, EOFError, nil] an Error or +nil+ if the
 *   operation has succeded
 * @return [self] itself
 */
static VALUE rbuv_stream_read_start(VALUE self) {
  rbuv_stream_t *rbuv_stream;
  VALUE block;

  rb_need_block();
  block = rb_block_proc();

  Data_Get_Handle_Struct(self, rbuv_stream_t, rbuv_stream);
  rbuv_stream->cb_on_read = block;

  uv_read_start(rbuv_stream->uv_handle, rbuv_alloc_buffer, rbuv_stream_on_read);

  return self;
}

/* Stop reading data
 * @return [self] itself
 */
static VALUE rbuv_stream_read_stop(VALUE self) {
  rbuv_stream_t *rbuv_stream;

  Data_Get_Handle_Struct(self, rbuv_stream_t, rbuv_stream);

  uv_read_stop(rbuv_stream->uv_handle);

  return self;
}

/* @overload write(data)
 *   Write data to stream. Buffers are written in order.
 *   @example
 *     stream.write("12") do |error| ... end
 *     stream.write("34") do |error|
 *       # if no error has happend
 *       # "1234" has been written to this stream
 *     end
 *   @yield The block is called when the write operation has finished
 *   @yieldparam error [Rbuv::Error, nil] an error if the operation has failed,
 *     otherwise +nil+
 *   @return [Rbuv::Stream::WriteRequest] itself
 */
VALUE rbuv_stream_write(VALUE self, VALUE data) {
  rbuv_stream_t *rbuv_stream;
  rbuv_write_t *rbuv_write;
  int uv_ret;

  if (TYPE(data) != T_STRING) {
    rb_raise(rb_eTypeError, "not valid value, should be a String");
    return Qnil;
  }
  rb_need_block();

  Data_Get_Handle_Struct(self, rbuv_stream_t, rbuv_stream);

  RBUV_DEBUG_LOG_DETAIL("self: %s, rbuv_server: %p, uv_handle: %p",
                        RSTRING_PTR(rb_inspect(self)),
                        rbuv_stream,
                        rbuv_stream->uv_handle);

  rbuv_write = malloc(sizeof(*rbuv_write));
  rbuv_write->uv_req = malloc(sizeof(*rbuv_write->uv_req));
  rbuv_write->uv_buf = uv_buf_init((char *)malloc(sizeof(char) * RSTRING_LEN(data)), (unsigned int)RSTRING_LEN(data));
  rbuv_write->cb_on_write = rb_block_proc();
  memcpy(rbuv_write->uv_buf.base, RSTRING_PTR(data), RSTRING_LEN(data));
  uv_ret = uv_write(rbuv_write->uv_req, rbuv_stream->uv_handle, &rbuv_write->uv_buf, 1, rbuv_stream_on_write);
  if (uv_ret < 0) {
    free(rbuv_write->uv_buf.base);
    rbuv_write->uv_buf.base = NULL;
    free(rbuv_write->uv_req);
    rbuv_write->uv_req = NULL;
    free(rbuv_write);
    rb_raise(eRbuvError, "%s", uv_strerror(uv_ret));
    return Qnil;
  } else {
    VALUE request = Data_Wrap_Struct(cRbuvStreamWriteRequest, rbuv_write_mark, rbuv_write_free, rbuv_write);
    rbuv_write->uv_req->data = (void *)request;
    rb_ary_push(rbuv_stream->requests, request);
    return request;
  }
}

void rbuv_stream_on_connection(uv_stream_t *uv_stream, int status) {
  rbuv_stream_on_connection_arg_t arg = {
    .uv_stream = uv_stream,
    .status = status
  };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)
                          rbuv_stream_on_connection_no_gvl, &arg);
}

void rbuv_stream_on_connection_no_gvl(rbuv_stream_on_connection_arg_t *arg) {
  uv_stream_t *uv_stream = arg->uv_stream;
  int status = arg->status;
  VALUE stream;
  rbuv_stream_t *rbuv_stream;
  VALUE on_connection;
  VALUE error;

  RBUV_DEBUG_LOG("uv_stream: %p, status: %d", uv_stream, status);

  stream = (VALUE)uv_stream->data;
  Data_Get_Handle_Struct(stream, rbuv_stream_t, rbuv_stream);
  on_connection = rbuv_stream->cb_on_connection;

  RBUV_DEBUG_LOG_DETAIL("stream: %s, on_connection: %s",
                        RSTRING_PTR(rb_inspect(stream)),
                        RSTRING_PTR(rb_inspect(on_connection)));

  if (status < 0) {
    RBUV_DEBUG_LOG_DETAIL("uv_stream: %p, status: %d, error: %s", uv_stream, status,
                          uv_strerror(status));
    error = rb_exc_new2(eRbuvError, uv_strerror(status));
  } else {
    error = Qnil;
  }
  rb_funcall(on_connection, id_call, 2, stream, error);
}

void rbuv_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

void rbuv_stream_on_read(uv_stream_t *uv_stream, ssize_t nread, const uv_buf_t *buf) {
  if (nread == 0) {
    if (buf->base != NULL) {
      free(buf->base);
    }
    return;
  } else {
    rbuv_stream_on_read_arg_t arg;
    arg.uv_stream = uv_stream;
    arg.nread = nread;

    if (nread < 0) {
      if (buf->base != NULL) {
        free(buf->base);
      }
      arg.base = NULL;
    } else {
      // Here uvrb uses realloc. not sure about that
      // buf->base = realloc(buf->base, nread);
      // should we reassign buf->len = nread?
      arg.base = buf->base;
    }
    rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)
      rbuv_stream_on_read_no_gvl, &arg);
  }
}

void rbuv_stream_on_read_no_gvl(rbuv_stream_on_read_arg_t *arg) {
  uv_stream_t *uv_stream = arg->uv_stream;
  ssize_t nread = arg->nread;
  char *base = arg->base;

  VALUE stream;
  rbuv_stream_t *rbuv_stream;
  VALUE on_read;
  VALUE error;
  VALUE data;

  RBUV_DEBUG_LOG("uv_stream: %p, nread: %lu", uv_stream, nread);

  stream = (VALUE)uv_stream->data;
  Data_Get_Handle_Struct(stream, rbuv_stream_t, rbuv_stream);
  on_read = rbuv_stream->cb_on_read;
  RBUV_DEBUG_LOG_DETAIL("stream: %s, on_read: %s",
                        RSTRING_PTR(rb_inspect(stream)),
                        RSTRING_PTR(rb_inspect(on_read)));

  if (nread < 0) {
    if (nread == UV_EOF) {
      error = rb_exc_new2(rb_eEOFError, "end of file reached");
    } else {
      error = rb_exc_new2(eRbuvError, uv_strerror(nread));
    }
    data = Qnil;
  } else {
    error = Qnil;
    data = rb_str_new(base, nread);
  }
  rb_funcall(on_read, id_call, 2, data, error);
}

void rbuv_stream_on_write(uv_write_t *uv_req, int status) {
  rbuv_stream_on_write_arg_t arg = {.uv_req = uv_req, .status = status};
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)rbuv_stream_on_write_no_gvl, &arg);
}

void rbuv_stream_on_write_no_gvl(rbuv_stream_on_write_arg_t *arg) {
  rbuv_write_t *rbuv_write;
  rbuv_stream_t *rbuv_stream;
  VALUE request;
  VALUE stream;
  VALUE error;

  request = (VALUE) arg->uv_req->data;
  Data_Get_Struct(request, rbuv_write_t, rbuv_write);
  if (rbuv_write->uv_buf.base != NULL) {
    free(rbuv_write->uv_buf.base);
    rbuv_write->uv_buf.base = NULL;
  }
  if (rbuv_write->uv_req != NULL) {
    free(rbuv_write->uv_req);
    rbuv_write->uv_req = NULL;
  }
  stream = (VALUE) arg->uv_req->handle->data;
  Data_Get_Struct(stream, rbuv_stream_t, rbuv_stream);
  rbuv_ary_delete_same_object(rbuv_stream->requests, request);

  if (arg->status < 0) {
    error = rb_exc_new2(eRbuvError, uv_strerror(arg->status));
  } else {
    error = Qnil;
  }
  // RBUV_DEBUG_LOG_DETAIL("cb_on_write: %s, cbs_on_write: %s, error: %s",
  //                     RSTRING_PTR(rb_inspect(cb_on_write)),
  //                     RSTRING_PTR(rb_inspect(rbuv_stream->cbs_on_write)),
  //                     RSTRING_PTR(rb_inspect(error)));

  rb_funcall(rbuv_write->cb_on_write, id_call, 1, error);
}

void rbuv_ary_delete_same_object(VALUE ary, VALUE obj) {
  long i;
  for (i = 0; i < RARRAY_LEN(ary); i++) {
    if (rb_ary_entry(ary, i) == obj) {
      rb_ary_delete_at(ary, i);
      break;
    }
  }
}

void Init_rbuv_stream() {
  cRbuvStream = rb_define_class_under(mRbuv, "Stream", cRbuvHandle);
  rb_undef_alloc_func(cRbuvStream);

  rb_define_method(cRbuvStream, "listen", rbuv_stream_listen, 1);
  rb_define_method(cRbuvStream, "accept", rbuv_stream_accept, 1);
  rb_define_method(cRbuvStream, "readable?", rbuv_stream_is_readable, 0);
  rb_define_method(cRbuvStream, "writable?", rbuv_stream_is_writable, 0);
  rb_define_method(cRbuvStream, "shutdown", rbuv_stream_shutdown, 0);
  rb_define_method(cRbuvStream, "read_start", rbuv_stream_read_start, 0);
//  rb_define_method(cRbuvStream, "read2_start", rbuv_stream_read2_start, 0);
  rb_define_method(cRbuvStream, "read_stop", rbuv_stream_read_stop, 0);
  rb_define_method(cRbuvStream, "write", rbuv_stream_write, 1);
//  rb_define_method(cRbuvStream, "write2", rbuv_stream_write2, 1);
}

/* This have to be declared after Init_* so it can replace YARD bad assumption
 * for parent class beeing RbuvHandle not Rbuv::Handle.
 * Also it need some text after document-class statement otherwise YARD won't
 * parse it
 */

/*
 * Document-class: Rbuv::Stream < Rbuv::Handle
 * @abstract
 */
