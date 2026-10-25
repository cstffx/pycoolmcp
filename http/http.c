#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define MAX_REQUEST   8192
#define MAX_OUTPUT    65536
#define MAX_ARGV      128
#define CHILD_TIMEOUT 10

static char pycoolctl_path[4096];
static int srv_fd = -1;

static const char *SECURITY_HEADERS =
	"X-Content-Type-Options: nosniff\r\n"
	"X-Frame-Options: DENY\r\n"
	"Cache-Control: no-store\r\n";

static void send_all(int fd, const char *s, size_t n)
{
	size_t off = 0;
	while (off < n) {
		ssize_t w = send(fd, s + off, n - off, 0);
		if (w <= 0) break;
		off += w;
	}
}

static int url_decode(char *s)
{
	char *o = s;
	while (*s) {
		if (*s == '+') { *o++ = ' '; s++; }
		else if (*s == '%' && s[1] && s[2]) {
			char hex[3] = { s[1], s[2], 0 };
			char *end = NULL;
			long v = strtol(hex, &end, 16);
			if (end != hex + 2) return -1;
			if (v <= 0 || v > 255) return -1; /* reject NUL and overlong */
			*o++ = (char)v;
			s += 3;
		} else {
			*o++ = *s++;
		}
	}
	*o = '\0';
	return 0;
}

static int valid_simple(const char *s)
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

static int valid_path(const char *s)
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

static int valid_number(const char *s)
{
	if (!s || !*s) return 0;
	size_t len = strlen(s);
	if (len > 32) return 0;
	for (size_t i = 0; i < len; i++) {
		if (!isdigit((unsigned char)s[i])) return 0;
	}
	return 1;
}

static int valid_info(const char *s)
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

static void send_response(int client, int status, const char *status_text,
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
	send_all(client, header, hlen);
	if (body && bodylen) send_all(client, body, bodylen);
}

static void run_query(int client, const char *subcmd, const char *query)
{
	char buf[4096];
	strncpy(buf, query ? query : "", sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	char *argv[MAX_ARGV];
	int argc = 0;
	argv[argc++] = "pycoolctl";
	argv[argc++] = (char *)subcmd;

	char *save = NULL;
	char *tok = strtok_r(buf, "&", &save);
	while (tok && argc < MAX_ARGV - 4) {
		char *eq = strchr(tok, '=');
		if (eq) {
			*eq = '\0';
			char *key = tok;
			char *val = eq + 1;
			if (url_decode(key) != 0 || url_decode(val) != 0) {
				tok = strtok_r(NULL, "&", &save);
				continue;
			}
			int handled = 0;
			if (strcmp(key, "name") == 0 || strcmp(key, "class") == 0) {
				if (valid_simple(val)) { argv[argc++] = val; handled = 1; }
			} else if (strcmp(key, "method") == 0) {
				if (valid_simple(val)) {
					argv[argc++] = "--method";
					argv[argc++] = val;
					handled = 1;
				}
			} else if (strcmp(key, "file") == 0) {
				if (valid_path(val)) {
					argv[argc++] = "--file";
					argv[argc++] = val;
					handled = 1;
				}
			} else if (strcmp(key, "db") == 0) {
				if (valid_path(val)) {
					argv[argc++] = "--db";
					argv[argc++] = val;
					handled = 1;
				}
			} else if (strcmp(key, "pid") == 0) {
				if (valid_number(val)) {
					argv[argc++] = "--pid";
					argv[argc++] = val;
					handled = 1;
				}
			} else if (strcmp(key, "info") == 0) {
				if (valid_info(val)) {
					argv[argc++] = "--info";
					argv[argc++] = val;
					handled = 1;
				}
			} else if (strcmp(key, "body") == 0) {
				argv[argc++] = "--body";
				handled = 1;
			} else if (strcmp(key, "refs") == 0) {
				argv[argc++] = "--refs";
				handled = 1;
			} else if (strcmp(key, "json") == 0) {
				argv[argc++] = "--json";
				handled = 1;
			} else if (strcmp(key, "compact") == 0) {
				argv[argc++] = "--compact";
				handled = 1;
			} else if (strcmp(key, "full") == 0) {
				argv[argc++] = "--full";
				handled = 1;
			} else if (strcmp(key, "limit") == 0) {
				if (valid_number(val)) {
					argv[argc++] = "--limit";
					argv[argc++] = val;
					handled = 1;
				}
			} else if (strcmp(key, "offset") == 0) {
				if (valid_number(val)) {
					argv[argc++] = "--offset";
					argv[argc++] = val;
					handled = 1;
				}
			}
			(void)handled;
		}
		tok = strtok_r(NULL, "&", &save);
	}
	argv[argc] = NULL;

	int pipefd[2];
	if (pipe(pipefd) != 0) {
		send_response(client, 500, "Internal Server Error", "text/plain; charset=utf-8", "pipe error\n", 11);
		return;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]); close(pipefd[1]);
		send_response(client, 500, "Internal Server Error", "text/plain; charset=utf-8", "fork error\n", 11);
		return;
	}
	if (pid == 0) {
		close(pipefd[0]);
		if (srv_fd >= 0) close(srv_fd);
		close(client);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);
		char *clean_env[] = { "PATH=/usr/local/bin:/usr/bin:/bin", NULL };
		execve(pycoolctl_path, argv, clean_env);
		perror("exec");
		_exit(127);
	}
	close(pipefd[1]);

	fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
	char out[MAX_OUTPUT];
	size_t off = 0;
	time_t start = time(NULL);
	int child_done = 0;
	while (!child_done) {
		if (time(NULL) - start > CHILD_TIMEOUT) {
			kill(pid, SIGKILL);
			child_done = 1;
			break;
		}
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(pipefd[0], &rfds);
		struct timeval tv = { 0, 100000 };
		int sel = select(pipefd[0] + 1, &rfds, NULL, NULL, &tv);
		if (sel > 0 && FD_ISSET(pipefd[0], &rfds)) {
			if (off < MAX_OUTPUT - 1) {
				ssize_t r = read(pipefd[0], out + off, (MAX_OUTPUT - 1) - off);
				if (r > 0) {
					off += r;
				} else if (r == 0) {
					child_done = 1;
				} else if (errno != EAGAIN && errno != EWOULDBLOCK) {
					child_done = 1;
				}
			} else {
				/* output too large: stop child */
				kill(pid, SIGKILL);
				child_done = 1;
			}
		}
		pid_t w = waitpid(pid, NULL, WNOHANG);
		if (w == pid) child_done = 1;
		if (w < 0) child_done = 1;
	}
	/* drain remaining */
	while (off < MAX_OUTPUT - 1) {
		ssize_t r = read(pipefd[0], out + off, (MAX_OUTPUT - 1) - off);
		if (r <= 0) break;
		off += r;
	}
	out[off] = '\0';
	close(pipefd[0]);
	waitpid(pid, NULL, 0);

	send_response(client, 200, "OK", "text/plain; charset=utf-8", out, strlen(out));
}

