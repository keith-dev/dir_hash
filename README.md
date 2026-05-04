# dir-hash

A header-only C++17 library that walks a directory tree, hashes every
regular file with [BLAKE3](https://github.com/BLAKE3-team/BLAKE3), and
produces a content-based Merkle-like fingerprint for every node — files
and directories alike. Name-agnostic: only content matters.

Intended for comparing large directory trees (e.g. backup snapshots) to
identify which subtrees have changed.

Includes a small `md5sum`/`md5(1)`-style command-line frontend, `dirhash`.

See [`Plan.md`](Plan.md) for the full design specification.

---

## Building

The build vendors BLAKE3 (portable C, SIMD disabled by default) and
produces the test binaries. Two makefiles are provided so each platform's
native `make` works:

| Platform | Makefile        | Invoke as   |
|----------|-----------------|-------------|
| Linux    | `GNUmakefile`   | `gmake` (or `make` if GNU make is the default) |
| FreeBSD  | `BSDmakefile`   | `make`      |

```sh
gmake test       # Linux
make test        # FreeBSD
```

Targets:

- `all` — build all test binaries under `build/`
- `test` — build and run all tests
- `clean` — remove `build/`
- `install` — install header and pkg-config file under `$(PREFIX)`
- `uninstall` — remove the installed files

### Build knobs

| Variable    | Default                          |
|-------------|----------------------------------|
| `CXX`       | `c++`                            |
| `CC`        | `cc`                             |
| `CXXFLAGS`  | `-O2 -g -Wall -Wextra`           |
| `CFLAGS`    | `-O2 -g -Wall -Wextra`           |

### Install knobs

| Variable       | Default (Linux)               | Default (FreeBSD)                  |
|----------------|-------------------------------|------------------------------------|
| `PREFIX`       | `/usr/local`                  | `/usr/local`                       |
| `DESTDIR`      | (empty — staging prefix)      | (empty)                            |
| `BINDIR`       | `$(PREFIX)/bin`               | `${PREFIX}/bin`                    |
| `INCLUDEDIR`   | `$(PREFIX)/include`           | `${PREFIX}/include`                |
| `LIBDIR`       | `$(PREFIX)/lib`               | `${PREFIX}/lib`                    |
| `PKGCONFIGDIR` | `$(LIBDIR)/pkgconfig`         | `${PREFIX}/libdata/pkgconfig`      |
| `MANDIR`       | `$(PREFIX)/share/man`         | `${PREFIX}/man`                    |
| `INSTALL`      | `install`                     | `install`                          |

Man pages ship in two flavours: `man/linux/` (`man(7)` macros, used by
`GNUmakefile`) and `man/freebsd/` (`mdoc(7)` macros, used by
`BSDmakefile`). Each makefile installs only its own flavour.

Examples:

```sh
gmake install                                  # /usr/local
gmake install PREFIX=$HOME/.local              # ~/.local
gmake install DESTDIR=/tmp/stage PREFIX=/usr   # staged
```

The pkg-config file declares `Requires: libblake3`, so consumers pick up
BLAKE3 through their own system package.

---

## Command-line tool

`dirhash` mirrors the `md5sum(1)` / `md5(1)` UX:

```
dirhash [-r] [-t] [-e] path...

  -r    print hashes for every node, not just the roots
  -t    BSD-style tagged output: BLAKE3 (path) = <hex>
  -e    print per-file I/O errors to stderr
  -h    show this help
```

Each argument may be a regular file or a directory. Directories get a
trailing `/` in the output so they're distinguishable from files.

```sh
$ dirhash /backup/2024-01
164ca99b569c541e95e3ddba0d73769a917d61afaa61694493639d09337df671  /backup/2024-01/

$ dirhash -t /backup/2024-01
BLAKE3 (/backup/2024-01/) = 164ca99b569c541e95e3ddba0d73769a917d61afaa61694493639d09337df671

$ dirhash -r /backup/2024-01 | head -3
53ee0df2...  /backup/2024-01/foo/c.txt
924f020f...  /backup/2024-01/foo/
8e4c7c1b...  /backup/2024-01/a.txt
```

Exit status is non-zero if any argument could not be hashed.

---

## Using the library

dir-hash is header-only. Add `-I<prefix>/include` to your build, link
against BLAKE3 (`-lblake3` or via `pkg-config --libs dir_hash`), and
`#include "dir_hash.hpp"`.

### API

```cpp
#include "dir_hash.hpp"

namespace dir_hash {
inline namespace v1 {

template <typename Fn, typename ErrFn = detail::NoopError>
std::array<uint8_t, 32> hash_directory(
    std::string_view           path,
    Fn&&                       callback,
    ErrFn&&                    on_error = {},
    std::pmr::memory_resource* mem      = std::pmr::get_default_resource());

} // namespace v1
} // namespace dir_hash
```

- **`callback`** — invoked once per node as its hash is finalised, with
  `(std::string_view path, const std::array<uint8_t, 32>& hash, bool is_dir)`.
  The `path` is valid only for the duration of the callback; copy it if
  you need to retain it.
- **`on_error`** — invoked for files or directories that cannot be read,
  with `(std::string_view path, std::error_code ec)`. The offending node
  is skipped and does not contribute to its parent's hash. Default is a
  no-op.
- **`mem`** — controls all internal allocations (path buffer, child-hash
  accumulators, read buffer). Pass any `std::pmr::memory_resource*`.
- **Returns** the root directory's hash.
- **Throws** `std::system_error` if the root path cannot be opened or
  is not a directory.

### Examples

Root hash only:

```cpp
auto root = dir_hash::hash_directory("/backup/2024-01",
    [](std::string_view, const std::array<uint8_t, 32>&, bool) {});
```

Print every node:

```cpp
dir_hash::hash_directory("/backup/2024-01",
    [](std::string_view path, const std::array<uint8_t, 32>& hash, bool is_dir) {
        std::printf("%s  %.*s\n", to_hex(hash).c_str(),
                    static_cast<int>(path.size()), path.data());
    });
```

Compare two snapshots, with error reporting:

```cpp
using HashMap = std::unordered_map<std::string, std::array<uint8_t, 32>>;

auto collect = [](std::string_view dir) {
    HashMap hashes;
    dir_hash::hash_directory(dir,
        [&](std::string_view path, const std::array<uint8_t, 32>& hash, bool is_dir) {
            if (is_dir) {
                hashes.emplace(std::string(path), hash);
            }
        },
        [](std::string_view path, std::error_code ec) {
            std::fprintf(stderr, "error: %.*s: %s\n",
                         static_cast<int>(path.size()), path.data(),
                         ec.message().c_str());
        });
    return hashes;
};

auto jan = collect("/backup/2024-01");
auto feb = collect("/backup/2024-02");
for (auto& [path, hash] : jan) {
    auto it = feb.find(path);
    if (it == feb.end() || it->second != hash) {
        std::printf("changed: %s\n", path.c_str());
    }
}
```

Custom allocator (monotonic arena):

```cpp
std::pmr::monotonic_buffer_resource arena(64 * 1024);
auto root = dir_hash::hash_directory("/backup/2024-01",
    [](std::string_view, const std::array<uint8_t, 32>&, bool) {},
    {},        // default on_error
    &arena);
```

---

## Algorithm

- **File hash:** BLAKE3 over the full file contents.
- **Directory hash:** BLAKE3 over the lexicographically-sorted byte
  concatenation of its children's 32-byte hashes. Symlinks and other
  non-regular entries are excluded. Empty directories hash to BLAKE3 of
  an empty input.

The directory hash is name-agnostic by construction: two directories
whose files have identical content — regardless of filenames or
arrangement — produce the same hash.

---

## I/O behaviour

Every file opened for hashing receives two `posix_fadvise` calls:

1. `POSIX_FADV_SEQUENTIAL` immediately after `open` — hints the
   prefetcher.
2. `POSIX_FADV_DONTNEED` before `close` — tells the kernel the data is
   no longer needed in cache.

Both are POSIX. On FreeBSD this protects the ZFS ARC working set; on
Linux it prevents page-cache pollution from bulk scans.

Files are read in 4 MiB buffers and fed to the BLAKE3 hasher
incrementally.

---

## Layout

```
include/dir_hash.hpp           Public header (header-only).
tools/dirhash.cpp              Command-line frontend.
man/linux/                     Man pages, man(7) macros (GNUmakefile).
man/freebsd/                   Man pages, mdoc(7) macros (BSDmakefile).
third_party/blake3/            Vendored BLAKE3 reference implementation.
tests/                         Self-contained test binaries.
GNUmakefile / BSDmakefile      Per-platform build.
Plan.md                        Design specification.
```

Tests cover correctness against manually-computed hashes,
name-agnosticism, empty and single-file directories, unreadable file
skipping with `on_error`, bad root throwing `std::system_error`, and
allocator discipline (default resource set to abort-on-use).

---

## Licensing

The dir-hash code is the user's. BLAKE3 is dual-licensed under
Apache-2.0-with-LLVM-exception and CC0-1.0; see
`third_party/blake3/LICENSE_A2LLVM` and
`third_party/blake3/LICENSE_CC0`.
