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

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_CHECK_PROG([RE2C], [re2c], [re2c], [])
AC_PROG_SED

# Checks for libraries.
PKG_CHECK_MODULES([CHECK], [check], [], [:])
PKG_CHECK_MODULES([LIBUNWIND], [libunwind], [], [AC_MSG_ERROR(
[libunwind could not be found. Make sure libunwind[-dev] is installed.])])
# While faster setjmp() would be nice, the way this library attempts to operate
# by redifining setjmp() and longjmp() is a bit problematic.
# PKG_CHECK_MODULES([LIBUNWIND_SETJMP], [libunwind-setjmp], [], [:])
AC_SEARCH_LIBS([sin], [c m])
AC_SEARCH_LIBS([GC_init], [gc], [],
               [AC_MSG_ERROR(
[The gc library could not be found. Make sure libgc[-dev] is installed.])])

# Checks for header files
AC_CHECK_HEADERS([gc.h gc/gc.h])
AC_CHECK_HEADERS([err.h bsd/err.h sysexits.h bsd/sysexits.h bsd/string.h])
AC_CHECK_HEADERS([sys/types.h sys/resource.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_C_CONST
AC_C_RESTRICT
AC_C_FLEXIBLE_ARRAY_MEMBER
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

# Checks for library functions.
AC_CHECK_FUNCS([setrlimit])

AC_CONFIG_FILES([Makefile src/Makefile src/runtime/Makefile test/Makefile])
AC_OUTPUT
