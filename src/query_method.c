#define _GNU_SOURCE
#include "method.h"
#include "../src/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "../index/core.h"

typedef struct {
	const char *class_name;
	const char *method_name;
	const char *file;
	const char *dbpath;
	int json_mode;
	int body_mode;
	const char *lines;
	int max_lines;
	int limit;
	int offset;
} MethodArgs;

static void print_json_str(FILE *out, const char *s)
{
	fputc('"', out);
	if (s) {
		for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
			switch (*p) {
				case '"': fputs("\\\"", out); break;
				case '\\': fputs("\\\\", out); break;
				case '\n': fputs("\\n", out); break;
				case '\r': fputs("\\r", out); break;
				case '\t': fputs("\\t", out); break;
				default:
					if (*p < 0x20) { fprintf(out, "\\u%04x", *p);
					} else { fputc(*p, out);
	}
			}
		}
	}
	fputc('"', out);
}

static char *build_params(sqlite3 *db, sqlite3_int64 method_id)
{
	char *buf = NULL;
	size_t cap = 0;
	size_t len = 0;
	sqlite3_stmt *stmt;
	const char *sql = "SELECT name, annotation, default_value, kind FROM method_args WHERE method_id=? ORDER BY position;";
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { return strdup("");
}
	sqlite3_bind_int64(stmt, 1, method_id);
	int first = 1;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(stmt, 0);
		const char *ann = (const char *)sqlite3_column_text(stmt, 1);
		const char *def = (const char *)sqlite3_column_text(stmt, 2);
		const char *kind = (const char *)sqlite3_column_text(stmt, 3);
		char piece[512];
		piece[0] = '\0';
		if (kind && strcmp(kind, "varargs") == 0) { snprintf(piece, sizeof(piece), "*%s", name ? name : "");
		} else if (kind && strcmp(kind, "kwargs") == 0) { snprintf(piece, sizeof(piece), "**%s", name ? name : "");
		} else { snprintf(piece, sizeof(piece), "%s", name ? name : "");
}
		if (ann && ann[0]) {
			char tmp[600];
			snprintf(tmp, sizeof(tmp), "%s: %s", piece, ann);
			strcpy(piece, tmp);
		}
		if (def && def[0]) {
			char tmp[700];
			snprintf(tmp, sizeof(tmp), "%s=%s", piece, def);
			strcpy(piece, tmp);
		}
		size_t plen = strlen(piece);
		if (len + plen + 4 >= cap) {
			cap = cap ? cap * 2 : 256;
			while (len + plen + 4 >= cap) { cap *= 2;
}
			char *tmpbuf = realloc(buf, cap);
			if (tmpbuf) { buf = tmpbuf;
}
		}
		if (!first) { buf[len++] = ','; buf[len++] = ' '; }
		first = 0;
		memcpy(buf + len, piece, plen);
		len += plen;
		buf[len] = '\0';
	}
	sqlite3_finalize(stmt);
	if (!buf) { buf = strdup("");
}
	return buf;
}

static void print_class_body(FILE *out, const char *path, long start, long end)
{
	if (!path || start <= 0) { return;
}
	if (end < start) { end = start;
}
	FILE *f = fopen(path, "r");
	if (!f) { perror(path); return; }
	char *line = NULL;
	size_t cap = 0;
	ssize_t len;
	long cur = 0;
	while ((len = getline(&line, &cap, f)) != -1) {
		cur++;
		if (cur >= start && cur <= end) { fputs(line, out);
}
		if (cur >= end) { break;
}
	}
	free(line);
	fclose(f);
}

static int resolve_method_pid(const char *pidstr, MethodArgs *args)
{
	char **roots;
	char **dbpaths; pid_t *pids; int n;
	if (registry_list_paths(&roots, &dbpaths, &pids, &n) != 0 || n == 0) {
		fprintf(stderr, "no running instance; pass --db PATH\n");
		return 1;
	}
	pid_t target = (pid_t)atoi(pidstr);
	for (int i = 0; i < n; i++) {
		if (pids[i] == target) { args->dbpath = dbpaths[i]; break; }
	}
	if (!args->dbpath) { fprintf(stderr, "pid %s not found\n", pidstr); return 1; }
	return 0;
}

