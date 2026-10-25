#ifndef PYCOOLS_TRAVERSE_H
#define PYCOOLS_TRAVERSE_H

#include "ast.h"

typedef void (*ast_visitor)(const char *node_type, int lineno, const char *name, void *userdata);

typedef struct AstSubscriber {
	const char **node_types; /* NULL-terminated list */
	void (*on_node)(const char *type, int lineno, const char *name, void *node, void *userdata);
	void *userdata;
	struct AstSubscriber *next;
} AstSubscriber;

/* Visit every node; calls visitor (if given) and matching subscribers in `subs`. */
void ast_traverse(Ast *ast, ast_visitor visitor, void *userdata, AstSubscriber *subs);

#endif /* PYCOOLS_TRAVERSE_H */
