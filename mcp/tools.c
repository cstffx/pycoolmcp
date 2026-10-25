#define _GNU_SOURCE
#include "tools.h"
#include "index_manager.h"
#include "../pycoolctl/class.h"
#include "../pycoolctl/method.h"
#include "../pycoolctl/list.h"
#include "../pycoolctl/info.h"
#include "../pycoolctl/reindex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

static const char *get_str(json_object *o, const char *key)
{
	json_object *v;
	if (json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_string))
		return json_object_get_string(v);
	return NULL;
}

static int get_bool(json_object *o, const char *key, int def)
{
	json_object *v;
	if (json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_boolean))
		return json_object_get_boolean(v);
	return def;
}

static int get_int(json_object *o, const char *key, int def)
{
	json_object *v;
	if (json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_int))
		return json_object_get_int(v);
	return def;
}

json_object *tool_result(const char *text, int is_error)
{
	json_object *content = json_object_new_array();
	json_object *item = json_object_new_object();
	json_object_object_add(item, "type", json_object_new_string("text"));
	json_object_object_add(item, "text", json_object_new_string(text ? text : ""));
	json_object_array_add(content, item);

	json_object *result = json_object_new_object();
	json_object_object_add(result, "content", content);
	json_object_object_add(result, "isError", json_object_new_boolean(is_error));
	return result;
}

static json_object *schema_obj(const char *json)
{
	json_object *o = json_tokener_parse(json);
	if (!o) o = json_object_new_object();
	return o;
}

static json_object *make_tool(const char *name, const char *desc, json_object *schema)
{
	json_object *t = json_object_new_object();
	json_object_object_add(t, "name", json_object_new_string(name));
	json_object_object_add(t, "description", json_object_new_string(desc));
	json_object_object_add(t, "inputSchema", schema);
	return t;
}

void send_response(json_object *id, json_object *result, json_object *error)
{
	json_object *resp = json_object_new_object();
	json_object_object_add(resp, "jsonrpc", json_object_new_string("2.0"));
	if (id) json_object_object_add(resp, "id", json_object_get(id));
	if (error) json_object_object_add(resp, "error", error);
	else json_object_object_add(resp, "result", result);
	fputs(json_object_to_json_string(resp), stdout);
	fputc('\n', stdout);
	fflush(stdout);
	json_object_put(resp);
}

void handle_tools_list(json_object *id)
{
	json_object *tools = json_object_new_array();

	json_object *s1 = schema_obj("{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},\"db\":{\"type\":\"string\"},\"file\":{\"type\":\"string\"},\"pid\":{\"type\":\"integer\"},\"info\":{\"type\":\"string\"},\"method\":{\"type\":\"string\"},\"body\":{\"type\":\"boolean\"},\"refs\":{\"type\":\"boolean\"},\"json\":{\"type\":\"boolean\"},\"compact\":{\"type\":\"boolean\"},\"full\":{\"type\":\"boolean\"},\"limit\":{\"type\":\"integer\"},\"offset\":{\"type\":\"integer\"}},\"required\":[\"name\"]}");
	json_object_array_add(tools, make_tool("query_class", "Query classes/methods by name in the index", s1));

	json_object *s2 = schema_obj("{\"type\":\"object\",\"properties\":{\"class\":{\"type\":\"string\"},\"method\":{\"type\":\"string\"},\"db\":{\"type\":\"string\"},\"file\":{\"type\":\"string\"},\"pid\":{\"type\":\"integer\"},\"body\":{\"type\":\"boolean\"},\"json\":{\"type\":\"boolean\"},\"limit\":{\"type\":\"integer\"},\"offset\":{\"type\":\"integer\"}},\"required\":[\"class\"]}");
	json_object_array_add(tools, make_tool("query_method", "Query methods of a class", s2));

	json_object *s3 = schema_obj("{\"type\":\"object\",\"properties\":{\"dbsize\":{\"type\":\"boolean\"}}}");
	json_object_array_add(tools, make_tool("list_instances", "List running pycool instances", s3));

	json_object *s4 = schema_obj("{\"type\":\"object\",\"properties\":{\"target\":{\"type\":\"string\"}},\"required\":[\"target\"]}");
	json_object_array_add(tools, make_tool("info", "Show index info (db, pid, or all)", s4));

	json_object *s5 = schema_obj("{\"type\":\"object\",\"properties\":{\"target\":{\"type\":\"string\"}},\"required\":[\"target\"]}");
	json_object_array_add(tools, make_tool("reindex", "Trigger reindex for a pid or all running instances", s5));

	json_object *s6 = schema_obj("{\"type\":\"object\",\"properties\":{\"root\":{\"type\":\"string\"},\"db\":{\"type\":\"string\"}},\"required\":[\"root\"]}");
	json_object_array_add(tools, make_tool("index_start", "Start pycool indexer in background", s6));

	json_object *s7 = schema_obj("{\"type\":\"object\",\"properties\":{}}");
	json_object_array_add(tools, make_tool("index_stop", "Stop background pycool indexer", s7));

	json_object *result = json_object_new_object();
	json_object_object_add(result, "tools", tools);
	send_response(id, result, NULL);
}

