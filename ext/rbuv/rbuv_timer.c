#include "rbuv_timer.h"

VALUE cRbuvTimer;

struct rbuv_timer_s {
  uv_timer_t *uv_handle;
  VALUE cb_on_close;
  VALUE cb_on_timeout;
};
typedef struct rbuv_timer_s rbuv_timer_t;

struct rbuv_timer_on_timeout_arg_s {
  uv_timer_t *uv_timer;
  int status;
};
typedef struct rbuv_timer_on_timeout_arg_s rbuv_timer_on_timeout_arg_t;


static VALUE rbuv_timer_alloc(VALUE klass);
static void rbuv_timer_mark(rbuv_timer_t *rbuv_timer);
static void rbuv_timer_free(rbuv_timer_t *rbuv_timer);

/* Private methods */
static void rbuv_timer_on_timeout(uv_timer_t *uv_timer, int status);
static void rbuv_timer_on_timeout_no_gvl(rbuv_timer_on_timeout_arg_t *arg);

VALUE rbuv_timer_alloc(VALUE klass) {
  rbuv_timer_t *rbuv_timer;

  rbuv_timer = malloc(sizeof(*rbuv_timer));
  rbuv_timer->uv_handle = NULL;
  rbuv_timer->cb_on_close = Qnil;
  rbuv_timer->cb_on_timeout = Qnil;
  return Data_Wrap_Struct(klass, rbuv_timer_mark, rbuv_timer_free, rbuv_timer);
}

void rbuv_timer_mark(rbuv_timer_t *rbuv_timer) {
  assert(rbuv_timer);
  rbuv_handle_mark((rbuv_handle_t *)rbuv_timer);
  rb_gc_mark(rbuv_timer->cb_on_timeout);
}

void rbuv_timer_free(rbuv_timer_t *rbuv_timer) {
  assert(rbuv_timer);
  RBUV_DEBUG_LOG_DETAIL("rbuv_timer: %p, uv_handle: %p", rbuv_timer, rbuv_timer->uv_handle);

  rbuv_handle_free((rbuv_handle_t *)rbuv_timer);
}

/*
 * @overload initialize(loop=nil)
 *   Create a new handle that fires on specified timeouts.
 *
 *   @param loop [Rbuv::Loop, nil] loop object where this handle runs, if it is
 *     +nil+ then it the runs the handle in the {Rbuv::Loop.default}
 *   @return [Rbuv::Timer]
 */
static VALUE rbuv_timer_initialize(int argc, VALUE *argv, VALUE self) {
  VALUE loop;
  rbuv_timer_t *rbuv_timer;
  rbuv_loop_t *rbuv_loop;

  rb_scan_args(argc, argv, "01", &loop);
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }

  Data_Get_Struct(self, rbuv_timer_t, rbuv_timer);
  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  rbuv_timer->uv_handle = malloc(sizeof(*rbuv_timer->uv_handle));
  uv_timer_init(rbuv_loop->uv_handle, rbuv_timer->uv_handle);
  rbuv_timer->uv_handle->data = (void *)self;

  return self;
}

/*
 * @overload start(timeout, repeat)
 *   Start the timer.
 *   @param timeout [Number] the timeout in millisecond.
 *   @param repeat [Number] the repeat interval in millisecond.
 *   @yieldparam timer [self] itself
 *   @return [self] itself
 */
VALUE rbuv_timer_start(VALUE self, VALUE timeout, VALUE repeat) {
  VALUE block;
  uint64_t uv_timeout;
  uint64_t uv_repeat;
  rbuv_timer_t *rbuv_timer;

  rb_need_block();
  block = rb_block_proc();
  uv_timeout = NUM2ULL(timeout);
  uv_repeat = NUM2ULL(repeat);

  Data_Get_Handle_Struct(self, rbuv_timer_t, rbuv_timer);
  rbuv_timer->cb_on_timeout = block;

  RBUV_DEBUG_LOG_DETAIL("rbuv_timer: %p, uv_handle: %p, rbuv_timer_on_timeout: %p, timer: %s",
                        rbuv_timer, rbuv_timer->uv_handle, rbuv_timer_on_timeout,
                        RSTRING_PTR(rb_inspect(self)));
  uv_timer_start(rbuv_timer->uv_handle, rbuv_timer_on_timeout,
                 uv_timeout, uv_repeat);

  return self;
}

/*
 * Stop the timer.
 *
 * @return [self] itself
 */
VALUE rbuv_timer_stop(VALUE self) {
  rbuv_timer_t *rbuv_timer;

  Data_Get_Handle_Struct(self, rbuv_timer_t, rbuv_timer);

  uv_timer_stop(rbuv_timer->uv_handle);

  return self;
}

VALUE rbuv_timer_repeat_get(VALUE self) {
  rbuv_timer_t *rbuv_timer;
  VALUE repeat;

  Data_Get_Handle_Struct(self, rbuv_timer_t, rbuv_timer);
  repeat = ULL2NUM(uv_timer_get_repeat(rbuv_timer->uv_handle));

  return repeat;
}

VALUE rbuv_timer_repeat_set(VALUE self, VALUE repeat) {
  rbuv_timer_t *rbuv_timer;
  uint64_t uv_repeat;

  uv_repeat = NUM2ULL(repeat);

  Data_Get_Handle_Struct(self, rbuv_timer_t, rbuv_timer);

  uv_timer_set_repeat(rbuv_timer->uv_handle, uv_repeat);

  return repeat;
}

void rbuv_timer_on_timeout(uv_timer_t *uv_timer, int status) {
  rbuv_timer_on_timeout_arg_t reg = {
    .uv_timer = uv_timer,
    .status = status
  };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)
                          rbuv_timer_on_timeout_no_gvl, &reg);
}

void rbuv_timer_on_timeout_no_gvl(rbuv_timer_on_timeout_arg_t *arg) {
  uv_timer_t *uv_timer = arg->uv_timer;
  int status = arg->status;

  VALUE timer;
  rbuv_timer_t *rbuv_timer;

  timer = (VALUE)uv_timer->data;
  Data_Get_Handle_Struct(timer, struct rbuv_timer_s, rbuv_timer);

  rb_funcall(rbuv_timer->cb_on_timeout, id_call, 1, timer);
}

void Init_rbuv_timer() {
  cRbuvTimer = rb_define_class_under(mRbuv, "Timer", cRbuvHandle);
  rb_define_alloc_func(cRbuvTimer, rbuv_timer_alloc);

  rb_define_method(cRbuvTimer, "initialize", rbuv_timer_initialize, -1);
  rb_define_method(cRbuvTimer, "start", rbuv_timer_start, 2);
  rb_define_method(cRbuvTimer, "stop", rbuv_timer_stop, 0);
  rb_define_method(cRbuvTimer, "repeat", rbuv_timer_repeat_get, 0);
  rb_define_method(cRbuvTimer, "repeat=", rbuv_timer_repeat_set, 1);
}

/* This have to be declared after Init_* so it can replace YARD bad assumption
 * for parent class beeing RbuvHandle not Rbuv::Handle.
 * Also it need some text after document-class statement otherwise YARD won't
 * parse it
 */

/*
 * Document-class: Rbuv::Timer < Rbuv::Handle
 * A Timer handle will run the supplied callback after the specified amount of
 * seconds.
 *
 * @!attribute [rw] repeat
 *   @note If the +repeat+ value is set from a timer callback it does not
 *     immediately take effect. If the timer was non-repeating before, it will
 *     have been stopped. If it was repeating, then the old repeat value will
 *     have been used to schedule the next timeout.
 *   @return [Number] the repeat interval in millisecond.
 */
