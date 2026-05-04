# dir-hash — Design Plan

A standalone C++ library that walks a directory tree, hashes every file
with BLAKE3, and produces a content-based Merkle-like fingerprint for
every node — files and directories alike. Name-agnostic by design: only
content matters. Intended for comparing large directory trees (e.g.
backup snapshots) to identify which subtrees have changed.

**Status**: design phase. No code yet. This document is the agreed-upon
specification; it should be read in full before implementation begins.

---

## 1. Goals and non-goals

### Goals

- Accept a directory path, walk the tree with `fts(3)`, hash every
  regular file with BLAKE3 (whole file), and compute a Merkle-like hash
  for every directory bottom-up.
- Stream results to a caller-supplied callback as each node is
  finalised. The root directory hash is also returned directly.
- Be name-agnostic: directory hashes are derived from the sorted
  multiset of child content hashes, not from filenames or structure.
- Accept an explicit `std::pmr::memory_resource*` for all internal
  allocations. Default to `std::pmr::get_default_resource()`.
- Traverse and hash serially. The workload is I/O-bound on spinning
  disk; a single thread saturates the drive and keeps the library simple.
- Issue `posix_fadvise` hints on every file: `POSIX_FADV_SEQUENTIAL`
  on open, `POSIX_FADV_DONTNEED` before close. These are POSIX and work
  identically on FreeBSD (protecting the ZFS ARC) and Linux. Net effect:
  the library does not pollute the page cache with bulk-scan data.
- Be a self-contained library with no dependencies beyond BLAKE3 and
  the C++17 standard library.

### Non-goals

- Multi-block-size hashing (whole-file BLAKE3 only).
- Symlinks: excluded from hashing and from directory hash inputs.
- Extended attributes, permissions, ownership, timestamps. Content only.
- Incremental or cached operation. Each call is a full traversal.
- Reporting, diffing, or output beyond the callback and return value.

---

## 2. Algorithm

### File hash

BLAKE3 over the full file contents. 32-byte raw output.

### Directory hash

Post-order: all children are finalised before their parent.

1. Collect the 32-byte hashes of all direct children (regular files and
   subdirectories). Symlinks and other non-regular entries are skipped.
2. Sort the collected hashes lexicographically by raw byte value.
3. Feed the sorted sequence into a single BLAKE3 hasher and finalise.

Name-agnostic by construction: two directories whose files have
identical content — regardless of filenames or arrangement — produce the
same hash. Empty directories hash to BLAKE3 of an empty input.

---

## 3. Public interface

Single public header: `include/dir_hash.hpp`.

```cpp
namespace dir_hash {
inline namespace v1 {

namespace detail {
struct NoopError {
    void operator()(std::string_view, std::error_code) const noexcept {}
};
} // namespace detail

// Callback (Fn) signature:
//   void(std::string_view path,
//        const std::array<uint8_t, 32>& hash,
//        bool is_dir)
//
// Error callback (ErrFn) signature:
//   void(std::string_view path,
//        std::error_code ec)

template <typename Fn, typename ErrFn = detail::NoopError>
std::array<uint8_t, 32> hash_directory(
    std::string_view           path,
    Fn&&                       callback,
    ErrFn&&                    on_error = {},
    std::pmr::memory_resource* mem      = std::pmr::get_default_resource());

} // namespace v1
} // namespace dir_hash
```

### Parameter notes

- **`path`**: the directory to hash. Converted to a null-terminated
  string (using `mem`) before passing to `fts_open`.
- **`callback`**: template parameter (`Fn`) so the compiler can inline
  the callback into the traversal loop — relevant when called millions
  of times per run. Called once per node as its hash is finalised.
  The `std::string_view path` argument is valid only for the duration
  of the callback — callers must copy it if they need to retain it.
- **`on_error`**: template parameter (`ErrFn`). Called for files that
  cannot be read; they are skipped and do not contribute to their
  parent's hash. Default is a no-op.
- **`mem`**: governs all internal allocations (path buffer, child-hash
  accumulators). Does not affect caller-side allocations.

### Return value

The hash of the directory at `path`. Identical to the value delivered to
the callback for that node. Returned directly as a convenience so
callers that only need the root hash can pass a no-op callback.

If `path` cannot be opened, throws `std::system_error`.

### Versioning

