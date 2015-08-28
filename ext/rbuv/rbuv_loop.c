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
ID RBUV_RUN_NOT_RUNNING;
ID RBUV_RUN_DEFAULT;
ID RBUV_RUN_ONCE;
ID RBUV_RUN_NOWAIT;

/* Allocator/deallocator */
static VALUE rbuv_loop_alloc(VALUE klass);
static void rbuv_loop_mark(rbuv_loop_t *rbuv_loop);
static void rbuv_loop_free(rbuv_loop_t *rbuv_loop);

/* Private methods */
static void rbuv_walk_ary_push_cb(uv_handle_t* uv_handle, void* arg);
static void rbuv_walk_unregister_cb(uv_handle_t* uv_handle, void* arg);
static void rbuv_walk_gc_mark_cb(uv_handle_t *uv_handle, void *arg);
static VALUE _rbuv_loop_run(VALUE self);
static void _rbuv_loop_run_no_gvl(rbuv_loop_run_arg_t *arg);
static VALUE rbuv_loop_get_handles2(rbuv_loop_t *rbuv_loop);

static VALUE rbuv_loop_alloc(VALUE klass) {
  rbuv_loop_t *rbuv_loop;
  VALUE loop;
  int uv_ret;

  rbuv_loop = malloc(sizeof(*rbuv_loop));
  rbuv_loop->uv_handle = malloc(sizeof(*rbuv_loop->uv_handle));
  uv_ret = uv_loop_init(rbuv_loop->uv_handle);
  if (uv_ret < 0) {
    free(rbuv_loop->uv_handle);
    rbuv_loop->uv_handle = NULL;
    free(rbuv_loop);
    rb_raise(eRbuvError, "%s", uv_strerror(uv_ret));
    return Qnil;
  }
  rbuv_loop->is_default = 0;
  rbuv_loop->run_mode = RBUV_RUN_NOT_RUNNING;
  rbuv_loop->requests = rb_ary_new();

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
  rb_gc_mark(rbuv_loop->requests);
}

static void rbuv_loop_free(rbuv_loop_t *rbuv_loop) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_loop: %p, uv_handle: %p", rbuv_loop, rbuv_loop->uv_handle);

  uv_walk(rbuv_loop->uv_handle, rbuv_walk_unregister_cb, NULL);
  if (rbuv_loop->is_default == 0) {
    uv_loop_close(rbuv_loop->uv_handle);
    free(rbuv_loop->uv_handle);
  } else {
    uv_loop_close(rbuv_loop->uv_handle);
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
    rbuv_loop->run_mode = RBUV_RUN_NOT_RUNNING;
    rbuv_loop->requests = rb_ary_new();

    loop = Data_Wrap_Struct(klass, rbuv_loop_mark, rbuv_loop_free, rbuv_loop);
    rbuv_loop->uv_handle->data = (void *)loop;

    RBUV_DEBUG_LOG_DETAIL("rbuv_loop: %p, uv_handle: %p, loop: %s",
                          rbuv_loop, rbuv_loop->uv_handle,
                          RSTRING_PTR(rb_inspect(loop)));

    rb_ivar_set(klass, _default, loop);
  }
  return loop;
}

static VALUE _rbuv_loop_after_run(VALUE self) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  rbuv_loop->run_mode = RBUV_RUN_NOT_RUNNING;
  return self;
}
/*
 * @api private
 * @overload _run(run_mode=:default)
 *   Runs the event loop.
 *
 *   If +run_mode+ is +:default+ Runs the event loop until the reference count
 *   drops to zero.
 *
 *   If +run_mode+ is +:once+ poll for new events once. Note that this function
 *   blocks if there are no pending events.
 *
 *   If +run_mode+ is +:nowait+ poll for new events once but don't block if
 *   there are no pending events.
 *
 *   @param run_mode [:default,:once,:nowait] the run mode
 *   @return [self] itself
 */
static VALUE rbuv_loop_run(int argc, VALUE *argv, VALUE self) {
  VALUE run_mode;
  ID run_mode_id;
  rbuv_loop_t *rbuv_loop;

  rb_scan_args(argc, argv, "01", &run_mode);
  if (run_mode == Qnil) {
    run_mode_id = RBUV_RUN_DEFAULT;
  } else {
    run_mode_id = SYM2ID(run_mode);
  }

  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  if (rbuv_loop->run_mode != RBUV_RUN_NOT_RUNNING) {
    rb_raise(eRbuvError, "This %s loop is already running", rb_obj_classname(self));
  }
  rbuv_loop->run_mode = run_mode_id;
  rb_ensure(_rbuv_loop_run, self, _rbuv_loop_after_run, self);
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
  return rbuv_loop_get_handles2(rbuv_loop);
}

