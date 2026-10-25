#define _GNU_SOURCE
#include "method.h"
#include "../src/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "../index/core.h"

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
					if (*p < 0x20) fprintf(out, "\\u%04x", *p);
					else fputc(*p, out);
			}
		}
	}
	fputc('"', out);
}

static char *build_params(sqlite3 *db, sqlite3_int64 method_id)
{
	char *buf = NULL;
	size_t cap = 0, len = 0;
	sqlite3_stmt *stmt;
	const char *sql = "SELECT name, annotation, default_value, kind FROM method_args WHERE method_id=? ORDER BY position;";
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return strdup("");
	sqlite3_bind_int64(stmt, 1, method_id);
	int first = 1;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char *name = (const char *)sqlite3_column_text(stmt, 0);
		const char *ann = (const char *)sqlite3_column_text(stmt, 1);
		const char *def = (const char *)sqlite3_column_text(stmt, 2);
		const char *kind = (const char *)sqlite3_column_text(stmt, 3);
		char piece[512];
		piece[0] = '\0';
		if (kind && strcmp(kind, "varargs") == 0) snprintf(piece, sizeof(piece), "*%s", name ? name : "");
		else if (kind && strcmp(kind, "kwargs") == 0) snprintf(piece, sizeof(piece), "**%s", name ? name : "");
		else snprintf(piece, sizeof(piece), "%s", name ? name : "");
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
			while (len + plen + 4 >= cap) cap *= 2;
			buf = realloc(buf, cap);
		}
		if (!first) { buf[len++] = ','; buf[len++] = ' '; }
		first = 0;
		memcpy(buf + len, piece, plen);
		len += plen;
		buf[len] = '\0';
	}
	sqlite3_finalize(stmt);
	if (!buf) buf = strdup("");
	return buf;
}

static void print_class_body(FILE *out, const char *path, long start, long end)
{
	if (!path || start <= 0) return;
	if (end < start) end = start;
	FILE *f = fopen(path, "r");
	if (!f) { perror(path); return; }
	char *line = NULL;
	size_t cap = 0;
	ssize_t len;
	long cur = 0;
	while ((len = getline(&line, &cap, f)) != -1) {
		cur++;
		if (cur >= start && cur <= end) fputs(line, out);
		if (cur >= end) break;
	}
	free(line);
	fclose(f);
}

