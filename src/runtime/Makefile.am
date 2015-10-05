AUTOMAKE_OPTIONS = subdir-objects
.MAKE.JOB.PREFIX=

include_HEADERS = avalanche.h \
avalanche/ava-config.h \
avalanche/alloc.h \
avalanche/code-gen.h \
avalanche/compile-frontend.h \
avalanche/context.h \
avalanche/defs.h \
avalanche/errors.h \
avalanche/gen-errors.h \
avalanche/exception.h \
avalanche/function.h \
avalanche/integer.h \
avalanche/interp.h \
avalanche/intrinsics.h \
avalanche/lex.h \
avalanche/list.h \
avalanche/list-proj.h \
avalanche/list-trait.h \
avalanche/macro-arg.h \
avalanche/macsub.h \
avalanche/map.h \
avalanche/map-trait.h \
avalanche/name-mangle.h \
avalanche/parser.h \
avalanche/pcode.h \
avalanche/pcode-validation.h \
avalanche/gen-pcode.h \
avalanche/pointer.h \
avalanche/pointer-trait.h \
avalanche/real.h \
avalanche/string.h \
avalanche/symbol.h \
avalanche/symtab.h \
avalanche/value.h \
avalanche/varscope.h

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

avalanche/gen-pcode.h: pcode-defs.tcl generate-pcode.tcl
	$(AM_V_GEN)$(TCLSH) generate-pcode.tcl header \
		<pcode-defs.tcl >avalanche/gen-pcode.h

avalanche/gen-errors.h: generate-errors.tcl
	$(AM_V_GEN)$(TCLSH) generate-errors.tcl header \
		>avalanche/gen-errors.h
