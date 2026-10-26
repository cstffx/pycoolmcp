# pycoolmcp

C/Python indexer (pycool daemon + pycoolctl CLI + MCP server).

## Build
`make` (or `make all`). Tests: `make test_ast test_traverse test_index test_dbpath test_info test_list test_reindex test_clean test_stop test_extract`.

## Coverage
Tool: **gcovr** (`.gcovr.cfg`). Build with `--coverage`, run tests, then `gcovr --root .`. `make clean` removes `*.gcda`, `*.gcno`, `*.gcov`.

## Notes
- Source: `src/`, `index/`, `pycoolctl/`, `mcp/`, `http/`.
- Coverage target list: `coverage.md`.

## Before commit
Always run `cppcheck --enable=all --inconclusive --quiet --suppress=missingInclude --suppress=missingIncludeSystem --suppress=checkLevelNormal --suppress=checkersReport --suppress=normalCheckLevelMaxBranches --suppress=unusedFunction --suppress=staticFunction src/ index/ mcp/ pycoolctl/ http/` and ensure `cppcheck.txt` is empty before committing. If there is any error/warning, fix it before committing.
