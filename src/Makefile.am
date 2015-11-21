AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=
SUBDIRS = runtime .

EXTRA_CFLAGS =
AM_CFLAGS = $(LIBFFI_CFLAGS) $(EXTRA_CFLAGS)
AM_CXXFLAGS = $(LIBFFI_CFLAGS) \
              $(EXTRA_CFLAGS) $(LLVM_CPPFLAGS) $(LLVM_CXXFLAGS) \
              -std=c++11 -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS
AM_CCASFLAGS = -g0
EMIT_LLVM_CFLAGS= -O3 -g0 -fno-caret-diagnostics -fno-diagnostics-fixit-info \
                  -fno-color-diagnostics -c -emit-llvm \
		  -DNDEBUG -DHAVE_CONFIG_H -Wall $(DEFAULT_INCLUDES)
EMIT_LLVM = $(AM_V_GEN)$(CLANG) $(EMIT_LLVM_CFLAGS) -o $@
EMIT_LLVMXX = $(AM_V_GEN)$(CLANGXX) $(EMIT_LLVM_CFLAGS) -o $@
DAT2C = $(AM_V_GEN)$(TCLSH) dat2c.tcl >$@
BOOTSTRAP_LIBS = libavart-bootstrap.la $(LIBFFI_LIBS)

AVA_COMPILE_MODULE = @AVA_BOOTSTRAP_TOOLCHAIN_DIR@/compile-module$(EXEEXT)
AVA_LINK_PACKAGE = @AVA_BOOTSTRAP_TOOLCHAIN_DIR@/link-package$(EXEEXT)
AVA_MAKE_INTERFACE = @AVA_BOOTSTRAP_TOOLCHAIN_DIR@/make-interface$(EXEEXT)
AVA_TO_ASM = @AVA_BOOTSTRAP_TOOLCHAIN_DIR@/to-asm$(EXEEXT)

# Use -b instead of -g if $(CC) doesn't support label pointers
RE2C_FLAGS = -g

# The -bootstrap static libraries are the same as the dynamic libraries we
# install, except they do not have avast fully compiled in. They are used for
# building the bootstrap tools which compile avast itself.
noinst_LTLIBRARIES = libavart-bootstrap.la
lib_LTLIBRARIES = libavart.la libavart-native.la
noinst_PROGRAMS = simple-interp llvm-ir-dump.t \
bootstrap/bin/compile-module bootstrap/bin/link-package \
bootstrap/bin/make-interface bootstrap/bin/to-asm

libavart_bootstrap_la_SOURCES = \
runtime/alloc.c \
runtime/array-list.c \
runtime/avast.c \
runtime/code-gen.c \
runtime/compenv.c \
runtime/context.c \
runtime/dtoa.c \
runtime/empty-list.c \
runtime/errors.c \
runtime/esba.c \
runtime/esba-list.c \
runtime/exception.cxx \
runtime/function.c \
runtime/gen-errors.c \
runtime/gen-integer.c \
runtime/gen-integer-decimal.c \
runtime/gen-lex.c \
runtime/gen-pcode.c \
runtime/hash-map.c \
runtime/hash-map-16.c \
runtime/hash-map-32.c \
runtime/hash-map-64.c \
runtime/hexes.c \
runtime/init.c \
runtime/integer-fast-dec.c \
runtime/interval.c \
runtime/intrinsics.c \
runtime/list.c \
runtime/list-map.c \
runtime/list-proj.c \
runtime/macro-arg.c \
runtime/macsub.c \
runtime/map.c \
runtime/module-cache.c \
runtime/name-mangle.c \
runtime/parser.c \
runtime/pcode-linker.c \
runtime/pcode-validation.c \
runtime/pointer.c \
runtime/real.c \
runtime/string.c \
runtime/symtab.c \
runtime/value.c \
runtime/varscope.c \
runtime/-intrinsics/defun.c \
runtime/-intrinsics/extern.c \
runtime/-intrinsics/funcall.c \
runtime/-intrinsics/fundamental.c \
runtime/-intrinsics/if.c \
runtime/-intrinsics/loop.c \
runtime/-intrinsics/namespace.c \
runtime/-intrinsics/pasta.c \
runtime/-intrinsics/reg-rvalue.c \
runtime/-intrinsics/require.c \
runtime/-intrinsics/ret.c \
runtime/-intrinsics/subscript.c \
runtime/-intrinsics/user-macro.c \
runtime/-intrinsics/variable.c