static VALUE rbuv_loop_get_handles2(rbuv_loop_t *rbuv_loop) {
  VALUE array = rb_ary_new();
  uv_walk(rbuv_loop->uv_handle, rbuv_walk_ary_push_cb, (void *)array);
  return array;
}

static VALUE rbuv_loop_get_requests(VALUE self) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  return rb_ary_dup(rbuv_loop->requests);
}

static VALUE rbuv_loop_get_ref_count(VALUE self) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  return UINT2NUM(rbuv_loop->uv_handle->active_handles);
}

static VALUE rbuv_loop_inspect(VALUE self) {
  rbuv_loop_t *rbuv_loop;
  const char *cname = rb_obj_classname(self);
  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  return rb_sprintf("#<%s:%p @handles=%s @requests=%s>", cname, (void*)self,
                    RSTRING_PTR(rb_inspect(rbuv_loop_get_handles2(rbuv_loop))),
                    RSTRING_PTR(rb_inspect(rbuv_loop->requests)));
}

static VALUE rbuv_loop_now(VALUE self) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  uint64_t now = uv_now(rbuv_loop->uv_handle);
  return UINT2NUM(now);
}

static VALUE rbuv_loop_update_time(VALUE self) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  uv_update_time(rbuv_loop->uv_handle);
  return self;
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

VALUE _rbuv_loop_run(VALUE self) {
  rbuv_loop_t *rbuv_loop;
  rbuv_loop_run_arg_t arg;
  uv_run_mode mode;

  Data_Get_Struct(self, rbuv_loop_t, rbuv_loop);
  arg.loop = rbuv_loop->uv_handle;
  if (rbuv_loop->run_mode == RBUV_RUN_DEFAULT) {
    arg.mode = UV_RUN_DEFAULT;
  } else if (rbuv_loop->run_mode == RBUV_RUN_ONCE) {
    arg.mode = UV_RUN_ONCE;
  } else if (rbuv_loop->run_mode == RBUV_RUN_NOWAIT) {
    arg.mode = UV_RUN_NOWAIT;
  } else {
    arg.mode = UV_RUN_DEFAULT; // TODO: raise error? better implementation?
  }
#ifdef HAVE_RB_THREAD_CALL_WITHOUT_GVL
  rb_thread_call_without_gvl((rbuv_rb_blocking_function_t)_rbuv_loop_run_no_gvl,
                             &arg, RUBY_UBF_IO, 0);
#else
  rb_thread_blocking_region((rb_blocking_function_t *)_rbuv_loop_run_no_gvl,
                            &arg, RUBY_UBF_IO, 0);
#endif
  return self;
}

void _rbuv_loop_run_no_gvl(rbuv_loop_run_arg_t *arg) {
  uv_run(arg->loop, arg->mode);
}

void rbuv_loop_register_request(VALUE loop, VALUE request) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  rb_ary_push(rbuv_loop->requests, request);
}
void rbuv_loop_unregister_request(VALUE loop, VALUE request) {
  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);
  rbuv_ary_delete_same_object(rbuv_loop->requests, request);
}


void Init_rbuv_loop() {
  RBUV_RUN_NOT_RUNNING = rb_intern("not_running");
  RBUV_RUN_DEFAULT = rb_intern("default");
  RBUV_RUN_ONCE = rb_intern("once");
  RBUV_RUN_NOWAIT = rb_intern("nowait");
  cRbuvLoop = rb_define_class_under(mRbuv, "Loop", rb_cObject);
  rb_define_alloc_func(cRbuvLoop, rbuv_loop_alloc);

  rb_define_method(cRbuvLoop, "_run", rbuv_loop_run, -1);
  rb_define_method(cRbuvLoop, "stop", rbuv_loop_stop, 0);
  rb_define_method(cRbuvLoop, "handles", rbuv_loop_get_handles, 0);
  rb_define_method(cRbuvLoop, "requests", rbuv_loop_get_requests, 0);
  rb_define_method(cRbuvLoop, "ref_count", rbuv_loop_get_ref_count, 0);
  rb_define_method(cRbuvLoop, "inspect", rbuv_loop_inspect, 0);
  rb_define_method(cRbuvLoop, "now", rbuv_loop_now, 0);
  rb_define_method(cRbuvLoop, "update_time", rbuv_loop_update_time, 0);
  rb_define_singleton_method(cRbuvLoop, "default", rbuv_loop_s_default, 0);
}
