#define _GNU_SOURCE
#include "ignore.h"
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct Ignore {
	char **patterns;
	int n, cap;
};

static void add_pattern(Ignore *ig, const char *pat)
{
	if (ig->n == ig->cap) {
		ig->cap = ig->cap ? ig->cap * 2 : 16;
		ig->patterns = realloc(ig->patterns, ig->cap * sizeof(char *));
	}
	ig->patterns[ig->n++] = strdup(pat);
}

Ignore *ignore_load(const char *root)
{
	Ignore *ig = calloc(1, sizeof(Ignore));
	char path[4096];
	snprintf(path, sizeof(path), "%s/.pycoolignore", root);
	FILE *f = fopen(path, "r");
	if (!f) return ig;
	char line[4096];
	while (fgets(line, sizeof(line), f)) {
		/* strip newline */
		size_t L = strlen(line);
		while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
		/* trim leading spaces */
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '\0' || *p == '#') continue;
		add_pattern(ig, p);
	}
	fclose(f);
	return ig;
}

void ignore_free(Ignore *ig)
{
	if (!ig) return;
	for (int i = 0; i < ig->n; i++) free(ig->patterns[i]);
	free(ig->patterns);
	free(ig);
}

int ignore_is_ignored(Ignore *ig, const char *root, const char *path)
{
	if (!ig || ig->n == 0) return 0;
	const char *rel = path;
	size_t rl = strlen(root);
	if (strncmp(path, root, rl) == 0 && (path[rl] == '/' || path[rl] == '\0')) {
		rel = path + rl;
		while (*rel == '/') rel++;
	}
	if (*rel == '\0') return 0;

	struct stat st;
	int is_dir = (stat(path, &st) == 0 && S_ISDIR(st.st_mode));

	for (int i = 0; i < ig->n; i++) {
		char pat[4096];
		strncpy(pat, ig->patterns[i], sizeof(pat)-1);
		pat[sizeof(pat)-1] = '\0';
		int dir_only = 0;
		size_t pl = strlen(pat);
		if (pl > 0 && pat[pl-1] == '/') { dir_only = 1; pat[pl-1] = '\0'; pl--; }
		if (dir_only && !is_dir) continue;
		/* use pat (without trailing slash) */
		if (strchr(pat, '/') != NULL) {
			if (fnmatch(pat, rel, FNM_PATHNAME) == 0) return 1;
		} else {
			const char *base = strrchr(path, '/');
			base = base ? base + 1 : path;
			if (fnmatch(pat, base, 0) == 0) return 1;
		}
	}
	return 0;
}
