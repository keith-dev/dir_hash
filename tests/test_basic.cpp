// Basic correctness: known small trees with manually-computed hashes.

#include "dir_hash.hpp"
#include "test_helpers.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

using Hash = std::array<std::uint8_t, 32>;

static Hash combine(std::vector<Hash> children) {
    std::sort(children.begin(), children.end());
    blake3_hasher h;
    blake3_hasher_init(&h);
    for (auto& c : children) {
        blake3_hasher_update(&h, c.data(), c.size());
    }
    Hash out;
    blake3_hasher_finalize(&h, out.data(), out.size());
    return out;
}

int main() {
    // Tree:
    //  root/
    //    a.txt   = "alpha"
    //    b.txt   = "beta"
    //    sub/
    //      c.txt = "gamma"
    auto root = test::make_tmpdir();
    test::write_file(root + "/a.txt", "alpha");
    test::write_file(root + "/b.txt", "beta");
    test::mkd(root + "/sub");
    test::write_file(root + "/sub/c.txt", "gamma");

    Hash ha = test::blake3_of(std::string("alpha"));
    Hash hb = test::blake3_of(std::string("beta"));
    Hash hc = test::blake3_of(std::string("gamma"));
    Hash hsub = combine({hc});
    Hash hroot_expected = combine({ha, hb, hsub});

    int file_callbacks = 0;
    int dir_callbacks = 0;
    Hash got_root = dir_hash::hash_directory(
        root,
        [&](std::string_view, const Hash&, bool is_dir) {
            if (is_dir) {
                ++dir_callbacks;
            } else {
                ++file_callbacks;
            }
        });

    REQUIRE(file_callbacks == 3);
    REQUIRE(dir_callbacks == 2);
    REQUIRE(got_root == hroot_expected);

    // Name-agnostic: rename children, hash should be unchanged.
    auto root2 = test::make_tmpdir();
    test::write_file(root2 + "/zzz", "alpha");
    test::write_file(root2 + "/aaa", "beta");
    test::mkd(root2 + "/qqq");
    test::write_file(root2 + "/qqq/whatever", "gamma");

    Hash got_root2 = dir_hash::hash_directory(
        root2, [](std::string_view, const Hash&, bool) {});
    REQUIRE(got_root2 == hroot_expected);

    test::rmtree(root);
    test::rmtree(root2);
    std::printf("test_basic OK\n");
    return 0;
}
