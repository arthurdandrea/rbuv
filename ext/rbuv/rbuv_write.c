#include "rbuv_write.h"

VALUE cRbuvStreamWriteRequest;

void rbuv_write_mark(rbuv_write_t* rbuv_write) {
  rbuv_request_mark((rbuv_request_t *)rbuv_write);
  rb_gc_mark(rbuv_write->cb_on_write);
  if (rbuv_write->uv_req != NULL) {
    rb_gc_mark((VALUE)rbuv_write->uv_req->handle->data);
  }
}
void rbuv_write_free(rbuv_write_t* rbuv_write) {
  if (rbuv_write->uv_buf.base != NULL) {
    free(rbuv_write->uv_buf.base);
    rbuv_write->uv_buf.base = NULL;
  }
  rbuv_request_free((rbuv_request_t *)rbuv_write);
}

static VALUE rbuv_write_get_handle(VALUE self) {
  rbuv_write_t *rbuv_write;
  Data_Get_Struct(self, rbuv_write_t, rbuv_write);
  if (rbuv_write->uv_req == NULL) {
    return Qnil;
  } else {
    return (VALUE)rbuv_write->uv_req->handle->data;
  }
}

void Init_rbuv_write() {
  cRbuvStreamWriteRequest = rb_define_class_under(cRbuvStream, "WriteRequest", cRbuvRequest);
  rb_undef_alloc_func(cRbuvStreamWriteRequest);

  rb_define_method(cRbuvStreamWriteRequest, "handle", rbuv_write_get_handle, 0);
}
