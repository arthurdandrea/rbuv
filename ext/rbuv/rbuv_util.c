#include "rbuv_util.h"
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

VALUE rbuv_util_extractname(struct sockaddr* sockname, int namelen) {
  VALUE ip;
  VALUE port;
  if (!rbuv_util_extractname2(sockname, namelen, &ip, &port)) {
    rb_raise(eRbuvError, "unexpected error");
    return Qnil; // to satisfy compilers
  } else {
    return rb_ary_new3(2, ip, port);
  }
}

int rbuv_util_extractname2(struct sockaddr* sockname, int namelen, VALUE *ip, VALUE *port) {
  if (sockname->sa_family == AF_INET6) {
    char ip_ptr[INET6_ADDRSTRLEN + 1] = "";
    struct sockaddr_in6 *sockname_in6 = (struct sockaddr_in6*) sockname;
    *port = UINT2NUM(ntohs(sockname_in6->sin6_port));
    RBUV_CHECK_UV_RETURN(uv_ip6_name(sockname_in6, ip_ptr, sizeof(ip_ptr)));
    *ip = rb_str_new_cstr(ip_ptr);
    return 1;
  } else if (sockname->sa_family == AF_INET) {
    char ip_ptr[INET_ADDRSTRLEN + 1] = "";
    struct sockaddr_in *sockname_in = (struct sockaddr_in*) sockname;
    *port = UINT2NUM(ntohs(sockname_in->sin_port));
    RBUV_CHECK_UV_RETURN(uv_ip4_name(sockname_in, ip_ptr, sizeof(ip_ptr)));
    *ip = rb_str_new_cstr(ip_ptr);
    return 1;
  } else {
    *port = INT2FIX(-1);
    *ip = rb_str_new_cstr("unknown");
    return 0;
  }
}

typedef struct {
  VALUE args;
  VALUE (* proc)(ANYARGS);
} rbuv_run_callback_arg_t;

VALUE rbuv_run_callback_begin(VALUE args) {
  rbuv_run_callback_arg_t *arg = (rbuv_run_callback_arg_t *)args;
  return rb_ary_new3(2, arg->proc(arg->args), Qnil);
}

VALUE rbuv_run_callback_rescue(VALUE args, VALUE exception_object) {
  return rb_ary_new3(2, Qnil, exception_object);
}

void rbuv_run_callback(VALUE callback, VALUE (* proc)(ANYARGS), VALUE args) {
  rbuv_run_callback_arg_t arg = {
    .args = args,
    .proc = proc
  };
  VALUE result_arr = rb_rescue(rbuv_run_callback_begin, (VALUE)&arg, rbuv_run_callback_rescue, Qnil);
  rb_funcall(callback, id_call, 2, rb_ary_entry(result_arr, 0), rb_ary_entry(result_arr, 1));
}

void rbuv_ary_delete_same_object(VALUE ary, VALUE obj) {
  long i;
  for (i = 0; i < RARRAY_LEN(ary); i++) {
    if (rb_ary_entry(ary, i) == obj) {
      rb_ary_delete_at(ary, i);
      break;
    }
  }
}
