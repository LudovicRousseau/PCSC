/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2018
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief This is the main pcscd daemon.
 *
 * The function \c main() starts up the communication environment.\n
 * Then an endless loop is calld to look for Client connections. For each
 * Client connection a call to \c CreateContextThread() is done.
 */

#include "config.h"
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef USE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "misc.h"
#include "pcsclite.h"
#include "pcscd.h"
#include "debuglog.h"
#include "winscard_msg.h"
#include "winscard_svc.h"
#include "sys_generic.h"
#include "hotplug.h"
#include "readerfactory.h"
#include "configfile.h"
#include "utils.h"
#include "eventhandler.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

char AraKiri = FALSE;
static char Init = TRUE;
char AutoExit = FALSE;
char SocketActivated = FALSE;
static int ExitValue = EXIT_FAILURE;
int HPForceReaderPolling = 0;
static int pipefd[] = {-1, -1};
static int signal_handler_fd[] = {-1, -1};
char Add_Serial_In_Name = TRUE;
char Add_Interface_In_Name = TRUE;

/*
 * Some internal functions
 */
static void at_exit(void);
static void clean_temp_files(void);
static void signal_trap(int);
static void print_version(void);
static void print_usage(char const * const);

/**
 * @brief The Server's Message Queue Listener function.
 *
 * An endless loop calls the function \c ProcessEventsServer() to check for
 * messages sent by clients.
 * If the message is valid, \c CreateContextThread() is called to serve this
 * request.
 */
static void SVCServiceRunLoop(void)
{
	int rsp;
	LONG rv;
	uint32_t dwClientID;	/* Connection ID used to reference the Client */

	while (TRUE)
	{
		if (AraKiri)
		{
			/* stop the hotpug thread and waits its exit */
#ifdef USE_USB
			(void)HPStopHotPluggables();
#endif
			(void)SYS_Sleep(1);

			/* now stop all the drivers */
			RFCleanupReaders();
			EHDeinitializeEventStructures();
			ContextsDeinitialize();
			at_exit();
		}

		switch (rsp = ProcessEventsServer(&dwClientID))
		{

		case 0:
			Log2(PCSC_LOG_DEBUG, "A new context thread creation is requested: %d", dwClientID);
			rv = CreateContextThread(&dwClientID);

			if (rv != SCARD_S_SUCCESS)
				Log1(PCSC_LOG_ERROR, "Problem during the context thread creation");
			break;

		case 2:
			/*
			 * timeout in ProcessEventsServer(): do nothing
			 * this is used to catch the Ctrl-C signal at some time when
			 * nothing else happens
			 */
			break;

		case -1:
			Log1(PCSC_LOG_ERROR, "Error in ProcessEventsServer");
			break;

		case -2:
			/* Nothing to do in case of a syscall interrupted
			 * It happens when SIGUSR1 (reload) or SIGINT (Ctrl-C) is received
			 * We just try again */

			/* we wait a bit so that the signal handler thread can do
			 * its job and set AraKiri if needed */
			SYS_USleep(1000);
			break;

		default:
			Log2(PCSC_LOG_ERROR, "ProcessEventsServer unknown retval: %d",
				rsp);
			break;
		}
	}
}

/**
 * thread dedicated to handle signals
 *
 * a signal handler can not call any function. See signal(7) for a list
 * of function that are safe to call from a signal handler.
 * The functions syslog(), gettimeofday() and remove() are NOT safe.
 */
