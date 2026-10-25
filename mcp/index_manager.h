#ifndef PYCOOL_MCP_INDEX_MANAGER_H
#define PYCOOL_MCP_INDEX_MANAGER_H

#include <sys/types.h>

void resolve_pycoolctl(const char *argv0);
void resolve_pycool(const char *argv0);

const char *pycoolctl_path(void);
const char *pycool_path(void);

int index_start(const char *root, const char *db, char **msg);
int index_stop(char **msg);
void index_cleanup(void);

#endif