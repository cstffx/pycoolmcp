#ifndef PYCOOLS_AST_H
#define PYCOOLS_AST_H

#include <Python.h>

typedef struct {
	PyObject *module; /* ast.Module */
} Ast;

/* Parse Python source into an AST. Returns NULL on error. */
Ast *ast_parse(const char *source, const char *filename);

/* Free an AST. */
void ast_free(Ast *ast);

/* Return number of top-level class definitions. */
int ast_class_count(Ast *ast);

/* Fill out_names with class names (caller frees each and array). Returns count. */
int ast_class_names(Ast *ast, char ***out_names);

#endif /* PYCOOLS_AST_H */
