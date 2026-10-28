#define _GNU_SOURCE
#include "class.h"
#include "method.h"
#include "../index/index.h"
#include <stdlib.h>
#include <string.h>

static void extract_class_attributes(PyObject *node, PyObject *ast_mod,
				      char **bases, char **docstring, char **decorators)
{
	*bases = extract_bases(node, ast_mod);
	*docstring = extract_docstring(node, ast_mod);
	*decorators = extract_decorators(node, ast_mod);
}

static const char *get_item_type(PyObject *item)
{
	PyObject *item_cls = PyObject_GetAttrString(item, "__class__");
	PyObject *item_name_obj = item_cls ? PyObject_GetAttrString(item_cls, "__name__") : NULL;
	const char *item_type = item_name_obj ? PyUnicode_AsUTF8(item_name_obj) : NULL;

	Py_XDECREF(item_name_obj);
	Py_XDECREF(item_cls);
	return item_type;
}

static void process_function_item(PyObject *item, int lineno, PyObject *ast_mod,
				   Ctx *ctx, sqlite3_int64 sid,
				   char **methods, size_t *cap, size_t *len)
{
	PyObject *mname = PyObject_GetAttrString(item, "name");
	const char *method_name = mname ? PyUnicode_AsUTF8(mname) : NULL;
	if (method_name && sid > 0) {
		extract_method_in_class(item, method_name, lineno, ast_mod, ctx,
					 sid, methods, cap, len);
	}
	Py_XDECREF(mname);
}

static void process_assign_item(PyObject *item, int lineno, PyObject *ast_mod,
				 Ctx *ctx, sqlite3_int64 sid,
				 char **methods, size_t *cap, size_t *len)
{
	extract_assigned_method_in_class(item, lineno, ast_mod, ctx, sid,
					  methods, cap, len);
}

static void process_body_item(PyObject *item, const char *item_type, int lineno,
			       PyObject *ast_mod, Ctx *ctx, sqlite3_int64 sid,
			       char **methods, size_t *cap, size_t *len)
{
	if (!item_type) {
		return;
	}

	if (strcmp(item_type, "FunctionDef") == 0 ||
	    strcmp(item_type, "AsyncFunctionDef") == 0) {
		process_function_item(item, lineno, ast_mod, ctx, sid, methods, cap, len);
	} else if (strcmp(item_type, "Assign") == 0) {
		process_assign_item(item, lineno, ast_mod, ctx, sid, methods, cap, len);
	}
}

static char *extract_class_body(PyObject *node, int lineno, PyObject *ast_mod,
				 Ctx *ctx, sqlite3_int64 sid)
{
	PyObject *body = PyObject_GetAttrString(node, "body");
	if (!body || !PyList_Check(body)) {
		Py_XDECREF(body);
		return NULL;
	}

	size_t cap = 256;
	size_t len = 0;
	char *methods = malloc(cap);
	if (!methods) {
		Py_XDECREF(body);
		return NULL;
	}
	methods[0] = '\0';

	Py_ssize_t nb = PyList_Size(body);
	for (Py_ssize_t bi = 0; bi < nb; bi++) {
		PyObject *item = PyList_GetItem(body, bi);
		const char *item_type = get_item_type(item);
		process_body_item(item, item_type, lineno, ast_mod, ctx, sid,
				   &methods, &cap, &len);
	}

	methods[len] = '\0';
	Py_XDECREF(body);
	return methods;
}

void extract_class(const char *class_name, int lineno, PyObject *node, Ctx *ctx)
{
	if (!class_name || !node) {
		return;
	}

	char *bases = NULL;
	char *methods = NULL;
	char *docstring = NULL;
	char *decorators = NULL;
	sqlite3_int64 sid = 0;

	index_add_symbol(ctx->db, ctx->file_id, class_name, "class", lineno, 0, NULL, &sid);

	PyObject *ast_mod = get_ast_module();
	if (ast_mod) {
		extract_class_attributes(node, ast_mod, &bases, &docstring, &decorators);
		methods = extract_class_body(node, lineno, ast_mod, ctx, sid);
		release_ast_module(ast_mod);
	}

	int end_line = get_int_attr(node, "end_lineno", 0);
	index_add_class_detail(ctx->db, sid, bases, methods, docstring, decorators, end_line);

	free(bases);
	free(methods);
	free(docstring);
	free(decorators);
}
