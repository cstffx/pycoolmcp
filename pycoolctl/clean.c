#define _GNU_SOURCE
#include "clean.h"
#include "../src/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

int clean_command(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s clean <pid|all>\n", argv[0]);
		return 2;
	}
	char **roots, **dbpaths; pid_t *pids; int n;
	registry_list_paths(&roots, &dbpaths, &pids, &n);
	if (strcmp(argv[2], "all") == 0) {
		for (int i = 0; i < n; i++) kill(pids[i], SIGTERM);
		usleep(500000);
		for (int i = 0; i < n; i++) remove(dbpaths[i]);
	} else {
		pid_t pid = atoi(argv[2]);
		kill(pid, SIGTERM);
		usleep(500000);
		for (int i = 0; i < n; i++) {
			if (pids[i] == pid) remove(dbpaths[i]);
		}
	}
	return 0;
}
