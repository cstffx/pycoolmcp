#define _GNU_SOURCE
#include "extract.h"
#include "ast.h"
#include "../index/index.h"
#include "ignore.h"
#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// cppcheck-suppress staticFunction
void extract_assign_targets(PyObject *target, Ctx *ctx, int line)
{
	if (!target) { return;
}
	PyObject *cls = PyObject_GetAttrString(target, "__class__");
	PyObject *cname = cls ? PyObject_GetAttrString(cls, "__name__") : NULL;
	const char *tname = cname ? PyUnicode_AsUTF8(cname) : NULL;
	if (tname && strcmp(tname, "Name") == 0) {
		PyObject *id = PyObject_GetAttrString(target, "id");
		if (id) {
			const char *s = PyUnicode_AsUTF8(id);
			if (s) {
				sqlite3_int64 sid;
				index_add_symbol(ctx->db, ctx->file_id, s, "variable", line, 0, NULL, &sid);
			}
			Py_DECREF(id);
		}
	} else if (tname && (strcmp(tname, "Tuple") == 0 || strcmp(tname, "List") == 0)) {
		PyObject *elts = PyObject_GetAttrString(target, "elts");
		if (elts && PyList_Check(elts)) {
			Py_ssize_t n = PyList_Size(elts);
			for (Py_ssize_t i = 0; i < n; i++) {
				extract_assign_targets(PyList_GetItem(elts, i), ctx, line);
			}
		}
		Py_XDECREF(elts);
	}
	Py_XDECREF(cname);
	Py_XDECREF(cls);
	PyErr_Clear();
}