static int process_flag_argument(const char *flag, const char *nextarg, int has_next, int *i, MethodArgs *args)
{
	if (strcmp(flag, "--db") == 0 && has_next) {
		args->dbpath = nextarg;
		(*i)++;
		return 1;
	}
	if (strcmp(flag, "--pid") == 0 && has_next && !args->dbpath) {
		int rc = resolve_method_pid(nextarg, args);
		if (rc != 0) { return -1;
		}
		(*i)++;
		return 1;
	}
	if (strcmp(flag, "--pid") == 0 && has_next) {
		(*i)++;
		return 1;
	}
	if (strcmp(flag, "--file") == 0 && has_next) {
		args->file = nextarg;
		(*i)++;
		return 1;
	}
	if (strcmp(flag, "--json") == 0) {
		args->json_mode = 1;
		return 1;
	}
	if (strcmp(flag, "--body") == 0) {
		args->body_mode = 1;
		return 1;
	}
	if (strcmp(flag, "--lines") == 0 && has_next) {
		args->lines = nextarg;
		(*i)++;
		return 1;
	}
	if (strcmp(flag, "--max-lines") == 0 && has_next) {
		args->max_lines = atoi(nextarg);
		(*i)++;
		return 1;
	}
	if (strcmp(flag, "--limit") == 0 && has_next) {
		args->limit = atoi(nextarg);
		(*i)++;
		return 1;
	}
	if (strcmp(flag, "--offset") == 0 && has_next) {
		args->offset = atoi(nextarg);
		(*i)++;
		return 1;
	}
	return 0;
}

static void set_positional_args(const char *arg, MethodArgs *args)
{
	if (!args->class_name) {
		args->class_name = arg;
	} else if (!args->method_name) {
		args->method_name = arg;
	}
}

static int parse_method_args(int argc, char **argv, MethodArgs *args)
{
	memset(args, 0, sizeof(*args));

	for (int i = 2; i < argc; i++) {
		int has_next = i + 1 < argc;
		int rc = process_flag_argument(argv[i], has_next ? argv[i + 1] : NULL, has_next, &i, args);
		if (rc < 0) { return 1;
		}
		if (rc == 0) {
			set_positional_args(argv[i], args);
		}
	}

	if (!args->class_name) {
		fprintf(stderr, "usage: pycoolctl method <Class> [Method] [--file PATH] [--body] [--json] [--limit N] [--offset N] [--pid PID] [--db PATH]\n");
		return 2;
	}

	return 0;
}

static int resolve_method_dbpath(const MethodArgs *args, char **resolved_db)
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

static int check_method_schema(sqlite3 *db)
{
	int sv = index_schema_version(db);
	if (sv != -1 && (sv != PYCOOL_SCHEMA_VERSION || !index_has_methods_table(db))) {
		fprintf(stderr, "error: schema mismatch (db=v%d, code=v%d)\n", sv, PYCOOL_SCHEMA_VERSION);
		return 1;
	}
	if (sv == -1) {
		char vsql[128];
		snprintf(vsql, sizeof(vsql), "INSERT OR REPLACE INTO meta(key,value) VALUES('schema_version','%d');", PYCOOL_SCHEMA_VERSION);
		sqlite3_exec(db, vsql, NULL, NULL, NULL);
	}
	return 0;
}

static void build_method_query(const MethodArgs *args, char *q, int qsize)
{
	snprintf(q, qsize,
	         "SELECT m.id, m.name, f.path, m.line, m.end_line, m.decorators, m.returns, m.docstring "
	         "FROM methods m JOIN symbols s ON m.class_symbol_id=s.id JOIN files f ON s.file_id=f.id "
	         "WHERE s.kind='class' AND s.name=?");
	size_t qlen = strlen(q);
	if (args->method_name) {
		snprintf(q + qlen, qsize - qlen, " AND m.name=?");
		qlen = strlen(q);
	}
	if (args->file) {
		snprintf(q + qlen, qsize - qlen, " AND f.path=?");
		qlen = strlen(q);
	}
	snprintf(q + qlen, qsize - qlen, " ORDER BY f.path, m.line");
	qlen = strlen(q);
	if (args->limit > 0) {
		snprintf(q + qlen, qsize - qlen, " LIMIT %d", args->limit);
		qlen = strlen(q);
	}
	if (args->offset > 0) {
		snprintf(q + qlen, qsize - qlen, " OFFSET %d", args->offset);
	}
}

static int bind_method_params(sqlite3_stmt *stmt, const MethodArgs *args)
{
	int bi = 1;
	sqlite3_bind_text(stmt, bi++, args->class_name, -1, SQLITE_TRANSIENT);
	if (args->method_name) {
		sqlite3_bind_text(stmt, bi++, args->method_name, -1, SQLITE_TRANSIENT);
	}
	if (args->file) {
		sqlite3_bind_text(stmt, bi++, args->file, -1, SQLITE_TRANSIENT);
	}
	return 0;
}

