// Unreadable file: skipped, on_error invoked, traversal continues.

#include "dir_hash.hpp"
#include "test_helpers.hpp"

#include <array>
#include <cstdio>
#include <string>
#include <system_error>
#include <vector>

#include <sys/stat.h>

using Hash = std::array<std::uint8_t, 32>;

int main() {
    if (::geteuid() == 0) {
        std::printf("test_errors SKIP (running as root, mode 000 still readable)\n");
        return 0;
    }

    auto root = test::make_tmpdir();
    test::write_file(root + "/ok.txt", "hello");
    test::write_file(root + "/bad.txt", "secret");
    ::chmod((root + "/bad.txt").c_str(), 0);

    int errors = 0;
    std::string err_path;
    int file_cbs = 0;

    Hash got = dir_hash::hash_directory(
        root,
        [&](std::string_view, const Hash&, bool is_dir) {
            if (!is_dir) ++file_cbs;
        },
        [&](std::string_view p, std::error_code) {
            ++errors;
            err_path.assign(p.data(), p.size());
        });

    REQUIRE(errors == 1);
    REQUIRE(file_cbs == 1); // only ok.txt contributed
    REQUIRE(err_path.find("bad.txt") != std::string::npos);

    // Hash should be BLAKE3 of BLAKE3("hello") only (bad file excluded).
    Hash hh = test::blake3_of(std::string("hello"));
    Hash expected = test::blake3_of(hh.data(), hh.size());
    REQUIRE(got == expected);

    // Fatal: nonexistent root throws.
    bool threw = false;
    try {
        dir_hash::hash_directory(
            "/nonexistent/path/should/not/exist/xyzzy",
            [](std::string_view, const Hash&, bool) {});
    } catch (const std::system_error&) {
        threw = true;
    }
    REQUIRE(threw);

    ::chmod((root + "/bad.txt").c_str(), 0644);
    test::rmtree(root);
    std::printf("test_errors OK\n");
    return 0;
}
