AC_PREREQ([2.6])
AC_INIT([avalanche], [0.0.0], [jason@lin.gl])
AC_CONFIG_SRCDIR([src/.c])
AM_INIT_AUTOMAKE([1.11])
AM_SILENT_RULES(yes)
AC_CONFIG_MACRO_DIR([m4])
LT_PREREQ([1.5])
LT_INIT()
AM_CONFIG_HEADER([config.h])
PKG_PROG_PKG_CONFIG()

: ${AVA_BOOTSTRAP_TOOLCHAIN_DIR:=bootstrap/bin}
AC_ARG_VAR([AVA_BOOTSTRAP_TOOLCHAIN_DIR],
[Directory containing bootstrap Avalanche tools. Must be set for cross-compiling.])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_CXX
AM_PROG_AS
AC_CHECK_PROG([RE2C], [re2c], [re2c], [])
AC_CHECK_PROGS([TCLSH], [tclsh8.6 tclsh8.5 tclsh])
AC_CHECK_PROGS([CLANG], [clang])
AC_PROG_SED

# Checks for libraries.
PKG_CHECK_MODULES([CHECK], [check], [], [:])
PKG_CHECK_MODULES([LIBUNWIND], [libunwind], [], [AC_MSG_ERROR(
[libunwind could not be found. Make sure libunwind[-dev] is installed.])])
PKG_CHECK_MODULES([LIBFFI], [libffi], [], [AC_MSG_ERROR(
[libffi could not be found. Make sure libffi[-dev] is installed.])])
# While faster setjmp() would be nice, the way this library attempts to operate
# by redifining setjmp() and longjmp() is a bit problematic.
# PKG_CHECK_MODULES([LIBUNWIND_SETJMP], [libunwind-setjmp], [], [:])
AC_SEARCH_LIBS([sin], [c m])
AC_SEARCH_LIBS([strlcpy], [c bsd])
AC_SEARCH_LIBS([dlopen], [c dl])
AC_SEARCH_LIBS([GC_init], [gc], [],
               [AC_MSG_ERROR(
[The gc library could not be found. Make sure libgc[-dev] is installed.])])

# Checks for header files
AC_CHECK_HEADERS([gc.h gc/gc.h atomic_ops.h])
AC_CHECK_HEADERS([err.h bsd/err.h sysexits.h bsd/sysexits.h bsd/string.h bsd/stdlib.h])
AC_CHECK_HEADERS([sys/queue.h bsd/sys/queue.h])
AC_CHECK_HEADERS([sys/tree.h bsd/sys/tree.h])
AC_CHECK_HEADERS([sys/types.h sys/resource.h])
AC_CHECK_HEADERS([nmmintrin.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_C_CONST
AC_C_RESTRICT
AC_C_FLEXIBLE_ARRAY_MEMBER
AC_C_BIGENDIAN
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INTPTR_T
AC_CHECK_SIZEOF([short])
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([long])
AC_CHECK_SIZEOF([long long])
AC_CHECK_SIZEOF([size_t])
AC_CHECK_SIZEOF([ffi_cif], [], [
#include <ffi.h>
])
AC_CHECK_SIZEOF([ffi_arg], [], [
#include <ffi.h>
])

# Checks for library functions.
AC_CHECK_FUNCS([setrlimit arc4random_buf dlfunc dlsym])
AC_CHECK_DECLS([FFI_THISCALL, FFI_STDCALL], [], [], [
#include <ffi.h>
])

# Instead of using something standard, like pkg-config or well-known include
# paths, llvm puts everything in versioned directories that Autoconf doesn't
# seem to provide a scalable way of searching.
#
# There is this llvm-config thing, but it too is versioned. There's also the
# issue that it dumps *all the flags the llvm library was compiled with* if you
# use the recommended approach, which includes things like *disabling asserts*.
#
# https://llvm.org/bugs/show_bug.cgi?id=8220
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=697755
#
# It also appears the versioning is like `llvm-config35` on FreeBSD and
# `llim-config-3.5` on Debian.
#
# So basically check for every possible llvm-config from 3.5 through 4.9, as
# well as unversioned since that appears to be a thing too, and hope it works
# out. We'll also be assembling the *FLAGS by hand to avoid the ridiculous
# stuff the recommended approach adds.
#
# (Regarding the choice of 4.9, 5.0 seems far enough into the future that we
# don't need to worry about it yet, just as y2k will never be upon us.)
#
# It may end up being better to just vendor llvm into the Avalanche build
# system, but that would make clean builds take painfully longer.
AC_CHECK_PROGS([LLVM_CONFIG],
               [llvm-config35 llvm-config-3.5 \
                llvm-config36 llvm-config-3.6 \
                llvm-config37 llvm-config-3.7 \
                llvm-config38 llvm-config-3.8 \
                llvm-config39 llvm-config-3.9 \
                llvm-config40 llvm-config-4.0 \
                llvm-config41 llvm-config-4.1 \
                llvm-config42 llvm-config-4.2 \
                llvm-config43 llvm-config-4.3 \
                llvm-config44 llvm-config-4.4 \
                llvm-config45 llvm-config-4.5 \
                llvm-config46 llvm-config-4.6 \
                llvm-config47 llvm-config-4.7 \
                llvm-config48 llvm-config-4.8 \
                llvm-config49 llvm-config-4.9 \
                llvm-config])
AS_IF([test -z "$LLVM_CONFIG"],
      [AC_MSG_ERROR([could not find any version of llvm-config])])

# --cppflags is a bunch of defines we definitely don't want plus an addition to
# the include path.
AC_MSG_CHECKING([flags for LLVM])
LLVM_CPPFLAGS="-I`$LLVM_CONFIG --includedir`"
AC_SUBST([LLVM_CPPFLAGS])
# --cxxflags is all the cruft from --cppflags plus some warnings, an
# *optimisation level*, a forcing of the dialect to c++11, -fPIC, warnings, and
# some -f*-sections flags.  We don't want any of it.
LLVM_CXXFLAGS=""
AC_SUBST([LLVM_CXXFLAGS])
# For the exact opposite of the above, --ldflags doesn't actually include any
# of the -ls, but only the -L, so we need to put both of them together. There's
# also --system-libs that needs to go in too.
LLVM_LIBS="`$LLVM_CONFIG --ldflags` `$LLVM_CONFIG --libs`"
LLVM_LIBS="$LLVM_LIBS `$LLVM_CONFIG --system-libs`"
AC_SUBST([LLVM_LIBS])
AC_MSG_RESULT([done])

AC_CONFIG_FILES([Makefile src/Makefile src/runtime/Makefile test/Makefile])
AC_OUTPUT
