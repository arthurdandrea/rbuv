#include "rbuv_tcp.h"

struct rbuv_tcp_s {
  uv_tcp_t *uv_handle;
  VALUE cb_on_close;
  VALUE cb_on_connection;
  VALUE cb_on_read;
  VALUE requests;
  VALUE cb_on_connect;
};
typedef struct rbuv_tcp_s rbuv_tcp_t;

struct rbuv_tcp_on_connect_arg_s {
  uv_stream_t *uv_handle;
  int status;
};
typedef struct rbuv_tcp_on_connect_arg_s rbuv_tcp_on_connect_arg_t;

VALUE cRbuvTcp;

/* Allocator / Mark / Deallocator */
static VALUE rbuv_tcp_alloc(VALUE klass);
static void rbuv_tcp_mark(rbuv_tcp_t *rbuv_tcp);
static void rbuv_tcp_free(rbuv_tcp_t *rbuv_tcp);

/* Private methods */
static VALUE rbuv_tcp_extractname(struct sockaddr* sockname, int namelen);
static void rbuv_tcp_on_connect(uv_connect_t *uv_connect, int status);
static void rbuv_tcp_on_connect_no_gvl(rbuv_tcp_on_connect_arg_t *arg);

VALUE rbuv_tcp_alloc(VALUE klass) {
  rbuv_tcp_t *rbuv_tcp;

  rbuv_tcp = malloc(sizeof(*rbuv_tcp));
  rbuv_handle_alloc((rbuv_handle_t *)rbuv_tcp);
  rbuv_tcp->requests = rb_ary_new();
  rbuv_tcp->cb_on_connection = Qnil;
  rbuv_tcp->cb_on_read = Qnil;
  rbuv_tcp->cb_on_connect = Qnil;

  return Data_Wrap_Struct(klass, rbuv_tcp_mark, rbuv_tcp_free, rbuv_tcp);
}

void rbuv_tcp_mark(rbuv_tcp_t *rbuv_tcp) {
  assert(rbuv_tcp);
  RBUV_DEBUG_LOG_DETAIL("rbuv_tcp: %p, uv_handle: %p", rbuv_tcp, rbuv_tcp->uv_handle);
  rbuv_handle_mark((rbuv_handle_t *)rbuv_tcp);
  rb_gc_mark(rbuv_tcp->requests);
  rb_gc_mark(rbuv_tcp->cb_on_connection);
  rb_gc_mark(rbuv_tcp->cb_on_read);
  rb_gc_mark(rbuv_tcp->cb_on_connect);
}

void rbuv_tcp_free(rbuv_tcp_t *rbuv_tcp) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_tcp: %p, uv_handle: %p", rbuv_tcp, rbuv_tcp->uv_handle);

  rbuv_handle_free((rbuv_handle_t *)rbuv_tcp);
}

/*
 * @overload initialize(loop=nil)
 *   Create a new handle to deal with a TCP.
 *
 *   @param loop [Rbuv::Loop, nil] loop object where this handle runs, if it is
 *     +nil+ then it the runs the handle in the {Rbuv::Loop.default}
 *   @return [Rbuv::Tcp]
 */
static VALUE rbuv_tcp_initialize(int argc, VALUE *argv, VALUE self) {
  VALUE loop;
  int uv_ret;
  rb_scan_args(argc, argv, "01", &loop);
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }
  rbuv_tcp_t *rbuv_tcp;
  rbuv_loop_t *rbuv_loop;

  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  Data_Get_Struct(self, rbuv_tcp_t, rbuv_tcp);
  rbuv_tcp->uv_handle = malloc(sizeof(*rbuv_tcp->uv_handle));
  uv_ret = uv_tcp_init(rbuv_loop->uv_handle, rbuv_tcp->uv_handle);
  if (uv_ret < 0) {
    free(rbuv_tcp->uv_handle);
    rbuv_tcp->uv_handle = NULL;
    rb_raise(eRbuvError, "%s", uv_strerror(uv_ret));
  }
  rbuv_tcp->uv_handle->data = (void *)self;
  return self;
}

