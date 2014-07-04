#include "rbuv_poll.h"

VALUE cRbuvPoll;

struct rbuv_poll_s {
  uv_poll_t *uv_handle;
  VALUE loop;
  VALUE cb_on_close;
  VALUE cb_on_available;
};

typedef struct {
  uv_poll_t *uv_poll;
  int status;
  int events;
} _uv_timer_on_available_no_gvl_arg_t;

/* Allocator/deallocator */

/* @see #initialize
 * @api private
 */
static VALUE rbuv_poll_s_new(int argc, VALUE *argv, VALUE klass);
static void rbuv_poll_mark(rbuv_poll_t *rbuv_poll);
static void rbuv_poll_free(rbuv_poll_t *rbuv_poll);
/* Private Allocatator */
static VALUE rbuv_poll_alloc(VALUE klass, VALUE loop, VALUE fd);

/* Methods */

/* @overload start(events)
 *  Starts polling the file descriptor.
 *  @note The user should not close the socket while +Rbuv::Poll+ is active.
 *    If the user does that anyway, the block *may* be called reporting an error
 *    but this is not guaranteed.
 *  @note Calling +start+ on an +Rbuv::Poll+ that is already active is fine.
 *    Doing so will update the events mask that is being watched for.
 *  @param [Fixnum] events a bitmask consisting made up of
 *    {Rbuv::Poll::READABLE} and {Rbuv::Poll::WRITABLE}
 *  @yield calls the block when any +event+ is detected on +fd+
 *  @yieldparam poll [self] itself
 *  @yieldparam events [Fixnum, nil]
 *    the detected events as a bitmask made up of {Rbuv::Poll::READABLE} and
 *    {Rbuv::Poll::WRITABLE}, if succeded
 *  @yieldparam error [Rbuv::Error, nil] the error, if not succeded
 *  @return [self]
 */
static VALUE rbuv_poll_start(VALUE self, VALUE events);

/*
 * Stops polling the file descriptor.
 * @return [self]
 */
static VALUE rbuv_poll_stop(VALUE self);

/* Private methods */
static void _uv_poll_on_available(uv_poll_t *uv_poll, int status, int events);
static void _uv_poll_on_available_no_gvl(_uv_timer_on_available_no_gvl_arg_t *arg);

void Init_rbuv_poll() {
  cRbuvPoll = rb_define_class_under(mRbuv, "Poll", cRbuvHandle);
  rb_undef_alloc_func(cRbuvPoll);
  rb_define_singleton_method(cRbuvPoll, "new", rbuv_poll_s_new, -1);

  rb_define_method(cRbuvPoll, "start", rbuv_poll_start, 1);
  rb_define_method(cRbuvPoll, "stop", rbuv_poll_stop, 0);

  /*
   * Represent a Readable event. Used to form bitmasks.
   * @note Different libuv versions may have different values for this constant,
   *   so don't relay on this always being the same on different environments.
   */
  rb_define_const(cRbuvPoll, "READABLE", INT2FIX(UV_READABLE));

  /*
   * Represent a Writable event. Used to form bitmasks.
   * @note Different libuv versions may have different values for this constant,
   *   so don't relay on this always being the same on different environments.
   */
  rb_define_const(cRbuvPoll, "WRITABLE", INT2FIX(UV_WRITABLE));
}

/*
 * Document-class: Rbuv::Poll < Rbuv::Handle
 * @!method initialize
 *   Creates a new poll watcher
 *   @overload initialize(loop, fd)
 *    @param [Rbuv::Loop] loop where this handle runs.
 *    @param [Fixnum] fd file descriptor.
 *   @overload initialize(fd)
 *    Uses the {Rbuv::Loop.default} to run this handle
 *    @param [Fixnum] fd file descriptor.
 *   @return [Rbuv::Poll]
 */
