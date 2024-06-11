# -*- mode: m4; -*-
# Process this file with autoconf to produce a configure script

AC_PREREQ([2.69])

AC_INIT([pcsc-lite], [2.2.0])
AC_CONFIG_SRCDIR(src/pcscdaemon.c)
AM_INIT_AUTOMAKE(1.8 dist-bzip2 no-dist-gzip foreign subdir-objects)
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_MACRO_DIR(m4)

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CANONICAL_HOST
AC_PROG_CC
AC_C_BIGENDIAN

# Check for some target-specific stuff
AS_CASE(["$host"],
	[*-*-hpux*], [CPPFLAGS="$CPPFLAGS -D_HPUX_SOURCE -D_XOPEN_SOURCE_EXTENDED"],
	[*-*-solaris*], [
		CPPFLAGS="$CPPFLAGS -I/usr/local/include"
		LDFLAGS="$LDFLAGS -L/usr/local/lib -R/usr/local/lib"
		need_dash_r=1 ],
	[*-*-sunos4*], [CPPFLAGS="$CPPFLAGS -DSUNOS4"],
	[*-*-aix*], [
		CPPFLAGS="$CPPFLAGS -I/usr/local/include"
		LDFLAGS="$LDFLAGS -L/usr/local/lib"
		AS_IF([test "$LD" != "gcc" && test -z "$blibpath"],
			[blibpath="/usr/lib:/lib:/usr/local/lib"])],
	[*-*-osf*], [CPPFLAGS="$CPPFLAGS -D_POSIX_PII_SOCKET"],
	[*-*-darwin*], [
		AC_SUBST(COREFOUNDATION)
		COREFOUNDATION="-Wl,-framework,CoreFoundation"
		AC_SUBST(IOKIT)
		IOKIT="-Wl,-framework,IOKit"
		AS_IF([test "$GCC" = "yes"], [CFLAGS="$CFLAGS -no-cpp-precomp"])
		use_libusb=no
		AC_MSG_WARN([libusb disabled on Darwin for pcsc-lite])]
)

# Options
AM_MAINTAINER_MODE

# Checks for programs
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LEX([noyywrap])
AS_IF([test $LEX = ":"], [
       AM_MISSING_PROG(MISSINGLEX, [flex])
       LEX=$MISSINGLEX]
)
PKG_PROG_PKG_CONFIG
AM_PROG_CC_C_O
AM_PROG_AR

# Special check for pthread support
AX_PTHREAD([
 AC_DEFINE(HAVE_PTHREAD, 1,
  [Define if you have POSIX threads libraries and header files.])
], [
 AC_MSG_ERROR([POSIX thread support required])
])

CC="$PTHREAD_CC"

AC_SUBST(PTHREAD_LIBS)
AC_SUBST(PTHREAD_CFLAGS)

# check for pthread_cancel()
saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
AC_CHECK_FUNCS(pthread_cancel)
CFLAGS="$saved_CFLAGS"

# Add libtool support
# Static lib is disabled by default. Use --enable-static if needed
LT_INIT(disable-static)
LT_INIT
# Automatically update the libtool script if it becomes out-of-date
#AC_SUBST(LIBTOOL_DEPS)

# Checks for header files
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([getopt.h sys/filio.h syslog.h dl.h fcntl.h alloca.h])

# Checks for typedefs, structures, and compiler characteristics
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_SIZE_T
AC_STRUCT_DIRENT_D_TYPE

# Checks for library functions
AC_FUNC_ERROR_AT_LINE
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(getopt_long nanosleep strerror vsnprintf getrandom)
AC_CHECK_FUNCS(secure_getenv issetugid)
AC_FUNC_ALLOCA

# C Compiler features
AC_C_INLINE
AS_IF([test "$GCC" = "yes"], [CFLAGS="-Wall -fno-common $CFLAGS"])

# check if the compiler support -fvisibility=hidden (GCC >= 4)
saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fvisibility=hidden"
AC_MSG_CHECKING([for -fvisibility=hidden])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([char foo;])],
	[	AC_MSG_RESULT([yes])
		SYMBOL_VISIBILITY="-fvisibility=hidden" ],
	AC_MSG_RESULT([no]))
