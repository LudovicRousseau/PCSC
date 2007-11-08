/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 1999-2007
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
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
#include <syslog.h>
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

#include "misc.h"
#include "pcsclite.h"
#include "pcscd.h"
#include "debuglog.h"
#include "winscard_msg.h"
#include "winscard_svc.h"
#include "sys_generic.h"
#include "thread_generic.h"
#include "hotplug.h"
#include "readerfactory.h"
#include "configfile.h"
#include "powermgt_generic.h"
#include "utils.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

char AraKiri = FALSE;
static char Init = TRUE;
int HPForceReaderPolling = 0;

/*
 * Some internal functions
 */
void SVCServiceRunLoop(void);
void SVCClientCleanup(psharedSegmentMsg);
void at_exit(void);
void clean_temp_files(void);
void signal_reload(int sig);
void signal_trap(int);
void print_version (void);
void print_usage (char const * const);

PCSCLITE_MUTEX usbNotifierMutex;

int SendHotplugSignal(void)
{
	pid_t pid;

	pid = GetDaemonPid();

	if (pid != -1)
	{
		Log2(PCSC_LOG_INFO, "Send hotplug signal to pcscd (pid=%d)", pid);
		if (kill(pid, SIGUSR1) < 0)
		{
			Log3(PCSC_LOG_CRITICAL, "Can't signal pcscd (pid=%d): %s",
				pid, strerror(errno));
			return EXIT_FAILURE ;
		}
	}

	return EXIT_SUCCESS;
} /* SendHotplugSignal */

/*
 * Cleans up messages still on the queue when a client dies
 */
void SVCClientCleanup(psharedSegmentMsg msgStruct)
{
	/*
	 * May be implemented in future releases
	 */
}

/**
 * @brief The Server's Message Queue Listener function.
 *
 * An endless loop calls the function \c SHMProcessEventsServer() to check for
 * messages sent by clients.
 * If the message is valid, \c CreateContextThread() is called to serve this
 * request.
 */
void SVCServiceRunLoop(void)
{
	int rsp;
	LONG rv;
	uint32_t dwClientID;	/* Connection ID used to reference the Client */

	rsp = 0;
	rv = 0;

	/*
	 * Initialize the comm structure
	 */
	rsp = SHMInitializeCommonSegment();

	if (rsp == -1)
	{
		Log1(PCSC_LOG_CRITICAL, "Error initializing pcscd.");
		exit(-1);
	}

	/*
	 * Initialize the contexts structure
	 */
	rv = ContextsInitialize();

	if (rv == -1)
	{
		Log1(PCSC_LOG_CRITICAL, "Error initializing pcscd.");
		exit(-1);
	}

	/*
	 * Solaris sends a SIGALRM and it is annoying
	 */

	signal(SIGALRM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);	/* needed for Solaris. The signal is sent
				 * when the shell is existed */

	/*
	 * This function always returns zero
	 */
	rsp = SYS_MutexInit(&usbNotifierMutex);

	/*
	 * Set up the search for USB/PCMCIA devices
	 */
	HPSearchHotPluggables();
	HPRegisterForHotplugEvents();

	/*
	 * Set up the power management callback routine
	 */
	PMRegisterForPowerEvents();

	while (TRUE)
	{
		switch (rsp = SHMProcessEventsServer(&dwClientID, 0))
		{

		case 0:
			Log2(PCSC_LOG_DEBUG, "A new context thread creation is requested: %d", dwClientID);
			rv = CreateContextThread(&dwClientID);

 			if (rv != SCARD_S_SUCCESS)
				Log1(PCSC_LOG_ERROR, "Problem during the context thread creation");
			break;

		case 2:
			/*
			 * timeout in SHMProcessEventsServer(): do nothing
			 * this is used to catch the Ctrl-C signal at some time when
			 * nothing else happens
			 */
			break;

		case -1:
			Log1(PCSC_LOG_ERROR, "Error in SHMProcessEventsServer");
			break;

		case -2:
			/* Nothing to do in case of a syscall interrupted
			 * It happens when SIGUSR1 (reload) or SIGINT (Ctrl-C) is received
			 * We just try again */
			break;

		default:
			Log2(PCSC_LOG_ERROR, "SHMProcessEventsServer unknown retval: %d",
				rsp);
			break;
		}

		if (AraKiri)
		{
			/* stop the hotpug thread and waits its exit */
			HPStopHotPluggables();
			SYS_Sleep(1);

			/* now stop all the drivers */
			RFCleanupReaders(1);
		}
	}
}

