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
runtime/test-symtab.t \
runtime/test-value.t \
runtime/test-varscope.t \
runtime/test-avalanche.t \
runtime/test-compiled.t

runtime_test_compiled_t_SOURCES = \
	runtime/test-compiled.c ava-tests.S dummy.cxx

COMPILED_MODULES = \
ava-tests/alias-macro.avam \
ava-tests/alias.avam \
ava-tests/all-map-operator.avam \
ava-tests/basic-subscript-read.avam \
ava-tests/basic-subscript-write.avam \
ava-tests/brace-function-body-returns-empty.avam \
ava-tests/catch-format-ex.avam \
ava-tests/closure-direct-dd.avam \
ava-tests/closure-direct-sd.avam \
ava-tests/closure-direct-ss.avam \
ava-tests/closure-nested-ss.avam \
ava-tests/closure-transitive-dd.avam \
ava-tests/closure-transitive-sd.avam \
ava-tests/closure-transitive-ss.avam \
ava-tests/compound-subscript-read.avam \
ava-tests/compound-subscript-write.avam \
ava-tests/const-semilit.avam \
ava-tests/defun-named-arg.avam \
ava-tests/defun-opt-arg-explicit.avam \
ava-tests/defun-opt-arg.avam \
ava-tests/dependent-module-inits-are-run.avam \
ava-tests/dynamic-invoke-double-spread.avam \
ava-tests/dynamic-invoke-empty-then-spread.avam \
ava-tests/dynamic-invoke-function-from-after-spread.avam \
ava-tests/dynamic-invoke-function-from-second-spread.avam \
ava-tests/dynamic-invoke-spread-after-function.avam \
ava-tests/dynamic-invoke-spread-single.avam \
ava-tests/dynamic-invoke-spread-then-empty.avam \
ava-tests/elseless-subst-if-choose-first.avam \
ava-tests/elseless-subst-if-choose-nothing.avam \
ava-tests/empty-block.avam \
ava-tests/empty-lambda-returns-empty-string.avam \
ava-tests/empty-spread.avam \
ava-tests/extern-with-string-native-name.avam \
ava-tests/finally-run-at-end-of-nothrow-try.avam \
ava-tests/finally-run-before-break.avam \
ava-tests/finally-run-before-ret.avam \
ava-tests/fun-opt-varargs-pos.avam \
ava-tests/fun-optional-explicit-named-arg.avam \
ava-tests/fun-optional-named-arg.avam \
ava-tests/fun-varargs.avam \
ava-tests/helpers/assert.avam \
ava-tests/helpers/decl-pass-test.avam \
ava-tests/helpers/fourty-two.avam \
ava-tests/helpers/real-common.avam \
ava-tests/helpers/require-fourty-two.avam \
ava-tests/if-block-force.avam \
ava-tests/if-subst-force.avam \
ava-tests/import-implicit-dest.avam \
ava-tests/import-with-colon.avam \
ava-tests/import-with-explicit-dot.avam \
ava-tests/integer-ops.avam \
ava-tests/intrinsic-string-concat.avam \
ava-tests/lenient-map-operator.avam \
ava-tests/list-ops.avam \
ava-tests/local-var.avam \
ava-tests/loop-basic-continue.avam \
ava-tests/loop-break-bypasses-else.avam \
ava-tests/loop-break-provide-value.avam \
ava-tests/loop-break-suppress-value-change.avam \
ava-tests/loop-collect-run-on-update.avam \
ava-tests/loop-collect-spread.avam \
ava-tests/loop-collect.avam \
ava-tests/loop-collecting-do-subst.avam \
ava-tests/loop-collecting-nothing.avam \
ava-tests/loop-continue-no-change-value.avam \
ava-tests/loop-continue-provides-value.avam \
ava-tests/loop-default-returns-empty.avam \
ava-tests/loop-do-while.avam \
ava-tests/loop-double-stride-each.avam \
ava-tests/loop-else-eval-subst-replaces-accum.avam \
ava-tests/loop-else-run-block.avam \
ava-tests/loop-parallel-each.avam \
ava-tests/loop-parallel-for.avam \
ava-tests/loop-simple-for.avam \
ava-tests/loop-simple-until.avam \
ava-tests/loop-simple-while.avam \
ava-tests/loop-single-stride-each.avam \
ava-tests/lots-of-registers.avam \
ava-tests/lvalue-order-of-ops.avam \
ava-tests/macro-bang-subst.avam \
ava-tests/macro-expand-into-block.avam \
ava-tests/macro-expand-into-semilit.avam \
ava-tests/macro-gensym-subst.avam \
ava-tests/macro-percent-subst.avam \
ava-tests/macro-right-angle-plus-some-args.avam \
ava-tests/macro-right-angle-star-no-args.avam \
ava-tests/macro-right-angle-star-some-args.avam \
ava-tests/macro-slice-left-left-open.avam \
ava-tests/macro-slice-left-range-args.avam \
ava-tests/macro-slice-left-range-plus-args.avam \
ava-tests/macro-slice-left-range-star-args.avam \
ava-tests/macro-slice-left-range-star-no-args.avam \
ava-tests/macro-slice-left-right-open.avam \
ava-tests/macro-slice-right-left-open.avam \
ava-tests/macro-slice-right-range-args.avam \
ava-tests/macro-slice-right-right-open.avam \
ava-tests/macro-slice-single-left-from-left.avam \
ava-tests/macro-slice-single-left-from-right.avam \
ava-tests/macro-slice-single-left-leftmost.avam \
ava-tests/macro-slice-single-left-rightmost.avam \
ava-tests/macro-slice-single-right-from-left.avam \
ava-tests/macro-slice-single-right-from-right.avam \
ava-tests/macro-slice-single-right-leftmost.avam \
ava-tests/macro-slice-single-right-rightmost.avam \
ava-tests/map-ops.avam \
ava-tests/module-inits-only-run-once.avam \
ava-tests/multi-spread-into-varargs.avam \
ava-tests/multi-statement-lambda-returns-nothing.avam \
ava-tests/namespaces.avam \
ava-tests/nested-closure-lambda.avam \
ava-tests/nested-spread-constexpr.avam \
ava-tests/nested-spread.avam \
ava-tests/nested-stateless-lambda.avam \
ava-tests/pasta-minimal.avam \
ava-tests/pasta-more-complex.avam \
ava-tests/pointer-ops.avam \
ava-tests/real-ops.avam \
ava-tests/real-props.avam \
ava-tests/relative-import.avam \
ava-tests/reqmod-within-function.avam \
ava-tests/ret.avam \
ava-tests/semilit-with-nested-funcall.avam \
ava-tests/simple-dynamic-invoke.avam \
ava-tests/simple-function-def-dd.avam \
ava-tests/simple-function-def-sd.avam \
ava-tests/simple-function-def.avam \
ava-tests/simple-invoke-sd.avam \
ava-tests/simple-lambda.avam \
ava-tests/simple-reqmod.avam \
ava-tests/simple-ss-varargs.avam \
ava-tests/single-spread-into-varargs.avam \
ava-tests/single-statement-lambda-returns-result.avam \
ava-tests/singleton-spread.avam \
ava-tests/spread-across-named-args.avam \
ava-tests/spread-across-varargs.avam \
ava-tests/spread-into-semilit.avam \
ava-tests/string-ops.avam \
ava-tests/subscript-wither-not-needed-for-readonly.avam \
ava-tests/three-way-if-choose-second.avam \
ava-tests/trivial-user-macro.avam \
ava-tests/trivial.avam \
ava-tests/try-nothing-thrown.avam \
ava-tests/two-condition-if-choose-neither.avam \
ava-tests/two-way-if-choose-else.avam \
ava-tests/two-way-if-choose-first.avam \
ava-tests/two-way-if-with-explicit-else.avam \
ava-tests/var-semilit.avam \
ava-tests/variadic-spread-to-no-args.avam

