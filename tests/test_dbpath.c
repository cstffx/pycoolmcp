#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "dbpath.h"

int main(void)
{
	/* env branch */
	setenv("PYCOOL_DB", "/tmp/opencode/env.db", 1);
	char *p = pycool_resolve_db();
	printf("env=%s\n", p ? p : "(null)");
	free(p);
	unsetenv("PYCOOL_DB");

	/* .pycool/index.db branch */
	char base[] = "/tmp/opencode/dbtestXXXXXX";
	char *dir = mkdtemp(base);
	char pc[4096];
	snprintf(pc, sizeof(pc), "%s/.pycool", dir);
	mkdir(pc, 0755);
	char dbp[4096];
	snprintf(dbp, sizeof(dbp), "%s/.pycool/index.db", dir);
	FILE *f = fopen(dbp, "w"); if (f) { fputc('x', f); fclose(f); }
	chdir(dir);
	p = pycool_resolve_db();
	printf("pycool=%s\n", p ? p : "(null)");
	free(p);

	/* index.db branch: remove .pycool db, create index.db */
	unlink(dbp);
	rmdir(pc);
	snprintf(dbp, sizeof(dbp), "%s/index.db", dir);
	f = fopen(dbp, "w"); if (f) { fputc('x', f); fclose(f); }
	p = pycool_resolve_db();
	printf("indexdb=%s\n", p ? p : "(null)");
	free(p);
	unlink(dbp);

	/* parent traversal: no db in child, db in parent */
	char parent[] = "/tmp/opencode/parentXXXXXX";
	char *pdir = mkdtemp(parent);
	char cdir[4096];
	snprintf(cdir, sizeof(cdir), "%s/child", pdir);
	mkdir(cdir, 0755);
	char pdb[4096];
	snprintf(pdb, sizeof(pdb), "%s/index.db", pdir);
	f = fopen(pdb, "w"); if (f) { fputc('x', f); fclose(f); }
	chdir(cdir);
	p = pycool_resolve_db();
	printf("parent=%s\n", p ? p : "(null)");
	free(p);
	unlink(pdb);
	rmdir(cdir);
	rmdir(pdir);

	/* no db -> NULL (and root break) */
	chdir("/");
	p = pycool_resolve_db();
	printf("none=%s\n", p ? p : "(null)");
	free(p);

	/* getcwd failure */
	char tmpdir[] = "/tmp/opencode/cwdXXXXXX";
	char *td = mkdtemp(tmpdir);
	chdir(td);
	rmdir(td); /* cwd now deleted */
	p = pycool_resolve_db();
	printf("getcwd_fail=%s\n", p ? p : "(null)");
	free(p);

	chdir("/tmp/opencode");
	return 0;
}