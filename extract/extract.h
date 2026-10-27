#ifndef EXTRACT_H
#define EXTRACT_H

#include <Python.h>
#include "common.h"

/* AST visitor callback - dispatches to specific handlers */
void on_node(const char *type, int lineno, const char *name, void *node_ptr, void *ud);

#endif
