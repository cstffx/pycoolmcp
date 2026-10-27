CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -I/usr/include/python3.13 -Isrc -Iindex -Ipycoolctl
LDFLAGS ?= -lpython3.13 -lsqlite3

SRC := src
INDEX := index
CLI := pycoolctl
TESTS := tests
HTTP := http
BUILD := build

# Library sources
LIB_SRCS := $(SRC)/ast.c $(SRC)/traverse.c $(SRC)/extract.c $(SRC)/scan.c \
            $(SRC)/ignore.c $(SRC)/observer.c $(SRC)/registry.c $(SRC)/pycool.c \
            $(SRC)/dbpath.c \
            $(SRC)/query_class.c $(SRC)/query_method.c $(SRC)/query_list.c $(SRC)/query_info.c \
            $(SRC)/query_reindex.c $(SRC)/query_search.c \
            $(INDEX)/core.c $(INDEX)/class.c $(INDEX)/method.c

LIB_OBJS := $(LIB_SRCS:%.c=$(BUILD)/%.o)

.PHONY: all clean test pipeline
all: $(BUILD)/pycool $(BUILD)/bin/pycoolctl $(BUILD)/http_server $(BUILD)/pycoolmcp $(BUILD)/libpycool.a $(BUILD)/libpycool.so

# --- generic object rule ---
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# --- library ---
$(BUILD)/libpycool.a: $(LIB_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $(LIB_OBJS)

$(BUILD)/libpycool.so $(BUILD)/libpycool.so.0: $(LIB_SRCS)
	@mkdir -p $(dir $@)
	$(CC) -shared -fPIC $(CFLAGS) -Wl,-soname,libpycool.so.0 -o $(BUILD)/libpycool.so.0 $(LIB_SRCS) $(LDFLAGS)
	ln -sf libpycool.so.0 $(BUILD)/libpycool.so

# --- daemon ---
$(BUILD)/pycool: $(SRC)/pycool_main.c $(BUILD)/libpycool.a
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(SRC)/pycool_main.c $(BUILD)/libpycool.a $(LDFLAGS)

# --- cli ---
$(BUILD)/bin/pycoolctl: $(CLI)/main.c $(SRC)/query_list.c $(SRC)/query_info.c $(SRC)/query_class.c $(SRC)/query_method.c $(SRC)/query_search.c $(CLI)/stop.c $(CLI)/clean.c $(SRC)/query_reindex.c $(BUILD)/libpycool.a
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(CLI)/main.c $(SRC)/query_list.c $(SRC)/query_info.c $(SRC)/query_class.c $(SRC)/query_method.c $(SRC)/query_search.c $(CLI)/stop.c $(CLI)/clean.c $(SRC)/query_reindex.c $(BUILD)/libpycool.a -lsqlite3

# --- http server ---
$(BUILD)/http_server: $(HTTP)/http.c mcp/http_app.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(HTTP)/http.c mcp/http_app.c

# --- mcp server ---
MCP_CFLAGS := $(CFLAGS) $(shell pkg-config --cflags json-c 2>/dev/null)
$(BUILD)/pycoolmcp: mcp/mcp_server.c mcp/tools.c mcp/index_manager.c $(BUILD)/libpycool.so
	@mkdir -p $(dir $@)
	$(CC) $(MCP_CFLAGS) -I mcp -o $@ mcp/mcp_server.c mcp/tools.c mcp/index_manager.c -L$(BUILD) -lpycool -ljson-c -lpython3.13 -lsqlite3 -Wl,-rpath,'$$ORIGIN' -Wl,-rpath,'$$ORIGIN/../lib'

# --- tests ---
TEST_BINS := $(BUILD)/test_ast $(BUILD)/test_traverse $(BUILD)/test_index $(BUILD)/test_dbpath $(BUILD)/test_info $(BUILD)/test_list $(BUILD)/test_reindex $(BUILD)/test_clean $(BUILD)/test_stop $(BUILD)/test_extract $(BUILD)/test_ignore $(BUILD)/test_observer $(BUILD)/test_class $(BUILD)/test_method $(BUILD)/test_registry $(BUILD)/test_overlap $(BUILD)/test_incremental

$(BUILD)/test_ast: $(TESTS)/test_ast.c $(BUILD)/src/ast.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_ast.c $(BUILD)/src/ast.o $(LDFLAGS)

$(BUILD)/test_traverse: $(TESTS)/test_traverse.c $(BUILD)/src/ast.o $(BUILD)/src/traverse.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_traverse.c $(BUILD)/src/ast.o $(BUILD)/src/traverse.o $(LDFLAGS)

$(BUILD)/test_index: $(TESTS)/test_index.c $(BUILD)/index/core.o $(BUILD)/index/class.o $(BUILD)/index/method.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_index.c $(BUILD)/index/core.o $(BUILD)/index/class.o $(BUILD)/index/method.o -lsqlite3

$(BUILD)/test_dbpath: $(TESTS)/test_dbpath.c $(BUILD)/src/dbpath.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_dbpath.c $(BUILD)/src/dbpath.o

$(BUILD)/test_reindex: $(TESTS)/test_reindex.c $(BUILD)/src/query_reindex.o $(BUILD)/src/registry.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_reindex.c $(BUILD)/src/query_reindex.o $(BUILD)/src/registry.o

$(BUILD)/test_list: $(TESTS)/test_list.c $(BUILD)/src/query_list.o $(BUILD)/src/registry.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_list.c $(BUILD)/src/query_list.o $(BUILD)/src/registry.o

$(BUILD)/test_info: $(TESTS)/test_info.c $(BUILD)/src/query_info.o $(BUILD)/src/registry.o $(BUILD)/index/core.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_info.c $(BUILD)/src/query_info.o $(BUILD)/src/registry.o $(BUILD)/index/core.o -lsqlite3

$(BUILD)/test_clean: $(TESTS)/test_clean.c pycoolctl/clean.c $(BUILD)/src/registry.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_clean.c pycoolctl/clean.c $(BUILD)/src/registry.o $(LDFLAGS)

$(BUILD)/test_stop: $(TESTS)/test_stop.c pycoolctl/stop.c $(BUILD)/src/registry.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_stop.c pycoolctl/stop.c $(BUILD)/src/registry.o $(LDFLAGS)

$(BUILD)/test_extract: $(TESTS)/test_extract.c $(BUILD)/src/extract.o $(BUILD)/src/scan.o $(BUILD)/src/ast.o $(BUILD)/src/traverse.o $(BUILD)/src/ignore.o $(BUILD)/src/registry.o $(BUILD)/src/observer.o $(BUILD)/src/pycool.o $(BUILD)/index/core.o $(BUILD)/index/class.o $(BUILD)/index/method.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Iindex -o $@ $(TESTS)/test_extract.c $(BUILD)/src/extract.o $(BUILD)/src/scan.o $(BUILD)/src/ast.o $(BUILD)/src/traverse.o $(BUILD)/src/ignore.o $(BUILD)/src/registry.o $(BUILD)/src/observer.o $(BUILD)/src/pycool.o $(BUILD)/index/core.o $(BUILD)/index/class.o $(BUILD)/index/method.o $(LDFLAGS) -lpthread -ldl -lutil -lm

$(BUILD)/test_ignore: $(TESTS)/test_ignore.c $(BUILD)/src/ignore.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_ignore.c $(BUILD)/src/ignore.o $(LDFLAGS)

$(BUILD)/test_observer: $(TESTS)/test_observer.c $(BUILD)/src/observer.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -o $@ $(TESTS)/test_observer.c $(BUILD)/src/observer.o -lpthread $(LDFLAGS)

$(BUILD)/test_class: $(TESTS)/test_class.c $(SRC)/query_class.c $(BUILD)/src/dbpath.o $(BUILD)/src/registry.o $(BUILD)/index/core.o $(BUILD)/index/class.o $(BUILD)/index/method.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Iindex -Ipycoolctl -o $@ $(TESTS)/test_class.c $(SRC)/query_class.c $(BUILD)/src/dbpath.o $(BUILD)/src/registry.o $(BUILD)/index/core.o $(BUILD)/index/class.o $(BUILD)/index/method.o --coverage -lsqlite3 -lpython3.13

$(BUILD)/test_method: $(TESTS)/test_method.c $(SRC)/query_method.c $(BUILD)/src/dbpath.o $(BUILD)/src/registry.o $(BUILD)/index/core.o $(BUILD)/index/class.o $(BUILD)/index/method.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -Iindex -Ipycoolctl -o $@ $(TESTS)/test_method.c $(SRC)/query_method.c $(BUILD)/src/dbpath.o $(BUILD)/src/registry.o $(BUILD)/index/core.o $(BUILD)/index/class.o $(BUILD)/index/method.o --coverage -lsqlite3 -lpython3.13

$(BUILD)/test_registry: $(TESTS)/test_registry.c $(BUILD)/src/registry.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Isrc -o $@ $(TESTS)/test_registry.c $(BUILD)/src/registry.o --coverage -lpthread

$(BUILD)/test_overlap: $(TESTS)/test_overlap.c $(BUILD)/src/registry.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_overlap.c $(BUILD)/src/registry.o $(LDFLAGS)

$(BUILD)/test_incremental: $(TESTS)/test_incremental.c $(BUILD)/libpycool.a
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_incremental.c $(BUILD)/libpycool.a $(LDFLAGS) -lpthread -ldl -lutil -lm

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "RUN $$t"; $$t >/tmp/opencode/$$(basename $$t).out 2>&1 || { echo "FAIL $$t"; cat /tmp/opencode/$$(basename $$t).out; exit 1; }; done
	@echo "ALL TESTS PASSED"

clean:
	rm -rf $(BUILD)

pipeline: cppcheck build test clean
	@echo "Pipeline OK - ready to push"

cppcheck:
	cppcheck --enable=all --inconclusive --inline-suppr --quiet --suppress=missingInclude --suppress=missingIncludeSystem --suppress=checkLevelNormal --suppress=checkersReport --suppress=normalCheckLevelMaxBranches --suppress=unmatchedSuppression src/ index/ mcp/ pycoolctl/ http/ 2>&1 | tee cppcheck.txt
	test ! -s cppcheck.txt

lint:
	clang-tidy -checks=-*,readability-*,modernize-* src/*.c index/*.c mcp/*.c pycoolctl/*.c http/*.c -- $(CFLAGS) 2>&1 | tee lint.txt
	test ! -s lint.txt || echo "Lint warnings found"

build: all

# Deep analysis targets (optional)
clang-analyze:
	scan-build $(CC) $(CFLAGS) -o /tmp/scan_build_out $(SRC)/pycool_main.c $(LIB_SRCS) $(LDFLAGS) 2>/dev/null || true
	@echo "Clang Static Analyzer results available in scan-build output"

.PHONY: all clean test pipeline cppcheck build clang-analyze
