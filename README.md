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

### Linux

```bash
cmake -S . -B build
cmake --build build
```

### Windows

OpenSSL is not available by default, so install it with vcpkg and point CMake at the
vcpkg toolchain file:

```powershell
vcpkg install openssl:x64-windows
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug
```

Visual Studio is a multi-config generator, so the configuration is chosen at build time
(`--config Debug` / `--config Release`) rather than through `CMAKE_BUILD_TYPE`. The
resulting binary is `build\Debug\fim_lite.exe`, and the tests have to be run with the
same configuration:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Tests that rely on POSIX file permissions are compiled only on non-Windows platforms.

## Usage

The examples below use the Linux binary path. On Windows the binary lives under the
configuration directory, so use `build\Debug\fim_lite.exe` (or `build\Release\fim_lite.exe`)
instead of `./build/fim_lite`.

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

On Windows, pass the configuration you built with, otherwise `ctest` finds no tests:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

The tests cover the core functionality, including file scanning, hashing, baseline storage, and change detection.
