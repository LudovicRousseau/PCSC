Some internal docs
==================

This file is not complete and may even not be exact. I (Ludovic
Rousseau) am not the author of pcsc-lite so I may have missed points.

The documentation correspond to pcsc-lite-1.0.2 compiled for GNU/Linux.
Information may be wrong for other pcsc-lite versions.


History:
--------
v 1.0, Jan 6 2002


compositions:
-------------
pcscd (daemon)
 libpcsclite-core.la
 pcscdaemon.c
 winscard_msg.c
 winscard_svc.c

libpcsclite-core.la
 atrhandler.c
 bundleparser.c
 configfile.c
 debuglog.c
 dyn_unix.c (this file is OS dependant)
 eventhandler.c
 hotplug_generic.c
 ifdwrapper.c
 prothandler.c
 readerfactory.c
 sys_unix.c (this file is OS dependant)
 thread_unix.c (this file is OS dependant)
 winscard.c
 winscard_msg.c

libpcsclite.la (client library)
 debuglog.c
 sys_unix.c
 winscard_clnt.c
 winscard_msg.c


Daemon global variables
-----------------------
readerfactory.c
 static PREADER_CONTEXT sContexts[PCSCLITE_MAX_CONTEXTS];
 static DWORD *dwNumContexts = 0;

*dwNumContexts is the number of Context
sContexts[] contains the Contexts

Why dwNumContexts is a PDWORD (pointer) and not just a DWORD? no idea.


eventhandler.c
 static PREADER_STATES readerStates[PCSCLITE_MAX_CONTEXTS];



Inter-thread communication
--------------------------

- to kill a context

    /* Set the thread to 0 to exit thread */
	rContext->dwLockId = 0xFFFF;



-- 
Ludovic Rousseau <ludovic.rouseau@free.fr>

