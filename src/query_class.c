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
					if (*p < 0x20) { fprintf(out, "\\u%04x", *p);
					} else { fputc(*p, out);
}
			}
		}
	}
	fputc('"', out);
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

typedef struct {
	const char *name;
	const char *info;
	const char *dbpath;
	const char *file;
	const char *pidstr;
	const char *method;
	const char *lines;
	int max_lines;
	int json_mode;
	int compact_mode;
	int full_mode;
	int body_mode;
	int refs_mode;
} ClassArgs;

static void set_default_mode(ClassArgs *args)
{
	if (!args->json_mode && args->info == NULL && !args->full_mode && !args->compact_mode)
		args->compact_mode = 1;
}

static void parse_valued_string_option(const char *opt, int *i, int argc, char **argv, const char **dest)
{
	if (strcmp(argv[*i], opt) == 0 && *i + 1 < argc)
		*dest = argv[++(*i)];
}

static void parse_int_option(const char *opt, int *i, int argc, char **argv, int *dest)
{
	if (strcmp(argv[*i], opt) == 0 && *i + 1 < argc)
		*dest = atoi(argv[++(*i)]);
}

static void parse_flag_option(int *dest)
{
	*dest = 1;
}

static void parse_class_args(int argc, char **argv, ClassArgs *args)
{
	memset(args, 0, sizeof(*args));
	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--info") == 0) {
			parse_valued_string_option("--info", &i, argc, argv, &args->info);
		} else if (strcmp(argv[i], "--db") == 0) {
			parse_valued_string_option("--db", &i, argc, argv, &args->dbpath);
		} else if (strcmp(argv[i], "--file") == 0 || strcmp(argv[i], "--module") == 0) {
			parse_valued_string_option(argv[i], &i, argc, argv, &args->file);
		} else if (strcmp(argv[i], "--json") == 0) {
			parse_flag_option(&args->json_mode);
		} else if (strcmp(argv[i], "--compact") == 0) {
			parse_flag_option(&args->compact_mode);
		} else if (strcmp(argv[i], "--full") == 0 || strcmp(argv[i], "--verbose") == 0) {
			parse_flag_option(&args->full_mode);
		} else if (strcmp(argv[i], "--body") == 0) {
			parse_flag_option(&args->body_mode);
		} else if (strcmp(argv[i], "--method") == 0) {
			parse_valued_string_option("--method", &i, argc, argv, &args->method);
		} else if (strcmp(argv[i], "--lines") == 0) {
			parse_valued_string_option("--lines", &i, argc, argv, &args->lines);
		} else if (strcmp(argv[i], "--max-lines") == 0) {
			parse_int_option("--max-lines", &i, argc, argv, &args->max_lines);
		} else if (strcmp(argv[i], "--refs") == 0) {
			parse_flag_option(&args->refs_mode);
		} else if (strcmp(argv[i], "--pid") == 0) {
			parse_valued_string_option("--pid", &i, argc, argv, &args->pidstr);
		} else if (!args->name) {
			args->name = argv[i];
		}
	}
	set_default_mode(args);
}

static int resolve_db_path(const char *dbpath, const char *pidstr, char **resolved_db, FILE *out)
{
	if (dbpath) { return 0; }
	if (pidstr) {
		char **roots, **dbpaths;
		pid_t *pids;
		int n;
		if (registry_list_paths(&roots, &dbpaths, &pids, &n) != 0 || n == 0) {
			fprintf(out, "no running instance; pass --db PATH\n");
			return 1;
		}
		pid_t target = (pid_t)atoi(pidstr);
		for (int i = 0; i < n; i++) {
			if (pids[i] == target) { *resolved_db = dbpaths[i]; return 0; }
		}
		fprintf(out, "pid %s not found\n", pidstr);
		return 1;
	}
	*resolved_db = registry_default_db();
	if (!*resolved_db) {
		fprintf(out, "no running instance and no default db; pass --db PATH or set PYCOOL_DB\n");
		return 1;
	}
	return 0;
}

