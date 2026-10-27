#ifndef EXTRACT_CLASS_H
#define EXTRACT_CLASS_H

#include <Python.h>
#include "common.h"

/* Extract class definition and its details */
void extract_class(const char *class_name, int lineno, PyObject *node, Ctx *ctx);

#endif
