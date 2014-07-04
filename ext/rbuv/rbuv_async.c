#include "rbuv_async.h"

struct rbuv_async_s {
  uv_async_t *uv_handle;
  VALUE loop;
  VALUE cb_on_close;
  VALUE cb_on_async;
};

typedef struct _uv_async_on_async_no_gvl_arg_s {
  uv_async_t *uv_async;
  int status;
} _uv_async_on_async_no_gvl_arg_t;

VALUE cRbuvAsync;

/* Allocator/deallocator */
/*
 * @api private
 */
static VALUE rbuv_async_s_new(int argc, VALUE *argv, VALUE klass);
static void rbuv_async_mark(rbuv_async_t *rbuv_async);
static void rbuv_async_free(rbuv_async_t *rbuv_async);
/* Private Allocatator */
static VALUE rbuv_async_alloc(VALUE klass, VALUE loop, VALUE block);

/* Methods */

/*
 * Wake up the event loop and call the async handle's block.
 *
 * @note There is no guarantee that every {#send} call leads to exactly one
 *   invocation of the block; the only guarantee is that the block function
 *   is called at least once after the call to {#send}.
 * @note Unlike all other {Rbuv} functions, {#send} can be called from another
 *   thread.
 * @return [self] itself
 */
static VALUE rbuv_async_send(VALUE self);

/* Private methods */
static void _uv_async_on_async(uv_async_t *uv_async, int status);
static void _uv_async_on_async_no_gvl(_uv_async_on_async_no_gvl_arg_t *arg);

void Init_rbuv_async() {
  cRbuvAsync = rb_define_class_under(mRbuv, "Async", cRbuvHandle);
  rb_undef_alloc_func(cRbuvAsync);
  rb_define_singleton_method(cRbuvAsync, "new", rbuv_async_s_new, -1);

  rb_define_method(cRbuvAsync, "send", rbuv_async_send, 0);
}
/*
 * Document-class: Rbuv::Async < Rbuv::Handle
 * Handle used to wakeup the event loop from another thread.
 *
 * @!method initialize(loop=nil)
 *   Creates a new async handle.
 *
 *   @param loop [Rbuv::Loop, nil] loop object where this handle runs, if it is
 *     +nil+ then it the runs the handle in the {Rbuv::Loop.default}
 *   @yield Calls the block (on the event loop thread) after receiving {#send}.
 *   @yieldparam async [self] itself
 *   @yieldparam error [Rbuv::Error, nil] An exception or +nil+ if it was
 *     succesful
 *
 *   @return [Rbuv::Async]
 */
VALUE rbuv_async_s_new(int argc, VALUE *argv, VALUE klass) {
  VALUE loop;
  VALUE block;
  rb_scan_args(argc, argv, "01", &loop);
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }
  rb_need_block();
  block = rb_block_proc();

  return rbuv_async_alloc(klass, loop, block);
}

VALUE rbuv_async_alloc(VALUE klass, VALUE loop, VALUE block) {
  rbuv_async_t *rbuv_async;
  rbuv_loop_t *rbuv_loop;
  VALUE async;

  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  rbuv_async = malloc(sizeof(*rbuv_async));
  rbuv_async->uv_handle = malloc(sizeof(*rbuv_async->uv_handle));
  uv_async_init(rbuv_loop->uv_handle, rbuv_async->uv_handle,
                _uv_async_on_async);
  rbuv_async->loop = loop;
  rbuv_async->cb_on_async = block;

  async = Data_Wrap_Struct(klass, rbuv_async_mark, rbuv_async_free, rbuv_async);
  rbuv_async->uv_handle->data = (void *)async;
  rbuv_loop_register_handle(rbuv_loop, rbuv_async, async);

  RBUV_DEBUG_LOG_DETAIL("rbuv_async: %p, uv_handle: %p, async: %s",
                        rbuv_async, rbuv_async->uv_handle,
                        RSTRING_PTR(rb_inspect(async)));

  return async;
}

void rbuv_async_mark(rbuv_async_t *rbuv_async) {
  assert(rbuv_async);
  RBUV_DEBUG_LOG_DETAIL("rbuv_async: %p, uv_handle: %p, self: %lx",
                        rbuv_async, rbuv_async->uv_handle,
                        (VALUE)rbuv_async->uv_handle->data);
  rb_gc_mark(rbuv_async->cb_on_close);
  rb_gc_mark(rbuv_async->cb_on_async);
  rb_gc_mark(rbuv_async->loop);
}

void rbuv_async_free(rbuv_async_t *rbuv_async) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_async: %p, uv_handle: %p", rbuv_async, rbuv_async->uv_handle);
  rbuv_handle_free((rbuv_handle_t *)rbuv_async);
}

VALUE rbuv_async_send(VALUE self) {
  VALUE block;
  rbuv_async_t *rbuv_async;

  Data_Get_Handle_Struct(self, rbuv_async_t, rbuv_async);

  RBUV_DEBUG_LOG_DETAIL("rbuv_async: %p, uv_handle: %p, _uv_async_on_async: %p, async: %s",
                        rbuv_async, rbuv_async->uv_handle, _uv_async_on_async,
                        RSTRING_PTR(rb_inspect(self)));
  uv_async_send(rbuv_async->uv_handle);

  return self;
}

void _uv_async_on_async(uv_async_t *uv_async, int status) {
  _uv_async_on_async_no_gvl_arg_t reg = { .uv_async = uv_async, .status = status };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)_uv_async_on_async_no_gvl, &reg);
}

void _uv_async_on_async_no_gvl(_uv_async_on_async_no_gvl_arg_t *arg) {
  uv_async_t *uv_async = arg->uv_async;
  int status = arg->status;

  VALUE async;
  VALUE error;
  rbuv_async_t *rbuv_async;

  async = (VALUE)uv_async->data;
  Data_Get_Handle_Struct(async, struct rbuv_async_s, rbuv_async);
  if (status < 0) {
    uv_err_t err = uv_last_error(uv_async->loop);
    error = rb_exc_new2(eRbuvError, uv_strerror(err));
  } else {
    error = Qnil;
  }
  rb_funcall(rbuv_async->cb_on_async, id_call, 2, async, error);
}
