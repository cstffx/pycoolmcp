#define _GNU_SOURCE
#include "info.h"
#include "../src/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sqlite3.h>

typedef struct {
	long files;
	long classes;
	long functions;
	long variables;
	long size;
} DbStats;

static void add_target(char ***targets, int *ntarget, int *cap, const char *path)
{
	if (*ntarget == *cap) {
		*cap = *cap ? *cap * 2 : 8;
		char **tmp = realloc(*targets, *cap * sizeof(char *));
		if (tmp) *targets = tmp;
	}
	(*targets)[(*ntarget)++] = (char *)path;
}

static int get_all_targets(char ***targets, int *ntarget, int *cap)
{
	char **roots;
	char **dbpaths;
	pid_t *pids;
	int n;

	if (registry_list_paths(&roots, &dbpaths, &pids, &n) != 0)
		return 1;

	for (int i = 0; i < n; i++)
		add_target(targets, ntarget, cap, dbpaths[i]);

	return 0;
}

static const char *resolve_single_target(const char *arg)
{
	char *end = NULL;
	long pid = strtol(arg, &end, 10);

	if (*end != '\0')
		return arg;

	char **roots;
	char **dbpaths;
	pid_t *pids;
	int n;

	if (registry_list_paths(&roots, &dbpaths, &pids, &n) != 0)
		return arg;

	for (int i = 0; i < n; i++) {
		if (pids[i] == (pid_t)pid)
			return dbpaths[i];
	}

	return arg;
}

static void count_symbol_kinds(sqlite3 *db, long *classes, long *functions, long *variables)
{
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, "SELECT kind, COUNT(*) FROM symbols GROUP BY kind;", -1, &stmt, NULL) != SQLITE_OK)
		return;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const unsigned char *k = sqlite3_column_text(stmt, 0);
		long c = sqlite3_column_int64(stmt, 1);
		if (!k) continue;

		if (strcmp((const char *)k, "class") == 0) {
			*classes = c;
		} else if (strcmp((const char *)k, "function") == 0) {
			*functions = c;
		} else if (strcmp((const char *)k, "variable") == 0) {
			*variables = c;
		}
	}
	sqlite3_finalize(stmt);
}

static int get_db_stats(const char *dbpath, DbStats *stats)
{
	sqlite3 *db;
	if (sqlite3_open(dbpath, &db) != SQLITE_OK) {
		fprintf(stderr, "cannot open %s\n", dbpath);
		return 1;
	}

	stats->files = 0;
	stats->classes = 0;
	stats->functions = 0;
	stats->variables = 0;
	stats->size = -1;

	struct stat st;
	if (stat(dbpath, &st) == 0)
		stats->size = (long)st.st_size;

	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM files;", -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW)
			stats->files = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
	}

	count_symbol_kinds(db, &stats->classes, &stats->functions, &stats->variables);

	sqlite3_close(db);
	return 0;
}

static void print_db_info(FILE *out, const char *dbpath, const DbStats *stats)
{
	fprintf(out, "DB: %s\n", dbpath);
	fprintf(out, "  size: %ld bytes\n", stats->size);
	fprintf(out, "  files: %ld\n", stats->files);
	fprintf(out, "  classes: %ld\n", stats->classes);
	fprintf(out, "  functions: %ld\n", stats->functions);
	fprintf(out, "  variables: %ld\n", stats->variables);
}

int info_command(int argc, char **argv, FILE *out)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s info <db|pid|all>\n", argv[0]);
		return 2;
	}

	char **targets = NULL;
	int ntarget = 0;
	int cap = 0;

	if (strcmp(argv[2], "all") == 0) {
		if (get_all_targets(&targets, &ntarget, &cap) != 0)
			return 1;
	} else {
		const char *resolved = resolve_single_target(argv[2]);
		add_target(&targets, &ntarget, &cap, resolved);
	}

	for (int t = 0; t < ntarget; t++) {
		DbStats stats;
		if (get_db_stats(targets[t], &stats) == 0)
			print_db_info(out, targets[t], &stats);
	}

	return 0;
}
