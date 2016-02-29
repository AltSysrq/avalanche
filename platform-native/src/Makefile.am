AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=
SUBDIRS = avalanche .

AM_CFLAGS = $(LIBFFI_CFLAGS) $(LIBUV_CFLAGS) $(EXTRA_CFLAGS)

lib_LTLIBRARIES = libavart-native.la

libavart_native_la_SOURCES = \
memory/stub.c \
threads/thread.c \
threads/subprocess.cxx

libavart_native_la_LIBADD = \
$(top_srcdir)/contrib/libbacktrace/libbacktrace.la \
$(LIBFFI_LIBS) $(LIBUV_CFLAGS)
