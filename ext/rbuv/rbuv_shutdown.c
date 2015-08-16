#include "rbuv_shutdown.h"

VALUE cRbuvStreamShutdownRequest;

void rbuv_shutdown_mark(rbuv_shutdown_t* rbuv_shutdown) {
  rbuv_request_mark((rbuv_request_t *)rbuv_shutdown);
  rb_gc_mark(rbuv_shutdown->cb_on_shutdown);
  if (rbuv_shutdown->uv_req != NULL) {
    rb_gc_mark((VALUE)rbuv_shutdown->uv_req->handle->data);
  }
}

void rbuv_shutdown_free(rbuv_shutdown_t* rbuv_shutdown) {
  rbuv_request_free((rbuv_request_t *)rbuv_shutdown);
}

static VALUE rbuv_shutdown_get_handle(VALUE self) {
  rbuv_shutdown_t *rbuv_shutdown;
  Data_Get_Struct(self, rbuv_shutdown_t, rbuv_shutdown);
  if (rbuv_shutdown->uv_req == NULL) {
    return Qnil;
  } else {
    return (VALUE)rbuv_shutdown->uv_req->handle->data;
  }
}

void Init_rbuv_shutdown() {
  cRbuvStreamShutdownRequest = rb_define_class_under(cRbuvStream, "ShutdownRequest", cRbuvRequest);
  rb_undef_alloc_func(cRbuvStreamShutdownRequest);

  rb_define_method(cRbuvStreamShutdownRequest, "handle", rbuv_shutdown_get_handle, 0);
}
