CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -I/usr/include/python3.13 -Isrc -Iindex -Ipycoolctl
LDFLAGS ?= -lpython3.13 -lsqlite3

SRC := src
INDEX := index
CLI := pycoolctl
TESTS := tests
HTTP := http

# Library sources (core indexer + SQLite index layer + registry)
LIB_SRCS := $(SRC)/ast.c $(SRC)/traverse.c $(SRC)/extract.c $(SRC)/scan.c \
            $(SRC)/ignore.c $(SRC)/observer.c $(SRC)/registry.c $(SRC)/pycool.c \
            $(SRC)/dbpath.c \
            $(SRC)/query_class.c $(SRC)/query_method.c $(SRC)/query_list.c $(SRC)/query_info.c \
            $(SRC)/query_reindex.c $(SRC)/query_search.c \
            $(INDEX)/core.c $(INDEX)/class.c $(INDEX)/method.c

LIB_OBJS := $(LIB_SRCS:.c=.o)

.PHONY: all
all: libpycool.a libpycool.so pycool bin/pycoolctl http_server pycoolmcp

lint:
	clang-tidy src/*.c index/*.c mcp/*.c http/*.c pycoolctl/*.c -- $(CFLAGS) 2>&1 | tee lint.txt

# --- library ---
libpycool.a: $(LIB_OBJS)
	ar rcs $@ $(LIB_OBJS)

libpycool.so libpycool.so.0: $(LIB_SRCS)
	$(CC) -shared -fPIC $(CFLAGS) -Wl,-soname,libpycool.so.0 -o libpycool.so.0 $(LIB_SRCS) $(LDFLAGS)
	ln -sf libpycool.so.0 libpycool.so

# object rules
$(SRC)/ast.o: $(SRC)/ast.c $(SRC)/ast.h
	$(CC) $(CFLAGS) -c $(SRC)/ast.c -o $(SRC)/ast.o

$(SRC)/traverse.o: $(SRC)/traverse.c $(SRC)/traverse.h $(SRC)/ast.h
	$(CC) $(CFLAGS) -c $(SRC)/traverse.c -o $(SRC)/traverse.o

$(SRC)/extract.o: $(SRC)/extract.c $(SRC)/extract.h
	$(CC) $(CFLAGS) -c $(SRC)/extract.c -o $(SRC)/extract.o

$(SRC)/scan.o: $(SRC)/scan.c $(SRC)/scan.h
	$(CC) $(CFLAGS) -c $(SRC)/scan.c -o $(SRC)/scan.o

$(SRC)/ignore.o: $(SRC)/ignore.c $(SRC)/ignore.h
	$(CC) $(CFLAGS) -c $(SRC)/ignore.c -o $(SRC)/ignore.o

$(SRC)/observer.o: $(SRC)/observer.c $(SRC)/observer.h
	$(CC) $(CFLAGS) -c $(SRC)/observer.c -o $(SRC)/observer.o

$(SRC)/registry.o: $(SRC)/registry.c $(SRC)/registry.h
	$(CC) $(CFLAGS) -c $(SRC)/registry.c -o $(SRC)/registry.o

$(SRC)/pycool.o: $(SRC)/pycool.c $(SRC)/pycool.h
	$(CC) $(CFLAGS) -c $(SRC)/pycool.c -o $(SRC)/pycool.o

$(SRC)/dbpath.o: $(SRC)/dbpath.c $(SRC)/dbpath.h
	$(CC) $(CFLAGS) -c $(SRC)/dbpath.c -o $(SRC)/dbpath.o

$(SRC)/query_class.o: $(SRC)/query_class.c pycoolctl/class.h
	$(CC) $(CFLAGS) -c $(SRC)/query_class.c -o $(SRC)/query_class.o

$(SRC)/query_method.o: $(SRC)/query_method.c pycoolctl/method.h
	$(CC) $(CFLAGS) -c $(SRC)/query_method.c -o $(SRC)/query_method.o

$(SRC)/query_list.o: $(SRC)/query_list.c pycoolctl/list.h
	$(CC) $(CFLAGS) -c $(SRC)/query_list.c -o $(SRC)/query_list.o

$(SRC)/query_info.o: $(SRC)/query_info.c pycoolctl/info.h
	$(CC) $(CFLAGS) -c $(SRC)/query_info.c -o $(SRC)/query_info.o

$(SRC)/query_reindex.o: $(SRC)/query_reindex.c pycoolctl/reindex.h
	$(CC) $(CFLAGS) -c $(SRC)/query_reindex.c -o $(SRC)/query_reindex.o

$(INDEX)/core.o: $(INDEX)/core.c $(INDEX)/core.h $(INDEX)/class.h $(INDEX)/method.h
	mkdir -p $(INDEX)
	$(CC) $(CFLAGS) -c $(INDEX)/core.c -o $(INDEX)/core.o

$(INDEX)/class.o: $(INDEX)/class.c $(INDEX)/class.h
	mkdir -p $(INDEX)
	$(CC) $(CFLAGS) -c $(INDEX)/class.c -o $(INDEX)/class.o

$(INDEX)/method.o: $(INDEX)/method.c $(INDEX)/method.h
	mkdir -p $(INDEX)
	$(CC) $(CFLAGS) -c $(INDEX)/method.c -o $(INDEX)/method.o

# --- pycool daemon ---
pycool: $(SRC)/pycool_main.c libpycool.a
	$(CC) $(CFLAGS) -o $@ $(SRC)/pycool_main.c libpycool.a $(LDFLAGS)

# --- pycoolctl cli ---
PYCOOLCTL_SRCS := $(CLI)/main.c $(SRC)/query_list.c $(SRC)/query_info.c $(SRC)/query_class.c $(SRC)/query_method.c $(SRC)/query_search.c \
                  $(CLI)/stop.c $(CLI)/clean.c $(SRC)/query_reindex.c

bin/pycoolctl: $(PYCOOLCTL_SRCS) libpycool.a
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(PYCOOLCTL_SRCS) libpycool.a -lsqlite3

# --- http server ---
http_server: $(HTTP)/http.c mcp/http_app.c
	$(CC) $(CFLAGS) -o http_server $(HTTP)/http.c mcp/http_app.c

# --- mcp server (stdio, JSON-RPC) ---
MCP_CFLAGS := $(CFLAGS) $(shell pkg-config --cflags json-c 2>/dev/null)
pycoolmcp: mcp/mcp_server.c mcp/tools.c mcp/index_manager.c libpycool.so
	$(CC) $(MCP_CFLAGS) -I mcp -o pycoolmcp mcp/mcp_server.c mcp/tools.c mcp/index_manager.c -L. -lpycool -ljson-c -lpython3.13 -lsqlite3 -Wl,-rpath,'$$ORIGIN' -Wl,-rpath,'$$ORIGIN/../lib' 

# --- tests ---
test_ast: $(TESTS)/test_ast.c $(SRC)/ast.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_ast.c $(SRC)/ast.o $(LDFLAGS)

test_traverse: $(TESTS)/test_traverse.c $(SRC)/ast.o $(SRC)/traverse.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_traverse.c $(SRC)/ast.o $(SRC)/traverse.o $(LDFLAGS)

test_index: $(TESTS)/test_index.c $(INDEX)/core.o $(INDEX)/class.o $(INDEX)/method.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_index.c $(INDEX)/core.o $(INDEX)/class.o $(INDEX)/method.o -lsqlite3

test_dbpath: $(TESTS)/test_dbpath.c $(SRC)/dbpath.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_dbpath.c $(SRC)/dbpath.o

test_reindex: $(TESTS)/test_reindex.c $(SRC)/query_reindex.o $(SRC)/registry.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_reindex.c $(SRC)/query_reindex.o $(SRC)/registry.o

test_list: $(TESTS)/test_list.c $(SRC)/query_list.o $(SRC)/registry.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_list.c $(SRC)/query_list.o $(SRC)/registry.o

test_info: $(TESTS)/test_info.c $(SRC)/query_info.o $(SRC)/registry.o $(INDEX)/core.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_info.c $(SRC)/query_info.o $(SRC)/registry.o $(INDEX)/core.o -lsqlite3

test_overlap: $(TESTS)/test_overlap.c $(SRC)/registry.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_overlap.c $(SRC)/registry.o $(LDFLAGS)

test_ignore: $(TESTS)/test_ignore.c $(SRC)/ignore.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_ignore.c $(SRC)/ignore.o $(LDFLAGS)

test_incremental: $(TESTS)/test_incremental.c libpycool.a
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_incremental.c libpycool.a $(LDFLAGS) -lpthread -ldl -lutil -lm

test_clean: $(TESTS)/test_clean.c pycoolctl/clean.c $(SRC)/registry.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_clean.c pycoolctl/clean.c $(SRC)/registry.o $(LDFLAGS)

test_stop: $(TESTS)/test_stop.c pycoolctl/stop.c $(SRC)/registry.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_stop.c pycoolctl/stop.c $(SRC)/registry.o $(LDFLAGS)

test_extract: $(TESTS)/test_extract.c $(SRC)/extract.o $(SRC)/scan.o $(SRC)/ast.o $(SRC)/traverse.o $(SRC)/ignore.o $(SRC)/registry.o $(SRC)/observer.o $(SRC)/pycool.o $(INDEX)/core.o $(INDEX)/class.o $(INDEX)/method.o
	$(CC) $(CFLAGS) -Isrc -Iindex -o $@ $(TESTS)/test_extract.c $(SRC)/extract.o $(SRC)/scan.o $(SRC)/ast.o $(SRC)/traverse.o $(SRC)/ignore.o $(SRC)/registry.o $(SRC)/observer.o $(SRC)/pycool.o $(INDEX)/core.o $(INDEX)/class.o $(INDEX)/method.o $(LDFLAGS) -lpthread -ldl -lutil -lm

test_observer: $(TESTS)/test_observer.c $(SRC)/observer.o
	$(CC) $(CFLAGS) -Isrc -o $@ $(TESTS)/test_observer.c $(SRC)/observer.o -lpthread $(LDFLAGS)

test_class: $(TESTS)/test_class.c $(SRC)/query_class.c $(SRC)/dbpath.o $(SRC)/registry.o $(INDEX)/core.o $(INDEX)/class.o $(INDEX)/method.o
	$(CC) $(CFLAGS) -Isrc -Iindex -Ipycoolctl -o $@ $(TESTS)/test_class.c $(SRC)/query_class.c $(SRC)/dbpath.o $(SRC)/registry.o $(INDEX)/core.o $(INDEX)/class.o $(INDEX)/method.o --coverage -lsqlite3 -lpython3.13

test_method: $(TESTS)/test_method.c $(SRC)/query_method.c $(SRC)/dbpath.o $(SRC)/registry.o $(INDEX)/core.o $(INDEX)/class.o $(INDEX)/method.o
	$(CC) $(CFLAGS) -Isrc -Iindex -Ipycoolctl -o $@ $(TESTS)/test_method.c $(SRC)/query_method.c $(SRC)/dbpath.o $(SRC)/registry.o $(INDEX)/core.o $(INDEX)/class.o $(INDEX)/method.o --coverage -lsqlite3 -lpython3.13

test_registry: $(TESTS)/test_registry.c $(SRC)/registry.o
	$(CC) $(CFLAGS) -Isrc -o $@ $(TESTS)/test_registry.c $(SRC)/registry.o --coverage -lpthread

test: test_ast test_traverse test_index test_dbpath test_info test_list test_reindex test_clean test_stop test_extract test_ignore test_observer test_class test_method test_registry test_overlap test_incremental
	@for t in test_ast test_traverse test_index test_dbpath test_info test_list test_reindex test_clean test_stop test_extract test_ignore test_observer test_class test_method test_registry test_overlap test_incremental; do \
		echo "RUN $$t"; \
		./$$t >/tmp/opencode/$$t.out 2>&1 || { echo "FAIL $$t"; cat /tmp/opencode/$$t.out; exit 1; }; \
	done
	@echo "ALL TESTS PASSED"

clean:
	rm -f *.o *.a *.so *.so.* test_ast test_traverse test_index test_dbpath test_reindex test_list test_info test_overlap test_ignore test_incremental test_clean test_stop test_extract test_observer test_class test_method test_registry pycool http_server pycoolmcp *.gcda *.gcno *.gcov
	rm -rf bin $(SRC)/*.o $(INDEX)/*.o $(SRC)/*.gcda $(SRC)/*.gcno $(INDEX)/*.gcda $(INDEX)/*.gcno

release:
	$(MAKE) clean
	$(MAKE) CFLAGS="-std=c11 -O3 -Wall -Wextra -I/usr/include/python3.13 -Isrc -Iindex -Ipycoolctl" all

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
DESTDIR ?=

install: all
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(INCLUDEDIR)/pycool
	install -m 0755 pycool $(DESTDIR)$(BINDIR)/pycool
	install -m 0755 bin/pycoolctl $(DESTDIR)$(BINDIR)/pycoolctl
	install -m 0755 pycoolmcp $(DESTDIR)$(BINDIR)/pycoolmcp
	install -m 0755 http_server $(DESTDIR)$(BINDIR)/http_server
	install -m 0644 libpycool.so.0 $(DESTDIR)$(LIBDIR)/libpycool.so.0
	ln -sf libpycool.so.0 $(DESTDIR)$(LIBDIR)/libpycool.so
	install -m 0644 src/pycool.h $(DESTDIR)$(INCLUDEDIR)/pycool/pycool.h
	install -m 0644 http/http.h $(DESTDIR)$(INCLUDEDIR)/pycool/http.h
	@if [ -z "$(DESTDIR)" ]; then ldconfig || true; fi

.PHONY: all clean release install
all: libpycool.a pycool bin/pycoolctl pycoolmcp