libavart_la_SOURCES = \
org.ava-lang.avast.S \
org.ava-lang.avast.avapi.c \
runtime/compenv-avast.c

libavart_native_la_SOURCES = \
runtime/-llvm-support/drivers/gen-isa-unchecked.c \
runtime/-llvm-support/drivers/gen-avast-unchecked.c \
runtime/-llvm-support/drivers/gen-avast-checked-1.c \
runtime/-llvm-support/drivers/gen-avast-checked-2.c \
runtime/-llvm-support/drivers/gen-main.c \
runtime/-llvm-support/driver-iface.cxx \
runtime/-llvm-support/ir-types.cxx \
runtime/-llvm-support/jit-context.cxx \
runtime/-llvm-support/optimisation.cxx \
runtime/-llvm-support/translation.cxx

libavart_la_LIBADD = $(BOOTSTRAP_LIBS)
libavart_native_la_LIBADD = $(LLVM_LIBS)
simple_interp_SOURCES = interp/main.c
simple_interp_LDADD = libavart.la libavart-native.la

llvm_ir_dump_t_SOURCES = llvm-ir-dump.cxx
llvm_ir_dump_t_LDADD = libavart.la libavart-native.la $(LLVM_LIBS)

# The dummy.cxx thing is there to force linking to occur with CXXLD, which is
# necessary so we get C++ exception support.
bootstrap_bin_compile_module_SOURCES = \
	bootstrap/compile-module.c bootstrap/dummy.cxx
bootstrap_bin_compile_module_LDADD = libavart-bootstrap.la $(BOOTSTRAP_LIBS)
bootstrap_bin_link_package_SOURCES = \
	bootstrap/link-package.c bootstrap/dummy.cxx
bootstrap_bin_link_package_LDADD = $(BOOTSTRAP_LIBS)
bootstrap_bin_make_interface_SOURCES = \
	bootstrap/make-interface.c bootstrap/dummy.cxx
bootstrap_bin_make_interface_LDADD = $(BOOTSTRAP_LIBS)
bootstrap_bin_to_asm_SOURCES = bootstrap/to-asm.cxx
bootstrap_bin_to_asm_LDADD = libavart-native.la $(BOOTSTRAP_LIBS) $(LLVM_LIBS)

runtime/gen-lex.c: runtime/lex.c
	$(AM_V_GEN)$(RE2C) --input custom -c $(RE2C_FLAGS) \
		-truntime/gen-lex.h \
		-oruntime/gen-lex.c \
		runtime/lex.c

runtime/gen-integer.c: runtime/integer.c
	$(AM_V_GEN)$(RE2C) --input custom -r $(RE2C_FLAGS) \
		-oruntime/gen-integer.c \
		runtime/integer.c

runtime/gen-integer-decimal.c: runtime/generate-integer-decimal.c.tcl
	$(AM_V_GEN)$(TCLSH) runtime/generate-integer-decimal.c.tcl \
		>runtime/gen-integer-decimal.c

runtime/gen-pcode.c: runtime/generate-pcode.tcl runtime/pcode-defs.tcl
	$(AM_V_GEN)$(TCLSH) runtime/generate-pcode.tcl impl \
		<runtime/pcode-defs.tcl >runtime/gen-pcode.c

runtime/gen-errors.c: runtime/generate-errors.tcl
	$(AM_V_GEN)$(TCLSH) runtime/generate-errors.tcl impl \
		>runtime/gen-errors.c