static void *signal_thread(void *arg)
{
	(void)arg;

	while (TRUE)
	{
		int r;
		int sig;

		r = read(signal_handler_fd[0], &sig, sizeof sig);
		if (r < 0)
		{
			Log2(PCSC_LOG_ERROR, "read failed: %s", strerror(errno));
			return NULL;
		}

		Log2(PCSC_LOG_INFO, "Received signal: %d", sig);

		/* signal for hotplug */
		if (SIGUSR1 == sig)
		{
#ifdef USE_USB
			if (! AraKiri)
				HPReCheckSerialReaders();
#endif
			/* Reenable the signal handler.
			 * This is needed on Solaris and HPUX. */
			(void)signal(SIGUSR1, signal_trap);

			continue;
		}

		/* do not wait if asked to terminate
		 * avoids waiting after the reader(s) in shutdown for example */
		if (SIGTERM == sig)
		{
			Log1(PCSC_LOG_INFO, "Direct suicide");
			ExitValue = EXIT_SUCCESS;
			at_exit();
		}

		if (SIGALRM == sig)
		{
			/* normal exit without error */
			ExitValue = EXIT_SUCCESS;
		}

		/* the signal handler is called several times for the same Ctrl-C */
		if (AraKiri == FALSE)
		{
			Log1(PCSC_LOG_INFO, "Preparing for suicide");
			AraKiri = TRUE;

			/* if still in the init/loading phase the AraKiri will not be
			 * seen by the main event loop
			 */
			if (Init)
			{
				Log1(PCSC_LOG_INFO, "Suicide during init");
				at_exit();
			}
		}
		else
		{
			/* if pcscd do not want to die */
			static int lives = 2;

			lives--;
			/* no live left. Something is blocking the normal death. */
			if (0 == lives)
			{
				Log1(PCSC_LOG_INFO, "Forced suicide");
				at_exit();
			}
		}
	}

	return NULL;
}


