#ifndef PYCOOL_HTTP_H
#define PYCOOL_HTTP_H

#include <stddef.h>

extern const char *SECURITY_HEADERS;

int url_decode(char *s);
int valid_simple(const char *s);
int valid_path(const char *s);
int valid_number(const char *s);
int valid_info(const char *s);

void send_all(int fd, const char *s, size_t n);
void send_response(int client, int status, const char *status_text,
                   const char *ctype, const char *body, size_t bodylen);

/* Returns 0 on success, -1 on I/O/timeout error, -2 if request too large. */
int read_request(int client, char *req, size_t reqsize);

#endif /* PYCOOL_HTTP_H */