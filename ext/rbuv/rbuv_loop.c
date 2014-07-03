#include "rbuv_loop.h"

typedef struct {
  uv_loop_t* loop;
  int mode;
} _rbuv_loop_run_arg_t;

VALUE cRbuvLoop;

/* Methods */

/*
 * Runs the event loop until the reference count drops to zero.
 * @yield If the block is passed, calls it right in the first loop iteration.
 * @yieldparam loop [self] the loop itself
 * @return [self] itself
 */
static VALUE rbuv_loop_run(VALUE loop);

/*
 * This function will stop the event loop by forcing {#run} or {#run_once} or
 * {#run_nowait} to end as soon as possible, but not sooner than the next loop
 * iteration.
 *
 * If this function was called before blocking for i/o, the loop won't
 * block for i/o on this iteration.
 *
 * @return [self] itself
 */
static VALUE rbuv_loop_stop(VALUE loop);

/*
 * Poll for new events once. Note that this function blocks if
 * there are no pending events.
 *
 * @yield (see #run)
 * @yieldparam loop (see #run)
 * @return (see #run)
 */
static VALUE rbuv_loop_run_once(VALUE loop);

/*
 * Poll for new events once but don't block if there are no
 * pending events.
 *
 * @yield (see #run)
 * @yieldparam loop (see #run)
 * @return (see #run)
 */
static VALUE rbuv_loop_run_nowait(VALUE loop);

static VALUE rbuv_loop_get_handles(VALUE loop);
static VALUE rbuv_loop_uv_active_handles(VALUE loop);
static VALUE rbuv_loop_inspect(VALUE loop);

/* Allocator/deallocator */
static VALUE rbuv_loop_alloc(VALUE klass);
static void rbuv_loop_mark(rbuv_loop_t *rbuv_loop);
static void rbuv_loop_free(rbuv_loop_t *rbuv_loop);

/* Private methods */
static int rbuv_loop_free_foreach_handle(VALUE key, VALUE handle, void *ptr);
static void _rbuv_loop_run(VALUE self, uv_run_mode mode);
static void _rbuv_loop_run_no_gvl(_rbuv_loop_run_arg_t *arg);

void Init_rbuv_loop() {
  cRbuvLoop = rb_define_class_under(mRbuv, "Loop", rb_cObject);
  rb_define_alloc_func(cRbuvLoop, rbuv_loop_alloc);

  rb_define_method(cRbuvLoop, "run", rbuv_loop_run, 0);
  rb_define_method(cRbuvLoop, "stop", rbuv_loop_stop, 0);
  rb_define_method(cRbuvLoop, "run_once", rbuv_loop_run_once, 0);
  rb_define_method(cRbuvLoop, "run_nowait", rbuv_loop_run_nowait, 0);
  rb_define_method(cRbuvLoop, "handles", rbuv_loop_get_handles, 0);
  rb_define_method(cRbuvLoop, "uv_active_handles", rbuv_loop_uv_active_handles, 0);
  rb_define_method(cRbuvLoop, "inspect", rbuv_loop_inspect, 0);
  rb_define_singleton_method(cRbuvLoop, "default", rbuv_loop_s_default, 0);
}

/*
 * Document-class: Rbuv::Loop
 *
 * @!attribute [r] handles
 *   Every handle associated with this loop, note that handles are disassociated
 *   when they are closed.
 *   @return [Array<Rbuv::Handle>] every handle associated with this loop.
 *
 * @!attribute [r] default
 *   @!scope class
 *   @return [Rbuv::Loop] the default loop
 *
 * @!method initialize
 *   Creates a new event loop.
 *   @return [Rbuv::Loop] a new loop
 */
VALUE rbuv_loop_alloc(VALUE klass) {
  rbuv_loop_t *rbuv_loop;
  VALUE loop;

  rbuv_loop = malloc(sizeof(*rbuv_loop));
  rbuv_loop->handles = rb_hash_new();
  rbuv_loop->uv_handle = uv_loop_new();
  rbuv_loop->is_default = 0;

  loop = Data_Wrap_Struct(klass, rbuv_loop_mark, rbuv_loop_free, rbuv_loop);
  rbuv_loop->uv_handle->data = (void *)loop;

  RBUV_DEBUG_LOG_DETAIL("rbuv_loop: %p, uv_handle: %p, loop: %s",
                        rbuv_loop, rbuv_loop->uv_handle,
                        RSTRING_PTR(rb_inspect(loop)));

  return loop;
}

void rbuv_loop_mark(rbuv_loop_t *rbuv_loop) {
  assert(rbuv_loop);
  RBUV_DEBUG_LOG_DETAIL("rbuv_loop: %p, uv_handle: %p, self: %lx",
                        rbuv_loop, rbuv_loop->uv_handle,
                        (VALUE)rbuv_loop->uv_handle->data);
  rb_gc_mark(rbuv_loop->handles);
}

