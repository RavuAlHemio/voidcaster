dnl Process this file with autoconf to generate configure.
dnl Better yet, just execute autoreconf -fi.

AC_PREREQ([2.59])

dnl Initialize autoconf and automake.
AC_INIT([voidcaster],[0.0.1],[http://github.com/RavuAlHemio/voidcaster])
AM_INIT_AUTOMAKE([silent-rules])
AM_SILENT_RULES([yes])

dnl Check for C compiler.
AC_PROG_CXX

dnl Activate all warnings.
CFLAGS="$CFLAGS -Wall -std=c99"

dnl Debug builds.
AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--enable-debug], [use debug compiler flags and macros @<:@default=disabled@:>@])],
	[enable_debug="$enableval"],
	[enable_debug=no]
)
if test "x$enable_debug" = "xyes"
then
	CFLAGS="$CFLAGS -O0 -g"
else
	CFLAGS="$CFLAGS -O3"
fi

dnl LLVM flags.
AC_PATH_PROG([LLVM_CONFIG], [llvm-config], [no])
if test "x$LLVM_CONFIG" != xno
then
	CFLAGS="$CFLAGS `$LLVM_CONFIG --cppflags`"
	LIBS="$LIBS `$LLVM_CONFIG --libs mc support`"
	LDFLAGS="$LDFLAGS `$LLVM_CONFIG --ldflags`"
else
	AC_MSG_ERROR([LLVM is required to build the Voidcaster.])
fi

dnl Check for clang.
dnl FIXME: Implement static linking option.
AC_SEARCH_LIBS([clang_parseTranslationUnit], [clang libclang], [], AC_MSG_ERROR([libclang is required to build the Voidcaster.]))

dnl Do configuration magic.
AC_CONFIG_FILES([Makefile])

dnl That's all, folks!
AC_OUTPUT
