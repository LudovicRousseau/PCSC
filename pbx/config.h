/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if you have the daemon function.  */
#define HAVE_DAEMON 1

/* Name of package */
#define PACKAGE "PCSC Framework"

/* Version number of package */
#define VERSION "1.1.2beta1"

/* OSX */
#define PCSC_TARGET_OSX 1
#define MSC_TARGET_OSX 1

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* enable full PCSC debug messaging. */
/* #define PCSC_DEBUG */

/* enable full musclecard debug messaging. */
/* #define MSC_DEBUG */

/* display ATR parsing debug messages. */
/* #define ATR_DEBUG */

/* send messages to syslog instead of stdout */
/* #define USE_SYSLOG */

/* pcsc runs as a daemon in the background. */
#define USE_DAEMON 1

/* enable client side thread safety. */
#define USE_THREAD_SAFETY 1

/* file containing pcscd pid */
#define USE_RUN_PID "/var/run/pcscd.pid"