/* @overload enable_nodelay
 * Disable Nagle's algorithm.
 *
 * @return [self] itself
 */
static VALUE rbuv_tcp_enable_nodelay(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_nodelay(rbuv_tcp->uv_handle, 1));
  return self;
}

/* @overload disable_nodelay
 * Enable Nagle's algorithm.
 *
 * @return [self] itself
 */
static VALUE rbuv_tcp_disable_nodelay(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_nodelay(rbuv_tcp->uv_handle, 0));
  return self;
}

/* @overload enable_keepalive(delay)
 * Enable TCP keep-alive.
 *
 * @param delay [Number] the initial delay in seconds
 * @return [self] itself
 */
static VALUE rbuv_tcp_enable_keepalive(VALUE self, VALUE delay) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_keepalive(rbuv_tcp->uv_handle, 1, 1));
  return self;
}

/* @overload disable_keepalive
 * Disable TCP keep-alive.
 *
 * @return [self] itself
 */
static VALUE rbuv_tcp_disable_keepalive(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_keepalive(rbuv_tcp->uv_handle, 0, 0));
  return self;
}

/* @overload enable_simultaneous_accepts
 * Enable simultaneous asynchronous accept requests that are queued by the
 * operating system when listening for new tcp connections.
 * This setting is used to tune a tcp server for the desired performance.
 * Having simultaneous accepts can significantly improve the rate of
 * accepting connections (which is why it is enabled by default) but
 * may lead to uneven load distribution in multi-process setups.
 *
 * @return [self] itself
 */
static VALUE rbuv_tcp_enable_simultaneous_accepts(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_simultaneous_accepts(rbuv_tcp->uv_handle, 1));

  return self;
}

/* @overload disable_simultaneous_accepts
 * Disable simultaneous asynchronous accept requests that are queued by the
 * operating system when listening for new tcp connections.
 * @see #enable_simultaneous_accepts
 *
 * @return [self] itself
 */
static VALUE rbuv_tcp_disable_simultaneous_accepts(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_simultaneous_accepts(rbuv_tcp->uv_handle, 0));
  return self;
}


static VALUE rbuv_tcp_getpeername(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  struct sockaddr peername;
  int namelen = sizeof peername;
  RBUV_CHECK_UV_RETURN(uv_tcp_getpeername(rbuv_tcp->uv_handle, &peername,
                                          &namelen));
  return rbuv_tcp_extractname(&peername, namelen);
}

static VALUE rbuv_tcp_getsockname(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  struct sockaddr sockname;
  int namelen = sizeof sockname;
  RBUV_CHECK_UV_RETURN(uv_tcp_getsockname(rbuv_tcp->uv_handle, &sockname,
                                          &namelen));
  return rbuv_tcp_extractname(&sockname, namelen);
}

/*
 * This call is used in conjunction with {#listen} to accept incoming
 * connections. Call {#accept} after the {#listen} block is called to accept
 * the connection.
 *
 * When the {#listen} block is called it is guaranteed that +accept+ will
 * complete successfully the first time. If you attempt to use it more than
 * once, it may fail. It is suggested to only call {#accept} once per
 * {#listen} block call.
 *
 * @overload accept
 *   @return [Rbuv::Tcp] a new {Rbuv::Tcp} object associated with the accepted
 *     connection
 * @overload accept(client)
 *   @param (see Rbuv::Stream#accept)
 *   @return (see Rbuv::Stream#accept)
 */
static VALUE rbuv_tcp_accept(int argc, VALUE *argv, VALUE self) {
  VALUE client;
  rb_scan_args(argc, argv, "01", &client);
  if (client == Qnil) {
    rbuv_tcp_t *rbuv_tcp;
    VALUE client;
    VALUE loop;

    Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
    loop = (VALUE)rbuv_tcp->uv_handle->loop->data;
    client = rb_class_new_instance(1, &loop, rb_class_of(self));
    rb_call_super(1, &client);
    return client;
  } else {
    return rb_call_super(argc, argv);
  }
}