int main(int argc, char **argv)
{
	int rv;
	char setToForeground;
	char HotPlug;
	char *newReaderConfig;
	struct stat fStatBuf;
	int opt;
#ifdef HAVE_GETOPT_LONG
	int option_index = 0;
	static struct option long_options[] = {
		{"config", 1, NULL, 'c'},
		{"foreground", 0, NULL, 'f'},
		{"help", 0, NULL, 'h'},
		{"version", 0, NULL, 'v'},
		{"apdu", 0, NULL, 'a'},
		{"debug", 0, NULL, 'd'},
		{"info", 0, NULL, 0},
		{"error", 0, NULL, 'e'},
		{"critical", 0, NULL, 'C'},
		{"hotplug", 0, NULL, 'H'},
		{"force-reader-polling", optional_argument, NULL, 0},
		{NULL, 0, NULL, 0}
	};
#endif
#define OPT_STRING "c:fdhvaeCH"

	rv = 0;
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
				/* debug to stderr instead of default syslog */
				DebugLogSetLogType(DEBUGLOG_STDERR_DEBUG);
				Log1(PCSC_LOG_INFO,
					"pcscd set to foreground with debug send to stderr");
				break;

			case 'd':
				DebugLogSetLevel(PCSC_LOG_DEBUG);
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
				/* debug to stderr instead of default syslog */
				DebugLogSetLogType(DEBUGLOG_STDERR_DEBUG);
				HotPlug = TRUE;
				break;

			default:
				print_usage (argv[0]);
				return EXIT_FAILURE;
		}

	}

	if (argv[optind])
	{
		printf("Unknown option: %s\n\n", argv[optind]);
		print_usage(argv[0]);
		return EXIT_SUCCESS;
	}

	/*
	 * test the presence of /var/run/pcsc.pub
	 */

	rv = SYS_Stat(PCSCLITE_PUBSHM_FILE, &fStatBuf);

	if (rv == 0)
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

			if (kill(pid, 0) == 0)
			{
				Log1(PCSC_LOG_CRITICAL,
					"file " PCSCLITE_PUBSHM_FILE " already exists.");
				Log2(PCSC_LOG_CRITICAL,
					"Another pcscd (pid: %d) seems to be running.", pid);
				return EXIT_FAILURE;
			}
			else
				/* the old pcscd is dead. make some cleanup */
				clean_temp_files();
		}
		else
		{
			if (HotPlug)
			{
				Log1(PCSC_LOG_CRITICAL, "file " PCSCLITE_RUN_PID " do not exist");
				Log1(PCSC_LOG_CRITICAL, "Hotplug failed");
				return EXIT_FAILURE;
			}

			Log1(PCSC_LOG_CRITICAL,
				"file " PCSCLITE_PUBSHM_FILE " already exists.");
			Log1(PCSC_LOG_CRITICAL,
				"Maybe another pcscd is running?");
			Log1(PCSC_LOG_CRITICAL,
				"I can't read process pid from " PCSCLITE_RUN_PID);
			Log1(PCSC_LOG_CRITICAL,
				"Remove " PCSCLITE_PUBSHM_FILE " and " PCSCLITE_CSOCK_NAME);
			Log1(PCSC_LOG_CRITICAL,
				"if pcscd is not running to clear this message.");
			return EXIT_FAILURE;
		}
	}
	else
		if (HotPlug)
		{
			Log1(PCSC_LOG_CRITICAL, "Hotplug failed: pcscd is not running");
			return EXIT_FAILURE;
		}

	/*
	 * If this is set to one the user has asked it not to fork
	 */
	if (!setToForeground)
	{
		if (SYS_Daemon(0, 0))
			Log2(PCSC_LOG_CRITICAL, "SYS_Daemon() failed: %s",
				strerror(errno));
	}

	/*
	 * cleanly remove /tmp/pcsc when exiting
	 */
	signal(SIGQUIT, signal_trap);
	signal(SIGTERM, signal_trap);
	signal(SIGINT, signal_trap);
	signal(SIGHUP, signal_trap);

	/*
	 * If PCSCLITE_IPC_DIR does not exist then create it
	 */
	rv = SYS_Stat(PCSCLITE_IPC_DIR, &fStatBuf);
	if (rv < 0)
	{
		rv = SYS_Mkdir(PCSCLITE_IPC_DIR,
			S_ISVTX | S_IROTH | S_IXOTH | S_IRGRP | S_IXGRP | S_IRWXU);
		if (rv != 0)
		{
			Log2(PCSC_LOG_CRITICAL,
				"cannot create " PCSCLITE_IPC_DIR ": %s", strerror(errno));
			return EXIT_FAILURE;
		}
	}

	/*
	 * Record our pid to make it easier
	 * to kill the correct pcscd
	 */
	{
		int f;

		if ((f = SYS_OpenFile(PCSCLITE_RUN_PID, O_RDWR | O_CREAT, 00644)) != -1)
		{
			char pid[PID_ASCII_SIZE];

			snprintf(pid, sizeof(pid), "%u", (unsigned) getpid());
			SYS_WriteFile(f, pid, strlen(pid));
			SYS_CloseFile(f);

			/* set mode so that the file is world readable.
			 * The file is used by libpcsclite */
			SYS_Chmod(PCSCLITE_RUN_PID, 0644);
		}
		else
			Log2(PCSC_LOG_CRITICAL, "cannot create " PCSCLITE_RUN_PID ": %s",
				strerror(errno));
	}

	/* cleanly remove /var/run/pcsc.* files when exiting */
	if (atexit(at_exit))
		Log2(PCSC_LOG_CRITICAL, "atexit() failed: %s", strerror(errno));

	/*
	 * Allocate memory for reader structures
	 */
	RFAllocateReaderSpace();

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
		rv = RFStartSerialReaders(PCSCLITE_READER_CONFIG);

