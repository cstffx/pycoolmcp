#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "ignore.h"

static void md(const char *p) { mkdir(p, 0755); }

int main(void)
{
	int _r = system("rm -rf /tmp/opencode/ti2"); (void)_r;
	md("/tmp/opencode/ti2");
	md("/tmp/opencode/ti2/build");
	md("/tmp/opencode/ti2/sub");
	FILE *f = fopen("/tmp/opencode/ti2/.pycoolignore", "w");
	fputs("*.pyc\nbuild/\n/rootonly.py\n!important.pyc\n# comment\n  spaced  \n\n", f);
	fclose(f);

	const char *root = "/tmp/opencode/ti2";
	Ignore *ig = ignore_load(root);

	/* null ig */
	printf("null_ignored=%d\n", ignore_is_ignored(NULL, root, "/tmp/opencode/ti2/foo.pyc"));

	/* n==0 ig */
	Ignore *ig0 = ignore_load("/tmp/opencode/noignore");
	printf("empty_ignored=%d\n", ignore_is_ignored(ig0, "/tmp/opencode/noignore", "/tmp/opencode/noignore/x.py"));
	ignore_free(ig0);

#define CHK(path, exp) do { \
	int got = ignore_is_ignored(ig, root, path); \
	printf("%s=%d (exp %d)\n", path, got, (exp)); \
} while (0)

	CHK("/tmp/opencode/ti2/foo.pyc", 1);
	CHK("/tmp/opencode/ti2/build", 1);
	CHK("/tmp/opencode/ti2/build/x.py", 0);
	CHK("/tmp/opencode/ti2/rootonly.py", 1);
	CHK("/tmp/opencode/ti2/sub/rootonly.py", 0);
	CHK("/tmp/opencode/ti2/keep.py", 0);
	CHK("/tmp/opencode/ti2/important.pyc", 0);
	CHK("/tmp/opencode/ti2/spaced", 1);
	/* pattern with slash */
	/* add slash pattern manually via file? test load already; test relative path */
	CHK("/tmp/opencode/ti2/sub/file.pyc", 1);

	ignore_free(ig);
	ignore_free(NULL);

	/* load with no file */
	Ignore *ig2 = ignore_load("/tmp/opencode/ti2none");
	printf("nofile_n=%d\n", ig2 ? 0 : 1); /* placeholder */
	ignore_free(ig2);

	return 0;
}
