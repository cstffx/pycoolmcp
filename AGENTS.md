# pycools for agents

Index Python code and query classes/methods without reading source.

## Start indexer
`pycool ROOT [DBPATH]` (daemon). ROOT is stored as absolute path.

## Query
`pycoolctl class <Name> [options]`
`pycoolctl method <Class> [Method] [options]`

Options:
- `--db PATH` or `--pid PID` (auto if one instance)
- `--file PATH` (definition or usage filter)
- `--info LETTERS`: A=name B=file C=line D=bases E=methods F=fields(n/a) G=docstring H=types(n/a) I=refs(use --refs) J=decorators K=module L=end_line
- `--compact` (default), `--full`, `--verbose`, `--json`
- `--body`: print source span
- `--method NAME`: with `--body`, print one method
- `--refs`: list usages as `file:line`
- `--limit N`, `--offset N`

## Output rules
- One line per symbol.
- Empty result: no output.
- No prose.