#include "rbuv_loop.h"

/*
 * Document-class: Rbuv::Loop
 *
 * @!attribute [r] handles
 *   Every handle associated with this loop, note that handles are disassociated
 *   when they are closed.
 *   @return [Array<Rbuv::Handle>] every handle associated with this loop.
 *
 * @!attribute [r] ref_count
 *   The libuv internal count of active and referenced handles.
 *   @return the count of active and referenced handles.
 *
 * @!attribute [r] default
 *   @!scope class
 *   @return [Rbuv::Loop] the default loop
 *
 * @!method initialize
 *   Creates a new event loop.
 *   @return [Rbuv::Loop] a new loop
 */

struct rbuv_loop_run_arg_s {
  uv_loop_t* loop;
  int mode;
};
typedef struct rbuv_loop_run_arg_s rbuv_loop_run_arg_t;

VALUE cRbuvLoop;

/* Allocator/deallocator */
static VALUE rbuv_loop_alloc(VALUE klass);
static void rbuv_loop_mark(rbuv_loop_t *rbuv_loop);
static void rbuv_loop_free(rbuv_loop_t *rbuv_loop);

/* Private methods */
static void rbuv_walk_ary_push_cb(uv_handle_t* uv_handle, void* arg);
static void rbuv_walk_unregister_cb(uv_handle_t* uv_handle, void* arg);
static void rbuv_walk_gc_mark_cb(uv_handle_t *uv_handle, void *arg);
static void _rbuv_loop_run(VALUE self, uv_run_mode mode);
static void _rbuv_loop_run_no_gvl(rbuv_loop_run_arg_t *arg);

static VALUE rbuv_loop_alloc(VALUE klass) {
  rbuv_loop_t *rbuv_loop;
  VALUE loop;

  rbuv_loop = malloc(sizeof(*rbuv_loop));
  rbuv_loop->uv_handle = uv_loop_new();
  rbuv_loop->is_default = 0;

  loop = Data_Wrap_Struct(klass, rbuv_loop_mark, rbuv_loop_free, rbuv_loop);
  rbuv_loop->uv_handle->data = (void *)loop;

  RBUV_DEBUG_LOG_DETAIL("rbuv_loop: %p, uv_handle: %p, loop: %s",
                        rbuv_loop, rbuv_loop->uv_handle,
                        RSTRING_PTR(rb_inspect(loop)));

  return loop;
}

static void rbuv_loop_mark(rbuv_loop_t *rbuv_loop) {
  assert(rbuv_loop);
  RBUV_DEBUG_LOG_DETAIL("rbuv_loop: %p, uv_handle: %p, self: %lx",
                        rbuv_loop, rbuv_loop->uv_handle,
                        (VALUE)rbuv_loop->uv_handle->data);
  uv_walk(rbuv_loop->uv_handle, rbuv_walk_gc_mark_cb, NULL);
}

static void rbuv_loop_free(rbuv_loop_t *rbuv_loop) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_loop: %p, uv_handle: %p", rbuv_loop, rbuv_loop->uv_handle);
  RBUV_DEBUG_LOG_DETAIL("handles: %d", RHASH_SIZE(rbuv_loop->handles));

  uv_walk(rbuv_loop->uv_handle, rbuv_walk_unregister_cb, NULL);
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

/*
 * Runs the event loop until the reference count drops to zero.
 * @yield If the block is passed, calls it right before the first loop
 *   iteration.
 * @yieldparam loop [self] the loop itself
 * @return [self] itself
 */
static VALUE rbuv_loop_run(VALUE self) {
  _rbuv_loop_run(self, UV_RUN_DEFAULT);
  return self;
}

/*
 * Poll for new events once. Note that this function blocks if
 * there are no pending events.
 *
 * @yield (see #run)
 * @yieldparam loop (see #run)
 * @return (see #run)
 */
static VALUE rbuv_loop_run_once(VALUE self) {
  _rbuv_loop_run(self, UV_RUN_ONCE);
  return self;
}

