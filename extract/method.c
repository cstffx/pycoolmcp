#define _GNU_SOURCE
#include "method.h"
#include "../index/index.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void append_to_methods(const char *sig, char **methods, size_t *cap, size_t *len)
{
	if (!sig || !methods) { return;
}
	size_t add = strlen(sig) + 2;
	if (*len + add >= *cap) {
		*cap = (*len + add) * 2;
		char *tmp = realloc(*methods, *cap);
		if (!tmp) { return;
}
		*methods = tmp;
	}
	if (*len > 0) { (*methods)[(*len)++] = ',';
}
	strcpy(*methods + *len, sig);
	*len += strlen(sig);
}

static void extract_posonly_args(PyObject *posonly, sqlite3_int64 method_id, Ctx *ctx, Py_ssize_t *npos_out)
{
	if (!posonly || !PyList_Check(posonly)) { return;
}
	Py_ssize_t npos = PyList_Size(posonly);
	*npos_out = npos;
	for (Py_ssize_t i = 0; i < npos; i++) {
		PyObject *arg = PyList_GetItem(posonly, i);
		PyObject *aname = PyObject_GetAttrString(arg, "arg");
		const char *astr = aname ? PyUnicode_AsUTF8(aname) : NULL;
		if (astr && !PyErr_Occurred()) {
			index_add_method_arg(ctx->db, method_id, astr, NULL, NULL, "positional-only", (int)i);
		}
		Py_XDECREF(aname);
	}
}

static void extract_positional_args(PyObject *pos, sqlite3_int64 method_id, Ctx *ctx, Py_ssize_t npos)
{
	if (!pos || !PyList_Check(pos)) { return;
}
	Py_ssize_t np = PyList_Size(pos);
	for (Py_ssize_t i = 0; i < np; i++) {
		PyObject *arg = PyList_GetItem(pos, i);
		PyObject *aname = PyObject_GetAttrString(arg, "arg");
		const char *astr = aname ? PyUnicode_AsUTF8(aname) : NULL;
		if (astr && !PyErr_Occurred()) {
			index_add_method_arg(ctx->db, method_id, astr, NULL, NULL, "positional", (int)(npos + i));
		}
		Py_XDECREF(aname);
	}
}

static void extract_kwonly_args(PyObject *kwonly, sqlite3_int64 method_id, Ctx *ctx)
{
	if (!kwonly || !PyList_Check(kwonly)) { return;
}
	Py_ssize_t nkw = PyList_Size(kwonly);
	for (Py_ssize_t i = 0; i < nkw; i++) {
		PyObject *arg = PyList_GetItem(kwonly, i);
		PyObject *aname = PyObject_GetAttrString(arg, "arg");
		const char *astr = aname ? PyUnicode_AsUTF8(aname) : NULL;
		if (astr && !PyErr_Occurred()) {
			index_add_method_arg(ctx->db, method_id, astr, NULL, NULL, "keyword-only", (int)i);
		}
		Py_XDECREF(aname);
	}
}

static void extract_varargs(PyObject *vararg, sqlite3_int64 method_id, Ctx *ctx)
{
	if (!vararg || vararg == Py_None) { return;
}
	PyObject *vname = PyObject_GetAttrString(vararg, "arg");
	const char *vstr = vname ? PyUnicode_AsUTF8(vname) : NULL;
	if (vstr && !PyErr_Occurred()) {
		index_add_method_arg(ctx->db, method_id, vstr, NULL, NULL, "varargs", -1);
	}
	Py_XDECREF(vname);
}

static void extract_kwargs(PyObject *kwarg, sqlite3_int64 method_id, Ctx *ctx)
{
	if (!kwarg || kwarg == Py_None) { return;
}
	PyObject *kname = PyObject_GetAttrString(kwarg, "arg");
	const char *kstr = kname ? PyUnicode_AsUTF8(kname) : NULL;
	if (kstr && !PyErr_Occurred()) {
		index_add_method_arg(ctx->db, method_id, kstr, NULL, NULL, "kwargs", -1);
	}
	Py_XDECREF(kname);
}

static void extract_method_args(PyObject *args_obj, sqlite3_int64 method_id, Ctx *ctx)
{
	if (!args_obj || !ctx) { return;
}
	PyObject *posonly = PyObject_GetAttrString(args_obj, "posonlyargs");
	PyObject *pos = PyObject_GetAttrString(args_obj, "args");
	PyObject *vararg = PyObject_GetAttrString(args_obj, "vararg");
	PyObject *kwonly = PyObject_GetAttrString(args_obj, "kwonlyargs");
	PyObject *defaults = PyObject_GetAttrString(args_obj, "defaults");
	PyObject *kw_defaults = PyObject_GetAttrString(args_obj, "kw_defaults");
	PyObject *kwarg = PyObject_GetAttrString(args_obj, "kwarg");

	Py_ssize_t npos = 0;
	extract_posonly_args(posonly, method_id, ctx, &npos);
	extract_positional_args(pos, method_id, ctx, npos);
	extract_kwonly_args(kwonly, method_id, ctx);
	extract_varargs(vararg, method_id, ctx);
	extract_kwargs(kwarg, method_id, ctx);

	Py_XDECREF(kwarg);
	Py_XDECREF(kw_defaults);
	Py_XDECREF(defaults);
	Py_XDECREF(kwonly);
	Py_XDECREF(vararg);
	Py_XDECREF(pos);
	Py_XDECREF(posonly);
	PyErr_Clear();
}

