#define _GNU_SOURCE
#include "list.h"
#include "../src/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int list_command(int argc, char **argv, FILE *out)
{
	int show_size = 0;
	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--dbsize") == 0) show_size = 1;
	}
	char **roots, **dbpaths; pid_t *pids; int n;
	if (registry_list_paths(&roots, &dbpaths, &pids, &n) != 0) return 1;
	if (show_size) fprintf(out, "PID\tROOT\tDB\tDBSIZE\n");
	else fprintf(out, "PID\tROOT\tDB\n");
	for (int i = 0; i < n; i++) {
		if (show_size) {
			struct stat st;
			long sz = -1;
			if (stat(dbpaths[i], &st) == 0) sz = (long)st.st_size;
			fprintf(out, "%d\t%s\t%s\t%ld\n", (int)pids[i], roots[i], dbpaths[i], sz);
		} else {
			fprintf(out, "%d\t%s\t%s\n", (int)pids[i], roots[i], dbpaths[i]);
		}
	}
	return 0;
}
