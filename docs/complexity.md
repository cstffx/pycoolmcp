# Functions with Cognitive Complexity > 25

These functions exceed the recommended cognitive complexity threshold of 25 and are candidates for refactoring.

| File | Function | Complexity | Priority |
|------|----------|-----------|----------|
| src/extract.c | on_node | 2197 | 🔴 Critical |
| src/query_class.c | class_command | 393 | 🔴 Critical |
| src/query_search.c | search_command | 180 | 🟠 High |
| src/query_method.c | method_command | 118 | 🟠 High |
| mcp/tools.c | handle_tools_call | 123 | 🟠 High |
| src/query_info.c | info_command | 66 | 🟡 Medium |
| mcp/http_app.c | run_query | 91 | 🟡 Medium |
| src/ignore.c | ignore_is_ignored | 84 | 🟡 Medium |
| src/traverse.c | ast_traverse | 47 | 🟡 Medium |
| mcp/http_app.c | main | 47 | 🟡 Medium |
| src/observer.c | observer_watch | 44 | 🟡 Medium |
| src/query_method.c | build_params | 37 | 🟡 Medium |

## Notes

- **on_node** (2197): Core AST visitor - high complexity is inherent to the task of processing diverse AST node types
- **class_command** (393): Command handler with many parameters and output formatting options
- **search_command** (180): Pattern matching and output formatting logic
- **method_command** (118): Similar to class_command but for methods
- **handle_tools_call** (123): MCP tools dispatcher with multiple commands

Consider breaking these into smaller, focused functions to improve maintainability.
