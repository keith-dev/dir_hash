# dir-hash — Project Status

*Last updated: 2026-05-18*

## State: complete — v1.2.0

All items in Plan.md have been implemented and are passing.

## What exists

| Component | Status |
|-----------|--------|
| `include/dir_hash.hpp` | Complete. Header-only, C++17, PMR allocator support. `hash_directory` accepts files as well as directories. |
| `tools/dirhash.cpp` | Complete. `md5sum`-style CLI frontend. Single dispatch path for files and directories. |
| `man/linux/` | Complete. `man(7)` macros, installed by GNUmakefile. |
| `man/freebsd/` | Complete. `mdoc(7)` macros, installed by BSDmakefile. |
| `GNUmakefile` / `BSDmakefile` | Complete. `all`, `test`, `clean`, `install`, `uninstall` targets. SSE2/SSE4.1/AVX2 BLAKE3 SIMD enabled; Termux/Android falls back to portable. |
| `pkg-config` | Complete. `dir_hash.pc` declares `Requires: libblake3`. |
| Tests | Complete. Correctness, name-agnosticism, error handling, allocator discipline. Termux-safe `$TMPDIR` handling. |

## Authorship

[![human input: collaborated](https://raw.githubusercontent.com/keith-dev/badges-repo/master/badge-collaborated.svg)](https://github.com/keith-dev/badges-repo)

Human-driven design spec (Plan.md), AI-assisted implementation, human review and refinement. See [badges-repo](https://github.com/keith-dev/badges-repo) for the scale.

## Known limitations / non-goals (by design)

- Serial traversal only — no parallel hashing.
- No incremental/cached operation; every call is a full traversal.
- Symlinks excluded from hashing.
- No diffing or reporting beyond the callback and return value.
- No multi-block-size hashing (BLAKE3 whole-file only).

## Possible future work

- A diff subcommand for `dirhash` to compare two roots directly.
- Optional parallel hashing for SSD workloads.
- Windows port (replace `fts(3)` and `posix_fadvise`).
