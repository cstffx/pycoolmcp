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

static int match_text(const char *hay, const char *needle, int regex_mode, regex_t *re)
{
	if (!hay) { return 0;
}
	if (regex_mode) {
		return regexec(re, hay, 0, NULL, 0) == 0;
	}
	char *h = strdup(hay);
	char *n = strdup(needle);
	if (!h || !n) { free(h); free(n); return 0; }
	for (char *p = h; *p; p++) { *p = (char)tolower((unsigned char)*p);
}
	for (char *p = n; *p; p++) { *p = (char)tolower((unsigned char)*p);
}
	int r = strstr(h, n) != NULL;
	free(h); free(n);
	return r;
}

int search_command(int argc, char **argv, FILE *out)
{
	const char *query = NULL;
	int regex_mode = 0;
	const char *kind = "all";
	const char *file = NULL;
	const char *dbpath = NULL;
	const char *pidstr = NULL;
	int limit = 0;
	int offset = 0;

	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--regex") == 0) { regex_mode = 1;
		} else if (strcmp(argv[i], "--kind") == 0 && i + 1 < argc) { kind = argv[++i];
		} else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) { file = argv[++i];
		} else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) { dbpath = argv[++i];
		} else if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) { pidstr = argv[++i];
		} else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) { limit = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) { offset = atoi(argv[++i]);
		} else if (!query) { query = argv[i];
}
	}

	if (!query) {
		fprintf(stderr, "usage: pycoolctl search <query> [--regex] [--kind ...] [--file GLOB] [--db PATH] [--limit N] [--offset N]\n");
		return 2;
	}

	char *resolved_db = NULL;
	if (!dbpath) {
		if (pidstr) {
			char **roots;
			char **dbpaths; pid_t *pids; int n;
			if (registry_list_paths(&roots, &dbpaths, &pids, &n) != 0 || n == 0) {
				fprintf(stderr, "no running instance; pass --db PATH\n");
				return 1;
			}
			pid_t target = (pid_t)atoi(pidstr);
			for (int i = 0; i < n; i++) { if (pids[i] == target) { dbpath = dbpaths[i]; break; }
}
			if (!dbpath) { fprintf(stderr, "pid %s not found\n", pidstr); return 1; }
		} else {
			resolved_db = registry_default_db();
			if (resolved_db) { dbpath = resolved_db;
			} else { fprintf(stderr, "no db; pass --db PATH or set PYCOOL_DB\n"); return 1; }
		}
	}

	sqlite3 *db;
	if (sqlite3_open(dbpath, &db) != SQLITE_OK) {
		fprintf(stderr, "cannot open db: %s\n", dbpath);
		free(resolved_db);
		return 1;
	}
	sqlite3_busy_timeout(db, 5000);

	int sv = index_schema_version(db);
	if (sv != -1 && sv != PYCOOL_SCHEMA_VERSION) {
		fprintf(out, "error: schema mismatch (db=v%d, code=v%d)\n", sv, PYCOOL_SCHEMA_VERSION);
		sqlite3_close(db);
		free(resolved_db);
		return 1;
	}

	regex_t re;
	int re_ok = 0;
	if (regex_mode) {
		if (regcomp(&re, query, REG_EXTENDED | REG_ICASE | REG_NOSUB) != 0) {
			fprintf(stderr, "invalid regex: %s\n", query);
			sqlite3_close(db);
			free(resolved_db);
			return 1;
		}
		re_ok = 1;
	}

	typedef struct { char *path; long line; char *kind; char *name; char *doc; } Row;
	Row *rows = NULL;
	int nrows = 0;
	int caprows = 0;

	int want_class = (strcmp(kind, "all") == 0 || strcmp(kind, "class") == 0);
	int want_function = (strcmp(kind, "all") == 0 || strcmp(kind, "function") == 0);
	int want_variable = (strcmp(kind, "all") == 0 || strcmp(kind, "variable") == 0);
	int want_method = (strcmp(kind, "all") == 0 || strcmp(kind, "method") == 0);

	if (want_class || want_function || want_variable) {
		char sql[1024];
		snprintf(sql, sizeof(sql),
			"SELECT s.kind, s.name, s.line, f.path, cd.docstring "
			"FROM symbols s JOIN files f ON s.file_id=f.id "
			"LEFT JOIN class_details cd ON cd.symbol_id=s.id "
			"WHERE 1=1");
		char kindclause[256] = "";
		if (!(want_class && want_function && want_variable)) {
			strcat(kindclause, " AND s.kind IN (");
			if (want_class) { if (kindclause[strlen(kindclause)-1] != '(') { strcat(kindclause, ","); 
}strcat(kindclause, "'class'"); }
			if (want_function) { if (kindclause[strlen(kindclause)-1] != '(') { strcat(kindclause, ","); 
}strcat(kindclause, "'function'"); }
			if (want_variable) { if (kindclause[strlen(kindclause)-1] != '(') { strcat(kindclause, ","); 
}strcat(kindclause, "'variable'"); }
			strcat(kindclause, ")");
		}
		strcat(sql, kindclause);
		if (file) {
			if (strchr(file, '*') || strchr(file, '?')) {
				snprintf(sql + strlen(sql), sizeof(sql) - strlen(sql), " AND f.path GLOB '%s'", file);
			} else {
				snprintf(sql + strlen(sql), sizeof(sql) - strlen(sql), " AND f.path = '%s'", file);
}
		}
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				const char *k = (const char *)sqlite3_column_text(stmt, 0);
				const char *nm = (const char *)sqlite3_column_text(stmt, 1);
				long ln = sqlite3_column_int64(stmt, 2);
				const char *p = (const char *)sqlite3_column_text(stmt, 3);
				const char *doc = (const char *)sqlite3_column_text(stmt, 4);
				if (!match_text(nm, query, regex_mode, &re) && !match_text(doc, query, regex_mode, &re)) { continue;
}
				if (nrows == caprows) { caprows = caprows ? caprows*2 : 64; Row *tmp = realloc(rows, caprows*sizeof(Row)); if (tmp) { rows = tmp; 
}}
				rows[nrows].path = strdup(p ? p : "");
				rows[nrows].line = ln;
				rows[nrows].kind = strdup(k ? k : "");
				rows[nrows].name = strdup(nm ? nm : "");
				rows[nrows].doc = doc ? strdup(doc) : NULL;
				nrows++;
			}
			sqlite3_finalize(stmt);
		}
	}

	if (want_method) {
		char sql[1024];
		snprintf(sql, sizeof(sql),
			"SELECT m.name, m.line, f.path, m.docstring "
			"FROM methods m JOIN symbols s ON m.class_symbol_id=s.id JOIN files f ON s.file_id=f.id "
			"WHERE 1=1");
		if (file) {
			if (strchr(file, '*') || strchr(file, '?')) {
				snprintf(sql + strlen(sql), sizeof(sql) - strlen(sql), " AND f.path GLOB '%s'", file);
			} else {
				snprintf(sql + strlen(sql), sizeof(sql) - strlen(sql), " AND f.path = '%s'", file);
}
		}
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				const char *nm = (const char *)sqlite3_column_text(stmt, 0);
				long ln = sqlite3_column_int64(stmt, 1);
				const char *p = (const char *)sqlite3_column_text(stmt, 2);
				const char *doc = (const char *)sqlite3_column_text(stmt, 3);
				if (!match_text(nm, query, regex_mode, &re) && !match_text(doc, query, regex_mode, &re)) { continue;
}
				if (nrows == caprows) { caprows = caprows ? caprows*2 : 64; Row *tmp = realloc(rows, caprows*sizeof(Row)); if (tmp) { rows = tmp; 
}}
				rows[nrows].path = strdup(p ? p : "");
				rows[nrows].line = ln;
				rows[nrows].kind = strdup("method");
				rows[nrows].name = strdup(nm ? nm : "");
				rows[nrows].doc = doc ? strdup(doc) : NULL;
				nrows++;
			}
			sqlite3_finalize(stmt);
		}
	}

	if (re_ok) { regfree(&re);
}

	int shown = 0;
	for (int i = 0; i < nrows; i++) {
		if (i < offset) { continue;
}
		if (limit > 0 && shown >= limit) { break;
}
		fprintf(out, "%s:%ld %s %s", rows[i].path, rows[i].line, rows[i].kind, rows[i].name);
		if (rows[i].doc && rows[i].doc[0]) {
			char *nl = strchr(rows[i].doc, '\n');
			if (nl) { *nl = '\0';
}
			fprintf(out, "  # %s", rows[i].doc);
		}
		fputc('\n', out);
		shown++;
	}

	for (int i = 0; i < nrows; i++) {
		free(rows[i].path); free(rows[i].kind); free(rows[i].name); free(rows[i].doc);
	}
	free(rows);

	sqlite3_close(db);
	free(resolved_db);
	return 0;
}