static char *extract_decorators_from_list(PyObject *decs, PyObject *unparse_fn)
{
	if (!decs || !PyList_Check(decs)) { return NULL;
}
	size_t dcap = 256, dlen = 0;
	char *decorators = malloc(dcap);
	if (!decorators) { return NULL;
}
	decorators[0] = '\0';
	Py_ssize_t ndec = PyList_Size(decs);
	for (Py_ssize_t i = 0; i < ndec; i++) {
		char *dec_str = get_unparse_str(PyList_GetItem(decs, i), unparse_fn);
		if (!dec_str) { continue;
}
		if (dlen + strlen(dec_str) + 2 >= dcap) {
			dcap = (dlen + strlen(dec_str) + 2) * 2;
			char *tmp = realloc(decorators, dcap);
			if (!tmp) { free(dec_str); break;
}
			decorators = tmp;
		}
		if (dlen > 0) { decorators[dlen++] = ' ';
}
		strcpy(decorators + dlen, dec_str);
		dlen += strlen(dec_str);
		free(dec_str);
	}
	decorators[dlen] = '\0';
	return decorators;
}

static char *build_method_signature(const char *method_name, PyObject *args_obj,
				     PyObject *unparse_fn, int is_async)
{
	char *sig = NULL;
	if (args_obj && unparse_fn) {
		PyObject *args_str = PyObject_CallFunction(unparse_fn, "(O)", args_obj);
		const char *args_s = (args_str && PyUnicode_Check(args_str)) ? PyUnicode_AsUTF8(args_str) : "";
		if (asprintf(&sig, "%s%s(%s)", is_async ? "async " : "", method_name, args_s ? args_s : "") == -1) {
			sig = NULL;
		}
		Py_XDECREF(args_str);
	} else if (asprintf(&sig, "%s%s()", is_async ? "async " : "", method_name) == -1) {
		sig = NULL;
	}
	return sig;
}

static int get_method_is_async(PyObject *node)
{
	PyObject *name_cls = PyObject_GetAttrString(node, "__class__");
	PyObject *name_type_obj = name_cls ? PyObject_GetAttrString(name_cls, "__name__") : NULL;
	const char *is_async_type = name_type_obj ? PyUnicode_AsUTF8(name_type_obj) : NULL;
	int is_async = (is_async_type && strcmp(is_async_type, "AsyncFunctionDef") == 0);
	Py_XDECREF(name_type_obj);
	Py_XDECREF(name_cls);
	return is_async;
}

void extract_method_in_class(PyObject *node, const char *method_name, int class_lineno,
			     PyObject *ast_mod, Ctx *ctx, sqlite3_int64 class_id,
			     char **methods, size_t *cap, size_t *len)
{
	if (!node || !method_name || !ast_mod || !ctx) { return;
}
	int is_async = get_method_is_async(node);
	PyObject *args_obj = PyObject_GetAttrString(node, "args");
	PyObject *unparse_fn = PyObject_GetAttrString(ast_mod, "unparse");
	PyObject *decs = PyObject_GetAttrString(node, "decorator_list");

	char *sig = build_method_signature(method_name, args_obj, unparse_fn, is_async);
	if (sig) {
		append_to_methods(sig, methods, cap, len);
		free(sig);
	}

	int mline = get_int_attr(node, "lineno", class_lineno);
	int mend = get_int_attr(node, "end_lineno", 0);
	char *decorators = extract_decorators_from_list(decs, unparse_fn);

	sqlite3_int64 mid = 0;
	char *docstring = extract_docstring(node, ast_mod);
	index_add_method(ctx->db, class_id, method_name, mline, mend, is_async,
			decorators ? decorators : "", NULL, docstring, &mid);

	if (mid > 0 && args_obj) {
		extract_method_args(args_obj, mid, ctx);
	}

	free(docstring);
	free(decorators);
	Py_XDECREF(decs);
	Py_XDECREF(unparse_fn);
	Py_XDECREF(args_obj);
	PyErr_Clear();
}

void extract_assigned_method_in_class(PyObject *node, int lineno, PyObject *ast_mod,
				      Ctx *ctx, sqlite3_int64 class_id, char **methods,
				      size_t *cap, size_t *len)
{
	/* Placeholder - assignment handling in class body */
	(void)node;
	(void)lineno;
	(void)ast_mod;
	(void)ctx;
	(void)class_id;
	(void)methods;
	(void)cap;
	(void)len;
}

void extract_function(const char *func_name, int lineno, PyObject *node, Ctx *ctx)
{
	if (!func_name || !ctx) { return;
}
	sqlite3_int64 sid = 0;
	index_add_symbol(ctx->db, ctx->file_id, func_name, "function", lineno, 0, NULL, &sid);
	(void)node;
}
