Some internal docs
==================

This file is not complete and may even not be exact. I (Ludovic
Rousseau) am not the author of pcsc-lite so I may have missed points.

The documentation corresponds to post pcsc-lite-1.2.0 compiled for
GNU/Linux.  Information may be wrong for other pcsc-lite versions.


History:
--------
v 1.0, Jan 6 2002
v 1.1, Jun 2004


compositions:
-------------
pcscd (daemon)
 atrhandler.c
 configfile.l
 debuglog.c
 dyn_hpux.c (this file is OS dependant)
 dyn_macosx.c (this file is OS dependant)
 dyn_unix.c (this file is OS dependant)
 eventhandler.c
 hotplug_generic.c
 hotplug_libusb.c
 hotplug_linux.c (this file is OS dependant)
 hotplug_macosx.c (this file is OS dependant)
 ifdwrapper.c
 pcscdaemon.c
 powermgt_generic.c
 powermgt_macosx.c (this file is OS dependant)
 prothandler.c
 readerfactory.c
 sys_unix.c (this file is OS dependant)
 thread_unix.c (this file is OS dependant)
 tokenparser.l
 winscard.c
 winscard_msg.c
 winscard_msg_srv.c
 winscard_svc.c

libpcsclite.la (client library)
 debug.c
 dyn_hpux.c
 dyn_macosx.c
 dyn_unix.c
 error.c
 sys_unix.c
 thread_unix.c
 winscard_clnt.c or winscard_scf.c
 winscard_msg.c


PC/SC Lite Concepts:
--------------------
These concepts will be available after the next release of pcsc-lite-1.2.0.
I (Damien Sauveron) will try to explain the following concepts:

Maximum applications
	PCSCLITE_MAX_APPLICATIONS

Maximum contexts by application
	PCSCLITE_MAX_APPLICATION_CONTEXTS

Maximum of applications contexts that PC/SC Ressources Manager can accept
	PCSCLITE_MAX_APPLICATIONS_CONTEXTS
 	= PCSCLITE_MAX_APPLICATIONS * PCSCLITE_MAX_APPLICATION_CONTEXTS

Maximum channels on a reader context
	PCSCLITE_MAX_READER_CONTEXT_CHANNELS

Maximum channels on an application context
	PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS

Maximum readers context (a slot is counted as a reader)
	PCSCLITE_MAX_READERS_CONTEXTS

First imagine:
- 3 PC/SC daemons started on 3 different hosts (it is possible to
  connect to PC/SC daemons over the network with SCardEstablishContext
  even if it is not yet implemented in PC/SC Lite): PC/SC D1, PC/SC D2,
  PC/SC D3
- 4 applications: App A, App B, App C, App D
- 2 readers R1 and R2 but where R2 has 2 slots that we call R2S1 and
  R2S2

On the following figure the (1), (2), ... (7) are some APPLICATION_CONTEXTS.
Also App A has 3 APPLICATION_CONTEXTS. Each of them is created by
SCardEstablishContext.

PC/SC D1 handles 5 APPLICATIONS_CONTEXTS.

PC/SC D1 also handles 3 READERS_CONTEXTS. These contexts are created for
example by the plug of the readers.

The maximum of applications contexts that PC/SC Ressources Manager can
accept is thus PCSCLITE_MAX_APPLICATIONS *
PCSCLITE_MAX_APPLICATION_CONTEXTS.

On each of these contexts on the application side there are some
APPLICATION_CONTEXT_CHANNELS. They are created by SCardConnect.

And on each of these contexts on the reader side there are some
READER_CONTEXT_CHANNELS.


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

For simplify, there are 3 differents roles: Application, PC/SC Daemon
and IFDhandler/reader.

Between these role there are some contexts and on the top of them there
are the channels.


Daemon global variables:
------------------------
readerfactory.c
 static PREADER_CONTEXT sReadersContexts[PCSCLITE_MAX_READERS_CONTEXTS];
 static DWORD *dwNumReadersContexts = 0;

dwNumReadersContexts is the number of Readers Contexts
sReadersContexts[] contains the Readers Contexts


eventhandler.c
 static PREADER_STATE readerStates[PCSCLITE_MAX_READERS_CONTEXTS];



IPC communication between pcscd and libpcsclite?:
-------------------------------------------------

pcscd and lipcsclite communicates through a named socket.
With post 1.2.0 pcsc-lite versions the client (libpcsclite) and the
server (pcscd) exchange a protocol version. With 1.2.0 and before the
protocol is 0:0 (major:minor).

The SCardControl() API changed from
  LONG SCardControl(SCARDHANDLE hCard,
      LPCBYTE pbSendBuffer,
	  DWORD cbSendLength,
	  LPBYTE pbRecvBuffer,
	  LPDWORD pcbRecvLength);
to
  LONG SCardControl(SCARDHANDLE hCard,
      DWORD dwControlCode,      <-- new
	  LPCVOID lpInBuffer,
	  DWORD nInBufferSize,
	  LPVOID lpOutBuffer,
	  DWORD nOutBufferSize,
	  LPDWORD lpBytesReturned); <-- new

This change was made to map Windows API.

This change also has an impact on the ifd handler (smart card driver).
The IFDHandler v3.0 use for post 1.2.0 version uses the new
IFDHControl() API.

We can have:
- libpcsclite0, pcscd (<= 1.2.0) and ifdhandler 1.0 or 2.0
 => old SCardControl
- libpcsclite0, pcscd (> 1.2.0) and ifdhandler 1.0 or 2.0
 => old SCardControl
- libpcsclite1, pcscd (> 1.2.0) and ifdhandler 1.0 or 2.0
 => old SCardControl
- libpcsclite1, pcscd (> 1.2.0) and ifdhandler 3.0
 => new SCardControl
- libpcsclite1, pcscd (<= 1.2.0)
 => does not work


Memory structures
-----------------

pcscd side:

- pcscd open/creates a shared memory segment (EHInitializeEventStructures()
  in eventhandler.c)
- static PREADER_STATE readerStates[PCSCLITE_MAX_READERS_CONTEXTS]; is
  an array of pointers on READER_STATE. Each entry readerStates[i]
  points to a memory shared segment. It contains the state of each
  readers.

- reader contexts are also created and maintained
- static PREADER_CONTEXT sReadersContexts[PCSCLITE_MAX_READERS_CONTEXTS];
  is an array of pointers on READER_CONTEXT
- the structure is allocated by RFAllocateReaderSpace() in
  readerfactory.c
- each READER_CONTEXT contains a pointer to a READER_STATE for the
  context


libpcsclite side:

- the library open the shared memory segment (SCardEstablishContextTH()
  in winscard_clnt.c)
- each entry readerStates[i] gets a reference to the memory segment of
  the server.

The memory is READ ONLY on the library side.


Inter-thread communication:
---------------------------

- to kill a context

    /* Set the thread to 0 to exit thread */
	rContext->dwLockId = 0xFFFF;

-- 
Ludovic Rousseau <ludovic.rouseau@free.fr>

