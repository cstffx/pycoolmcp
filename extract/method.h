#ifndef EXTRACT_METHOD_H
#define EXTRACT_METHOD_H

#include <Python.h>
#include <sqlite3.h>
#include "common.h"

/* Extract method/function definition within a class */
void extract_method_in_class(PyObject *node, const char *method_name, int class_lineno,
			     PyObject *ast_mod, Ctx *ctx, sqlite3_int64 class_id,
			     char **methods, size_t *cap, size_t *len);

/* Extract assigned method within a class */
void extract_assigned_method_in_class(PyObject *node, int lineno, PyObject *ast_mod,
				      Ctx *ctx, sqlite3_int64 class_id, char **methods,
				      size_t *cap, size_t *len);

/* Extract module-level function/method */
void extract_function(const char *func_name, int lineno, PyObject *node, Ctx *ctx);

/* Extract method arguments (internal use) */
/* void extract_method_args(...); */

#endif
