AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

TESTS = runtime/test-string.t runtime/test-lex.t \
runtime/test-value.t

EXTRA_CFLAGS =
AM_CFLAGS = $(CHECK_CFLAGS) -I../src $(EXTRA_CFLAGS)
AM_LDFLAGS = $(CHECK_LIBS) ../src/libavart.la

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = runtime/rope-vis.t
runtime_test_string_t_SOURCES = runtime/test-string.c
runtime_test_lex_t_SOURCES = runtime/test-lex.c
runtime_test_value_t_SOURCES = runtime/test-value.c
runtime_rope_vis_t_SOURCES = runtime/rope-vis.c
