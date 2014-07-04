#include "rbuv_check.h"

struct rbuv_check_s {
  uv_check_t *uv_handle;
  VALUE loop;
  VALUE cb_on_close;
  VALUE cb_on_check;
};

typedef struct _uv_check_on_check_no_gvl_arg_s {
  uv_check_t *uv_check;
  int status;
} _uv_check_on_check_no_gvl_arg_t;

VALUE cRbuvCheck;

/* Allocator/deallocator */
/*
 * @api private
 */
static VALUE rbuv_check_s_new(int argc, VALUE *argv, VALUE klass);
static void rbuv_check_mark(rbuv_check_t *rbuv_check);
static void rbuv_check_free(rbuv_check_t *rbuv_check);
/* Private Allocatator */
static VALUE rbuv_check_alloc(VALUE klass, VALUE loop);

/* Methods */

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
static VALUE rbuv_check_start(VALUE self);

/*
 * Stop this check handle.
 *
 * @return [self] itself
 */
static VALUE rbuv_check_stop(VALUE self);

/* Private methods */
static void _uv_check_on_check(uv_check_t *uv_check, int status);
static void _uv_check_on_check_no_gvl(_uv_check_on_check_no_gvl_arg_t *arg);

void Init_rbuv_check() {
  cRbuvCheck = rb_define_class_under(mRbuv, "Check", cRbuvHandle);
  rb_undef_alloc_func(cRbuvCheck);
  rb_define_singleton_method(cRbuvCheck, "new", rbuv_check_s_new, -1);

  rb_define_method(cRbuvCheck, "start", rbuv_check_start, 0);
  rb_define_method(cRbuvCheck, "stop", rbuv_check_stop, 0);
}
/*
 * Document-class: Rbuv::Check < Rbuv::Handle
 * Every active check handle gets its callback called exactly once per loop
 * iteration, just after the system returns from blocking.
 *
 * @!method initialize(loop=nil)
 *   Creates a new check handle.
 *
 *   @param loop [Rbuv::Loop, nil] loop object where this handle runs, if it is
 *     +nil+ then it the runs the handle in the {Rbuv::Loop.default}
 *   @return [Rbuv::Check]
 */
VALUE rbuv_check_s_new(int argc, VALUE *argv, VALUE klass) {
  VALUE loop;
  rb_scan_args(argc, argv, "01", &loop);
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }
  return rbuv_check_alloc(klass, loop);
}

VALUE rbuv_check_alloc(VALUE klass, VALUE loop) {
  rbuv_check_t *rbuv_check;
  rbuv_loop_t *rbuv_loop;
  VALUE check;

  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  rbuv_check = malloc(sizeof(*rbuv_check));
  rbuv_check->uv_handle = malloc(sizeof(*rbuv_check->uv_handle));
  uv_check_init(rbuv_loop->uv_handle, rbuv_check->uv_handle);
  rbuv_check->loop = loop;
  rbuv_check->cb_on_check = Qnil;

  check = Data_Wrap_Struct(klass, rbuv_check_mark, rbuv_check_free, rbuv_check);
  rbuv_check->uv_handle->data = (void *)check;

  RBUV_DEBUG_LOG_DETAIL("rbuv_check: %p, uv_handle: %p, check: %s",
                        rbuv_check, rbuv_check->uv_handle,
                        RSTRING_PTR(rb_inspect(check)));

  return check;
}

void rbuv_check_mark(rbuv_check_t *rbuv_check) {
  assert(rbuv_check);
  RBUV_DEBUG_LOG_DETAIL("rbuv_check: %p, uv_handle: %p, self: %lx",
                        rbuv_check, rbuv_check->uv_handle,
                        (VALUE)rbuv_check->uv_handle->data);
  rb_gc_mark(rbuv_check->cb_on_close);
  rb_gc_mark(rbuv_check->cb_on_check);
  rb_gc_mark(rbuv_check->loop);
}

void rbuv_check_free(rbuv_check_t *rbuv_check) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_check: %p, uv_handle: %p", rbuv_check, rbuv_check->uv_handle);
  rbuv_handle_free((rbuv_handle_t *)rbuv_check);
}

VALUE rbuv_check_start(VALUE self) {
  VALUE block;
  rbuv_check_t *rbuv_check;

  rb_need_block();
  block = rb_block_proc();

  Data_Get_Handle_Struct(self, rbuv_check_t, rbuv_check);
  rbuv_check->cb_on_check = block;

  RBUV_DEBUG_LOG_DETAIL("rbuv_check: %p, uv_handle: %p, _uv_check_on_check: %p, check: %s",
                        rbuv_check, rbuv_check->uv_handle, _uv_check_on_check,
                        RSTRING_PTR(rb_inspect(self)));
  uv_check_start(rbuv_check->uv_handle, _uv_check_on_check);

  return self;
}

VALUE rbuv_check_stop(VALUE self) {
  rbuv_check_t *rbuv_check;

  Data_Get_Handle_Struct(self, rbuv_check_t, rbuv_check);

  uv_check_stop(rbuv_check->uv_handle);

  return self;
}

void _uv_check_on_check(uv_check_t *uv_check, int status) {
  _uv_check_on_check_no_gvl_arg_t reg = { .uv_check = uv_check, .status = status };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)_uv_check_on_check_no_gvl, &reg);
}

void _uv_check_on_check_no_gvl(_uv_check_on_check_no_gvl_arg_t *arg) {
  uv_check_t *uv_check = arg->uv_check;
  int status = arg->status;

  VALUE check;
  VALUE error;
  rbuv_check_t *rbuv_check;

  check = (VALUE)uv_check->data;
  Data_Get_Handle_Struct(check, struct rbuv_check_s, rbuv_check);
  if (status < 0) {
    uv_err_t err = uv_last_error(uv_check->loop);
    error = rb_exc_new2(eRbuvError, uv_strerror(err));
  } else {
    error = Qnil;
  }
  rb_funcall(rbuv_check->cb_on_check, id_call, 2, check, error);
}
