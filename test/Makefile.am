AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

# Since the tests aren't runnable when cross-compiling anyway, just use the
# bootstrap executables directly.
AVA_COMPILE_MODULE=../src/bootstrap/bin/compile-module-std$(EXEEXT)
AVA_LINK_PACKAGE=../src/bootstrap/bin/link-package$(EXEEXT)
AVA_TO_ASM=../src/bootstrap/bin/to-asm$(EXEEXT)

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
runtime/test-interval.t \
runtime/test-intrinsics-fundamental.t \
runtime/test-lex.t \
runtime/test-list-map.t \
runtime/test-list-proj.t \
runtime/test-list.t \
runtime/test-macsub.t \
runtime/test-map.t \
runtime/test-module-cache.t \
runtime/test-name-mangle.t \
runtime/test-parser.t \
runtime/test-pcode.t \
runtime/test-pcode-linker.t \
runtime/test-pcode-validation.t \
runtime/test-pointer.t \
runtime/test-real.t \
runtime/test-string.t \
runtime/test-struct.t \
runtime/test-symtab.t \
runtime/test-value.t \
runtime/test-varscope.t \
runtime/test-avalanche.t \
runtime/test-compiled.t

runtime_test_compiled_t_SOURCES = \
	runtime/test-compiled.c ava-tests.S dummy.cxx

ava-tests.S: compile-ava-tests.sh ava-tests/*.ava ava-tests/helpers/*.ava \
		$(AVA_TO_ASM) $(AVA_COMPILE_MODULE) $(AVA_LINK_PACKAGE)
	$(AM_V_GEN)./compile-ava-tests.sh

BENCHMARKS = benchmarks/list/avalanche.t benchmarks/map/avalanche.t \
	benchmarks/string/length.t

EXTRA_CFLAGS =
AM_CFLAGS = $(CHECK_CFLAGS) -I../src $(EXTRA_CFLAGS) -funwind-tables
AM_CXXFLAGS = $(CHECK_CFLAGS) -I../src $(EXTRA_CFLAGS) -funwind-tables
AM_LDFLAGS = -export-dynamic $(CHECK_LIBS) \
	../src/libavart.la ../src/libavart-native.la
AM_CCASFLAGS = -g0

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS =  $(BENCHMARKS)

runtime_test_cxx_include_t_SOURCES = runtime/test-cxx-include.cxx

benchmarks_list_avalanche_t_SOURCES = benchmarks/list/avalanche.c
benchmarks_map_avalanche_t_SOURCES = benchmarks/map/avalanche.c
benchmarks_string_length_t_SOURCES = benchmarks/string/length.c

clean-local:
	find . \( -name \*.avam -o -name \*.avap -o -name \*.avapi \) -delete
	find . \( -name \*.S \) -delete
