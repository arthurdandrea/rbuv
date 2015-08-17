#include "rbuv_util.h"

VALUE rbuv_util_extractname(struct sockaddr* sockname, int namelen) {
  int addr_type;
  VALUE ip;
  ushort *port;
  char *ip_ptr = malloc(sizeof(char) * namelen);
  if (sockname->sa_family == AF_INET6) {
    struct sockaddr_in6 sockname_in6 = *(struct sockaddr_in6*) sockname;
    port = &sockname_in6.sin6_port;
    uv_ip6_name(&sockname_in6, ip_ptr, namelen);
  } else if (sockname->sa_family == AF_INET) {
    struct sockaddr_in sockname_in = *(struct sockaddr_in*) sockname;
    port = &sockname_in.sin_port;
    uv_ip4_name(&sockname_in, ip_ptr, namelen);
  } else {
    free(ip_ptr); // free before jumping out of here
    rb_raise(eRbuvError, "unexpected error");
    return Qnil; // to satisfy compilers
  }
  ip = rb_str_new_cstr(ip_ptr);
  free(ip_ptr);

  return rb_ary_new3(2, ip, UINT2NUM(ntohs(*port)));
}
