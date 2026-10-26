#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sqlite3.h>
#include "index.h"
#include "../pycoolctl/class.h"

int main(void)
{
	const char *dbpath = "/tmp/opencode/class.db";
	unlink(dbpath);
	FILE *sf = fopen("/tmp/opencode/a.py", "w");
	for (int i = 1; i <= 10; i++) fprintf(sf, "line %d\n", i);
	fclose(sf);

	sqlite3 *db;
	index_open(dbpath, &db);
	sqlite3_int64 fid = 0, sid = 0;
	index_upsert_file(db, "/tmp/opencode/a.py", 1, "h", &fid);
	index_add_symbol(db, fid, "Foo", "class", 1, 0, NULL, &sid);
	/* special chars in class detail */
	index_add_class_detail(db, sid, "a\"b\\c\nd\re\tf\x01", "m", "d\nd", "de\nc", 10);
	sqlite3_int64 mid = 0;
	index_add_symbol(db, fid, "bar", "function", 2, 0, NULL, &mid);
	index_add_method(db, sid, "bar", 2, 3, 0, "dec", NULL, "doc", NULL);
	index_add_ref(db, sid, fid, 5, 0);
	index_close(db);

	FILE *out = fopen("/dev/null", "w");
	char *dbarg = (char*)dbpath;

	/* usage */
	{ char *a[] = {"pycoolctl", "class"}; class_command(2, a, out); }

	/* no db, no registry */
	{
		setenv("XDG_STATE_HOME", "/tmp/opencode/empty_state", 1);
		mkdir("/tmp/opencode/empty_state", 0755);
		char *a[] = {"pycoolctl", "class", "Foo"}; class_command(3, a, out);
		unsetenv("XDG_STATE_HOME");
	}

	/* pid not found */
	{
		setenv("XDG_STATE_HOME", "/tmp/opencode/cstate", 1);
		mkdir("/tmp/opencode/cstate", 0755);
		char rdir[4096]; snprintf(rdir, sizeof(rdir), "/tmp/opencode/cstate/pycools"); mkdir(rdir, 0755);
		char regpath[4096]; snprintf(regpath, sizeof(regpath), "/tmp/opencode/cstate/pycools/%d.reg", (int)getpid());
		FILE *rf = fopen(regpath, "w");
		fprintf(rf, "%d\n/tmp\n%s\n", (int)getpid(), dbpath); fclose(rf);
		char *a[] = {"pycoolctl", "class", "Foo", "--pid", "999999"}; class_command(5, a, out);
		unsetenv("XDG_STATE_HOME");
	}

	/* pid found */
	{
		setenv("XDG_STATE_HOME", "/tmp/opencode/cstate", 1);
		char pidbuf[32]; snprintf(pidbuf, sizeof(pidbuf), "%d", (int)getpid());
		char *a[] = {"pycoolctl", "class", "Foo", "--pid", pidbuf}; class_command(5, a, out);
		unsetenv("XDG_STATE_HOME");
	}

	/* registry resolution without pid */
	{
		setenv("XDG_STATE_HOME", "/tmp/opencode/cstate", 1);
		char *a[] = {"pycoolctl", "class", "Foo"}; class_command(3, a, out);
		unsetenv("XDG_STATE_HOME");
	}

	/* db open failure */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--db", "/proc/cannot/open.db"}; class_command(5, a, out); }

	/* schema mismatch */
	{
		char *mdb = "/tmp/opencode/mismatch.db";
		unlink(mdb);
		sqlite3 *mdbdb; index_open(mdb, &mdbdb);
		sqlite3_exec(mdbdb, "INSERT OR REPLACE INTO meta(key,value) VALUES('schema_version','0');", NULL, NULL, NULL);
		index_close(mdbdb);
		char *a[] = {"pycoolctl", "class", "Foo", "--db", mdb}; class_command(5, a, out);
	}

	/* sv==-1: empty sqlite db (no tables) */
	{
		char *edb = "/tmp/opencode/empty.db";
		unlink(edb);
		sqlite3 *ed; sqlite3_open(edb, &ed); sqlite3_close(ed);
		char *a[] = {"pycoolctl", "class", "Foo", "--db", edb}; class_command(5, a, out);
	}

	/* refs mode usage (no name) */
	{ char *a[] = {"pycoolctl", "class", "--refs", "--db", dbarg}; class_command(5, a, out); }

	/* refs mode with name */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--refs", "--db", dbarg}; class_command(6, a, out); }

	/* refs mode with file */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--refs", "--file", "/tmp/opencode/a.py", "--db", dbarg}; class_command(8, a, out); }

	/* refs mode error (empty db) */
	{ char *edb = "/tmp/opencode/empty.db"; char *a[] = {"pycoolctl", "class", "Foo", "--refs", "--db", edb}; class_command(6, a, out); }

	/* body mode without method */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--db", dbarg, "--body"}; class_command(6, a, out); }

	/* body mode with method found */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--db", dbarg, "--body", "--method", "bar"}; class_command(8, a, out); }

	/* body mode with method not found */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--db", dbarg, "--body", "--method", "nope"}; class_command(8, a, out); }

	/* body mode with method + file */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--db", dbarg, "--body", "--method", "bar", "--file", "/tmp/opencode/a.py"}; class_command(10, a, out); }

	/* json mode */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--db", dbarg, "--json"}; class_command(6, a, out); }

	/* json mode no rows */
	{ char *a[] = {"pycoolctl", "class", "Nope", "--db", dbarg, "--json"}; class_command(6, a, out); }

	/* compact mode */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--db", dbarg, "--compact"}; class_command(6, a, out); }

	/* full mode */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--db", dbarg, "--full"}; class_command(6, a, out); }

	/* info all letters including H and unknown */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--db", dbarg, "--info", "ABCDEFGH IJKLMNZ"}; class_command(7, a, out); }

	/* info specific */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--db", dbarg, "--info", "A"}; class_command(6, a, out); }

	/* file filter */
	{ char *a[] = {"pycoolctl", "class", "Foo", "--db", dbarg, "--file", "/tmp/opencode/a.py"}; class_command(7, a, out); }

	/* no rows */
	{ char *a[] = {"pycoolctl", "class", "Nope", "--db", dbarg}; class_command(5, a, out); }

	/* list all classes (no name) */
	{ char *a[] = {"pycoolctl", "class", "--db", dbarg}; class_command(4, a, out); }

	/* list all classes with file filter */
	{ char *a[] = {"pycoolctl", "class", "--db", dbarg, "--file", "/tmp/opencode/a.py"}; class_command(6, a, out); }

	/* query error main sql (empty db) */
	{ char *edb = "/tmp/opencode/empty.db"; char *a[] = {"pycoolctl", "class", "Foo", "--db", edb}; class_command(5, a, out); }

	fclose(out);
	return 0;
}
