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


PC/SC Lite Concepts
-------------------
These concepts will be available after the next release of pcsc-lite-1.2.0.
I (Damien Sauveron) will try to explain the following concepts:
#define PCSCLITE_MAX_APPLICATIONS         4	/* Maximum applications */
#define PCSCLITE_MAX_APPLICATION_CONTEXTS 16	/* Maximum contexts by application */
#define PCSCLITE_MAX_APPLICATIONS_CONTEXTS PCSCLITE_MAX_APPLICATIONS * PCSCLITE_MAX_APPLICATION_CONTEXTS
	/* Maximum of applications contexts that PC/SC Ressources Manager can accept */

#define PCSCLITE_MAX_READER_CONTEXT_CHANNELS      16	/* Maximum channels on a reader context */
#define PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS 16	/* Maximum channels on a client context */

#define PCSCLITE_MAX_READERS_CONTEXTS            256	/* Maximum readers context (a slot is count as a reader) */

First imagine:
- 3 PC/SC daemons started on 3 different hosts (it is possible to connect to PC/SC daemons over the network with SCardEstablishContext even if it is not yet implemented in PC/SC Lite): PC/SC D1, PC/SC D2, PC/SC D3
- 4 applications: App A, App B, App C, App D
- 2 readers R1 and R2 but where R2 has 2 slots that we call R2S1 and R2S2

On the following figure the (1), (2), ... (7) are some APPLICATION_CONTEXTS.
Also App A has 3 APPLICATION_CONTEXTS. Each of them is created by SCardEstablishContext.
PC/SC D1 handles 5 APPLICATIONS_CONTEXTS.
PC/SC D1 also handles 3 READERS_CONTEXTS. These contexts are created for example by the plug of the readers.
The maximum of applications contexts that PC/SC Ressources Manager can accept is thus PCSCLITE_MAX_APPLICATIONS * PCSCLITE_MAX_APPLICATION_CONTEXTS.

On each of these contexts on the application side there are some APPLICATION_CONTEXT_CHANNELS. They are created by SCardConnect.
And on each of these contexts on the reader side there are some READER_CONTEXT_CHANNELS.


PC/SC D2
        \ (3)           (1)              -- R1
         ------       --------          / 
               \     /        \        /
                App A          PC/SC D1---- R2S1
                     \        /  | | | \ 
                      --------   | | |  \ 
                        (2)      | | |   -- R2S2
PC/SC D3                         | | |
        \   (4)          (5)     | | |
         -------App B -----------/ | |
                         (6)       | |
                App C -------------/ |
                         (7)         |
                App D ---------------/

For simplify, there are 3 differents roles: Application, PC/SC Daemon and IFDhandler/reader.
Between these role there are some contexts and on the top of them there are the channels.


Daemon global variables
-----------------------
readerfactory.c
 static PREADER_CONTEXT sReadersContexts[PCSCLITE_MAX_READERS_CONTEXTS];
 static DWORD *dwNumReadersContexts = 0;

*dwNumReadersContexts is the number of Readers Contexts
sReadersContexts[] contains the Readers Contexts

Why dwNumContexts is a PDWORD (pointer) and not just a DWORD? no idea.


eventhandler.c
 static PREADER_STATES readerStates[PCSCLITE_MAX_READERS_CONTEXTS];



Inter-thread communication
--------------------------

- to kill a context

    /* Set the thread to 0 to exit thread */
	rContext->dwLockId = 0xFFFF;



-- 
Ludovic Rousseau <ludovic.rouseau@free.fr>

