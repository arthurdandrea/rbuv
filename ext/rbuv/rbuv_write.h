#ifndef RBUV_WRITE_H_
#define RBUV_WRITE_H_

#include "rbuv.h"

typedef struct {
  uv_write_t *uv_req;
  uv_buf_t uv_buf;
  VALUE cb_on_write;
} rbuv_write_t;

extern VALUE cRbuvStreamWriteRequest;

void rbuv_write_mark(rbuv_write_t* rbuv_write);
void rbuv_write_free(rbuv_write_t* rbuv_write);
void Init_rbuv_write();

#endif  /* RBUV_WRITE_H_ */