runtime/-llvm-support/drivers/isa-unchecked.bc: \
	runtime/-llvm-support/drivers/isa.cxx \
	runtime/avalanche.h \
	runtime/avalanche/*.h
	$(EMIT_LLVMXX) runtime/-llvm-support/drivers/isa.cxx

runtime/-llvm-support/drivers/gen-isa-unchecked.c: \
	runtime/-llvm-support/drivers/isa-unchecked.bc \
	dat2c.tcl
	$(DAT2C) ava_driver_isa_unchecked \
		<runtime/-llvm-support/drivers/isa-unchecked.bc

runtime/-llvm-support/drivers/main.bc: \
	runtime/-llvm-support/drivers/main.c \
	runtime/avalanche/*.h
	$(EMIT_LLVM) runtime/-llvm-support/drivers/main.c

runtime/-llvm-support/drivers/gen-main.c: \
	runtime/-llvm-support/drivers/main.bc \
	dat2c.tcl
	$(DAT2C) ava_driver_main \
		<runtime/-llvm-support/drivers/main.bc

runtime/-llvm-support/drivers/gen-avast-unchecked.c: \
	runtime/-llvm-support/drivers/avast-unchecked.bc dat2c.tcl
	$(DAT2C) ava_driver_avast_unchecked \
		<runtime/-llvm-support/drivers/avast-unchecked.bc

runtime/-llvm-support/drivers/avast-unchecked.bc: \
	runtime/avast.c runtime/avalanche/*.h
	$(EMIT_LLVM) -DAVAST_CHECK_LEVEL=0 -DCOMPILING_DRIVER runtime/avast.c

runtime/-llvm-support/drivers/gen-avast-checked-1.c: \
	runtime/-llvm-support/drivers/avast-checked-1.bc dat2c.tcl
	$(DAT2C) ava_driver_avast_checked_1 \
		<runtime/-llvm-support/drivers/avast-checked-1.bc

runtime/-llvm-support/drivers/avast-checked-1.bc: \
	runtime/avast.c runtime/avalanche/*.h
	$(EMIT_LLVM) -DAVAST_CHECK_LEVEL=1 -DCOMPILING_DRIVER runtime/avast.c

runtime/-llvm-support/drivers/gen-avast-checked-2.c: \
	runtime/-llvm-support/drivers/avast-checked-2.bc dat2c.tcl
	$(DAT2C) ava_driver_avast_checked_2 \
		<runtime/-llvm-support/drivers/avast-checked-2.bc

runtime/-llvm-support/drivers/avast-checked-2.bc: \
	runtime/avast.c runtime/avalanche/*.h
	$(EMIT_LLVM) -DAVAST_CHECK_LEVEL=2 -DCOMPILING_DRIVER runtime/avast.c

ASM_DRIVERS = \
runtime/-llvm-support/drivers/isa-unchecked.bc \
runtime/-llvm-support/drivers/avast-unchecked.bc

.avap.S: $(AVA_TO_ASM) $(ASM_DRIVERS)
	$(AM_V_GEN)$(AVA_TO_ASM) $(ASM_DRIVERS) $< >$@

AVAST_MODULES = \
org.ava-lang.avast/builtins.avam \
org.ava-lang.avast/byte-strings.avam \
org.ava-lang.avast/cstdio.avam \
org.ava-lang.avast/operators.avam

org.ava-lang.avast.avap: $(AVA_LINK_PACKAGE) $(AVAST_MODULES)
	$(AM_V_GEN)$(AVA_LINK_PACKAGE) $(AVAST_MODULES)

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

org.ava-lang.avast.avapi.c: org.ava-lang.avast.avapi dat2c.tcl
	$(DAT2C) ava_org_ava_lang_avast_avapi \
		<org.ava-lang.avast.avapi

clean-local:
	find . \( -name \*.avam -o -name \*.avap -o -name \*.avapi \) -delete
	find . \( -name gen-\*.h -o -name gen-\*.c \) -delete
	find . \( -name \*.bc -o -name org.\*.S -o -name \*.avapi.c \) -delete