CFLAGS="$saved_CFLAGS"
AC_SUBST(SYMBOL_VISIBILITY)

AC_SEARCH_LIBS([dlopen], [dl dld], [], [
  AC_MSG_ERROR([unable to find the dlopen() function])
])

# Use 'uname' output as an architecture define
PCSC_ARCH=`uname | sed -e s,/,_,`
AS_CASE(["$PCSC_ARCH"],
	[Darwin], [PCSC_ARCH=MacOS],
	[SunOS], [PCSC_ARCH=Solaris])
AC_DEFINE_UNQUOTED(PCSC_ARCH, "$PCSC_ARCH", [PC/SC target architecture])
PCSCLITE_FEATURES="${PCSCLITE_FEATURES} $PCSC_ARCH $host"

# --disable-documentation
AC_ARG_ENABLE(documentation,
	AS_HELP_STRING([--disable-documentation], [do not build documentation]),
	[ enable_doc="${enableval}" ], [ enable_doc="yes" ] )
AM_CONDITIONAL(ENABLE_DOC, test "$enable_doc" != "no")

# See if socket() is found from libsocket
AC_CHECK_LIB(socket, socket, [LIBS="$LIBS -lsocket"])

# check for mq_getattr()
AC_CHECK_LIB(rt, mq_getattr, [LIBS="$LIBS -lrt"])

# check for libsystemd
AC_ARG_ENABLE(libsystemd,
	AS_HELP_STRING([--disable-libsystemd], [do not use libsystemd]),
	[ use_libsystemd="${enableval}" ], [ use_libsystemd="yes" ] )
AS_IF([test "$use_libsystemd" != "no"],
	[PKG_CHECK_MODULES(LIBSYSTEMD, libsystemd,,
		[ AC_MSG_ERROR([install libsystemd-dev or use --disable-libsystemd]) ])
	AC_DEFINE(USE_LIBSYSTEMD, 1, [Use libsystemd])
	PCSCLITE_FEATURES="${PCSCLITE_FEATURES} libsystemd"])

# --disable-serial
AC_ARG_ENABLE(serial,
	AS_HELP_STRING([--disable-serial], [do not use serial reader.conf files]),
	[ use_serial="${enableval}" ], [ use_serial="yes" ] )
AM_CONDITIONAL(ENABLE_SERIAL, test "$use_serial" != "no")
AS_IF([test "$use_serial" != "no"],
	[AC_DEFINE(USE_SERIAL, 1, [Use serial conf file mechanism])
	 PCSCLITE_FEATURES="${PCSCLITE_FEATURES} serial"])

# --disable-usb
AC_ARG_ENABLE(usb,
	AS_HELP_STRING([--disable-usb], [do not use usb hotplug]),
	[ use_usb="${enableval}" ], [ use_usb="yes" ] )
AM_CONDITIONAL(ENABLE_USB, test "$use_usb" != "no")
AS_IF([test "$use_usb" != "no"],
	[AC_DEFINE(USE_USB, 1, [Use USB hotplug mechanism])
	 PCSCLITE_FEATURES="${PCSCLITE_FEATURES} usb"])

# --enable-libudev
AC_ARG_ENABLE(libudev,
	AS_HELP_STRING([--disable-libudev], [do not use libudev]),
	[ use_libudev="${enableval}" ],
	[ AS_IF([test "$PCSC_ARCH" != "Linux"],
		[use_libudev="no"],
		[use_libudev="${use_libudev-yes}"])
	])

# disable libudev check is USB is not used
AS_IF([test "$use_usb" = "no"], [use_libudev="no"])

