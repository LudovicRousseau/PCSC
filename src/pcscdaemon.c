/******************************************************************

	Title  : pcscdaemon.c
	Package: PC/SC Lite
	Author : David Corcoran
	Date   : 10/24/99
	License: Copyright (C) 1999 David Corcoran
			<corcoran@linuxnet.com>
	Purpose: This is the main pcscd daemon.

$Id$

********************************************************************/

#include "config.h"

#include <time.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#ifdef  HAVE_GETOPT_H
#include <getopt.h>
#endif

#include "wintypes.h"
#include "pcsclite.h"
#include "winscard_msg.h"
#include "winscard_svc.h"
#include "sys_generic.h"
#include "thread_generic.h"
#include "hotplug.h"
#include "debuglog.h"
#include "readerfactory.h"
#include "configfile.h"
#include "powermgt_generic.h"

static char AraKiri = 0;
static char Init = 1;
extern int errno;

/*
 * Some internal functions 
 */
void SVCServiceRunLoop();
void SVCClientCleanup(psharedSegmentMsg);
void at_exit(void);
void clean_temp_files(void);
void signal_trap(int);
void print_version (void);
void print_usage (char const * const);

PCSCLITE_MUTEX usbNotifierMutex;

/*
 * Cleans up messages still on the queue when a client dies 
 */
void SVCClientCleanup(psharedSegmentMsg msgStruct)
{
	/*
	 * May be implemented in future releases 
	 */
}

/*
 * The Message Queue Listener function 
 */
