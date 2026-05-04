# BSD make build (FreeBSD). Use: make test
#
# BLAKE3 is built portable-only; SIMD impls can be added by listing the
# corresponding .c files with appropriate -m flags.

CXX     ?= c++
CC      ?= cc
CXXFLAGS ?= -O2 -g -Wall -Wextra
CFLAGS   ?= -O2 -g -Wall -Wextra

CXXSTD    = -std=c++17
INCLUDES  = -Iinclude -Ithird_party/blake3
BLAKE3_DEFS = -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 \
              -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512

BUILD = build

BLAKE3_OBJS = ${BUILD}/blake3/blake3.o \
              ${BUILD}/blake3/blake3_dispatch.o \
              ${BUILD}/blake3/blake3_portable.o

TESTS = test_basic test_empty test_errors test_allocator
TEST_BINS = ${TESTS:S,^,${BUILD}/,}

.PHONY: all test clean

all: ${TEST_BINS}

test: ${TEST_BINS}
.for t in ${TEST_BINS}
	@echo "==> ${t}"; ${t}
.endfor

${BUILD}/blake3/blake3.o: third_party/blake3/blake3.c
	@mkdir -p ${BUILD}/blake3
	${CC} ${CFLAGS} ${BLAKE3_DEFS} -Ithird_party/blake3 -c -o ${.TARGET} ${.ALLSRC}

${BUILD}/blake3/blake3_dispatch.o: third_party/blake3/blake3_dispatch.c
	@mkdir -p ${BUILD}/blake3
	${CC} ${CFLAGS} ${BLAKE3_DEFS} -Ithird_party/blake3 -c -o ${.TARGET} ${.ALLSRC}

${BUILD}/blake3/blake3_portable.o: third_party/blake3/blake3_portable.c
	@mkdir -p ${BUILD}/blake3
	${CC} ${CFLAGS} ${BLAKE3_DEFS} -Ithird_party/blake3 -c -o ${.TARGET} ${.ALLSRC}

.for t in ${TESTS}
${BUILD}/${t}: tests/${t}.cpp ${BLAKE3_OBJS} include/dir_hash.hpp tests/test_helpers.hpp
	@mkdir -p ${BUILD}
	${CXX} ${CXXSTD} ${CXXFLAGS} ${INCLUDES} -o ${.TARGET} tests/${t}.cpp ${BLAKE3_OBJS}
.endfor

clean:
	rm -rf ${BUILD}
