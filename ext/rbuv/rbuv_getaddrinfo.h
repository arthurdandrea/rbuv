#ifndef RBUV_GETADDRINFO_H_
#define RBUV_GETADDRINFO_H_

#include "rbuv.h"

typedef struct {
  uv_getaddrinfo_t *uv_req;
  VALUE cb_on_getaddrinfo;
} rbuv_getaddrinfo_t;

extern VALUE cRbuvGetaddrinfoRequest;

void rbuv_getaddrinfo_mark(rbuv_getaddrinfo_t* rbuv_getaddrinfo);
void rbuv_getaddrinfo_free(rbuv_getaddrinfo_t* rbuv_getaddrinfo);
void Init_rbuv_getaddrinfo();

#endif  /* RBUV_GETADDRINFO_H_ */
