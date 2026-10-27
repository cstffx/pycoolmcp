#define _GNU_SOURCE
#include "extract.h"
#include "class.h"
#include "assign.h"
#include "method.h"
#include "../index/index.h"
#include <Python.h>
#include <string.h>

void on_node(const char *type, int lineno, const char *name, void *node_ptr, void *ud)
{
	Ctx *ctx = ud;
	PyObject *node = node_ptr;

	if (!type || !ctx) { return;
}

	if (strcmp(type, "ClassDef") == 0) {
		extract_class(name, lineno, node, ctx);
	} else if (strcmp(type, "FunctionDef") == 0 || strcmp(type, "AsyncFunctionDef") == 0) {
		extract_function(name, lineno, node, ctx);
	} else if (strcmp(type, "Assign") == 0) {
		extract_assign(node, lineno, ctx);
	} else if (strcmp(type, "Name") == 0 && name) {
		const char *sql = "SELECT id FROM symbols WHERE name=?1 AND kind='class';";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				sqlite3_int64 ref_sid = sqlite3_column_int64(stmt, 0);
				index_add_ref(ctx->db, ref_sid, ctx->file_id, lineno, 0);
			}
			sqlite3_finalize(stmt);
		}
	}

	PyErr_Clear();
}
