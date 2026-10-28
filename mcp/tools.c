#define _GNU_SOURCE
#include "tools.h"
#include "index_manager.h"
#include "../pycoolctl/class.h"
#include "../pycoolctl/method.h"
#include "../pycoolctl/search.h"
#include "../pycoolctl/list.h"
#include "../pycoolctl/info.h"
#include "../pycoolctl/reindex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

struct ToolContext {
	char *argv[64];
	int argc;
};

static json_object *error_response(int code, const char *msg);
static int handle_pycool_meta(json_object *args, const char **name, json_object **args_out);
static int handle_index_commands(json_object *id, const char *name, json_object *args);
static int build_argv_query_class(struct ToolContext *ctx, json_object *args);
static int build_argv_query_method(struct ToolContext *ctx, json_object *args);
static int build_argv_search_symbols(struct ToolContext *ctx, json_object *args);
static int build_argv_list_instances(struct ToolContext *ctx, json_object *args);
static int build_argv_info(struct ToolContext *ctx, json_object *args);
static int build_argv_reindex(struct ToolContext *ctx, json_object *args);
static int validate_required_param(json_object *id, json_object *args, const char *key, const char *msg);
static int execute_tool_command(json_object *id, const char *name, struct ToolContext *ctx);

static const char *get_str(json_object *o, const char *key)
{
	json_object *v;
	if (json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_string)) {
		return json_object_get_string(v);
}
	return NULL;
}

static int get_bool(json_object *o, const char *key, int def)
{
	json_object *v;
	if (json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_boolean)) {
		return json_object_get_boolean(v);
}
	return def;
}

static int get_int(json_object *o, const char *key, int def)
{
	json_object *v;
	if (json_object_object_get_ex(o, key, &v) && json_object_is_type(v, json_type_int)) {
		return json_object_get_int(v);
}
	return def;
}

// cppcheck-suppress staticFunction
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
	if (!o) { o = json_object_new_object();
}
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

static json_object *error_response(int code, const char *msg)
{
	json_object *err = json_object_new_object();
	json_object_object_add(err, "code", json_object_new_int(code));
	json_object_object_add(err, "message", json_object_new_string(msg));
	return err;
}

static int validate_required_param(json_object *id, json_object *args, const char *key, const char *msg)
{
	if (!get_str(args, key)) {
		send_response(id, NULL, error_response(-32602, msg));
		return 0;
	}
	return 1;
}

static int handle_pycool_meta(json_object *args, const char **name, json_object **args_out)
{
	if (!*name || strcmp(*name, "pycool") != 0) {
		return 1;
	}
	const char *op = get_str(args, "op");
	json_object *inner = NULL;
	json_object_object_get_ex(args, "args", &inner);
	if (!op) { return 0;
	}
	if (!inner) { inner = json_object_new_object();
	}
	*name = op;
	*args_out = inner;
	return 1;
}

static int handle_index_commands(json_object *id, const char *name, json_object *args)
{
	if (strcmp(name, "index_start") == 0) {
		const char *root = get_str(args, "root");
		if (!validate_required_param(id, args, "root", "missing root")) { return 1;
		}
		const char *db = get_str(args, "db");
		char *msg = NULL;
		int rc = index_start(root, db, &msg);
		json_object *result = tool_result(msg ? msg : "", rc != 0);
		send_response(id, result, NULL);
		free(msg);
		return 1;
	}
	if (strcmp(name, "index_stop") == 0) {
		char *msg = NULL;
		int rc = index_stop(&msg);
		json_object *result = tool_result(msg ? msg : "", rc != 0);
		send_response(id, result, NULL);
		free(msg);
		return 1;
	}
	return 0;
}

