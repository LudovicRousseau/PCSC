/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2002-2010
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This demarshalls functions over the message queue and
 * keeps track of clients and their handles.
 */

#ifndef __winscard_svc_h__
#define __winscard_svc_h__

#ifdef __cplusplus
extern "C"
{
#endif
	LONG ContextsInitialize(int, int);
	void ContextsDeinitialize(void);
	LONG CreateContextThread(uint32_t *);
	LONG MSGSignalClient(uint32_t filedes, LONG rv);
#ifdef __cplusplus
}
#endif

#endif
