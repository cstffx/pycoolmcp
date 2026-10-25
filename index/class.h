#ifndef PYCOOLS_INDEX_CLASS_H
#define PYCOOLS_INDEX_CLASS_H

#include <sqlite3.h>

int index_add_class_detail(sqlite3 *db, sqlite3_int64 symbol_id, const char *bases, const char *methods, const char *docstring, const char *decorators, int end_line);

#endif /* PYCOOLS_INDEX_CLASS_H */