#ifndef PYCOOLS_EXTRACT_H
#define PYCOOLS_EXTRACT_H

#include <sqlite3.h>
// cppcheck-suppress missingIncludeSystem
#include <Python.h>

typedef struct { sqlite3 *db; sqlite3_int64 file_id; } Ctx;

void extract_assign_targets(PyObject *target, Ctx *ctx, int line);
void on_node(const char *type, int lineno, const char *name, void *node_ptr, void *ud);

#endif /* PYCOOLS_EXTRACT_H */