int main(int argc, char **argv)
{
	int rv;
	char setToForeground;
	char HotPlug;
	char *newReaderConfig;
	struct stat fStatBuf;
	int customMaxThreadCounter = 0;
	int customMaxReaderHandles = 0;
	int customMaxThreadCardHandles = 0;
	int opt;
	int r;
#ifdef HAVE_GETOPT_LONG
	int option_index = 0;
	static struct option long_options[] = {
		{"config", 1, NULL, 'c'},
		{"foreground", 0, NULL, 'f'},
		{"color", 0, NULL, 'T'},
		{"help", 0, NULL, 'h'},
		{"version", 0, NULL, 'v'},
		{"apdu", 0, NULL, 'a'},
		{"debug", 0, NULL, 'd'},
		{"info", 0, NULL, 'i'},
		{"error", 0, NULL, 'e'},
		{"critical", 0, NULL, 'C'},
		{"hotplug", 0, NULL, 'H'},
		{"force-reader-polling", optional_argument, NULL, 0},
		{"max-thread", 1, NULL, 't'},
		{"max-card-handle-per-thread", 1, NULL, 's'},
		{"max-card-handle-per-reader", 1, NULL, 'r'},
		{"auto-exit", 0, NULL, 'x'},
		{"reader-name-no-serial", 0, NULL, 'S'},
		{"reader-name-no-interface", 0, NULL, 'I'},
		{NULL, 0, NULL, 0}
	};
#endif
#define OPT_STRING "c:fTdhvaieCHt:r:s:xSI"

	newReaderConfig = NULL;
	setToForeground = FALSE;
	HotPlug = FALSE;

	/*
	 * test the version
	 */
	if (strcmp(PCSCLITE_VERSION_NUMBER, VERSION) != 0)
	{
		printf("BUILD ERROR: The release version number PCSCLITE_VERSION_NUMBER\n");
		printf("  in pcsclite.h (%s) does not match the release version number\n",
			PCSCLITE_VERSION_NUMBER);
		printf("  generated in config.h (%s) (see configure.in).\n", VERSION);

		return EXIT_FAILURE;
	}

	/* Init the PRNG */
	SYS_InitRandom();

	/*
	 * By default we create a daemon (not connected to any output)
	 * so log to syslog to have error messages.
	 */
	DebugLogSetLogType(DEBUGLOG_SYSLOG_DEBUG);

	/*
	 * Handle any command line arguments
	 */
#ifdef  HAVE_GETOPT_LONG
	while ((opt = getopt_long (argc, argv, OPT_STRING, long_options, &option_index)) != -1) {
#else
	while ((opt = getopt (argc, argv, OPT_STRING)) != -1) {
#endif
		switch (opt) {
#ifdef  HAVE_GETOPT_LONG
			case 0:
				if (strcmp(long_options[option_index].name,
					"force-reader-polling") == 0)
					HPForceReaderPolling = optarg ? abs(atoi(optarg)) : 1;
				break;
#endif
			case 'c':
				Log2(PCSC_LOG_INFO, "using new config file: %s", optarg);
				newReaderConfig = optarg;
				break;

			case 'f':
				setToForeground = TRUE;
				/* debug to stdout instead of default syslog */
				DebugLogSetLogType(DEBUGLOG_STDOUT_DEBUG);
				Log1(PCSC_LOG_INFO,
					"pcscd set to foreground with debug send to stdout");
				break;

			case 'T':
				DebugLogSetLogType(DEBUGLOG_STDOUT_COLOR_DEBUG);
				Log1(PCSC_LOG_INFO, "Force colored logs");
				break;

			case 'd':
				DebugLogSetLevel(PCSC_LOG_DEBUG);
				break;

			case 'i':
				DebugLogSetLevel(PCSC_LOG_INFO);
				break;

			case 'e':
				DebugLogSetLevel(PCSC_LOG_ERROR);
				break;

			case 'C':
				DebugLogSetLevel(PCSC_LOG_CRITICAL);
				break;

			case 'h':
				print_usage (argv[0]);
				return EXIT_SUCCESS;

			case 'v':
				print_version ();
				return EXIT_SUCCESS;

			case 'a':
				DebugLogSetCategory(DEBUG_CATEGORY_APDU);
				break;

			case 'H':
				/* debug to stdout instead of default syslog */
				DebugLogSetLogType(DEBUGLOG_STDOUT_DEBUG);
				HotPlug = TRUE;
				break;

			case 't':
				customMaxThreadCounter = optarg ? atoi(optarg) : 0;
				Log2(PCSC_LOG_INFO, "setting customMaxThreadCounter to: %d",
					customMaxThreadCounter);
				break;

			case 'r':
				customMaxReaderHandles = optarg ? atoi(optarg) : 0;
				Log2(PCSC_LOG_INFO, "setting customMaxReaderHandles to: %d",
					customMaxReaderHandles);
				break;

			case 's':
				customMaxThreadCardHandles = optarg ? atoi(optarg) : 0;
				Log2(PCSC_LOG_INFO, "setting customMaxThreadCardHandles to: %d",
					customMaxThreadCardHandles);
				break;

			case 'x':
				AutoExit = TRUE;
				Log2(PCSC_LOG_INFO, "Auto exit after %d seconds of inactivity",
					TIME_BEFORE_SUICIDE);
				break;

			case 'S':
				Add_Serial_In_Name = FALSE;
				break;

			case 'I':
				Add_Interface_In_Name = FALSE;
				break;

			default:
				print_usage (argv[0]);
				return EXIT_FAILURE;
		}

	}

	if (argv[optind])
	{
		printf("Unknown option: %s\n", argv[optind]);
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

#ifdef USE_LIBSYSTEMD
	/*
	 * Check if systemd passed us any file descriptors
	 */
	rv = sd_listen_fds(0);
	if (rv > 1)
	{
		Log1(PCSC_LOG_CRITICAL, "Too many file descriptors received");
		return EXIT_FAILURE;
	}
	else
	{
		if (rv == 1)
		{
			SocketActivated = TRUE;
			Log1(PCSC_LOG_INFO, "Started by systemd");
		}
		else
			SocketActivated = FALSE;
	}
#endif

	/*
	 * test the presence of /var/run/pcscd/pcscd.comm
	 */

	rv = stat(PCSCLITE_CSOCK_NAME, &fStatBuf);

	/* if the file exist and pcscd was _not_ started by systemd */
	if (rv == 0 && !SocketActivated)
	{
		pid_t pid;

		/* read the pid file to get the old pid and test if the old pcscd is
		 * still running
		 */
		pid = GetDaemonPid();

		if (pid != -1)
		{
			if (HotPlug)
				return SendHotplugSignal();

			rv = kill(pid, 0);
			if (0 == rv)
			{
				Log1(PCSC_LOG_CRITICAL,
					"file " PCSCLITE_CSOCK_NAME " already exists.");
				Log2(PCSC_LOG_CRITICAL,
					"Another pcscd (pid: %ld) seems to be running.", (long)pid);
				return EXIT_FAILURE;
			}
			else
				if (ESRCH == errno)
				{
					/* the old pcscd is dead. make some cleanup */
					clean_temp_files();
				}
				else
				{
					/* permission denied or other error */
					Log2(PCSC_LOG_CRITICAL, "kill failed: %s", strerror(errno));
					return EXIT_FAILURE;
				}
		}
		else
		{
			if (HotPlug)
			{
				Log1(PCSC_LOG_CRITICAL, "file " PCSCLITE_RUN_PID " do not exist");
				Log1(PCSC_LOG_CRITICAL, "Hotplug failed");
				return EXIT_FAILURE;
			}
		}
	}
	else
		if (HotPlug)
		{
			Log1(PCSC_LOG_CRITICAL, "Hotplug failed: pcscd is not running");
			return EXIT_FAILURE;
		}

	/* like in daemon(3): changes the current working directory to the
	 * root ("/") */
	r = chdir("/");
	if (r < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "chdir() failed: %s", strerror(errno));
		return EXIT_FAILURE;
	}

	/*
	 * If this is set to one the user has asked it not to fork
	 */
	if (!setToForeground)
	{
		int pid;
		int fd;

		if (pipe(pipefd) == -1)
		{
			Log2(PCSC_LOG_CRITICAL, "pipe() failed: %s", strerror(errno));
			return EXIT_FAILURE;
		}

		pid = fork();
		if (-1 == pid)
		{
			Log2(PCSC_LOG_CRITICAL, "fork() failed: %s", strerror(errno));
			return EXIT_FAILURE;
		}

		/* like in daemon(3): redirect standard input, standard output
		 * and standard error to /dev/null */
		fd = open("/dev/null", O_RDWR);
		if (fd != -1)
		{
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);

			/* do not close stdin, stdout or stderr */
			if (fd > 2)
				close(fd);
		}

		if (pid)
		/* in the father */
		{
			char buf;
			int ret;

			/* close write side */
			close(pipefd[1]);

			/* wait for the son to write the return code */
			ret = read(pipefd[0], &buf, 1);
			if (ret <= 0)
				return 2;

			close(pipefd[0]);

			/* exit code */
			return buf;
		}
		else
		/* in the son */
		{
			/* close read side */
			close(pipefd[0]);
		}
	}

	/*
	 * cleanly remove /var/run/pcscd/files when exiting
	 * signal_trap() does just set a global variable used by the main loop
	 */
	(void)signal(SIGQUIT, signal_trap);
	(void)signal(SIGTERM, signal_trap); /* default kill signal & init round 1 */
	(void)signal(SIGINT, signal_trap);	/* sent by Ctrl-C */

	/* exits on SIGALARM to allow pcscd to suicide if not used */
	(void)signal(SIGALRM, signal_trap);

	if (pipe(signal_handler_fd) == -1)
	{
		Log2(PCSC_LOG_CRITICAL, "pipe() failed: %s", strerror(errno));
		return EXIT_FAILURE;
	}

	pthread_t signal_handler_thread;
	rv = pthread_create(&signal_handler_thread, NULL, signal_thread, NULL);
	if (rv)
	{
		Log2(PCSC_LOG_CRITICAL, "pthread_create failed: %s", strerror(rv));
		return EXIT_FAILURE;
	}

	/*
	 * If PCSCLITE_IPC_DIR does not exist then create it
	 */
	{
		int mode = S_IROTH | S_IXOTH | S_IRGRP | S_IXGRP | S_IRWXU;

		rv = mkdir(PCSCLITE_IPC_DIR, mode);
		if ((rv != 0) && (errno != EEXIST))
		{
			Log2(PCSC_LOG_CRITICAL,
				"cannot create " PCSCLITE_IPC_DIR ": %s", strerror(errno));
			return EXIT_FAILURE;
		}

		/* set mode so that the directory is world readable and
		 * executable even is umask is restrictive
		 * The directory containes files used by libpcsclite */
		(void)chmod(PCSCLITE_IPC_DIR, mode);
	}

	/*
	 * Allocate memory for reader structures
	 */
	rv = RFAllocateReaderSpace(customMaxReaderHandles);
	if (SCARD_S_SUCCESS != rv)
		at_exit();

#ifdef USE_SERIAL
	/*
	 * Grab the information from the reader.conf
	 */
	if (newReaderConfig)
	{
		rv = RFStartSerialReaders(newReaderConfig);
		if (rv != 0)
		{
			Log3(PCSC_LOG_CRITICAL, "invalid file %s: %s", newReaderConfig,
				strerror(errno));
			at_exit();
		}
	}
	else
	{
		rv = RFStartSerialReaders(PCSCLITE_CONFIG_DIR);
		if (rv == -1)
			at_exit();
	}
#endif

	Log1(PCSC_LOG_INFO, "pcsc-lite " VERSION " daemon ready.");

	/*
	 * Record our pid to make it easier
	 * to kill the correct pcscd
	 *
	 * Do not fork after this point or the stored pid will be wrong
	 */
	{
		int f;
		int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

		f = open(PCSCLITE_RUN_PID, O_RDWR | O_CREAT, mode);
		if (f != -1)
		{
			char pid[PID_ASCII_SIZE];
			ssize_t rr;

			(void)snprintf(pid, sizeof(pid), "%u\n", (unsigned) getpid());
			rr = write(f, pid, strlen(pid) + 1);
			if (rr < 0)
			{
				Log2(PCSC_LOG_CRITICAL,
					"writing " PCSCLITE_RUN_PID " failed: %s",
					strerror(errno));
			}

			/* set mode so that the file is world readable even is umask is
			 * restrictive
			 * The file is used by libpcsclite */
			(void)fchmod(f, mode);

			(void)close(f);
		}
		else
			Log2(PCSC_LOG_CRITICAL, "cannot create " PCSCLITE_RUN_PID ": %s",
				strerror(errno));
	}

	/*
	 * post initialistion
	 */
	Init = FALSE;

	/*
	 * Hotplug rescan
	 */
	(void)signal(SIGUSR1, signal_trap);

	/*
	 * Initialize the comm structure
	 */
#ifdef USE_LIBSYSTEMD
	if (SocketActivated)
		rv = ListenExistingSocket(SD_LISTEN_FDS_START + 0);
	else
#endif
		rv = InitializeSocket();

	if (rv)
	{
		Log1(PCSC_LOG_CRITICAL, "Error initializing pcscd.");
		at_exit();
	}

	/*
	 * Initialize the contexts structure
	 */
	rv = ContextsInitialize(customMaxThreadCounter, customMaxThreadCardHandles);

	if (rv == -1)
	{
		Log1(PCSC_LOG_CRITICAL, "Error initializing pcscd.");
		at_exit();
	}

	(void)signal(SIGPIPE, SIG_IGN);
	(void)signal(SIGHUP, SIG_IGN);	/* needed for Solaris. The signal is sent
				 * when the shell is existed */

#if !defined(PCSCLITE_STATIC_DRIVER) && defined(USE_USB)
	/*
	 * Set up the search for USB/PCMCIA devices
	 */
	rv = HPSearchHotPluggables();
#ifndef USE_SERIAL
	if (rv)
		at_exit();
#endif

	rv = HPRegisterForHotplugEvents();
	if (rv)
	{
		Log1(PCSC_LOG_ERROR, "HPRegisterForHotplugEvents failed");
		at_exit();
	}

	RFWaitForReaderInit();
#endif

	/* initialisation succeeded */
	if (pipefd[1] >= 0)
	{
		char buf = 0;
		ssize_t rr;

		/* write a 0 (success) to father process */
		rr = write(pipefd[1], &buf, 1);
		if (rr < 0)
		{
			Log2(PCSC_LOG_ERROR, "write() failed: %s", strerror(errno));
		}
		close(pipefd[1]);
		pipefd[1] = -1;
	}

	SVCServiceRunLoop();

	Log1(PCSC_LOG_ERROR, "SVCServiceRunLoop returned");
	return EXIT_FAILURE;
}

