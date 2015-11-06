AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=
SUBDIRS = runtime .

EXTRA_CFLAGS =
AM_CFLAGS = $(LIBUNWIND_CFLAGS) $(LIBFFI_CFLAGS) \
            $(EXTRA_CFLAGS) -funwind-tables
AM_CXXFLAGS = $(LIBUNWIND_CFLAGS) $(LIBFFI_CFLAGS) \
              $(EXTRA_CFLAGS) $(LLVM_CPPFLAGS) $(LLVM_CXXFLAGS) \
              -std=c++11 -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS
EMIT_LLVM_CFLAGS= -O3 -g0 -fno-caret-diagnostics -fno-diagnostics-fixit-info \
                  -fno-color-diagnostics -c -emit-llvm \
		  -DNDEBUG -DHAVE_CONFIG_H -Wall $(DEFAULT_INCLUDES)
EMIT_LLVM = $(AM_V_GEN)$(CLANG) $(EMIT_LLVM_CFLAGS) -o $@
DAT2C = $(AM_V_GEN)$(TCLSH) dat2c.tcl >$@

# Use -b instead of -g if $(CC) doesn't support label pointers
RE2C_FLAGS = -g

AVA_DRIVERS = runtime/-llvm-support/drivers/isa-unchecked.bc

lib_LTLIBRARIES = libavart.la libavart-native.la
noinst_PROGRAMS = simple-interp llvm-ir-dump.t \
bootstrap/bin/compile-module bootstrap/bin/link-package

noinst_DATA =  $(AVA_DRIVERS)
libavart_la_SOURCES = \
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
runtime/exception.c \
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
runtime/-intrinsics/require.c \
runtime/-intrinsics/ret.c \
runtime/-intrinsics/user-macro.c \
runtime/-intrinsics/variable.c

libavart_native_la_SOURCES = \
runtime/-llvm-support/drivers/gen-isa-unchecked.c \
runtime/-llvm-support/drivers/gen-main.c \
runtime/-llvm-support/driver-iface.cxx \
runtime/-llvm-support/ir-types.cxx \
runtime/-llvm-support/jit-context.cxx \
runtime/-llvm-support/optimisation.cxx \
runtime/-llvm-support/translation.cxx

libavart_la_LIBADD = $(LIBUNWIND_LIBS) $(LIBFFI_LIBS)
libavart_native_la_LIBADD = $(LLVM_LIBS)
simple_interp_SOURCES = interp/main.c
simple_interp_LDADD = libavart.la libavart-native.la

llvm_ir_dump_t_SOURCES = llvm-ir-dump.cxx
llvm_ir_dump_t_LDADD = libavart.la libavart-native.la $(LLVM_LIBS)

bootstrap_bin_compile_module_SOURCES = bootstrap/compile-module.c
bootstrap_bin_compile_module_LDADD = libavart.la
bootstrap_bin_link_package_SOURCES = bootstrap/link-package.c
bootstrap_bin_link_package_LDADD = libavart.la

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
	runtime/-llvm-support/drivers/isa.c \
	runtime/avalanche.h \
	runtime/avalanche/*.h
	$(EMIT_LLVM) runtime/-llvm-support/drivers/isa.c

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
