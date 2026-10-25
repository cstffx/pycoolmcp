#define _GNU_SOURCE
#include "class.h"
#include "../src/registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include "../index/core.h"

static const char *field_name(char c)
{
	switch (c) {
	case 'A': return "name";
	case 'B': return "file";
	case 'C': return "line";
	case 'D': return "bases";
	case 'E': return "methods";
	case 'F': return "fields";
	case 'G': return "docstring";
	case 'H': return "types";
	case 'I': return "refs";
	case 'J': return "decorators";
	case 'K': return "module";
	case 'L': return "end_line";
	default: return NULL;
	}
}

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

int class_command(int argc, char **argv, FILE *out)
{
	const char *name = NULL;
	const char *info = NULL;
	const char *dbpath = NULL;
	const char *file = NULL;
	const char *pidstr = NULL;
	const char *method = NULL;
	int json_mode = 0;
	int compact_mode = 0;
	int full_mode = 0;
	int body_mode = 0;
	int refs_mode = 0;
	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--info") == 0 && i + 1 < argc) {
			info = argv[++i];
		} else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
			dbpath = argv[++i];
		} else if ((strcmp(argv[i], "--file") == 0 || strcmp(argv[i], "--module") == 0) && i + 1 < argc) {
			file = argv[++i];
		} else if (strcmp(argv[i], "--json") == 0) {
			json_mode = 1;
		} else if (strcmp(argv[i], "--compact") == 0) {
			compact_mode = 1;
		} else if (strcmp(argv[i], "--full") == 0 || strcmp(argv[i], "--verbose") == 0) {
			full_mode = 1;
		} else if (strcmp(argv[i], "--body") == 0) {
			body_mode = 1;
		} else if (strcmp(argv[i], "--method") == 0 && i + 1 < argc) {
			method = argv[++i];
		} else if (strcmp(argv[i], "--refs") == 0) {
			refs_mode = 1;
		} else if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
			pidstr = argv[++i];
		} else if (!name) {
			name = argv[i];
		}
	}
	if (!json_mode && info == NULL && !full_mode && !compact_mode) {
		compact_mode = 1;
	}
	if (!name) {
		fprintf(stderr, "usage: pycoolctl class <Name> [--file PATH] [--info LETTERS] [--compact|--full|--verbose|--json|--body|--refs] [--pid PID] [--db PATH]\n");
		return 2;
	}
	if (!dbpath) {
		char **roots, **dbpaths; pid_t *pids; int n;
		if (registry_list_paths(&roots, &dbpaths, &pids, &n) != 0 || n == 0) {
			fprintf(stderr, "no running instance; pass --db PATH\n");
			return 1;
		}
		if (pidstr) {
			pid_t target = (pid_t)atoi(pidstr);
			for (int i = 0; i < n; i++) {
				if (pids[i] == target) { dbpath = dbpaths[i]; break; }
			}
			if (!dbpath) {
				fprintf(stderr, "pid %s not found\n", pidstr);
				return 1;
			}
		} else {
			dbpath = dbpaths[0];
		}
	}
	sqlite3 *db;
	if (sqlite3_open(dbpath, &db) != SQLITE_OK) {
		fprintf(stderr, "cannot open %s\n", dbpath);
		return 1;
	}
	{
		int sv = index_schema_version(db);
		if (sv != -1 && sv != PYCOOL_SCHEMA_VERSION) {
			fprintf(out, "error: schema mismatch (db=v%d, code=v%d)\n", sv, PYCOOL_SCHEMA_VERSION);
			fprintf(out, "remedy: rebuild index with: pycool <root> %s\n", dbpath ? dbpath : "(db)");
			sqlite3_close(db);
			return 1;
		}
		if (sv == -1) {
			char vsql[128];
			snprintf(vsql, sizeof(vsql), "INSERT OR REPLACE INTO meta(key,value) VALUES('schema_version','%d');", PYCOOL_SCHEMA_VERSION);
			sqlite3_exec(db, vsql, NULL, NULL, NULL);
		}
	}
	{
		int sv = index_schema_version(db);
		if (sv != -1 && sv != PYCOOL_SCHEMA_VERSION) {
			fprintf(out, "error: schema mismatch (db=v%d, code=v%d)\n", sv, PYCOOL_SCHEMA_VERSION);
			fprintf(out, "remedy: rebuild index with: pycool <root> %s\n", dbpath ? dbpath : "(db)");
			sqlite3_close(db);
			return 1;
		}
		if (sv == -1) {
			char vsql[128];
			snprintf(vsql, sizeof(vsql), "INSERT OR REPLACE INTO meta(key,value) VALUES('schema_version','%d');", PYCOOL_SCHEMA_VERSION);
			sqlite3_exec(db, vsql, NULL, NULL, NULL);
		}
	}
	if (refs_mode) {
		if (!name) {
			fprintf(stderr, "usage: pycoolctl class <Name> --refs [--file PATH] [--db PATH]\n");
			sqlite3_close(db);
			return 2;
		}
		char rsql[2048];
		if (file) {
			snprintf(rsql, sizeof(rsql),
			         "SELECT DISTINCT f.path, r.line FROM refs r "
			         "JOIN files f ON r.file_id=f.id "
			         "JOIN symbols s ON r.symbol_id=s.id "
			         "WHERE s.kind='class' AND s.name=? AND f.path=? "
			         "ORDER BY f.path, r.line;");
		} else {
			snprintf(rsql, sizeof(rsql),
			         "SELECT DISTINCT f.path, r.line FROM refs r "
			         "JOIN files f ON r.file_id=f.id "
			         "JOIN symbols s ON r.symbol_id=s.id "
			         "WHERE s.kind='class' AND s.name=? "
			         "ORDER BY f.path, r.line;");
		}
		sqlite3_stmt *rstmt;
		if (sqlite3_prepare_v2(db, rsql, -1, &rstmt, NULL) != SQLITE_OK) {
			fprintf(stderr, "query error: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			return 1;
		}
		sqlite3_bind_text(rstmt, 1, name, -1, SQLITE_TRANSIENT);
		if (file) sqlite3_bind_text(rstmt, 2, file, -1, SQLITE_TRANSIENT);
		while (sqlite3_step(rstmt) == SQLITE_ROW) {
			const char *p = (const char *)sqlite3_column_text(rstmt, 0);
			long ln = sqlite3_column_int64(rstmt, 1);
			fprintf(out, "%s:%ld\n", p ? p : "", ln);
		}
		sqlite3_finalize(rstmt);
		sqlite3_close(db);
		return 0;
	}

	char sql[2048];
	if (name) {
		if (file) {
			snprintf(sql, sizeof(sql),
			         "SELECT s.name, f.path, s.line, cd.bases, cd.methods, cd.docstring, cd.decorators, cd.end_line "
			         "FROM symbols s JOIN files f ON s.file_id=f.id "
			         "LEFT JOIN class_details cd ON cd.symbol_id=s.id "
			         "WHERE s.kind='class' AND s.name=? AND f.path=?;");
		} else {
			snprintf(sql, sizeof(sql),
			         "SELECT s.name, f.path, s.line, cd.bases, cd.methods, cd.docstring, cd.decorators, cd.end_line "
			         "FROM symbols s JOIN files f ON s.file_id=f.id "
			         "LEFT JOIN class_details cd ON cd.symbol_id=s.id "
			         "WHERE s.kind='class' AND s.name=?;");
		}
	} else {
		if (file) {
			snprintf(sql, sizeof(sql),
			         "SELECT s.name, f.path, s.line, cd.bases, cd.methods, cd.docstring, cd.decorators, cd.end_line "
			         "FROM symbols s JOIN files f ON s.file_id=f.id "
			         "LEFT JOIN class_details cd ON cd.symbol_id=s.id "
			         "WHERE s.kind='class' AND f.path=?;");
		} else {
			snprintf(sql, sizeof(sql),
			         "SELECT s.name, f.path, s.line, cd.bases, cd.methods, cd.docstring, cd.decorators, cd.end_line "
			         "FROM symbols s JOIN files f ON s.file_id=f.id "
			         "LEFT JOIN class_details cd ON cd.symbol_id=s.id "
			         "WHERE s.kind='class';");
		}
	}
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "query error: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}
	int bind_idx = 1;
	if (name) sqlite3_bind_text(stmt, bind_idx++, name, -1, SQLITE_TRANSIENT);
	if (file) sqlite3_bind_text(stmt, bind_idx++, file, -1, SQLITE_TRANSIENT);

	int all = (info == NULL);
	int had_rows = 0;
	int json_first = 1;
	if (json_mode) fprintf(out, "[\n");
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char *n = (const char *)sqlite3_column_text(stmt, 0);
		const char *path = (const char *)sqlite3_column_text(stmt, 1);
		long line = sqlite3_column_int64(stmt, 2);
		const char *bases = (const char *)sqlite3_column_text(stmt, 3);
		const char *methods = (const char *)sqlite3_column_text(stmt, 4);
		const char *doc = (const char *)sqlite3_column_text(stmt, 5);
		const char *decs = (const char *)sqlite3_column_text(stmt, 6);
		long end_line = sqlite3_column_int64(stmt, 7);
		had_rows = 1;

		if (body_mode) {
			if (method) {
				char msql[2048];
				if (file) {
					snprintf(msql, sizeof(msql),
					         "SELECT f.path, m.line, m.end_line FROM methods m "
					         "JOIN symbols s ON m.class_symbol_id=s.id "
					         "JOIN files f ON s.file_id=f.id "
					         "WHERE s.kind='class' AND s.name=? AND m.name=? AND f.path=?;");
				} else {
					snprintf(msql, sizeof(msql),
					         "SELECT f.path, m.line, m.end_line FROM methods m "
					         "JOIN symbols s ON m.class_symbol_id=s.id "
					         "JOIN files f ON s.file_id=f.id "
					         "WHERE s.kind='class' AND s.name=? AND m.name=?;");
				}
				sqlite3_stmt *mstmt;
				if (sqlite3_prepare_v2(db, msql, -1, &mstmt, NULL) == SQLITE_OK) {
					sqlite3_bind_text(mstmt, 1, n ? n : name, -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(mstmt, 2, method, -1, SQLITE_TRANSIENT);
					if (file) sqlite3_bind_text(mstmt, 3, file, -1, SQLITE_TRANSIENT);
					int found = 0;
					while (sqlite3_step(mstmt) == SQLITE_ROW) {
						const char *mpath = (const char *)sqlite3_column_text(mstmt, 0);
						long mstart = sqlite3_column_int64(mstmt, 1);
						long mend = sqlite3_column_int64(mstmt, 2);
						if (mend <= 0) mend = mstart;
						print_class_body(out, mpath, mstart, mend);
						found = 1;
					}
					sqlite3_finalize(mstmt);
					if (!found) fprintf(stderr, "method not found: %s\n", method);
				}
			} else {
				long el = end_line > 0 ? end_line : line;
				print_class_body(out, path, line, el);
			}
			continue;
		}

		if (json_mode) {
			if (!json_first) fprintf(out, ",\n");
			json_first = 0;
			fprintf(out, "  {\"name\":");
			print_json_str(out, n);
			fprintf(out, ",\"file\":");
			print_json_str(out, path);
			fprintf(out, ",\"line\":%ld", line);
			fprintf(out, ",\"bases\":");
			print_json_str(out, bases);
			fprintf(out, ",\"methods\":");
			print_json_str(out, methods);
			fprintf(out, ",\"docstring\":");
			print_json_str(out, doc);
			fprintf(out, ",\"decorators\":");
			print_json_str(out, decs);
			fprintf(out, ",\"end_line\":%ld", end_line);
			fprintf(out, "}");
		} else if (compact_mode) {
			fprintf(out, "%s %s:%ld", n ? n : "", path ? path : "", line);
			if (bases && bases[0]) fprintf(out, " bases=%s", bases);
			if (methods && methods[0]) fprintf(out, " methods=%s", methods);
			if (doc && doc[0]) fprintf(out, " docstring=%s", doc);
			if (decs && decs[0]) fprintf(out, " decorators=%s", decs);
			if (end_line > 0) fprintf(out, " end_line=%ld", end_line);
			fprintf(out, "\n");
		} else {
			/* print selected or all */
			int first = 1;
			const char *letters = all ? "ABCDEFGIJKL" : info;
			for (const char *p = letters; *p; p++) {
				const char *fname = field_name(*p);
				if (!fname) continue;
				if (!first) fprintf(out, " | ");
				first = 0;
				if (*p == 'A') fprintf(out, "name=%s", n ? n : "");
				else if (*p == 'B') fprintf(out, "file=%s", path ? path : "");
				else if (*p == 'C') fprintf(out, "line=%ld", line);
				else if (*p == 'D') fprintf(out, "bases=%s", bases ? bases : "");
				else if (*p == 'E') fprintf(out, "methods=%s", methods ? methods : "");
				else if (*p == 'F') fprintf(out, "fields=n/a");
				else if (*p == 'G') fprintf(out, "docstring=%s", doc ? doc : "");
				else if (*p == 'H') fprintf(out, "types=n/a");
				else if (*p == 'I') fprintf(out, "refs=n/a");
				else if (*p == 'J') fprintf(out, "decorators=%s", decs ? decs : "");
				else if (*p == 'K') fprintf(out, "module=%s", path ? path : "");
				else if (*p == 'L') fprintf(out, "end_line=%ld", end_line);
			}
			fprintf(out, "\n");
		}
	}
	if (json_mode) {
		if (had_rows) fprintf(out, "\n]\n");
		else fprintf(out, "[]\n");
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return 0;
}
