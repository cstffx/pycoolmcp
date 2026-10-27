# Functions with Cognitive Complexity > 25

These functions exceed the recommended cognitive complexity threshold of 25 and are candidates for refactoring.

| File | Function | Complexity | Priority | Status |
|------|----------|-----------|----------|--------|
| src/extract.c | on_node | 64 | 🔴 Critical | ↓ 2197→64 |
| src/query_search.c | search_command | 180 | 🔴 Critical | — |
| mcp/tools.c | handle_tools_call | 123 | 🟠 High | — |
| src/query_method.c | method_command | 118 | 🟠 High | — |
| mcp/http_app.c | run_query | 91 | 🟡 Medium | — |
| src/ignore.c | ignore_is_ignored | 84 | 🟡 Medium | — |
| src/query_info.c | info_command | 66 | 🟡 Medium | — |
| src/observer.c | observer_watch | 44 | 🟡 Medium | — |
| src/traverse.c | ast_traverse | 43 | 🟡 Medium | ↓ 47→43 |
| mcp/http_app.c | main | 47 | 🟡 Medium | — |
| src/query_method.c | build_params | 37 | 🟡 Medium | — |

## Recently Refactored

✅ **class_command** refactored from 393 → < 25 (extracted: parse_class_args, resolve_db_path, check_schema_version, handle_ambiguous_names, handle_refs_mode, print_method_body, print_class_body_range, print_output_line + output format handlers)

✅ **on_node** refactored from 2197 → 64 (extract module decomposition)

✅ **extract/class.c, extract/method.c, extract/common.c, extract/assign.c** all < 25

## Notes

- **on_node** (64): Core AST visitor, 97% reduction but still above threshold due to inherent complexity
- **search_command** (180): Pattern matching & output formatting - next priority
- **handle_tools_call** (123): MCP tools dispatcher with multiple command paths
- **method_command** (118): Similar structure to class_command, likely needs same refactoring approach

Next targets: search_command → handle_tools_call → method_command