static int build_argv_query_class(struct ToolContext *ctx, json_object *args)
{
	const char *n = get_str(args, "name");
	if (!n) { return 0;
	}
	ctx->argv[ctx->argc++] = "class";
	ctx->argv[ctx->argc++] = (char *)n;
	const char *method = get_str(args, "method");
	if (method) { ctx->argv[ctx->argc++] = "--method"; ctx->argv[ctx->argc++] = (char *)method; }
	const char *file = get_str(args, "file");
	if (file) { ctx->argv[ctx->argc++] = "--file"; ctx->argv[ctx->argc++] = (char *)file; }
	const char *db = get_str(args, "db");
	if (db) { ctx->argv[ctx->argc++] = "--db"; ctx->argv[ctx->argc++] = (char *)db; }
	int pid = get_int(args, "pid", -1);
	if (pid >= 0) {
		static char pidbuf[32];
		snprintf(pidbuf, sizeof(pidbuf), "%d", pid);
		ctx->argv[ctx->argc++] = "--pid"; ctx->argv[ctx->argc++] = pidbuf;
	}
	const char *info = get_str(args, "info");
	if (info) { ctx->argv[ctx->argc++] = "--info"; ctx->argv[ctx->argc++] = (char *)info; }
	if (get_bool(args, "body", 0)) { ctx->argv[ctx->argc++] = "--body";
	}
	const char *lines = get_str(args, "lines");
	if (lines) { ctx->argv[ctx->argc++] = "--lines"; ctx->argv[ctx->argc++] = (char *)lines; }
	int max_lines = get_int(args, "max_lines", 0);
	if (max_lines > 0) {
		static char mlbuf[32]; snprintf(mlbuf, sizeof(mlbuf), "%d", max_lines);
		ctx->argv[ctx->argc++] = "--max-lines"; ctx->argv[ctx->argc++] = mlbuf;
	}
	if (get_bool(args, "refs", 0)) { ctx->argv[ctx->argc++] = "--refs";
	}
	if (get_bool(args, "json", 0)) { ctx->argv[ctx->argc++] = "--json";
	}
	if (get_bool(args, "compact", 0)) { ctx->argv[ctx->argc++] = "--compact";
	}
	if (get_bool(args, "full", 0)) { ctx->argv[ctx->argc++] = "--full";
	}
	int limit = get_int(args, "limit", 0);
	if (limit > 0) {
		static char lbuf[32]; snprintf(lbuf, sizeof(lbuf), "%d", limit);
		ctx->argv[ctx->argc++] = "--limit"; ctx->argv[ctx->argc++] = lbuf;
	}
	int offset = get_int(args, "offset", 0);
	if (offset > 0) {
		static char obuf[32]; snprintf(obuf, sizeof(obuf), "%d", offset);
		ctx->argv[ctx->argc++] = "--offset"; ctx->argv[ctx->argc++] = obuf;
	}
	return 1;
}

static int build_argv_query_method(struct ToolContext *ctx, json_object *args)
{
	const char *cls = get_str(args, "class");
	if (!cls) { return 0;
	}
	ctx->argv[ctx->argc++] = "method";
	ctx->argv[ctx->argc++] = (char *)cls;
	const char *method = get_str(args, "method");
	if (method) { ctx->argv[ctx->argc++] = (char *)method;
	}
	const char *file = get_str(args, "file");
	if (file) { ctx->argv[ctx->argc++] = "--file"; ctx->argv[ctx->argc++] = (char *)file; }
	const char *db = get_str(args, "db");
	if (db) { ctx->argv[ctx->argc++] = "--db"; ctx->argv[ctx->argc++] = (char *)db; }
	int pid = get_int(args, "pid", -1);
	if (pid >= 0) {
		static char pidbuf[32];
		snprintf(pidbuf, sizeof(pidbuf), "%d", pid);
		ctx->argv[ctx->argc++] = "--pid"; ctx->argv[ctx->argc++] = pidbuf;
	}
	if (get_bool(args, "body", 0)) { ctx->argv[ctx->argc++] = "--body";
	}
	const char *mlines = get_str(args, "lines");
	if (mlines) { ctx->argv[ctx->argc++] = "--lines"; ctx->argv[ctx->argc++] = (char *)mlines; }
	int mmax = get_int(args, "max_lines", 0);
	if (mmax > 0) {
		static char mmbuf[32]; snprintf(mmbuf, sizeof(mmbuf), "%d", mmax);
		ctx->argv[ctx->argc++] = "--max-lines"; ctx->argv[ctx->argc++] = mmbuf;
	}
	if (get_bool(args, "json", 0)) { ctx->argv[ctx->argc++] = "--json";
	}
	int limit = get_int(args, "limit", 0);
	if (limit > 0) {
		static char lbuf[32]; snprintf(lbuf, sizeof(lbuf), "%d", limit);
		ctx->argv[ctx->argc++] = "--limit"; ctx->argv[ctx->argc++] = lbuf;
	}
	int offset = get_int(args, "offset", 0);
	if (offset > 0) {
		static char obuf[32]; snprintf(obuf, sizeof(obuf), "%d", offset);
		ctx->argv[ctx->argc++] = "--offset"; ctx->argv[ctx->argc++] = obuf;
	}
	return 1;
}

