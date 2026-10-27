# Build

## Requirements

- GCC or Clang (C11 compatible compiler)
- Python 3.13 development headers
- SQLite 3 development libraries
- libjson-c development libraries
- pkg-config
- GNU Make

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo add-apt-repository ppa:deadsnakes/ppa -y
sudo apt-get update
sudo apt-get install -y build-essential \
  python3.13-dev libsqlite3-dev libjson-c-dev pkg-config
```

**macOS (using Homebrew):**
```bash
brew install python@3.13 sqlite json-c
```

## Building

```bash
make
```

Output binaries:
- `build/pycool` - Indexing daemon
- `build/bin/pycoolctl` - Command-line tool
- `build/pycoolmcp` - MCP server
- `build/libpycool.a` - Static library
- `build/libpycool.so` - Shared library

## Build Targets

- `make all` - Build everything (default)
- `make clean` - Clean build artifacts
- `make test` - Run all tests
- `make cppcheck` - Run static analysis
- `make lint` - Run code quality checks
- `make pipeline` - Run full CI pipeline (cppcheck + build + test + clean)

## Coverage

Build with coverage:
```bash
make CFLAGS="-O2 --coverage"
```

Generate coverage report:
```bash
gcovr --root .
```

Clean coverage data:
```bash
make clean  # removes *.gcda, *.gcno, *.gcov files
```
