# fim-lite

A file integrity monitoring tool for Linux and Windows. It scans a directory, computes
SHA-256 hashes, stores them as a JSON baseline, and on the next run tells you what
changed. Baselines can optionally be signed with HMAC-SHA256 so you can tell if someone
tampered with the baseline itself.

## Features

- SHA-256 hashing through OpenSSL
- Optional HMAC-SHA256 signing of the baseline
- Exclude patterns stored in the baseline and reused on check
- Atomic baseline writes (temp file, fsync, rename)
- Distinct exit codes for scripts and monitoring

## Build

Requires CMake 3.20+, a C++17 compiler and OpenSSL. nlohmann/json and Catch2 are fetched
by CMake and pinned to a checksum and a commit.

### Linux

```sh
sudo apt install libssl-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary ends up at `build/fim_lite`.

### Windows

OpenSSL is not available by default. Install it with vcpkg and point CMake at the
toolchain file:

```powershell
vcpkg install openssl:x64-windows
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

The binary ends up at `build\Release\fim_lite.exe`.

## Usage

Examples use the Linux path. On Windows use `build\Release\fim_lite.exe` instead.

### Create a baseline

```sh
./build/fim_lite init /path/to/folder baseline.json
```

```
Baseline created: 12 files scanned.
```

### Check for changes

```sh
./build/fim_lite check /path/to/folder baseline.json
```

```
[MODIFIED] config.ini
[ADDED] new_file.txt
[REMOVED] old_file.txt
```

With no changes it prints `No changes detected.` and exits with 0.

### Exclude files or directories

```sh
./build/fim_lite init /srv/app baseline.json --exclude .git --exclude node_modules
```

Patterns match a file or directory name, not a path, so `--exclude build` skips every
`build` directory in the tree. Matching ignores case for ASCII letters only.

The patterns get saved into the baseline and `check` picks them up automatically.
Passing `--exclude` on check overrides whatever was stored.

### Sign the baseline

```sh
export FIMLITE_HMAC_KEY="$(cat /etc/fim-lite/key)"
./build/fim_lite init /srv/app baseline.json
./build/fim_lite check /srv/app baseline.json
```

`--hmac-key <key>` overrides the environment variable, but a key on the command line is
visible to other users through the process list.

If the baseline is signed and no key is given, `check` prints a warning and skips
verification. If a key is given and the baseline is not signed, `check` exits with 5.

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | No changes, all entries read successfully. |
| 1 | Changes detected (`check` only). |
| 2 | Invalid usage. |
| 3 | Incomplete run: some entries could not be read. Symlinks don't count. |
| 4 | Runtime error: missing or corrupt baseline, unreadable root directory, I/O failure. |
| 5 | Baseline signature verification failed. |

When both changes and unreadable entries are present, 1 wins over 3.

## Behaviour notes

- Changes are detected by SHA-256 only. File size and modification time are stored but
  not compared.
- Symlinks are not followed. Their count is printed to stderr and they don't affect the
  exit code.
- An entry that cannot be read is listed on stderr and never reported as `[REMOVED]`.
- Paths in the baseline are UTF-8 with `/` separators, so a baseline is portable between
  Linux and Windows.
- Without a key the baseline is plain JSON. Anyone who can write to it can hide a change.

## Tests

```sh
ctest --test-dir build --output-on-failure
```

On Windows pass the configuration:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Tests that make files unreadable with `chmod` run on Linux only.

## Dependencies

| Library | Purpose | How |
|---------|---------|-----|
| [OpenSSL](https://www.openssl.org/) | SHA-256, HMAC-SHA256 | System package / vcpkg |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON baseline format | FetchContent |
| [Catch2](https://github.com/catchorg/Catch2) | Unit tests | FetchContent |
