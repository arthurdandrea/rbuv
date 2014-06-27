#include "rbuv_stream.h"

struct rbuv_stream_s {
  uv_stream_t *uv_handle;
  VALUE loop;
  VALUE cb_on_close;
  VALUE cb_on_connection;
  VALUE cb_on_read;
  VALUE cbs_on_write;
};

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
  rbuv_stream_t *rbuv_stream;
  VALUE cb_on_write;
} rbuv_write_req_t;

typedef struct {
  uv_stream_t *uv_stream;
  int status;
} _uv_stream_on_connection_arg_t;

typedef struct {
  uv_stream_t *uv_stream;
  ssize_t nread;
  char *base;
} _uv_stream_on_read_arg_t;

typedef struct {
  uv_stream_t *uv_stream;
  int status;
  VALUE cb_on_write;
  rbuv_stream_t *rbuv_stream;
} _uv_stream_on_write_arg_t;

VALUE cRbuvStream;

/* Methods */
static VALUE rbuv_stream_listen(VALUE self, VALUE backlog);
static VALUE rbuv_stream_accept(VALUE self, VALUE client);
static VALUE rbuv_stream_is_readable(VALUE self);
static VALUE rbuv_stream_is_writable(VALUE self);
static VALUE rbuv_stream_shutdown(VALUE self);
static VALUE rbuv_stream_read_start(VALUE self);
//static VALUE rbuv_stream_read2_start(VALUE self, VALUE client);
static VALUE rbuv_stream_read_stop(VALUE self);
static VALUE rbuv_stream_write(VALUE self, VALUE data);
//static VALUE rbuv_stream_write2(VALUE self);

/* Private methods */
static void _uv_stream_on_connection(uv_stream_t *server, int status);
static void _uv_stream_on_connection_no_gvl(_uv_stream_on_connection_arg_t *arg);
void __uv_stream_on_connection_no_gvl(uv_stream_t *stream, int status);
static uv_buf_t _uv_alloc_buffer(uv_handle_t *handle, size_t suggested_size);
static void _uv_stream_on_read(uv_stream_t *stream, ssize_t nread, uv_buf_t buf);
static void _uv_stream_on_read_no_gvl(_uv_stream_on_read_arg_t *arg);
static void _uv_stream_on_write(uv_write_t *req, int status);
static void _uv_stream_on_write_no_gvl(_uv_stream_on_write_arg_t *arg);
static VALUE _rbuv_ary_delete_same_object(VALUE ary, VALUE obj);
static VALUE _rbuv_ary_new_from_2_args(VALUE elem1, VALUE elem2);

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

VALUE rbuv_stream_listen(VALUE self, VALUE backlog) {
  rbuv_stream_t *rbuv_server;
  VALUE block;
  int uv_backlog;

  rb_need_block();
  block = rb_block_proc();

  Data_Get_Struct(self, rbuv_stream_t, rbuv_server);
  rbuv_server->cb_on_connection = block;

  uv_backlog = FIX2INT(backlog);

  RBUV_DEBUG_LOG_DETAIL("self: %s, backlog: %d, block: %s, rbuv_server: %p, "
                        "uv_handle: %p, _uv_stream_on_connection: %p",
                        RSTRING_PTR(rb_inspect(self)),
                        uv_backlog,
                        RSTRING_PTR(rb_inspect(block)),
                        rbuv_server,
                        rbuv_server->uv_handle,
                        _uv_stream_on_connection);
  RBUV_CHECK_UV_RETURN(uv_listen(rbuv_server->uv_handle, uv_backlog, _uv_stream_on_connection), rbuv_server->uv_handle->loop);

  return self;
}

VALUE rbuv_stream_accept(VALUE self, VALUE client) {
  rbuv_stream_t *rbuv_server;
  rbuv_stream_t *rbuv_client;

  Data_Get_Struct(self, rbuv_stream_t, rbuv_server);
  Data_Get_Struct(client, rbuv_stream_t, rbuv_client);

  RBUV_CHECK_UV_RETURN(uv_accept(rbuv_server->uv_handle, rbuv_client->uv_handle), rbuv_client->uv_handle->loop);

  return self;
}

VALUE rbuv_stream_is_readable(VALUE self) {
  rbuv_stream_t *rbuv_stream;

  Data_Get_Struct(self, rbuv_stream_t, rbuv_stream);

  return uv_is_readable(rbuv_stream->uv_handle) ? Qtrue : Qfalse;
}

VALUE rbuv_stream_is_writable(VALUE self) {
  rbuv_stream_t *rbuv_stream;

  Data_Get_Struct(self, rbuv_stream_t, rbuv_stream);

  return uv_is_writable(rbuv_stream->uv_handle) ? Qtrue : Qfalse;
}

