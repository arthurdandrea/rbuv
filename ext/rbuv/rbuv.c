#include "rbuv.h"

ID id_call;

VALUE mRbuv;

VALUE rbuv_version(VALUE self);
VALUE rbuv_version_string(VALUE self);

void Init_rbuv() {
  id_call = rb_intern("call");

  mRbuv = rb_define_module("Rbuv");
  rb_define_singleton_method(mRbuv, "version", rbuv_version, 0);
  rb_define_singleton_method(mRbuv, "version_string", rbuv_version_string, 0);

  Init_rbuv_error();
  Init_rbuv_handle();
  Init_rbuv_loop();
  Init_rbuv_timer();
  Init_rbuv_stream();
  Init_rbuv_tcp();
  Init_rbuv_signal();
  Init_rbuv_poll();
}

VALUE rbuv_version(VALUE self) {
  return UINT2NUM(uv_version());
}

VALUE rbuv_version_string(VALUE self) {
  return rb_str_new2(uv_version_string());
}