void SVCServiceRunLoop()
{

	char errMessage[200];
	sharedSegmentMsg msgStruct;
	int currHandle, rsp;

	currHandle = 0, rsp = 0;

	/*
	 * Initialize the comm structure 
	 */
	rsp = SHMInitializeCommonSegment();

	if (rsp == -1)
	{
		DebugLogA("SVCServiceRunLoop: Error initializing pcscd.");
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

	while (1)
	{

		switch (rsp = SHMProcessEvents(&msgStruct, 0))
		{

		case 0:
			if (msgStruct.mtype == CMD_CLIENT_DIED)
			{
				/*
				 * Clean up the dead client 
				 */
				SYS_MutexLock(&usbNotifierMutex);
				MSGCleanupClient(&msgStruct);
				SYS_MutexUnLock(&usbNotifierMutex);

#if HAVE_SNPRINTF
				snprintf(errMessage, sizeof(errMessage), "%s%d%s",
					"SVCServiceRun: Client ", msgStruct.request_id,
					" has disappeared.");
#else
				sprintf(errMessage, "%s%d%s", "SVCServiceRun: Client ", 
                                        msgStruct.request_id, " has disappeared.");

#endif


				DebugLogB("%s", errMessage);
			} else
			{
				continue;
			}

			break;

		case 1:
			if (msgStruct.mtype == CMD_FUNCTION)
			{
				/*
				 * Command must be found 
				 */
				SYS_MutexLock(&usbNotifierMutex);
				MSGFunctionDemarshall(&msgStruct);
				rsp = SHMMessageSend(&msgStruct, msgStruct.request_id,
					PCSCLITE_SERVER_ATTEMPTS);
				SYS_MutexUnLock(&usbNotifierMutex);
			} else
			{
				continue;
			}

			break;

		case 2:
			// timeout in SHMProcessEvents(): do nothing
			// this is used to catch the Ctrl-C signal at some time when
			// nothing else happens
			break;

		case -1:
			DebugLogA("SVCServiceRun: Error in SHMProcessEvents");
			break;

		default:
			DebugLogB("SVCServiceRun: SHMProcessEvents unknown retval: %d",
				rsp);
			break;
		}

		if (AraKiri)
			RFCleanupReaders(1);
	}
}

int main(int argc, char **argv)
{

	int rv;
	char setToForeground;
	char *newReaderConfig;
	struct stat fStatBuf;
	int opt;
#ifdef HAVE_GETOPT_LONG
	int option_index;
	static struct option long_options[] = {
		{"config", 1, 0, 'c'},
		{"foreground", 0, 0, 'f'},
		{"debug", 1, 0, 'd'},
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'v'},
		{"apdu", 0, 0, 'a'},
		{0, 0, 0, 0}
	};
#endif
	
	rv = 0;
	newReaderConfig = 0;
	setToForeground = 0;

	/*
	 * test the version 
	 */
	if (strcmp(PCSCLITE_VERSION_NUMBER, VERSION) != 0)
	{
		printf("BUILD ERROR: The release version number PCSCLITE_VERSION_NUMBER\n");
		printf("  in pcsclite.h (%s) does not match the release version number\n",
			PCSCLITE_VERSION_NUMBER);
		printf("  generated in config.h (%s) (see configure.in).\n", VERSION);

		return 1;
	}

	/*
	 * log to stderr by default
	 */
	DebugLogSetLogType(DEBUGLOG_STDERR_DEBUG);

	/*
	 * Handle any command line arguments 
	 */
#ifdef  HAVE_GETOPT_LONG
	while ((opt = getopt_long (argc, argv, "c:fd:hva", long_options, &option_index)) != -1) {
#else
	while ((opt = getopt (argc, argv, "c:fd:hva")) != -1) {
#endif
		switch (opt) {
			case 'c':
				DebugLogB("main: using new config file: %s", optarg);
				newReaderConfig = optarg;
				break;

			case 'f':
				DebugLogA("main: pcscd set to foreground");
				setToForeground = 1;
				break;

			case 'd':
				if (strncmp(optarg, "syslog", PCSCLITE_MAX_COMSIZE) == 0) 
					DebugLogSetLogType(DEBUGLOG_SYSLOG_DEBUG);
				else if (strncmp(optarg, "stderr", PCSCLITE_MAX_COMSIZE) == 0) {
					DebugLogSetLogType(DEBUGLOG_STDERR_DEBUG);
					DebugLogA("main: debug messages to stderr");
					setToForeground = 1;
				} 
				else if (strncmp(optarg, "stdout", PCSCLITE_MAX_COMSIZE) == 0) {
					DebugLogSetLogType(DEBUGLOG_STDOUT_DEBUG);
					DebugLogA("main: debug messages to stdout");
					setToForeground = 1;
				}
				else
				{
					DebugLogB("unknown debug argument: %s", optarg);
					print_usage (argv[0]);
					return 1;
				}
				break;

			case 'h':
				print_usage (argv[0]);
				return 0;

			case 'v':
				print_version ();
				return 0;

			case 'a':
				DebugLogSetCategory(DEBUG_CATEGORY_APDU);
				break;

			default:
				print_usage (argv[0]);
				return 1;
		}

	}
	
	/*
	 * test the presence of /var/run/pcsc.pub
	 */

	rv = SYS_Stat(PCSCLITE_PUBSHM_FILE, &fStatBuf);

	if (rv == 0)
	{
#ifdef USE_RUN_PID

		// read the pid file to get the old pid and test if the old pcscd is
		// still running 

		FILE *f;
		// pids are only 15 bits but 4294967296 (32 bits in case of a new
		// system use it) is on 10 bytes
#define PID_ASCII_SIZE 11
		char pid_ascii[PID_ASCII_SIZE];
		int pid;

		if ((f = fopen(USE_RUN_PID, "rb")) != NULL)
		{
			fgets(pid_ascii, PID_ASCII_SIZE, f);
			fclose(f);

			pid = atoi(pid_ascii);

			if (kill(pid, 0) == 0)
			{
				DebugLogA("main: file " PCSCLITE_PUBSHM_FILE " already exists.");
				DebugLogB("Another pcscd (pid: %d) seems to be running.", pid);
				return 1;
			}
			else
				// the old pcscd is dead. make some cleanup
				clean_temp_files();
		}
		else
		{
			DebugLogA("main: file " PCSCLITE_PUBSHM_FILE " already exists.");
			DebugLogA("Maybe another pcscd is running?");
			DebugLogA("I can't read process pid from " USE_RUN_PID);
			DebugLogA("Remove " PCSCLITE_PUBSHM_FILE " and " PCSCLITE_CSOCK_NAME);
			DebugLogA("if pcscd is not running to clear this message.");
			return 1;
		}
#else
		DebugLogA("main: file " PCSCLITE_PUBSHM_FILE " already exists.");
		DebugLogA("Maybe another pcscd is running?");
		DebugLogA("Remove " PCSCLITE_PUBSHM_FILE " and " PCSCLITE_CSOCK_NAME);
		DebugLogA("if pcscd is not running to clear this message.");
		return 1;
#endif
	}

	/*
	 * If this is set to one the user has asked it not to fork 
	 */

	if (setToForeground == 0)
	{
		if (SYS_Daemon(0, 0))
			DebugLogB("main: SYS_Daemon() failed: %s", strerror(errno));
	}

	/*
	 * cleanly remove /tmp/pcsc when exiting 
	 */
	signal(SIGQUIT, signal_trap);
	signal(SIGTERM, signal_trap);
	signal(SIGINT, signal_trap);
	signal(SIGHUP, signal_trap);

#ifdef USE_RUN_PID
	/*
	 * Record our pid to make it easier
	 * to kill the correct pcscd
	 */
	{
		FILE *f;

		if ((f = fopen(USE_RUN_PID, "wb")) != NULL)
		{
			fprintf(f, "%u\n", (unsigned) getpid());
			fclose(f);
		}
	}
#endif

	/*
	 * If PCSCLITE_IPC_DIR does not exist then create it
	 */
	rv = SYS_Stat(PCSCLITE_IPC_DIR, &fStatBuf);
	if (rv < 0)
	{
		rv = SYS_Mkdir(PCSCLITE_IPC_DIR, S_ISVTX | S_IRWXO | S_IRWXG | S_IRWXU);
		if (rv != 0)
		{
			DebugLogB("main: cannot create " PCSCLITE_IPC_DIR ": %s",
				strerror(errno));
			return 1;
		}
	}

	/* cleanly remove /var/run/pcsc.* files when exiting */
	if (atexit(at_exit))
		DebugLogB("main: atexit() failed: %s", strerror(errno));

	/*
	 * Allocate memory for reader structures 
	 */
	RFAllocateReaderSpace(PCSCLITE_MAX_CONTEXTS);

	/*
	 * Grab the information from the reader.conf 
	 */
	if (newReaderConfig)
	{
		rv = DBUpdateReaders(newReaderConfig);
		if (rv != 0)
		{
			DebugLogB("main: invalid file %s", newReaderConfig);
			at_exit();
			return 1;
		}
	} else
	{
		rv = DBUpdateReaders(PCSCLITE_READER_CONFIG);

		if (rv == 1)
		{
			DebugLogA("main: warning: no reader.conf found");
			/*
			 * Token error in file 
			 */
		} else if (rv == -1)
		{
			at_exit();
			return 1;
		}
	}

	/*
	 * Set the default globals 
	 */
	g_rgSCardT0Pci.dwProtocol = SCARD_PROTOCOL_T0;
	g_rgSCardT1Pci.dwProtocol = SCARD_PROTOCOL_T1;
	g_rgSCardRawPci.dwProtocol = SCARD_PROTOCOL_RAW;

	DebugLogA("main: pcsc-lite daemon ready.");

	/*
	 * post initialistion 
	 */
	Init = 0;

	/*
	 * signal_trap() does just set a global variable used by the main loop 
	 */
	signal(SIGQUIT, signal_trap);
	signal(SIGTERM, signal_trap);
	signal(SIGINT, signal_trap);
	signal(SIGHUP, signal_trap);

	SVCServiceRunLoop();

	DebugLogA("pcscdaemon.c: main: SVCServiceRunLoop returned");
	return 1;
}

void at_exit(void)
{
	DebugLogA("at_exit: cleaning " PCSCLITE_IPC_DIR);

	clean_temp_files();

	SYS_Exit(1);
}

void clean_temp_files(void)
{
	int rv;

	rv = SYS_Unlink(PCSCLITE_PUBSHM_FILE);
	if (rv != 0)
		DebugLogB("main: Cannot unlink " PCSCLITE_PUBSHM_FILE ": %s",
			strerror(errno));

	rv = SYS_Unlink(PCSCLITE_CSOCK_NAME);
	if (rv != 0)
		DebugLogB("main: Cannot unlink " PCSCLITE_CSOCK_NAME ": %s",
			strerror(errno));

#ifdef USE_RUN_PID
	rv = SYS_Unlink(USE_RUN_PID);
	if (rv != 0)
		DebugLogB("main: Cannot unlink " USE_RUN_PID ": %s",
			strerror(errno));
#endif
}

void signal_trap(int sig)
{
	// the signal handler is called several times for the same Ctrl-C
	if (AraKiri == 0)
	{
		DebugLogA("Preparing for suicide");
		AraKiri = 1;

		// if still in the init/loading phase the AraKiri will not be
		// seen by the main event loop
		if (Init)
		{
			DebugLogA("Suicide during init");
			at_exit();
		}
	}
}

void print_version (void)
{
	printf("%s version %s.\n",  PACKAGE, VERSION);
	printf("Copyright (C) 1999-2002 by David Corcoran <corcoran@linuxnet.com>.\n");
	printf("Copyright (C) 2001-2003 by Ludovic Rousseau <ludovic.rouseau.free.fr>.\n");
	printf("Report bugs to <sclinux@linuxnet.com>.\n");
}

void print_usage (char const * const progname)
{
	printf("Usage: %s [-c file] [-f] [-d output] [-v] [-h]\n", progname);
	printf("Options:\n");
#ifdef HAVE_GETOPT_LONG
	printf("  -a, --apdu		log APDU commands and results\n");
	printf("  -c, --config		path to reader.conf\n");
	printf("  -f, --foreground	run in foreground (no daemon)\n");
	printf("  -d, --debug output	display debug messages. Output may be:\n"); 
	printf("			\"stdout\" (imply -f), \"stderr\" (imply -f),\n");
	printf("			or \"syslog\"\n");
	printf("  -h, --help		display usage information\n");
	printf("  -v, --version		display the program version number\n");
#else
	printf("  -a    log APDU commands and results\n");
	printf("  -c 	path to reader.conf\n");
	printf("  -f 	run in foreground (no daemon)\n");
	printf("  -d 	display debug messages. Output may be:\n"); 
	printf("	stdout (imply -f), stderr (imply -f),\n");
	printf("	or syslog\n");
	printf("  -h 	display usage information\n");
	printf("  -v 	display the program version number\n");
#endif
}