VALUE rbuv_stream_shutdown(VALUE self) {
  rbuv_stream_t *rbuv_stream;

  Data_Get_Struct(self, rbuv_stream_t, rbuv_stream);

  rb_raise(rb_eNotImpError, __func__);

  return self;
}

VALUE rbuv_stream_read_start(VALUE self) {
  rbuv_stream_t *rbuv_stream;
  VALUE block;

  rb_need_block();
  block = rb_block_proc();

  Data_Get_Struct(self, rbuv_stream_t, rbuv_stream);
  rbuv_stream->cb_on_read = block;

  uv_read_start(rbuv_stream->uv_handle, _uv_alloc_buffer, _uv_stream_on_read);

  return self;
}

VALUE rbuv_stream_read_stop(VALUE self) {
  rbuv_stream_t *rbuv_stream;

  Data_Get_Struct(self, rbuv_stream_t, rbuv_stream);

  uv_read_stop(rbuv_stream->uv_handle);

  return self;
}

VALUE rbuv_stream_write(VALUE self, VALUE data) {
  rbuv_stream_t *rbuv_stream;
  rbuv_write_req_t *req;
  rb_need_block();
  VALUE block = rb_block_proc();

  Data_Get_Struct(self, rbuv_stream_t, rbuv_stream);

  RBUV_DEBUG_LOG_DETAIL("self: %s, block: %s, rbuv_server: %p, uv_handle: %p",
                        RSTRING_PTR(rb_inspect(self)),
                        RSTRING_PTR(rb_inspect(block)),
                        rbuv_stream,
                        rbuv_stream->uv_handle);


  req = malloc(sizeof(*req));
  req->cb_on_write = rb_block_proc();
  switch(TYPE(rbuv_stream->cbs_on_write)) {
    case T_DATA: // Proc
      rbuv_stream->cbs_on_write = _rbuv_ary_new_from_2_args(req->cb_on_write, rbuv_stream->cbs_on_write);
      break;
    case T_NIL:
      rbuv_stream->cbs_on_write = req->cb_on_write;
      break;
    case T_ARRAY:
      rb_ary_push(rbuv_stream->cbs_on_write, req->cb_on_write);
      break;
    default:
      rb_raise(rb_eTypeError, "not valid value");
      return Qnil;
  }
  RBUV_DEBUG_LOG_DETAIL("cbs_on_write: %s",
                        RSTRING_PTR(rb_inspect(rbuv_stream->cbs_on_write)));
  req->rbuv_stream = rbuv_stream;

  req->buf = uv_buf_init((char *)malloc(RSTRING_LEN(data)), (unsigned int)RSTRING_LEN(data));
  memcpy(req->buf.base, RSTRING_PTR(data), RSTRING_LEN(data));

  RBUV_CHECK_UV_RETURN(uv_write(&req->req, rbuv_stream->uv_handle, &req->buf, 1, _uv_stream_on_write), rbuv_stream->uv_handle->loop);

  return self;
}

void _uv_stream_on_connection(uv_stream_t *uv_stream, int status) {
  _uv_stream_on_connection_arg_t arg = { .uv_stream = uv_stream, .status = status };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)_uv_stream_on_connection_no_gvl, &arg);
}

void _uv_stream_on_connection_no_gvl(_uv_stream_on_connection_arg_t *arg) {
  uv_stream_t *uv_stream = arg->uv_stream;
  int status = arg->status;

  __uv_stream_on_connection_no_gvl(uv_stream, status);
}

void __uv_stream_on_connection_no_gvl(uv_stream_t *uv_stream, int status) {
  VALUE stream;
  rbuv_stream_t *rbuv_stream;
  VALUE on_connection;
  VALUE error;
  uv_err_t uv_err;

  RBUV_DEBUG_LOG("uv_stream: %p, status: %d", uv_stream, status);

  stream = (VALUE)uv_stream->data;
  Data_Get_Struct(stream, rbuv_stream_t, rbuv_stream);
  on_connection = rbuv_stream->cb_on_connection;

  RBUV_DEBUG_LOG_DETAIL("stream: %s, on_connection: %s",
                        RSTRING_PTR(rb_inspect(stream)),
                        RSTRING_PTR(rb_inspect(on_connection)));

  if (status == -1) {
    uv_err = uv_last_error(uv_stream->loop);
    RBUV_DEBUG_LOG_DETAIL("uv_stream: %p, status: %d, error: %s", uv_stream, status,
                          uv_strerror(uv_err));
    error = rb_exc_new2(eRbuvError, uv_strerror(uv_err));
    rb_funcall(on_connection, id_call, 2, stream, error);
  } else {
    rb_funcall(on_connection, id_call, 1, stream);
  }
}

uv_buf_t _uv_alloc_buffer(uv_handle_t *handle, size_t suggested_size) {
  return uv_buf_init((char *)malloc(suggested_size), (unsigned int)suggested_size);
}

