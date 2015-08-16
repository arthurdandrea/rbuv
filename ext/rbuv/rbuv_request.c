#include "rbuv_request.h"

/*
 * Document-class: Rbuv::Request
 * @abstract
 *
 */

VALUE cRbuvRequest;

void rbuv_request_mark(rbuv_request_t *rbuv_request) {
}

/*
 * This is called when the Ruby CG is freeing a Rbuv::Request
 */
void rbuv_request_free(rbuv_request_t *rbuv_request) {
  RBUV_DEBUG_LOG_DETAIL("rbuv_request: %p, uv_req: %p", rbuv_request, rbuv_request->uv_req);

  if (rbuv_request->uv_req != NULL) {
    free(rbuv_request->uv_req);
  }
  free(rbuv_request);
}

/*
 * Cancel a pending request. Fails if the request is executing or has finished executing.
 *
 * @overload cancel
 * @return [self] returns itself
 */
static VALUE rbuv_request_cancel(VALUE self) {
  rbuv_request_t *rbuv_request;

  Data_Get_Struct(self, rbuv_request_t, rbuv_request);
  if (rbuv_request->uv_req == NULL) {
    rb_raise(eRbuvError, "This %s request is closed", rb_obj_classname(self));
  } else {
    RBUV_CHECK_UV_RETURN(uv_cancel(rbuv_request->uv_req));
  }
  return self;
}

void Init_rbuv_request() {
  cRbuvRequest = rb_define_class_under(mRbuv, "Request", rb_cObject);
  rb_undef_alloc_func(cRbuvRequest);

  rb_define_method(cRbuvRequest, "cancel", rbuv_request_cancel, 0);
}
