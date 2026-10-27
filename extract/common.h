#ifndef EXTRACT_COMMON_H
#define EXTRACT_COMMON_H

#include <Python.h>
#include <sqlite3.h>

/* Forward declare Ctx - defined in src/extract.h */
typedef struct { sqlite3 *db; sqlite3_int64 file_id; } Ctx;

/* Utilities for building strings from AST nodes */
char *extract_bases(PyObject *node, PyObject *ast_mod);
char *extract_decorators(PyObject *node, PyObject *ast_mod);
char *extract_docstring(PyObject *node, PyObject *ast_mod);

/* Get and release AST module safely */
PyObject *get_ast_module(void);
void release_ast_module(PyObject *ast_mod);

/* Safe attribute access */
char *get_unparse_str(PyObject *obj, PyObject *unparse_fn);
int get_int_attr(PyObject *obj, const char *attr_name, int default_val);

#endif
