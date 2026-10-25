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
	char *dbpath = NULL;
	if (argc >= 3) {
		dbpath = strdup(argv[2]);
		if (!dbpath) {
			perror("strdup");
			return 2;
		}
	} else {
		char *dir = NULL;
		if (asprintf(&dir, "%s/.pycool", root) == -1 || !dir) {
			perror("asprintf");
			return 2;
		}
		mkdir(dir, 0755);
		if (asprintf(&dbpath, "%s/index.db", dir) == -1 || !dbpath) {
			free(dir);
			perror("asprintf");
			return 2;
		}
		free(dir);
	}
	int rc = pycool_run(root, dbpath);
	free(dbpath);
	return rc;
}