# check if libudev is used
AS_IF([test "x$use_libudev" != xno],
	[PKG_CHECK_MODULES(LIBUDEV, libudev, [],
		[ AC_MSG_ERROR([install libudev-dev or use --disable-libudev]) ])

	saved_CPPFLAGS="$CPPFLAGS"
	saved_LIBS="$LIBS"

	CPPFLAGS="$CPPFLAGS $LIBUDEV_CFLAGS"
	LIBS="$LDFLAGS $LIBUDEV_LIBS"

	AC_CHECK_HEADERS(libudev.h, [],
		[ AC_MSG_ERROR([libudev.h not found, use ./configure LIBUDEV_CFLAGS=...]) ])

	LIBS="$LIBS $COREFOUNDATION $IOKIT"

	AC_MSG_CHECKING([for udev_monitor_unref])
	AC_TRY_LINK_FUNC(udev_monitor_unref, [ AC_MSG_RESULT([yes]) ],
		[ AC_MSG_ERROR([udev_monitor_unref not found, use ./configure LIBUDEV_LIBS=...]) ])

	# everything needed for libudev support is present
	use_libudev=yes

	CPPFLAGS="$saved_CPPFLAGS"
	LIBS="$saved_LIBS"
	])
AC_SUBST(LIBUDEV_CFLAGS)
AC_SUBST(LIBUDEV_LIBS)

AS_IF([test x$use_libudev = xyes],
	[AC_DEFINE(HAVE_LIBUDEV, 1, [Libudev is available])
	 PCSCLITE_FEATURES="${PCSCLITE_FEATURES} libudev"])

# --enable-libusb
AC_ARG_ENABLE(libusb,
	AS_HELP_STRING([--enable-libusb], [use libusb]),
	[ use_libusb="${enableval}" ],
	[ AS_IF([test "x$use_libudev" = xyes],
		   	[use_libusb="${use_libusb-no}"],
			[use_libusb="${use_libusb-yes}"])] )

# disable libusb check is USB is not used
AS_IF([test "$use_usb" = "no"], [use_libusb="no"])

# check if libusb is used
AS_IF([test "x$use_libusb" != xno],
	[
	AS_IF([test "x$use_libudev" != xno],
		[AC_MSG_ERROR([You can't use libudev _and_ libusb. Select only one.])])

	PKG_CHECK_MODULES(LIBUSB, libusb-1.0, [],
		[
			AC_MSG_RESULT([no])
			AC_CHECK_PROG([LIBUSBCONFIG], [libusb-config], [yes])

			AS_IF([test "$LIBUSBCONFIG" = "yes"],
				[LIBUSB_CFLAGS="$LIBUSB_CFLAGS `libusb-config --cflags`"
				 LIBUSB_LIBS="$LIBUSB_LIBS `libusb-config --libs`" ],
				[AC_MSG_WARN([libusb-config not found.])])
		])

	saved_CPPFLAGS="$CPPFLAGS"
	saved_LIBS="$LIBS"

	CPPFLAGS="$CPPFLAGS $LIBUSB_CFLAGS"
	LIBS="$LDFLAGS $LIBUSB_LIBS"

	AC_CHECK_HEADERS(libusb.h, [],
		[ AC_MSG_ERROR([libusb.h not found, use ./configure LIBUSB_CFLAGS=...]) ])

	LIBS="$LIBS $COREFOUNDATION $IOKIT"

	AC_MSG_CHECKING([for libusb_init])
	AC_TRY_LINK_FUNC(libusb_init, [ AC_MSG_RESULT([yes]) ],
		[ AC_MSG_ERROR([libusb not found, use ./configure LIBUSB_LIBS=...]) ])

	# everything needed for libusb support is present
	use_libusb=yes

	CPPFLAGS="$saved_CPPFLAGS"
	LIBS="$saved_LIBS"
])
AC_SUBST(LIBUSB_CFLAGS)
AC_SUBST(LIBUSB_LIBS)

AS_IF([test x$use_libusb = xyes],
	[AC_DEFINE(HAVE_LIBUSB, 1, [Libusb is available])
	 PCSCLITE_FEATURES="${PCSCLITE_FEATURES} libusb"])

# --disable-polkit
POLKIT_MINIMUM=0.111
AC_ARG_ENABLE(polkit,
	AS_HELP_STRING([--disable-polkit],
		[do not build with polkit support]),
		use_polkit=$enableval, use_polkit=yes)
AS_IF([test "$use_polkit" != "no"],
	[PKG_CHECK_MODULES(POLKIT, [polkit-gobject-1 >= $POLKIT_MINIMUM], [use_polkit=yes], [use_polkit=no])
	AS_IF([test "$use_polkit" != "no"],
		[AC_DEFINE([HAVE_POLKIT], 1, [Build polkit access control support])
		 polkit_policy_dir=$($PKG_CONFIG polkit-gobject-1 --variable=policydir)
		 AC_SUBST(POLICY_DIR, [$polkit_policy_dir])
		 PCSCLITE_FEATURES="${PCSCLITE_FEATURES} polkit"],
		[use_polkit=no
		 AC_MSG_ERROR([[
***
*** polkit >= $POLKIT_MINIMUM was not found. Access control will be disabled.
*** You may get it from http://www.freedesktop.org/software/polkit/
*** ]])
		])
])
AM_CONDITIONAL(ENABLE_POLKIT, test "$use_polkit" != "no")

# --with-systemdsystemunitdir=DIR
AC_ARG_WITH([systemdsystemunitdir],
  AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files]),
  [], [with_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)])
AS_IF([test "x$with_systemdsystemunitdir" != xno],
	[AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])])
