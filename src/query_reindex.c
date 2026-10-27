#define _GNU_SOURCE
#include "reindex.h"
#include "../src/registry.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int reindex_command(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s reindex <pid|all>\n", argv[0]);
		return 2;
	}
	char **roots;
	char **dbpaths; pid_t *pids; int n;
	registry_list_paths(&roots, &dbpaths, &pids, &n);
	if (strcmp(argv[2], "all") == 0) {
		for (int i = 0; i < n; i++) { kill(pids[i], SIGUSR2);
}
	} else {
		kill(atoi(argv[2]), SIGUSR2);
	}
	return 0;
}
