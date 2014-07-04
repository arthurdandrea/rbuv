#ifndef RBUV_HANDLE_H_
#define RBUV_HANDLE_H_

#include "rbuv.h"
struct rbuv_handle_s {
  uv_handle_t *uv_handle;
  VALUE loop;
  VALUE cb_on_close;
};

typedef struct rbuv_handle_s rbuv_handle_t;

extern VALUE cRbuvHandle;

void Init_rbuv_handle();

void rbuv_handle_unregister_loop(rbuv_handle_t *rbuv_handle);
void rbuv_handle_free(rbuv_handle_t *rbuv_handle);

#endif  /* RBUV_HANDLE_H_ */
