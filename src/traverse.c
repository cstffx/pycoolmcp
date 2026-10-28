#include "traverse.h"
#include <Python.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	char type[64];
	int lineno;
	char *name;
} NodeMetadata;

static void extract_node_metadata(PyObject *node, NodeMetadata *meta)
{
	PyObject *cls = PyObject_GetAttrString(node, "__class__");
	PyObject *name_attr = cls ? PyObject_GetAttrString(cls, "__name__") : NULL;
	if (name_attr) {
		const char *type_str = PyUnicode_AsUTF8(name_attr);
		if (type_str) { strncpy(meta->type, type_str, sizeof(meta->type) - 1); }
		Py_DECREF(name_attr);
	}
	if (cls) { Py_DECREF(cls); }

	meta->lineno = 0;
	PyObject *lineno_obj = PyObject_GetAttrString(node, "lineno");
	if (lineno_obj) {
		meta->lineno = (int)PyLong_AsLong(lineno_obj);
		if (PyErr_Occurred()) { PyErr_Clear(); }
		Py_DECREF(lineno_obj);
	} else {
		PyErr_Clear();
	}

	meta->name = NULL;
	PyObject *name_obj = PyObject_GetAttrString(node, "name");
	if (!name_obj) {
		PyErr_Clear();
		name_obj = PyObject_GetAttrString(node, "id");
	}
	if (name_obj) {
		const char *name_str = PyUnicode_AsUTF8(name_obj);
		if (name_str && !PyErr_Occurred()) { meta->name = strdup(name_str); }
		Py_DECREF(name_obj);
	} else {
		PyErr_Clear();
	}
}

static void invoke_subscribers(const char *type, int lineno, const char *name, PyObject *node, AstSubscriber *subs)
{
	for (AstSubscriber *s = subs; s; s = s->next) {
		int match = 0;
		if (s->node_types) {
			for (int i = 0; s->node_types[i]; i++) {
				if (strcmp(s->node_types[i], type) == 0) { match = 1; break; }
			}
		} else {
			match = 1;
		}
		if (match && s->on_node) { s->on_node(type, lineno, name, node, s->userdata); }
	}
}

static int init_ast_walker(PyObject **ast_mod, PyObject **walk_fn, PyObject **it, PyObject *module)
{
	*ast_mod = PyImport_ImportModule("ast");
	if (!*ast_mod) { PyErr_Print(); return -1; }

	*walk_fn = PyObject_GetAttrString(*ast_mod, "walk");
	if (!*walk_fn) { Py_DECREF(*ast_mod); PyErr_Print(); return -1; }

	PyObject *args = Py_BuildValue("(O)", module);
	*it = PyObject_CallObject(*walk_fn, args);
	Py_DECREF(args);
	if (!*it) { Py_DECREF(*walk_fn); Py_DECREF(*ast_mod); PyErr_Print(); return -1; }

	return 0;
}

void ast_traverse(Ast *ast, ast_visitor visitor, void *userdata, AstSubscriber *subs)
{
	if (!ast || !ast->module) { return; }
	if (!Py_IsInitialized()) { Py_Initialize(); }

	PyObject *ast_mod, *walk_fn, *it;
	if (init_ast_walker(&ast_mod, &walk_fn, &it, ast->module) != 0) { return; }

	PyObject *node;
	while ((node = PyIter_Next(it)) != NULL) {
		NodeMetadata meta = {0};
		extract_node_metadata(node, &meta);

		if (visitor) { visitor(meta.type, meta.lineno, meta.name, userdata); }
		invoke_subscribers(meta.type, meta.lineno, meta.name, node, subs);

		free(meta.name);
		Py_DECREF(node);
		PyErr_Clear();
	}
	if (PyErr_Occurred()) { PyErr_Print(); }

	Py_DECREF(it);
	Py_DECREF(walk_fn);
	Py_DECREF(ast_mod);
}
