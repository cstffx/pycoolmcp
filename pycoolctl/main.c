#include <stdio.h>
#include <string.h>
#include "list.h"
#include "info.h"
#include "class.h"
#include "method.h"
#include "search.h"
#include "stop.h"
#include "clean.h"
#include "reindex.h"

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s list|info|stop|clean|reindex ...\n", argv[0]);
		return 2;
	}
	if (strcmp(argv[1], "list") == 0) { return list_command(argc, argv, stdout);
}
	if (strcmp(argv[1], "info") == 0) { return info_command(argc, argv, stdout);
}
	if (strcmp(argv[1], "class") == 0) { return class_command(argc, argv, stdout);
}
	if (strcmp(argv[1], "method") == 0) { return method_command(argc, argv, stdout);
}
	if (strcmp(argv[1], "search") == 0) { return search_command(argc, argv, stdout);
}
	if (strcmp(argv[1], "stop") == 0) { return stop_command(argc, argv);
}
	if (strcmp(argv[1], "clean") == 0) { return clean_command(argc, argv);
}
	if (strcmp(argv[1], "reindex") == 0) { return reindex_command(argc, argv);
}
	fprintf(stderr, "Unknown command: %s\n", argv[1]);
	return 2;
}
