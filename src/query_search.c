#define _GNU_SOURCE
#include "../pycoolctl/search.h"
#include "../src/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <sqlite3.h>
#include "../index/core.h"

typedef struct {
	char *path;
	long line;
	char *kind;
	char *name;
	char *doc;
} SearchRow;

typedef struct {
	const char *query;
	int regex_mode;
	const char *kind;
	const char *file;
	const char *dbpath;
	int limit;
	int offset;
} SearchArgs;

static void string_to_lower(char *s)
{
	for (char *p = s; *p; p++) { *p = (char)tolower((unsigned char)*p); }
}

static int case_insensitive_match(const char *hay, const char *needle)
{
	char *h = strdup(hay);
	char *n = strdup(needle);
	if (!h || !n) { free(h); free(n); return 0; }
	string_to_lower(h);
	string_to_lower(n);
	int r = strstr(h, n) != NULL;
	free(h);
	free(n);
	return r;
}

static int match_text(const char *hay, const char *needle, int regex_mode, regex_t *re)
{
	if (!hay) { return 0; }
	if (regex_mode) {
		return regexec(re, hay, 0, NULL, 0) == 0;
	}
	return case_insensitive_match(hay, needle);
}

static int resolve_search_pid(const char *pidstr, SearchArgs *args)
{
	char **roots;
	char **dbpaths; pid_t *pids; int n;
	if (registry_list_paths(&roots, &dbpaths, &pids, &n) != 0 || n == 0) {
		fprintf(stderr, "no running instance; pass --db PATH\n");
		return 1;
	}
	pid_t target = (pid_t)atoi(pidstr);
	for (int j = 0; j < n; j++) {
		if (pids[j] == target) { args->dbpath = dbpaths[j]; break; }
	}
	if (!args->dbpath) { fprintf(stderr, "pid %s not found\n", pidstr); return 1; }
	return 0;
}

static int parse_search_args(int argc, char **argv, SearchArgs *args)
{
	memset(args, 0, sizeof(*args));
	args->kind = "all";
	args->limit = 0;
	args->offset = 0;

	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--regex") == 0) {
			args->regex_mode = 1;
		} else if (strcmp(argv[i], "--kind") == 0 && i + 1 < argc) {
			args->kind = argv[++i];
		} else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
			args->file = argv[++i];
		} else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
			args->dbpath = argv[++i];
		} else if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
			if (!args->dbpath) {
				int rc = resolve_search_pid(argv[++i], args);
				if (rc != 0) { return rc; }
			} else {
				i++;
			}
		} else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
			args->limit = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) {
			args->offset = atoi(argv[++i]);
		} else if (!args->query) {
			args->query = argv[i];
		}
	}

	if (!args->query) {
		fprintf(stderr, "usage: pycoolctl search <query> [--regex] [--kind ...] [--file GLOB] [--db PATH] [--limit N] [--offset N]\n");
		return 2;
	}

	return 0;
}

static int resolve_search_dbpath(const SearchArgs *args, char **resolved_db)
{
	if (args->dbpath) {
		return 0;
	}

	*resolved_db = registry_default_db();
	if (*resolved_db) {
		return 0;
	}

	fprintf(stderr, "no db; pass --db PATH or set PYCOOL_DB\n");
	return 1;
}

static int check_search_schema(sqlite3 *db)
{
	int sv = index_schema_version(db);
	if (sv != -1 && sv != PYCOOL_SCHEMA_VERSION) {
		fprintf(stderr, "error: schema mismatch (db=v%d, code=v%d)\n", sv, PYCOOL_SCHEMA_VERSION);
		return 1;
	}
	return 0;
}