VALUE rbuv_poll_s_new(int argc, VALUE *argv, VALUE klass) {
  VALUE loop;
  VALUE fd;
  rb_scan_args(argc, argv, "11", &loop, &fd);
  if (fd == Qnil) {
    fd = loop;
    loop = Qnil;
  }
  if (TYPE(fd) != T_FIXNUM) {
    rb_raise(rb_eTypeError, "not an integer");
  }
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }
  return rbuv_poll_alloc(klass, loop, FIX2INT(fd));
}

VALUE rbuv_poll_alloc(VALUE klass, VALUE loop, VALUE fd) {
  rbuv_poll_t *rbuv_poll;
  rbuv_loop_t *rbuv_loop;
  VALUE timer;

  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  rbuv_poll = malloc(sizeof(*rbuv_poll));
  rbuv_poll->uv_handle = malloc(sizeof(*rbuv_poll->uv_handle));
  uv_poll_init(rbuv_loop->uv_handle, rbuv_poll->uv_handle, fd);
  rbuv_poll->cb_on_close = Qnil;
  rbuv_poll->cb_on_available = Qnil;
  rbuv_poll->loop = loop;

  timer = Data_Wrap_Struct(klass, rbuv_poll_mark, rbuv_poll_free, rbuv_poll);
  rbuv_poll->uv_handle->data = (void *)timer;
  rbuv_loop_register_handle(rbuv_loop, rbuv_poll, timer);

  return timer;
}

void rbuv_poll_mark(rbuv_poll_t *rbuv_poll) {
  assert(rbuv_poll);
  rb_gc_mark(rbuv_poll->loop);
  rb_gc_mark(rbuv_poll->cb_on_close);
  rb_gc_mark(rbuv_poll->cb_on_available);
}

void rbuv_poll_free(rbuv_poll_t *rbuv_poll) {
  assert(rbuv_poll);
  RBUV_DEBUG_LOG_DETAIL("rbuv_poll: %p, uv_handle: %p", rbuv_poll, rbuv_poll->uv_handle);

  rbuv_handle_free((rbuv_handle_t *)rbuv_poll);
}

VALUE rbuv_poll_start(VALUE self, VALUE events) {
  VALUE block;
  int uv_events;
  rbuv_poll_t *rbuv_poll;

  rb_need_block();
  block = rb_block_proc();
  uv_events = FIX2INT(events);

  Data_Get_Handle_Struct(self, rbuv_poll_t, rbuv_poll);
  rbuv_poll->cb_on_available = block;

  uv_poll_start(rbuv_poll->uv_handle, uv_events, _uv_poll_on_available);
  return self;
}

/**
 * stop the poll.
 * @return self
 */
VALUE rbuv_poll_stop(VALUE self) {
  rbuv_poll_t *rbuv_poll;

  Data_Get_Handle_Struct(self, rbuv_poll_t, rbuv_poll);

  uv_poll_stop(rbuv_poll->uv_handle);

  return self;
}

void _uv_poll_on_available(uv_poll_t *uv_poll, int status, int events) {
  _uv_timer_on_available_no_gvl_arg_t reg = {
    .uv_poll = uv_poll,
    .status = status,
    .events = events };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)_uv_poll_on_available_no_gvl, &reg);
}

void _uv_poll_on_available_no_gvl(_uv_timer_on_available_no_gvl_arg_t *arg) {
  uv_poll_t *uv_poll = arg->uv_poll;
  int status = arg->status;

  VALUE events;
  VALUE poll;
  VALUE error;
  uv_err_t uv_err;
  rbuv_poll_t *rbuv_poll;

  poll = (VALUE)uv_poll->data;
  events = INT2FIX(arg->events);
  Data_Get_Handle_Struct(poll, struct rbuv_poll_s, rbuv_poll);
  if (status == -1) {
    uv_err = uv_last_error(uv_poll->loop);
    RBUV_DEBUG_LOG_DETAIL("uv_poll: %p, status: %d, error: %s", uv_stream,
                          status, uv_strerror(uv_err));
    error = rb_exc_new2(eRbuvError, uv_strerror(uv_err));
  } else {
    error = Qnil;
  }
  rb_funcall(rbuv_poll->cb_on_available, id_call, 3, poll, events, error);
}
