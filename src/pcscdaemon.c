/******************************************************************

            Title  : pcscdaemon.c
            Package: PC/SC Lite
            Author : David Corcoran
            Date   : 10/24/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This is the main pcscd daemon.

********************************************************************/

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

#include "config.h"
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

#ifdef USE_DAEMON
#ifndef USE_SYSLOG
#error You must use '--enable-syslog' when also using '--enable-daemon' or you will not get any message
#endif
#endif

static char AraKiri = 0;
static char Init = 1;

/* Some internal functions */
void SVCServiceRunLoop();
void SVCClientCleanup( psharedSegmentMsg );
void at_exit(void);
void signal_trap(int);

PCSCLITE_MUTEX usbNotifierMutex;

/* Cleans up messages still on the queue
   when a client dies
*/
void SVCClientCleanup( psharedSegmentMsg msgStruct ) {
  /* May be implemented in future releases */
}

/* The Message Queue Listener function */
void SVCServiceRunLoop() {

  char errMessage[200];
  sharedSegmentMsg msgStruct;        
  int currHandle, rsp;  
    
  currHandle=0, rsp=0;


  /* Initialize the comm structure */
  rsp = SHMInitializeCommonSegment();

  if ( rsp == -1 ) {
    DebugLogA("SVCServiceRunLoop: Error initializing pcscd.");
    exit(-1);
  }

  /* Solaris sends a SIGALRM and it is annoying */
 
  signal(SIGALRM, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP, SIG_IGN);	/* needed for Solaris. The signal is sent when the
							   shell is existed */

  /* This function always returns zero */
  rsp = SYS_MutexInit(&usbNotifierMutex);

  /* Set up the search for USB/PCMCIA devices */
  HPSearchHotPluggables();

  while(1) {

    switch (SHMProcessEvents(&msgStruct, 0)) {

    case 0:
      if ( msgStruct.mtype == CMD_CLIENT_DIED ) {
	/* Clean up the dead client */
	SYS_MutexLock(&usbNotifierMutex);
	MSGCleanupClient( &msgStruct ); 
	SYS_MutexUnLock(&usbNotifierMutex);
	snprintf(errMessage, sizeof(errMessage), "%s%d%s",
		 "SVCServiceRun: Client ", msgStruct.request_id, " has disappeared." );
	DebugLogB("%s", errMessage);
      } else {
	continue;
      }

      break;

    case 1:
	if ( msgStruct.mtype == CMD_FUNCTION ) {
	  /* Command must be found */
	  SYS_MutexLock(&usbNotifierMutex);
	  MSGFunctionDemarshall( &msgStruct );
	  rsp = SHMMessageSend( &msgStruct, msgStruct.request_id, 0 );
	  SYS_MutexUnLock(&usbNotifierMutex);
	} else {
	  continue;
	}

	break;

    case -1:
	DebugLogA("SVCServiceRun: Error in ProcessEvents.");
	break;

    default:
      break;;
    }

	if (AraKiri)
		RFCleanupReaders();
  }
}

int main(int argc, char **argv) {
  
  int rv;
  char *newReaderConfig;

  rv = 0; newReaderConfig=0;

  /* test the version */
  if (strcmp(PCSCLITE_VERSION_NUMBER, VERSION) != 0)
  {
	  printf("BUILD ERROR: The release version number PCSCLITE_VERSION_NUMBER\n");
	  printf("  in pcsclite.h (%s) does not match the release version number\n", PCSCLITE_VERSION_NUMBER);
	  printf("  generated in config.h (%s) (see configure.in).\n", VERSION);

	  return 1;
  }
  
  /* Handle any command line arguments */
  if ( argc == 2 && (strcmp(argv[1], "-v") == 0) ) {
    printf("pcsc-lite version: %s <corcoran@linuxnet.com>\n",
           PCSCLITE_VERSION_NUMBER);
    return 0;
  } else if ( argc == 2 && (strcmp(argv[1], "-help") == 0) ) {
    printf("pcscd -v       - Display version and exit\n");
    printf("pcscd -c file  - New path to reader.conf\n");
    printf("pcscd -help    - This help menu\n");
    return 0;
  } else if ( argc == 3 && (strcmp(argv[1], "-c") == 0) ) {
    DebugLogB("main: Using new config file: %s", argv[2]);
    newReaderConfig = argv[2];
  } else if ( argc == 1 ) {
    /* All OK Here */
  } else {
    printf("pcsc-lite: Invalid arguments, try -help \n");
    return 1;
  }

  /* test the presence of /tmp/pcsc */
  {
	struct stat buf;

    rv = SYS_Stat(PCSCLITE_IPC_DIR, &buf);
	if (rv == 0)
	{
    	DebugLogA("main: directory " PCSCLITE_IPC_DIR " already exists."
			" Maybe another pcscd is running?");
		return 1;
	}

  }

#ifdef USE_DAEMON
  /* standard daemonizing actions */
#ifndef HAVE_DAEMON
  switch (SYS_Fork()) {
    case -1:
      return (-1);
    case 0:
      break;
    default:
      return (0);
  }
  close(0);
  close(1);
  close(2);
  chdir("/");
#else
  daemon( 0, 0 );
#endif
#endif

  /* cleanly remove /tmp/pcsc when exiting */
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
		FILE * f;
	   
		if ((f = fopen(USE_RUN_PID, "wb")) != NULL)
		{
			fprintf(f, "%u\n", (unsigned) getpid());
			fclose(f);
		}
	}
