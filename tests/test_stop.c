#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "stop.h"
#include "registry.h"

int main(void)
{
	char state[] = "/tmp/opencode/stopstateXXXXXX";
	char *sd = mkdtemp(state);
	setenv("XDG_STATE_HOME", sd, 1);

	/* usage branch */
	char *argv0[] = {"pycoolctl", "stop"};
	printf("usage=%d\n", stop_command(2, argv0));

	/* else branch */
	char *argv1[] = {"pycoolctl", "stop", "999999"};
	printf("pid=%d\n", stop_command(3, argv1));

	char regdir[4096];
	snprintf(regdir, sizeof(regdir), "%s/pycools", sd);
	mkdir(regdir, 0755);

	char dbpath[] = "/tmp/opencode/stop.db";
	unlink(dbpath);
	FILE *f = fopen(dbpath, "w"); if (f) { fputc('x', f); fclose(f); }

	pid_t child = fork();
	if (child == 0) {
		signal(SIGTERM, SIG_IGN);
		pause();
		_exit(0);
	}
	char regpath[4096];
	snprintf(regpath, sizeof(regpath), "%s/%d.reg", regdir, (int)child);
	FILE *rf = fopen(regpath, "w");
	if (rf) { fprintf(rf, "%d\n%s\n%s\n", (int)child, "/tmp", dbpath); fclose(rf); }

	/* all branch */
	char *argv2[] = {"pycoolctl", "stop", "all"};
	printf("all=%d\n", stop_command(3, argv2));

	kill(child, SIGKILL);
	waitpid(child, NULL, 0);
	unlink(dbpath);
	unsetenv("XDG_STATE_HOME");
	return 0;
}