AM_CONDITIONAL(HAVE_SYSTEMD, [test -n "$with_systemdsystemunitdir" -a "x$with_systemdsystemunitdir" != xno ])

# --enable-embedded
AC_ARG_ENABLE(embedded,
  AS_HELP_STRING([--enable-embedded], [limit RAM and CPU resources by disabling features (log)]),
  [ use_embedded="${enableval}" ])

AS_IF([test x$use_embedded = xyes],
	[AC_DEFINE(NO_LOG, 1, [Disable logging support])
	 PCSCLITE_FEATURES="${PCSCLITE_FEATURES} nolog"])

# --enable-usbdropdir=DIR
AC_ARG_ENABLE(usbdropdir,
  AS_HELP_STRING([--enable-usbdropdir=DIR], [directory containing USB
				  drivers (default /usr/local/lib/pcsc/drivers)]),
[usbdropdir="${enableval}"], [usbdropdir=false])
AS_IF([test x${usbdropdir} = xfalse],
	[AS_IF([test "x$libdir" != xNONE],
		[usbdropdir="$libdir/pcsc/drivers"],
		[usbdropdir="$ac_default_libdir/pcsc/drivers"])
])
AX_RECURSIVE_EVAL($usbdropdir,usbdropdir_exp)
usbdropdir=$usbdropdir_exp
AC_DEFINE_UNQUOTED(PCSCLITE_HP_DROPDIR, "$usbdropdir", [directory containing USB drivers])
PCSCLITE_FEATURES="${PCSCLITE_FEATURES} usbdropdir=${usbdropdir}"

# --enable-debugatr
AC_ARG_ENABLE(debugatr,
  AS_HELP_STRING([--enable-debugatr], [enable ATR debug messages from pcscd]),
[ AS_CASE(["${enableval}"],
	[yes], [debugatr=true],
	[no], [debugatr=false],
	[*], [AC_MSG_ERROR([bad value ${enableval} for --enable-debugatr])])
], [debugatr=false])

AS_IF([test x${debugatr} = xtrue],
	[AC_DEFINE(ATR_DEBUG, 1, [display ATR parsing debug messages.])
	 PCSCLITE_FEATURES="${PCSCLITE_FEATURES} debugatr"])

# --enable-ipcdir=DIR
AC_ARG_ENABLE(ipcdir,
  AS_HELP_STRING([--enable-ipcdir=DIR], [directory containing IPC files
				  (default /run/pcscd)]),
[ipcdir="${enableval}"], [ipcdir=false])

AS_IF([test x${ipcdir} = xfalse], [ipcdir="/run/pcscd"])
PCSCLITE_FEATURES="${PCSCLITE_FEATURES} ipcdir=${ipcdir}"

