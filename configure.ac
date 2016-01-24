AC_PREREQ([2.6])
AC_INIT([avalanche], [0.0.0], [jason@lin.gl])
AC_CANONICAL_TARGET
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CONFIG_SRCDIR([hello.ava])
AM_INIT_AUTOMAKE([1.11])
AM_SILENT_RULES(yes)
AC_CONFIG_MACRO_DIR([m4])
LT_PREREQ([1.5])
LT_INIT()
AM_CONFIG_HEADER([config.h])
PKG_PROG_PKG_CONFIG()
AC_CONFIG_SUBDIRS([contrib/libbacktrace])

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
AS_IF([test "x$RE2C" = "x"],
      [AC_MSG_ERROR([re2c not found; make sure it is installed])])
AC_CHECK_PROGS([TCLSH], [tclsh8.5 tclsh8.6 tclsh8.7 tclsh])
AS_IF([test "x$TCLSH" = "x"],
      [AC_MSG_ERROR([tclsh not found. Make sure Tcl 8.5 or later is installed.])])
# See the diatribe below on LLVM versioning for why we check for clang and
# clang++ this way.
# Note that these are used only for compiling the native code generation
# drivers, not the library proper.
AC_CHECK_TARGET_TOOLS([CLANG], [clang37 clang-3.7])
AS_IF([test "x$CLANG" = "x"],
      [AC_MSG_ERROR([clang37 not found. Make sure Clang 3.7 configured for your
desired target is installed.

If your Clang installation uses an unusual version naming convention, specify
the name of the Clang executable with CLANG=/path/to/clang when you call
configure.

Note that you cannot use an arbitrary version of Clang; it must match the
version of LLVM used by Avalanche.])])
AC_CHECK_TARGET_TOOLS([CLANGXX], [clang++37 clang++-3.7])
AS_IF([test "x$CLANG" = "x"],
      [AC_MSG_ERROR([clang++37 not found. Make sure Clang++ 3.7 configured for
your desired target is installed.

If your Clang++ installation uses an unusual version naming convention, specify
the name of the Clang++ executable with CLANG=/path/to/clang++ when you call
configure.

Note that you cannot use an arbitrary version of Clang++; it must batch the
version of LLVM used by Avalanche.])])
AC_PROG_SED

# Checks for libraries.
PKG_CHECK_MODULES([CHECK], [check], [], [:])
PKG_CHECK_MODULES([LIBFFI], [libffi], [], [AC_MSG_ERROR(
[libffi could not be found. Make sure libffi[[-dev]] is installed.])])
AC_SEARCH_LIBS([sin], [c m])
AC_SEARCH_LIBS([strlcpy], [c bsd])
AC_SEARCH_LIBS([dlopen], [c dl])
AC_SEARCH_LIBS([GC_init], [gc], [],
               [AC_MSG_ERROR(
[The gc library could not be found. Make sure boehm-gc[[-dev]] or libgc[[-dev]] is
installed.])])

# Checks for header files
AC_CHECK_HEADERS([gc.h gc/gc.h], [FOUND_GC_H=yes])
AS_IF([test "x$FOUND_GC_H" = "x"],
      [AC_MSG_ERROR(
[Neither gc.h nor gc/gc.h found. Make sure boehm-gc[[-dev]] or libgc-dev is
installed.])])
AC_CHECK_HEADERS([atomic_ops.h], [:],
                 [AC_MSG_ERROR(
[atomics_ops.h not found. Make sure libatomicops[[-dev]] is installed.])])
AC_CHECK_HEADERS([err.h bsd/err.h], [FOUND_ERR_H=yes])
AC_CHECK_HEADERS([sysexits.h bsd/sysexits.h], [FOUND_SYSEXITS_H=yes])
AC_CHECK_HEADERS([bsd/string.h bsd/stdlib.h])
AC_CHECK_HEADERS([sys/queue.h bsd/sys/queue.h], [FOUND_SYS_QUEUE_H=yes])
AC_CHECK_HEADERS([sys/tree.h bsd/sys/tree.h], [FOUND_SYS_TREE_H=yes])
AS_IF([test "x$FOUND_ERR_H" = "x" ||
       test "x$FOUND_SYSEXITS_H" = "x" ||
       test "x$FOUND_SYS_QUEUE_H" = "x" ||
       test "x$FOUND_SYS_TREE_H" = "x"],
      [AC_MSG_ERROR(
[One or more required headers from the BSD standard library was not found. You
may need to install libsd[[-dev]].])])
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
# We used to also permit future versions, but since LLVM breaks compatibility
# on minor releases, there's no point.
#
# It may end up being better to just vendor llvm into the Avalanche build
# system, but that would make clean builds take painfully longer.
AC_CHECK_PROGS([LLVM_CONFIG],
               [llvm-config37 llvm-config-3.7])
AS_IF([test -z "$LLVM_CONFIG"],
      [AC_MSG_ERROR([could not find llvm-config37.

Make sure llvm37[[-dev]] / llvm-3.7[[-dev]] is installed.

If your installation uses an unusual version suffix, specify the name of the
lllvm-config program with LLVM_CONFIG=/path/to/llvm-config when running
configure.

Note that versions of LLVM other than 3.7 are not supported.])])

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

AC_CONFIG_FILES([Makefile
                 bootstrap-runtime/src/Makefile
                 bootstrap-runtime/src/runtime/Makefile
                 bootstrap-runtime/test/Makefile])
AC_OUTPUT
