#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include "scan.h"
#include "pycool.h"
#include "ignore.h"
#include "../index/index.h"

extern const char *pycool_root;
extern Ignore *pycool_ignore;

int count_symbols(sqlite3 *db)
{
	sqlite3_stmt *st;
	int n = 0;
	if (sqlite3_prepare_v2(db, "SELECT count(*) FROM symbols;", -1, &st, NULL) == SQLITE_OK) {
		if (sqlite3_step(st) == SQLITE_ROW) n = (int)sqlite3_column_int64(st, 0);
		sqlite3_finalize(st);
	}
	return n;
}

int has_symbol(sqlite3 *db, const char *name)
{
	sqlite3_stmt *st;
	int found = 0;
	if (sqlite3_prepare_v2(db, "SELECT 1 FROM symbols WHERE name=?;", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_text(st, 1, name, -1, SQLITE_TRANSIENT);
		if (sqlite3_step(st) == SQLITE_ROW) found = 1;
		sqlite3_finalize(st);
	}
	return found;
}

int main(void)
{
	int _r = system("rm -rf /tmp/opencode/tinc3 && mkdir -p /tmp/opencode/tinc3"); (void)_r;
	const char *root = "/tmp/opencode/tinc3";
	FILE *f = fopen("/tmp/opencode/tinc3/a.py", "w");
	fputs("class A:\n    pass\ndef f():\n    x = 1\n", f);
	fclose(f);

	pycool_root = root;
	pycool_ignore = ignore_load(root);

	remove("/tmp/opencode/tinc3.db");
	sqlite3 *db;
	if (index_init("/tmp/opencode/tinc3.db", &db) != 0) { printf("db open fail\n"); return 1; }

	scan_dir(root, db);
	int ok = 1;
	if (count_symbols(db) != 3) { printf("FAIL: initial count %d\n", count_symbols(db)); ok = 0; }

	/* unchanged reindex should not duplicate */
	reindex_file("/tmp/opencode/tinc3/a.py", db);
	if (count_symbols(db) != 3) { printf("FAIL: after unchanged count %d\n", count_symbols(db)); ok = 0; }

	/* modify file */
	f = fopen("/tmp/opencode/tinc3/a.py", "w");
	fputs("class C:\n    pass\n", f);
	fclose(f);
	reindex_file("/tmp/opencode/tinc3/a.py", db);
	if (!has_symbol(db, "C")) { printf("FAIL: C missing\n"); ok = 0; }
	if (has_symbol(db, "A")) { printf("FAIL: stale A remains\n"); ok = 0; }
	if (has_symbol(db, "f")) { printf("FAIL: stale f remains\n"); ok = 0; }

	index_close(db);
	ignore_free(pycool_ignore);
	printf(ok ? "PASS\n" : "FAIL\n");
	return ok ? 0 : 1;
}