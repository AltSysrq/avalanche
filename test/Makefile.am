AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

TESTS = runtime/test-string.t runtime/test-lex.t \
runtime/test-value.t runtime/test-exception.t \
runtime/test-integer.t runtime/test-empty-list.t \
runtime/test-array-list.t runtime/test-list.t \
runtime/test-esba.t runtime/test-esba-list.t

EXTRA_CFLAGS =
AM_CFLAGS = $(CHECK_CFLAGS) -I../src $(EXTRA_CFLAGS) -funwind-tables
AM_LDFLAGS = $(CHECK_LIBS) ../src/libavart.la

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = runtime/rope-vis.t
runtime_test_string_t_SOURCES = runtime/test-string.c
runtime_test_lex_t_SOURCES = runtime/test-lex.c
runtime_test_value_t_SOURCES = runtime/test-value.c
runtime_test_exception_t_SOURCES = runtime/test-exception.c
runtime_test_integer_t_SOURCES = runtime/test-integer.c
runtime_test_empty_list_t_SOURCES = runtime/test-empty-list.c
runtime_test_array_list_t_SOURCES = runtime/test-array-list.c
runtime_test_list_t_SOURCES = runtime/test-list.c
runtime_test_esba_t_SOURCES = runtime/test-esba.c
runtime_test_esba_list_t_SOURCES = runtime/test-esba-list.c
runtime_rope_vis_t_SOURCES = runtime/rope-vis.c