The public API lives inside `inline namespace v1`. Both
`dir_hash::hash_directory` and `dir_hash::v1::hash_directory` resolve
to the same function. When a future revision changes the interface,
`inline` moves to the new namespace; clients that pinned to `v1`
explicitly continue to compile.

---

## 4. Client code examples

```cpp
#include "dir_hash.hpp"
#include <array>
#include <cstdio>
#include <string>
#include <system_error>
#include <unordered_map>


// 1. Root hash only — no interest in subtrees, errors silently skipped
auto root = dir_hash::hash_directory("/backup/2024-01",
    [](std::string_view, const std::array<uint8_t, 32>&, bool) {});


// 2. Print every node as it is hashed
dir_hash::hash_directory("/backup/2024-01",
    [](std::string_view path, const std::array<uint8_t, 32>& hash, bool is_dir) {
        std::printf("%s  %s\n", to_hex(hash).c_str(), path.data());
    });


// 3. Compare two backup snapshots with error reporting
using HashMap = std::unordered_map<std::string, std::array<uint8_t, 32>>;

auto collect = [](std::string_view dir) {
    HashMap hashes;
    dir_hash::hash_directory(dir,
        [&hashes](std::string_view path, const std::array<uint8_t, 32>& hash, bool is_dir) {
            if (is_dir)
                hashes.emplace(std::string(path), hash);
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
    if (it == feb.end() || it->second != hash)
        std::printf("changed: %s\n", path.c_str());
}


// 4. Custom allocator — library internals use a monotonic arena
std::pmr::monotonic_buffer_resource arena(64 * 1024);
auto root2 = dir_hash::hash_directory("/backup/2024-01",
    [](std::string_view, const std::array<uint8_t, 32>&, bool) {},
    {},       // default on_error
    &arena);
```

---

## 5. I/O behaviour

Every file opened for hashing receives two `posix_fadvise` calls:

1. `posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL)` immediately after
   `open` — hints to the prefetcher that the file will be read
   front-to-back.
2. `posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED)` before `close` —
   advises the kernel that the data is no longer needed in cache.

On FreeBSD this prevents scan reads from evicting the ZFS ARC working
set. On Linux it prevents pollution of the page cache. Both are POSIX;
no conditional compilation required.

Files are read in 4 MiB buffers, fed to the BLAKE3 hasher incrementally.

---

## 6. Error handling

Errors are represented using standard library classes:
`std::error_code` for per-file errors delivered to `on_error`, and
`std::system_error` for fatal startup failures. Both wrap `errno`
values via `std::system_category()`.

### Fatal (thrown)

- `fts_open` failure (bad path, permission denied on root).
- Thrown as `std::system_error` with the underlying `errno`.

### Per-file (reported via `on_error`, traversal continues)

| `errno` | Cause |
|---------|-------|
| `EACCES` | Permission denied on file or directory |
| `EIO` | I/O error during read |
| `ENOENT` | File disappeared between `fts` stat and `open` |
| `EMFILE` / `ENFILE` | Too many open files |

Unreadable files are skipped and do not contribute to their parent
directory's hash. Unreadable directories are skipped along with their
entire subtree.

---

## 7. Module layout

```
include/
  dir_hash.hpp      Public interface and implementation.

third_party/
  blake3/           BLAKE3 reference implementation (vendored).

tests/
  test_basic.cpp    Known directory trees with pre-computed hashes.
  test_empty.cpp    Empty directory, single-file directory.
  test_errors.cpp   Unreadable files, unreadable directories.
  test_allocator.cpp  Custom pmr resource; verify no default-allocator use.

BSDmakefile
```

Dependencies: `libblake3`. C++17. `fts(3)` available on FreeBSD and
Linux (glibc).

---

## 8. Implementation order

1. **Skeleton**: header with signature, no-op body, builds and links.
2. **File hashing**: open, fadvise, BLAKE3, fadvise, close. Test with
   known inputs.
3. **Traversal**: `fts(3)` walk, print paths. Verify post-order
   (`FTS_DP`) behaviour and symlink exclusion.
4. **Directory hash**: accumulate child hashes per directory, sort,
   finalise. Test with a small known tree.
5. **Callback integration**: wire up `Fn` callback; verify
   `string_view` lifetime contract.
6. **Error callback**: wire up `ErrFn`; test with unreadable files and
   directories.
7. **Allocator**: thread `mem` through all internal allocations; test
   with a custom `pmr` resource that asserts no allocation escapes to
   the default resource.