#endif

  /* Create the /tmp/pcsc directory and chmod it */
  rv = SYS_Mkdir(PCSCLITE_IPC_DIR, S_ISVTX | S_IRWXO | S_IRWXG | S_IRWXU );
  if ( rv != 0 ) {
    DebugLogB("main: Cannot create " PCSCLITE_IPC_DIR ": %s", strerror(errno));
	return 1;
  }

  rv = SYS_Chmod(PCSCLITE_IPC_DIR, S_ISVTX | S_IRWXO | S_IRWXG | S_IRWXU );
  if ( rv != 0 ) {
    DebugLogB("main: Cannot chmod " PCSCLITE_IPC_DIR ": %s", strerror(errno));
	return 1;
  }

  /* Allocate memory for reader structures     */
  RFAllocateReaderSpace( PCSCLITE_MAX_CONTEXTS );

  /* Grab the information from the reader.conf */    
  if ( newReaderConfig ) {
    DBUpdateReaders ( newReaderConfig );
  } else {
    DBUpdateReaders ( PCSCLITE_READER_CONFIG );
  }

  /* Set the default globals                   */
  g_rgSCardT0Pci.dwProtocol  = SCARD_PROTOCOL_T0;
  g_rgSCardT1Pci.dwProtocol  = SCARD_PROTOCOL_T1;  
  g_rgSCardRawPci.dwProtocol = SCARD_PROTOCOL_RAW;
  
  DebugLogA("main: PC/SC Lite Daemon Ready.");

  /* post initialistion */
  Init = 0;

  /* cleanly remove /tmp/pcsc when exiting */
  /* signal_trap() does just set a global variable used by the main loop */
  atexit(at_exit);
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
	int rv;

	DebugLogA("at_exit: cleaning " PCSCLITE_IPC_DIR);

	rv = SYS_Unlink(PCSCLITE_SHM_FILE);
	if ( rv != 0 )
    	DebugLogB("main: Cannot unlink " PCSCLITE_SHM_FILE ": %s",
			strerror(errno));

	rv = SYS_Unlink(PCSCLITE_PUBSHM_FILE);
	if ( rv != 0 )
    	DebugLogB("main: Cannot unlink " PCSCLITE_PUBSHM_FILE ": %s",
			strerror(errno));

	rv = SYS_Unlink(PCSCLITE_CSOCK_NAME);
	if ( rv != 0 )
    	DebugLogB("main: Cannot unlink " PCSCLITE_CSOCK_NAME ": %s",
			strerror(errno));

	rv = SYS_Rmdir(PCSCLITE_IPC_DIR);
	if ( rv != 0 )
    	DebugLogB("main: Cannot rmdir " PCSCLITE_IPC_DIR ": %s",
			strerror(errno));

#ifdef USE_RUN_PID
	rv = SYS_Unlink(USE_RUN_PID);
	if ( rv != 0 )
		DebugLogB("main: Cannot unlink " USE_RUN_PID ": %s", strerror(errno));
#endif

	SYS_Exit(1);
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

