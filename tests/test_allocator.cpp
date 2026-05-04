// Custom pmr resource: verify the library does not allocate from the default
// resource. We pass a tracking resource that records allocations and set the
// default resource to one that aborts on use.

#include "dir_hash.hpp"
#include "test_helpers.hpp"

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory_resource>
#include <string>

using Hash = std::array<std::uint8_t, 32>;

namespace {

class CountingResource : public std::pmr::memory_resource {
public:
    explicit CountingResource(std::pmr::memory_resource* upstream)
        : upstream_(upstream) {}
    std::size_t allocations = 0;
    std::size_t bytes = 0;

private:
    void* do_allocate(std::size_t b, std::size_t a) override {
        ++allocations;
        bytes += b;
        return upstream_->allocate(b, a);
    }
    void do_deallocate(void* p, std::size_t b, std::size_t a) override {
        upstream_->deallocate(p, b, a);
    }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override {
        return this == &o;
    }
    std::pmr::memory_resource* upstream_;
};

class AbortResource : public std::pmr::memory_resource {
private:
    void* do_allocate(std::size_t, std::size_t) override {
        std::fprintf(stderr,
                     "FAIL: allocation reached default pmr resource\n");
        std::abort();
    }
    void do_deallocate(void*, std::size_t, std::size_t) override {}
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override {
        return this == &o;
    }
};

} // namespace

int main() {
    auto root = test::make_tmpdir();
    test::write_file(root + "/a", "aaa");
    test::write_file(root + "/b", "bbb");
    test::mkd(root + "/sub");
    test::write_file(root + "/sub/c", "ccc");

    AbortResource abort_res;
    std::pmr::set_default_resource(&abort_res);

    CountingResource counting(std::pmr::new_delete_resource());

    Hash got = dir_hash::hash_directory(
        root,
        [](std::string_view, const Hash&, bool) {},
        dir_hash::detail::NoopError{},
        &counting);

    REQUIRE(counting.allocations > 0);

    // Reset default before tearing down (rmtree may need allocator).
    std::pmr::set_default_resource(std::pmr::new_delete_resource());
    test::rmtree(root);

    (void)got;
    std::printf("test_allocator OK (%zu allocations, %zu bytes)\n",
                counting.allocations, counting.bytes);
    return 0;
}