/*
 * Poll for new events once but don't block if there are no
 * pending events.
 *
 * @yield (see #run)
 * @yieldparam loop (see #run)
 * @return (see #run)
 */
static VALUE rbuv_loop_run_nowait(VALUE self) {
  _rbuv_loop_run(self, UV_RUN_NOWAIT);
  return self;
}

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
static VALUE rbuv_loop_stop(VALUE self) {
  rbuv_loop_t *rbuv_loop;

  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);

  uv_stop(rbuv_loop->uv_handle);
  return self;
}

static VALUE rbuv_loop_get_handles(VALUE self) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  VALUE array = rb_ary_new();
  uv_walk(rbuv_loop->uv_handle, rbuv_walk_ary_push_cb, (void *)array);
  return array;
}

static VALUE rbuv_loop_get_ref_count(VALUE self) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  return UINT2NUM(rbuv_loop->uv_handle->active_handles);
}

static VALUE rbuv_loop_inspect(VALUE self) {
  const char *cname = rb_obj_classname(self);
  return rb_sprintf("#<%s:%p @handles=%s>", cname, (void*)self,
                    RSTRING_PTR(rb_inspect(rbuv_loop_get_handles(self))));
}

/* Private methods */

void rbuv_walk_ary_push_cb(uv_handle_t* uv_handle, void* arg) {
  VALUE array = (VALUE)arg;
  VALUE handle = (VALUE)uv_handle->data;
  rb_ary_push(array, handle);
}

void rbuv_walk_unregister_cb(uv_handle_t* uv_handle, void* arg) {
  // dont call if the object have already been GC'd
  VALUE handle = (VALUE)uv_handle->data;
  if (TYPE(handle) != T_NONE) {
    rbuv_handle_t *rbuv_handle = (rbuv_handle_t *)DATA_PTR(handle);
    rbuv_handle_unregister_loop(rbuv_handle);
  }
}

void rbuv_walk_gc_mark_cb(uv_handle_t *uv_handle, void *arg) {
  VALUE handle = (VALUE)uv_handle->data;
  rb_gc_mark(handle);
}

void _rbuv_loop_run(VALUE self, uv_run_mode mode) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  if (rb_block_given_p()) {
    rb_yield(self);
  }
  rbuv_loop_run_arg_t arg = {
    .mode = mode,
    .loop = rbuv_loop->uv_handle
  };
#ifdef HAVE_RB_THREAD_CALL_WITHOUT_GVL
  rb_thread_call_without_gvl((rbuv_rb_blocking_function_t)_rbuv_loop_run_no_gvl,
                             &arg, RUBY_UBF_IO, 0);
#else
  rb_thread_blocking_region((rb_blocking_function_t *)_rbuv_loop_run_no_gvl,
                            &arg, RUBY_UBF_IO, 0);
#endif
}

void _rbuv_loop_run_no_gvl(rbuv_loop_run_arg_t *arg) {
  uv_run(arg->loop, arg->mode);
}

void Init_rbuv_loop() {
  cRbuvLoop = rb_define_class_under(mRbuv, "Loop", rb_cObject);
  rb_define_alloc_func(cRbuvLoop, rbuv_loop_alloc);

  rb_define_method(cRbuvLoop, "run", rbuv_loop_run, 0);
  rb_define_method(cRbuvLoop, "stop", rbuv_loop_stop, 0);
  rb_define_method(cRbuvLoop, "run_once", rbuv_loop_run_once, 0);
  rb_define_method(cRbuvLoop, "run_nowait", rbuv_loop_run_nowait, 0);
  rb_define_method(cRbuvLoop, "handles", rbuv_loop_get_handles, 0);
  rb_define_method(cRbuvLoop, "ref_count", rbuv_loop_get_ref_count, 0);
  rb_define_method(cRbuvLoop, "inspect", rbuv_loop_inspect, 0);
  rb_define_singleton_method(cRbuvLoop, "default", rbuv_loop_s_default, 0);
}