/* @overload bind(ip, port)
 * Bind this tcp object to the given address and port.
 * @param ip [String] the ip address to bind to
 * @param port [Number] the port to bind to
 * @return [self] itself
 */
static VALUE rbuv_tcp_bind(VALUE self, VALUE ip, VALUE port) {
  const char *uv_ip;
  int uv_port;
  rbuv_tcp_t *rbuv_tcp;
  struct sockaddr_in bind_addr;

  uv_ip = RSTRING_PTR(ip);
  uv_port = FIX2INT(port);

  uv_ip4_addr(uv_ip, uv_port, &bind_addr);

  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_bind(rbuv_tcp->uv_handle, (const struct sockaddr *) &bind_addr, 0));

  RBUV_DEBUG_LOG_DETAIL("self: %s, ip: %s, port: %d, rbuv_tcp: %p, uv_handle: %p",
                        RSTRING_PTR(rb_inspect(self)), uv_ip, uv_port, rbuv_tcp,
                        rbuv_tcp->uv_handle);

  return self;
}

/* @overload connect(ip, port)
 * Connect this tcp object to the given address and port.
 * @param ip [String] the ip address to bind to
 * @param port [Number] the port to bind to
 * @yield callback
 * @yieldparam stream [self]
 * @yieldparam error [Rbuv::Error, nil]
 * @return [self] itself
 */
static VALUE rbuv_tcp_connect(VALUE self, VALUE ip, VALUE port) {
  VALUE block;
  const char *uv_ip;
  int uv_port;
  rbuv_tcp_t *rbuv_tcp;
  struct sockaddr_in connect_addr;
  uv_connect_t *uv_connect;
  int uv_ret;

  rb_need_block();
  block = rb_block_proc();

  uv_ip = RSTRING_PTR(ip);
  uv_port = FIX2INT(port);

  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  rbuv_tcp->cb_on_connect = block;

  uv_ret = uv_ip4_addr(uv_ip, uv_port, &connect_addr);
  if (uv_ret < 0) {
    rb_raise(eRbuvError, "%s", uv_strerror(uv_ret));
    return Qnil;
  }
  RBUV_DEBUG_LOG_DETAIL("self: %s, ip: %s, port: %d, rbuv_tcp: %p, uv_handle: %p",
                        RSTRING_PTR(rb_inspect(self)), uv_ip, uv_port, rbuv_tcp,
                        rbuv_tcp->uv_handle);

  uv_connect = malloc(sizeof(*uv_connect));
  uv_ret = uv_tcp_connect(uv_connect, rbuv_tcp->uv_handle,
                                      (const struct sockaddr *) &connect_addr,
                                      rbuv_tcp_on_connect);
  if (uv_ret < 0) {
    free(uv_connect);
    uv_connect = NULL;
    rb_raise(eRbuvError, "%s", uv_strerror(uv_ret));
    return Qnil;
  }
  RBUV_DEBUG_LOG_DETAIL("self: %s, ip: %s, port: %d, rbuv_tcp: %p, uv_handle: %p",
                        RSTRING_PTR(rb_inspect(self)), uv_ip, uv_port, rbuv_tcp,
                        rbuv_tcp->uv_handle);

  return self;
}

void rbuv_tcp_on_connect(uv_connect_t *uv_connect, int status) {
  rbuv_tcp_on_connect_arg_t arg = {
    .uv_handle = uv_connect->handle,
    .status = status
  };
  free(uv_connect);
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)
                          rbuv_tcp_on_connect_no_gvl, &arg);
}

