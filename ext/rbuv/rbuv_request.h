#ifndef RBUV_REQUEST_H_
#define RBUV_REQUEST_H_

#include "rbuv.h"
struct rbuv_request_s {
  uv_req_t *uv_req;
};

typedef struct rbuv_request_s rbuv_request_t;

extern VALUE cRbuvRequest;

void Init_rbuv_request();

void rbuv_request_mark(rbuv_request_t *rbuv_request);
void rbuv_request_free(rbuv_request_t *rbuv_request);

#endif  /* RBUV_REQUEST_H_ */