static void build_kind_clause(const SearchArgs *args, char *kindclause, int size)
{
	int want_class = (strcmp(args->kind, "all") == 0 || strcmp(args->kind, "class") == 0);
	int want_function = (strcmp(args->kind, "all") == 0 || strcmp(args->kind, "function") == 0);
	int want_variable = (strcmp(args->kind, "all") == 0 || strcmp(args->kind, "variable") == 0);

	if (!(want_class && want_function && want_variable)) {
		snprintf(kindclause, size, " AND s.kind IN (");
		if (want_class) { strcat(kindclause, "'class'"); }
		if (want_function) {
			if (kindclause[strlen(kindclause)-1] != '(') { strcat(kindclause, ","); }
			strcat(kindclause, "'function'");
		}
		if (want_variable) {
			if (kindclause[strlen(kindclause)-1] != '(') { strcat(kindclause, ","); }
			strcat(kindclause, "'variable'");
		}
		strcat(kindclause, ")");
	} else {
		kindclause[0] = '\0';
	}
}

static void build_symbol_query(const SearchArgs *args, char *sql, int sql_size)
{
	snprintf(sql, sql_size,
		"SELECT s.kind, s.name, s.line, f.path, cd.docstring "
		"FROM symbols s JOIN files f ON s.file_id=f.id "
		"LEFT JOIN class_details cd ON cd.symbol_id=s.id "
		"WHERE 1=1");

	char kindclause[256] = "";
	build_kind_clause(args, kindclause, sizeof(kindclause));
	if (kindclause[0]) { strcat(sql, kindclause); }

	if (args->file) {
		if (strchr(args->file, '*') || strchr(args->file, '?')) {
			snprintf(sql + strlen(sql), sql_size - strlen(sql), " AND f.path GLOB '%s'", args->file);
		} else {
			snprintf(sql + strlen(sql), sql_size - strlen(sql), " AND f.path = '%s'", args->file);
		}
	}
}

static void build_method_query(const SearchArgs *args, char *sql, int sql_size)
{
	snprintf(sql, sql_size,
		"SELECT m.name, m.line, f.path, m.docstring "
		"FROM methods m JOIN symbols s ON m.class_symbol_id=s.id JOIN files f ON s.file_id=f.id "
		"WHERE 1=1");

	if (args->file) {
		if (strchr(args->file, '*') || strchr(args->file, '?')) {
			snprintf(sql + strlen(sql), sql_size - strlen(sql), " AND f.path GLOB '%s'", args->file);
		} else {
			snprintf(sql + strlen(sql), sql_size - strlen(sql), " AND f.path = '%s'", args->file);
		}
	}
}

static int execute_symbol_query(sqlite3 *db, const char *sql, const SearchArgs *args, regex_t *re, SearchRow **rows, int *nrows, int *caprows)
{
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		return 1;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char *k = (const char *)sqlite3_column_text(stmt, 0);
		const char *nm = (const char *)sqlite3_column_text(stmt, 1);
		long ln = sqlite3_column_int64(stmt, 2);
		const char *p = (const char *)sqlite3_column_text(stmt, 3);
		const char *doc = (const char *)sqlite3_column_text(stmt, 4);

		if (!match_text(nm, args->query, args->regex_mode, re) && !match_text(doc, args->query, args->regex_mode, re)) {
			continue;
		}

		if (*nrows == *caprows) {
			*caprows = *caprows ? *caprows * 2 : 64;
			SearchRow *tmp = realloc(*rows, *caprows * sizeof(SearchRow));
			if (!tmp) { return 1; }
			*rows = tmp;
		}

		(*rows)[*nrows].path = strdup(p ? p : "");
		(*rows)[*nrows].line = ln;
		(*rows)[*nrows].kind = strdup(k ? k : "");
		(*rows)[*nrows].name = strdup(nm ? nm : "");
		(*rows)[*nrows].doc = doc ? strdup(doc) : NULL;
		(*nrows)++;
	}

	sqlite3_finalize(stmt);
	return 0;
}