void handle_tools_call(json_object *id, json_object *params)
{
	const char *name = get_str(params, "name");
	json_object *args = NULL;
	json_object_object_get_ex(params, "arguments", &args);
	if (!args) args = json_object_new_object();

	if (!name) {
		json_object *err = json_object_new_object();
		json_object_object_add(err, "code", json_object_new_int(-32602));
		json_object_object_add(err, "message", json_object_new_string("missing tool name"));
		send_response(id, NULL, err);
		return;
	}

	if (strcmp(name, "index_start") == 0) {
		const char *root = get_str(args, "root");
		const char *db = get_str(args, "db");
		if (!root) {
			json_object *err = json_object_new_object();
			json_object_object_add(err, "code", json_object_new_int(-32602));
			json_object_object_add(err, "message", json_object_new_string("missing root"));
			send_response(id, NULL, err);
			return;
		}
		char *msg = NULL;
		int rc = index_start(root, db, &msg);
		json_object *result = tool_result(msg ? msg : "", rc != 0);
		send_response(id, result, NULL);
		free(msg);
		return;
	}

	if (strcmp(name, "index_stop") == 0) {
		char *msg = NULL;
		int rc = index_stop(&msg);
		json_object *result = tool_result(msg ? msg : "", rc != 0);
		send_response(id, result, NULL);
		free(msg);
		return;
	}

	char *argv[64];
	int argc = 0;
	argv[argc++] = (char *)pycoolctl_path();

	if (strcmp(name, "query_class") == 0) {
		const char *n = get_str(args, "name");
		if (!n) {
			json_object *err = json_object_new_object();
			json_object_object_add(err, "code", json_object_new_int(-32602));
			json_object_object_add(err, "message", json_object_new_string("missing name"));
			send_response(id, NULL, err);
			return;
		}
		argv[argc++] = "class";
		argv[argc++] = (char *)n;
		const char *method = get_str(args, "method");
		if (method) { argv[argc++] = "--method"; argv[argc++] = (char *)method; }
		const char *file = get_str(args, "file");
		if (file) { argv[argc++] = "--file"; argv[argc++] = (char *)file; }
		const char *db = get_str(args, "db");
		if (db) { argv[argc++] = "--db"; argv[argc++] = (char *)db; }
		int pid = get_int(args, "pid", -1);
		if (pid >= 0) {
			static char pidbuf[32];
			snprintf(pidbuf, sizeof(pidbuf), "%d", pid);
			argv[argc++] = "--pid"; argv[argc++] = pidbuf;
		}
		const char *info = get_str(args, "info");
		if (info) { argv[argc++] = "--info"; argv[argc++] = (char *)info; }
		if (get_bool(args, "body", 0)) argv[argc++] = "--body";
		if (get_bool(args, "refs", 0)) argv[argc++] = "--refs";
		if (get_bool(args, "json", 0)) argv[argc++] = "--json";
		if (get_bool(args, "compact", 0)) argv[argc++] = "--compact";
		if (get_bool(args, "full", 0)) argv[argc++] = "--full";
		int limit = get_int(args, "limit", 0);
		if (limit > 0) {
			static char lbuf[32]; snprintf(lbuf, sizeof(lbuf), "%d", limit);
			argv[argc++] = "--limit"; argv[argc++] = lbuf;
		}
		int offset = get_int(args, "offset", 0);
		if (offset > 0) {
			static char obuf[32]; snprintf(obuf, sizeof(obuf), "%d", offset);
			argv[argc++] = "--offset"; argv[argc++] = obuf;
		}
	} else if (strcmp(name, "query_method") == 0) {
		const char *cls = get_str(args, "class");
		if (!cls) {
			json_object *err = json_object_new_object();
			json_object_object_add(err, "code", json_object_new_int(-32602));
			json_object_object_add(err, "message", json_object_new_string("missing class"));
			send_response(id, NULL, err);
			return;
		}
		argv[argc++] = "method";
		argv[argc++] = (char *)cls;
		const char *method = get_str(args, "method");
		if (method) argv[argc++] = (char *)method;
		const char *file = get_str(args, "file");
		if (file) { argv[argc++] = "--file"; argv[argc++] = (char *)file; }
		const char *db = get_str(args, "db");
		if (db) { argv[argc++] = "--db"; argv[argc++] = (char *)db; }
		int pid = get_int(args, "pid", -1);
		if (pid >= 0) {
			static char pidbuf[32];
			snprintf(pidbuf, sizeof(pidbuf), "%d", pid);
			argv[argc++] = "--pid"; argv[argc++] = pidbuf;
		}
		if (get_bool(args, "body", 0)) argv[argc++] = "--body";
		if (get_bool(args, "json", 0)) argv[argc++] = "--json";
		int limit = get_int(args, "limit", 0);
		if (limit > 0) {
			static char lbuf[32]; snprintf(lbuf, sizeof(lbuf), "%d", limit);
			argv[argc++] = "--limit"; argv[argc++] = lbuf;
		}
		int offset = get_int(args, "offset", 0);
		if (offset > 0) {
			static char obuf[32]; snprintf(obuf, sizeof(obuf), "%d", offset);
			argv[argc++] = "--offset"; argv[argc++] = obuf;
		}
	} else if (strcmp(name, "list_instances") == 0) {
		argv[argc++] = "list";
		if (get_bool(args, "dbsize", 0)) argv[argc++] = "--dbsize";
	} else if (strcmp(name, "info") == 0) {
		const char *target = get_str(args, "target");
		if (!target) target = "all";
		argv[argc++] = "info";
		argv[argc++] = (char *)target;
	} else if (strcmp(name, "reindex") == 0) {
		const char *target = get_str(args, "target");
		if (!target) target = "all";
		argv[argc++] = "reindex";
		argv[argc++] = (char *)target;
	} else {
		json_object *err = json_object_new_object();
		json_object_object_add(err, "code", json_object_new_int(-32601));
		json_object_object_add(err, "message", json_object_new_string("unknown tool"));
		send_response(id, NULL, err);
		return;
	}
	argv[argc] = NULL;

	char *buf = NULL;
	size_t bufsize = 0;
	FILE *mem = open_memstream(&buf, &bufsize);
	int rc = 0;
	if (strcmp(name, "query_class") == 0) rc = class_command(argc, argv, mem);
	else if (strcmp(name, "query_method") == 0) rc = method_command(argc, argv, mem);
	else if (strcmp(name, "list_instances") == 0) rc = list_command(argc, argv, mem);
	else if (strcmp(name, "info") == 0) rc = info_command(argc, argv, mem);
	else if (strcmp(name, "reindex") == 0) rc = reindex_command(argc, argv);
	fclose(mem);

	if (strcmp(name, "reindex") == 0 && rc == 0 && (!buf || !buf[0])) {
		free(buf);
		buf = strdup("reindex triggered");
	}
	int is_error = (rc != 0);
	if (buf && (strstr(buf, "cannot open") || strstr(buf, "no running instance") || strstr(buf, "not found") || strstr(buf, "schema mismatch")))
		is_error = 1;
	json_object *result = tool_result(buf ? buf : "", is_error);
	send_response(id, result, NULL);
	free(buf);
}