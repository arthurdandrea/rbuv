#include "rbuv_tcp.h"

struct rbuv_tcp_s {
  uv_tcp_t *uv_handle;
  VALUE loop;
  VALUE cb_on_close;
  VALUE cb_on_connection;
  VALUE cb_on_read;
  VALUE cbs_on_write;
};

typedef struct {
  uv_connect_t *uv_connect;
  int status;
} _uv_tcp_on_connect_arg_t;

VALUE cRbuvTcp;

/* Allocator/deallocator */

/*
 * @see #initialize
 * @api private
 */
static VALUE rbuv_tcp_s_new(int argc, VALUE *argv, VALUE klass);
static void rbuv_tcp_mark(rbuv_tcp_t *rbuv_tcp);
static void rbuv_tcp_free(rbuv_tcp_t *rbuv_tcp);
/* Private Allocatator */
static VALUE rbuv_tcp_alloc(VALUE klass, VALUE loop);

/* Methods */
/*
 uv_tcp_bind(uv_tcp_t* handle, struct sockaddr_in)
 uv_tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6)
 uv_tcp_connect(uv_connect_t* req, uv_tcp_t* handle, struct sockaddr_in address, uv_connect❯
 uv_tcp_connect6(uv_connect_t* req, uv_tcp_t* handle, struct sockaddr_in6 address, uv_conne❯
 uv_tcp_getpeername(uv_tcp_t* handle, struct sockaddr* name, int* namelen)
 uv_tcp_getsockname(uv_tcp_t* handle, struct sockaddr* name, int* namelen)
 uv_tcp_init(uv_loop_t*, uv_tcp_t* handle)
 uv_tcp_keepalive(uv_tcp_t* handle, int enable, unsigned int delay)
 uv_tcp_nodelay(uv_tcp_t* handle, int enable)
 uv_tcp_open(uv_tcp_t* handle, uv_os_sock_t sock)
 uv_tcp_simultaneous_accepts(uv_tcp_t* handle, int enable)
 */

/* @overload bind(ip, port)
 * Bind this tcp object to the given address and port.
 * @param ip [String] the ip address to bind to
 * @param port [Number] the port to bind to
 * @return [self] itself
 */
static VALUE rbuv_tcp_bind(VALUE self, VALUE ip, VALUE port);
//static VALUE rbuv_tcp_bind6(VALUE self, VALUE ip, VALUE port);

/* @overload connect(ip, port)
 * Connect this tcp object to the given address and port.
 * @param ip [String] the ip address to bind to
 * @param port [Number] the port to bind to
 * @return [self] itself
 */
static VALUE rbuv_tcp_connect(VALUE self, VALUE ip, VALUE port);
//static VALUE rbuv_tcp_connect6(VALUE self, VALUE ip, VALUE port);

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
static VALUE rbuv_tcp_accept(int argc, VALUE *argv, VALUE self);

/* @overload enable_keepalive(delay)
 * Enable TCP keep-alive.
 *
 * @param delay [Number] the initial delay in seconds
 * @return [self] itself
 */
static VALUE rbuv_tcp_enable_keepalive(VALUE self, VALUE delay);

/* @overload disable_keepalive
 * Disable TCP keep-alive.
 *
 * @return [self] itself
 */
static VALUE rbuv_tcp_disable_keepalive(VALUE self);

/* @overload enable_nodelay
 * Disable Nagle's algorithm.
 *
 * @return [self] itself
 */
static VALUE rbuv_tcp_enable_nodelay(VALUE self);

/* @overload disable_nodelay
 * Enable Nagle's algorithm.
 *
 * @return [self] itself
 */
static VALUE rbuv_tcp_disable_nodelay(VALUE self);

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
static VALUE rbuv_tcp_enable_simultaneous_accepts(VALUE self);

/* @overload disable_simultaneous_accepts
 * Disable simultaneous asynchronous accept requests that are queued by the
 * operating system when listening for new tcp connections.
 * @see #enable_simultaneous_accepts
 *
 * @return [self] itself
 */
static VALUE rbuv_tcp_disable_simultaneous_accepts(VALUE self);
static VALUE rbuv_tcp_getpeername(VALUE self);
static VALUE rbuv_tcp_getsockname(VALUE self);

/* Private methods */
static VALUE rbuv_tcp_extractname(struct sockaddr* sockname, int namelen);
static void _uv_tcp_on_connect(uv_connect_t *uv_connect, int status);
static void _uv_tcp_on_connect_no_gvl(_uv_tcp_on_connect_arg_t *arg);
extern void __uv_stream_on_connection_no_gvl(uv_stream_t *uv_stream, int status);

