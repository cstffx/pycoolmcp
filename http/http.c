#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "http.h"

#define MAX_REQUEST 8192

const char *SECURITY_HEADERS =
	"X-Content-Type-Options: nosniff\r\n"
	"X-Frame-Options: DENY\r\n"
	"Cache-Control: no-store\r\n";

void send_all(int fd, const char *s, size_t n)
{
	size_t off = 0;
	while (off < n) {
		ssize_t w = send(fd, s + off, n - off, 0);
		if (w <= 0) break;
		off += w;
	}
}

int url_decode(char *s)
{
	char *o = s;
	while (*s) {
		if (*s == '+') { *o++ = ' '; s++; }
		else if (*s == '%' && s[1] && s[2]) {
			char hex[3] = { s[1], s[2], 0 };
			char *end = NULL;
			long v = strtol(hex, &end, 16);
			if (end != hex + 2) return -1;
			/* reject NUL (v==0) and any out-of-byte-range value */
			if (v <= 0 || v > 255) return -1;
			*o++ = (char)v;
			s += 3;
		} else {
			*o++ = *s++;
		}
	}
	*o = '\0';
	return 0;
}

int valid_simple(const char *s)
{
	if (!s || !*s) return 0;
	size_t len = strlen(s);
	if (len > 256) return 0;
	if (s[0] == '-') return 0;
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		if (c < 0x20 || c == 0x7f) return 0;
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') return 0;
	}
	return 1;
}

int valid_path(const char *s)
{
	if (!s || !*s) return 0;
	size_t len = strlen(s);
	if (len > 4096) return 0;
	if (s[0] == '-') return 0;
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		if (c < 0x20 || c == 0x7f) return 0;
	}
	if (strstr(s, "..") != NULL) return 0;
	return 1;
}

int valid_number(const char *s)
{
	if (!s || !*s) return 0;
	size_t len = strlen(s);
	if (len > 32) return 0;
	for (size_t i = 0; i < len; i++) {
		if (!isdigit((unsigned char)s[i])) return 0;
	}
	return 1;
}

int valid_info(const char *s)
{
	if (!s || !*s) return 0;
	size_t len = strlen(s);
	if (len > 16) return 0;
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		if (!(c >= 'A' && c <= 'L')) return 0;
	}
	return 1;
}

void send_response(int client, int status, const char *status_text,
                   const char *ctype, const char *body, size_t bodylen)
{
	char header[512];
	int hlen = snprintf(header, sizeof(header),
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: %s\r\n"
		"Content-Length: %zu\r\n"
		"Connection: close\r\n"
		"%s"
		"\r\n", status, status_text, ctype, bodylen, SECURITY_HEADERS);
	/* Guard against snprintf truncation/overflow: never trust hlen blindly. */
	if (hlen < 0) hlen = 0;
	if (hlen >= (int)sizeof(header)) hlen = (int)sizeof(header) - 1;
	send_all(client, header, (size_t)hlen);
	if (body && bodylen) send_all(client, body, bodylen);
}

int read_request(int client, char *req, size_t reqsize)
{
	size_t total = 0;
	req[0] = '\0';
	while (total < reqsize - 1) {
		ssize_t n = recv(client, req + total, reqsize - 1 - total, 0);
		if (n < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		if (n == 0) break;
		total += (size_t)n;
		req[total] = '\0';
		if (strstr(req, "\r\n\r\n") != NULL) break;
	}
	if (total == 0) return -1;
	if (memchr(req, '\0', total) != NULL) return -1;
	/* If buffer filled without a complete header block, request is too large. */
	if (total == reqsize - 1 && strstr(req, "\r\n\r\n") == NULL) return -2;
	return 0;
}