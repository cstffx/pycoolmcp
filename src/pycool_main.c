#define _GNU_SOURCE
#include "pycool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s ROOT [DBPATH]\n", argv[0]);
		return 2;
	}
	const char *root = argv[1];
	if (!root || !*root) {
		fprintf(stderr, "error: empty root path\n");
		return 2;
	}
	char *dbpath = NULL;
	if (argc >= 3) {
		dbpath = strdup(argv[2]);
		if (!dbpath) { return 2; }
	} else {
		char *dir = NULL;
		if (asprintf(&dir, "%s/.pycool", root) < 0) { return 2; }
		if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
			fprintf(stderr, "error: mkdir %s: %s\n", dir, strerror(errno));
			free(dir);
			return 2;
		}
		if (asprintf(&dbpath, "%s/index.db", dir) < 0) {
			free(dir);
			return 2;
		}
		free(dir);
	}
	int rc = pycool_run(root, dbpath);
	free(dbpath);
	return rc;
}