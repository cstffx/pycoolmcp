#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "list.h"
#include "registry.h"

int main(void)
{
	char state[] = "/tmp/opencode/stateXXXXXX";
	char *sd = mkdtemp(state);
	setenv("XDG_STATE_HOME", sd, 1);
	char dbp[] = "/tmp/opencode/list.db";
	FILE *f = fopen(dbp, "w"); if (f) { fputc('x', f); fclose(f); }
	registry_register("/tmp", dbp);

	char *argv0[] = {"pycoolctl", "list"};
	printf("list=%d\n", list_command(2, argv0, stdout));

	char *argv1[] = {"pycoolctl", "list", "--dbsize"};
	printf("listsize=%d\n", list_command(3, argv1, stdout));

	registry_unregister();
	unsetenv("XDG_STATE_HOME");
	unlink(dbp);
	return 0;
}