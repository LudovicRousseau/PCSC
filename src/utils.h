/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2006-2007
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id: pcscdaemon.c 2377 2007-02-05 13:13:56Z rousseau $
 */

#ifndef __utils_h__
#define __utils_h__

#include <sys/types.h>
#include "wintypes.h"
#include "readerfactory.h"

#define PID_ASCII_SIZE 11
pid_t GetDaemonPid(void);
int SendHotplugSignal(void);

/* defined in winscard_clnt.c */
LONG SCardCheckDaemonAvailability(void);

int StatSynchronize(/*@null@*/ struct pubReaderStatesList *readerState);

int CheckForOpenCT(void);

#endif

