#define _GNU_SOURCE
#include "info.h"
#include "../src/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sqlite3.h>

int info_command(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s info <db|pid|all>\n", argv[0]);
		return 2;
	}
	char **targets = NULL;
	int ntarget = 0, cap = 0;
	if (strcmp(argv[2], "all") == 0) {
		char **roots, **dbpaths; pid_t *pids; int n;
		if (registry_list_paths(&roots, &dbpaths, &pids, &n) != 0) return 1;
		for (int i = 0; i < n; i++) {
			if (ntarget == cap) { cap = cap ? cap * 2 : 8; targets = realloc(targets, cap * sizeof(char *)); }
			targets[ntarget++] = dbpaths[i];
		}
	} else {
		char *end = NULL;
		long pid = strtol(argv[2], &end, 10);
		char *dbpath = NULL;
		if (*end == '\0') {
			char **roots, **dbpaths; pid_t *pids; int n;
			if (registry_list_paths(&roots, &dbpaths, &pids, &n) == 0) {
				for (int i = 0; i < n; i++) {
					if (pids[i] == (pid_t)pid) { dbpath = dbpaths[i]; break; }
				}
			}
		}
		if (!dbpath) dbpath = argv[2];
		if (ntarget == cap) { cap = cap ? cap * 2 : 8; targets = realloc(targets, cap * sizeof(char *)); }
		targets[ntarget++] = strdup(dbpath);
	}
	for (int t = 0; t < ntarget; t++) {
		const char *dbpath = targets[t];
		struct stat st;
		long size = -1;
		if (stat(dbpath, &st) == 0) size = (long)st.st_size;
		sqlite3 *db;
		if (sqlite3_open(dbpath, &db) != SQLITE_OK) {
			fprintf(stderr, "cannot open %s\n", dbpath);
			continue;
		}
		long files = 0, classes = 0, functions = 0, variables = 0;
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM files;", -1, &stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(stmt) == SQLITE_ROW) files = sqlite3_column_int64(stmt, 0);
			sqlite3_finalize(stmt);
		}
		if (sqlite3_prepare_v2(db, "SELECT kind, COUNT(*) FROM symbols GROUP BY kind;", -1, &stmt, NULL) == SQLITE_OK) {
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				const unsigned char *k = sqlite3_column_text(stmt, 0);
				long c = sqlite3_column_int64(stmt, 1);
				if (k && strcmp((const char *)k, "class") == 0) classes = c;
				else if (k && strcmp((const char *)k, "function") == 0) functions = c;
				else if (k && strcmp((const char *)k, "variable") == 0) variables = c;
			}
			sqlite3_finalize(stmt);
		}
		sqlite3_close(db);
		printf("DB: %s\n", dbpath);
		printf("  size: %ld bytes\n", size);
		printf("  files: %ld\n", files);
		printf("  classes: %ld\n", classes);
		printf("  functions: %ld\n", functions);
		printf("  variables: %ld\n", variables);
	}
	return 0;
}
