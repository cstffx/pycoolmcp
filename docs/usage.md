# Usage

## Daemon: Index a Python Project

Start the indexing daemon to scan a Python project and maintain an index:

```bash
./build/pycool /path/to/python/project /path/to/index.db
```

The daemon:
- Scans the entire directory tree for Python files
- Extracts symbols (classes, methods, etc.)
- Builds and maintains a SQLite index database
- Watches for file changes and updates the index incrementally

## CLI: pycoolctl

Query the index using the command-line tool.

### list

List all indexed symbols of a given type.

```bash
./build/bin/pycoolctl list [options]
```

Options:
- `--db PATH` - Path to index database
- `--pid PID` - Process ID (for process-specific index)

Example:
```bash
./build/bin/pycoolctl list --db ./index.db
```

### info

Get detailed information about a symbol.

```bash
./build/bin/pycoolctl info NAME [options]
```

Options:
- `--db PATH` - Path to index database
- `--info TYPE` - Information type to retrieve (class, method, etc.)

Example:
```bash
./build/bin/pycoolctl info MyClass --db ./index.db --info class
```

### class

Find and display information about a Python class.

```bash
./build/bin/pycoolctl class CLASS_NAME [options]
```

Options:
- `--db PATH` - Path to index database
- `--method NAME` - Filter to a specific method within the class

Example:
```bash
./build/bin/pycoolctl class MyClass --db ./index.db
./build/bin/pycoolctl class MyClass --method __init__ --db ./index.db
```

### method

Find and display information about a Python method.

```bash
./build/bin/pycoolctl method CLASS_NAME METHOD_NAME [options]
```

Options:
- `--db PATH` - Path to index database

Example:
```bash
./build/bin/pycoolctl method MyClass my_method --db ./index.db
```

### search

Search for symbols by pattern across the entire index.

```bash
./build/bin/pycoolctl search PATTERN [options]
```

Options:
- `--db PATH` - Path to index database
- `--file PATH` - Restrict search to a specific file

Example:
```bash
./build/bin/pycoolctl search "get_*" --db ./index.db
./build/bin/pycoolctl search Config --file config.py --db ./index.db
```

### stop

Stop the indexing daemon.

```bash
./build/bin/pycoolctl stop [options]
```

Options:
- `--pid PID` - Process ID of the daemon to stop (stops all if not specified)

Example:
```bash
./build/bin/pycoolctl stop
./build/bin/pycoolctl stop --pid 1234
```

### clean

Clean up the index database.

```bash
./build/bin/pycoolctl clean [options]
```

Options:
- `--db PATH` - Path to index database to clean

Example:
```bash
./build/bin/pycoolctl clean --db ./index.db
```

### reindex

Rebuild the index from scratch.

```bash
./build/bin/pycoolctl reindex [options]
```

Options:
- `--db PATH` - Path to index database
- `--pid PID` - Process ID associated with the index

Example:
```bash
./build/bin/pycoolctl reindex --db ./index.db
```

## MCP Server

Run the Model Context Protocol server for integration with editors and IDEs:

```bash
./build/pycoolmcp
```

The MCP server provides:
- Standard MCP protocol interface
- Integration with Claude and other MCP-compatible tools
- Query capabilities matching the CLI commands
- Real-time index updates

## Database Management

### Create Index

```bash
./build/pycool /path/to/project /path/to/index.db
```

### Query Index

Use `pycoolctl` commands with the `--db` flag:

```bash
./build/bin/pycoolctl class MyClass --db /path/to/index.db
./build/bin/pycoolctl search pattern --db /path/to/index.db
```

### Clean Index

Remove all data from the index:

```bash
./build/bin/pycoolctl clean --db /path/to/index.db
```

### Rebuild Index

Delete and recreate the entire index:

```bash
./build/bin/pycoolctl reindex --db /path/to/index.db
```
