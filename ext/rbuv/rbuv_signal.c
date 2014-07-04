#include "rbuv_signal.h"

struct rbuv_signal_s {
  uv_signal_t *uv_handle;
  VALUE loop;
  VALUE cb_on_close;
  VALUE cb_on_signal;
};

typedef struct _uv_signal_on_signal_no_gvl_arg_s {
  uv_signal_t *uv_signal;
  int signum;
} _uv_signal_on_signal_no_gvl_arg_t;

VALUE cRbuvSignal;

/* Allocator/deallocator */
/*
 * @api private
 */
static VALUE rbuv_signal_s_new(int argc, VALUE *argv, VALUE klass);
static void rbuv_signal_mark(rbuv_signal_t *rbuv_signal);
static void rbuv_signal_free(rbuv_signal_t *rbuv_signal);
/* Private Allocatator */
static VALUE rbuv_signal_alloc(VALUE klass, VALUE loop);

/* Methods */

/* @overload start(signum)
 * Start watching for signal +signum+ with this handle.
 *
 * @param signum [Number] the signal number to watch for
 *
 * @yield Calls the block when the +signum+ is recieved
 * @yieldparam signal [self] itself
 * @yieldparam signum [Number] the signal number recieved
 * @return [self] itself
 */
static VALUE rbuv_signal_start(VALUE self, VALUE signum);

/*
 * Stop watching for signals with this handle.
 *
 * @return [self] itself
 */
static VALUE rbuv_signal_stop(VALUE self);

/* Private methods */
static void _uv_signal_on_signal(uv_signal_t *uv_signal, int signum);
static void _uv_signal_on_signal_no_gvl(_uv_signal_on_signal_no_gvl_arg_t *arg);

void Init_rbuv_signal() {
  cRbuvSignal = rb_define_class_under(mRbuv, "Signal", cRbuvHandle);
  rb_undef_alloc_func(cRbuvSignal);
  rb_define_singleton_method(cRbuvSignal, "new", rbuv_signal_s_new, -1);

  rb_define_method(cRbuvSignal, "start", rbuv_signal_start, 1);
  rb_define_method(cRbuvSignal, "stop", rbuv_signal_stop, 0);
}
/*
 * Document-class: Rbuv::Signal < Rbuv::Handle
 *
 * @!method initialize(loop=nil)
 *   Creates a new handle to watch for signals.
 *
 *   @param loop [Rbuv::Loop, nil] loop object where this handle runs, if it is
 *     +nil+ then it the runs the handle in the {Rbuv::Loop.default}
 *   @return [Rbuv::Signal]
 */
VALUE rbuv_signal_s_new(int argc, VALUE *argv, VALUE klass) {
  VALUE loop;
  rb_scan_args(argc, argv, "01", &loop);
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }
  return rbuv_signal_alloc(klass, loop);
}

VALUE rbuv_signal_alloc(VALUE klass, VALUE loop) {
  rbuv_signal_t *rbuv_signal;
  rbuv_loop_t *rbuv_loop;
  VALUE signal;

  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  rbuv_signal = malloc(sizeof(*rbuv_signal));
  rbuv_signal->uv_handle = malloc(sizeof(*rbuv_signal->uv_handle));
  uv_signal_init(rbuv_loop->uv_handle, rbuv_signal->uv_handle);
  rbuv_signal->loop = loop;
  rbuv_signal->cb_on_signal = Qnil;

  signal = Data_Wrap_Struct(klass, rbuv_signal_mark, rbuv_signal_free, rbuv_signal);
  rbuv_signal->uv_handle->data = (void *)signal;

  RBUV_DEBUG_LOG_DETAIL("rbuv_signal: %p, uv_handle: %p, signal: %s",
                        rbuv_signal, rbuv_signal->uv_handle,
                        RSTRING_PTR(rb_inspect(signal)));

  return signal;
}

void rbuv_signal_mark(rbuv_signal_t *rbuv_signal) {
  assert(rbuv_signal);
  RBUV_DEBUG_LOG_DETAIL("rbuv_signal: %p, uv_handle: %p, self: %lx",
                        rbuv_signal, rbuv_signal->uv_handle,
                        (VALUE)rbuv_signal->uv_handle->data);
  rb_gc_mark(rbuv_signal->cb_on_close);
  rb_gc_mark(rbuv_signal->cb_on_signal);
  rb_gc_mark(rbuv_signal->loop);
}

void rbuv_signal_free(rbuv_signal_t *rbuv_signal) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_signal: %p, uv_handle: %p", rbuv_signal, rbuv_signal->uv_handle);
  rbuv_handle_free((rbuv_handle_t *)rbuv_signal);
}

VALUE rbuv_signal_start(VALUE self, VALUE signum) {
  VALUE block;
  int uv_signum;
  rbuv_signal_t *rbuv_signal;

  rb_need_block();
  block = rb_block_proc();
  uv_signum = NUM2INT(signum);

  Data_Get_Handle_Struct(self, rbuv_signal_t, rbuv_signal);
  rbuv_signal->cb_on_signal = block;

  RBUV_DEBUG_LOG_DETAIL("rbuv_signal: %p, uv_handle: %p, _uv_signal_on_signal: %p, signal: %s",
                        rbuv_signal, rbuv_signal->uv_handle, _uv_signal_on_signal,
                        RSTRING_PTR(rb_inspect(self)));
  uv_signal_start(rbuv_signal->uv_handle, _uv_signal_on_signal, uv_signum);

  return self;
}

VALUE rbuv_signal_stop(VALUE self) {
  rbuv_signal_t *rbuv_signal;

  Data_Get_Handle_Struct(self, rbuv_signal_t, rbuv_signal);

  uv_signal_stop(rbuv_signal->uv_handle);

  return self;
}

void _uv_signal_on_signal(uv_signal_t *uv_signal, int signum) {
  _uv_signal_on_signal_no_gvl_arg_t reg = { .uv_signal = uv_signal, .signum = signum };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)_uv_signal_on_signal_no_gvl, &reg);
}

void _uv_signal_on_signal_no_gvl(_uv_signal_on_signal_no_gvl_arg_t *arg) {
  uv_signal_t *uv_signal = arg->uv_signal;
  int signum = arg->signum;

  VALUE signal;
  rbuv_signal_t *rbuv_signal;

  signal = (VALUE)uv_signal->data;
  Data_Get_Handle_Struct(signal, struct rbuv_signal_s, rbuv_signal);

  rb_funcall(rbuv_signal->cb_on_signal, id_call, 2, signal, INT2FIX(signum));
}