static int build_argv_search_symbols(struct ToolContext *ctx, json_object *args)
{
	const char *q = get_str(args, "query");
	if (!q) { return 0;
	}
	ctx->argv[ctx->argc++] = "search";
	ctx->argv[ctx->argc++] = (char *)q;
	if (get_bool(args, "regex", 0)) { ctx->argv[ctx->argc++] = "--regex";
	}
	const char *kind = get_str(args, "kind");
	if (kind) { ctx->argv[ctx->argc++] = "--kind"; ctx->argv[ctx->argc++] = (char *)kind; }
	const char *file = get_str(args, "file");
	if (file) { ctx->argv[ctx->argc++] = "--file"; ctx->argv[ctx->argc++] = (char *)file; }
	const char *db = get_str(args, "db");
	if (db) { ctx->argv[ctx->argc++] = "--db"; ctx->argv[ctx->argc++] = (char *)db; }
	int pid = get_int(args, "pid", -1);
	if (pid >= 0) {
		static char pidbuf[32];
		snprintf(pidbuf, sizeof(pidbuf), "%d", pid);
		ctx->argv[ctx->argc++] = "--pid"; ctx->argv[ctx->argc++] = pidbuf;
	}
	int limit = get_int(args, "limit", 0);
	if (limit > 0) {
		static char lbuf[32]; snprintf(lbuf, sizeof(lbuf), "%d", limit);
		ctx->argv[ctx->argc++] = "--limit"; ctx->argv[ctx->argc++] = lbuf;
	}
	int offset = get_int(args, "offset", 0);
	if (offset > 0) {
		static char obuf[32]; snprintf(obuf, sizeof(obuf), "%d", offset);
		ctx->argv[ctx->argc++] = "--offset"; ctx->argv[ctx->argc++] = obuf;
	}
	return 1;
}

static int build_argv_list_instances(struct ToolContext *ctx, json_object *args)
{
	ctx->argv[ctx->argc++] = "list";
	if (get_bool(args, "dbsize", 0)) { ctx->argv[ctx->argc++] = "--dbsize";
	}
	return 1;
}

static int build_argv_info(struct ToolContext *ctx, json_object *args)
{
	const char *target = get_str(args, "target");
	if (!target) { target = "all";
	}
	ctx->argv[ctx->argc++] = "info";
	ctx->argv[ctx->argc++] = (char *)target;
	return 1;
}

static int build_argv_reindex(struct ToolContext *ctx, json_object *args)
{
	const char *target = get_str(args, "target");
	if (!target) { target = "all";
	}
	ctx->argv[ctx->argc++] = "reindex";
	ctx->argv[ctx->argc++] = (char *)target;
	return 1;
}

static void format_result_buffer(char **buf)
{
	if (!buf || !*buf) { return;
	}
	size_t blen = strlen(*buf);
	if (blen > 3000) {
		size_t cap = 3000;
		while (cap > 0 && (*buf)[cap] != '\n') { cap--;
		}
		if (cap == 0) { cap = 3000;
		}
		(*buf)[cap] = '\0';
		char tail[128];
		snprintf(tail, sizeof(tail), "\n… (+%zu more bytes; use limit/offset)\n", blen - cap);
		size_t nlen = strlen(*buf);
		char *tmp = realloc(*buf, nlen + strlen(tail) + 1);
		if (tmp) {
			*buf = tmp;
			strcat(*buf, tail);
		}
	}
}

static int is_error_output(int rc, const char *buf)
{
	if (rc != 0) { return 1;
	}
	if (buf && (strstr(buf, "cannot open") || strstr(buf, "no running instance") || strstr(buf, "not found") || strstr(buf, "schema mismatch"))) {
		return 1;
	}
	return 0;
}

