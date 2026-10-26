#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include "observer.h"

static void handler(int s){ (void)s; observer_stop(); }

static void cb(const char *path, uint32_t mask, void *ud) { (void)path; (void)mask; (void)ud; }

static char *g_root;

static void *events(void *arg) {
	usleep(200000);
	char fpath[4096]; snprintf(fpath, sizeof(fpath), "%s/f.txt", g_root);
	int fd = open(fpath, O_WRONLY|O_CREAT, 0644); if (fd>=0){ write(fd,"x",1); close(fd); }
	usleep(100000);
	chmod(fpath, 0600);
	usleep(100000);
	char gpath[4096]; snprintf(gpath, sizeof(gpath), "%s/g.txt", g_root);
	rename(fpath, gpath);
	usleep(100000);
	unlink(gpath);
	usleep(100000);
	char dpath[4096]; snprintf(dpath, sizeof(dpath), "%s/ndir", g_root);
	mkdir(dpath, 0755);
	usleep(100000);
	rmdir(dpath);
	usleep(100000);
	char ddel[4096]; snprintf(ddel, sizeof(ddel), "%s/delme", g_root);
	rmdir(ddel);
	usleep(100000);
	return NULL;
}

int main(void)
{
	signal(SIGALRM, handler);

	char root[] = "/tmp/opencode/obstestXXXXXX";
	char *d = mkdtemp(root);
	g_root = d;
	char subdir[4096]; snprintf(subdir, sizeof(subdir), "%s/sub", d);
	mkdir(subdir, 0755);
	char delme[4096]; snprintf(delme, sizeof(delme), "%s/delme", d);
	mkdir(delme, 0755);

	pthread_t th;
	pthread_create(&th, NULL, events, NULL);
	alarm(1);
	observer_watch(d, cb, NULL);
	pthread_join(th, NULL);

	/* nonexistent root */
	printf("nonexist=%d\n", observer_watch("/tmp/opencode/does_not_exist_observer", cb, NULL));

	/* regular file root -> opendir failure */
	char rf[] = "/tmp/opencode/regular_observer_file";
	unlink(rf);
	int fd = open(rf, O_WRONLY|O_CREAT, 0644); close(fd);
	alarm(1);
	observer_watch(rf, NULL, NULL);
	unlink(rf);

	return 0;
}