void _uv_stream_on_read(uv_stream_t *uv_stream, ssize_t nread, uv_buf_t buf) {
  if (nread == 0) {
    if (buf.base != NULL) {
      free(buf.base);
    }
    return;
  } else {
    _uv_stream_on_read_arg_t arg;
    arg.uv_stream = uv_stream;
    arg.nread = nread;

    if (nread < 0) {
      if (buf.base != NULL) {
        free(buf.base);
      }
      arg.base = NULL;
    } else {
      // Here uvrb uses realloc. not sure about that
      // buf.base = realloc(buf.base, nread);
      // should we reassign buf.len = nread?
      arg.base = buf.base;
    }
    rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)
      _uv_stream_on_read_no_gvl, &arg);
  }
}

void _uv_stream_on_read_no_gvl(_uv_stream_on_read_arg_t *arg) {
  uv_stream_t *uv_stream = arg->uv_stream;
  ssize_t nread = arg->nread;
  char *base = arg->base;

  VALUE stream;
  rbuv_stream_t *rbuv_stream;
  VALUE on_read;
  uv_err_t uv_err;
  VALUE error;

  RBUV_DEBUG_LOG("uv_stream: %p, nread: %lu", uv_stream, nread);

  stream = (VALUE)uv_stream->data;
  Data_Get_Struct(stream, rbuv_stream_t, rbuv_stream);
  on_read = rbuv_stream->cb_on_read;
  RBUV_DEBUG_LOG_DETAIL("stream: %s, on_read: %s",
                        RSTRING_PTR(rb_inspect(stream)),
                        RSTRING_PTR(rb_inspect(on_read)));

  if (nread < 0) {
    uv_err = uv_last_error(uv_stream->loop);
    if (uv_err.code == UV_EOF) {
      error = rb_exc_new2(rb_eEOFError, "end of file reached");
    } else {
      error = rb_exc_new2(eRbuvError, uv_strerror(uv_err));
    }
    rb_funcall(on_read, id_call, 2, Qnil, error);
  } else {
    rb_funcall(on_read, id_call, 1, rb_str_new(base, nread));
  }
}

void _uv_stream_on_write(uv_write_t *uv_req, int status) {
  rbuv_write_req_t *rbuv_req = RBUV_CONTAINTER_OF(uv_req, rbuv_write_req_t, req);

  _uv_stream_on_write_arg_t arg = {
    .rbuv_stream = rbuv_req->rbuv_stream,
    .uv_stream = uv_req->handle,
    .status = status,
    .cb_on_write = rbuv_req->cb_on_write};

  if (rbuv_req->buf.base != NULL) {
    free(rbuv_req->buf.base);
  }
  free(rbuv_req);
rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)_uv_stream_on_write_no_gvl, &arg);
}

void _uv_stream_on_write_no_gvl(_uv_stream_on_write_arg_t *arg) {
  VALUE cb_on_write = arg->cb_on_write;
  uv_stream_t *uv_stream = arg->uv_stream;
  rbuv_stream_t *rbuv_stream = arg->rbuv_stream;

  VALUE error = Qnil;
  RBUV_DEBUG_LOG_DETAIL("cbs_on_write: %s",
                      RSTRING_PTR(rb_inspect(rbuv_stream->cbs_on_write)));

  switch(TYPE(rbuv_stream->cbs_on_write)) {
    case T_NIL:
      break;
    case T_ARRAY:
      rbuv_stream->cbs_on_write =  _rbuv_ary_delete_same_object(rbuv_stream->cbs_on_write, cb_on_write);
      break;
    default:
      rbuv_stream->cbs_on_write = Qnil;
      break;
  }

  if ((arg->status < 0) && (error == Qnil)) {
    uv_err_t err = uv_last_error(uv_stream->loop);
    error = rb_exc_new2(eRbuvError, uv_strerror(err));
  }
  RBUV_DEBUG_LOG_DETAIL("cb_on_write: %s, cbs_on_write: %s, error: %s",
                      RSTRING_PTR(rb_inspect(cb_on_write)),
                      RSTRING_PTR(rb_inspect(rbuv_stream->cbs_on_write)),
                      RSTRING_PTR(rb_inspect(error)));

  rb_funcall(cb_on_write, id_call, 1, error);
}


VALUE _rbuv_ary_delete_same_object(VALUE ary, VALUE obj) {
  long i;
  for (i = 0; i < RARRAY_LEN(ary); i++) {
    if (rb_ary_entry(ary, i) == obj) {
      rb_ary_delete_at(ary, i);
      break;
    }
  }
  return ary;
}

VALUE _rbuv_ary_new_from_2_args(VALUE elem1, VALUE elem2) {
#ifdef RBUV_RBX
  VALUE ary = rb_ary_new();
  rb_ary_push(ary, elem1);
  rb_ary_push(ary, elem2);
  return ary;
#else
  return rb_ary_new_from_args(2, elem1, elem2);
#endif
}
