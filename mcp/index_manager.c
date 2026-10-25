#define _GNU_SOURCE
#include "index_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

static char pycoolctl_path_buf[4096];
static char pycool_path_buf[4096];
static pid_t index_pid = -1;

static void resolve_binary(const char *argv0, const char *name, char *out, size_t outsz)
{
	char dir[4096];
	strncpy(dir, argv0, sizeof(dir) - 1);
	dir[sizeof(dir) - 1] = '\0';
	char *slash = strrchr(dir, '/');
	if (slash) *slash = '\0';
	else strcpy(dir, ".");

	const char *candidates[4];
	char c0[4096], c1[4096], c2[4096];
	snprintf(c0, sizeof(c0), "%s/%s", dir, name);
	snprintf(c1, sizeof(c1), "%s/bin/%s", dir, name);
	snprintf(c2, sizeof(c2), "%s/../bin/%s", dir, name);
	candidates[0] = c0;
	candidates[1] = c1;
	candidates[2] = c2;
	candidates[3] = name;

	for (int i = 0; i < 4; i++) {
		struct stat st;
		if (access(candidates[i], X_OK) == 0 && stat(candidates[i], &st) == 0 && S_ISREG(st.st_mode)) {
			strncpy(out, candidates[i], outsz - 1);
			out[outsz - 1] = '\0';
			return;
		}
	}
	strncpy(out, name, outsz - 1);
	out[outsz - 1] = '\0';
}

void resolve_pycoolctl(const char *argv0)
{
	resolve_binary(argv0, "pycoolctl", pycoolctl_path_buf, sizeof(pycoolctl_path_buf));
}

void resolve_pycool(const char *argv0)
{
	resolve_binary(argv0, "pycool", pycool_path_buf, sizeof(pycool_path_buf));
}

const char *pycoolctl_path(void) { return pycoolctl_path_buf; }
const char *pycool_path(void) { return pycool_path_buf; }

int index_start(const char *root, const char *db, char **msg)
{
	if (index_pid > 0) { *msg = strdup("index already running"); return 0; }
	pid_t p = fork();
	if (p < 0) { *msg = strdup("fork failed"); return 1; }
	if (p == 0) {
		setsid();
		int fd = open("/dev/null", O_RDWR);
		if (fd >= 0) { dup2(fd, STDIN_FILENO); dup2(fd, STDOUT_FILENO); dup2(fd, STDERR_FILENO); close(fd); }
		char *a[4]; int ac = 0;
		a[ac++] = "pycool";
		a[ac++] = (char *)root;
		if (db) a[ac++] = (char *)db;
		a[ac] = NULL;
		execv(pycool_path_buf, a);
		_exit(127);
	}
	index_pid = p;
	*msg = strdup("index started");
	return 0;
}

int index_stop(char **msg)
{
	if (index_pid <= 0) { *msg = strdup("no index running"); return 0; }
	kill(index_pid, SIGTERM);
	waitpid(index_pid, NULL, 0);
	index_pid = -1;
	*msg = strdup("index stopped");
	return 0;
}

void index_cleanup(void)
{
	if (index_pid > 0) {
		kill(index_pid, SIGTERM);
		waitpid(index_pid, NULL, 0);
		index_pid = -1;
	}
}