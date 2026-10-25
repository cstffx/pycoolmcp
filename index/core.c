#include "core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA =
"CREATE TABLE IF NOT EXISTS files ("
"  id INTEGER PRIMARY KEY,"
"  path TEXT UNIQUE NOT NULL,"
"  mtime INTEGER,"
"  hash TEXT"
");"
"CREATE TABLE IF NOT EXISTS symbols ("
"  id INTEGER PRIMARY KEY,"
"  file_id INTEGER NOT NULL,"
"  name TEXT NOT NULL,"
"  kind TEXT NOT NULL,"
"  line INTEGER,"
"  col INTEGER,"
"  detail TEXT,"
"  FOREIGN KEY(file_id) REFERENCES files(id)"
");"
"CREATE TABLE IF NOT EXISTS refs ("
"  id INTEGER PRIMARY KEY,"
"  symbol_id INTEGER NOT NULL,"
"  file_id INTEGER NOT NULL,"
"  line INTEGER,"
"  col INTEGER,"
"  FOREIGN KEY(symbol_id) REFERENCES symbols(id),"
"  FOREIGN KEY(file_id) REFERENCES files(id)"
");"
"CREATE TABLE IF NOT EXISTS types ("
"  id INTEGER PRIMARY KEY,"
"  symbol_id INTEGER NOT NULL,"
"  type_name TEXT NOT NULL,"
"  FOREIGN KEY(symbol_id) REFERENCES symbols(id)"
");"
"CREATE TABLE IF NOT EXISTS class_details ("
"  symbol_id INTEGER PRIMARY KEY,"
"  bases TEXT,"
"  methods TEXT,"
"  docstring TEXT,"
"  decorators TEXT,"
"  end_line INTEGER,"
"  FOREIGN KEY(symbol_id) REFERENCES symbols(id)"
");"
"CREATE TABLE IF NOT EXISTS methods ("
"  id INTEGER PRIMARY KEY,"
"  class_symbol_id INTEGER NOT NULL,"
"  name TEXT NOT NULL,"
"  line INTEGER,"
"  end_line INTEGER,"
"  is_async INTEGER,"
"  decorators TEXT,"
"  returns TEXT,"
"  docstring TEXT,"
"  FOREIGN KEY(class_symbol_id) REFERENCES symbols(id)"
");"
"CREATE TABLE IF NOT EXISTS method_args ("
"  id INTEGER PRIMARY KEY,"
"  method_id INTEGER NOT NULL,"
"  name TEXT,"
"  annotation TEXT,"
"  default_value TEXT,"
"  kind TEXT,"
"  position INTEGER,"
"  FOREIGN KEY(method_id) REFERENCES methods(id)"
");"
"CREATE INDEX IF NOT EXISTS idx_symbols_name ON symbols(name);"
"CREATE INDEX IF NOT EXISTS idx_symbols_file ON symbols(file_id);"
"CREATE INDEX IF NOT EXISTS idx_refs_symbol ON refs(symbol_id);"
"CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT);";

int index_open(const char *path, sqlite3 **db)
{
	if (sqlite3_open(path, db) != SQLITE_OK) return -1;
	sqlite3_busy_timeout(*db, 5000);
	char *err = NULL;
	if (sqlite3_exec(*db, SCHEMA, NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "sqlite error: %s\n", err);
		sqlite3_free(err);
		sqlite3_close(*db);
		*db = NULL;
		return -1;
	}
	sqlite3_exec(*db, "ALTER TABLE symbols ADD COLUMN detail TEXT;", NULL, NULL, NULL);
	sqlite3_exec(*db, "ALTER TABLE class_details ADD COLUMN end_line INTEGER;", NULL, NULL, NULL);
	return 0;
}

void index_close(sqlite3 *db)
{
	if (db) sqlite3_close(db);
}

int index_upsert_file(sqlite3 *db, const char *path, long mtime, const char *hash, sqlite3_int64 *file_id)
{
	const char *sql = "INSERT INTO files(path,mtime,hash) VALUES(?,?,?) "
	                  "ON CONFLICT(path) DO UPDATE SET mtime=excluded.mtime, hash=excluded.hash "
	                  "RETURNING id;";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, mtime);
	sqlite3_bind_text(stmt, 3, hash ? hash : "", -1, SQLITE_TRANSIENT);
	int rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		if (file_id) *file_id = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return (rc == SQLITE_ROW) ? 0 : -1;
}

