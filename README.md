# fim-lite

A lightweight C++ CLI tool for monitoring files and detecting changes.

`fim-lite` scans a directory, calculates SHA-256 hashes for files, stores the current state as a JSON baseline, and later compares the directory against that baseline.

The basic workflow is:

1. Create a baseline.
2. Modify the directory.
3. Run a check.
4. See which files were added, removed, or modified.

## Tech stack

- **C++17**
- **OpenSSL** - SHA-256 hashing
- **nlohmann/json** - JSON baseline storage
- **Catch2** - unit tests
- **CMake** - build system

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Usage

### Create a baseline

```bash
./build/fim_lite init /path/to/folder baseline.json
```

Example output:

```text
Baseline created: 12 files scanned.
```

### Check for changes

```bash
./build/fim_lite check /path/to/folder baseline.json
```

If changes are detected:

```text
[ADDED] new_file.txt
[REMOVED] old_file.txt
[MODIFIED] config.ini
```

If the directory matches the saved baseline:

```text
No changes detected.
```

## Tests

Build and run the test suite with:

```bash
ctest --test-dir build --output-on-failure
```

The tests cover the core functionality, including file scanning, hashing, baseline storage, and change detection.
