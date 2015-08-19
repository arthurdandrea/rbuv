#include "rbuv_getaddrinfo.h"

typedef struct {
  uv_getaddrinfo_t* uv_req;
  int status;
  struct addrinfo* res;
} rbuv_getaddrinfo_on_getaddrinfo_arg_t;
VALUE cRbuvGetaddrinfoRequest;

static void rbuv_getaddrinfo_on_getaddrinfo(uv_getaddrinfo_t* req, int status, struct addrinfo* res);
static void rbuv_getaddrinfo_on_getaddrinfo_no_gvl(rbuv_getaddrinfo_on_getaddrinfo_arg_t* arg);


void rbuv_getaddrinfo_mark(rbuv_getaddrinfo_t* rbuv_getaddrinfo) {
  rbuv_request_mark((rbuv_request_t *)rbuv_getaddrinfo);
  rb_gc_mark(rbuv_getaddrinfo->cb_on_getaddrinfo);
  if (rbuv_getaddrinfo->uv_req != NULL) {
    rb_gc_mark((VALUE)rbuv_getaddrinfo->uv_req->loop->data);
  }
}

void rbuv_getaddrinfo_free(rbuv_getaddrinfo_t* rbuv_getaddrinfo) {
  rbuv_request_free((rbuv_request_t *)rbuv_getaddrinfo);
}

VALUE rbuv_getaddrinfo_alloc(VALUE klass) {
  rbuv_getaddrinfo_t* rbuv_getaddrinfo;
  rbuv_getaddrinfo = malloc(sizeof(rbuv_getaddrinfo_t));
  rbuv_getaddrinfo->uv_req = NULL;
  rbuv_getaddrinfo->cb_on_getaddrinfo = Qnil;
  return Data_Wrap_Struct(klass, rbuv_getaddrinfo_mark, rbuv_getaddrinfo_free,
                          rbuv_getaddrinfo);
}

static VALUE rbuv_getaddrinfo_initialize(int argc, VALUE *argv, VALUE self) {
  rbuv_getaddrinfo_t *rbuv_getaddrinfo;
  rbuv_loop_t *rbuv_loop;
  VALUE nodename, srvname, loop;
  char *node, *service;
  int uv_ret;

  rb_scan_args(argc, argv, "03", &nodename, &srvname, &loop);
  rb_need_block();
  if (loop == Qnil) {
    loop = rbuv_loop_s_default(cRbuvLoop);
  }
  node = nodename == Qnil ? NULL : StringValueCStr(nodename);
  service = srvname == Qnil ? NULL : StringValueCStr(srvname);
  Data_Get_Struct(self, rbuv_getaddrinfo_t, rbuv_getaddrinfo);
  Data_Get_Struct(loop, rbuv_loop_t, rbuv_loop);


  rbuv_getaddrinfo->uv_req = malloc(sizeof(*rbuv_getaddrinfo->uv_req));
  uv_ret = uv_getaddrinfo(rbuv_loop->uv_handle, rbuv_getaddrinfo->uv_req, rbuv_getaddrinfo_on_getaddrinfo, node, service, NULL);
  if (uv_ret < 0) {
    free(rbuv_getaddrinfo->uv_req);
    rbuv_getaddrinfo->uv_req = NULL;
    rb_raise(eRbuvError, "%s", uv_strerror(uv_ret));
  } else {
    rbuv_getaddrinfo->uv_req->data = (void *)self;
    rbuv_getaddrinfo->cb_on_getaddrinfo = rb_block_proc();
    rbuv_loop_register_request(loop, self);
  }
  return self;
}

static VALUE rbuv_getaddrinfo_get_loop(VALUE self) {
  rbuv_getaddrinfo_t *rbuv_getaddrinfo;
  Data_Get_Struct(self, rbuv_getaddrinfo_t, rbuv_getaddrinfo);
  if (rbuv_getaddrinfo->uv_req == NULL) {
    return Qnil;
  } else {
    return (VALUE)rbuv_getaddrinfo->uv_req->loop->data;
  }
}
static void rbuv_getaddrinfo_on_getaddrinfo(uv_getaddrinfo_t* uv_req, int status, struct addrinfo* res) {
  rbuv_getaddrinfo_on_getaddrinfo_arg_t arg = {
    .uv_req = uv_req,
    .status = status,
    .res = res
  };
  rb_thread_call_with_gvl((rbuv_rb_blocking_function_t)rbuv_getaddrinfo_on_getaddrinfo_no_gvl, &arg);
}

static VALUE rbuv_getaddrinfo_on_getaddrinfo_no_gvl2(VALUE args) {
  rbuv_getaddrinfo_on_getaddrinfo_arg_t* arg = (rbuv_getaddrinfo_on_getaddrinfo_arg_t*)args;
  if (arg->status < 0) {
    rb_raise(eRbuvError, "%s", uv_strerror(arg->status));
    return Qnil;
  } else {
    struct addrinfo *ptr;
    VALUE result = rb_ary_new();
    for (ptr = arg->res; ptr != NULL; ptr = ptr->ai_next) {
      if (ptr->ai_addrlen != 0) {
        VALUE array[6];
        switch(ptr->ai_family) {
          case AF_INET:
            array[0] = rb_str_new_cstr("AF_INET");
            break;
          case AF_INET6:
            array[0] = rb_str_new_cstr("AF_INET6");
            break;
          default:
            array[0] = rb_sprintf("unknown:%d", ptr->ai_family);
            break;
        }
        rbuv_util_extractname2(ptr->ai_addr, ptr->ai_addrlen, &(array[2]), &(array[1]));
        array[3] = INT2NUM(ptr->ai_family);
        array[4] = INT2NUM(ptr->ai_socktype);
        array[5] = INT2NUM(ptr->ai_protocol);
        rb_ary_push(result, rb_ary_new4(6, array));
      }
    }
    return result;
  }
}

static void rbuv_getaddrinfo_on_getaddrinfo_no_gvl(rbuv_getaddrinfo_on_getaddrinfo_arg_t* arg) {
  VALUE cb_on_getaddrinfo;
  rbuv_getaddrinfo_t *rbuv_getaddrinfo;
  VALUE request = (VALUE)arg->uv_req->data;

  Data_Get_Struct(request, rbuv_getaddrinfo_t, rbuv_getaddrinfo);

  cb_on_getaddrinfo = rbuv_getaddrinfo->cb_on_getaddrinfo;
  rbuv_getaddrinfo->cb_on_getaddrinfo = Qnil;
  rbuv_run_callback(cb_on_getaddrinfo, rbuv_getaddrinfo_on_getaddrinfo_no_gvl2, (VALUE)arg);
  uv_freeaddrinfo(arg->res);
  rbuv_loop_unregister_request((VALUE)arg->uv_req->loop->data, request);
  free(rbuv_getaddrinfo->uv_req);
  rbuv_getaddrinfo->uv_req = NULL;
}

void Init_rbuv_getaddrinfo() {
  cRbuvGetaddrinfoRequest = rb_define_class_under(mRbuv, "GetaddrinfoRequest", cRbuvRequest);
  rb_define_alloc_func(cRbuvGetaddrinfoRequest, rbuv_getaddrinfo_alloc);
  rb_define_method(cRbuvGetaddrinfoRequest, "initialize", rbuv_getaddrinfo_initialize, -1);
  rb_define_method(cRbuvGetaddrinfoRequest, "loop", rbuv_getaddrinfo_get_loop, 0);
}
