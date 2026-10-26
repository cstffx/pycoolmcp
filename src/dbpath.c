#define _GNU_SOURCE
#include "dbpath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

char *pycool_resolve_db(void)
{
	const char *env = getenv("PYCOOL_DB");
	if (env && env[0]) return strdup(env);

	char cur[4096];
	if (!getcwd(cur, sizeof(cur))) return NULL;

	for (;;) {
		char path[8192];
		snprintf(path, sizeof(path), "%s/.pycool/index.db", cur);
		struct stat st;
		if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) return strdup(path);

		snprintf(path, sizeof(path), "%s/index.db", cur);
		if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) return strdup(path);

		char *slash = strrchr(cur, '/');
		if (!slash || slash == cur) break;
		*slash = '\0';
	}
	return NULL;
}