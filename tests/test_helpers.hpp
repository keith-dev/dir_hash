#ifndef DIR_HASH_TEST_HELPERS_HPP
#define DIR_HASH_TEST_HELPERS_HPP

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" {
#include "blake3.h"
}

#define REQUIRE(cond)                                                    \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
                         #cond);                                         \
            std::exit(1);                                                \
        }                                                                \
    } while (0)

namespace test {

inline std::array<std::uint8_t, 32> blake3_of(const void* data,
                                              std::size_t len) {
    blake3_hasher h;
    blake3_hasher_init(&h);
    blake3_hasher_update(&h, data, len);
    std::array<std::uint8_t, 32> out;
    blake3_hasher_finalize(&h, out.data(), out.size());
    return out;
}

inline std::array<std::uint8_t, 32> blake3_of(const std::string& s) {
    return blake3_of(s.data(), s.size());
}

inline std::string make_tmpdir() {
    char tmpl[] = "/tmp/dirhash-test-XXXXXX";
    char* p = ::mkdtemp(tmpl);
    if (!p) {
        std::perror("mkdtemp");
        std::exit(1);
    }
    return std::string(p);
}

inline void write_file(const std::string& path, const std::string& contents) {
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        std::perror(("open " + path).c_str());
        std::exit(1);
    }
    const char* p = contents.data();
    std::size_t left = contents.size();
    while (left) {
        ssize_t n = ::write(fd, p, left);
        if (n < 0) {
            std::perror("write");
            std::exit(1);
        }
        p += n;
        left -= static_cast<std::size_t>(n);
    }
    ::close(fd);
}

inline void mkd(const std::string& path) {
    if (::mkdir(path.c_str(), 0755) != 0) {
        std::perror(("mkdir " + path).c_str());
        std::exit(1);
    }
}

inline int rm_cb(const char* fpath, const struct stat*, int, struct FTW*) {
    ::chmod(fpath, 0700);
    return ::remove(fpath);
}

inline void rmtree(const std::string& path) {
    ::nftw(path.c_str(), rm_cb, 16, FTW_DEPTH | FTW_PHYS);
}

} // namespace test

#endif
