#ifndef PYCOOLS_OBSERVER_H
#define PYCOOLS_OBSERVER_H

#include <stdint.h>
#include <sys/inotify.h>

/* Callback invoked for each inotify event. mask contains IN_* flags. */
typedef void (*observer_callback)(const char *path, uint32_t mask, void *userdata);

/* Blocking watch loop. Returns 0 on normal exit, -1 on error. */
int observer_watch(const char *root, observer_callback cb, void *userdata);

/* Request the running observer to stop. */
void observer_stop(void);

#endif /* PYCOOLS_OBSERVER_H */
