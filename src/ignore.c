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
		size_t L = strlen(line);
		while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
		const char *p = line;
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

static int pattern_negated(const char *raw)
{
	return raw && raw[0] == '!';
}

int ignore_is_ignored(const Ignore *ig, const char *root, const char *path)
{
	if (!ig || ig->n == 0 || !path) return 0;
	const char *rel = path;
	size_t rl = strlen(root ? root : "");
	if (root && strncmp(path, root, rl) == 0 && (path[rl] == '/' || path[rl] == '\0')) {
		rel = path + rl;
		while (*rel == '/') rel++;
	}
	if (*rel == '\0') return 0;

	struct stat st;
	int is_dir = (stat(path, &st) == 0 && S_ISDIR(st.st_mode));

	int ignored = 0;
	for (int i = 0; i < ig->n; i++) {
		char pat[4096];
		strncpy(pat, ig->patterns[i], sizeof(pat)-1);
		pat[sizeof(pat)-1] = '\0';

		int negated = pattern_negated(pat);
		if (negated) memmove(pat, pat + 1, strlen(pat) + 1);

		int dir_only = 0;
		size_t pl = strlen(pat);
		if (pl > 0 && pat[pl-1] == '/') { dir_only = 1; pat[pl-1] = '\0'; pl--; }
		if (pl == 0) continue;

		int anchored = 0;
		if (pat[0] == '/') { anchored = 1; memmove(pat, pat + 1, strlen(pat) + 1); pl--; }
		if (pl == 0) continue;

		int has_slash = (strchr(pat, '/') != NULL);

		char target[4096];
		strncpy(target, rel, sizeof(target)-1);
		target[sizeof(target)-1] = '\0';

		int matched = 0;
		/* full path */
		if (!(dir_only && !is_dir)) {
			if (anchored && !has_slash) {
				/* root-level only */
				if (strchr(target, '/') == NULL && fnmatch(pat, target, 0) == 0) matched = 1;
			} else if (has_slash) {
				if (fnmatch(pat, target, FNM_PATHNAME) == 0) matched = 1;
			} else {
				const char *base = strrchr(target, '/');
				base = base ? base + 1 : target;
				if (fnmatch(pat, base, 0) == 0) matched = 1;
			}
		}
		/* parent directories (skip if anchored) */
		if (!matched && !anchored) {
			char tmp[4096];
			strncpy(tmp, target, sizeof(tmp)-1);
			tmp[sizeof(tmp)-1] = '\0';
			char *slash;
			while ((slash = strrchr(tmp, '/')) != NULL) {
				*slash = '\0';
				if (tmp[0] == '\0') break;
				if (has_slash) {
					if (fnmatch(pat, tmp, FNM_PATHNAME) == 0) { matched = 1; break; }
				} else {
					const char *base = strrchr(tmp, '/');
					base = base ? base + 1 : tmp;
					if (fnmatch(pat, base, 0) == 0) { matched = 1; break; }
				}
			}
		}

		if (matched) {
			if (negated) ignored = 0;
			else ignored = 1;
		}
	}
	return ignored;
}