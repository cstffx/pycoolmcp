#include <stdio.h>
#include "ast.h"

int main(void)
{
	const char *src = "class Foo:\n    pass\nclass Bar(Foo):\n    pass\n";
	Ast *a = ast_parse(src, "t.py");
	printf("classes: %d\n", ast_class_count(a));
	char **names = NULL;
	int n = ast_class_names(a, &names);
	for (int i = 0; i < n; i++) { printf("%s\n", names[i]); free(names[i]); }
	free(names);
	ast_free(a);
	return 0;
}
