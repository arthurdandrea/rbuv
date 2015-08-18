#ifndef RBUV_UTIL_H_
#define RBUV_UTIL_H_

#include "rbuv.h"

VALUE rbuv_util_extractname(struct sockaddr* sockname, int namelen);
int rbuv_util_extractname2(struct sockaddr* sockname, int namelen, VALUE *ip, VALUE *port);
void rbuv_run_callback(VALUE callback, VALUE (* proc)(ANYARGS), VALUE args);
#endif  /* RBUV_UTIL_H_ */
