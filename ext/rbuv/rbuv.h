#ifndef RBUV_H_
#define RBUV_H_

#include <assert.h>

#include <ruby.h>
#ifdef HAVE_RUBY_THREAD_H
# include <ruby/thread.h>
#endif
#include <uv.h>

#include "rbuv_debug.h"

#include "rbuv_error.h"
#include "rbuv_handle.h"
#include "rbuv_loop.h"
#include "rbuv_timer.h"
#include "rbuv_request.h"
#include "rbuv_write.h"
#include "rbuv_shutdown.h"
#include "rbuv_getaddrinfo.h"
#include "rbuv_stream.h"
#include "rbuv_tcp.h"
#include "rbuv_signal.h"
#include "rbuv_poll.h"
#include "rbuv_prepare.h"
#include "rbuv_check.h"
#include "rbuv_async.h"
#include "rbuv_util.h"
#include "rbuv_idle.h"

extern ID id_call;

extern VALUE mRbuv;

#define RBUV_CHECK_UV_RETURN(uv_ret) do { \
  if (uv_ret < 0) { \
    rb_raise(eRbuvError, "%s", uv_strerror(uv_ret)); \
  } \
} while(0)

#define RBUV_OFFSETOF(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define RBUV_CONTAINTER_OF(ptr, type, member) ({ \
  const typeof( ((type *)0)->member ) *__mptr = (ptr); \
  (type *)( (char *)__mptr - RBUV_OFFSETOF(type, member) );})
#define Data_Get_Handle_Struct(obj, type, sval) do { \
  Data_Get_Struct(obj, type, sval); \
  if (sval->uv_handle == NULL) { \
    rb_raise(eRbuvError, "This %s handle is closed", rb_obj_classname(obj));\
  } \
} while(0);

typedef void *(*rbuv_rb_blocking_function_t)(void *);

#endif  /* RBUV_H_ */
