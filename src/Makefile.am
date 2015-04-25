AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=
SUBDIRS = runtime .

EXTRA_CFLAGS =
AM_CFLAGS = $(LIBUNWIND_CFLAGS) \
            $(EXTRA_CFLAGS) -funwind-tables

# Use -b instead of -g if $(CC) doesn't support label pointers
RE2C_FLAGS = -g

lib_LTLIBRARIES = libavart.la
bin_PROGRAMS = bootstrap-avac
libavart_la_SOURCES = \
runtime/alloc.c \
runtime/string.c \
runtime/gen-lex.c \
runtime/value.c \
runtime/function.c
libavart_la_LIBADD = $(LIBUNWIND_LIBS)
bootstrap_avac_SOURCES = bootstrap/main.c
bootstrap_avac_LDADD = libavart.la

runtime/gen-lex.c: runtime/lex.c
	$(AM_V_GEN)$(RE2C) --input custom -c $(RE2C_FLAGS) \
		-truntime/gen-lex.h \
		-oruntime/gen-lex.c \
		runtime/lex.c
