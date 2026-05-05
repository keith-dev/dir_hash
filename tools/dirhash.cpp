// dirhash — md5sum-style command-line frontend for dir_hash.
//
//   dirhash [-r] [-t] [-e] path...
//
// Default output (md5sum-compatible):
//     <hex>  <path>
//     <hex>  <path>/        (for directories in recursive mode)
//
// Tag output (-t, BSD md5-style):
//     BLAKE3 (<path>) = <hex>

#include "dir_hash.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

using Hash = std::array<std::uint8_t, 32>;

bool g_tag_format = false;

std::string to_hex(const Hash& h) {
    static const char d[] = "0123456789abcdef";
    std::string s(64, '\0');
    for (std::size_t i = 0; i < 32; ++i) {
        s[2 * i]     = d[h[i] >> 4];
        s[2 * i + 1] = d[h[i] & 0xf];
    }
    return s;
}

void print_line(const Hash& h, std::string_view path, bool is_dir) {
    std::string hex = to_hex(h);
    if (g_tag_format) {
        if (is_dir) {
            std::printf("BLAKE3 (%.*s/) = %s\n",
                        static_cast<int>(path.size()), path.data(),
                        hex.c_str());
        } else {
            std::printf("BLAKE3 (%.*s) = %s\n",
                        static_cast<int>(path.size()), path.data(),
                        hex.c_str());
        }
    } else {
        if (is_dir) {
            std::printf("%s  %.*s/\n", hex.c_str(),
                        static_cast<int>(path.size()), path.data());
        } else {
            std::printf("%s  %.*s\n", hex.c_str(),
                        static_cast<int>(path.size()), path.data());
        }
    }
}



void usage(std::FILE* f) {
    std::fprintf(f,
        "usage: dirhash [-r] [-t] [-e] path...\n"
        "\n"
        "  -r    print hashes for every node, not just the roots\n"
        "  -t    BSD-style tagged output: BLAKE3 (path) = <hex>\n"
        "  -e    print per-file I/O errors to stderr\n"
        "  -h    show this help\n"
        "\n"
        "Each path may be a regular file or a directory. Directories are\n"
        "hashed content-recursively (BLAKE3 over the sorted multiset of\n"
        "child hashes). Symlinks are skipped.\n");
}

} // namespace

int main(int argc, char** argv) {
    bool recursive     = false;
    bool report_errors = false;

    int opt;
    while ((opt = ::getopt(argc, argv, "rteh")) != -1) {
        switch (opt) {
        case 'r': recursive = true; break;
        case 't': g_tag_format = true; break;
        case 'e': report_errors = true; break;
        case 'h': usage(stdout); return 0;
        default:  usage(stderr); return 2;
        }
    }

    if (optind >= argc) {
        usage(stderr);
        return 2;
    }

    int exit_code = 0;

    for (int i = optind; i < argc; ++i) {
        const char* path = argv[i];

        struct stat st;
        if (::lstat(path, &st) != 0) {
            std::fprintf(stderr, "dirhash: %s: %s\n",
                         path, std::strerror(errno));
            exit_code = 1;
            continue;
        }

        if (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)) {
            try {
                Hash root = dir_hash::hash_directory(
                    path,
                    [&](std::string_view p, const Hash& h, bool is_dir) {
                        if (recursive) {
                            print_line(h, p, is_dir);
                        }
                    },
                    [&](std::string_view p, std::error_code ec) {
                        exit_code = 1;
                        if (report_errors) {
                            std::fprintf(stderr, "dirhash: %.*s: %s\n",
                                         static_cast<int>(p.size()),
                                         p.data(),
                                         ec.message().c_str());
                        }
                    });
                if (!recursive || S_ISREG(st.st_mode)) {
                    print_line(root, path, S_ISDIR(st.st_mode));
                }
            } catch (const std::system_error& e) {
                std::fprintf(stderr, "dirhash: %s: %s\n",
                             path, e.code().message().c_str());
                exit_code = 1;
            }
        } else {
            std::fprintf(stderr,
                         "dirhash: %s: not a regular file or directory\n",
                         path);
            exit_code = 1;
        }
    }

    return exit_code;
}