static int read_request(int client, char *req, size_t reqsize)
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
	return 0;
}

int main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);

	char *slash = strrchr(argv[0], '/');
	if (slash) {
		snprintf(pycoolctl_path, sizeof(pycoolctl_path), "%.*s/bin/pycoolctl",
		         (int)(slash - argv[0]), argv[0]);
	} else {
		snprintf(pycoolctl_path, sizeof(pycoolctl_path), "./bin/pycoolctl");
	}
	char resolved[4096];
	if (realpath(pycoolctl_path, resolved) != NULL) {
		strncpy(pycoolctl_path, resolved, sizeof(pycoolctl_path) - 1);
		pycoolctl_path[sizeof(pycoolctl_path) - 1] = '\0';
	}
	if (access(pycoolctl_path, X_OK) != 0) {
		snprintf(pycoolctl_path, sizeof(pycoolctl_path), "pycoolctl");
	}

	int port = 8080;
	const char *bind_addr = "127.0.0.1";
	if (argc >= 2) port = atoi(argv[1]);
	if (argc >= 3) bind_addr = argv[2];
	if (port <= 0 || port > 65535) {
		fprintf(stderr, "invalid port: %d\n", port);
		return 1;
	}

	srv_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (srv_fd < 0) { perror("socket"); return 1; }
	fcntl(srv_fd, F_SETFD, FD_CLOEXEC);
	int opt = 1;
	setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, bind_addr, &addr.sin_addr) != 1) {
		fprintf(stderr, "invalid bind address: %s\n", bind_addr);
		close(srv_fd);
		return 1;
	}
	if (bind(srv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); close(srv_fd); return 1; }
	if (listen(srv_fd, 16) < 0) { perror("listen"); close(srv_fd); return 1; }
	printf("pycool http listening on %s:%d\n", bind_addr, port);
	fflush(stdout);

	for (;;) {
		int client = accept(srv_fd, NULL, NULL);
		if (client < 0) {
			if (errno == EINTR) continue;
			continue;
		}
		struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
		setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

		char req[MAX_REQUEST];
		if (read_request(client, req, sizeof(req)) != 0) {
			close(client);
			continue;
		}

		char method[16], path[1024];
		method[0] = '\0'; path[0] = '\0';
		if (sscanf(req, "%15s %1023s", method, path) != 2) {
			send_response(client, 400, "Bad Request", "text/plain; charset=utf-8", "bad request\n", 12);
			close(client);
			continue;
		}
		if (strcmp(method, "GET") != 0) {
			const char *body = "method not allowed\n";
			char h[256];
			snprintf(h, sizeof(h),
				"HTTP/1.1 405 Method Not Allowed\r\n"
				"Allow: GET\r\n"
				"Content-Type: text/plain; charset=utf-8\r\n"
				"Content-Length: %zu\r\n"
				"Connection: close\r\n"
				"%s"
				"\r\n", strlen(body), SECURITY_HEADERS);
			send_all(client, h, strlen(h));
			send_all(client, body, strlen(body));
			close(client);
			continue;
		}

		/* validate path characters */
		int badpath = 0;
		for (char *p = path; *p; p++) {
			unsigned char c = (unsigned char)*p;
			if (c < 0x20 || c == 0x7f) { badpath = 1; break; }
		}
		if (badpath) {
			send_response(client, 400, "Bad Request", "text/plain; charset=utf-8", "bad request\n", 12);
			close(client);
			continue;
		}

		char *q = strchr(path, '?');
		char query[4096] = "";
		if (q) {
			*q = '\0';
			strncpy(query, q + 1, sizeof(query) - 1);
			query[sizeof(query) - 1] = '\0';
		}

		if (strcmp(path, "/") == 0) {
			const char *body = "pycools http ok\n";
			send_response(client, 200, "OK", "text/plain; charset=utf-8", body, strlen(body));
		} else if (strcmp(path, "/class") == 0) {
			run_query(client, "class", query);
		} else if (strcmp(path, "/method") == 0) {
			run_query(client, "method", query);
		} else {
			const char *body = "not found\n";
			send_response(client, 404, "Not Found", "text/plain; charset=utf-8", body, strlen(body));
		}
		close(client);
	}
	return 0;
}