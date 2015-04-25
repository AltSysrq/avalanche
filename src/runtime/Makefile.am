AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

include_HEADERS = avalanche.h \
avalanche/ava-config.h \
avalanche/alloc.h \
avalanche/defs.h \
avalanche/function.h \
avalanche/lex.h \
avalanche/string.h \
avalanche/value.h

avalanche/ava-config.h: $(top_srcdir)/config.h
	$(AM_V_GEN)$(SED) <$(top_srcdir)/config.h >avalanche/ava-config.h \
		'/[Rr]estrict/!s/# *define /#define AVA_/;/[Rr]estrict/!s/# *undef /#undef AVA_/'
