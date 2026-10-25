#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "registry.h"

static void md(const char *p) { mkdir(p, 0755); }

int main(void)
{
	int _r = system("rm -rf /tmp/opencode/test_overlap_state /tmp/opencode/to"); (void)_r;
	md("/tmp/opencode");
	md("/tmp/opencode/to");
	md("/tmp/opencode/to/parent");
	md("/tmp/opencode/to/parent/child");
	md("/tmp/opencode/to/sibling");

	setenv("XDG_STATE_HOME", "/tmp/opencode/test_overlap_state", 1);
	md("/tmp/opencode/test_overlap_state");

	const char *root = "/tmp/opencode/to/parent";
	int ok = 1;
	if (registry_register(root, "/tmp/opencode/to/a.db") != 0) { printf("register fail\n"); return 1; }

	if (!registry_overlap(root)) { printf("FAIL: same not overlap\n"); ok = 0; }
	if (!registry_overlap("/tmp/opencode/to/parent/child")) { printf("FAIL: child not overlap\n"); ok = 0; }
	if (!registry_overlap("/tmp/opencode/to")) { printf("FAIL: parent not overlap\n"); ok = 0; }
	if (registry_overlap("/tmp/opencode/to/sibling")) { printf("FAIL: sibling overlap\n"); ok = 0; }

	registry_unregister();
	if (registry_overlap(root)) { printf("FAIL: still overlap after unregister\n"); ok = 0; }

	printf(ok ? "PASS\n" : "FAIL\n");
	return ok ? 0 : 1;
}