void on_node(const char *type, int lineno, const char *name, void *node_ptr, void *ud)
{
	Ctx *ctx = ud;
	PyObject *node = node_ptr;
	sqlite3_int64 sid;
	if (strcmp(type, "ClassDef") == 0) {
		char *bases = NULL;
		char *methods = NULL;
		char *docstring = NULL;
		char *decorators = NULL;
		sid = 0;
		if (name) { index_add_symbol(ctx->db, ctx->file_id, name, "class", lineno, 0, NULL, &sid);
}
		PyObject *ast_mod = PyImport_ImportModule("ast");
		if (ast_mod && node) {
			/* bases */
			PyObject *bases_obj = PyObject_GetAttrString(node, "bases");
			if (bases_obj && PyList_Check(bases_obj)) {
				size_t cap = 256;
				size_t len = 0;
				bases = malloc(cap);
				bases[0] = '\0';
				Py_ssize_t nb = PyList_Size(bases_obj);
				for (Py_ssize_t i = 0; i < nb; i++) {
					PyObject *b = PyList_GetItem(bases_obj, i);
					PyObject *u = NULL;
					PyObject *unparse = PyObject_GetAttrString(ast_mod, "unparse");
					if (unparse) { u = PyObject_CallFunction(unparse, "(O)", b); Py_DECREF(unparse); }
					if (!u) { u = PyObject_Str(b);
}
					if (u) {
						const char *bs = PyUnicode_AsUTF8(u);
						if (bs) {
							size_t add = strlen(bs) + 2;
							if (len + add >= cap) { cap = (len+add)*2; char *tmp = realloc(bases, cap); if (tmp) { bases = tmp; 
}}
							if (len > 0) { bases[len++] = ',';
}
							strcpy(bases + len, bs);
							len += strlen(bs);
						}
						Py_DECREF(u);
					}
				}
				if (bases) { bases[len] = '\0';
}
			}
			Py_XDECREF(bases_obj);
			/* methods */
			PyObject *body = PyObject_GetAttrString(node, "body");
			if (body && PyList_Check(body)) {
				size_t cap = 256;
				size_t len = 0;
				methods = malloc(cap);
				methods[0] = '\0';
				Py_ssize_t nb = PyList_Size(body);
				for (Py_ssize_t bi = 0; bi < nb; bi++) {
					PyObject *item = PyList_GetItem(body, bi);
					PyObject *icls = PyObject_GetAttrString(item, "__class__");
					PyObject *iname = icls ? PyObject_GetAttrString(icls, "__name__") : NULL;
					const char *cn = iname ? PyUnicode_AsUTF8(iname) : NULL;
					if (cn && (strcmp(cn, "FunctionDef") == 0 || strcmp(cn, "AsyncFunctionDef") == 0)) {
						PyObject *mname = PyObject_GetAttrString(item, "name");
						const char *ms = mname ? PyUnicode_AsUTF8(mname) : NULL;
						if (ms) {
							int is_async = (strcmp(cn, "AsyncFunctionDef") == 0);
							PyObject *args_obj = PyObject_GetAttrString(item, "args");
							PyObject *returns_obj = PyObject_GetAttrString(item, "returns");
							PyObject *decs_obj = PyObject_GetAttrString(item, "decorator_list");
							PyObject *unparse_fn = ast_mod ? PyObject_GetAttrString(ast_mod, "unparse") : NULL;
							PyObject *args_str = (unparse_fn && args_obj) ? PyObject_CallFunction(unparse_fn, "(O)", args_obj) : NULL;
							PyObject *ret_str = (unparse_fn && returns_obj && returns_obj != Py_None) ? PyObject_CallFunction(unparse_fn, "(O)", returns_obj) : NULL;
							const char *args_s = (args_str && PyUnicode_Check(args_str)) ? PyUnicode_AsUTF8(args_str) : "";
							const char *ret_s = (ret_str && PyUnicode_Check(ret_str)) ? PyUnicode_AsUTF8(ret_str) : NULL;

							char dec_prefix[512];
							dec_prefix[0] = '\0';
							if (decs_obj && PyList_Check(decs_obj) && unparse_fn) {
								Py_ssize_t nd = PyList_Size(decs_obj);
								size_t dlen = 0;
								for (Py_ssize_t di = 0; di < nd; di++) {
									PyObject *dec = PyList_GetItem(decs_obj, di);
									PyObject *u = PyObject_CallFunction(unparse_fn, "(O)", dec);
									const char *ds = u ? (PyUnicode_Check(u) ? PyUnicode_AsUTF8(u) : NULL) : NULL;
									if (ds) {
										if (dlen > 0 && dlen + 1 < sizeof(dec_prefix)) { dec_prefix[dlen++] = ' ';
}
										size_t dl = strlen(ds);
										if (dlen + dl + 2 < sizeof(dec_prefix)) {
											dec_prefix[dlen++] = '@';
											memcpy(dec_prefix + dlen, ds, dl);
											dlen += dl;
											dec_prefix[dlen] = '\0';
										}
									}
									Py_XDECREF(u);
								}
							}

							char *sig = NULL;
							char prefix[600];
							snprintf(prefix, sizeof(prefix), "%s%s%s", dec_prefix, dec_prefix[0] ? " " : "", is_async ? "async " : "");
							if (ret_s) {
								if (asprintf(&sig, "%s%s(%s) -> %s", prefix, ms, args_s ? args_s : "", ret_s) == -1) { sig = NULL;
}
							} else {
								if (asprintf(&sig, "%s%s(%s)", prefix, ms, args_s ? args_s : "") == -1) { sig = NULL;
}
							}
							if (sig) {
								size_t add = strlen(sig) + 2;
								if (len + add >= cap) { cap = (len+add)*2; char *tmp = realloc(methods, cap); if (tmp) { methods = tmp; 
}}
								if (len > 0) { methods[len++] = ',';
}
								strcpy(methods + len, sig);
								len += strlen(sig);
								free(sig);
							}

							/* docstring */
							char *mdoc = NULL;
							PyObject *gds = ast_mod ? PyObject_GetAttrString(ast_mod, "get_docstring") : NULL;
							if (gds) {
								PyObject *ds = PyObject_CallFunction(gds, "(O)", item);
								if (ds && ds != Py_None) {
									const char *dstr = PyUnicode_AsUTF8(ds);
									if (dstr) { mdoc = strdup(dstr);
}
								}
								Py_XDECREF(ds);
								Py_DECREF(gds);
							}

							/* insert method row + args */
							if (name) {
								sqlite3_int64 mid = 0;
								int mline = lineno;
								PyObject *mln_obj = PyObject_GetAttrString(item, "lineno");
								if (mln_obj) { mline = (int)PyLong_AsLong(mln_obj); if (PyErr_Occurred()) { PyErr_Clear(); 
}Py_DECREF(mln_obj); } else { PyErr_Clear();
}
								int mend = 0;
								PyObject *end_lineno_obj = PyObject_GetAttrString(item, "end_lineno");
								if (end_lineno_obj) {
									mend = (int)PyLong_AsLong(end_lineno_obj);
									if (PyErr_Occurred()) { PyErr_Clear();
}
									Py_DECREF(end_lineno_obj);
								} else {
									PyErr_Clear();
								}
								index_add_method(ctx->db, sid, ms, mline, mend, is_async, dec_prefix, ret_s, mdoc, &mid);
								if (mid > 0 && args_obj) {
									PyObject *posonly = PyObject_GetAttrString(args_obj, "posonlyargs");
									PyObject *pos = PyObject_GetAttrString(args_obj, "args");
									PyObject *vararg = PyObject_GetAttrString(args_obj, "vararg");
									PyObject *kwonly = PyObject_GetAttrString(args_obj, "kwonlyargs");
									PyObject *defaults = PyObject_GetAttrString(args_obj, "defaults");
									PyObject *kw_defaults = PyObject_GetAttrString(args_obj, "kw_defaults");
									PyObject *kwarg = PyObject_GetAttrString(args_obj, "kwarg");

									/* combined positional list */
									PyObject *combined = PyList_New(0);
									if (posonly && PyList_Check(posonly)) {
										Py_ssize_t np = PyList_Size(posonly);
										for (Py_ssize_t i = 0; i < np; i++) { PyList_Append(combined, PyList_GetItem(posonly, i));
}
									}
									if (pos && PyList_Check(pos)) {
										Py_ssize_t np = PyList_Size(pos);
										for (Py_ssize_t i = 0; i < np; i++) { PyList_Append(combined, PyList_GetItem(pos, i));
}
									}
									Py_ssize_t ncombined = PyList_Check(combined) ? PyList_Size(combined) : 0;
									Py_ssize_t ndefaults = (defaults && PyList_Check(defaults)) ? PyList_Size(defaults) : 0;

									for (Py_ssize_t i = 0; i < ncombined; i++) {
										PyObject *arg = PyList_GetItem(combined, i);
										PyObject *argname = arg ? PyObject_GetAttrString(arg, "arg") : NULL;
										const char *an = argname ? PyUnicode_AsUTF8(argname) : NULL;
										if (an && PyErr_Occurred()) { PyErr_Clear(); an = NULL; }
										PyObject *ann = arg ? PyObject_GetAttrString(arg, "annotation") : NULL;
										PyObject *ann_str = (unparse_fn && ann && ann != Py_None) ? PyObject_CallFunction(unparse_fn, "(O)", ann) : NULL;
										const char *ann_s = (ann_str && PyUnicode_Check(ann_str)) ? PyUnicode_AsUTF8(ann_str) : NULL;
										char *def_s = NULL;
										Py_ssize_t def_index = i - (ncombined - ndefaults);
										if (def_index >= 0 && defaults && PyList_Check(defaults)) {
											PyObject *dobj = PyList_GetItem(defaults, def_index);
											PyObject *dstr = (unparse_fn && dobj) ? PyObject_CallFunction(unparse_fn, "(O)", dobj) : NULL;
											if (dstr && PyUnicode_Check(dstr)) {
												const char *ds = PyUnicode_AsUTF8(dstr);
												if (ds) { def_s = strdup(ds);
}
											}
											Py_XDECREF(dstr);
										}
										const char *kind = "positional";
										if (posonly && PyList_Check(posonly) && i < PyList_Size(posonly)) { kind = "positional-only";
}
										index_add_method_arg(ctx->db, mid, an, ann_s, def_s, kind, (int)i);
										free(def_s);
										Py_XDECREF(ann_str);
										Py_XDECREF(ann);
										Py_XDECREF(argname);
									}
									Py_XDECREF(combined);

									/* vararg */
									if (vararg && vararg != Py_None) {
										PyObject *varg_name2 = PyObject_GetAttrString(vararg, "arg");
										const char *vn = varg_name2 ? PyUnicode_AsUTF8(varg_name2) : NULL;
										if (vn && PyErr_Occurred()) { PyErr_Clear(); vn = NULL; }
										PyObject *vann = PyObject_GetAttrString(vararg, "annotation");
										PyObject *vann_str = (unparse_fn && vann && vann != Py_None) ? PyObject_CallFunction(unparse_fn, "(O)", vann) : NULL;
										const char *vann_s = (vann_str && PyUnicode_Check(vann_str)) ? PyUnicode_AsUTF8(vann_str) : NULL;
										index_add_method_arg(ctx->db, mid, vn, vann_s, NULL, "varargs", -1);
										Py_XDECREF(vann_str);
										Py_XDECREF(vann);
										Py_XDECREF(varg_name2);
									}

									/* kwonly args */
									if (kwonly && PyList_Check(kwonly)) {
										Py_ssize_t nkw = PyList_Size(kwonly);
										for (Py_ssize_t i = 0; i < nkw; i++) {
											PyObject *arg = PyList_GetItem(kwonly, i);
											PyObject *argname = arg ? PyObject_GetAttrString(arg, "arg") : NULL;
											const char *an = argname ? PyUnicode_AsUTF8(argname) : NULL;
											if (an && PyErr_Occurred()) { PyErr_Clear(); an = NULL; }
											PyObject *ann = arg ? PyObject_GetAttrString(arg, "annotation") : NULL;
											PyObject *ann_str = (unparse_fn && ann && ann != Py_None) ? PyObject_CallFunction(unparse_fn, "(O)", ann) : NULL;
											const char *ann_s = (ann_str && PyUnicode_Check(ann_str)) ? PyUnicode_AsUTF8(ann_str) : NULL;
											char *def_s = NULL;
											if (kw_defaults && PyList_Check(kw_defaults) && i < PyList_Size(kw_defaults)) {
												PyObject *dobj = PyList_GetItem(kw_defaults, i);
												if (dobj && dobj != Py_None && unparse_fn) {
													PyObject *dstr = PyObject_CallFunction(unparse_fn, "(O)", dobj);
													if (dstr && PyUnicode_Check(dstr)) {
														const char *ds = PyUnicode_AsUTF8(dstr);
														if (ds) { def_s = strdup(ds);
}
													}
													Py_XDECREF(dstr);
												}
											}
											index_add_method_arg(ctx->db, mid, an, ann_s, def_s, "keyword-only", (int)i);
											free(def_s);
											Py_XDECREF(ann_str);
											Py_XDECREF(ann);
											Py_XDECREF(argname);
										}
									}

									/* kwarg */
									if (kwarg && kwarg != Py_None) {
										PyObject *kname = PyObject_GetAttrString(kwarg, "arg");
										const char *kn = kname ? PyUnicode_AsUTF8(kname) : NULL;
										if (kn && PyErr_Occurred()) { PyErr_Clear(); kn = NULL; }
										PyObject *kann = PyObject_GetAttrString(kwarg, "annotation");
										PyObject *kann_str = (unparse_fn && kann && kann != Py_None) ? PyObject_CallFunction(unparse_fn, "(O)", kann) : NULL;
										const char *kann_s = (kann_str && PyUnicode_Check(kann_str)) ? PyUnicode_AsUTF8(kann_str) : NULL;
										index_add_method_arg(ctx->db, mid, kn, kann_s, NULL, "kwargs", -1);
										Py_XDECREF(kann_str);
										Py_XDECREF(kann);
										Py_XDECREF(kname);
									}

									Py_XDECREF(kwarg);
									Py_XDECREF(kw_defaults);
									Py_XDECREF(defaults);
									Py_XDECREF(kwonly);
									Py_XDECREF(vararg);
									Py_XDECREF(pos);
									Py_XDECREF(posonly);
								}
								free(mdoc);
							}

							Py_XDECREF(ret_str);
							Py_XDECREF(args_str);
							Py_XDECREF(unparse_fn);
							Py_XDECREF(returns_obj);
							Py_XDECREF(args_obj);
							Py_XDECREF(decs_obj);
						}
						Py_XDECREF(mname);
					} else if (cn && strcmp(cn, "Assign") == 0) {
						PyObject *targets = PyObject_GetAttrString(item, "targets");
						PyObject *value = PyObject_GetAttrString(item, "value");
						PyObject *vcls = value ? PyObject_GetAttrString(value, "__class__") : NULL;
						PyObject *unparse_fn = ast_mod ? PyObject_GetAttrString(ast_mod, "unparse") : NULL;
						PyObject *vname = vcls ? PyObject_GetAttrString(vcls, "__name__") : NULL;
						const char *vkind = vname ? PyUnicode_AsUTF8(vname) : NULL;
						int is_lambda = vkind && strcmp(vkind, "Lambda") == 0;
						int is_method_like = is_lambda || (vkind && (strcmp(vkind, "Name") == 0 || strcmp(vkind, "Call") == 0));

						if (is_method_like && targets && PyList_Check(targets)) {
							Py_ssize_t nt = PyList_Size(targets);
							for (Py_ssize_t ti = 0; ti < nt; ti++) {
								PyObject *tgt = PyList_GetItem(targets, ti);
								PyObject *tcls = tgt ? PyObject_GetAttrString(tgt, "__class__") : NULL;
								PyObject *tname_attr = tcls ? PyObject_GetAttrString(tcls, "__name__") : NULL;
								const char *tclsname = tname_attr ? PyUnicode_AsUTF8(tname_attr) : NULL;
								if (tclsname && strcmp(tclsname, "Name") == 0) {
									PyObject *idobj = PyObject_GetAttrString(tgt, "id");
									const char *methname = idobj ? PyUnicode_AsUTF8(idobj) : NULL;
									if (methname) {
										if (is_lambda) {
											/* extract lambda args */
											PyObject *largs = PyObject_GetAttrString(value, "args");
											PyObject *lbody = PyObject_GetAttrString(value, "body");
											PyObject *largs_str = (unparse_fn && largs) ? PyObject_CallFunction(unparse_fn, "(O)", largs) : NULL;
											const char *largs_s = (largs_str && PyUnicode_Check(largs_str)) ? PyUnicode_AsUTF8(largs_str) : "";
											char *sig = NULL;
											if (asprintf(&sig, "%s(%s)", methname, largs_s ? largs_s : "") == -1) { sig = NULL;
}
											if (sig) {
												size_t add = strlen(sig) + 2;
												if (len + add >= cap) { cap = (len+add)*2; char *tmp = realloc(methods, cap); if (tmp) { methods = tmp; 
}}
												if (len > 0) { methods[len++] = ',';
}
												strcpy(methods + len, sig);
												len += strlen(sig);
												free(sig);
											}
											/* insert method row + args (reuse arg parsing by treating lambda args) */
											if (name) {
												sqlite3_int64 mid = 0;
												int mline = lineno;
												int mend = lineno;
												PyObject *end_lineno_obj = PyObject_GetAttrString(item, "end_lineno");
												if (end_lineno_obj) {
													mend = (int)PyLong_AsLong(end_lineno_obj);
													if (PyErr_Occurred()) { PyErr_Clear();
}
													Py_DECREF(end_lineno_obj);
												} else { PyErr_Clear();
}
												index_add_method(ctx->db, sid, methname, mline, mend, 0, "@lambda", NULL, NULL, &mid);
												/* args */
												if (mid > 0 && largs) {
													/* positional */
													PyObject *posonly = PyObject_GetAttrString(largs, "posonlyargs");
													PyObject *pos = PyObject_GetAttrString(largs, "args");
													PyObject *vararg = PyObject_GetAttrString(largs, "vararg");
													PyObject *kwonly = PyObject_GetAttrString(largs, "kwonlyargs");
													PyObject *defaults = PyObject_GetAttrString(largs, "defaults");
													PyObject *kw_defaults = PyObject_GetAttrString(largs, "kw_defaults");
													PyObject *kwarg = PyObject_GetAttrString(largs, "kwarg");

													PyObject *combined = PyList_New(0);
													if (posonly && PyList_Check(posonly)) { for (Py_ssize_t i=0;i<PyList_Size(posonly);i++) { PyList_Append(combined, PyList_GetItem(posonly,i));
}
}
													if (pos && PyList_Check(pos)) { for (Py_ssize_t i=0;i<PyList_Size(pos);i++) { PyList_Append(combined, PyList_GetItem(pos,i));
}
}
													Py_ssize_t ncombined = PyList_Check(combined) ? PyList_Size(combined) : 0;
													Py_ssize_t ndefaults = (defaults && PyList_Check(defaults)) ? PyList_Size(defaults) : 0;
													for (Py_ssize_t i=0;i<ncombined;i++) {
														PyObject *arg = PyList_GetItem(combined,i);
														PyObject *argname = arg ? PyObject_GetAttrString(arg,"arg") : NULL;
														const char *an = argname ? PyUnicode_AsUTF8(argname) : NULL;
														if (an && PyErr_Occurred()) { PyErr_Clear(); an=NULL; }
														PyObject *ann = arg ? PyObject_GetAttrString(arg,"annotation") : NULL;
														PyObject *ann_str = (unparse_fn && ann && ann != Py_None) ? PyObject_CallFunction(unparse_fn,"(O)",ann) : NULL;
														const char *ann_s = (ann_str && PyUnicode_Check(ann_str)) ? PyUnicode_AsUTF8(ann_str) : NULL;
														char *def_s = NULL;
														Py_ssize_t def_index = i - (ncombined - ndefaults);
														if (def_index >= 0 && defaults && PyList_Check(defaults)) {
															PyObject *dobj = PyList_GetItem(defaults, def_index);
															PyObject *dstr = (unparse_fn && dobj) ? PyObject_CallFunction(unparse_fn,"(O)",dobj) : NULL;
															if (dstr && PyUnicode_Check(dstr)) { const char *ds=PyUnicode_AsUTF8(dstr); if (ds) { def_s=strdup(ds); 
}}
															Py_XDECREF(dstr);
														}
														const char *kind = "positional";
														if (posonly && PyList_Check(posonly) && i < PyList_Size(posonly)) { kind = "positional-only";
}
														index_add_method_arg(ctx->db, mid, an, ann_s, def_s, kind, (int)i);
														free(def_s);
														Py_XDECREF(ann_str);
														Py_XDECREF(ann);
														Py_XDECREF(argname);
													}
													Py_XDECREF(combined);
													if (vararg && vararg != Py_None) {
														PyObject *varg_name = PyObject_GetAttrString(vararg,"arg");
														const char *vn = varg_name ? PyUnicode_AsUTF8(varg_name) : NULL;
														if (vn && PyErr_Occurred()) { PyErr_Clear(); vn=NULL; }
														PyObject *vann = PyObject_GetAttrString(vararg,"annotation");
														PyObject *vann_str = (unparse_fn && vann && vann != Py_None) ? PyObject_CallFunction(unparse_fn,"(O)",vann) : NULL;
														const char *vann_s = (vann_str && PyUnicode_Check(vann_str)) ? PyUnicode_AsUTF8(vann_str) : NULL;
														index_add_method_arg(ctx->db, mid, vn, vann_s, NULL, "varargs", -1);
														Py_XDECREF(vann_str); Py_XDECREF(vann); Py_XDECREF(varg_name);
													}
													if (kwonly && PyList_Check(kwonly)) {
														Py_ssize_t nkw = PyList_Size(kwonly);
														for (Py_ssize_t i=0;i<nkw;i++) {
															PyObject *arg = PyList_GetItem(kwonly,i);
															PyObject *argname = arg ? PyObject_GetAttrString(arg,"arg") : NULL;
															const char *an = argname ? PyUnicode_AsUTF8(argname) : NULL;
															if (an && PyErr_Occurred()) { PyErr_Clear(); an=NULL; }
															PyObject *ann = arg ? PyObject_GetAttrString(arg,"annotation") : NULL;
															PyObject *ann_str = (unparse_fn && ann && ann != Py_None) ? PyObject_CallFunction(unparse_fn,"(O)",ann) : NULL;
															const char *ann_s = (ann_str && PyUnicode_Check(ann_str)) ? PyUnicode_AsUTF8(ann_str) : NULL;
															char *def_s = NULL;
															if (kw_defaults && PyList_Check(kw_defaults) && i < PyList_Size(kw_defaults)) {
																PyObject *dobj = PyList_GetItem(kw_defaults,i);
																if (dobj && dobj != Py_None && unparse_fn) {
																	PyObject *dstr = PyObject_CallFunction(unparse_fn,"(O)",dobj);
																	if (dstr && PyUnicode_Check(dstr)) { const char *ds=PyUnicode_AsUTF8(dstr); if (ds) { def_s=strdup(ds); 
}}
																	Py_XDECREF(dstr);
																}
															}
															index_add_method_arg(ctx->db, mid, an, ann_s, def_s, "keyword-only", (int)i);
															free(def_s);
															Py_XDECREF(ann_str);
															Py_XDECREF(ann);
															Py_XDECREF(argname);
														}
													}
													if (kwarg && kwarg != Py_None) {
														PyObject *kname = PyObject_GetAttrString(kwarg,"arg");
														const char *kn = kname ? PyUnicode_AsUTF8(kname) : NULL;
														if (kn && PyErr_Occurred()) { PyErr_Clear(); kn=NULL; }
														PyObject *kann = PyObject_GetAttrString(kwarg,"annotation");
														PyObject *kann_str = (unparse_fn && kann && kann != Py_None) ? PyObject_CallFunction(unparse_fn,"(O)",kann) : NULL;
														const char *kann_s = (kann_str && PyUnicode_Check(kann_str)) ? PyUnicode_AsUTF8(kann_str) : NULL;
														index_add_method_arg(ctx->db, mid, kn, kann_s, NULL, "kwargs", -1);
														Py_XDECREF(kann_str); Py_XDECREF(kann); Py_XDECREF(kname);
													}
													Py_XDECREF(kwarg); Py_XDECREF(kw_defaults); Py_XDECREF(defaults);
													Py_XDECREF(kwonly); Py_XDECREF(vararg); Py_XDECREF(pos); Py_XDECREF(posonly);
												}
												Py_XDECREF(largs_str);
												Py_XDECREF(largs);
												Py_XDECREF(lbody);
											}
											Py_XDECREF(idobj);
										} else {
											/* assigned method (non-lambda): store reference */
											PyObject *val_str = (unparse_fn && value) ? PyObject_CallFunction(unparse_fn, "(O)", value) : NULL;
											const char *vs = (val_str && PyUnicode_Check(val_str)) ? PyUnicode_AsUTF8(val_str) : NULL;
											char *sig = NULL;
											if (vs) {
												if (asprintf(&sig, "%s = %s", methname, vs) == -1) { sig = NULL;
}
											} else {
												if (asprintf(&sig, "%s = <expr>", methname) == -1) { sig = NULL;
}
											}
											if (sig) {
												size_t add = strlen(sig) + 2;
												if (len + add >= cap) { cap = (len+add)*2; char *tmp = realloc(methods, cap); if (tmp) { methods = tmp; 
}}
												if (len > 0) { methods[len++] = ',';
}
												strcpy(methods + len, sig);
												len += strlen(sig);
												free(sig);
											}
											if (name) {
												sqlite3_int64 mid = 0;
												PyObject *end_lineno_obj = PyObject_GetAttrString(item, "end_lineno");
												int mend = lineno;
												if (end_lineno_obj) {
													mend = (int)PyLong_AsLong(end_lineno_obj);
													if (PyErr_Occurred()) { PyErr_Clear();
}
													Py_DECREF(end_lineno_obj);
												} else { PyErr_Clear();
}
												index_add_method(ctx->db, sid, methname, lineno, mend, 0, "assigned", NULL, NULL, &mid);
											}
											Py_XDECREF(val_str);
											Py_XDECREF(idobj);
										}
									}
									Py_XDECREF(tname_attr);
									Py_XDECREF(tcls);
								}
							}
							Py_XDECREF(targets);
							Py_XDECREF(value);
							Py_XDECREF(vname);
							Py_XDECREF(vcls);
							Py_XDECREF(unparse_fn);
						}
					}
					Py_XDECREF(iname);
					Py_XDECREF(icls);
				}
				if (methods) { methods[len] = '\0';
}
			}
			Py_XDECREF(body);
			/* docstring */
			PyObject *gds = PyObject_GetAttrString(ast_mod, "get_docstring");
			if (gds) {
				PyObject *ds = PyObject_CallFunction(gds, "(O)", node);
				if (ds && ds != Py_None) {
					const char *dstr = PyUnicode_AsUTF8(ds);
					if (dstr) { docstring = strdup(dstr);
}
				}
				Py_XDECREF(ds);
				Py_DECREF(gds);
			}
			/* decorators */
			PyObject *decs = PyObject_GetAttrString(node, "decorator_list");
			if (decs && PyList_Check(decs)) {
				size_t cap = 256;
				size_t len = 0;
				decorators = malloc(cap);
				decorators[0] = '\0';
				Py_ssize_t nd = PyList_Size(decs);
				for (Py_ssize_t i = 0; i < nd; i++) {
					PyObject *dec = PyList_GetItem(decs, i);
					PyObject *unparse = PyObject_GetAttrString(ast_mod, "unparse");
					PyObject *u = NULL;
					if (unparse) { u = PyObject_CallFunction(unparse, "(O)", dec); Py_DECREF(unparse); }
					if (!u) { u = PyObject_Str(dec);
}
					if (u) {
						const char *ds = PyUnicode_AsUTF8(u);
						if (ds) {
							size_t add = strlen(ds) + 2;
							if (len + add >= cap) { cap = (len+add)*2; char *tmp = realloc(decorators, cap); if (tmp) { decorators = tmp; 
}}
							if (len > 0) { decorators[len++] = ',';
}
							strcpy(decorators + len, ds);
							len += strlen(ds);
						}
						Py_DECREF(u);
					}
				}
				if (decorators) { decorators[len] = '\0';
}
			}
			Py_XDECREF(decs);
		}
		if (ast_mod) { Py_DECREF(ast_mod);
}
		int end_line = 0;
		PyObject *end_lineno_obj = PyObject_GetAttrString(node, "end_lineno");
		if (end_lineno_obj) {
			end_line = (int)PyLong_AsLong(end_lineno_obj);
			if (PyErr_Occurred()) { PyErr_Clear();
}
			Py_DECREF(end_lineno_obj);
		} else {
			PyErr_Clear();
		}
		index_add_class_detail(ctx->db, sid, bases, methods, docstring, decorators, end_line);
		free(bases); free(methods); free(docstring); free(decorators);
	} else if (strcmp(type, "FunctionDef") == 0 || strcmp(type, "AsyncFunctionDef") == 0) {
		if (name) { index_add_symbol(ctx->db, ctx->file_id, name, "function", lineno, 0, NULL, &sid);
}
	} else if (strcmp(type, "Assign") == 0) {
		PyObject *targets = PyObject_GetAttrString(node, "targets");
		PyObject *value = PyObject_GetAttrString(node, "value");
		PyObject *vcls = value ? PyObject_GetAttrString(value, "__class__") : NULL;
		PyObject *vname = vcls ? PyObject_GetAttrString(vcls, "__name__") : NULL;
		const char *vkind = vname ? PyUnicode_AsUTF8(vname) : NULL;
		int is_method_like = vkind && (strcmp(vkind, "Lambda") == 0 || strcmp(vkind, "Name") == 0 || strcmp(vkind, "Call") == 0);
		if (is_method_like && targets && PyList_Check(targets)) {
			Py_ssize_t nt = PyList_Size(targets);
			for (Py_ssize_t ti = 0; ti < nt; ti++) {
				PyObject *tgt = PyList_GetItem(targets, ti);
				PyObject *tcls = tgt ? PyObject_GetAttrString(tgt, "__class__") : NULL;
				PyObject *tclsname_obj = tcls ? PyObject_GetAttrString(tcls, "__name__") : NULL;
				const char *tclsname = tclsname_obj ? PyUnicode_AsUTF8(tclsname_obj) : NULL;
				if (tclsname && strcmp(tclsname, "Attribute") == 0) {
					PyObject *obj = PyObject_GetAttrString(tgt, "value");
					PyObject *attr = PyObject_GetAttrString(tgt, "attr");
					PyObject *ocls = obj ? PyObject_GetAttrString(obj, "__class__") : NULL;
					PyObject *oname_obj = ocls ? PyObject_GetAttrString(ocls, "__name__") : NULL;
					const char *oname_cls = oname_obj ? PyUnicode_AsUTF8(oname_obj) : NULL;
					PyObject *idobj = (obj && oname_cls && strcmp(oname_cls, "Name") == 0) ? PyObject_GetAttrString(obj, "id") : NULL;
					const char *classname = idobj ? PyUnicode_AsUTF8(idobj) : NULL;
					if (classname && PyErr_Occurred()) { PyErr_Clear(); classname = NULL; }
					const char *methodname = attr ? PyUnicode_AsUTF8(attr) : NULL;
					if (classname && methodname) {
						/* find class symbols */
						const char *sql = "SELECT id FROM symbols WHERE name=?1 AND kind='class';";
						sqlite3_stmt *stmt;
						if (sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
							sqlite3_bind_text(stmt, 1, classname, -1, SQLITE_TRANSIENT);
							while (sqlite3_step(stmt) == SQLITE_ROW) {
								sqlite3_int64 csid = sqlite3_column_int64(stmt, 0);
								int is_lambda = vkind && strcmp(vkind, "Lambda") == 0;
								PyObject *unparse_fn = PyImport_ImportModule("ast") ? PyObject_GetAttrString(PyImport_ImportModule("ast"), "unparse") : NULL;
								/* insert method */
								sqlite3_int64 mid = 0;
								const char *decorators = is_lambda ? "@lambda" : "assigned";
								const char *returns = NULL;
								if (vkind && strcmp(vkind, "Lambda") == 0) {
									PyObject *lreturns = NULL; /* lambda has no annotation */
									(void)lreturns;
								}
								index_add_method(ctx->db, csid, methodname, lineno, lineno, 0, decorators, returns, NULL, &mid);
								/* update class_details.methods string */
								/* build sig */
								char *sig = NULL;
								if (is_lambda) {
									PyObject *largs = PyObject_GetAttrString(value, "args");
									PyObject *la = unparse_fn && largs ? PyObject_CallFunction(unparse_fn, "(O)", largs) : NULL;
									const char *las = (la && PyUnicode_Check(la)) ? PyUnicode_AsUTF8(la) : "";
									if (asprintf(&sig, "%s(%s)", methodname, las ? las : "") == -1) { sig = NULL;
}
									Py_XDECREF(la);
									Py_XDECREF(largs);
								} else {
									PyObject *vs = unparse_fn && value ? PyObject_CallFunction(unparse_fn, "(O)", value) : NULL;
									const char *vstr = (vs && PyUnicode_Check(vs)) ? PyUnicode_AsUTF8(vs) : NULL;
									if (vstr) {
										if (asprintf(&sig, "%s = %s", methodname, vstr) == -1) { sig = NULL;
}
									} else {
										if (asprintf(&sig, "%s = <expr>", methodname) == -1) { sig = NULL;
}
									}
									Py_XDECREF(vs);
								}
								if (sig) {
									/* append to class_details.methods */
									sqlite3_stmt *us;
									const char *usql = "SELECT methods FROM class_details WHERE symbol_id=?;";
									if (sqlite3_prepare_v2(ctx->db, usql, -1, &us, NULL) == SQLITE_OK) {
										sqlite3_bind_int64(us, 1, csid);
										if (sqlite3_step(us) == SQLITE_ROW) {
											const char *old = (const char *)sqlite3_column_text(us, 0);
											char *newm = NULL;
											if (old && old[0]) { if (asprintf(&newm, "%s,%s", old, sig) == -1) { newm = NULL; 
}}
											else { if (asprintf(&newm, "%s", sig) == -1) { newm = NULL; 
}}
											sqlite3_finalize(us);
											if (newm) {
												sqlite3_stmt *up;
												const char *upsql = "UPDATE class_details SET methods=? WHERE symbol_id=?;";
												if (sqlite3_prepare_v2(ctx->db, upsql, -1, &up, NULL) == SQLITE_OK) {
													sqlite3_bind_text(up, 1, newm, -1, SQLITE_TRANSIENT);
													sqlite3_bind_int64(up, 2, csid);
													sqlite3_step(up);
													sqlite3_finalize(up);
												}
												free(newm);
											}
										} else {
											sqlite3_finalize(us);
										}
									}
									free(sig);
								}
								Py_XDECREF(unparse_fn);
							}
							sqlite3_finalize(stmt);
						}
					}
					Py_XDECREF(idobj);
					Py_XDECREF(tclsname_obj);
					Py_XDECREF(tcls);
					Py_XDECREF(attr);
					Py_XDECREF(obj);
				}
			}
		}
		Py_XDECREF(vname);
		Py_XDECREF(vcls);
		Py_XDECREF(value);
		/* original variable extraction */
		if (targets && PyList_Check(targets)) {
			Py_ssize_t n = PyList_Size(targets);
			for (Py_ssize_t i = 0; i < n; i++) {
				extract_assign_targets(PyList_GetItem(targets, i), ctx, lineno);
			}
		}
		Py_XDECREF(targets);
	} else if (strcmp(type, "Name") == 0 && name) {
		/* Record uses of class names as references. */
		const char *sql = "SELECT id FROM symbols WHERE name=?1 AND kind='class';";
		sqlite3_stmt *stmt;
		if (sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				sqlite3_int64 ref_sid = sqlite3_column_int64(stmt, 0);
				index_add_ref(ctx->db, ref_sid, ctx->file_id, lineno, 0);
			}
			sqlite3_finalize(stmt);
		}
	}
	PyErr_Clear();
}