int rbuv_loop_free_foreach_handle(VALUE key, VALUE handle, void *ptr) {
  // dont call if the object have already been GC'd
  if (TYPE(handle) != T_NONE) {
    rbuv_handle_t *rbuv_handle = (rbuv_handle_t *)DATA_PTR(handle);
    rbuv_handle_unregister_loop(rbuv_handle);
  }
#ifdef RBUV_RBX
  return 2; // ST_DELETE
#else
  return ST_DELETE;
#endif
}

void rbuv_loop_free(rbuv_loop_t *rbuv_loop) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_loop: %p, uv_handle: %p", rbuv_loop, rbuv_loop->uv_handle);
  RBUV_DEBUG_LOG_DETAIL("handles: %d", RHASH_SIZE(rbuv_loop->handles));

  rb_hash_foreach(rbuv_loop->handles, rbuv_loop_free_foreach_handle, Qnil);
  if (rbuv_loop->is_default == 0) {
    uv_loop_delete(rbuv_loop->uv_handle);
  }

  free(rbuv_loop);
}

VALUE rbuv_loop_s_default(VALUE klass) {
  ID _default = rb_intern("@default");
  VALUE loop = rb_ivar_get(klass, _default);
  if (loop == Qnil) {
    rbuv_loop_t *rbuv_loop;

    rbuv_loop = malloc(sizeof(*rbuv_loop));
    rbuv_loop->handles = rb_hash_new();
    rbuv_loop->uv_handle = uv_default_loop();
    rbuv_loop->is_default = 1;

    loop = Data_Wrap_Struct(klass, rbuv_loop_mark, rbuv_loop_free, rbuv_loop);
    rbuv_loop->uv_handle->data = (void *)loop;

    RBUV_DEBUG_LOG_DETAIL("rbuv_loop: %p, uv_handle: %p, loop: %s",
                          rbuv_loop, rbuv_loop->uv_handle,
                          RSTRING_PTR(rb_inspect(loop)));

    rb_ivar_set(klass, _default, loop);
  }
  return loop;
}

VALUE rbuv_loop_run(VALUE self) {
  _rbuv_loop_run(self, UV_RUN_DEFAULT);
  return self;
}

VALUE rbuv_loop_stop(VALUE self) {
  rbuv_loop_t *rbuv_loop;

  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);

  uv_stop(rbuv_loop->uv_handle);
  return self;
}

VALUE rbuv_loop_run_once(VALUE self) {
  _rbuv_loop_run(self, UV_RUN_ONCE);
  return self;
}

VALUE rbuv_loop_run_nowait(VALUE self) {
  _rbuv_loop_run(self, UV_RUN_NOWAIT);
  return self;
}

VALUE rbuv_loop_get_handles(VALUE self) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  return rb_funcall(rbuv_loop->handles, rb_intern("values"), 0);
}

VALUE rbuv_loop_uv_active_handles(VALUE self) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  return UINT2NUM(rbuv_loop->uv_handle->active_handles);
}

VALUE rbuv_loop_inspect(VALUE self) {
  const char *cname = rb_obj_classname(self);
  return rb_sprintf("#<%s:%p @handles=%s>", cname, (void*)self,
                    RSTRING_PTR(rb_inspect(rbuv_loop_get_handles(self))));
}

void rbuv_loop_register_handle(rbuv_loop_t *rbuv_loop, void *rbuv_handle, VALUE handle) {
  long pointer = (long)rbuv_handle;
  RBUV_DEBUG_LOG_DETAIL("registering %lx", pointer);
  rb_hash_aset(rbuv_loop->handles, LONG2FIX(pointer), handle);
}

void rbuv_loop_unregister_handle(rbuv_loop_t *rbuv_loop, void *rbuv_handle) {
  long pointer = (long)rbuv_handle;
  RBUV_DEBUG_LOG_DETAIL("unregistering %lx from %lp", pointer, rbuv_loop);
  rb_hash_delete(rbuv_loop->handles, LONG2FIX(pointer));
}

void _rbuv_loop_run(VALUE self, uv_run_mode mode) {
  rbuv_loop_t *rbuv_loop;

  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  _rbuv_loop_run_arg_t arg = { .mode = mode, .loop = rbuv_loop->uv_handle};
#ifdef HAVE_RB_THREAD_CALL_WITHOUT_GVL
  rb_thread_call_without_gvl((rbuv_rb_blocking_function_t)_rbuv_loop_run_no_gvl,
                             &arg, RUBY_UBF_IO, 0);
#else
  rb_thread_blocking_region((rb_blocking_function_t *)_rbuv_loop_run_no_gvl,
                            &arg, RUBY_UBF_IO, 0);
#endif
}

void _rbuv_loop_run_no_gvl(_rbuv_loop_run_arg_t *arg) {
  uv_run(arg->loop, arg->mode);
}
