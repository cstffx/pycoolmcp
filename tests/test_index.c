#include <stdio.h>
#include "../index/index.h"

int main(void)
{
	sqlite3 *db;
	if (index_open("/tmp/opencode/test.db", &db) != 0) { fprintf(stderr, "open fail\n"); return 1; }
	sqlite3_int64 fid, sid;
	index_upsert_file(db, "a.py", 123, "hash", &fid);
	index_add_symbol(db, fid, "Foo", "class", 1, 0, NULL, &sid);
	index_add_type(db, sid, "object");
	printf("file=%ld symbol=%ld\n", (long)fid, (long)sid);
	index_close(db);
	return 0;
}
