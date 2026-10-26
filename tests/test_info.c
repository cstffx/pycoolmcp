#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "info.h"
#include "registry.h"
#include "index.h"

int main(void)
{
	char state[] = "/tmp/opencode/stateXXXXXX";
	char *sd = mkdtemp(state);
	setenv("XDG_STATE_HOME", sd, 1);

	const char *dbp = "/tmp/opencode/info.db";
	unlink(dbp);
	sqlite3 *db;
	index_open(dbp, &db);
	sqlite3_int64 fid, sid;
	index_upsert_file(db, "a.py", 1, "h", &fid);
	index_add_symbol(db, fid, "C", "class", 1, 0, NULL, &sid);
	index_add_symbol(db, fid, "f", "function", 2, 0, NULL, &sid);
	index_add_symbol(db, fid, "v", "variable", 3, 0, NULL, &sid);
	index_close(db);

	registry_register("/tmp", dbp);

	char *argv0[] = {"pycoolctl", "info"};
	printf("usage=%d\n", info_command(2, argv0, stdout));

	char *argv1[] = {"pycoolctl", "info", "all"};
	printf("all=%d\n", info_command(3, argv1, stdout));

	char pidbuf[32];
	snprintf(pidbuf, sizeof(pidbuf), "%d", (int)getpid());
	char *argv2[] = {"pycoolctl", "info", pidbuf};
	printf("pid=%d\n", info_command(3, argv2, stdout));

	char *argv3[] = {"pycoolctl", "info", dbp};
	printf("path=%d\n", info_command(3, argv3, stdout));

	char *argv4[] = {"pycoolctl", "info", "/proc/cannot/open.db"};
	printf("bad=%d\n", info_command(3, argv4, stdout));

	registry_unregister();
	unsetenv("XDG_STATE_HOME");
	unlink(dbp);
	return 0;
}