void rbuv_tcp_on_connect_no_gvl(rbuv_tcp_on_connect_arg_t *arg) {
  uv_stream_t *uv_stream = arg->uv_handle;
  int status = arg->status;

  rbuv_tcp_t *rbuv_tcp;
  VALUE tcp;
  VALUE on_connect;
  VALUE error;

  RBUV_DEBUG_LOG("uv_stream: %p, status: %d", uv_stream, status);

  tcp = (VALUE)uv_stream->data;
  Data_Get_Handle_Struct(tcp, rbuv_tcp_t, rbuv_tcp);
  on_connect = rbuv_tcp->cb_on_connect;
  rbuv_tcp->cb_on_connect = Qnil;

  RBUV_DEBUG_LOG_DETAIL("tcp: %s, on_connect: %s",
                        RSTRING_PTR(rb_inspect(tcp)),
                        RSTRING_PTR(rb_inspect(on_connect)));

  if (status < 0) {
    RBUV_DEBUG_LOG_DETAIL("uv_stream: %p, status: %d, error: %s", uv_stream, status,
                          uv_strerror(status));
    error = rb_exc_new2(eRbuvError, uv_strerror(status));
  } else {
    error = Qnil;
  }
  rb_funcall(on_connect, id_call, 2, tcp, error);
}

VALUE rbuv_tcp_extractname(struct sockaddr* sockname, int namelen) {
  int addr_type;
  VALUE ip;
  ushort *port;
  char *ip_ptr = malloc(sizeof(char) * namelen);
  if (sockname->sa_family == AF_INET6) {
    struct sockaddr_in6 sockname_in6 = *(struct sockaddr_in6*) sockname;
    port = &sockname_in6.sin6_port;
    uv_ip6_name(&sockname_in6, ip_ptr, namelen);
  } else if (sockname->sa_family == AF_INET) {
    struct sockaddr_in sockname_in = *(struct sockaddr_in*) sockname;
    port = &sockname_in.sin_port;
    uv_ip4_name(&sockname_in, ip_ptr, namelen);
  } else {
    free(ip_ptr); // free before jumping out of here
    rb_raise(eRbuvError, "unexpected error");
    return Qnil; // to satisfy compilers
  }
  ip = rb_str_new_cstr(ip_ptr);
  free(ip_ptr);

  return rb_ary_new3(2, ip, UINT2NUM(ntohs(*port)));
}

void Init_rbuv_tcp() {
  cRbuvTcp = rb_define_class_under(mRbuv, "Tcp", cRbuvStream);
  rb_define_alloc_func(cRbuvTcp, rbuv_tcp_alloc);

  rb_define_method(cRbuvTcp, "initialize", rbuv_tcp_initialize, -1);
  rb_define_method(cRbuvTcp, "bind", rbuv_tcp_bind, 2);
  rb_define_method(cRbuvTcp, "connect", rbuv_tcp_connect, 2);
  rb_define_method(cRbuvTcp, "accept", rbuv_tcp_accept, -1);
  rb_define_method(cRbuvTcp, "enable_keepalive", rbuv_tcp_enable_keepalive, 1);
  rb_define_method(cRbuvTcp, "disable_keepalive",
                   rbuv_tcp_disable_keepalive, 0);
  rb_define_method(cRbuvTcp, "enable_nodelay", rbuv_tcp_enable_nodelay, 0);
  rb_define_method(cRbuvTcp, "disable_nodelay", rbuv_tcp_disable_nodelay, 0);
  rb_define_method(cRbuvTcp, "enable_simultaneous_accepts",
                   rbuv_tcp_enable_simultaneous_accepts, 0);
  rb_define_method(cRbuvTcp, "disable_simultaneous_accepts",
                   rbuv_tcp_disable_simultaneous_accepts, 0);
  rb_define_method(cRbuvTcp, "peername", rbuv_tcp_getpeername, 0);
  rb_define_method(cRbuvTcp, "sockname", rbuv_tcp_getsockname, 0);
}

/* This have to be declared after Init_* so it can replace YARD bad assumption
 * for parent class beeing RbuvStream not Rbuv::Stream.
 * Also it need some text after document-class statement otherwise YARD won't
 * parse it
 */

/*
 * Document-class: Rbuv::Tcp < Rbuv::Stream
 *
 * @!attribute [r] sockname
 *   @return [Array(String, Number)] the socket ip and port
 *
 * @!attribute [r] peername
 *   @return [Array(String, Number)] the peer ip and port
 *
 */