static int execute_method_query(sqlite3 *db, const char *sql, const SearchArgs *args, regex_t *re, SearchRow **rows, int *nrows, int *caprows)
{
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		return 1;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char *nm = (const char *)sqlite3_column_text(stmt, 0);
		long ln = sqlite3_column_int64(stmt, 1);
		const char *p = (const char *)sqlite3_column_text(stmt, 2);
		const char *doc = (const char *)sqlite3_column_text(stmt, 3);

		if (!match_text(nm, args->query, args->regex_mode, re) && !match_text(doc, args->query, args->regex_mode, re)) {
			continue;
		}

		if (*nrows == *caprows) {
			*caprows = *caprows ? *caprows * 2 : 64;
			SearchRow *tmp = realloc(*rows, *caprows * sizeof(SearchRow));
			if (!tmp) { return 1; }
			*rows = tmp;
		}

		(*rows)[*nrows].path = strdup(p ? p : "");
		(*rows)[*nrows].line = ln;
		(*rows)[*nrows].kind = strdup("method");
		(*rows)[*nrows].name = strdup(nm ? nm : "");
		(*rows)[*nrows].doc = doc ? strdup(doc) : NULL;
		(*nrows)++;
	}

	sqlite3_finalize(stmt);
	return 0;
}

static void print_output_row(FILE *out, const SearchRow *row, int limit, int *shown)
{
	if (*shown >= limit && limit > 0) {
		return;
	}

	fprintf(out, "%s:%ld %s %s", row->path, row->line, row->kind, row->name);
	if (row->doc && row->doc[0]) {
		char *doc_copy = strdup(row->doc);
		if (doc_copy) {
			char *nl = strchr(doc_copy, '\n');
			if (nl) { *nl = '\0'; }
			fprintf(out, "  # %s", doc_copy);
			free(doc_copy);
		}
	}
	fputc('\n', out);
	(*shown)++;
}

static void free_search_rows(SearchRow *rows, int nrows)
{
	for (int i = 0; i < nrows; i++) {
		free(rows[i].path);
		free(rows[i].kind);
		free(rows[i].name);
		free(rows[i].doc);
	}
	free(rows);
}

int search_command(int argc, char **argv, FILE *out)
{
	SearchArgs args;
	int rc = parse_search_args(argc, argv, &args);
	if (rc != 0) { return rc; }

	char *resolved_db = NULL;
	if (resolve_search_dbpath(&args, &resolved_db) != 0) {
		return 1;
	}
	const char *dbpath = args.dbpath ? args.dbpath : resolved_db;

	sqlite3 *db;
	if (sqlite3_open(dbpath, &db) != SQLITE_OK) {
		fprintf(stderr, "cannot open db: %s\n", dbpath);
		free(resolved_db);
		return 1;
	}
	sqlite3_busy_timeout(db, 5000);

	if (check_search_schema(db) != 0) {
		sqlite3_close(db);
		free(resolved_db);
		return 1;
	}

	regex_t re;
	int re_ok = 0;
	if (args.regex_mode) {
		if (regcomp(&re, args.query, REG_EXTENDED | REG_ICASE | REG_NOSUB) != 0) {
			fprintf(stderr, "invalid regex: %s\n", args.query);
			sqlite3_close(db);
			free(resolved_db);
			return 1;
		}
		re_ok = 1;
	}

	SearchRow *rows = NULL;
	int nrows = 0;
	int caprows = 0;

	int want_class = (strcmp(args.kind, "all") == 0 || strcmp(args.kind, "class") == 0);
	int want_function = (strcmp(args.kind, "all") == 0 || strcmp(args.kind, "function") == 0);
	int want_variable = (strcmp(args.kind, "all") == 0 || strcmp(args.kind, "variable") == 0);
	int want_method = (strcmp(args.kind, "all") == 0 || strcmp(args.kind, "method") == 0);

	if (want_class || want_function || want_variable) {
		char sql[1024];
		build_symbol_query(&args, sql, sizeof(sql));
		execute_symbol_query(db, sql, &args, &re, &rows, &nrows, &caprows);
	}

	if (want_method) {
		char sql[1024];
		build_method_query(&args, sql, sizeof(sql));
		execute_method_query(db, sql, &args, &re, &rows, &nrows, &caprows);
	}

	if (re_ok) {
		regfree(&re);
	}

	int shown = 0;
	for (int i = 0; i < nrows; i++) {
		if (i < args.offset) { continue; }
		if (args.limit > 0 && shown >= args.limit) { break; }
		print_output_row(out, &rows[i], args.limit, &shown);
	}

	free_search_rows(rows, nrows);
	sqlite3_close(db);
	free(resolved_db);
	return 0;
}
