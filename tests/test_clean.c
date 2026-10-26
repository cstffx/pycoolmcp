#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "clean.h"
#include "registry.h"

int main(void)
{
	char state[] = "/tmp/opencode/cleanstateXXXXXX";
	char *sd = mkdtemp(state);
	setenv("XDG_STATE_HOME", sd, 1);

	/* usage branch */
	char *argv0[] = {"pycoolctl", "clean"};
	printf("usage=%d\n", clean_command(2, argv0));

	/* pid branch with no matching registry entry */
	char *argv1[] = {"pycoolctl", "clean", "999999"};
	printf("pid_nomatch=%d\n", clean_command(3, argv1));

	char regdir[4096];
	snprintf(regdir, sizeof(regdir), "%s/pycools", sd);
	mkdir(regdir, 0755);

	/* pid branch with a matching entry (child ignores SIGTERM) */
	char dbpath[] = "/tmp/opencode/clean.db";
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

	char pidbuf[32];
	snprintf(pidbuf, sizeof(pidbuf), "%d", (int)child);
	char *argv2[] = {"pycoolctl", "clean", pidbuf};
	printf("pid_match=%d\n", clean_command(3, argv2));

	kill(child, SIGKILL);
	waitpid(child, NULL, 0);

	/* all branch with a matching alive entry */
	char dbpath2[] = "/tmp/opencode/clean2.db";
	unlink(dbpath2);
	f = fopen(dbpath2, "w"); if (f) { fputc('x', f); fclose(f); }

	pid_t child2 = fork();
	if (child2 == 0) {
		signal(SIGTERM, SIG_IGN);
		pause();
		_exit(0);
	}
	char regpath2[4096];
	snprintf(regpath2, sizeof(regpath2), "%s/%d.reg", regdir, (int)child2);
	rf = fopen(regpath2, "w");
	if (rf) { fprintf(rf, "%d\n%s\n%s\n", (int)child2, "/tmp", dbpath2); fclose(rf); }

	char *argv3[] = {"pycoolctl", "clean", "all"};
	printf("all=%d\n", clean_command(3, argv3));

	kill(child2, SIGKILL);
	waitpid(child2, NULL, 0);

	unlink(dbpath);
	unlink(dbpath2);
	unsetenv("XDG_STATE_HOME");
	return 0;
}
