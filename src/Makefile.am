AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

EXTRA_CFLAGS =
AM_CFLAGS = $(EXTRA_CFLAGS)

lib_LTLIBRARIES = libavart.la
bin_PROGRAMS = bootstrap-avac
libavart_la_SOURCES = \
runtime/alloc.c \
runtime/string.c
bootstrap_avac_SOURCES = bootstrap/main.c
bootstrap_avac_LDADD = libavart.la
