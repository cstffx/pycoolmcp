CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -I/usr/include/python3.13 -Isrc -Iindex -Ipycoolctl
LDFLAGS ?= -lpython3.13 -lsqlite3

SRC := src
INDEX := index
CLI := pycoolctl
TESTS := tests

SRCS := $(SRC)/pycool.c $(SRC)/extract.c $(SRC)/scan.c $(SRC)/ast.c $(SRC)/traverse.c \
        $(SRC)/ignore.c $(SRC)/registry.c $(SRC)/observer.c

OBJS := $(SRC)/ast.o $(SRC)/traverse.o $(INDEX)/core.o $(INDEX)/class.o $(INDEX)/method.o \
        $(SRC)/observer.o $(SRC)/registry.o $(SRC)/ignore.o

all: libast.a pycool bin/pycoolctl

libast.a: $(OBJS)
	ar rcs $@ $(OBJS)

$(SRC)/ast.o: $(SRC)/ast.c $(SRC)/ast.h
	$(CC) $(CFLAGS) -c $(SRC)/ast.c -o $(SRC)/ast.o

$(SRC)/traverse.o: $(SRC)/traverse.c $(SRC)/traverse.h $(SRC)/ast.h
	$(CC) $(CFLAGS) -c $(SRC)/traverse.c -o $(SRC)/traverse.o

$(INDEX)/core.o: $(INDEX)/core.c $(INDEX)/core.h $(INDEX)/class.h $(INDEX)/method.h
	mkdir -p $(INDEX)
	$(CC) $(CFLAGS) -c $(INDEX)/core.c -o $(INDEX)/core.o

$(INDEX)/class.o: $(INDEX)/class.c $(INDEX)/class.h
	mkdir -p $(INDEX)
	$(CC) $(CFLAGS) -c $(INDEX)/class.c -o $(INDEX)/class.o

$(INDEX)/method.o: $(INDEX)/method.c $(INDEX)/method.h
	mkdir -p $(INDEX)
	$(CC) $(CFLAGS) -c $(INDEX)/method.c -o $(INDEX)/method.o

$(SRC)/observer.o: $(SRC)/observer.c $(SRC)/observer.h
	$(CC) $(CFLAGS) -c $(SRC)/observer.c -o $(SRC)/observer.o

$(SRC)/registry.o: $(SRC)/registry.c $(SRC)/registry.h
	$(CC) $(CFLAGS) -c $(SRC)/registry.c -o $(SRC)/registry.o

$(SRC)/ignore.o: $(SRC)/ignore.c $(SRC)/ignore.h
	$(CC) $(CFLAGS) -c $(SRC)/ignore.c -o $(SRC)/ignore.o

pycool: $(SRCS) libast.a
	$(CC) $(CFLAGS) -o $@ $(SRCS) libast.a $(LDFLAGS)

bin/pycoolctl: $(CLI)/main.c $(CLI)/list.c $(CLI)/info.c $(CLI)/class.c $(CLI)/method.c $(CLI)/stop.c $(CLI)/clean.c $(CLI)/reindex.c $(SRC)/registry.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(CLI)/main.c $(CLI)/list.c $(CLI)/info.c $(CLI)/class.c $(CLI)/method.c $(CLI)/stop.c $(CLI)/clean.c $(CLI)/reindex.c $(SRC)/registry.o -lsqlite3

test_ast: $(TESTS)/test_ast.c $(SRC)/ast.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_ast.c $(SRC)/ast.o $(LDFLAGS)

test_traverse: $(TESTS)/test_traverse.c $(SRC)/ast.o $(SRC)/traverse.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_traverse.c $(SRC)/ast.o $(SRC)/traverse.o $(LDFLAGS)

test_index: $(TESTS)/test_index.c $(INDEX)/core.o
	$(CC) $(CFLAGS) -o $@ $(TESTS)/test_index.c $(INDEX)/core.o -lsqlite3

http: src/http.c
	$(CC) $(CFLAGS) -o http src/http.c

clean:
	rm -f *.o *.a test_ast test_traverse test_index pycool http
	rm -rf bin $(SRC)/*.o $(INDEX)/*.o

release:
	$(MAKE) clean
	$(MAKE) CFLAGS="-std=c11 -O3 -Wall -Wextra -I/usr/include/python3.13 -Isrc -Iindex -Ipycoolctl" all

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

install: release
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 pycool $(DESTDIR)$(BINDIR)/pycool
	install -m 0755 bin/pycoolctl $(DESTDIR)$(BINDIR)/pycoolctl

.PHONY: all clean release install