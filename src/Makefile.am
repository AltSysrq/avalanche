AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=
SUBDIRS = runtime .

EXTRA_CFLAGS =
AM_CFLAGS = $(LIBUNWIND_CFLAGS) $(LIBFFI_CFLAGS) \
            $(EXTRA_CFLAGS) -funwind-tables

# Use -b instead of -g if $(CC) doesn't support label pointers
RE2C_FLAGS = -g

lib_LTLIBRARIES = libavart.la
bin_PROGRAMS = bootstrap-avac
libavart_la_SOURCES = \
runtime/alloc.c \
runtime/array-list.c \
runtime/context.c \
runtime/dtoa.c \
runtime/empty-list.c \
runtime/esba.c \
runtime/esba-list.c \
runtime/exception.c \
runtime/function.c \
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
runtime/list.c \
runtime/list-map.c \
runtime/list-proj.c \
runtime/macsub.c \
runtime/map.c \
runtime/name-mangle.c \
runtime/parser.c \
runtime/pointer.c \
runtime/real.c \
runtime/string.c \
runtime/symbol-table.c \
runtime/value.c \
runtime/-intrinsics/funcall.c \
runtime/-intrinsics/fundamental.c \
runtime/-intrinsics/variable.c

libavart_la_LIBADD = $(LIBUNWIND_LIBS) $(LIBFFI_LIBS)
bootstrap_avac_SOURCES = bootstrap/main.c
bootstrap_avac_LDADD = libavart.la

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