static void at_exit(void)
{
	Log1(PCSC_LOG_INFO, "cleaning " PCSCLITE_IPC_DIR);

	clean_temp_files();

	if (pipefd[1] >= 0)
	{
		char buf;
		ssize_t r;

		/* write the error code to father process */
		buf = ExitValue;
		r = write(pipefd[1], &buf, 1);
		if (r < 0)
		{
			Log2(PCSC_LOG_ERROR, "write() failed: %s", strerror(errno));
		}
		close(pipefd[1]);
	}

	exit(ExitValue);
}

static void clean_temp_files(void)
{
	int rv;

	if (!SocketActivated)
	{
		rv = remove(PCSCLITE_CSOCK_NAME);
		if (rv != 0)
			Log2(PCSC_LOG_ERROR, "Cannot remove " PCSCLITE_CSOCK_NAME ": %s",
				strerror(errno));
	}

	rv = remove(PCSCLITE_RUN_PID);
	if (rv != 0)
		Log2(PCSC_LOG_ERROR, "Cannot remove " PCSCLITE_RUN_PID ": %s",
			strerror(errno));
}

static void signal_trap(int sig)
{
	int r;

	r = write(signal_handler_fd[1], &sig, sizeof sig);
	if (r < 0)
		Log2(PCSC_LOG_ERROR, "write failed: %s", strerror(errno));
}