static int execute_tool_command(json_object *id, const char *name, struct ToolContext *ctx)
{
	ctx->argv[ctx->argc] = NULL;
	char *buf = NULL;
	size_t bufsize = 0;
	FILE *mem = open_memstream(&buf, &bufsize);
	int rc = 0;
	if (strcmp(name, "query_class") == 0) { rc = class_command(ctx->argc, ctx->argv, mem);
	} else if (strcmp(name, "query_method") == 0) { rc = method_command(ctx->argc, ctx->argv, mem);
	} else if (strcmp(name, "search_symbols") == 0) { rc = search_command(ctx->argc, ctx->argv, mem);
	} else if (strcmp(name, "list_instances") == 0) { rc = list_command(ctx->argc, ctx->argv, mem);
	} else if (strcmp(name, "info") == 0) { rc = info_command(ctx->argc, ctx->argv, mem);
	} else if (strcmp(name, "reindex") == 0) { rc = reindex_command(ctx->argc, ctx->argv);
	}
	fclose(mem);

	if (strcmp(name, "reindex") == 0 && rc == 0 && (!buf || !buf[0])) {
		free(buf);
		buf = strdup("reindex triggered");
	}
	format_result_buffer(&buf);
	int is_error = is_error_output(rc, buf);
	json_object *result = tool_result(buf ? buf : "", is_error);
	send_response(id, result, NULL);
	free(buf);
	return 1;
}

void send_response(json_object *id, json_object *result, json_object *error)
{
	json_object *resp = json_object_new_object();
	json_object_object_add(resp, "jsonrpc", json_object_new_string("2.0"));
	if (id) { json_object_object_add(resp, "id", json_object_get(id));
}
	if (error) { json_object_object_add(resp, "error", error);
	} else { json_object_object_add(resp, "result", result);
}
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

	json_object *s_search = schema_obj("{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"},\"regex\":{\"type\":\"boolean\"},\"kind\":{\"type\":\"string\"},\"file\":{\"type\":\"string\"},\"db\":{\"type\":\"string\"},\"pid\":{\"type\":\"integer\"},\"limit\":{\"type\":\"integer\"},\"offset\":{\"type\":\"integer\"}},\"required\":[\"query\"]}");
	json_object_array_add(tools, make_tool("search_symbols", "Search symbols by name/docstring", s_search));

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

	json_object *s8 = schema_obj("{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"args\":{\"type\":\"object\"}},\"required\":[\"op\"]}");
	json_object_array_add(tools, make_tool("pycool", "Consolidated pycool command (op,args)", s8));

	json_object *result = json_object_new_object();
	json_object_object_add(result, "tools", tools);
	send_response(id, result, NULL);
}

void handle_tools_call(json_object *id, json_object *params)
{
	const char *name = get_str(params, "name");
	json_object *args = NULL;
	json_object_object_get_ex(params, "arguments", &args);
	if (!args) { args = json_object_new_object();
	}

	if (!handle_pycool_meta(args, &name, &args)) {
		send_response(id, NULL, error_response(-32602, "missing op"));
		return;
	}

	if (!name) {
		send_response(id, NULL, error_response(-32602, "missing tool name"));
		return;
	}

	if (handle_index_commands(id, name, args)) { return;
	}

	struct ToolContext ctx = { 0 };
	ctx.argv[ctx.argc++] = (char *)pycoolctl_path();

	if (strcmp(name, "query_class") == 0) {
		if (!validate_required_param(id, args, "name", "missing name")) { return;
		}
		if (!build_argv_query_class(&ctx, args)) { return;
		}
	} else if (strcmp(name, "query_method") == 0) {
		if (!validate_required_param(id, args, "class", "missing class")) { return;
		}
		if (!build_argv_query_method(&ctx, args)) { return;
		}
	} else if (strcmp(name, "search_symbols") == 0) {
		if (!validate_required_param(id, args, "query", "missing query")) { return;
		}
		if (!build_argv_search_symbols(&ctx, args)) { return;
		}
	} else if (strcmp(name, "list_instances") == 0) {
		build_argv_list_instances(&ctx, args);
	} else if (strcmp(name, "info") == 0) {
		build_argv_info(&ctx, args);
	} else if (strcmp(name, "reindex") == 0) {
		build_argv_reindex(&ctx, args);
	} else {
		send_response(id, NULL, error_response(-32601, "unknown tool"));
		return;
	}

	execute_tool_command(id, name, &ctx);
}