int index_add_symbol(sqlite3 *db, sqlite3_int64 file_id, const char *name, const char *kind, int line, int col, const char *detail, sqlite3_int64 *symbol_id)
{
	const char *sql = "INSERT INTO symbols(file_id,name,kind,line,col,detail) VALUES(?,?,?,?,?,?) RETURNING id;";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
	sqlite3_bind_int64(stmt, 1, file_id);
	sqlite3_bind_text(stmt, 2, name, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, kind, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 4, line);
	sqlite3_bind_int(stmt, 5, col);
	if (detail) sqlite3_bind_text(stmt, 6, detail, -1, SQLITE_TRANSIENT);
	else sqlite3_bind_null(stmt, 6);
	int rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW && symbol_id) *symbol_id = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	return (rc == SQLITE_ROW) ? 0 : -1;
}

int index_add_ref(sqlite3 *db, sqlite3_int64 symbol_id, sqlite3_int64 file_id, int line, int col)
{
	const char *sql = "INSERT INTO refs(symbol_id,file_id,line,col) VALUES(?,?,?,?);";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
	sqlite3_bind_int64(stmt, 1, symbol_id);
	sqlite3_bind_int64(stmt, 2, file_id);
	sqlite3_bind_int(stmt, 3, line);
	sqlite3_bind_int(stmt, 4, col);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return (rc == SQLITE_DONE) ? 0 : -1;
}

int index_add_type(sqlite3 *db, sqlite3_int64 symbol_id, const char *type_name)
{
	const char *sql = "INSERT INTO types(symbol_id,type_name) VALUES(?,?);";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
	sqlite3_bind_int64(stmt, 1, symbol_id);
	sqlite3_bind_text(stmt, 2, type_name, -1, SQLITE_TRANSIENT);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return (rc == SQLITE_DONE) ? 0 : -1;
}

int index_init(const char *path, sqlite3 **db)
{
	return index_open(path, db);
}

int index_remove_file(sqlite3 *db, const char *path)
{
	if (index_begin(db) != 0) return -1;
	sqlite3_int64 file_id = 0;
	{
		const char *sql = "SELECT id FROM files WHERE path=?;";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { index_rollback(db); return -1; }
		sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmt) == SQLITE_ROW) file_id = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
	}
	if (file_id == 0) { index_commit(db); return 0; } /* not present */
	/* delete types for symbols of this file */
	{
		const char *sql = "DELETE FROM types WHERE symbol_id IN (SELECT id FROM symbols WHERE file_id=?);";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { index_rollback(db); return -1; }
		sqlite3_bind_int64(stmt, 1, file_id);
		if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); index_rollback(db); return -1; }
		sqlite3_finalize(stmt);
	}
	/* delete refs referencing this file or its symbols */
	{
		const char *sql = "DELETE FROM refs WHERE file_id=? OR symbol_id IN (SELECT id FROM symbols WHERE file_id=?);";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { index_rollback(db); return -1; }
		sqlite3_bind_int64(stmt, 1, file_id);
		sqlite3_bind_int64(stmt, 2, file_id);
		if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); index_rollback(db); return -1; }
		sqlite3_finalize(stmt);
	}
	/* delete method args and methods for classes of this file */
	{
		const char *sql = "DELETE FROM method_args WHERE method_id IN (SELECT id FROM methods WHERE class_symbol_id IN (SELECT id FROM symbols WHERE file_id=?));";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(stmt, 1, file_id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}
	{
		const char *sql = "DELETE FROM methods WHERE class_symbol_id IN (SELECT id FROM symbols WHERE file_id=?);";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(stmt, 1, file_id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}
	/* delete symbols */
	{
		const char *sql = "DELETE FROM symbols WHERE file_id=?;";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { index_rollback(db); return -1; }
		sqlite3_bind_int64(stmt, 1, file_id);
		if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); index_rollback(db); return -1; }
		sqlite3_finalize(stmt);
	}
	/* delete file */
	{
		const char *sql = "DELETE FROM files WHERE id=?;";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { index_rollback(db); return -1; }
		sqlite3_bind_int64(stmt, 1, file_id);
		if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); index_rollback(db); return -1; }
		sqlite3_finalize(stmt);
	}
	return index_commit(db);
}

