/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Name of package */
#define PACKAGE "PCSC Framework"

/* Version number of package */
#define VERSION "1.1.2"

/* OSX */
#define PCSC_TARGET_SOLARIS 1
#define MSC_TARGET_SOLARIS 1

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* Define if you have syslog */
#define HAVE_SYSLOG_H 1

/* enable full PCSC debug messaging. */
  #define PCSC_DEBUG 1

/* enable full musclecard debug messaging. */
  #define MSC_DEBUG 1

/* display ATR parsing debug messages. */
/* #define ATR_DEBUG */

/* send messages to syslog instead of stdout */
/* #define USE_SYSLOG */

/* pcsc runs as a daemon in the background. */
#define USE_DAEMON 1

/* enable client side thread safety. */
#define USE_THREAD_SAFETY 1

/* enable run pid */
#define USE_RUN_PID "/var/run/pcscd.pid"
