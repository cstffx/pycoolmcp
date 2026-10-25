#ifndef PYCOOLS_INDEX_CORE_H
#define PYCOOLS_INDEX_CORE_H

#include <sqlite3.h>

#define PYCOOL_SCHEMA_VERSION 2

int index_has_methods_table(sqlite3 *db);

int index_open(const char *path, sqlite3 **db);
int index_schema_version(sqlite3 *db);
int index_init(const char *path, sqlite3 **db);
int index_remove_file(sqlite3 *db, const char *path);
int index_remove_dir(sqlite3 *db, const char *dir);
void index_close(sqlite3 *db);
int index_upsert_file(sqlite3 *db, const char *path, long mtime, const char *hash, sqlite3_int64 *file_id);
int index_add_symbol(sqlite3 *db, sqlite3_int64 file_id, const char *name, const char *kind, int line, int col, const char *detail, sqlite3_int64 *symbol_id);
int index_add_ref(sqlite3 *db, sqlite3_int64 symbol_id, sqlite3_int64 file_id, int line, int col);
int index_add_type(sqlite3 *db, sqlite3_int64 symbol_id, const char *type_name);
int index_begin(sqlite3 *db);
int index_commit(sqlite3 *db);
int index_rollback(sqlite3 *db);
int index_file_changed(sqlite3 *db, const char *path, long mtime, const char *hash);

#endif /* PYCOOLS_INDEX_CORE_H */