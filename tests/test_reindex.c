#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "reindex.h"

int main(void)
{
	char *argv0[] = {"pycoolctl", "reindex"};
	printf("usage=%d\n", reindex_command(2, argv0));

	signal(SIGUSR2, SIG_IGN);
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", (int)getpid());
	char *argv1[] = {"pycoolctl", "reindex", "all"};
	printf("all=%d\n", reindex_command(3, argv1));
	char *argv2[] = {"pycoolctl", "reindex", buf};
	printf("pid=%d\n", reindex_command(3, argv2));
	return 0;
}