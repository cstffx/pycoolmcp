#include "traverse.h"
// cppcheck-suppress missingIncludeSystem
#include <Python.h>
#include <stdlib.h>
#include <string.h>

void ast_traverse(Ast *ast, ast_visitor visitor, void *userdata, AstSubscriber *subs)
{
	if (!ast || !ast->module) return;
	if (!Py_IsInitialized()) Py_Initialize();

	PyObject *ast_mod = PyImport_ImportModule("ast");
	if (!ast_mod) { PyErr_Print(); return; }

	PyObject *walk_fn = PyObject_GetAttrString(ast_mod, "walk");
	if (!walk_fn) { Py_DECREF(ast_mod); PyErr_Print(); return; }

	PyObject *args = Py_BuildValue("(O)", ast->module);
	PyObject *it = PyObject_CallObject(walk_fn, args);
	Py_DECREF(args);
	if (!it) { Py_DECREF(walk_fn); Py_DECREF(ast_mod); PyErr_Print(); return; }

	PyObject *node;
	while ((node = PyIter_Next(it)) != NULL) {
		PyObject *cls = PyObject_GetAttrString(node, "__class__");
		PyObject *name_attr = cls ? PyObject_GetAttrString(cls, "__name__") : NULL;
		const char *type = name_attr ? PyUnicode_AsUTF8(name_attr) : "node";

		PyObject *lineno_obj = PyObject_GetAttrString(node, "lineno");
		int lineno = 0;
		if (lineno_obj) {
			lineno = (int)PyLong_AsLong(lineno_obj);
			if (PyErr_Occurred()) PyErr_Clear();
			Py_DECREF(lineno_obj);
		} else {
			PyErr_Clear();
		}

		const char *name = NULL;
		PyObject *name_obj = PyObject_GetAttrString(node, "name");
		if (!name_obj) {
			PyErr_Clear();
			name_obj = PyObject_GetAttrString(node, "id");
		}
		if (name_obj) {
			name = PyUnicode_AsUTF8(name_obj);
			if (PyErr_Occurred()) { PyErr_Clear(); name = NULL; }
		} else {
			PyErr_Clear();
		}

		if (visitor) visitor(type, lineno, name, userdata);

		for (AstSubscriber *s = subs; s; s = s->next) {
			int match = 0;
			if (s->node_types) {
				for (int i = 0; s->node_types[i]; i++) {
					if (strcmp(s->node_types[i], type) == 0) { match = 1; break; }
				}
			} else {
				match = 1;
			}
			if (match && s->on_node) s->on_node(type, lineno, name, node, s->userdata);
		}

		Py_XDECREF(name_obj);
		Py_XDECREF(name_attr);
		Py_XDECREF(cls);
		Py_DECREF(node);
		PyErr_Clear();
	}
	if (PyErr_Occurred()) PyErr_Print();

	Py_DECREF(it);
	Py_DECREF(walk_fn);
	Py_DECREF(ast_mod);
}
