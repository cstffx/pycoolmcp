#define _GNU_SOURCE
#include "common.h"
#include "../index/index.h"
#include <stdlib.h>
#include <string.h>

PyObject *get_ast_module(void)
{
	return PyImport_ImportModule("ast");
}

void release_ast_module(PyObject *ast_mod)
{
	if (ast_mod) { Py_DECREF(ast_mod);
}
}

char *get_unparse_str(PyObject *obj, PyObject *unparse_fn)
{
	if (!obj || !unparse_fn) { return NULL;
}
	PyObject *result = PyObject_CallFunction(unparse_fn, "(O)", obj);
	if (!result) { return NULL;
}
	const char *str = PyUnicode_AsUTF8(result);
	char *copy = str ? strdup(str) : NULL;
	Py_DECREF(result);
	return copy;
}

int get_int_attr(PyObject *obj, const char *attr_name, int default_val)
{
	if (!obj) { return default_val;
}
	PyObject *attr = PyObject_GetAttrString(obj, attr_name);
	if (!attr) { PyErr_Clear(); return default_val;
}
	int val = (int)PyLong_AsLong(attr);
	if (PyErr_Occurred()) { PyErr_Clear(); val = default_val;
}
	Py_DECREF(attr);
	return val;
}

static char *build_string_list(PyObject *items, const char *sep, PyObject *unparse_fn)
{
	if (!items || !PyList_Check(items)) { return NULL;
}
	size_t cap = 256;
	size_t len = 0;
	char *result = malloc(cap);
	if (!result) { return NULL;
}
	result[0] = '\0';

	Py_ssize_t n = PyList_Size(items);
	for (Py_ssize_t i = 0; i < n; i++) {
		PyObject *item = PyList_GetItem(items, i);
		char *item_str = get_unparse_str(item, unparse_fn);
		if (!item_str) {
			PyObject *str_obj = PyObject_Str(item);
			if (str_obj) {
				const char *s = PyUnicode_AsUTF8(str_obj);
				if (s) { item_str = strdup(s); }
				Py_DECREF(str_obj);
			}
		}
		if (!item_str) { continue;
}

		size_t add_len = strlen(item_str) + 1;
		if (len + add_len >= cap) {
			cap = (len + add_len) * 2;
			char *tmp = realloc(result, cap);
			if (!tmp) { free(result); free(item_str); return NULL; }
			result = tmp;
		}

		if (len > 0) { result[len++] = *sep;
}
		strcpy(result + len, item_str);
		len += strlen(item_str);
		free(item_str);
	}

	result[len] = '\0';
	return result;
}

char *extract_bases(PyObject *node, PyObject *ast_mod)
{
	if (!node || !ast_mod) { return NULL;
}
	PyObject *bases_obj = PyObject_GetAttrString(node, "bases");
	if (!bases_obj) { PyErr_Clear(); return NULL;
}
	PyObject *unparse = PyObject_GetAttrString(ast_mod, "unparse");
	char *result = build_string_list(bases_obj, ",", unparse);
	Py_XDECREF(unparse);
	Py_XDECREF(bases_obj);
	return result;
}

char *extract_decorators(PyObject *node, PyObject *ast_mod)
{
	if (!node || !ast_mod) { return NULL;
}
	PyObject *decs = PyObject_GetAttrString(node, "decorator_list");
	if (!decs || !PyList_Check(decs)) { Py_XDECREF(decs); PyErr_Clear(); return NULL;
}
	PyObject *unparse = PyObject_GetAttrString(ast_mod, "unparse");

	size_t cap = 256;
	size_t len = 0;
	char *result = malloc(cap);
	if (!result) { Py_XDECREF(unparse); Py_XDECREF(decs); return NULL;
}
	result[0] = '\0';

	Py_ssize_t nd = PyList_Size(decs);
	for (Py_ssize_t i = 0; i < nd; i++) {
		PyObject *dec = PyList_GetItem(decs, i);
		char *dec_str = get_unparse_str(dec, unparse);
		if (!dec_str) { continue;
}

		size_t add = strlen(dec_str) + 2;
		if (len + add >= cap) {
			cap = (len + add) * 2;
			char *tmp = realloc(result, cap);
			if (!tmp) { free(result); free(dec_str); break;
}
			result = tmp;
		}

		if (len > 0) { result[len++] = ',';
}
		strcpy(result + len, dec_str);
		len += strlen(dec_str);
		free(dec_str);
	}

	result[len] = '\0';
	Py_XDECREF(unparse);
	Py_XDECREF(decs);
	return result;
}

char *extract_docstring(PyObject *node, PyObject *ast_mod)
{
	if (!node || !ast_mod) { return NULL;
}
	PyObject *gds = PyObject_GetAttrString(ast_mod, "get_docstring");
	if (!gds) { PyErr_Clear(); return NULL;
}
	PyObject *ds = PyObject_CallFunction(gds, "(O)", node);
	char *result = NULL;
	if (ds && ds != Py_None) {
		const char *dstr = PyUnicode_AsUTF8(ds);
		if (dstr) { result = strdup(dstr);
}
		Py_DECREF(ds);
	}
	Py_DECREF(gds);
	return result;
}
