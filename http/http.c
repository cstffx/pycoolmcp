#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

static void url_decode(char *s)
{
	char *o = s;
	while (*s) {
		if (*s == '+') { *o++ = ' '; s++; }
		else if (*s == '%' && s[1] && s[2]) {
			char hex[3] = { s[1], s[2], 0 };
			*o++ = (char)strtol(hex, NULL, 16);
			s += 3;
		} else { *o++ = *s++; }
	}
	*o = '\0';
}

static void send_all(int fd, const char *s, size_t n)
{
	size_t off = 0;
	while (off < n) {
		ssize_t w = send(fd, s + off, n - off, 0);
		if (w <= 0) break;
		off += w;
	}
}

static void run_query(int client, const char *subcmd, const char *query)
{
	/* parse query into argv */
	char buf[4096];
	strncpy(buf, query ? query : "", sizeof(buf) - 1);
	buf[sizeof(buf)-1] = '\0';
	char *argv[64];
	int argc = 0;
	argv[argc++] = "pycoolctl";
	argv[argc++] = (char *)subcmd;
	/* split */
	char *save = NULL;
	char *tok = strtok_r(buf, "&", &save);
	while (tok && argc < 62) {
		char *eq = strchr(tok, '=');
		if (eq) {
			*eq = '\0';
			char *key = tok;
			char *val = eq + 1;
			url_decode(key);
			url_decode(val);
			if (strcmp(key, "name") == 0 || strcmp(key, "class") == 0) {
				argv[argc++] = strdup(val);
			} else if (strcmp(key, "method") == 0) {
				argv[argc++] = strdup("--method");
				argv[argc++] = strdup(val);
			} else if (strcmp(key, "file") == 0) {
				argv[argc++] = strdup("--file");
				argv[argc++] = strdup(val);
			} else if (strcmp(key, "db") == 0) {
				argv[argc++] = strdup("--db");
				argv[argc++] = strdup(val);
			} else if (strcmp(key, "pid") == 0) {
				argv[argc++] = strdup("--pid");
				argv[argc++] = strdup(val);
			} else if (strcmp(key, "info") == 0) {
				argv[argc++] = strdup("--info");
				argv[argc++] = strdup(val);
			} else if (strcmp(key, "body") == 0) {
				argv[argc++] = strdup("--body");
			} else if (strcmp(key, "refs") == 0) {
				argv[argc++] = strdup("--refs");
			} else if (strcmp(key, "json") == 0) {
				argv[argc++] = strdup("--json");
			} else if (strcmp(key, "compact") == 0) {
				argv[argc++] = strdup("--compact");
			} else if (strcmp(key, "full") == 0) {
				argv[argc++] = strdup("--full");
			} else if (strcmp(key, "limit") == 0) {
				argv[argc++] = strdup("--limit");
				argv[argc++] = strdup(val);
			} else if (strcmp(key, "offset") == 0) {
				argv[argc++] = strdup("--offset");
				argv[argc++] = strdup(val);
			}
		}
		tok = strtok_r(NULL, "&", &save);
	}
	argv[argc] = NULL;

	int pipefd[2];
	if (pipe(pipefd) != 0) { perror("pipe"); return; }
	pid_t pid = fork();
	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);
		execv("./bin/pycoolctl", argv);
		perror("exec");
		_exit(127);
	}
	close(pipefd[1]);
	char out[65536];
	size_t off = 0;
	ssize_t r;
	while ((r = read(pipefd[0], out + off, sizeof(out) - off - 1)) > 0) {
		off += r;
		if (off >= sizeof(out) - 1) break;
	}
	out[off] = '\0';
	close(pipefd[0]);
	waitpid(pid, NULL, 0);

	char header[256];
	int hlen = snprintf(header, sizeof(header),
		"HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", strlen(out));
	send_all(client, header, hlen);
	send_all(client, out, strlen(out));
}

int main(int argc, char **argv)
{
	int port = 8080;
	if (argc >= 2) port = atoi(argv[1]);
	int srv = socket(AF_INET, SOCK_STREAM, 0);
	if (srv < 0) { perror("socket"); return 1; }
	int opt = 1;
	setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
	if (listen(srv, 16) < 0) { perror("listen"); return 1; }
	printf("pycool http listening on %d\n", port);
	fflush(stdout);
	for (;;) {
		int client = accept(srv, NULL, NULL);
		if (client < 0) continue;
		char req[8192];
		ssize_t n = recv(client, req, sizeof(req) - 1, 0);
		if (n <= 0) { close(client); continue; }
		req[n] = '\0';
		/* parse request line */
		char method[16], path[1024];
		if (sscanf(req, "%15s %1023s", method, path) != 2) { close(client); continue; }
		char *q = strchr(path, '?');
		char query[4096] = "";
		if (q) { *q = '\0'; strncpy(query, q + 1, sizeof(query)-1); }
		if (strcmp(path, "/") == 0) {
			const char *body = "pycools http ok\n";
			char h[128];
			snprintf(h, sizeof(h), "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", strlen(body));
			send_all(client, h, strlen(h));
			send_all(client, body, strlen(body));
		} else if (strcmp(path, "/class") == 0) {
			run_query(client, "class", query);
		} else if (strcmp(path, "/method") == 0) {
			run_query(client, "method", query);
		} else {
			const char *body = "not found\n";
			char h[128];
			snprintf(h, sizeof(h), "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n", strlen(body));
			send_all(client, h, strlen(h));
			send_all(client, body, strlen(body));
		}
		close(client);
	}
	return 0;
}