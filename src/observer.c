#define _GNU_SOURCE
#include "observer.h"
#include <sys/inotify.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

typedef struct { int wd; char *path; } Watch;

static Watch *watches = NULL;
static size_t n_watches = 0, cap_watches = 0;
static int inotify_fd_global = -1;

static int add_watch_entry(int fd, const char *path)
{
	int wd = inotify_add_watch(fd, path, IN_CREATE | IN_DELETE | IN_MODIFY |
	                           IN_MOVED_FROM | IN_MOVED_TO | IN_ATTRIB);
	if (wd < 0) return -1;
	if (n_watches == cap_watches) {
		cap_watches = cap_watches ? cap_watches * 2 : 64;
		watches = realloc(watches, cap_watches * sizeof(Watch));
	}
	watches[n_watches].wd = wd;
	watches[n_watches].path = strdup(path);
	n_watches++;
	return 0;
}

static const char *path_for_wd(int wd)
{
	for (size_t i = 0; i < n_watches; i++)
		if (watches[i].wd == wd) return watches[i].path;
	return NULL;
}

static void remove_watch(int wd)
{
	for (size_t i = 0; i < n_watches; i++) {
		if (watches[i].wd == wd) {
			free(watches[i].path);
			watches[i] = watches[--n_watches];
			return;
		}
	}
}

static int add_recursive(int fd, const char *root)
{
	if (add_watch_entry(fd, root) != 0) return -1;
	DIR *d = opendir(root);
	if (!d) {
		if (errno != ENOENT) fprintf(stderr, "opendir %s: %s\n", root, strerror(errno));
		return 0;
	}
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
		size_t plen = strlen(root) + strlen(ent->d_name) + 2;
		char *path = malloc(plen);
		if (!path) continue;
		snprintf(path, plen, "%s/%s", root, ent->d_name);
		struct stat st;
		if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			add_recursive(fd, path);
		}
		free(path);
	}
	closedir(d);
	return 0;
}

int observer_watch(const char *root, observer_callback cb, void *userdata)
{
	int fd = inotify_init1(IN_CLOEXEC);
	if (fd < 0) return -1;
	inotify_fd_global = fd;
	if (add_recursive(fd, root) != 0) { close(fd); return -1; }
	path_for_wd(-1);

	char buf[65536];
	while (1) {
		ssize_t len = read(fd, buf, sizeof(buf));
		if (len < 0) {
			if (errno == EINTR) continue;
			break;
		}
		if (len == 0) break;
		for (char *ptr = buf; ptr < buf + len; ) {
			struct inotify_event *ev = (struct inotify_event *)ptr;
			const char *dir = path_for_wd(ev->wd);
			if (dir) {
				char *full = NULL;
				if (ev->len > 0) {
					size_t flen = strlen(dir) + strlen(ev->name) + 2;
					full = malloc(flen);
					if (full) snprintf(full, flen, "%s/%s", dir, ev->name);
				} else {
					full = strdup(dir);
				}
				if (!full) { ptr += sizeof(struct inotify_event) + ev->len; continue; }

				if (cb) cb(full, ev->mask, userdata);

				if ((ev->mask & IN_CREATE || ev->mask & IN_MOVED_TO) && (ev->mask & IN_ISDIR)) {
					add_recursive(fd, full);
				}
				if (ev->mask & IN_IGNORED) {
					remove_watch(ev->wd);
				}
				free(full);
			}
			ptr += sizeof(struct inotify_event) + ev->len;
		}
	}
	close(fd);
	inotify_fd_global = -1;
	for (size_t i = 0; i < n_watches; i++) free(watches[i].path);
	free(watches);
	watches = NULL; n_watches = cap_watches = 0;
	return 0;
}

void observer_stop(void)
{
	if (inotify_fd_global >= 0) {
		close(inotify_fd_global);
		inotify_fd_global = -1;
	}
}
