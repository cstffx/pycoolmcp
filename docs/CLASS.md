# pycoolctl class

Query a class from a pycools index.

## Usage

```
pycoolctl class <Name> [--file PATH] [--module PATH] [--info LETTERS]
               [--compact|--full|--verbose|--json|--body|--refs]
               [--method NAME] [--pid PID] [--db PATH]
```

`<Name>` is required.

## Options

- `--file` / `--module`: restrict to one source file (definition for normal output; usage file for `--refs`).
- `--info LETTERS`: select fields (verbose mode).
- `--compact`: one compact line per class (default).
- `--full` / `--verbose`: verbose `key=value | ...` output.
- `--json`: JSON array.
- `--body`: print only the class source span (or a method span with `--method`).
- `--method NAME`: target a method for `--body`/details.
- `--refs`: print usages as `file:line`.
- `--pid PID`: use the DB of a running pycool process.
- `--db PATH`: index database (or first running instance).

## Info letters

- `A` = name
- `B` = file
- `C` = line
- `D` = bases
- `E` = methods
- `F` = fields (n/a)
- `G` = docstring
- `H` = types (n/a)
- `I` = refs (use `--refs`)
- `J` = decorators
- `K` = module
- `L` = end_line

## Output

- Compact: `Name path:line [bases=...] [methods=...] [docstring=...] [decorators=...] [end_line=...]`
- Empty result: no output.
- Stable, one line per class.