void Init_rbuv_tcp() {
  cRbuvTcp = rb_define_class_under(mRbuv, "Tcp", cRbuvStream);
  rb_undef_alloc_func(cRbuvTcp);
  rb_define_singleton_method(cRbuvTcp, "new", rbuv_tcp_s_new, -1);

  rb_define_method(cRbuvTcp, "bind", rbuv_tcp_bind, 2);
  //rb_define_method(cRbuvTcp, "bind6", rbuv_tcp_bind6, 2);
  rb_define_method(cRbuvTcp, "connect", rbuv_tcp_connect, 2);
  //rb_define_method(cRbuvTcp, "connect6", rbuv_tcp_connect6, 2);
  rb_define_method(cRbuvTcp, "accept", rbuv_tcp_accept, -1);
  rb_define_method(cRbuvTcp, "enable_keepalive", rbuv_tcp_enable_keepalive, 1);
  rb_define_method(cRbuvTcp, "disable_keepalive", rbuv_tcp_disable_keepalive, 0);
  rb_define_method(cRbuvTcp, "enable_nodelay", rbuv_tcp_enable_nodelay, 0);
  rb_define_method(cRbuvTcp, "disable_nodelay", rbuv_tcp_disable_nodelay, 0);
  rb_define_method(cRbuvTcp, "enable_simultaneous_accepts",
                   rbuv_tcp_enable_simultaneous_accepts, 0);
  rb_define_method(cRbuvTcp, "disable_simultaneous_accepts",
                   rbuv_tcp_disable_simultaneous_accepts, 0);
  rb_define_method(cRbuvTcp, "peername", rbuv_tcp_getpeername, 0);
  rb_define_method(cRbuvTcp, "sockname", rbuv_tcp_getsockname, 0);
}

/*
 * Document-class: Rbuv::Tcp < Rbuv::Stream
 *
 * @!attribute [r] sockname
 *   @return [Array(String, Number)] the socket ip and port
 *
 * @!attribute [r] peername
 *   @return [Array(String, Number)] the peer ip and port
 *
 * @!method initialize(loop=nil)
 *   Create a new handle to deal with a TCP.
 *
 *   @param loop [Rbuv::Loop, nil] loop object where this handle runs, if it is
 *     +nil+ then it the runs the handle in the {Rbuv::Loop.default}
 *   @return [Rbuv::Tcp]
 */
VALUE rbuv_tcp_s_new(int argc, VALUE *argv, VALUE klass) {
  VALUE loop;
  rb_scan_args(argc, argv, "01", &loop);
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }
  return rbuv_tcp_alloc(klass, loop);
}

VALUE rbuv_tcp_alloc(VALUE klass, VALUE loop) {
  rbuv_tcp_t *rbuv_tcp;
  rbuv_loop_t *rbuv_loop;
  VALUE tcp;

  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  rbuv_tcp = malloc(sizeof(*rbuv_tcp));
  rbuv_tcp->uv_handle = malloc(sizeof(*rbuv_tcp->uv_handle));
  uv_tcp_init(rbuv_loop->uv_handle, rbuv_tcp->uv_handle);
  rbuv_tcp->cbs_on_write = Qnil;
  rbuv_tcp->cb_on_close = Qnil;
  rbuv_tcp->cb_on_connection = Qnil;
  rbuv_tcp->cb_on_read = Qnil;
  rbuv_tcp->loop = loop;

  tcp = Data_Wrap_Struct(klass, rbuv_tcp_mark, rbuv_tcp_free, rbuv_tcp);
  rbuv_tcp->uv_handle->data = (void *)tcp;
  rbuv_loop_register_handle(rbuv_loop, rbuv_tcp, tcp);
  RBUV_DEBUG_LOG_DETAIL("rbuv_tcp: %p, uv_handle: %p, tcp: %s",
                        rbuv_tcp, rbuv_tcp->uv_handle,
                        RSTRING_PTR(rb_inspect(tcp)));

  return tcp;
}

void rbuv_tcp_mark(rbuv_tcp_t *rbuv_tcp) {
  assert(rbuv_tcp);
  RBUV_DEBUG_LOG_DETAIL("rbuv_tcp: %p, uv_handle: %p, self: %lx",
                        rbuv_tcp, rbuv_tcp->uv_handle,
                        (VALUE)rbuv_tcp->uv_handle->data);
  rb_gc_mark(rbuv_tcp->cbs_on_write);
  rb_gc_mark(rbuv_tcp->cb_on_close);
  rb_gc_mark(rbuv_tcp->cb_on_connection);
  rb_gc_mark(rbuv_tcp->cb_on_read);
  rb_gc_mark(rbuv_tcp->loop);
}

void rbuv_tcp_free(rbuv_tcp_t *rbuv_tcp) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_tcp: %p, uv_handle: %p", rbuv_tcp, rbuv_tcp->uv_handle);

  rbuv_handle_free((rbuv_handle_t *)rbuv_tcp);
}

VALUE rbuv_tcp_enable_nodelay(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_nodelay(rbuv_tcp->uv_handle, 1),
                       rbuv_tcp->uv_handle->loop);
  return self;
}

VALUE rbuv_tcp_disable_nodelay(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_nodelay(rbuv_tcp->uv_handle, 0),
                       rbuv_tcp->uv_handle->loop);
  return self;
}

VALUE rbuv_tcp_enable_keepalive(VALUE self, VALUE delay) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_keepalive(rbuv_tcp->uv_handle, 1,
                                        NUM2UINT(delay)),
                       rbuv_tcp->uv_handle->loop);
  return self;
}

