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
runtime/test-lex.t \
runtime/test-list-map.t \
runtime/test-list-proj.t \
runtime/test-list.t \
runtime/test-map.t \
runtime/test-name-mangle.t \
runtime/test-parser.t \
runtime/test-pointer.t \
runtime/test-real.t \
runtime/test-string.t \
runtime/test-value.t

BENCHMARKS = benchmarks/list/avalanche.t benchmarks/map/avalanche.t

EXTRA_CFLAGS =
AM_CFLAGS = $(CHECK_CFLAGS) -I../src $(EXTRA_CFLAGS) -funwind-tables
AM_CXXFLAGS = $(CHECK_CFLAGS) -I../src $(EXTRA_CFLAGS) -funwind-tables
AM_LDFLAGS = $(CHECK_LIBS) ../src/libavart.la

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS =  $(BENCHMARKS)

runtime_test_array_list_t_SOURCES = runtime/test-array-list.c
runtime_test_cxx_include_t_SOURCES = runtime/test-cxx-include.cxx
runtime_test_empty_list_t_SOURCES = runtime/test-empty-list.c
runtime_test_esba_list_t_SOURCES = runtime/test-esba-list.c
runtime_test_esba_t_SOURCES = runtime/test-esba.c
runtime_test_exception_t_SOURCES = runtime/test-exception.c
runtime_test_function_t_SOURCES = runtime/test-function.c
runtime_test_hash_map_t_SOURCES = runtime/test-hash-map.c
runtime_test_integer_t_SOURCES = runtime/test-integer.c
runtime_test_lex_t_SOURCES = runtime/test-lex.c
runtime_test_list_map_t_SOURCES = runtime/test-list-map.c
runtime_test_list_proj_t_SOURCES = runtime/test-list-proj.c
runtime_test_list_t_SOURCES = runtime/test-list.c
runtime_test_name_mangle_t_SOURCES = runtime/test-name-mangle.c
runtime_test_parser_t_SOURCES = runtime/test-parser.c
runtime_test_pointer_t_SOURCES = runtime/test-pointer.c
runtime_test_real_t_SOURCES = runtime/test-real.c
runtime_test_string_t_SOURCES = runtime/test-string.c
runtime_test_value_t_SOURCES = runtime/test-value.c

benchmarks_list_avalanche_t_SOURCES = benchmarks/list/avalanche.c
benchmarks_map_avalanche_t_SOURCES = benchmarks/map/avalanche.c
