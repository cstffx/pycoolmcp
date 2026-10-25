#ifndef PYCOOLS_SCAN_H
#define PYCOOLS_SCAN_H

#include <sqlite3.h>
#include "ignore.h"

extern Ignore *pycool_ignore; /* defined in pycool.c */
extern const char *pycool_root;

void reindex_file(const char *path, sqlite3 *db);
void scan_dir(const char *root, sqlite3 *db);

#endif /* PYCOOLS_SCAN_H */