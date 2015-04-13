AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

lib_LTLIBRARIES = libavart.la
bin_PROGRAMS = bootstrap-avac
libavart_la_SOURCES = runtime/alloc.c
bootstrap_avac_SOURCES = bootstrap/main.c
bootstrap_avac_LDADD = libavart.la
