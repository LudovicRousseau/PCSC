/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

//#include "PCSC.h"

/* Define if lex declares yytext as a char * by default, not a char[].  */
#define YYTEXT_POINTER 1

/* Define if you have the daemon function.  */
#define HAVE_DAEMON 1

/* Name of package */
#define PACKAGE "pcsc-lite"

/* Version number of package */
#define VERSION "1.0.2.cvs"

/* Define to the necessary symbol if this constant
                           uses a non-standard name on your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* enable full PCSC debug messaging. */
#define PCSC_DEBUG 1

/* enable full musclecard debug messaging. */
#define MSC_DEBUG 1

/* display ATR parsing debug messages. */
/* #undef ATR_DEBUG */

/* directory containing reader.conf (default /etc) */
/* #undef USE_READER_CONF */

/* file containing pcscd pid */
/* #undef USE_RUN_PID */

/* enable client side thread safety. */
#define USE_THREAD_SAFETY 1
