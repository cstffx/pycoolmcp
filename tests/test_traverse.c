#include <stdio.h>
#include "ast.h"
#include "traverse.h"

static void cb(const char *type, int lineno, const char *name, void *ud)
{
	(void)ud;
	printf("%s %d %s\n", type, lineno, name ? name : "-");
}

int main(void)
{
	const char *src = "class Foo:\n    pass\ndef bar():\n    x = 1\n";
	Ast *a = ast_parse(src, "t.py");
	ast_traverse(a, cb, NULL, NULL);
	ast_free(a);
	return 0;
}