int method_command(int argc, char **argv, FILE *out)
{
	const char *class_name = NULL;
	const char *method_name = NULL;
	const char *file = NULL;
	const char *pidstr = NULL;
	const char *dbpath = NULL;
	int json_mode = 0;
	int body_mode = 0;
	int limit = 0;
	const char *lines = NULL;
	int max_lines = 0;
	int offset = 0;
	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) dbpath = argv[++i];
		else if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) pidstr = argv[++i];
		else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) file = argv[++i];
		else if (strcmp(argv[i], "--json") == 0) json_mode = 1;
		else if (strcmp(argv[i], "--body") == 0) body_mode = 1;
		else if (strcmp(argv[i], "--lines") == 0 && i + 1 < argc) lines = argv[++i];
		else if (strcmp(argv[i], "--max-lines") == 0 && i + 1 < argc) max_lines = atoi(argv[++i]);
		else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) limit = atoi(argv[++i]);
		else if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) offset = atoi(argv[++i]);
		else if (!class_name) class_name = argv[i];
		else if (!method_name) method_name = argv[i];
	}
	if (!class_name) {
		fprintf(stderr, "usage: pycoolctl method <Class> [Method] [--file PATH] [--body] [--json] [--limit N] [--offset N] [--pid PID] [--db PATH]\n");
		return 2;
	}
	char *resolved_db = NULL;
	if (!dbpath) {
		if (pidstr) {
			char **roots, **dbpaths; pid_t *pids; int n;
			if (registry_list_paths(&roots, &dbpaths, &pids, &n) != 0 || n == 0) {
				fprintf(stderr, "no running instance; pass --db PATH\n");
				return 1;
			}
			pid_t target = (pid_t)atoi(pidstr);
			for (int i = 0; i < n; i++) if (pids[i] == target) { dbpath = dbpaths[i]; break; }
			if (!dbpath) { fprintf(stderr, "pid %s not found\n", pidstr); return 1; }
		} else {
			resolved_db = registry_default_db();
			if (resolved_db) dbpath = resolved_db;
			else { fprintf(stderr, "no running instance and no default db; pass --db PATH or set PYCOOL_DB\n"); return 1; }
		}
	}
	sqlite3 *db;
	if (sqlite3_open(dbpath, &db) != SQLITE_OK) {
		fprintf(stderr, "cannot open %s\n", sqlite3_errmsg(db));
		return 1;
	}
	{
		int sv = index_schema_version(db);
		if (sv != -1 && (sv != PYCOOL_SCHEMA_VERSION || !index_has_methods_table(db))) {
			fprintf(out, "error: schema mismatch (db=v%d, code=v%d)\n", sv, PYCOOL_SCHEMA_VERSION);
			fprintf(out, "remedy: rebuild index with: pycool <root> %s\n", dbpath ? dbpath : "(db)");
			sqlite3_close(db);
			free(resolved_db);
			return 1;
		}
		if (sv == -1) {
			char vsql[128];
			snprintf(vsql, sizeof(vsql), "INSERT OR REPLACE INTO meta(key,value) VALUES('schema_version','%d');", PYCOOL_SCHEMA_VERSION);
			sqlite3_exec(db, vsql, NULL, NULL, NULL);
		}
	}
	char sql[2048];
	if (file) {
		snprintf(sql, sizeof(sql),
		         "SELECT m.id, m.name, f.path, m.line, m.end_line, m.decorators, m.returns, m.docstring "
		         "FROM methods m JOIN symbols s ON m.class_symbol_id=s.id JOIN files f ON s.file_id=f.id "
		         "WHERE s.kind='class' AND s.name=? AND m.name %s ? AND f.path=? "
		         "ORDER BY f.path, m.line %s %s;",
		         method_name ? "=" : "LIKE", limit > 0 ? "" : "", offset > 0 ? "" : "");
	} else {
		snprintf(sql, sizeof(sql),
		         "SELECT m.id, m.name, f.path, m.line, m.end_line, m.decorators, m.returns, m.docstring "
		         "FROM methods m JOIN symbols s ON m.class_symbol_id=s.id JOIN files f ON s.file_id=f.id "
		         "WHERE s.kind='class' AND s.name=? %s %s "
		         "ORDER BY f.path, m.line %s %s;",
		         method_name ? "AND m.name=?" : "", "", limit > 0 ? "" : "", offset > 0 ? "" : "");
	}
	/* rebuild cleanly with optional filters */
	/* We'll just prepare a simpler dynamic query */
	sqlite3_stmt *stmt;
	/* Construct final query string properly */
	char q[2048];
	q[0] = '\0';
	snprintf(q, sizeof(q),
	         "SELECT m.id, m.name, f.path, m.line, m.end_line, m.decorators, m.returns, m.docstring "
	         "FROM methods m JOIN symbols s ON m.class_symbol_id=s.id JOIN files f ON s.file_id=f.id "
	         "WHERE s.kind='class' AND s.name=?");
	size_t qlen = strlen(q);
	if (method_name) {
		snprintf(q + qlen, sizeof(q) - qlen, " AND m.name=?");
		qlen = strlen(q);
	}
	if (file) {
		snprintf(q + qlen, sizeof(q) - qlen, " AND f.path=?");
		qlen = strlen(q);
	}
	snprintf(q + qlen, sizeof(q) - qlen, " ORDER BY f.path, m.line");
	qlen = strlen(q);
	if (limit > 0) {
		snprintf(q + qlen, sizeof(q) - qlen, " LIMIT %d", limit);
		qlen = strlen(q);
	}
	if (offset > 0) {
		snprintf(q + qlen, sizeof(q) - qlen, " OFFSET %d", offset);
		qlen = strlen(q);
	}
	if (sqlite3_prepare_v2(db, q, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "query error: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}
	int bi = 1;
	sqlite3_bind_text(stmt, bi++, class_name, -1, SQLITE_TRANSIENT);
	if (method_name) sqlite3_bind_text(stmt, bi++, method_name, -1, SQLITE_TRANSIENT);
	if (file) sqlite3_bind_text(stmt, bi++, file, -1, SQLITE_TRANSIENT);

	if (json_mode) fprintf(out, "[\n");
	int first = 1;
	int had = 0;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		had = 1;
		sqlite3_int64 mid = sqlite3_column_int64(stmt, 0);
		const char *mname = (const char *)sqlite3_column_text(stmt, 1);
		const char *mpath = (const char *)sqlite3_column_text(stmt, 2);
		long mline = sqlite3_column_int64(stmt, 3);
		long mend = sqlite3_column_int64(stmt, 4);
		const char *mdecs = (const char *)sqlite3_column_text(stmt, 5);
		const char *mret = (const char *)sqlite3_column_text(stmt, 6);
		const char *mdoc = (const char *)sqlite3_column_text(stmt, 7);
		char *params = build_params(db, mid);
		if (body_mode) {
			if (mend <= 0) mend = mline;
			long rstart = mline, rend = mend;
			if (lines) {
				char buf[64]; strncpy(buf, lines, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
				char *comma = strchr(buf, ',');
				if (comma) { *comma='\0'; rstart = atoi(buf); rend = atoi(comma+1); }
				else { rstart = atoi(buf); rend = rstart; }
			}
			if (rstart < mline) rstart = mline;
			if (rend > mend) rend = mend;
			if (max_lines > 0 && rend - rstart + 1 > max_lines) rend = rstart + max_lines - 1;
			print_class_body(out, mpath, rstart, rend);
			free(params);
			continue;
		}
		if (json_mode) {
			if (!first) fprintf(out, ",\n");
			first = 0;
			fprintf(out, "  {\"class\":");
			print_json_str(out, class_name);
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
		} else {
			fprintf(out, "%s.%s(%s)", class_name, mname ? mname : "", params ? params : "");
			if (mret && mret[0]) fprintf(out, " -> %s", mret);
			if (mdecs && mdecs[0]) fprintf(out, " %s", mdecs);
			fprintf(out, " %s:%ld-%ld\n", mpath ? mpath : "", mline, mend);
		}
		free(params);
	}
	if (json_mode) {
		if (had) fprintf(out, "\n]\n");
		else fprintf(out, "[]\n");
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	free(resolved_db);
	return 0;
}