#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../index/index.h"

int main(void)
{
	const char *dbpath = "/tmp/opencode/test.db";
	unlink(dbpath);
	sqlite3 *db;
	if (index_open(dbpath, &db) != 0) { fprintf(stderr, "open fail\n"); return 1; }

	sqlite3 *db2 = NULL;
	if (index_init("/tmp/opencode/test_init.db", &db2) == 0) { index_close(db2); }
	unlink("/tmp/opencode/test_init.db");

	sqlite3_int64 fid = 0, sid = 0, fid2 = 0;
	index_upsert_file(db, "a.py", 123, "hash", &fid);
	index_upsert_file(db, "a.py", 124, "hash2", NULL); /* file_id NULL branch */
	index_upsert_file(db, "dir/b.py", 456, "hashb", &fid2);
	index_add_symbol(db, fid, "Foo", "class", 1, 0, NULL, &sid);
	index_add_symbol(db, fid, "Bar", "class", 2, 0, "detail", NULL);
	index_add_ref(db, sid, fid, 2, 0);
	index_add_type(db, sid, "object");

	printf("changed_same=%d\n", index_file_changed(db, "a.py", 124, "hash2"));
	printf("changed_mtime=%d\n", index_file_changed(db, "a.py", 999, "hash2"));
	printf("changed_hash=%d\n", index_file_changed(db, "a.py", 124, "other"));
	printf("changed_new=%d\n", index_file_changed(db, "zzz.py", 1, "h"));

	printf("version=%d\n", index_schema_version(db));
	printf("has_methods=%d\n", index_has_methods_table(db));

	/* remove nonexistent file */
	index_remove_file(db, "nope.py");
	/* remove existing file */
	index_remove_file(db, "a.py");

	/* remove dir */
	sqlite3_int64 fid3 = 0;
	index_upsert_file(db, "dir/c.py", 1, "h", &fid3);
	index_remove_dir(db, "dir");

	/* class/method details */
	index_add_class_detail(db, sid, "object", "m", "doc", "dec", 10);
	index_add_method(db, sid, "foo", 1, 2, 0, "dec", "int", "doc", &sid);
	index_add_method_arg(db, sid, "x", "int", "1", "positional", 0);

	/* error-ish paths with NULL db */
	index_add_class_detail(NULL, 1, "b", "m", "d", "dec", 1);
	index_add_method(NULL, 1, "foo", 1, 2, 0, "dec", "int", "doc", &sid);
	index_add_method_arg(NULL, 1, "x", "int", "1", "positional", 0);

	/* transactions */
	index_begin(db);
	index_rollback(db);
	index_begin(db);
	index_commit(db);

	/* error-ish paths with NULL db */
	printf("open_bad=%d\n", index_open("/proc/cannot/create.db", &db2));

	/* schema creation failure on garbage file */
	{
		FILE *f = fopen("/tmp/opencode/garbage.db", "w");
		if (f) { fputs("not a database", f); fclose(f); }
		sqlite3 *gdb = NULL;
		printf("open_garbage=%d\n", index_open("/tmp/opencode/garbage.db", &gdb));
		unlink("/tmp/opencode/garbage.db");
	}

	/* meta insert failure on read-only db */
	{
		sqlite3 *rdb = NULL;
		if (index_open("/tmp/opencode/readonly.db", &rdb) == 0) {
			index_close(rdb);
		}
		chmod("/tmp/opencode/readonly.db", 0444);
		sqlite3 *rdb2 = NULL;
		printf("open_readonly=%d\n", index_open("/tmp/opencode/readonly.db", &rdb2));
		if (rdb2) index_close(rdb2);
		chmod("/tmp/opencode/readonly.db", 0644);
		unlink("/tmp/opencode/readonly.db");
	}

	/* interrupt to force step error in index_file_changed */
	sqlite3_interrupt(db);
	printf("file_changed_interrupted=%d\n", index_file_changed(db, "a.py", 1, "h"));

	printf("remove_file_null=%d\n", index_remove_file(NULL, "x"));
	printf("remove_dir_null=%d\n", index_remove_dir(NULL, "x"));
	printf("file_changed_null=%d\n", index_file_changed(NULL, "x", 1, "h"));
	printf("schema_version_null=%d\n", index_schema_version(NULL));
	printf("has_methods_null=%d\n", index_has_methods_table(NULL));

	index_close(db);
	index_close(NULL);
	return 0;
}