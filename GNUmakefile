# GNU make build (Linux). Use: gmake (or make) test
#
# BLAKE3 is built portable-only; SIMD impls can be added by listing the
# corresponding .c files with appropriate -m flags.

CXX      ?= c++
CC       ?= cc
CXXFLAGS ?= -O2 -g -Wall -Wextra
CFLAGS   ?= -O2 -g -Wall -Wextra

CXXSTD   = -std=c++17
INCLUDES = -Iinclude -Ithird_party/blake3
BLAKE3_DEFS = -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 \
              -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512

BUILD = build

BLAKE3_SRCS = third_party/blake3/blake3.c \
              third_party/blake3/blake3_dispatch.c \
              third_party/blake3/blake3_portable.c

BLAKE3_OBJS = $(patsubst third_party/blake3/%.c,$(BUILD)/blake3/%.o,$(BLAKE3_SRCS))

TESTS = test_basic test_empty test_errors test_allocator
TEST_BINS = $(addprefix $(BUILD)/,$(TESTS))

.PHONY: all test clean
all: $(TEST_BINS)

test: $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "==> $$t"; $$t; done

$(BUILD)/blake3/%.o: third_party/blake3/%.c | $(BUILD)/blake3
	$(CC) $(CFLAGS) $(BLAKE3_DEFS) -Ithird_party/blake3 -c -o $@ $<

$(BUILD)/%: tests/%.cpp $(BLAKE3_OBJS) include/dir_hash.hpp tests/test_helpers.hpp | $(BUILD)
	$(CXX) $(CXXSTD) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(BLAKE3_OBJS)

$(BUILD) $(BUILD)/blake3:
	@mkdir -p $@

clean:
	rm -rf $(BUILD)
