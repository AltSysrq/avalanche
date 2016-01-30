AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

include_HEADERS = \
abi.h \
ava-config.h \
defs.h

ava-config.h: $(top_srcdir)/config.h
	$(AM_V_GEN)$(SED) <$(top_srcdir)/config.h >avalanche/ava-config.h \
		'/[Rr]estrict/!s/# *define /#define AVA_/;/[Rr]estrict/!s/# *undef /#undef AVA_/'