CPPFLAGS="-I\${top_srcdir}/src $CPPFLAGS"

# --enable-confdir=DIR
AC_ARG_ENABLE(confdir,
AS_HELP_STRING([--enable-confdir=DIR], [directory containing reader configurations (default ${sysconfdir}/reader.conf.d)]),
	[confdir="${enableval}"],
	[confdir="${sysconfdir}/reader.conf.d"])

# --disable-filter
AC_ARG_ENABLE(filter,
  AS_HELP_STRING([--disable-filter], [disable reader filtering using
				  PCSCLITE_FILTER_IGNORE_READER_NAMES and PCSCLITE_FILTER_EXTEND_READER_NAMES]),
  [ use_filter="${enableval}" ], [ use_filter="yes" ])

AS_IF([test x$use_filter = xyes],
	[AC_DEFINE(FILTER_NAMES, 1, [Filter reader names])
	 PCSCLITE_FEATURES="${PCSCLITE_FEATURES} filter"])

# --enable-strict
AC_ARG_ENABLE(
	[strict],
	[AS_HELP_STRING([--enable-strict], [enable strict compile mode @<:@enabled@:>@])],
	,
	[enable_strict="no"]
)
AS_IF([test "${enable_strict}" = "yes"], [CFLAGS="-Wall -Wextra -Werror ${CFLAGS}"])

# Setup dist stuff
AC_SUBST(usbdropdir)
AC_SUBST(ipcdir)
AC_SUBST(serialconfdir)
AC_SUBST(host_to_ccid_16)
AC_SUBST(host_to_ccid_32)
AX_RECURSIVE_EVAL($confdir,confdir_exp)
AX_RECURSIVE_EVAL($sbindir,sbindir_exp)

PCSCLITE_CONFIG_DIR="${confdir_exp}"
AC_SUBST(PCSCLITE_CONFIG_DIR)
AC_SUBST(sbindir_exp)
PCSCLITE_FEATURES="${PCSCLITE_FEATURES} configdir=${PCSCLITE_CONFIG_DIR}"
AC_DEFINE_UNQUOTED([PCSCLITE_FEATURES], ["${PCSCLITE_FEATURES}"], [Enabled PC/SC lite features])

cat << EOF

PC/SC lite has been configured with following options:

Version:             ${PACKAGE_VERSION}
System binaries:     ${sbindir_exp}
Configuration dir:   ${PCSCLITE_CONFIG_DIR}

Host:                ${host}
Compiler:            ${CC}
Preprocessor flags:  ${CPPFLAGS}
Compiler flags:      ${CFLAGS}
Preprocessor flags:  ${CPPFLAGS}
Linker flags:        ${LDFLAGS}
Libraries:           ${LIBS}

PTHREAD_CFLAGS:      ${PTHREAD_CFLAGS}
PTHREAD_LIBS:        ${PTHREAD_LIBS}
PCSC_ARCH:           ${PCSC_ARCH}

polkit support:         ${use_polkit}
polkit policy dir:      ${polkit_policy_dir}
libudev support:        ${use_libudev}
libusb support:         ${use_libusb}
USB drop directory:     ${usbdropdir}
ATR parsing messages:   ${debugatr}
ipcdir:                 ${ipcdir}
use serial:             ${use_serial}
use usb:                ${use_usb}
use libsystemd:         ${use_libsystemd}
systemd unit directory: ${with_systemdsystemunitdir}
serial config dir.:     ${confdir_exp}
filter:                 ${use_filter}
documentation:          ${enable_doc}

PCSCLITE_FEATURES:      ${PCSCLITE_FEATURES}

EOF

# Write Makefiles
AC_CONFIG_FILES([
Makefile
doc/Makefile
doc/doxygen.conf
doc/pcscd.8
doc/reader.conf.5
doc/example/Makefile
etc/Makefile
src/Makefile
src/libpcsclite.pc
src/pcscd.h
src/PCSC/pcsclite.h
src/spy/Makefile
])
AC_OUTPUT