static void format_method_json(FILE *out, const MethodArgs *args, const char *mname, const char *mpath,
                               long mline, long mend, const char *mdecs, const char *mret, const char *mdoc,
                               const char *params, int *first)
{
	if (!*first) { fprintf(out, ",\n");
}
	*first = 0;
	fprintf(out, "  {\"class\":");
	print_json_str(out, args->class_name);
	fprintf(out, ",\"method\":");
	print_json_str(out, mname);
	fprintf(out, ",\"file\":");
	print_json_str(out, mpath);
	fprintf(out, ",\"line\":%ld,\"end_line\":%ld", mline, mend);
	fprintf(out, ",\"decorators\":");
	print_json_str(out, mdecs);
	fprintf(out, ",\"returns\":");
	print_json_str(out, mret);
	fprintf(out, ",\"docstring\":");
	print_json_str(out, mdoc);
	fprintf(out, ",\"params\":\"%s\"}", params ? params : "");
}

static void format_method_body(FILE *out, const MethodArgs *args, const char *mpath, long mline, long mend)
{
	if (mend <= 0) { mend = mline;
}
	long rstart = mline;
	long rend = mend;
	if (args->lines) {
		char buf[64]; strncpy(buf, args->lines, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
		char *comma = strchr(buf, ',');
		if (comma) { *comma='\0'; rstart = atoi(buf); rend = atoi(comma+1); }
		else { rstart = atoi(buf); rend = rstart; }
	}
	if (rstart < mline) { rstart = mline;
}
	if (rend > mend) { rend = mend;
}
	if (args->max_lines > 0 && rend - rstart + 1 > args->max_lines) { rend = rstart + args->max_lines - 1;
}
	print_class_body(out, mpath, rstart, rend);
}

static void format_method_regular(FILE *out, const MethodArgs *args, const char *mname, const char *mpath,
                                   long mline, long mend, const char *mdecs, const char *mret, const char *params)
{
	fprintf(out, "%s.%s(%s)", args->class_name, mname ? mname : "", params ? params : "");
	if (mret && mret[0]) { fprintf(out, " -> %s", mret);
}
	if (mdecs && mdecs[0]) { fprintf(out, " %s", mdecs);
}
	fprintf(out, " %s:%ld-%ld\n", mpath ? mpath : "", mline, mend);
}

static void format_method_row(FILE *out, sqlite3 *db, const MethodArgs *args, sqlite3_stmt *stmt, int *first)
{
	sqlite3_int64 mid = sqlite3_column_int64(stmt, 0);
	const char *mname = (const char *)sqlite3_column_text(stmt, 1);
	const char *mpath = (const char *)sqlite3_column_text(stmt, 2);
	long mline = sqlite3_column_int64(stmt, 3);
	long mend = sqlite3_column_int64(stmt, 4);
	const char *mdecs = (const char *)sqlite3_column_text(stmt, 5);
	const char *mret = (const char *)sqlite3_column_text(stmt, 6);
	const char *mdoc = (const char *)sqlite3_column_text(stmt, 7);
	char *params = build_params(db, mid);

	if (args->body_mode) {
		format_method_body(out, args, mpath, mline, mend);
	} else if (args->json_mode) {
		format_method_json(out, args, mname, mpath, mline, mend, mdecs, mret, mdoc, params, first);
	} else {
		format_method_regular(out, args, mname, mpath, mline, mend, mdecs, mret, params);
	}

	free(params);
}

static int execute_method_query(sqlite3 *db, const MethodArgs *args, const char *q, FILE *out)
{
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "query error: %s\n", sqlite3_errmsg(db));
		return 1;
	}

	bind_method_params(stmt, args);

	if (args->json_mode) { fprintf(out, "[\n");
}
	int first = 1;
	int had = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		had = 1;
		format_method_row(out, db, args, stmt, &first);
	}
	if (args->json_mode) {
		if (had) { fprintf(out, "\n]\n");
		} else { fprintf(out, "[]\n");
}
	}

	sqlite3_finalize(stmt);
	return 0;
}

int method_command(int argc, char **argv, FILE *out)
{
	MethodArgs args;
	int rc = parse_method_args(argc, argv, &args);
	if (rc != 0) { return rc; }

	char *resolved_db = NULL;
	const char *dbpath = args.dbpath;
	if (!dbpath) {
		if (resolve_method_dbpath(&args, &resolved_db) != 0) {
			return 1;
		}
		dbpath = resolved_db;
	}

	sqlite3 *db;
	if (sqlite3_open(dbpath, &db) != SQLITE_OK) {
		fprintf(stderr, "cannot open %s\n", sqlite3_errmsg(db));
		free(resolved_db);
		return 1;
	}

	if (check_method_schema(db) != 0) {
		sqlite3_close(db);
		free(resolved_db);
		return 1;
	}

	char q[2048];
	build_method_query(&args, q, sizeof(q));

	if (execute_method_query(db, &args, q, out) != 0) {
		sqlite3_close(db);
		free(resolved_db);
		return 1;
	}

	sqlite3_close(db);
	free(resolved_db);
	return 0;
}
