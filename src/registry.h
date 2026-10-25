#ifndef PYCOOLS_REGISTRY_H
#define PYCOOLS_REGISTRY_H

#include <stddef.h>
#include <sys/types.h>

char *registry_dir(void);
int registry_register(const char *root, const char *dbpath);
int registry_unregister(void);
int registry_list_paths(char ***roots, char ***dbpaths, pid_t **pids, int *count);
int registry_stop(pid_t pid);

/* Return 1 if root overlaps an already running instance. */
int registry_overlap(const char *root);

/* Resolve a default db path: PYCOOL_DB, .pycool/index.db walking up from cwd, or first running instance. Caller frees. */
char *registry_default_db(void);

#endif /* PYCOOLS_REGISTRY_H */
