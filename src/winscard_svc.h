/*
 * This demarshalls functions over the message queue and
 * keeps track of clients and their handles.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *
 * $Id$
 */

#ifndef __winscard_svc_h__
#define __winscard_svc_h__

#ifdef __cplusplus
extern "C"
{
#endif
	LONG ContextsInitialize();
	LONG CreateContextThread(PDWORD);
#ifdef __cplusplus
}
#endif

#endif
