#include "ast.h"
#include <stdlib.h>
#include <string.h>

Ast *ast_parse(const char *source, const char *filename)
{
	if (!Py_IsInitialized()) {
		Py_Initialize();
	}
	if (!filename) { filename = "<string>";
}

	PyObject *ast_mod = PyImport_ImportModule("ast");
	if (!ast_mod) { PyErr_Print(); return NULL; }

	PyObject *parse_fn = PyObject_GetAttrString(ast_mod, "parse");
	if (!parse_fn) { Py_DECREF(ast_mod); PyErr_Print(); return NULL; }

	PyObject *args = Py_BuildValue("(ss)", source, filename);
	if (!args) { Py_DECREF(parse_fn); Py_DECREF(ast_mod); PyErr_Print(); return NULL; }
	PyObject *mod = PyObject_CallObject(parse_fn, args);
	Py_DECREF(args);
	Py_DECREF(parse_fn);

	if (!mod) { Py_DECREF(ast_mod); PyErr_Print(); return NULL; }

	Ast *a = calloc(1, sizeof(Ast));
	if (!a) { return NULL;
}
	a->module = mod;
	/* keep ast module ref? not stored; class checks import again */
	Py_DECREF(ast_mod);
	return a;
}

void ast_free(Ast *ast)
{
	if (!ast) { return;
}
	Py_XDECREF(ast->module);
	free(ast);
}

static PyObject *ast_module(void)
{
	return PyImport_ImportModule("ast");
}

// cppcheck-suppress staticFunction
int ast_class_count(Ast *ast)
{
	if (!ast || !ast->module) { return 0;
}
	PyObject *body = PyObject_GetAttrString(ast->module, "body");
	if (!body || !PyList_Check(body)) { Py_XDECREF(body); return 0; }
	PyObject *ast_mod = ast_module();
	PyObject *classdef = ast_mod ? PyObject_GetAttrString(ast_mod, "ClassDef") : NULL;
	int count = 0;
	Py_ssize_t n = PyList_Size(body);
	for (Py_ssize_t i = 0; i < n; i++) {
		PyObject *node = PyList_GetItem(body, i);
		if (classdef && PyObject_IsInstance(node, classdef)) { count++;
}
	}
	Py_XDECREF(classdef);
	Py_XDECREF(ast_mod);
	Py_DECREF(body);
	return count;
}

// cppcheck-suppress unusedFunction
int ast_class_names(Ast *ast, char ***out_names)
{
	if (!ast || !ast->module || !out_names) { return 0;
}
	*out_names = NULL;
	PyObject *body = PyObject_GetAttrString(ast->module, "body");
	if (!body || !PyList_Check(body)) { Py_XDECREF(body); return 0; }
	PyObject *ast_mod = ast_module();
	PyObject *classdef = ast_mod ? PyObject_GetAttrString(ast_mod, "ClassDef") : NULL;

	int count = ast_class_count(ast);
	if (count <= 0) {
		Py_XDECREF(classdef); Py_XDECREF(ast_mod); Py_DECREF(body);
		return 0;
	}
	char **names = calloc(count, sizeof(char *));
	if (!names) {
		Py_XDECREF(classdef); Py_XDECREF(ast_mod); Py_DECREF(body);
		return 0;
	}
	int idx = 0;
	Py_ssize_t n = PyList_Size(body);
	for (Py_ssize_t i = 0; i < n && idx < count; i++) {
		PyObject *node = PyList_GetItem(body, i);
		if (classdef && PyObject_IsInstance(node, classdef)) {
			PyObject *name = PyObject_GetAttrString(node, "name");
			if (name) {
				const char *s = PyUnicode_AsUTF8(name);
				if (s) {
					char *dup = strdup(s);
					if (dup) { names[idx++] = dup;
}
				}
				Py_DECREF(name);
			}
		}
	}
	Py_XDECREF(classdef);
	Py_XDECREF(ast_mod);
	Py_DECREF(body);
	*out_names = names;
	return idx;
}