VALUE rbuv_tcp_disable_keepalive(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_keepalive(rbuv_tcp->uv_handle, 0, 0),
                       rbuv_tcp->uv_handle->loop);
  return self;
}

VALUE rbuv_tcp_enable_simultaneous_accepts(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_simultaneous_accepts(rbuv_tcp->uv_handle, 1),
                       rbuv_tcp->uv_handle->loop);

  return self;
}
VALUE rbuv_tcp_disable_simultaneous_accepts(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_simultaneous_accepts(rbuv_tcp->uv_handle, 0),
                       rbuv_tcp->uv_handle->loop);
  return self;
}


VALUE rbuv_tcp_getpeername(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  struct sockaddr peername;
  int namelen = sizeof peername;
  RBUV_CHECK_UV_RETURN(uv_tcp_getpeername(rbuv_tcp->uv_handle, &peername,
                                          &namelen),
                       rbuv_tcp->uv_handle->loop);
  return rbuv_tcp_extractname(&peername, namelen);
}

VALUE rbuv_tcp_getsockname(VALUE self) {
  rbuv_tcp_t *rbuv_tcp;
  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  struct sockaddr sockname;
  int namelen = sizeof sockname;
  RBUV_CHECK_UV_RETURN(uv_tcp_getsockname(rbuv_tcp->uv_handle, &sockname,
                                          &namelen),
                       rbuv_tcp->uv_handle->loop);
  return rbuv_tcp_extractname(&sockname, namelen);
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

#ifdef RBUV_RBX
  VALUE ary = rb_ary_new(2);
  rb_ary_push(ary, ip);
  rb_ary_push(ary, UINT2NUM(ntohs(*port)));
  return ary;
#else
  return rb_ary_new_from_args(2, ip,
                              UINT2NUM(ntohs(*port)));
#endif
}

VALUE rbuv_tcp_accept(int argc, VALUE *argv, VALUE self) {
  VALUE client;
  rb_scan_args(argc, argv, "01", &client);
  if (client == Qnil) {
    rbuv_tcp_t *rbuv_tcp;
    Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
    client = rbuv_tcp_alloc(rb_class_of(self), rbuv_tcp->loop);
    VALUE super_argv[1] = { client };
    rb_call_super(1, super_argv);
    return client;
  } else {
    return rb_call_super(argc, argv);
  }
}

VALUE rbuv_tcp_bind(VALUE self, VALUE ip, VALUE port) {
  const char *uv_ip;
  int uv_port;
  rbuv_tcp_t *rbuv_tcp;
  struct sockaddr_in bind_addr;

  uv_ip = RSTRING_PTR(ip);
  uv_port = FIX2INT(port);

  bind_addr = uv_ip4_addr(uv_ip, uv_port);

  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  RBUV_CHECK_UV_RETURN(uv_tcp_bind(rbuv_tcp->uv_handle, bind_addr),
                       rbuv_tcp->uv_handle->loop);

  RBUV_DEBUG_LOG_DETAIL("self: %s, ip: %s, port: %d, rbuv_tcp: %p, uv_handle: %p",
                        RSTRING_PTR(rb_inspect(self)), uv_ip, uv_port, rbuv_tcp,
                        rbuv_tcp->uv_handle);

  return self;
}

VALUE rbuv_tcp_connect(VALUE self, VALUE ip, VALUE port) {
  VALUE block;
  const char *uv_ip;
  int uv_port;
  rbuv_tcp_t *rbuv_tcp;
  struct sockaddr_in connect_addr;
  uv_connect_t *uv_connect;

  rb_need_block();
  block = rb_block_proc();

  uv_ip = RSTRING_PTR(ip);
  uv_port = FIX2INT(port);

  Data_Get_Handle_Struct(self, rbuv_tcp_t, rbuv_tcp);
  rbuv_tcp->cb_on_connection = block;

  uv_connect = malloc(sizeof(*uv_connect));
  connect_addr = uv_ip4_addr(uv_ip, uv_port);

  RBUV_CHECK_UV_RETURN(uv_tcp_connect(uv_connect, rbuv_tcp->uv_handle,
                                      connect_addr, _uv_tcp_on_connect),
                       rbuv_tcp->uv_handle->loop);

  RBUV_DEBUG_LOG_DETAIL("self: %s, ip: %s, port: %d, rbuv_tcp: %p, uv_handle: %p",
                        RSTRING_PTR(rb_inspect(self)), uv_ip, uv_port, rbuv_tcp,
                        rbuv_tcp->uv_handle);

  return self;
}

void _uv_tcp_on_connect(uv_connect_t *uv_connect, int status) {
  _uv_tcp_on_connect_arg_t arg = { .uv_connect = uv_connect, .status = status };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)_uv_tcp_on_connect_no_gvl, &arg);
}

void _uv_tcp_on_connect_no_gvl(_uv_tcp_on_connect_arg_t *arg) {
  uv_connect_t *uv_connect = arg->uv_connect;
  int status = arg->status;

  __uv_stream_on_connection_no_gvl(uv_connect->handle, status);

  free(uv_connect);
}
