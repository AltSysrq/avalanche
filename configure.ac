AC_PREREQ([2.6])
AC_INIT([mantigraphia], [0.0.0], [jason@lin.gl])
AC_CONFIG_SRCDIR([src/.c])
AM_INIT_AUTOMAKE([1.11])
AM_SILENT_RULES(yes)
AC_CONFIG_MACRO_DIR([m4])
LT_PREREQ([1.5])
LT_INIT()
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O

# Checks for libraries.
AC_SEARCH_LIBS([sin], [c m])
AC_SEARCH_LIBS([GC_init], [gc], [],
               [AC_MSG_ERROR(
[The gc library could not be found. Make sure libgc[-dev] is installed.])])

# Checks for header files
AC_CHECK_HEADERS([gc.h gc/gc.h])

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

AC_CONFIG_FILES([Makefile src/Makefile test/Makefile])
AC_OUTPUT
