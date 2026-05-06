# GNU make build (Linux). Use: gmake (or make) test
#
# BLAKE3 is built portable-only; SIMD impls can be added by listing the
# corresponding .c files with appropriate -m flags.

CXX      ?= c++
CC       ?= cc
CXXFLAGS ?= -O2 -g -Wall -Wextra
CFLAGS   ?= -O2 -g -Wall -Wextra

PREFIX       ?= /usr/local
DESTDIR      ?=
BINDIR       ?= $(PREFIX)/bin
INCLUDEDIR   ?= $(PREFIX)/include
LIBDIR       ?= $(PREFIX)/lib
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
MANDIR       ?= $(PREFIX)/share/man
INSTALL      ?= install
MANSRC        = man/linux

VERSION = 1.1.0

CXXSTD   = -std=c++17
INCLUDES = -Iinclude -Ithird_party/blake3
BLAKE3_DEFS = -DBLAKE3_NO_AVX512

BUILD = build

BLAKE3_SRCS = third_party/blake3/blake3.c \
              third_party/blake3/blake3_dispatch.c \
              third_party/blake3/blake3_portable.c \
              third_party/blake3/blake3_sse2.c \
              third_party/blake3/blake3_sse41.c \
              third_party/blake3/blake3_avx2.c

BLAKE3_OBJS = $(patsubst third_party/blake3/%.c,$(BUILD)/blake3/%.o,$(BLAKE3_SRCS))

TESTS = test_basic test_empty test_errors test_allocator
TEST_BINS = $(addprefix $(BUILD)/,$(TESTS))

DIRHASH = $(BUILD)/dirhash

.PHONY: all test clean install uninstall
all: $(TEST_BINS) $(DIRHASH)

test: $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "==> $$t"; $$t; done

$(BUILD)/blake3/blake3_sse2.o: third_party/blake3/blake3_sse2.c | $(BUILD)/blake3
	$(CC) $(CFLAGS) $(BLAKE3_DEFS) -msse2 -Ithird_party/blake3 -c -o $@ $<

$(BUILD)/blake3/blake3_sse41.o: third_party/blake3/blake3_sse41.c | $(BUILD)/blake3
	$(CC) $(CFLAGS) $(BLAKE3_DEFS) -msse4.1 -Ithird_party/blake3 -c -o $@ $<

$(BUILD)/blake3/blake3_avx2.o: third_party/blake3/blake3_avx2.c | $(BUILD)/blake3
	$(CC) $(CFLAGS) $(BLAKE3_DEFS) -mavx2 -Ithird_party/blake3 -c -o $@ $<

$(BUILD)/blake3/%.o: third_party/blake3/%.c | $(BUILD)/blake3
	$(CC) $(CFLAGS) $(BLAKE3_DEFS) -Ithird_party/blake3 -c -o $@ $<

$(BUILD)/%: tests/%.cpp $(BLAKE3_OBJS) include/dir_hash.hpp tests/test_helpers.hpp | $(BUILD)
	$(CXX) $(CXXSTD) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(BLAKE3_OBJS)

$(DIRHASH): tools/dirhash.cpp $(BLAKE3_OBJS) include/dir_hash.hpp | $(BUILD)
	$(CXX) $(CXXSTD) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(BLAKE3_OBJS)

$(BUILD) $(BUILD)/blake3:
	@mkdir -p $@

clean:
	rm -rf $(BUILD)

install: $(DIRHASH)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(DIRHASH) $(DESTDIR)$(BINDIR)/dirhash
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL) -m 644 include/dir_hash.hpp $(DESTDIR)$(INCLUDEDIR)/dir_hash.hpp
	$(INSTALL) -d $(DESTDIR)$(MANDIR)/man1 $(DESTDIR)$(MANDIR)/man3
	$(INSTALL) -m 644 $(MANSRC)/dirhash.1 $(DESTDIR)$(MANDIR)/man1/dirhash.1
	$(INSTALL) -m 644 $(MANSRC)/dir_hash.3 $(DESTDIR)$(MANDIR)/man3/dir_hash.3
	$(INSTALL) -d $(DESTDIR)$(PKGCONFIGDIR)
	@printf '%s\n' \
	    'prefix=$(PREFIX)' \
	    'exec_prefix=$${prefix}' \
	    'includedir=$${prefix}/include' \
	    '' \
	    'Name: dir_hash' \
	    'Description: Content-based directory tree hashing with BLAKE3' \
	    'Version: $(VERSION)' \
	    'Requires: libblake3' \
	    'Cflags: -I$${includedir}' \
	    > $(DESTDIR)$(PKGCONFIGDIR)/dir_hash.pc

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/dirhash
	rm -f $(DESTDIR)$(INCLUDEDIR)/dir_hash.hpp
	rm -f $(DESTDIR)$(MANDIR)/man1/dirhash.1
	rm -f $(DESTDIR)$(MANDIR)/man3/dir_hash.3
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/dir_hash.pc
