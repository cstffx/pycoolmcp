# pycoolmcp

Index Python source trees and query symbols fast.

## Features

- **AST parsing**: Parse Python files and extract code structure
- **Symbol indexing**: Index classes, methods, and other Python symbols
- **Fast queries**: Search symbols by name and type
- **Incremental updates**: Watch directories for changes and update index
- **MCP server**: Model Context Protocol integration for editor plugins
- **CLI tool**: `pycoolctl` for command-line queries
- **SQLite backend**: Persistent, queryable index database

## Build

```bash
make
```

## Usage

**Index a Python project:**
```bash
./build/pycool /path/to/python/project /path/to/index.db
```

**Query the index:**
```bash
./build/bin/pycoolctl class ClassName --db /path/to/index.db
./build/bin/pycoolctl method ClassName methodName --db /path/to/index.db
```

**MCP server:**
```bash
./build/pycoolmcp
```