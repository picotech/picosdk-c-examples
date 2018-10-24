# Process this file with autoconf to produce a configure script.

# Initialize autotools
AC_PREREQ(2.59)
AC_INIT([Pico Technology Ltd. PicoLog CM3 Example],1.0.0,[[support@picotech.com]],[plcm3Con])
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_SRCDIR(plcm3Con.c)
AC_CONFIG_HEADERS(config.h)

AM_INIT_AUTOMAKE([foreign] [no-dist])

AC_DEFUN([AX_SPLIT_VERSION],[
	ax_major_version=`echo "$2" | sed 's/\([[^.]][[^.]]*\).*/\1/'`
	ax_minor_version=`echo "$2" | sed 's/[[^.]][[^.]]*.\([[^.]][[^.]]*\).*/\1/'`
	ax_point_version=`echo "$2" | sed 's/[[^.]][[^.]]*.[[^.]][[^.]]*.\(.*\)/\1/'`

	AC_DEFINE_UNQUOTED([$1_MAJOR], [$ax_major_version], [Define to major version for $1])
	AC_DEFINE_UNQUOTED([$1_MINOR], [$ax_minor_version], [Define to minor version for $1])
	AC_DEFINE_UNQUOTED([$1_POINT], [$ax_point_version], [Define to point version for $1])
])

AX_SPLIT_VERSION([LIBTOOL_VERSION],[VERSION])

# Remove the static library
AM_DISABLE_STATIC

# Checks for programs
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

AC_MSG_CHECKING([operating system])
case $host in
    *-linux*)
	AC_DEFINE(OS_LINUX, [], [Linux backend])
	AC_SUBST(OS_LINUX)
	AC_MSG_RESULT([Linux])
	backend="linux"
	AM_LDFLAGS=""
	;;
	*-darwin*)
	AC_DEFINE(OS_DARWIN, [], [Darwin backend])
	AC_SUBST(OS_DARWIN)
	AC_DEFINE(__APPLE__, [1], [Mac OSX])
	AC_SUBST(__APPLE__)
	AC_MSG_RESULT([Darwin/MacOS X])
	backend="darwin"
	AM_LDFLAGS="-Wl,-framework -Wl,IOKit -Wl,-framework -Wl,CoreFoundation -Wl,-prebind -no-undefined"
	;;
	*)
	AC_MSG_ERROR([unsupported operating system])
esac

AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug],
        [enable debug (default n)])],
        [debug_enabled=$enableval],
        [debug_enabled='no'])
if test "x$debug_enabled" != "xno"; then
        AC_SUBST([CXXFLAGS],["-g3 -O0 -DDEBUG"])
        AC_SUBST([CFLAGS],["-g3 -O0 -DDEBUG"])
fi

AC_ARG_ENABLE([silent-rules],[],
	[
	m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])
	],[
	m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([no])])
	])

AC_CHECK_LIB([pthread],[pthread_atfork],[])

if test "x$backend" == "xlinux"
then
	# tell ld to not strip this object from static links
	LDFLAGS=${LDFLAGS}" -Wl,-u,pthread_atfork"
fi

AC_ARG_WITH([pico-libs],
    [AS_HELP_STRING([--with-pico-libs],
        [location of picoscope libraries])],
    [pico_libs_path="$withval"],
    [pico_libs_path="/opt/picoscope/lib"])
LDFLAGS=${LDFLAGS}" -L$pico_libs_path"

AC_CHECK_LIB([plcm3], [PLCM3OpenUnit],[],AC_MSG_ERROR([libplcm3 missing!]))

# Checks for header files.
AC_HEADER_STDC

AC_ARG_WITH([pico-headers],
    [AS_HELP_STRING([--with-pico-headers],
        [location of picoscope libraries])],
    [pico_headers_path="$withval"],
    [pico_headers_path="/opt/picoscope/include"])
CFLAGS=${CFLAGS}" -I$pico_headers_path"
CPPFLAGS=${CXXFLAGS}" -I$pico_headers_path"

AC_CHECK_HEADERS([stdio.h sys/types.h string.h termios.h sys/ioctl.h sys/types.h unistd.h stdlib.h libplcm3-1.0/PLCM3Api.h])

if test "$ac_cv_header_libplcm3_1_0_PLCM3Api_h" == no
then
    AC_MSG_ERROR([libplcm3-1.0/PLCM3Api.h missing!])
fi


# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Generate output
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

