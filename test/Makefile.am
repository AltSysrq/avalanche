AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

TESTS = \
runtime/test-array-list.t \
runtime/test-cxx-include.t \
runtime/test-empty-list.t \
runtime/test-esba-list.t \
runtime/test-esba.t \
runtime/test-exception.t \
runtime/test-function.t \
runtime/test-hash-map.t \
runtime/test-integer.t \
runtime/test-intrinsics-fundamental.t \
runtime/test-lex.t \
runtime/test-list-map.t \
runtime/test-list-proj.t \
runtime/test-list.t \
runtime/test-macsub.t \
runtime/test-map.t \
runtime/test-name-mangle.t \
runtime/test-parser.t \
runtime/test-pcode.t \
runtime/test-pointer.t \
runtime/test-real.t \
runtime/test-string.t \
runtime/test-symbol-table.t \
runtime/test-value.t

BENCHMARKS = benchmarks/list/avalanche.t benchmarks/map/avalanche.t

EXTRA_CFLAGS =
AM_CFLAGS = $(CHECK_CFLAGS) -I../src $(EXTRA_CFLAGS) -funwind-tables
AM_CXXFLAGS = $(CHECK_CFLAGS) -I../src $(EXTRA_CFLAGS) -funwind-tables
AM_LDFLAGS = $(CHECK_LIBS) ../src/libavart.la

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS =  $(BENCHMARKS)

runtime_test_cxx_include_t_SOURCES = runtime/test-cxx-include.cxx

benchmarks_list_avalanche_t_SOURCES = benchmarks/list/avalanche.c
benchmarks_map_avalanche_t_SOURCES = benchmarks/map/avalanche.c
