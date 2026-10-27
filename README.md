# pycoolmcp

[![CI](https://github.com/cstffx/pycoolmcp/actions/workflows/ci.yml/badge.svg)](https://github.com/cstffx/pycoolmcp/actions/workflows/ci.yml)
[![Release](https://github.com/cstffx/pycoolmcp/actions/workflows/release.yml/badge.svg)](https://github.com/cstffx/pycoolmcp/actions/workflows/release.yml)

Index Python source trees and query symbols fast.

## Features

- **AST parsing**: Parse Python files and extract code structure
- **Symbol indexing**: Index classes, methods, and other Python symbols
- **Fast queries**: Search symbols by name and type
- **Incremental updates**: Watch directories for changes and update index
- **MCP server**: Model Context Protocol integration for editor plugins
- **CLI tool**: `pycoolctl` for command-line queries
- **SQLite backend**: Persistent, queryable index database

## Documentation

- **[Build](docs/build.md)** - Installation requirements and build instructions
- **[Usage](docs/usage.md)** - Complete command reference and examples