#if 0
		if (rv == 1)
		{
			Log1(PCSC_LOG_INFO,
				"warning: no " PCSCLITE_READER_CONFIG " found");
			/*
			 * Token error in file
			 */
		}
		else
#endif
			if (rv == -1)
				at_exit();
	}

	/*
	 * Set the default globals
	 */
	g_rgSCardT0Pci.dwProtocol = SCARD_PROTOCOL_T0;
	g_rgSCardT1Pci.dwProtocol = SCARD_PROTOCOL_T1;
	g_rgSCardRawPci.dwProtocol = SCARD_PROTOCOL_RAW;

	Log1(PCSC_LOG_INFO, "pcsc-lite " VERSION " daemon ready.");

	/*
	 * post initialistion
	 */
	Init = FALSE;

	/*
	 * signal_trap() does just set a global variable used by the main loop
	 */
	signal(SIGQUIT, signal_trap);
	signal(SIGTERM, signal_trap);
	signal(SIGINT, signal_trap);
	signal(SIGHUP, signal_trap);

	signal(SIGUSR1, signal_reload);

	SVCServiceRunLoop();

	Log1(PCSC_LOG_ERROR, "SVCServiceRunLoop returned");
	return EXIT_FAILURE;
}

void at_exit(void)
{
	Log1(PCSC_LOG_INFO, "cleaning " PCSCLITE_IPC_DIR);

	clean_temp_files();

	SYS_Exit(EXIT_SUCCESS);
}

void clean_temp_files(void)
{
	int rv;

	rv = SYS_Unlink(PCSCLITE_PUBSHM_FILE);
	if (rv != 0)
		Log2(PCSC_LOG_ERROR, "Cannot unlink " PCSCLITE_PUBSHM_FILE ": %s",
			strerror(errno));

	rv = SYS_Unlink(PCSCLITE_CSOCK_NAME);
	if (rv != 0)
		Log2(PCSC_LOG_ERROR, "Cannot unlink " PCSCLITE_CSOCK_NAME ": %s",
			strerror(errno));

	rv = SYS_Unlink(PCSCLITE_RUN_PID);
	if (rv != 0)
		Log2(PCSC_LOG_ERROR, "Cannot unlink " PCSCLITE_RUN_PID ": %s",
			strerror(errno));
}

void signal_reload(int sig)
{
	if (AraKiri)
		return;

	HPReCheckSerialReaders();
} /* signal_reload */

void signal_trap(int sig)
{
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
}

void print_version (void)
{
	printf("%s version %s.\n",  PACKAGE, VERSION);
	printf("Copyright (C) 1999-2002 by David Corcoran <corcoran@linuxnet.com>.\n");
	printf("Copyright (C) 2001-2007 by Ludovic Rousseau <ludovic.rousseau@free.fr>.\n");
	printf("Copyright (C) 2003-2004 by Damien Sauveron <sauveron@labri.fr>.\n");
	printf("Report bugs to <sclinux@linuxnet.com>.\n");
}

void print_usage (char const * const progname)
{
	printf("Usage: %s options\n", progname);
	printf("Options:\n");
#ifdef HAVE_GETOPT_LONG
	printf("  -a, --apdu		log APDU commands and results\n");
	printf("  -c, --config		path to reader.conf\n");
	printf("  -f, --foreground	run in foreground (no daemon),\n");
	printf("			send logs to stderr instead of syslog\n");
	printf("  -h, --help		display usage information\n");
	printf("  -H, --hotplug		ask the daemon to rescan the available readers\n");
	printf("  -v, --version		display the program version number\n");
	printf("  -d, --debug	 	display lower level debug messages\n");
	printf("      --info	 	display info level debug messages (default level)\n");
	printf("  -e  --error	 	display error level debug messages\n");
	printf("  -C  --critical 	display critical only level debug messages\n");
	printf("  --force-reader-polling ignore the IFD_GENERATE_HOTPLUG reader capability\n");
#else
	printf("  -a    log APDU commands and results\n");
	printf("  -c 	path to reader.conf\n");
	printf("  -f	run in foreground (no daemon), send logs to stderr instead of syslog\n");
	printf("  -d 	display debug messages. Output may be:\n");
	printf("  -h 	display usage information\n");
	printf("  -H	ask the daemon to rescan the available readers\n");
	printf("  -v 	display the program version number\n");
#endif
}

