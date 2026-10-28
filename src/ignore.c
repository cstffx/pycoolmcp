#define _GNU_SOURCE
#include "ignore.h"
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct PatternInfo {
	char pattern[4096];
	int negated;
	int dir_only;
	int anchored;
	int has_slash;
};

static const char *extract_relative_path(const char *root, const char *path);
static int check_is_directory(const char *path);
static int parse_ignore_pattern(const char *raw, struct PatternInfo *info);
static int match_full_path(const char *rel, int is_dir, const struct PatternInfo *info);
static int match_parent_directories(const char *rel, const struct PatternInfo *info);
static int apply_pattern_match(int matched, int negated);

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
	if (!f) { return ig;
}
	char line[4096];
	while (fgets(line, sizeof(line), f)) {
		size_t L = strlen(line);
		while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) { line[--L] = '\0';
}
		const char *p = line;
		while (*p == ' ' || *p == '\t') { p++;
}
		if (*p == '\0' || *p == '#') { continue;
}
		add_pattern(ig, p);
	}
	fclose(f);
	return ig;
}

void ignore_free(Ignore *ig)
{
	if (!ig) { return;
}
	for (int i = 0; i < ig->n; i++) { free(ig->patterns[i]);
}
	free(ig->patterns);
	free(ig);
}

static int pattern_negated(const char *raw)
{
	return raw && raw[0] == '!';
}

static const char *extract_relative_path(const char *root, const char *path)
{
	if (!root) { return path;
	}
	size_t rl = strlen(root);
	if (strncmp(path, root, rl) != 0) { return path;
	}
	if (path[rl] != '/' && path[rl] != '\0') { return path;
	}
	const char *rel = path + rl;
	while (*rel == '/') { rel++;
	}
	return rel;
}

static int check_is_directory(const char *path)
{
	struct stat st;
	return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static int parse_ignore_pattern(const char *raw, struct PatternInfo *info)
{
	strncpy(info->pattern, raw, sizeof(info->pattern) - 1);
	info->pattern[sizeof(info->pattern) - 1] = '\0';

	info->negated = pattern_negated(info->pattern);
	if (info->negated) { memmove(info->pattern, info->pattern + 1, strlen(info->pattern) + 1);
	}

	info->dir_only = 0;
	size_t pl = strlen(info->pattern);
	if (pl > 0 && info->pattern[pl - 1] == '/') {
		info->dir_only = 1;
		info->pattern[pl - 1] = '\0';
		pl--;
	}
	if (pl == 0) { return 0;
	}

	info->anchored = 0;
	if (info->pattern[0] == '/') {
		info->anchored = 1;
		memmove(info->pattern, info->pattern + 1, strlen(info->pattern) + 1);
		pl--;
	}
	if (pl == 0) { return 0;
	}

	info->has_slash = (strchr(info->pattern, '/') != NULL);
	return 1;
}

static int match_full_path(const char *rel, int is_dir, const struct PatternInfo *info)
{
	if (info->dir_only && !is_dir) { return 0;
	}
	if (info->anchored && !info->has_slash) {
		if (strchr(rel, '/') == NULL && fnmatch(info->pattern, rel, 0) == 0) { return 1;
		}
	} else if (info->has_slash) {
		if (fnmatch(info->pattern, rel, FNM_PATHNAME) == 0) { return 1;
		}
	} else {
		const char *base = strrchr(rel, '/');
		base = base ? base + 1 : rel;
		if (fnmatch(info->pattern, base, 0) == 0) { return 1;
		}
	}
	return 0;
}

static int match_parent_directories(const char *rel, const struct PatternInfo *info)
{
	if (info->anchored) { return 0;
	}
	char tmp[4096];
	strncpy(tmp, rel, sizeof(tmp) - 1);
	tmp[sizeof(tmp) - 1] = '\0';
	char *slash;
	while ((slash = strrchr(tmp, '/')) != NULL) {
		*slash = '\0';
		if (tmp[0] == '\0') { break;
		}
		if (info->has_slash) {
			if (fnmatch(info->pattern, tmp, FNM_PATHNAME) == 0) { return 1;
			}
		} else {
			const char *base = strrchr(tmp, '/');
			base = base ? base + 1 : tmp;
			if (fnmatch(info->pattern, base, 0) == 0) { return 1;
			}
		}
	}
	return 0;
}

static int apply_pattern_match(int matched, int negated)
{
	if (matched) {
		return negated ? 0 : 1;
	}
	return -1;
}

int ignore_is_ignored(const Ignore *ig, const char *root, const char *path)
{
	if (!ig || ig->n == 0 || !path) { return 0;
	}
	const char *rel = extract_relative_path(root, path);
	if (*rel == '\0') { return 0;
	}

	int is_dir = check_is_directory(path);
	int ignored = 0;

	for (int i = 0; i < ig->n; i++) {
		struct PatternInfo info = { 0 };
		if (!parse_ignore_pattern(ig->patterns[i], &info)) { continue;
		}

		int matched = match_full_path(rel, is_dir, &info);
		if (!matched) { matched = match_parent_directories(rel, &info);
		}

		int result = apply_pattern_match(matched, info.negated);
		if (result >= 0) { ignored = result;
		}
	}
	return ignored;
}