#ifndef PYCOOLS_INDEX_METHOD_H
#define PYCOOLS_INDEX_METHOD_H

#include <sqlite3.h>

int index_add_method(sqlite3 *db, sqlite3_int64 class_symbol_id, const char *name, int line, int end_line, int is_async, const char *decorators, const char *returns, const char *docstring, sqlite3_int64 *method_id);
int index_add_method_arg(sqlite3 *db, sqlite3_int64 method_id, const char *name, const char *annotation, const char *default_value, const char *kind, int position);

#endif /* PYCOOLS_INDEX_METHOD_H */