static int check_schema_version(sqlite3 *db, FILE *out, const char *dbpath)
{
	int sv = index_schema_version(db);
	if (sv != -1 && sv != PYCOOL_SCHEMA_VERSION) {
		fprintf(out, "error: schema mismatch (db=v%d, code=v%d)\n", sv, PYCOOL_SCHEMA_VERSION);
		fprintf(out, "remedy: rebuild index with: pycool <root> %s\n", dbpath ? dbpath : "(db)");
		return 1;
	}
	if (sv == -1) {
		char vsql[128];
		snprintf(vsql, sizeof(vsql), "INSERT OR REPLACE INTO meta(key,value) VALUES('schema_version','%d');", PYCOOL_SCHEMA_VERSION);
		sqlite3_exec(db, vsql, NULL, NULL, NULL);
	}
	return 0;
}

static int handle_ambiguous_names(sqlite3 *db, const char *name, const char *file, FILE *out)
{
	if (name && !file) {
		char csql[512];
		snprintf(csql, sizeof(csql),
		         "SELECT count(DISTINCT f.path) FROM symbols s JOIN files f ON s.file_id=f.id WHERE s.kind='class' AND s.name=?;");
		sqlite3_stmt *cstmt;
		int nfiles = 0;
		if (sqlite3_prepare_v2(db, csql, -1, &cstmt, NULL) == SQLITE_OK) {
			sqlite3_bind_text(cstmt, 1, name, -1, SQLITE_TRANSIENT);
			if (sqlite3_step(cstmt) == SQLITE_ROW) { nfiles = sqlite3_column_int(cstmt, 0); }
			sqlite3_finalize(cstmt);
		}
		if (nfiles > 5) {
			fprintf(out, "ambiguous: '%s' in %d files; specify --file\n", name, nfiles);
			char lsql[512];
			snprintf(lsql, sizeof(lsql),
			         "SELECT f.path, s.line FROM symbols s JOIN files f ON s.file_id=f.id WHERE s.kind='class' AND s.name=? ORDER BY f.path;");
			sqlite3_stmt *lstmt;
			if (sqlite3_prepare_v2(db, lsql, -1, &lstmt, NULL) == SQLITE_OK) {
				sqlite3_bind_text(lstmt, 1, name, -1, SQLITE_TRANSIENT);
				while (sqlite3_step(lstmt) == SQLITE_ROW) {
					const char *p = (const char *)sqlite3_column_text(lstmt, 0);
					long ln = sqlite3_column_int64(lstmt, 1);
					fprintf(out, "  %s:%ld\n", p ? p : "", ln);
				}
				sqlite3_finalize(lstmt);
			}
			return 1;
		}
	}
	return 0;
}

typedef struct {
	int *arr;
	int count;
	int cap;
} LineArray;

static void flush_refs_line(FILE *out, const char *path, LineArray *lines, int total)
{
	if (!path || !path[0] || lines->count == 0) { return; }
	fprintf(out, "%s:", path);
	int i = 0;
	int first = 1;
	while (i < lines->count) {
		int start = lines->arr[i];
		int end = start;
		while (i + 1 < lines->count && lines->arr[i+1] == end + 1) { i++; end = lines->arr[i]; }
		if (!first) { fprintf(out, ","); }
		if (start == end) { fprintf(out, "%d", start);
		} else { fprintf(out, "%d-%d", start, end); }
		first = 0;
		i++;
	}
	if (total > lines->count) { fprintf(out, " (+%d)", total - lines->count); }
	fprintf(out, "\n");
}

