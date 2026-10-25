#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <json-c/json.h>

#include "tools.h"
#include "index_manager.h"

static void handle_initialize(json_object *id)
{
	json_object *result = json_object_new_object();
	json_object_object_add(result, "protocolVersion", json_object_new_string("2024-11-05"));

	json_object *caps = json_object_new_object();
	json_object *tools_cap = json_object_new_object();
	json_object_object_add(tools_cap, "listChanged", json_object_new_boolean(0));
	json_object_object_add(caps, "tools", tools_cap);
	json_object_object_add(result, "capabilities", caps);

	json_object *info = json_object_new_object();
	json_object_object_add(info, "name", json_object_new_string("pycoolmcp"));
	json_object_object_add(info, "version", json_object_new_string("0.1.0"));
	json_object_object_add(result, "serverInfo", info);

	send_response(id, result, NULL);
}

static void handle_request(json_object *req)
{
	json_object *id = json_object_object_get(req, "id");
	json_object *method_obj = json_object_object_get(req, "method");
	const char *method = method_obj ? json_object_get_string(method_obj) : NULL;
	json_object *params = json_object_object_get(req, "params");

	if (!method) {
		json_object *err = json_object_new_object();
		json_object_object_add(err, "code", json_object_new_int(-32600));
		json_object_object_add(err, "message", json_object_new_string("invalid request"));
		send_response(id, NULL, err);
		return;
	}

	if (strcmp(method, "initialize") == 0) {
		handle_initialize(id);
	} else if (strcmp(method, "notifications/initialized") == 0) {
		/* no response */
	} else if (strcmp(method, "ping") == 0) {
		send_response(id, json_object_new_object(), NULL);
	} else if (strcmp(method, "tools/list") == 0) {
		handle_tools_list(id);
	} else if (strcmp(method, "tools/call") == 0) {
		if (!params) params = json_object_new_object();
		handle_tools_call(id, params);
	} else {
		json_object *err = json_object_new_object();
		json_object_object_add(err, "code", json_object_new_int(-32601));
		json_object_object_add(err, "message", json_object_new_string("method not found"));
		send_response(id, NULL, err);
	}
}

int main(int argc, char **argv)
{
	(void)argc;
	resolve_pycoolctl(argv[0]);
	resolve_pycool(argv[0]);

	char *line = NULL;
	size_t cap = 0;
	ssize_t len;
	while ((len = getline(&line, &cap, stdin)) != -1) {
		json_object *req = json_tokener_parse(line);
		if (!req) {
			json_object *err = json_object_new_object();
			json_object_object_add(err, "code", json_object_new_int(-32700));
			json_object_object_add(err, "message", json_object_new_string("parse error"));
			send_response(NULL, NULL, err);
			continue;
		}
		handle_request(req);
		json_object_put(req);
	}
	index_cleanup();
	free(line);
	return 0;
}