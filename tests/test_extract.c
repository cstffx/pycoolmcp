#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <Python.h>
#include "../index/index.h"
#include "scan.h"
#include "ignore.h"
#include "extract.h"

int main(void)
{
	Py_Initialize();

	const char *dbpath = "/tmp/opencode/extract.db";
	unlink(dbpath);
	sqlite3 *db = NULL;
	if (index_open(dbpath, &db) != 0) { fprintf(stderr, "open fail\n"); return 1; }

	const char *srcpath = "/tmp/opencode/extract_test.py";
	FILE *f = fopen(srcpath, "w");
	if (!f) { perror("fopen"); return 1; }
	fputs(
"import typing\n"
"\n"
"@dec\n"
"class Foo(Base, Mixin):\n"
"    \"\"\"doc\"\"\"\n"
"    x = lambda self, a, b=1, *args, c, d=None, **kw: a\n"
"    y = some_call()\n"
"    z = other_name\n"
"    w = obj.method\n"
"    a, (b, [c, d]) = 1, (2, [3, 4])\n"
"\n"
"    def method(self, a: int, b=\"x\", /, c=None, *args: int, d=None, **kw: str) -> str:\n"
"        \"\"\"m\"\"\"\n"
"        pass\n"
"\n"
"    @dec\n    def decorated(self):\n        pass\n\n    async def amethod(self):\n"
"        pass\n"
"\n"
"    u = lambda x: x\n"
"\n"
"class Bar:\n"
"    pass\n"
"\n"
"Foo.bar = lambda self, x: x\n"
"Foo.baz = some_call()\n"
"Foo.qux = other_name\n"
"\n"
"def top_func():\n"
"    pass\n"
"\n"
"x = 1\n"
"y = Foo\n"
, f);
	fclose(f);

	reindex_file(srcpath, db);

	/* scan a directory too */
	char dir[] = "/tmp/opencode/extractdirXXXXXX";
	char *d = mkdtemp(dir);
	char fpath[4096];
	snprintf(fpath, sizeof(fpath), "%s/m.py", d);
	FILE *g = fopen(fpath, "w");
	if (g) { fputs("z = 1\n", g); fclose(g); }
	scan_dir(d, db);


	/* synthetic nodes to cover end_lineno else branches and edge cases */
	sqlite3_int64 synth_fid = 0;
	/* ensure fid variable exists: re-open/upsert if needed */
	{
		sqlite3_int64 tmpfid = 0;
		index_upsert_file(db, srcpath, 2, "hash2", &tmpfid);
		if (tmpfid) synth_fid = tmpfid;
	}
	Ctx ctx = { db, synth_fid };

	/* helper types */
	PyObject *FuncType = PyObject_CallFunction((PyObject*)&PyType_Type, "s(O){}", "FunctionDef", &PyBaseObject_Type);
	PyObject *ClassType = PyObject_CallFunction((PyObject*)&PyType_Type, "s(O){}", "ClassDef", &PyBaseObject_Type);
	PyObject *AssignType = PyObject_CallFunction((PyObject*)&PyType_Type, "s(O){}", "Assign", &PyBaseObject_Type);
	PyObject *NameType = PyObject_CallFunction((PyObject*)&PyType_Type, "s(O){}", "Name", &PyBaseObject_Type);
	PyObject *AttrType = PyObject_CallFunction((PyObject*)&PyType_Type, "s(O){}", "Attribute", &PyBaseObject_Type);
	PyObject *LambdaType = PyObject_CallFunction((PyObject*)&PyType_Type, "s(O){}", "Lambda", &PyBaseObject_Type);
	PyObject *ArgsType = PyObject_CallFunction((PyObject*)&PyType_Type, "s(O){}", "arguments", &PyBaseObject_Type);

	/* method node without end_lineno */
	PyObject *method_node = PyObject_CallObject(FuncType, NULL);
	PyObject_SetAttrString(method_node, "name", PyUnicode_FromString("m"));
	PyObject *args_obj = PyObject_CallObject(ArgsType, NULL);
	PyObject_SetAttrString(args_obj, "posonlyargs", PyList_New(0));
	PyObject_SetAttrString(args_obj, "args", PyList_New(0));
	PyObject_SetAttrString(args_obj, "vararg", Py_None);
	PyObject_SetAttrString(args_obj, "kwonlyargs", PyList_New(0));
	PyObject_SetAttrString(args_obj, "kw_defaults", PyList_New(0));
	PyObject_SetAttrString(args_obj, "defaults", PyList_New(0));
	PyObject_SetAttrString(args_obj, "kwarg", Py_None);
	Py_INCREF(Py_None);
	Py_INCREF(Py_None);
	PyObject_SetAttrString(method_node, "args", args_obj);
	PyObject_SetAttrString(method_node, "body", PyList_New(0));
	PyObject_SetAttrString(method_node, "decorator_list", PyList_New(0));
	PyObject_SetAttrString(method_node, "returns", Py_None);
	Py_INCREF(Py_None);

	/* lambda assign node without end_lineno */
	PyObject *lam = PyObject_CallObject(LambdaType, NULL);
	PyObject_SetAttrString(lam, "args", args_obj);
	PyObject_SetAttrString(lam, "body", Py_None);
	Py_INCREF(Py_None);
	PyObject *lam_assign = PyObject_CallObject(AssignType, NULL);
	PyObject *lam_targets = PyList_New(0);
	PyObject *lam_name = PyObject_CallObject(NameType, NULL);
	PyObject_SetAttrString(lam_name, "id", PyUnicode_FromString("lam"));
	PyList_Append(lam_targets, lam_name);
	PyObject_SetAttrString(lam_assign, "targets", lam_targets);
	PyObject_SetAttrString(lam_assign, "value", lam);

	/* non-lambda assign node without end_lineno */
	PyObject *nonlam_assign = PyObject_CallObject(AssignType, NULL);
	PyObject *nonlam_targets = PyList_New(0);
	PyObject *nonlam_name = PyObject_CallObject(NameType, NULL);
	PyObject_SetAttrString(nonlam_name, "id", PyUnicode_FromString("assignedmeth"));
	PyList_Append(nonlam_targets, nonlam_name);
	PyObject *nonlam_value = PyObject_CallObject(NameType, NULL);
	PyObject_SetAttrString(nonlam_value, "id", Py_None);
	Py_INCREF(Py_None);
	PyObject_SetAttrString(nonlam_assign, "targets", nonlam_targets);
	PyObject_SetAttrString(nonlam_assign, "value", nonlam_value);

	/* class node without end_lineno */
	PyObject *cls_node = PyObject_CallObject(ClassType, NULL);
	PyObject_SetAttrString(cls_node, "bases", PyList_New(0));
	PyObject *body = PyList_New(0);
	PyList_Append(body, method_node);
	PyList_Append(body, lam_assign);
	PyList_Append(body, nonlam_assign);
	PyObject_SetAttrString(cls_node, "body", body);
	PyObject_SetAttrString(cls_node, "decorator_list", PyList_New(0));
	on_node("ClassDef", 1, "Foo", cls_node, &ctx);

	/* top-level ClassDef without end_lineno (already covered by above? on_node called with ClassDef) */
	/* Attribute assigned method to Baz: create class Baz first */
	PyObject *baz_cls = PyObject_CallObject(ClassType, NULL);
	PyObject_SetAttrString(baz_cls, "bases", PyList_New(0));
	PyObject_SetAttrString(baz_cls, "body", PyList_New(0));
	PyObject_SetAttrString(baz_cls, "decorator_list", PyList_New(0));
	on_node("ClassDef", 1, "Baz", baz_cls, &ctx);

	/* Baz assigned method (non-lambda) at module level */
	PyObject *baz_assign = PyObject_CallObject(AssignType, NULL);
	PyObject *baz_targets = PyList_New(0);
	PyObject *baz_attr = PyObject_CallObject(AttrType, NULL);
	PyObject *baz_obj = PyObject_CallObject(NameType, NULL);
	PyObject_SetAttrString(baz_obj, "id", PyUnicode_FromString("Baz"));
	PyObject_SetAttrString(baz_attr, "value", baz_obj);
	PyObject_SetAttrString(baz_attr, "attr", PyUnicode_FromString("m"));
	PyList_Append(baz_targets, baz_attr);
	PyObject *baz_val = PyObject_CallObject(NameType, NULL);
	PyObject_SetAttrString(baz_val, "id", Py_None);
	Py_INCREF(Py_None);
	PyObject_SetAttrString(baz_assign, "targets", baz_targets);
	PyObject_SetAttrString(baz_assign, "value", baz_val);
	on_node("Assign", 2, NULL, baz_assign, &ctx);

	/* Ghost: class symbol without class_details row, then assigned method to trigger no-row finalize */
	sqlite3_int64 ghost_sid = 0;
	index_add_symbol(db, synth_fid, "Ghost", "class", 1, 0, NULL, &ghost_sid);
	PyObject *ghost_assign = PyObject_CallObject(AssignType, NULL);
	PyObject *ghost_targets = PyList_New(0);
	PyObject *ghost_attr = PyObject_CallObject(AttrType, NULL);
	PyObject *ghost_obj = PyObject_CallObject(NameType, NULL);
	PyObject_SetAttrString(ghost_obj, "id", PyUnicode_FromString("Ghost"));
	PyObject_SetAttrString(ghost_attr, "value", ghost_obj);
	PyObject_SetAttrString(ghost_attr, "attr", PyUnicode_FromString("m"));
	PyList_Append(ghost_targets, ghost_attr);
	PyObject *ghost_val = PyObject_CallObject(NameType, NULL);
	PyObject_SetAttrString(ghost_val, "id", Py_None);
	Py_INCREF(Py_None);
	PyObject_SetAttrString(ghost_assign, "targets", ghost_targets);
	PyObject_SetAttrString(ghost_assign, "value", ghost_val);
	on_node("Assign", 3, NULL, ghost_assign, &ctx);

	/* lambda assigned method at module level to Foo (already has class) */
	PyObject *foo_lam_assign = PyObject_CallObject(AssignType, NULL);
	PyObject *foo_lam_targets = PyList_New(0);
	PyObject *foo_lam_attr = PyObject_CallObject(AttrType, NULL);
	PyObject *foo_lam_obj = PyObject_CallObject(NameType, NULL);
	PyObject_SetAttrString(foo_lam_obj, "id", PyUnicode_FromString("Foo"));
	PyObject_SetAttrString(foo_lam_attr, "value", foo_lam_obj);
	PyObject_SetAttrString(foo_lam_attr, "attr", PyUnicode_FromString("lam2"));
	PyList_Append(foo_lam_targets, foo_lam_attr);
	PyObject *foo_lam_val = PyObject_CallObject(LambdaType, NULL);
	PyObject_SetAttrString(foo_lam_val, "args", args_obj);
	PyObject_SetAttrString(foo_lam_val, "body", Py_None);
	Py_INCREF(Py_None);
	PyObject_SetAttrString(foo_lam_assign, "targets", foo_lam_targets);
	PyObject_SetAttrString(foo_lam_assign, "value", foo_lam_val);
	on_node("Assign", 4, NULL, foo_lam_assign, &ctx);

	PyErr_Clear();


	/* non-python file branch */
	reindex_file("/tmp/opencode/notpython.txt", db);

	/* ignored file branch */
	mkdir("/tmp/opencode/igroot", 0755);
	FILE *igf = fopen("/tmp/opencode/igroot/.pycoolignore", "w");
	if (igf) { fputs("*.ignored.py\n", igf); fclose(igf); }
	pycool_root = "/tmp/opencode/igroot";
	pycool_ignore = ignore_load(pycool_root);
	char igpath[4096]; snprintf(igpath, sizeof(igpath), "%s/x.ignored.py", pycool_root);
	FILE *igh = fopen(igpath, "w"); if (igh) { fputs("x=1\n", igh); fclose(igh); }
	reindex_file(igpath, db);
	pycool_root = NULL;
	ignore_free(pycool_ignore);
	pycool_ignore = NULL;

	/* index_upsert_file failure branch: empty db */
	{
		sqlite3 *edb = NULL;
		sqlite3_open("/tmp/opencode/empty_scan.db", &edb);
		reindex_file(srcpath, edb);
		sqlite3_close(edb);
		unlink("/tmp/opencode/empty_scan.db");
	}

	/* scan_dir: opendir failure (not ENOENT) */
	scan_dir("/tmp/opencode/regular_file_for_scan", db);

	/* scan_dir: create a regular file with that name first for opendir fail? use a file */
	{
		FILE *rf = fopen("/tmp/opencode/regular_file_for_scan", "w"); if (rf) { fputc('x', rf); fclose(rf); }
		scan_dir("/tmp/opencode/regular_file_for_scan", db);
		unlink("/tmp/opencode/regular_file_for_scan");
	}

	/* scan_dir: recursion + non-file entry */
	{
		char sdir[] = "/tmp/opencode/scandirXXXXXX";
		char *sd = mkdtemp(sdir);
		char sub[4096]; snprintf(sub, sizeof(sub), "%s/sub", sd);
		mkdir(sub, 0755);
		char fpath[4096]; snprintf(fpath, sizeof(fpath), "%s/m.py", sd);
		FILE *g = fopen(fpath, "w"); if (g) { fputs("z = 1\n", g); fclose(g); }
		/* fifo for else branch */
		char fifo[4096]; snprintf(fifo, sizeof(fifo), "%s/q.fifo", sd);
		mkfifo(fifo, 0644);
		scan_dir(sd, db);
		unlink(fifo); unlink(fpath); rmdir(sub); rmdir(sd);
	}

	index_close(db);
	Py_Finalize();
	return 0;
}