int index_remove_dir(sqlite3 *db, const char *dir)
{
	size_t plen = strlen(dir) + 1;
	char prefix[4096];
	if (plen + 1 > sizeof(prefix)) return -1;
	snprintf(prefix, sizeof(prefix), "%s/", dir);

	if (index_begin(db) != 0) return -1;

	/* types for symbols of affected files */
	const char *q1 = "DELETE FROM types WHERE symbol_id IN ("
	                 "  SELECT id FROM symbols WHERE file_id IN ("
	                 "    SELECT id FROM files WHERE substr(path,1,?) = ?"
	                 "  )"
	                 ");";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, q1, -1, &stmt, NULL) != SQLITE_OK) { index_rollback(db); return -1; }
	sqlite3_bind_int(stmt, 1, (int)plen);
	sqlite3_bind_text(stmt, 2, prefix, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); index_rollback(db); return -1; }
	sqlite3_finalize(stmt);

	/* refs */
	const char *q2 = "DELETE FROM refs WHERE file_id IN (SELECT id FROM files WHERE substr(path,1,?) = ?) "
	                 "OR symbol_id IN (SELECT id FROM symbols WHERE file_id IN (SELECT id FROM files WHERE substr(path,1,?) = ?));";
	if (sqlite3_prepare_v2(db, q2, -1, &stmt, NULL) != SQLITE_OK) { index_rollback(db); return -1; }
	sqlite3_bind_int(stmt, 1, (int)plen);
	sqlite3_bind_text(stmt, 2, prefix, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, (int)plen);
	sqlite3_bind_text(stmt, 4, prefix, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); index_rollback(db); return -1; }
	sqlite3_finalize(stmt);

	/* method args and methods */
	{
		const char *qm1 = "DELETE FROM method_args WHERE method_id IN (SELECT id FROM methods WHERE class_symbol_id IN (SELECT id FROM symbols WHERE file_id IN (SELECT id FROM files WHERE substr(path,1,?) = ?)));";
		sqlite3_stmt *st;
		if (sqlite3_prepare_v2(db, qm1, -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_int(st, 1, (int)plen);
			sqlite3_bind_text(st, 2, prefix, -1, SQLITE_TRANSIENT);
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
	}
	{
		const char *qm2 = "DELETE FROM methods WHERE class_symbol_id IN (SELECT id FROM symbols WHERE file_id IN (SELECT id FROM files WHERE substr(path,1,?) = ?));";
		sqlite3_stmt *st;
		if (sqlite3_prepare_v2(db, qm2, -1, &st, NULL) == SQLITE_OK) {
			sqlite3_bind_int(st, 1, (int)plen);
			sqlite3_bind_text(st, 2, prefix, -1, SQLITE_TRANSIENT);
			sqlite3_step(st);
			sqlite3_finalize(st);
		}
	}
	/* symbols */
	const char *q3 = "DELETE FROM symbols WHERE file_id IN (SELECT id FROM files WHERE substr(path,1,?) = ?);";
	if (sqlite3_prepare_v2(db, q3, -1, &stmt, NULL) != SQLITE_OK) { index_rollback(db); return -1; }
	sqlite3_bind_int(stmt, 1, (int)plen);
	sqlite3_bind_text(stmt, 2, prefix, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); index_rollback(db); return -1; }
	sqlite3_finalize(stmt);

	/* files */
	const char *q4 = "DELETE FROM files WHERE substr(path,1,?) = ?;";
	if (sqlite3_prepare_v2(db, q4, -1, &stmt, NULL) != SQLITE_OK) { index_rollback(db); return -1; }
	sqlite3_bind_int(stmt, 1, (int)plen);
	sqlite3_bind_text(stmt, 2, prefix, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); index_rollback(db); return -1; }
	sqlite3_finalize(stmt);

	return index_commit(db);
}



int index_begin(sqlite3 *db)
{
	return sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) == SQLITE_OK ? 0 : -1;
}

int index_commit(sqlite3 *db)
{
	return sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL) == SQLITE_OK ? 0 : -1;
}

int index_rollback(sqlite3 *db)
{
	return sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL) == SQLITE_OK ? 0 : -1;
}

int index_file_changed(sqlite3 *db, const char *path, long mtime, const char *hash)
{
	const char *sql = "SELECT mtime, hash FROM files WHERE path=?;";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
	sqlite3_bind_text(stmt, 1, path, -1, SQLITE_TRANSIENT);
	int rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		long old_mtime = sqlite3_column_int64(stmt, 0);
		const unsigned char *old_hash = sqlite3_column_text(stmt, 1);
		int changed = (old_mtime != mtime) ||
		              (hash && old_hash && strcmp((const char*)old_hash, hash) != 0) ||
		              (hash && !old_hash);
		sqlite3_finalize(stmt);
		return changed ? 1 : 0;
	}
	sqlite3_finalize(stmt);
	if (rc == SQLITE_DONE) return 1; /* new file */
	return -1;
}




