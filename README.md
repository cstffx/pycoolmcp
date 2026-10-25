pycoolmcp
=========

Index Python source trees. Query symbols fast.

Build
-----

    make

Run
---

    ./pycool /path/to/root /path/to/index.db

Query
-----

    ./bin/pycoolctl class Type --db /path/to/index.db
    ./bin/pycoolctl method Class Method --db /path/to/index.db

MCP server
----------

    ./pycoolmcp

HTTP server
-----------

    ./http_server [port] [bind]

Notes
-----

Work in progress. Bugs likely. Use at your own risk.