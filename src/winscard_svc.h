/*
 * This demarshalls functions over the message queue and
 * keeps track of clients and their handles.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id$
 */

#ifndef __winscard_svc_h__
#define __winscard_svc_h__

#ifdef __cplusplus
extern "C"
{
#endif

	LONG MSGFunctionDemarshall(psharedSegmentMsg);
	LONG MSGAddContext(SCARDCONTEXT, DWORD);
	LONG MSGRemoveContext(SCARDCONTEXT, DWORD);
	LONG MSGAddHandle(SCARDCONTEXT, DWORD, SCARDHANDLE);
	LONG MSGRemoveHandle(SCARDCONTEXT, DWORD, SCARDHANDLE);
	LONG MSGCleanupClient(psharedSegmentMsg);

#ifdef __cplusplus
}
#endif

#endif
