#ifndef RBUV_LOOP_H_
#define RBUV_LOOP_H_

#include "rbuv.h"

struct rbuv_loop_s {
  uv_loop_t* uv_handle;
  int is_default;
  ID run_mode;
  VALUE requests;
};
typedef struct rbuv_loop_s rbuv_loop_t;

extern VALUE cRbuvLoop;

VALUE rbuv_loop_s_default(VALUE klass);
void rbuv_loop_register_request(VALUE loop, VALUE request);
void rbuv_loop_unregister_request(VALUE loop, VALUE request);
void Init_rbuv_loop();

#endif  /* RBUV_LOOP_H_ */
