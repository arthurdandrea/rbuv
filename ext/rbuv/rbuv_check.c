#include "rbuv_check.h"

struct rbuv_check_s {
  uv_check_t *uv_handle;
  VALUE cb_on_close;
  VALUE cb_on_check;
};
typedef struct rbuv_check_s rbuv_check_t;

VALUE cRbuvCheck;

/* Allocator / Mark / Deallocator */
static VALUE rbuv_check_alloc(VALUE klass);
static void rbuv_check_mark(rbuv_check_t *rbuv_check);
static void rbuv_check_free(rbuv_check_t *rbuv_check);

/* Private methods */
static void rbuv_check_on_check(uv_check_t *uv_check);
static void rbuv_check_on_check_no_gvl(uv_check_t *uv_check);

VALUE rbuv_check_alloc(VALUE klass) {
  rbuv_check_t *rbuv_check;

  rbuv_check = malloc(sizeof(*rbuv_check));
  rbuv_handle_alloc((rbuv_handle_t *)rbuv_check);
  rbuv_check->cb_on_check = Qnil;
  return Data_Wrap_Struct(klass, rbuv_check_mark, rbuv_check_free, rbuv_check);
}

void rbuv_check_mark(rbuv_check_t *rbuv_check) {
  assert(rbuv_check);
  RBUV_DEBUG_LOG_DETAIL("rbuv_check: %p, uv_handle: %p, self: %lx",
                        rbuv_check, rbuv_check->uv_handle,
                        (VALUE)rbuv_check->uv_handle->data);
  rbuv_handle_mark((rbuv_handle_t *)rbuv_check);
  rb_gc_mark(rbuv_check->cb_on_check);
}

void rbuv_check_free(rbuv_check_t *rbuv_check) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_check: %p, uv_handle: %p", rbuv_check, rbuv_check->uv_handle);
  rbuv_handle_free((rbuv_handle_t *)rbuv_check);
}

/*
 * @overload initialize(loop=nil)
 *   Creates a new check handle.
 *
 *   @param loop [Rbuv::Loop, nil] loop object where this handle runs, if it is
 *     +nil+ then it the runs the handle in the {Rbuv::Loop.default}
 *   @return [Rbuv::Check]
 */
VALUE rbuv_check_initialize(int argc, VALUE *argv, VALUE self) {
  VALUE loop;
  rbuv_check_t *rbuv_check;
  rbuv_loop_t *rbuv_loop;

  rb_scan_args(argc, argv, "01", &loop);
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }

  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  Data_Get_Struct(self, rbuv_check_t, rbuv_check);

  rbuv_check->uv_handle = malloc(sizeof(*rbuv_check->uv_handle));
  uv_check_init(rbuv_loop->uv_handle, rbuv_check->uv_handle);
  rbuv_check->uv_handle->data = (void *)self;
  return self;
}

/*
 * Start this check handle.
 *
 * @yield Calls the block exactly once per loop iteration, just after the system
 *   returns from blocking.
 * @yieldparam check [self] itself
 * @yieldparam error [Rbuv::Error, nil] An exception or +nil+ if it was
 *   succesful
 * @return [self] itself
 */
static VALUE rbuv_check_start(VALUE self) {
  VALUE block;
  rbuv_check_t *rbuv_check;

  rb_need_block();
  block = rb_block_proc();

  Data_Get_Handle_Struct(self, rbuv_check_t, rbuv_check);
  rbuv_check->cb_on_check = block;

  RBUV_DEBUG_LOG_DETAIL("rbuv_check: %p, uv_handle: %p, rbuv_check_on_check: %p, check: %s",
                        rbuv_check, rbuv_check->uv_handle, rbuv_check_on_check,
                        RSTRING_PTR(rb_inspect(self)));
  uv_check_start(rbuv_check->uv_handle, rbuv_check_on_check);
  return self;
}

/*
 * Stop this check handle.
 *
 * @return [self] itself
 */
static VALUE rbuv_check_stop(VALUE self) {
  rbuv_check_t *rbuv_check;

  Data_Get_Handle_Struct(self, rbuv_check_t, rbuv_check);
  uv_check_stop(rbuv_check->uv_handle);
  return self;
}

void rbuv_check_on_check(uv_check_t *uv_check) {
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)rbuv_check_on_check_no_gvl, uv_check);
}

void rbuv_check_on_check_no_gvl(uv_check_t *uv_check) {
  VALUE check;
  VALUE error;
  rbuv_check_t *rbuv_check;

  check = (VALUE)uv_check->data;
  Data_Get_Handle_Struct(check, struct rbuv_check_s, rbuv_check);
  error = Qnil;
  rb_funcall(rbuv_check->cb_on_check, id_call, 2, check, error);
}

void Init_rbuv_check() {
  cRbuvCheck = rb_define_class_under(mRbuv, "Check", cRbuvHandle);
  rb_define_alloc_func(cRbuvCheck, rbuv_check_alloc);

  rb_define_method(cRbuvCheck, "initialize", rbuv_check_initialize, -1);
  rb_define_method(cRbuvCheck, "start", rbuv_check_start, 0);
  rb_define_method(cRbuvCheck, "stop", rbuv_check_stop, 0);
}

/* This have to be declared after Init_* so it can replace YARD bad assumption
 * for parent class beeing RbuvHandle not Rbuv::Handle.
 * Also it need some text after document-class statement otherwise YARD won't
 * parse it
 */

/*
 * Document-class: Rbuv::Check < Rbuv::Handle
 *
 * Every active check handle gets its callback called exactly once per loop
 * iteration, just after the system returns from blocking.
 */
