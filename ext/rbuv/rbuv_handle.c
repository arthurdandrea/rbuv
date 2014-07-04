#include "rbuv_handle.h"

typedef struct {
  uv_handle_t *uv_handle;
} _uv_handle_on_close_arg_t;

VALUE cRbuvHandle;

/* Methods */

/*
 * Manually modify the event loop's reference count. Useful to revert a call to
 * {#unref}
 *
 * @return [self] returns itself
 * @raise [Rbuv::Error] if handle is closed
 */
static VALUE rbuv_handle_ref(VALUE self);

/*
 * Manually modify the event loop's reference count. Useful if the user wants
 * to have a handle or timeout that doesn't keep the loop alive.
 *
 * @return [self] returns itself
 * @raise [Rbuv::Error] if handle is closed
 */
static VALUE rbuv_handle_unref(VALUE self);

/*
 * Request handle to be closed.
 *
 * @overload close
 * @overload close
 *   @yield The block will be called asynchronously after this handle is closed.
 *   @yieldparam handle [self] the handle itself
 * @return [self] returns itself
 */
static VALUE rbuv_handle_close(VALUE self);

/*
 * Used to determine whether a handle is active
 *
 * @return [Boolean] true if this +handle+ is active, +false+ otherwise
 * @raise [Rbuv::Error] if handle is closed
 */
static VALUE rbuv_handle_is_active(VALUE self);

/*
 * Used to determine whether a handle is closed
 *
 * @return [Boolean] true if this +handle+ is closed, +false+ otherwise
 */
static VALUE rbuv_handle_is_closed(VALUE self);

/*
 * Used to determine whether a handle is closing
 *
 * @return [Boolean] true if this +handle+ is closing, +false+ otherwise
 * @raise [Rbuv::Error] if handle is closed
 */
static VALUE rbuv_handle_is_closing(VALUE self);

/* Private methods */
static void _uv_handle_close(uv_handle_t *uv_handle);
static void _uv_handle_on_close(uv_handle_t *uv_handle);
static void _uv_handle_on_close_no_gvl(_uv_handle_on_close_arg_t *arg);

void Init_rbuv_handle() {
  cRbuvHandle = rb_define_class_under(mRbuv, "Handle", rb_cObject);
  rb_undef_alloc_func(cRbuvHandle);

  rb_define_method(cRbuvHandle, "ref", rbuv_handle_ref, 0);
  rb_define_method(cRbuvHandle, "unref", rbuv_handle_unref, 0);
  rb_define_method(cRbuvHandle, "close", rbuv_handle_close, 0);
  rb_define_method(cRbuvHandle, "active?", rbuv_handle_is_active, 0);
  rb_define_method(cRbuvHandle, "closing?", rbuv_handle_is_closing, 0);
  rb_define_method(cRbuvHandle, "closed?", rbuv_handle_is_closed, 0);
}

/*
 * Document-class: Rbuv::Handle
 * @abstract
 */

/*
 * This is called when the Ruby CG is freeing a Rbuv::Loop
 *
 * The loop free method in turn call this method for each associated handle,
 * where we remove any reference to the dying loop, for libuv happiness we also
 * close the handle if it has not been closed before.
 */
void rbuv_handle_unregister_loop(rbuv_handle_t *rbuv_handle) {
  rbuv_handle->loop = Qnil;
  if (rbuv_handle->uv_handle != NULL) {
    if (_rbuv_handle_is_closing(rbuv_handle)) {
      rb_warn("The GC freed Rbuv::Loop before the Rbuv::Handle#close completed. Consider using Rbuv::Loop#dispose\n");
    } else {
      rb_warn("The GC freed Rbuv::Loop before the Rbuv::Handle#close is called. Consider using Rbuv::Loop#dispose\n");
      uv_close(rbuv_handle->uv_handle, NULL);
      rbuv_handle->uv_handle = NULL;
      free(rbuv_handle->uv_handle);
    }
  }
}

/*
 * This is called when the Ruby CG is freeing a Rbuv::Handle
 */
void rbuv_handle_free(rbuv_handle_t *rbuv_handle) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_handle: %p, uv_handle: %p", rbuv_handle, rbuv_handle->uv_handle);
  if ((TYPE(rbuv_handle->loop) != T_NONE) && (rbuv_handle->loop != Qnil)) {

#ifdef RBUV_RBX
    rbuv_loop_t *rbuv_loop;
    rbuv_loop = (rbuv_loop_t*)DATA_PTR(rbuv_handle->loop);
    rbuv_loop_unregister_handle(rbuv_loop, rbuv_handle);
#else
    // dont call if the loop is about to be GC'd
    if (TYPE(rbuv_handle->loop) != T_ZOMBIE) {
      rbuv_loop_t *rbuv_loop;
      rbuv_loop = (rbuv_loop_t*)DATA_PTR(rbuv_handle->loop);
      rbuv_loop_unregister_handle(rbuv_loop, rbuv_handle);
    }
#endif
    if (rbuv_handle->uv_handle != NULL) {
      if (_rbuv_handle_is_closing(rbuv_handle)) {
        rb_warn("The GC freed the Rbuv::Handle before #close completed.Consider using Rbuv::Loop#dispose\n");
      } else {
        rb_warn("The GC freed the Rbuv::Handle before #close is called.Consider using Rbuv::Loop#dispose\n");
        uv_close(rbuv_handle->uv_handle, NULL);
      }
    }
  }
  if (rbuv_handle->uv_handle != NULL) {
    free(rbuv_handle->uv_handle);
  }
  free(rbuv_handle);
}

