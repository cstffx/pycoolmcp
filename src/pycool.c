#define _GNU_SOURCE
#define _GNU_SOURCE
#include "pycool.h"
#include "extract.h"
#include "scan.h"
#include "ast.h"
#include "traverse.h"
#include "../index/index.h"
#include "observer.h"
#include "registry.h"
#include "ignore.h"
#include <signal.h>
#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

Ignore *pycool_ignore = NULL;
const char *pycool_root = NULL;
static volatile sig_atomic_t pycool_running = 1;
static volatile sig_atomic_t pycool_clean_requested = 0;
static volatile sig_atomic_t pycool_reindex_requested = 0;



static void on_event(const char *path, uint32_t mask, void *userdata)
{
	sqlite3 *db = userdata;
	if (pycool_root && ignore_is_ignored(pycool_ignore, pycool_root, path)) {
		if (!(mask & (IN_DELETE | IN_MOVED_FROM))) index_remove_file(db, path);
		return;
	}
	struct stat st;
	if (mask & (IN_DELETE | IN_MOVED_FROM)) {
		if (mask & IN_ISDIR) index_remove_dir(db, path);
		else index_remove_file(db, path);
		return;
	}
	if (stat(path, &st) != 0) return;
	if (S_ISDIR(st.st_mode)) {
		scan_dir(path, db);
	} else if (S_ISREG(st.st_mode)) {
		reindex_file(path, db);
	}
}

static void handle_signal(int sig)
{
	if (sig == SIGUSR1) {
		pycool_clean_requested = 1;
	} else if (sig == SIGUSR2) {
		pycool_reindex_requested = 1;
	} else {
		pycool_running = 0;
	}
	observer_stop();
}

static void clean_index(sqlite3 *db)
{
	index_begin(db);
	sqlite3_exec(db, "DELETE FROM method_args;", NULL, NULL, NULL);
	sqlite3_exec(db, "DELETE FROM methods;", NULL, NULL, NULL);
	sqlite3_exec(db, "DELETE FROM class_details;", NULL, NULL, NULL);
	sqlite3_exec(db, "DELETE FROM types;", NULL, NULL, NULL);
	sqlite3_exec(db, "DELETE FROM refs;", NULL, NULL, NULL);
	sqlite3_exec(db, "DELETE FROM symbols;", NULL, NULL, NULL);
	sqlite3_exec(db, "DELETE FROM files;", NULL, NULL, NULL);
	index_commit(db);
}

int pycool_run(const char *root, const char *dbpath)
{
	char *root_abs = realpath(root, NULL);
	if (!root_abs) root_abs = strdup(root);
	if (registry_overlap(root_abs)) {
		fprintf(stderr, "error: %s is already monitored (or overlaps a monitored directory)\n", root_abs);
		free(root_abs);
		return -1;
	}
	signal(SIGTERM, handle_signal);
	signal(SIGINT, handle_signal);
	signal(SIGUSR1, handle_signal);
	signal(SIGUSR2, handle_signal);

	sqlite3 *db;
	if (index_init(dbpath, &db) != 0) {
		fprintf(stderr, "cannot open db %s\n", dbpath);
		free(root_abs);
		return -1;
	}
	pycool_root = root_abs;
	pycool_ignore = ignore_load(root_abs);
	registry_register(root_abs, dbpath);
	scan_dir(root_abs, db);
	while (pycool_running) {
		observer_watch(root_abs, on_event, db);
		if (pycool_clean_requested) {
			clean_index(db);
			pycool_clean_requested = 0;
		}
		if (pycool_reindex_requested) {
			clean_index(db);
			scan_dir(root_abs, db);
			pycool_reindex_requested = 0;
		}
	}
	index_close(db);
	registry_unregister();
	ignore_free(pycool_ignore);
	pycool_ignore = NULL;
	pycool_root = NULL;
	free(root_abs);
	return 0;
}
