#ifndef DIR_HASH_HPP_INCLUDED
#define DIR_HASH_HPP_INCLUDED

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <fts.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" {
#include "blake3.h"
}

namespace dir_hash {
inline namespace v1 {

namespace detail {

struct NoopError {
    void operator()(std::string_view, std::error_code) const noexcept {}
};

using Hash = std::array<std::uint8_t, 32>;

inline std::error_code make_ec(int e) noexcept {
    return std::error_code(e, std::system_category());
}

template <typename ErrFn>
inline bool hash_file(const char* accpath, std::string_view report_path,
                      Hash& out, ErrFn& on_error,
                      std::uint8_t* buffer, std::size_t buffer_size) {
    int fd = ::open(accpath, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        on_error(report_path, make_ec(errno));
        return false;
    }
    ::posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

    blake3_hasher h;
    blake3_hasher_init(&h);

    bool ok = true;
    for (;;) {
        ssize_t n = ::read(fd, buffer, buffer_size);
        if (n > 0) {
            blake3_hasher_update(&h, buffer, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        on_error(report_path, make_ec(errno));
        ok = false;
        break;
    }

    ::posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    ::close(fd);

    if (ok) {
        blake3_hasher_finalize(&h, out.data(), out.size());
    }
    return ok;
}

inline Hash combine_directory(std::pmr::vector<Hash>& children) {
    std::sort(children.begin(), children.end());
    blake3_hasher h;
    blake3_hasher_init(&h);
    for (const auto& c : children) {
        blake3_hasher_update(&h, c.data(), c.size());
    }
    Hash out;
    blake3_hasher_finalize(&h, out.data(), out.size());
    return out;
}

} // namespace detail

// Hash a single regular file. Returns false and calls on_error if the file
// cannot be opened or read. Uses the supplied buffer for I/O; if buf/buf_size
// are omitted a temporary 4 MiB heap buffer is used.
template <typename ErrFn = detail::NoopError>
inline bool hash_file(
    std::string_view           path,
    detail::Hash&              out,
    ErrFn&&                    on_error  = {},
    std::pmr::memory_resource* mem       = std::pmr::get_default_resource())
{
    constexpr std::size_t kBufSize = 4 * 1024 * 1024;
    std::pmr::vector<std::uint8_t> buf(kBufSize, mem);
    return detail::hash_file(std::string(path).c_str(), path, out,
                             on_error, buf.data(), buf.size());
}

template <typename Fn, typename ErrFn = detail::NoopError>
std::array<std::uint8_t, 32> hash_directory(
    std::string_view           path,
    Fn&&                       callback,
    ErrFn&&                    on_error = {},
    std::pmr::memory_resource* mem      = std::pmr::get_default_resource())
{
    using detail::Hash;

    std::pmr::string root_path(mem);
    root_path.assign(path.data(), path.size());

    char* argv[2];
    argv[0] = root_path.data();
    argv[1] = nullptr;

    FTS* fts = ::fts_open(argv, FTS_PHYSICAL | FTS_NOCHDIR | FTS_COMFOLLOW, nullptr);
    if (!fts) {
        throw std::system_error(detail::make_ec(errno),
                                "dir_hash::hash_directory: fts_open");
    }

    constexpr std::size_t kBufSize = 4 * 1024 * 1024;
    std::pmr::vector<std::uint8_t> buf(kBufSize, mem);

    std::pmr::vector<std::pmr::vector<Hash>> stack(mem);

    Hash root_hash{};
    bool got_root = false;
    std::error_code root_error;

    FTSENT* ent;
    errno = 0;
    while ((ent = ::fts_read(fts)) != nullptr) {
        std::string_view ent_path(ent->fts_path, ent->fts_pathlen);

        switch (ent->fts_info) {
        case FTS_D:
            stack.emplace_back();
            break;

        case FTS_DP: {
            std::pmr::vector<Hash> children = std::move(stack.back());
            stack.pop_back();
            Hash dh = detail::combine_directory(children);
            callback(ent_path, dh, true);
            if (!stack.empty()) {
                stack.back().push_back(dh);
            } else {
                root_hash = dh;
                got_root = true;
            }
            break;
        }

        case FTS_F: {
            Hash fh;
            if (detail::hash_file(ent->fts_accpath, ent_path, fh, on_error,
                                  buf.data(), buf.size())) {
                callback(ent_path, fh, false);
                if (!stack.empty()) {
                    stack.back().push_back(fh);
                } else {
                    // Plain file passed as the root path.
                    root_hash = fh;
                    got_root  = true;
                }
            }
            break;
        }

        case FTS_SL:
        case FTS_SLNONE:
        case FTS_DEFAULT:
            break;

        case FTS_DNR:
        case FTS_ERR:
        case FTS_NS: {
            std::error_code ec = detail::make_ec(ent->fts_errno);
            if (ent->fts_level <= 0) {
                root_error = ec;
            } else {
                on_error(ent_path, ec);
            }
            break;
        }

        default:
            break;
        }
        errno = 0;
    }

    int read_errno = errno;
    ::fts_close(fts);

    if (!got_root) {
        if (root_error) {
            throw std::system_error(root_error,
                                    "dir_hash::hash_directory: root unreadable");
        }
        if (read_errno) {
            throw std::system_error(detail::make_ec(read_errno),
                                    "dir_hash::hash_directory: fts_read");
        }
        // Path was an unsupported type (symlink, device, etc.).
        throw std::system_error(detail::make_ec(ENOTDIR),
                                "dir_hash::hash_directory: not a file or directory");
    }

    return root_hash;
}

} // namespace v1
} // namespace dir_hash

#endif
