#ifndef EXTRACT_ASSIGN_H
#define EXTRACT_ASSIGN_H

#include <Python.h>
#include "common.h"

/* Extract assignment targets (variables) */
void extract_assign_targets(PyObject *target, Ctx *ctx, int line);

/* Extract assignment at module level */
void extract_assign(PyObject *node, int lineno, Ctx *ctx);

#endif
