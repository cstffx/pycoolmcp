#define _GNU_SOURCE
#include "assign.h"
#include "../index/index.h"
#include <stdlib.h>
#include <string.h>

// cppcheck-suppress staticFunction
void extract_assign_targets(PyObject *target, Ctx *ctx, int line)
{
	if (!target || !ctx) { return;
}
	PyObject *cls = PyObject_GetAttrString(target, "__class__");
	PyObject *cname = cls ? PyObject_GetAttrString(cls, "__name__") : NULL;
	const char *tname = cname ? PyUnicode_AsUTF8(cname) : NULL;

	if (tname && strcmp(tname, "Name") == 0) {
		PyObject *id = PyObject_GetAttrString(target, "id");
		if (id) {
			const char *s = PyUnicode_AsUTF8(id);
			if (s) {
				sqlite3_int64 sid;
				index_add_symbol(ctx->db, ctx->file_id, s, "variable", line, 0, NULL, &sid);
			}
			Py_DECREF(id);
		}
	} else if (tname && (strcmp(tname, "Tuple") == 0 || strcmp(tname, "List") == 0)) {
		PyObject *elts = PyObject_GetAttrString(target, "elts");
		if (elts && PyList_Check(elts)) {
			Py_ssize_t n = PyList_Size(elts);
			for (Py_ssize_t i = 0; i < n; i++) {
				extract_assign_targets(PyList_GetItem(elts, i), ctx, line);
			}
		}
		Py_XDECREF(elts);
	}

	Py_XDECREF(cname);
	Py_XDECREF(cls);
	PyErr_Clear();
}

void extract_assign(PyObject *node, int lineno, Ctx *ctx)
{
	if (!node || !ctx) { return;
}
	PyObject *targets = PyObject_GetAttrString(node, "targets");
	if (targets && PyList_Check(targets)) {
		Py_ssize_t n = PyList_Size(targets);
		for (Py_ssize_t i = 0; i < n; i++) {
			extract_assign_targets(PyList_GetItem(targets, i), ctx, lineno);
		}
	}
	Py_XDECREF(targets);
	PyErr_Clear();
}