static void print_method_body(sqlite3 *db, const char *n, const char *name, const char *file, const char *method, const char *lines, int max_lines, FILE *out)
{
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
	if (sqlite3_prepare_v2(db, msql, -1, &mstmt, NULL) != SQLITE_OK) { return; }
	sqlite3_bind_text(mstmt, 1, n ? n : name, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(mstmt, 2, method, -1, SQLITE_TRANSIENT);
	if (file) { sqlite3_bind_text(mstmt, 3, file, -1, SQLITE_TRANSIENT); }
	int found = 0;
	while (sqlite3_step(mstmt) == SQLITE_ROW) {
		const char *mpath = (const char *)sqlite3_column_text(mstmt, 0);
		long mstart = sqlite3_column_int64(mstmt, 1);
		long mend = sqlite3_column_int64(mstmt, 2);
		if (mend <= 0) { mend = mstart; }
		long rstart = mstart, rend = mend;
		if (lines) {
			char buf[64];
			strncpy(buf, lines, sizeof(buf)-1);
			buf[sizeof(buf)-1] = '\0';
			char *comma = strchr(buf, ',');
			if (comma) { *comma = '\0'; rstart = atoi(buf); rend = atoi(comma+1); }
			else { rstart = atoi(buf); rend = rstart; }
		}
		if (rstart < mstart) { rstart = mstart; }
		if (rend > mend) { rend = mend; }
		if (max_lines > 0 && rend - rstart + 1 > max_lines) { rend = rstart + max_lines - 1; }
		print_class_body(out, mpath, rstart, rend);
		found = 1;
	}
	sqlite3_finalize(mstmt);
	if (!found) { fprintf(stderr, "method not found: %s\n", method); }
}

static void print_class_body_range(const char *path, long line, long end_line, const char *lines, int max_lines, FILE *out)
{
	long el = end_line > 0 ? end_line : line;
	long rstart = line, rend = el;
	if (lines) {
		char buf[64];
		strncpy(buf, lines, sizeof(buf)-1);
		buf[sizeof(buf)-1] = '\0';
		char *comma = strchr(buf, ',');
		if (comma) { *comma = '\0'; rstart = atoi(buf); rend = atoi(comma+1); }
		else { rstart = atoi(buf); rend = rstart; }
	}
	if (rstart < line) { rstart = line; }
	if (rend > el) { rend = el; }
	if (max_lines > 0 && rend - rstart + 1 > max_lines) { rend = rstart + max_lines - 1; }
	print_class_body(out, path, rstart, rend);
}

static int handle_refs_mode(sqlite3 *db, const char *name, const char *file, FILE *out)
{
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
		return 1;
	}
	sqlite3_bind_text(rstmt, 1, name, -1, SQLITE_TRANSIENT);
	if (file) { sqlite3_bind_text(rstmt, 2, file, -1, SQLITE_TRANSIENT); }

	char curpath[4096] = "";
	LineArray lines = {NULL, 0, 0};
	int total = 0;

	while (sqlite3_step(rstmt) == SQLITE_ROW) {
		const char *p = (const char *)sqlite3_column_text(rstmt, 0);
		long ln = sqlite3_column_int64(rstmt, 1);
		if (!p) { p = ""; }
		if (strcmp(curpath, p) != 0) {
			flush_refs_line(out, curpath, &lines, total);
			strncpy(curpath, p, sizeof(curpath)-1);
			curpath[sizeof(curpath)-1] = '\0';
			lines.count = 0;
			total = 0;
		}
		if (lines.count == lines.cap) {
			lines.cap = lines.cap ? lines.cap * 2 : 64;
			int *tmp = realloc(lines.arr, lines.cap * sizeof(int));
			if (tmp) { lines.arr = tmp; }
		}
		lines.arr[lines.count++] = (int)ln;
		total++;
	}
	flush_refs_line(out, curpath, &lines, total);
	free(lines.arr);
	sqlite3_finalize(rstmt);
	return 0;
}

static void print_json_line(FILE *out, const char *n, const char *path, long line, const char *bases, const char *methods, const char *doc, const char *decs, long end_line, int *json_first)
{
	if (!*json_first) { fprintf(out, ",\n"); }
	*json_first = 0;
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
	fprintf(out, ",\"end_line\":%ld}", end_line);
}

static void print_compact_line(FILE *out, const char *n, const char *path, long line, const char *bases, const char *methods, const char *doc, const char *decs, long end_line)
{
	fprintf(out, "%s %s:%ld", n ? n : "", path ? path : "", line);
	if (bases && bases[0]) { fprintf(out, " bases=%s", bases); }
	if (methods && methods[0]) { fprintf(out, " methods=%s", methods); }
	if (doc && doc[0]) { fprintf(out, " docstring=%s", doc); }
	if (decs && decs[0]) { fprintf(out, " decorators=%s", decs); }
	if (end_line > 0) { fprintf(out, " end_line=%ld", end_line); }
	fprintf(out, "\n");
}

static void print_field(FILE *out, char code, const char *n, const char *path, long line, const char *bases, const char *methods, const char *doc, const char *decs, long end_line)
{
	switch (code) {
	case 'A': fprintf(out, "name=%s", n ? n : ""); break;
	case 'B': fprintf(out, "file=%s", path ? path : ""); break;
	case 'C': fprintf(out, "line=%ld", line); break;
	case 'D': fprintf(out, "bases=%s", bases ? bases : ""); break;
	case 'E': fprintf(out, "methods=%s", methods ? methods : ""); break;
	case 'F': fprintf(out, "fields=n/a"); break;
	case 'G': fprintf(out, "docstring=%s", doc ? doc : ""); break;
	case 'H': fprintf(out, "types=n/a"); break;
	case 'I': fprintf(out, "refs=n/a"); break;
	case 'J': fprintf(out, "decorators=%s", decs ? decs : ""); break;
	case 'L': fprintf(out, "end_line=%ld", end_line); break;
	}
}

