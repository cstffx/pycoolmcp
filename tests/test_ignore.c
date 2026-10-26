#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "ignore.h"

static void md(const char *p) { mkdir(p, 0755); }

int main(void)
{
	int _r = system("rm -rf /tmp/opencode/ti2"); (void)_r;
	md("/tmp/opencode/ti2");
	md("/tmp/opencode/ti2/build");
	md("/tmp/opencode/ti2/sub");
	FILE *f = fopen("/tmp/opencode/ti2/.pycoolignore", "w");
	fputs("*.pyc\nbuild/\n/rootonly.py\n!important.pyc\n", f);
	fclose(f);

	const char *root = "/tmp/opencode/ti2";
	Ignore *ig = ignore_load(root);

	int ok = 1;
#define CHK(path, exp) do { \
	int got = ignore_is_ignored(ig, root, path); \
	if (got != (exp)) { printf("FAIL: %s expected %d got %d\n", path, (exp), got); ok = 0; } \
} while (0)

	CHK("/tmp/opencode/ti2/foo.pyc", 1);
	CHK("/tmp/opencode/ti2/build", 1);
	CHK("/tmp/opencode/ti2/build/x.py", 1);
	CHK("/tmp/opencode/ti2/rootonly.py", 1);
	CHK("/tmp/opencode/ti2/sub/rootonly.py", 0);
	CHK("/tmp/opencode/ti2/keep.py", 0);
	CHK("/tmp/opencode/ti2/important.pyc", 0);

	ignore_free(ig);
	printf(ok ? "PASS\n" : "FAIL\n");
	return ok ? 0 : 1;
}