VALUE rbuv_handle_ref(VALUE self) {
  rbuv_handle_t *rbuv_handle;
  Data_Get_Handle_Struct(self, rbuv_handle_t, rbuv_handle);
  uv_ref(rbuv_handle->uv_handle);
  return self;
}

VALUE rbuv_handle_unref(VALUE self) {
  rbuv_handle_t *rbuv_handle;
  Data_Get_Handle_Struct(self, rbuv_handle_t, rbuv_handle);
  uv_unref(rbuv_handle->uv_handle);
  return self;
}

VALUE rbuv_handle_close(VALUE self) {
  rbuv_handle_t *rbuv_handle;
  VALUE block;

  if (rb_block_given_p()) {
    block = rb_block_proc();
  } else {
    block = Qnil;
  }

  Data_Get_Handle_Struct(self, rbuv_handle_t, rbuv_handle);
  _rbuv_handle_close(rbuv_handle, block);
  return self;
}

VALUE rbuv_handle_is_closed(VALUE self) {
  rbuv_handle_t *rbuv_handle;

  Data_Get_Struct(self, rbuv_handle_t, rbuv_handle);

  return (rbuv_handle->uv_handle == NULL) ? Qtrue : Qfalse;
}

VALUE rbuv_handle_is_active(VALUE self) {
  rbuv_handle_t *rbuv_handle;

  Data_Get_Handle_Struct(self, rbuv_handle_t, rbuv_handle);
  return _rbuv_handle_is_active(rbuv_handle) ? Qtrue : Qfalse;
}

VALUE rbuv_handle_is_closing(VALUE self) {
  rbuv_handle_t *rbuv_handle;

  Data_Get_Handle_Struct(self, rbuv_handle_t, rbuv_handle);
  return _rbuv_handle_is_closing(rbuv_handle) ? Qtrue : Qfalse;
}

int _rbuv_handle_is_active(rbuv_handle_t *rbuv_handle) {
  assert(rbuv_handle);
  return uv_is_active(rbuv_handle->uv_handle);
}

int _rbuv_handle_is_closing(rbuv_handle_t *rbuv_handle) {
  assert(rbuv_handle);
  return uv_is_closing(rbuv_handle->uv_handle);
}

void _rbuv_handle_close(rbuv_handle_t *rbuv_handle, VALUE block) {
  assert(rbuv_handle);
  RBUV_DEBUG_LOG_DETAIL("rbuv_handle: %p, uv_handle: %p, block: %s",
                        rbuv_handle, rbuv_handle->uv_handle,
                        RSTRING_PTR(rb_inspect(block)));
  if (!_rbuv_handle_is_closing(rbuv_handle)) {
    RBUV_DEBUG_LOG_DETAIL("closing rbuv_handle: %p, uv_handle: %p, block: %s",
                          rbuv_handle, rbuv_handle->uv_handle,
                          RSTRING_PTR(rb_inspect(block)));
    rbuv_handle->cb_on_close = block;
    _uv_handle_close(rbuv_handle->uv_handle);
  }
}

void _uv_handle_close(uv_handle_t *uv_handle) {
  assert(uv_handle);
  uv_close(uv_handle, _uv_handle_on_close);
}

void _uv_handle_on_close(uv_handle_t *uv_handle) {
  _uv_handle_on_close_arg_t arg = { .uv_handle = uv_handle };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)_uv_handle_on_close_no_gvl, &arg);
}

void _uv_handle_on_close_no_gvl(_uv_handle_on_close_arg_t *arg) {
  uv_handle_t *uv_handle = arg->uv_handle;

  VALUE handle;
  rbuv_handle_t *rbuv_handle;
  VALUE on_close;

  handle = (VALUE)uv_handle->data;

  RBUV_DEBUG_LOG_DETAIL("uv_handle: %p, handle: %s",
                        uv_handle, RSTRING_PTR(rb_inspect(handle)));

  Data_Get_Handle_Struct(handle, rbuv_handle_t, rbuv_handle);
  free(rbuv_handle->uv_handle);
  rbuv_handle->uv_handle = NULL;

  rbuv_loop_t *rbuv_loop;
  Data_Get_Struct(rbuv_handle->loop, rbuv_loop_t, rbuv_loop);
  rbuv_loop_unregister_handle(rbuv_loop, rbuv_handle);
  rbuv_handle->loop = Qnil;

  on_close = rbuv_handle->cb_on_close;
  rbuv_handle->cb_on_close = Qnil;

  RBUV_DEBUG_LOG_DETAIL("handle: %s, on_close: %s",
                        RSTRING_PTR(rb_inspect(handle)),
                        RSTRING_PTR(rb_inspect(on_close)));

  if (RTEST(on_close)) {
    rb_funcall(on_close, id_call, 1, handle);
  }
}
