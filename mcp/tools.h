#ifndef PYCOOL_MCP_TOOLS_H
#define PYCOOL_MCP_TOOLS_H

#include <json-c/json.h>

void send_response(json_object *id, json_object *result, json_object *error);
void handle_tools_list(json_object *id);
void handle_tools_call(json_object *id, json_object *params);

#endif