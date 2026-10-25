pycoolmcp
=========

Python source tree indexer for fast symbol lookup, with a built-in HTTP query server.

pycoolmcp indexes Python codebases (classes, methods, references) into SQLite and
lets you query symbols without re-reading source. A small HTTP server exposes the
same queries over GET.

Status: work in progress; bugs likely. Use at your own risk.

Build
-----

    make            # builds libast.a, pycool, bin/pycoolctl, http_server
    make release    # optimized build
    make clean

Run indexer
------------

    ./pycool /path/to/root /path/to/index.db

Query (CLI)
-----------

    ./bin/pycoolctl class Type --db /path/to/index.db
    ./bin/pycoolctl method Class Method --db /path/to/index.db

See AGENTS.md for the full option reference.

HTTP server
-----------

    ./http_server [port] [bind]   # default: 127.0.0.1:8080

Endpoints:

    GET /            health check
    GET /class?name=Type[&file=PATH&info=LETTERS&...]
    GET /method?class=Type[&method=Name&body=1&...]

The server binds to localhost by default, allows only GET, validates and
sanitizes all parameters, enforces request size and timeouts, and executes
queries without a shell.

License
-------

No license specified yet.