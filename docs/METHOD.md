# pycoolctl method

Query methods of a class from a pycools index.

## Usage

```
pycoolctl method <Class> [Method] [--file PATH] [--body] [--json]
                 [--limit N] [--offset N] [--pid PID] [--db PATH]
```

`<Class>` is required. `[Method]` is optional and, when given, restricts to that method name.

## Options

- `--file PATH`: restrict to methods defined in one source file (matched by path).
- `--body`: print the source span of each matching method (or class span when no method).
- `--json`: JSON array output.
- `--limit N`: limit number of results.
- `--offset N`: skip first N results.
- `--pid PID`: use the DB of a running pycool process.
- `--db PATH`: index database (or first running instance).

## Output

- Default: `Class.method(params) -> returns decorators path:line-end_line`
- `--json`: JSON array of objects with class, method, file, line, end_line, decorators, returns, docstring, params.
- `--body`: source lines of each method.
- Empty result: no output (or `[]` in JSON mode).