static void print_version(void)
{
	printf("%s version %s.\n",  PACKAGE, VERSION);
	printf("Copyright (C) 1999-2002 by David Corcoran <corcoran@musclecard.com>.\n");
	printf("Copyright (C) 2001-2018 by Ludovic Rousseau <ludovic.rousseau@free.fr>.\n");
	printf("Copyright (C) 2003-2004 by Damien Sauveron <sauveron@labri.fr>.\n");
	printf("Report bugs to <pcsclite-muscle@lists.infradead.org>.\n");

	printf ("Enabled features:%s\n", PCSCLITE_FEATURES);
}

static void print_usage(char const * const progname)
{
	printf("Usage: %s options\n", progname);
	printf("Options:\n");
#ifdef HAVE_GETOPT_LONG
	printf("  -a, --apdu		log APDU commands and results\n");
	printf("  -c, --config		path to reader.conf\n");
	printf("  -f, --foreground	run in foreground (no daemon),\n");
	printf("			send logs to stdout instead of syslog\n");
	printf("  -T, --color		force use of colored logs\n");
	printf("  -h, --help		display usage information\n");
	printf("  -H, --hotplug		ask the daemon to rescan the available readers\n");
	printf("  -v, --version		display the program version number\n");
	printf("  -d, --debug		display lower level debug messages\n");
	printf("  -i, --info		display info level debug messages\n");
	printf("  -e  --error		display error level debug messages (default level)\n");
	printf("  -C  --critical	display critical only level debug messages\n");
	printf("  --force-reader-polling ignore the IFD_GENERATE_HOTPLUG reader capability\n");
	printf("  -t, --max-thread	maximum number of threads (default %d)\n", PCSC_MAX_CONTEXT_THREADS);
	printf("  -s, --max-card-handle-per-thread	maximum number of card handle per thread (default: %d)\n", PCSC_MAX_CONTEXT_CARD_HANDLES);
	printf("  -r, --max-card-handle-per-reader	maximum number of card handle per reader (default: %d)\n", PCSC_MAX_READER_HANDLES);
	printf("  -x, --auto-exit	pcscd will quit after %d seconds of inactivity\n", TIME_BEFORE_SUICIDE);
	printf("  -S, --reader-name-no-serial    do not include the USB serial number in the name\n");
	printf("  -I, --reader-name-no-interface do not include the USB interface name in the name\n");
#else
	printf("  -a    log APDU commands and results\n");
	printf("  -c	path to reader.conf\n");
	printf("  -f	run in foreground (no daemon), send logs to stdout instead of syslog\n");
	printf("  -T    force use of colored logs\n");
	printf("  -d	display debug messages.\n");
	printf("  -i	display info messages.\n");
	printf("  -e	display error messages (default level).\n");
	printf("  -C	display critical messages.\n");
	printf("  -h	display usage information\n");
	printf("  -H	ask the daemon to rescan the available readers\n");
	printf("  -v	display the program version number\n");
	printf("  -t    maximum number of threads\n");
	printf("  -s    maximum number of card handle per thread\n");
	printf("  -r    maximum number of card handle per reader\n");
	printf("  -x	pcscd will quit after %d seconds of inactivity\n", TIME_BEFORE_SUICIDE);
#endif
}