ASM_DRIVERS = \
../src/runtime/-llvm-support/drivers/isa-unchecked.bc \
../src/runtime/-llvm-support/drivers/avast-unchecked.bc

BENCHMARKS = benchmarks/list/avalanche.t benchmarks/map/avalanche.t \
	benchmarks/string/length.t

EXTRA_CFLAGS =
AM_CFLAGS = $(CHECK_CFLAGS) -I../src $(EXTRA_CFLAGS) -funwind-tables
AM_CXXFLAGS = $(CHECK_CFLAGS) -I../src $(EXTRA_CFLAGS) -funwind-tables
AM_LDFLAGS = -export-dynamic $(CHECK_LIBS) \
	../src/libavart.la ../src/libavart-native.la

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS =  $(BENCHMARKS)

runtime_test_cxx_include_t_SOURCES = runtime/test-cxx-include.cxx

benchmarks_list_avalanche_t_SOURCES = benchmarks/list/avalanche.c
benchmarks_map_avalanche_t_SOURCES = benchmarks/map/avalanche.c
benchmarks_string_length_t_SOURCES = benchmarks/string/length.c

ava-tests.avap: $(AVA_LINK_PACKAGE) $(COMPILED_MODULES)
	$(AM_V_GEN)$(AVA_LINK_PACKAGE) $(COMPILED_MODULES)

.avap.S: $(AVA_TO_ASM) $(ASM_DRIVERS)
	$(AM_V_GEN)$(AVA_TO_ASM) $(ASM_DRIVERS) $< >$@

.ava.avam: $(AVA_COMPILE_MODULE)
	$(AM_V_GEN)$(AVA_COMPILE_MODULE) $<

.avap.avapi: $(AVA_MAKE_INTERFACE)
	$(AM_V_GEN)$(AVA_MAKE_INTERFACE) $<

# Duplicates of the above, since GNU Make doesn't support prereqs on
# traditional implicit rules.
# Non-GNU-Make will silently ignore these.
%.avam: %.ava $(AVA_COMPILE_MODULE)
	$(AM_V_GEN)$(AVA_COMPILE_MODULE) $<
%.avapi: %.avap $(AVA_MAKE_INTERFACE)
	$(AM_V_GEN)$(AVA_MAKE_INTERFACE) $<
%.S: %.avap $(AVA_TO_ASM) $(ASM_DRIVERS)
	$(AM_V_GEN)$(AVA_TO_ASM) $(ASM_DRIVERS) $< >$@

clean-local:
	find . \( -name \*.avam -o -name \*.avap -o -name \*.avapi \) -delete
	find . \( -name \*.S \) -delete