static int should_skip_field(char code, const char *bases, const char *methods, const char *doc, const char *decs)
{
	if (code == 'D' && (!bases || !bases[0])) { return 1; }
	if (code == 'E' && (!methods || !methods[0])) { return 1; }
	if (code == 'G' && (!doc || !doc[0])) { return 1; }
	if (code == 'J' && (!decs || !decs[0])) { return 1; }
	if (code == 'K') { return 1; }
	return 0;
}

static void print_verbose_line(FILE *out, const char *info, const char *n, const char *path, long line, const char *bases, const char *methods, const char *doc, const char *decs, long end_line)
{
	int all = (info == NULL);
	int first = 1;
	const char *letters = all ? "ABCDEGJL" : info;
	for (const char *p = letters; *p; p++) {
		const char *fname = field_name(*p);
		if (!fname || should_skip_field(*p, bases, methods, doc, decs)) { continue; }
		if (!first) { fprintf(out, " | "); }
		first = 0;
		print_field(out, *p, n, path, line, bases, methods, doc, decs, end_line);
	}
	fprintf(out, "\n");
}

static void print_output_line(FILE *out, int json_mode, int compact_mode, const char *info, const char *n, const char *path, long line, const char *bases, const char *methods, const char *doc, const char *decs, long end_line, int *json_first)
{
	if (json_mode) {
		print_json_line(out, n, path, line, bases, methods, doc, decs, end_line, json_first);
	} else if (compact_mode) {
		print_compact_line(out, n, path, line, bases, methods, doc, decs, end_line);
	} else {
		print_verbose_line(out, info, n, path, line, bases, methods, doc, decs, end_line);
	}
}

int class_command(int argc, char **argv, FILE *out)
{
	ClassArgs args;
	parse_class_args(argc, argv, &args);

	if (!args.name) {
		fprintf(stderr, "usage: pycoolctl class <Name> [--file PATH] [--info LETTERS] [--compact|--full|--verbose|--json|--body|--refs] [--pid PID] [--db PATH]\n");
		return 2;
	}

	char *resolved_db = NULL;
	if (resolve_db_path(args.dbpath, args.pidstr, &resolved_db, stderr) != 0) { return 1; }
	const char *dbpath = args.dbpath ? args.dbpath : resolved_db;

	sqlite3 *db;
	if (sqlite3_open(dbpath, &db) != SQLITE_OK) {
		fprintf(stderr, "cannot open %s\n", dbpath);
		free(resolved_db);
		return 1;
	}

	if (check_schema_version(db, out, dbpath) != 0) {
		sqlite3_close(db);
		free(resolved_db);
		return 1;
	}

	if (args.refs_mode) {
		int ret = handle_refs_mode(db, args.name, args.file, out);
		sqlite3_close(db);
		free(resolved_db);
		return ret;
	}

	if (handle_ambiguous_names(db, args.name, args.file, out) != 0) {
		sqlite3_close(db);
		free(resolved_db);
		return 0;
	}

	char sql[2048];
	if (args.name) {
		if (args.file) {
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
	}
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "query error: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		free(resolved_db);
		return 1;
	}
	int bind_idx = 1;
	sqlite3_bind_text(stmt, bind_idx++, args.name, -1, SQLITE_TRANSIENT);
	if (args.file) { sqlite3_bind_text(stmt, bind_idx++, args.file, -1, SQLITE_TRANSIENT); }

	int had_rows = 0;
	int json_first = 1;
	if (args.json_mode) { fprintf(out, "[\n"); }

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

		if (args.body_mode) {
			if (args.method) {
				print_method_body(db, n, args.name, args.file, args.method, args.lines, args.max_lines, out);
			} else {
				print_class_body_range(path, line, end_line, args.lines, args.max_lines, out);
			}
			continue;
		}

		print_output_line(out, args.json_mode, args.compact_mode, args.info, n, path, line, bases, methods, doc, decs, end_line, &json_first);
	}
	if (args.json_mode) {
		if (had_rows) { fprintf(out, "\n]\n");
		} else { fprintf(out, "[]\n"); }
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	free(resolved_db);
	return 0;
}
