#include "rbuv_async.h"

struct rbuv_async_s {
  uv_async_t *uv_handle;
  VALUE loop;
  VALUE cb_on_close;
  VALUE cb_on_async;
};
typedef struct rbuv_async_s rbuv_async_t;

struct rbuv_async_on_async_arg_s {
  uv_async_t *uv_async;
  int status;
};
typedef struct rbuv_async_on_async_arg_s rbuv_async_on_async_arg_t;

VALUE cRbuvAsync;

/* Allocator / Mark / Deallocator */
static VALUE rbuv_async_alloc(VALUE klass);
static void rbuv_async_mark(rbuv_async_t *rbuv_async);
static void rbuv_async_free(rbuv_async_t *rbuv_async);

/* Private methods */
static void rbuv_async_on_async(uv_async_t *uv_async, int status);
static void rbuv_async_on_async_no_gvl(rbuv_async_on_async_arg_t *arg);

static VALUE rbuv_async_alloc(VALUE klass) {
  rbuv_async_t *rbuv_async;

  rbuv_async = malloc(sizeof(*rbuv_async));
  rbuv_async->uv_handle = NULL;
  rbuv_async->loop = Qnil;
  rbuv_async->cb_on_async = Qnil;
  return Data_Wrap_Struct(klass, rbuv_async_mark, rbuv_async_free, rbuv_async);
}

static void rbuv_async_mark(rbuv_async_t *rbuv_async) {
  assert(rbuv_async);
  RBUV_DEBUG_LOG_DETAIL("rbuv_async: %p, uv_handle: %p, self: %lx",
                        rbuv_async, rbuv_async->uv_handle,
                        (VALUE)rbuv_async->uv_handle->data);
  rb_gc_mark(rbuv_async->cb_on_close);
  rb_gc_mark(rbuv_async->cb_on_async);
  rb_gc_mark(rbuv_async->loop);
}

static void rbuv_async_free(rbuv_async_t *rbuv_async) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_async: %p, uv_handle: %p", rbuv_async, rbuv_async->uv_handle);
  rbuv_handle_free((rbuv_handle_t *)rbuv_async);
}

/*
 * @overload initialize(loop = nil)
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
static VALUE rbuv_async_intialize(int argc, VALUE *argv, VALUE self) {
  VALUE loop;
  rbuv_loop_t *rbuv_loop;
  rbuv_async_t *rbuv_async;

  rb_scan_args(argc, argv, "01", &loop);
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }

  rb_need_block(); // Raise error if block is not given

  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  Data_Get_Struct(self, rbuv_async_t, rbuv_async);
  rbuv_async->loop = loop;
  rbuv_async->cb_on_async = rb_block_proc();
  rbuv_async->uv_handle = malloc(sizeof(*rbuv_async->uv_handle));
  rbuv_async->uv_handle->data = (void *)self;
  uv_async_init(rbuv_loop->uv_handle, rbuv_async->uv_handle,
                rbuv_async_on_async);
  return self;
}

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
static VALUE rbuv_async_send(VALUE self) {
  rbuv_async_t *rbuv_async;

  Data_Get_Handle_Struct(self, rbuv_async_t, rbuv_async);

  RBUV_DEBUG_LOG_DETAIL("rbuv_async: %p, uv_handle: %p, rbuv_async_on_async: %p, async: %s",
                        rbuv_async, rbuv_async->uv_handle, rbuv_async_on_async,
                        RSTRING_PTR(rb_inspect(self)));
  uv_async_send(rbuv_async->uv_handle);
  return self;
}

static void rbuv_async_on_async(uv_async_t *uv_async, int status) {
  rbuv_async_on_async_arg_t arg = {
    .uv_async = uv_async,
    .status = status
  };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)rbuv_async_on_async_no_gvl, &arg);
}

static void rbuv_async_on_async_no_gvl(rbuv_async_on_async_arg_t *arg) {
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

void Init_rbuv_async() {
  cRbuvAsync = rb_define_class_under(mRbuv, "Async", cRbuvHandle);
  rb_define_alloc_func(cRbuvAsync, rbuv_async_alloc);

  rb_define_method(cRbuvAsync, "initialize", rbuv_async_intialize, -1);
  rb_define_method(cRbuvAsync, "send", rbuv_async_send, 0);
}

/* This have to be declared after Init_* so it can replace YARD bad assumption
 * for parent class beeing RbuvHandle not Rbuv::Handle.
 * Also it need some text after document-class statement otherwise YARD won't
 * parse it
 */

/*
 * Document-class: Rbuv::Async < Rbuv::Handle
 *
 * Handle used to wakeup the event loop from another thread.
 */
