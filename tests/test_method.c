#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sqlite3.h>
#include "index.h"
#include "../pycoolctl/method.h"

int main(void)
{
	const char *dbpath = "/tmp/opencode/method.db";
	unlink(dbpath);
	FILE *sf = fopen("/tmp/opencode/m.py", "w");
	for (int i = 1; i <= 10; i++) fprintf(sf, "line %d\n", i);
	fclose(sf);

	sqlite3 *db;
	index_open(dbpath, &db);
	sqlite3_int64 fid = 0, sid = 0, mid = 0;
	index_upsert_file(db, "/tmp/opencode/m.py", 1, "h", &fid);
	index_add_symbol(db, fid, "Foo", "class", 1, 0, NULL, &sid);
	index_add_class_detail(db, sid, "Base", "bar", "doc", "dec", 10);
	index_add_symbol(db, fid, "bar", "function", 2, 0, NULL, &mid);
	index_add_method(db, sid, "bar", 2, 3, 0, "dec", "a\"\\c\td\r\ne", "doc", &mid);
	/* args */
	index_add_method_arg(db, mid, "x", "int", "1", "positional", 0);
	index_add_method_arg(db, mid, "y", NULL, NULL, "varargs", -1);
	index_add_method_arg(db, mid, "z", "str", NULL, "kwargs", -1);
	index_add_method_arg(db, mid, "w", "int", "2", "keyword-only", 1);
	index_close(db);

	FILE *out = fopen("/dev/null", "w");
	char *dbarg = (char*)dbpath;

	/* usage */
	{ char *a[] = {"pycoolctl", "method"}; method_command(2, a, out); }

	/* no db, no registry */
	{
		setenv("XDG_STATE_HOME", "/tmp/opencode/empty_state", 1);
		mkdir("/tmp/opencode/empty_state", 0755);
		char *a[] = {"pycoolctl", "method", "Foo"}; method_command(3, a, out);
		unsetenv("XDG_STATE_HOME");
	}

	/* pid not found */
	{
		setenv("XDG_STATE_HOME", "/tmp/opencode/mstate", 1);
		mkdir("/tmp/opencode/mstate", 0755);
		char rdir[4096]; snprintf(rdir, sizeof(rdir), "/tmp/opencode/mstate/pycools"); mkdir(rdir, 0755);
		char regpath[4096]; snprintf(regpath, sizeof(regpath), "/tmp/opencode/mstate/pycools/%d.reg", (int)getpid());
		FILE *rf = fopen(regpath, "w");
		fprintf(rf, "%d\n/tmp\n%s\n", (int)getpid(), dbpath); fclose(rf);
		char *a[] = {"pycoolctl", "method", "Foo", "--pid", "999999"}; method_command(5, a, out);
		unsetenv("XDG_STATE_HOME");
	}

	/* pid found */
	{
		setenv("XDG_STATE_HOME", "/tmp/opencode/mstate", 1);
		char pidbuf[32]; snprintf(pidbuf, sizeof(pidbuf), "%d", (int)getpid());
		char *a[] = {"pycoolctl", "method", "Foo", "--pid", pidbuf}; method_command(5, a, out);
		unsetenv("XDG_STATE_HOME");
	}

	/* registry resolution without pid */
	{
		setenv("XDG_STATE_HOME", "/tmp/opencode/mstate", 1);
		char *a[] = {"pycoolctl", "method", "Foo"}; method_command(3, a, out);
		unsetenv("XDG_STATE_HOME");
	}

	/* db open failure */
	{ char *a[] = {"pycoolctl", "method", "Foo", "--db", "/proc/cannot/open.db"}; method_command(5, a, out); }

	/* schema mismatch */
	{
		char *mdb = "/tmp/opencode/mm.db";
		unlink(mdb);
		sqlite3 *mdbdb; index_open(mdb, &mdbdb);
		sqlite3_exec(mdbdb, "INSERT OR REPLACE INTO meta(key,value) VALUES('schema_version','0');", NULL, NULL, NULL);
		index_close(mdbdb);
		char *a[] = {"pycoolctl", "method", "Foo", "--db", mdb}; method_command(5, a, out);
	}

	/* sv==-1 empty db */
	{
		char *edb = "/tmp/opencode/emptym.db";
		unlink(edb);
		sqlite3 *ed; sqlite3_open(edb, &ed); if (ed) sqlite3_close(ed);
		char *a[] = {"pycoolctl", "method", "Foo", "--db", edb}; method_command(5, a, out);
	}

	/* has_methods_table false: db without methods table */
	{
		char *hdb = "/tmp/opencode/hasm.db";
		unlink(hdb);
		sqlite3 *hd; index_open(hdb, &hd);
		/* drop methods table */
		sqlite3_exec(hd, "DROP TABLE methods;", NULL, NULL, NULL);
		index_close(hd);
		char *a[] = {"pycoolctl", "method", "Foo", "--db", hdb}; method_command(5, a, out);
	}

	/* build_params error (empty db) handled by empty db above */

	/* list all methods (no method name) */
	{ char *a[] = {"pycoolctl", "method", "Foo", "--db", dbarg}; method_command(4, a, out); }

	/* specific method */
	{ char *a[] = {"pycoolctl", "method", "Foo", "bar", "--db", dbarg}; method_command(6, a, out); }

	/* method not found */
	{ char *a[] = {"pycoolctl", "method", "Foo", "nope", "--db", dbarg}; method_command(6, a, out); }

	/* json mode */
	{ char *a[] = {"pycoolctl", "method", "Foo", "--db", dbarg, "--json"}; method_command(6, a, out); }

	/* json mode no rows */
	{ char *a[] = {"pycoolctl", "method", "Nope", "--db", dbarg, "--json"}; method_command(6, a, out); }

	/* body mode */
	{ char *a[] = {"pycoolctl", "method", "Foo", "bar", "--db", dbarg, "--body"}; method_command(7, a, out); }

	/* file filter */
	{ char *a[] = {"pycoolctl", "method", "Foo", "--db", dbarg, "--file", "/tmp/opencode/m.py"}; method_command(7, a, out); }

	/* limit/offset */
	{ char *a[] = {"pycoolctl", "method", "Foo", "--db", dbarg, "--limit", "1", "--offset", "0"}; method_command(8, a, out); }
	{ char *a[] = {"pycoolctl", "method", "Foo", "--db", dbarg, "--offset", "1"}; method_command(7, a, out); }

	/* print_json_str escapes: data with special chars */
	/* already in DB? no; add via separate db not needed */

	fclose(out);
	return 0;
}
