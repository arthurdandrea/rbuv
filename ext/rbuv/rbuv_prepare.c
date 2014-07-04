#include "rbuv_prepare.h"

struct rbuv_prepare_s {
  uv_prepare_t *uv_handle;
  VALUE loop;
  VALUE cb_on_close;
  VALUE cb_on_prepare;
};

typedef struct _uv_prepare_on_prepare_no_gvl_arg_s {
  uv_prepare_t *uv_prepare;
  int status;
} _uv_prepare_on_prepare_no_gvl_arg_t;

VALUE cRbuvPrepare;

/* Allocator/deallocator */
/*
 * @api private
 */
static VALUE rbuv_prepare_s_new(int argc, VALUE *argv, VALUE klass);
static void rbuv_prepare_mark(rbuv_prepare_t *rbuv_prepare);
static void rbuv_prepare_free(rbuv_prepare_t *rbuv_prepare);
/* Private Allocatator */
static VALUE rbuv_prepare_alloc(VALUE klass, VALUE loop);

/* Methods */

/*
 * Start this prepare handle.
 *
 * @yield Calls the block exactly once per loop iteration, just before the
 *   system blocks to wait for completed i/o.
 * @yieldparam prepare [self] itself
 * @yieldparam error [Rbuv::Error, nil] An exception or +nil+ if it was
 *   succesful
 * @return [self] itself
 */
static VALUE rbuv_prepare_start(VALUE self);

/*
 * Stop this prepare handle.
 *
 * @return [self] itself
 */
static VALUE rbuv_prepare_stop(VALUE self);

/* Private methods */
static void _uv_prepare_on_prepare(uv_prepare_t *uv_prepare, int status);
static void _uv_prepare_on_prepare_no_gvl(_uv_prepare_on_prepare_no_gvl_arg_t *arg);

void Init_rbuv_prepare() {
  cRbuvPrepare = rb_define_class_under(mRbuv, "Prepare", cRbuvHandle);
  rb_undef_alloc_func(cRbuvPrepare);
  rb_define_singleton_method(cRbuvPrepare, "new", rbuv_prepare_s_new, -1);

  rb_define_method(cRbuvPrepare, "start", rbuv_prepare_start, 0);
  rb_define_method(cRbuvPrepare, "stop", rbuv_prepare_stop, 0);
}
/*
 * Document-class: Rbuv::Prepare < Rbuv::Handle
 * Every active prepare handle gets its callback called exactly once per loop
 * iteration, just before the system blocks to wait for completed i/o.
 *
 * @!method initialize(loop=nil)
 *   Creates a new prepare handle.
 *
 *   @param loop [Rbuv::Loop, nil] loop object where this handle runs, if it is
 *     +nil+ then it the runs the handle in the {Rbuv::Loop.default}
 *   @return [Rbuv::Prepare]
 */
VALUE rbuv_prepare_s_new(int argc, VALUE *argv, VALUE klass) {
  VALUE loop;
  rb_scan_args(argc, argv, "01", &loop);
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }
  return rbuv_prepare_alloc(klass, loop);
}

VALUE rbuv_prepare_alloc(VALUE klass, VALUE loop) {
  rbuv_prepare_t *rbuv_prepare;
  rbuv_loop_t *rbuv_loop;
  VALUE prepare;

  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  rbuv_prepare = malloc(sizeof(*rbuv_prepare));
  rbuv_prepare->uv_handle = malloc(sizeof(*rbuv_prepare->uv_handle));
  uv_prepare_init(rbuv_loop->uv_handle, rbuv_prepare->uv_handle);
  rbuv_prepare->loop = loop;
  rbuv_prepare->cb_on_prepare = Qnil;

  prepare = Data_Wrap_Struct(klass, rbuv_prepare_mark, rbuv_prepare_free, rbuv_prepare);
  rbuv_prepare->uv_handle->data = (void *)prepare;
  rbuv_loop_register_handle(rbuv_loop, rbuv_prepare, prepare);

  RBUV_DEBUG_LOG_DETAIL("rbuv_prepare: %p, uv_handle: %p, prepare: %s",
                        rbuv_prepare, rbuv_prepare->uv_handle,
                        RSTRING_PTR(rb_inspect(prepare)));

  return prepare;
}

void rbuv_prepare_mark(rbuv_prepare_t *rbuv_prepare) {
  assert(rbuv_prepare);
  RBUV_DEBUG_LOG_DETAIL("rbuv_prepare: %p, uv_handle: %p, self: %lx",
                        rbuv_prepare, rbuv_prepare->uv_handle,
                        (VALUE)rbuv_prepare->uv_handle->data);
  rb_gc_mark(rbuv_prepare->cb_on_close);
  rb_gc_mark(rbuv_prepare->cb_on_prepare);
  rb_gc_mark(rbuv_prepare->loop);
}

void rbuv_prepare_free(rbuv_prepare_t *rbuv_prepare) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_prepare: %p, uv_handle: %p", rbuv_prepare, rbuv_prepare->uv_handle);
  rbuv_handle_free((rbuv_handle_t *)rbuv_prepare);
}

VALUE rbuv_prepare_start(VALUE self) {
  VALUE block;
  rbuv_prepare_t *rbuv_prepare;

  rb_need_block();
  block = rb_block_proc();

  Data_Get_Handle_Struct(self, rbuv_prepare_t, rbuv_prepare);
  rbuv_prepare->cb_on_prepare = block;

  RBUV_DEBUG_LOG_DETAIL("rbuv_prepare: %p, uv_handle: %p, _uv_prepare_on_prepare: %p, prepare: %s",
                        rbuv_prepare, rbuv_prepare->uv_handle, _uv_prepare_on_prepare,
                        RSTRING_PTR(rb_inspect(self)));
  uv_prepare_start(rbuv_prepare->uv_handle, _uv_prepare_on_prepare);

  return self;
}

VALUE rbuv_prepare_stop(VALUE self) {
  rbuv_prepare_t *rbuv_prepare;

  Data_Get_Handle_Struct(self, rbuv_prepare_t, rbuv_prepare);

  uv_prepare_stop(rbuv_prepare->uv_handle);

  return self;
}

void _uv_prepare_on_prepare(uv_prepare_t *uv_prepare, int status) {
  _uv_prepare_on_prepare_no_gvl_arg_t reg = { .uv_prepare = uv_prepare, .status = status };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)_uv_prepare_on_prepare_no_gvl, &reg);
}

void _uv_prepare_on_prepare_no_gvl(_uv_prepare_on_prepare_no_gvl_arg_t *arg) {
  uv_prepare_t *uv_prepare = arg->uv_prepare;
  int status = arg->status;

  VALUE prepare;
  VALUE error;
  rbuv_prepare_t *rbuv_prepare;

  prepare = (VALUE)uv_prepare->data;
  Data_Get_Handle_Struct(prepare, struct rbuv_prepare_s, rbuv_prepare);
  if (status < 0) {
    uv_err_t err = uv_last_error(uv_prepare->loop);
    error = rb_exc_new2(eRbuvError, uv_strerror(err));
  } else {
    error = Qnil;
  }
  rb_funcall(rbuv_prepare->cb_on_prepare, id_call, 2, prepare, error);
}
