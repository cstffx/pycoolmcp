#define _GNU_SOURCE
#include "scan.h"
#include "extract.h"
#include "ast.h"
#include "traverse.h"
#include "../index/index.h"
#include "ignore.h"
#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

extern Ignore *pycool_ignore;
extern const char *pycool_root;

static const char *symbol_types[] = { "ClassDef", "FunctionDef", "AsyncFunctionDef", "Assign", "Name", NULL };

static unsigned long fnv1a_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	unsigned long h = 2166136261UL;
	char buf[65536];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		for (size_t i = 0; i < n; i++) {
			h ^= (unsigned char)buf[i];
			h *= 16777619UL;
		}
	}
	fclose(f);
	return h;
}

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0) { fclose(f); return NULL; }
	char *buf = malloc(sz + 1);
	if (!buf) { fclose(f); return NULL; }
	size_t got = fread(buf, 1, sz, f);
	buf[got] = '\0';
	fclose(f);
	return buf;
}

static int is_python_file(const char *path)
{
	size_t n = strlen(path);
	return n >= 3 && strcmp(path + n - 3, ".py") == 0;
}

void reindex_file(const char *path, sqlite3 *db)
{
	if (!is_python_file(path)) {
		/* keep the index limited to Python sources */
		index_remove_file(db, path);
		return;
	}
	if (pycool_root && ignore_is_ignored(pycool_ignore, pycool_root, path)) {
		/* best effort: remove from index */
		index_remove_file(db, path);
		return;
	}
	struct stat st;
	if (stat(path, &st) != 0) {
		/* file gone: drop stale entry */
		index_remove_file(db, path);
		return;
	}
	if (!S_ISREG(st.st_mode)) return;

	char hash[32];
	snprintf(hash, sizeof(hash), "%lu", fnv1a_file(path));

	if (index_file_changed(db, path, st.st_mtime, hash) == 0) return;

	/* remove previous entry (manages its own transaction) */
	index_remove_file(db, path);

	if (index_begin(db) != 0) return;

	sqlite3_int64 file_id;
	if (index_upsert_file(db, path, st.st_mtime, hash, &file_id) != 0) {
		index_rollback(db);
		return;
	}

	char *src = read_file(path);
	if (src) {
		Ast *a = ast_parse(src, path);
		if (a) {
			Ctx ctx = { db, file_id };
			AstSubscriber sub;
			sub.node_types = symbol_types;
			sub.on_node = on_node;
			sub.userdata = &ctx;
			sub.next = NULL;
			ast_traverse(a, NULL, NULL, &sub);
			ast_free(a);
		}
		free(src);
	}
	index_commit(db);
}

void scan_dir(const char *root, sqlite3 *db)
{
	DIR *d = opendir(root);
	if (!d) {
		if (errno != ENOENT) fprintf(stderr, "opendir %s: %s\n", root, strerror(errno));
		return;
	}
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
		size_t plen = strlen(root) + strlen(ent->d_name) + 2;
		char *path = malloc(plen);
		if (!path) continue;
		snprintf(path, plen, "%s/%s", root, ent->d_name);
		if (ignore_is_ignored(pycool_ignore, root, path)) { free(path); continue; }
		struct stat st;
		if (lstat(path, &st) != 0) { free(path); continue; }
		if (S_ISDIR(st.st_mode)) {
			if (strcmp(ent->d_name, "tests") == 0 || strcmp(ent->d_name, "vendor") == 0 ||
			    strcmp(ent->d_name, ".venv") == 0 || strcmp(ent->d_name, "__pycache__") == 0 ||
			    strcmp(ent->d_name, ".git") == 0 || strcmp(ent->d_name, "node_modules") == 0) {
				free(path);
				continue;
			}
			scan_dir(path, db);
			free(path);
		} else if (S_ISREG(st.st_mode)) {
			reindex_file(path, db);
			free(path);
		} else {
			free(path);
		}
	}
	closedir(d);
}

