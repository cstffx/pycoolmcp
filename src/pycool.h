#ifndef PYCOOLS_PYCOOL_H
#define PYCOOLS_PYCOOL_H

/* Run the indexing daemon: scan root and watch for changes, maintaining dbpath. */
int pycool_run(const char *root, const char *dbpath);

#endif /* PYCOOLS_PYCOOL_H */
