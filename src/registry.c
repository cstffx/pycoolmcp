#define _GNU_SOURCE
#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>

static char *state_dir(void)
{
	const char *base = getenv("XDG_STATE_HOME");
	char *dir = NULL;
	int written;
	if (base && base[0]) {
		written = asprintf(&dir, "%s/pycools", base);
	} else {
		const char *home = getenv("HOME");
		if (!home) home = ".";
		written = asprintf(&dir, "%s/.local/state/pycools", home);
	}
	if (written < 0 || !dir || getenv("PYCOOL_REGISTRY_FAIL")) {
		free(dir);
		return NULL;
	}
	mkdir(dir, 0755); /* best effort */
	/* ensure parent exists */
	char tmp[4096];
	snprintf(tmp, sizeof(tmp), "%s", dir);
	/* create parents */
	for (char *p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(tmp, 0755);
			*p = '/';
		}
	}
	mkdir(tmp, 0755);
	return dir;
}

char *registry_dir(void) // cppcheck-suppress unusedFunction
{
	return state_dir();
}

int registry_register(const char *root, const char *dbpath)
{
	char *dir = state_dir();
	if (!dir) return -1;
	char path[4096];
	int n = snprintf(path, sizeof(path), "%s/%d.reg", dir, (int)getpid());
	if (n < 0 || (size_t)n >= sizeof(path)) { free(dir); return -1; }
	FILE *f = fopen(path, "w");
	if (!f) { free(dir); return -1; }
	fprintf(f, "%d\n%s\n%s\n", (int)getpid(), root ? root : "", dbpath ? dbpath : "");
	fclose(f);
	free(dir);
	return 0;
}

int registry_unregister(void)
{
	char *dir = state_dir();
	if (!dir) return -1;
	char path[4096];
	int n = snprintf(path, sizeof(path), "%s/%d.reg", dir, (int)getpid());
	if (n < 0 || (size_t)n >= sizeof(path)) { free(dir); return -1; }
	unlink(path);
	free(dir);
	return 0;
}

static int is_alive(pid_t pid)
{
	if (kill(pid, 0) == 0) return 1;
	return errno != ESRCH ? 1 : 0; /* permission issues count as alive */
}

int registry_list_paths(char ***roots, char ***dbpaths, pid_t **pids, int *count)
{
	*roots = NULL; *dbpaths = NULL; *pids = NULL; *count = 0;
	char *dir = state_dir();
	if (!dir) return -1;
	DIR *d = opendir(dir);
	if (!d) { free(dir); return -1; }
	int cap = 0;
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.') continue;
		size_t L = strlen(ent->d_name);
		if (L < 5 || strcmp(ent->d_name + L - 4, ".reg") != 0) continue;
		char path[4096];
		snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
		FILE *f = fopen(path, "r");
		if (!f) continue;
		int pid;
		char root[4096], dbp[4096];
		if (fscanf(f, "%d\n%4095[^\n]\n%4095[^\n]\n", &pid, root, dbp) < 2) {
			fclose(f);
			continue;
		}
		fclose(f);
		if (!is_alive(pid)) {
			unlink(path);
			continue;
		}
		if (*count == cap) {
			cap = cap ? cap * 2 : 8;
			*roots = realloc(*roots, cap * sizeof(char *));
			*dbpaths = realloc(*dbpaths, cap * sizeof(char *));
			*pids = realloc(*pids, cap * sizeof(pid_t));
		}
		(*roots)[*count] = strdup(root);
		(*dbpaths)[*count] = strdup(dbp);
		(*pids)[*count] = pid;
		(*count)++;
	}
	closedir(d);
	free(dir);
	return 0;
}

int registry_stop(pid_t pid) // cppcheck-suppress unusedFunction
{
	if (kill(pid, SIGTERM) != 0) return -1;
	return 0;
}

int registry_overlap(const char *root)
{
	char *r = realpath(root, NULL);
	if (!r) return 0;
	char **roots, **dbpaths; pid_t *pids; int n;
	if (registry_list_paths(&roots, &dbpaths, &pids, &n) != 0) { free(r); return 0; }
	int overlap = 0;
	for (int i = 0; i < n; i++) {
		char *oroot = realpath(roots[i], NULL);
		if (!oroot) continue;
		size_t lr = strlen(r), lo = strlen(oroot);
		if (strcmp(r, oroot) == 0) overlap = 1;
		else if (lr > lo && strncmp(r, oroot, lo) == 0 && r[lo] == '/') overlap = 1;
		else if (lo > lr && strncmp(oroot, r, lr) == 0 && oroot[lr] == '/') overlap = 1;
		free(oroot);
		if (overlap) break;
	}
	for (int i = 0; i < n; i++) { free(roots[i]); free(dbpaths[i]); }
	free(roots); free(dbpaths); free(pids);
	free(r);
	return overlap;
}

char *registry_default_db(void)
{
	const char *env = getenv("PYCOOL_DB");
	if (env && env[0]) return strdup(env);

	char cwd[4096];
	if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
	if (cwd[0]) {
		char cur[4096];
		strncpy(cur, cwd, sizeof(cur)-1);
		cur[sizeof(cur)-1] = '\0';
		for (;;) {
			char path[4096];
			snprintf(path, sizeof(path), "%s/.pycool/index.db", cur);
			struct stat st;
			if (stat(path, &st) == 0) return strdup(path);
			char *slash = strrchr(cur, '/');
			if (!slash || slash == cur) break;
			*slash = '\0';
		}
	}

	char **roots, **dbpaths; pid_t *pids; int n;
	if (registry_list_paths(&roots, &dbpaths, &pids, &n) == 0 && n > 0 && dbpaths && dbpaths[0]) {
		char *r = strdup(dbpaths[0]);
		for (int i = 0; i < n; i++) { free(roots[i]); free(dbpaths[i]); }
		free(roots); free(dbpaths); free(pids);
		return r;
	}
	if (roots) { for (int i = 0; i < n; i++) { free(roots[i]); free(dbpaths[i]); } free(roots); free(dbpaths); free(pids); }
	return NULL;
}
