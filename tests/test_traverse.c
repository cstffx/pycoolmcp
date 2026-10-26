#include <stdio.h>
#include "ast.h"
#include "traverse.h"

static void cb(const char *type, int lineno, const char *name, void *ud)
{
	(void)ud;
	printf("%s %d %s\n", type, lineno, name ? name : "-");
}

static void on_node(const char *type, int lineno, const char *name, void *node, void *ud)
{
	(void)node; (void)ud;
	printf("sub %s %d %s\n", type, lineno, name ? name : "-");
}

int main(void)
{
	const char *src = "class Foo:\n    pass\ndef bar():\n    x = 1\n";
	Ast *a = ast_parse(src, "t.py");

	const char *types[] = { "ClassDef", NULL };
	AstSubscriber sub2 = { NULL, on_node, NULL, NULL };
	AstSubscriber sub1 = { types, on_node, NULL, &sub2 };

	ast_traverse(a, cb, NULL, &sub1);
	ast_free(a);
	return 0;
}
