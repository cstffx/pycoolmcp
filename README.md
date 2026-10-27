# pycoolmcp

Index Python source trees and query symbols fast.

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

**HTTP server:**
```bash
./build/http_server [port] [bind]
```