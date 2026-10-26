#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	(void)argc; (void)argv;
	/* usage branch */
	char *a0[] = {"pycool"};
	printf("usage=%d\n", main(1, a0));

	/* normal run in child, terminated by signal */
	char root[] = "/tmp/opencode/pycoolrootXXXXXX";
	char *dir = mkdtemp(root);
	char dbpath[4096];
	snprintf(dbpath, sizeof(dbpath), "%s/index.db", dir);

	pid_t pid = fork();
	if (pid == 0) {
		char *a1[] = {"pycool", dir, dbpath};
		exit(main(3, a1));
	}
	sleep(1);
	kill(pid, SIGTERM);
	int status = 0;
	waitpid(pid, &status, 0);
	printf("child_status=%d\n", status);

	/* argc>=3 dbpath branch (already covered by child) */
	return 0;
}