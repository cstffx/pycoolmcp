#define _GNU_SOURCE
#include "stop.h"
#include "../src/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

int stop_command(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s stop <pid|all>\n", argv[0]);
		return 2;
	}
	char **roots, **dbpaths; pid_t *pids; int n;
	registry_list_paths(&roots, &dbpaths, &pids, &n);
	if (strcmp(argv[2], "all") == 0) {
		for (int i = 0; i < n; i++) kill(pids[i], SIGTERM);
	} else {
		kill(atoi(argv[2]), SIGTERM);
	}
	return 0;
}
