#define _GNU_SOURCE
#include "class.h"
#include "method.h"
#include "../index/index.h"
#include <stdlib.h>
#include <string.h>

void extract_class(const char *class_name, int lineno, PyObject *node, Ctx *ctx)
{
	if (!class_name || !node) { return;
}
	char *bases = NULL;
	char *methods = NULL;
	char *docstring = NULL;
	char *decorators = NULL;
	sqlite3_int64 sid = 0;

	index_add_symbol(ctx->db, ctx->file_id, class_name, "class", lineno, 0, NULL, &sid);

	PyObject *ast_mod = get_ast_module();
	if (ast_mod) {
		/* Extract class attributes */
		bases = extract_bases(node, ast_mod);
		docstring = extract_docstring(node, ast_mod);
		decorators = extract_decorators(node, ast_mod);

		/* Extract class body (methods and variables) */
		PyObject *body = PyObject_GetAttrString(node, "body");
		if (body && PyList_Check(body)) {
			size_t cap = 256;
			size_t len = 0;
			methods = malloc(cap);
			if (methods) {
				methods[0] = '\0';

			Py_ssize_t nb = PyList_Size(body);
			for (Py_ssize_t bi = 0; bi < nb; bi++) {
				PyObject *item = PyList_GetItem(body, bi);
				PyObject *item_cls = PyObject_GetAttrString(item, "__class__");
				PyObject *item_name_obj = item_cls ? PyObject_GetAttrString(item_cls, "__name__") : NULL;
				const char *item_type = item_name_obj ? PyUnicode_AsUTF8(item_name_obj) : NULL;

				if (item_type && (strcmp(item_type, "FunctionDef") == 0 ||
						  strcmp(item_type, "AsyncFunctionDef") == 0)) {
					PyObject *mname = PyObject_GetAttrString(item, "name");
					const char *method_name = mname ? PyUnicode_AsUTF8(mname) : NULL;
					if (method_name && sid > 0) {
						extract_method_in_class(item, method_name, lineno, ast_mod, ctx,
									 sid, &methods, &cap, &len);
					}
					Py_XDECREF(mname);
				} else if (item_type && strcmp(item_type, "Assign") == 0) {
					extract_assigned_method_in_class(item, lineno, ast_mod, ctx, sid,
									  &methods, &cap, &len);
				}

				Py_XDECREF(item_name_obj);
				Py_XDECREF(item_cls);
			}

				if (methods) { methods[len] = '\0';
}
			}
			Py_XDECREF(body);
		}

		release_ast_module(ast_mod);
	}

	int end_line = get_int_attr(node, "end_lineno", 0);
	index_add_class_detail(ctx->db, sid, bases, methods, docstring, decorators, end_line);

	free(bases);
	free(methods);
	free(docstring);
	free(decorators);
}
