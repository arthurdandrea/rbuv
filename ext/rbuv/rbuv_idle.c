#include "rbuv_idle.h"

struct rbuv_idle_s {
  uv_idle_t *uv_handle;
  VALUE cb_on_close;
  VALUE cb_on_idle;
};
typedef struct rbuv_idle_s rbuv_idle_t;

VALUE cRbuvIdle;

/* Allocator / Mark / Deallocator */
static VALUE rbuv_idle_alloc(VALUE klass);
static void rbuv_idle_mark(rbuv_idle_t *rbuv_idle);
static void rbuv_idle_free(rbuv_idle_t *rbuv_idle);

/* Private methods */
static void rbuv_idle_on_idle(uv_idle_t *uv_idle);
static void rbuv_idle_on_idle_no_gvl(uv_idle_t *uv_idle);

VALUE rbuv_idle_alloc(VALUE klass) {
  rbuv_idle_t *rbuv_idle;

  rbuv_idle = malloc(sizeof(*rbuv_idle));
  rbuv_handle_alloc((rbuv_handle_t *)rbuv_idle);
  rbuv_idle->cb_on_idle = Qnil;
  return Data_Wrap_Struct(klass, rbuv_idle_mark, rbuv_idle_free, rbuv_idle);
}

void rbuv_idle_mark(rbuv_idle_t *rbuv_idle) {
  assert(rbuv_idle);
  RBUV_DEBUG_LOG_DETAIL("rbuv_idle: %p, uv_handle: %p", rbuv_idle, rbuv_idle->uv_handle);
  rbuv_handle_mark((rbuv_handle_t *)rbuv_idle);
  rb_gc_mark(rbuv_idle->cb_on_idle);
}

void rbuv_idle_free(rbuv_idle_t *rbuv_idle) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_idle: %p, uv_handle: %p", rbuv_idle, rbuv_idle->uv_handle);
  rbuv_handle_free((rbuv_handle_t *)rbuv_idle);
}

/*
 * @overload initialize(loop=nil)
 *   Creates a new idle handle.
 *
 *   @param loop [Rbuv::Loop, nil] loop object where this handle runs, if it is
 *     +nil+ then it the runs the handle in the {Rbuv::Loop.default}
 *   @return [Rbuv::Idle]
 */
VALUE rbuv_idle_initialize(int argc, VALUE *argv, VALUE self) {
  VALUE loop;
  rbuv_idle_t *rbuv_idle;
  rbuv_loop_t *rbuv_loop;
  int uv_ret;

  rb_scan_args(argc, argv, "01", &loop);
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }

  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  Data_Get_Struct(self, rbuv_idle_t, rbuv_idle);

  rbuv_idle->uv_handle = malloc(sizeof(*rbuv_idle->uv_handle));
  uv_ret = uv_idle_init(rbuv_loop->uv_handle, rbuv_idle->uv_handle);
  if (uv_ret < 0) {
    rbuv_idle->uv_handle->data = NULL;
    free(rbuv_idle->uv_handle);
    rbuv_idle->uv_handle = NULL;
    rb_raise(eRbuvError, "%s", uv_strerror(uv_ret));
  } else {
    rbuv_idle->uv_handle->data = (void *)self;
    return self;
  }
}

/*
 * Start this idle handle.
 *
 * @yield Calls the block exactly once per loop iteration, right before
 *   {Rbuv::Prepare} handles.
 * @yieldparam idle [self] itself
 * @yieldparam error [Rbuv::Error, nil] An exception or +nil+ if it was
 *   succesful
 * @return [self] itself
 */
static VALUE rbuv_idle_start(VALUE self) {
  VALUE block;
  rbuv_idle_t *rbuv_idle;

  rb_need_block();
  block = rb_block_proc();

  Data_Get_Handle_Struct(self, rbuv_idle_t, rbuv_idle);
  rbuv_idle->cb_on_idle = block;

  RBUV_DEBUG_LOG_DETAIL("rbuv_idle: %p, uv_handle: %p, rbuv_idle_on_idle: %p, idle: %s",
                        rbuv_idle, rbuv_idle->uv_handle, rbuv_idle_on_idle,
                        RSTRING_PTR(rb_inspect(self)));
  uv_idle_start(rbuv_idle->uv_handle, rbuv_idle_on_idle);
  return self;
}

/*
 * Stop this idle handle.
 *
 * @return [self] itself
 */
static VALUE rbuv_idle_stop(VALUE self) {
  rbuv_idle_t *rbuv_idle;

  Data_Get_Handle_Struct(self, rbuv_idle_t, rbuv_idle);
  uv_idle_stop(rbuv_idle->uv_handle);
  return self;
}

void rbuv_idle_on_idle(uv_idle_t *uv_idle) {
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)rbuv_idle_on_idle_no_gvl, uv_idle);
}

void rbuv_idle_on_idle_no_gvl(uv_idle_t *uv_idle) {
  VALUE idle;
  VALUE error;
  rbuv_idle_t *rbuv_idle;

  idle = (VALUE)uv_idle->data;
  Data_Get_Handle_Struct(idle, struct rbuv_idle_s, rbuv_idle);
  error = Qnil;
  rb_funcall(rbuv_idle->cb_on_idle, id_call, 2, idle, error);
}

void Init_rbuv_idle() {
  cRbuvIdle = rb_define_class_under(mRbuv, "Idle", cRbuvHandle);
  rb_define_alloc_func(cRbuvIdle, rbuv_idle_alloc);

  rb_define_method(cRbuvIdle, "initialize", rbuv_idle_initialize, -1);
  rb_define_method(cRbuvIdle, "start", rbuv_idle_start, 0);
  rb_define_method(cRbuvIdle, "stop", rbuv_idle_stop, 0);
}

/* This have to be declared after Init_* so it can replace YARD bad assumption
 * for parent class beeing RbuvHandle not Rbuv::Handle.
 * Also it need some text after document-class statement otherwise YARD won't
 * parse it
 */

/*
 * Document-class: Rbuv::Idle < Rbuv::Handle
 *
 * Every active idle handle gets its callback called exactly once per
 * iteration, right before the {Rbuv::Prepare} handles.
 */
