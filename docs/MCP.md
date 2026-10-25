# pycoolmcp (MCP server)

`pycoolmcp` is a stdio MCP server. It exposes pycool index queries as tools:

- `query_class` — query classes/methods by name.
- `query_method` — query methods of a class.
- `list_instances` — list running pycool instances.
- `info` — show index info.

The agent discovers these via the MCP `tools/list` request, including names,
descriptions and JSON input schemas.

## Client configuration example

```json
{
  "mcpServers": {
    "pycool": {
      "command": "/path/to/pycoolmcp",
      "args": []
    }
  }
}
```

The server resolves `pycoolctl` next to its own binary (or in `bin/`).