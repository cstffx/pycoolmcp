#ifndef PYCOOLS_IGNORE_H
#define PYCOOLS_IGNORE_H

typedef struct Ignore Ignore;

Ignore *ignore_load(const char *root);
void ignore_free(Ignore *ig);
int ignore_is_ignored(Ignore *ig, const char *root, const char *path);

#endif /* PYCOOLS_IGNORE_H */
