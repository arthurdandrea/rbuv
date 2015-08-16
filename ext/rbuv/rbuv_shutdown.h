#ifndef RBUV_SHUTDOWN_H_
#define RBUV_SHUTDOWN_H_

#include "rbuv.h"

typedef struct {
  uv_shutdown_t *uv_req;
  VALUE cb_on_shutdown;
} rbuv_shutdown_t;

extern VALUE cRbuvStreamShutdownRequest;

void rbuv_shutdown_mark(rbuv_shutdown_t* rbuv_shutdown);
void rbuv_shutdown_free(rbuv_shutdown_t* rbuv_shutdown);
void Init_rbuv_shutdown();

#endif  /* RBUV_SHUTDOWN_H_ */
