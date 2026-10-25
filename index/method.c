#include "method.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

int index_add_method(sqlite3 *db, sqlite3_int64 class_symbol_id, const char *name, int line, int end_line, int is_async, const char *decorators, const char *returns, const char *docstring, sqlite3_int64 *method_id)
{
	const char *sql = "INSERT INTO methods(class_symbol_id,name,line,end_line,is_async,decorators,returns,docstring) VALUES(?,?,?,?,?,?,?,?) RETURNING id;";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
	sqlite3_bind_int64(stmt, 1, class_symbol_id);
	sqlite3_bind_text(stmt, 2, name ? name : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, line);
	sqlite3_bind_int(stmt, 4, end_line);
	sqlite3_bind_int(stmt, 5, is_async);
	sqlite3_bind_text(stmt, 6, decorators ? decorators : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 7, returns ? returns : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 8, docstring ? docstring : "", -1, SQLITE_TRANSIENT);
	int rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW && method_id) *method_id = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	return (rc == SQLITE_ROW) ? 0 : -1;
}
int index_add_method_arg(sqlite3 *db, sqlite3_int64 method_id, const char *name, const char *annotation, const char *default_value, const char *kind, int position)
{
	const char *sql = "INSERT INTO method_args(method_id,name,annotation,default_value,kind,position) VALUES(?,?,?,?,?,?);";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
	sqlite3_bind_int64(stmt, 1, method_id);
	sqlite3_bind_text(stmt, 2, name ? name : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, annotation ? annotation : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, default_value ? default_value : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, kind ? kind : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 6, position);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return (rc == SQLITE_DONE) ? 0 : -1;
}
