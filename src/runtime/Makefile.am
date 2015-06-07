AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

include_HEADERS = avalanche.h \
avalanche/ava-config.h \
avalanche/alloc.h \
avalanche/context.h \
avalanche/defs.h \
avalanche/exception.h \
avalanche/integer.h \
avalanche/lex.h \
avalanche/list.h \
avalanche/list-proj.h \
avalanche/list-trait.h \
avalanche/map.h \
avalanche/map-trait.h \
avalanche/pointer.h \
avalanche/pointer-trait.h \
avalanche/real.h \
avalanche/string.h \
avalanche/value.h

avalanche/ava-config.h: $(top_srcdir)/config.h
	$(AM_V_GEN)$(SED) <$(top_srcdir)/config.h >avalanche/ava-config.h \
		'/[Rr]estrict/!s/# *define /#define AVA_/;/[Rr]estrict/!s/# *undef /#undef AVA_/'

avalanche/list-trait.h: avalanche/generate-trait.tcl avalanche/list.trait
	$(AM_V_GEN)$(TCLSH) avalanche/generate-trait.tcl avalanche/list.trait \
		>avalanche/list-trait.h

avalanche/map-trait.h: avalanche/generate-trait.tcl avalanche/map.trait
	$(AM_V_GEN)$(TCLSH) avalanche/generate-trait.tcl avalanche/map.trait \
		>avalanche/map-trait.h

avalanche/pointer-trait.h: avalanche/generate-trait.tcl avalanche/pointer.trait
	$(AM_V_GEN)$(TCLSH) avalanche/generate-trait.tcl avalanche/pointer.trait \
		>avalanche/pointer-trait.h
