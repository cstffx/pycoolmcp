#include "class.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

int index_add_class_detail(sqlite3 *db, sqlite3_int64 symbol_id, const char *bases, const char *methods, const char *docstring, const char *decorators, int end_line)
{
	const char *sql = "INSERT OR REPLACE INTO class_details(symbol_id,bases,methods,docstring,decorators,end_line) VALUES(?,?,?,?,?,?);";
	sqlite3_stmt *stmt;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
	sqlite3_bind_int64(stmt, 1, symbol_id);
	sqlite3_bind_text(stmt, 2, bases ? bases : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, methods ? methods : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, docstring ? docstring : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, decorators ? decorators : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 6, end_line);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return (rc == SQLITE_DONE) ? 0 : -1;
}
