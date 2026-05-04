// Empty directory and single-file directory.

#include "dir_hash.hpp"
#include "test_helpers.hpp"

#include <array>
#include <cstdio>
#include <string>

using Hash = std::array<std::uint8_t, 32>;

int main() {
    // Empty dir → BLAKE3 of empty input.
    auto empty = test::make_tmpdir();
    Hash expected_empty = test::blake3_of("", 0);
    Hash got_empty = dir_hash::hash_directory(
        empty, [](std::string_view, const Hash&, bool) {});
    REQUIRE(got_empty == expected_empty);

    // Single-file dir with content "x" → BLAKE3 of BLAKE3("x").
    auto one = test::make_tmpdir();
    test::write_file(one + "/only", "x");
    Hash hx = test::blake3_of(std::string("x"));
    Hash expected_one = test::blake3_of(hx.data(), hx.size());
    Hash got_one = dir_hash::hash_directory(
        one, [](std::string_view, const Hash&, bool) {});
    REQUIRE(got_one == expected_one);

    test::rmtree(empty);
    test::rmtree(one);
    std::printf("test_empty OK\n");
    return 0;
}
