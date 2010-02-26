/*
 * Header file for reading lexical config files.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

#ifndef __configfile_h__
#define __configfile_h__

#ifdef __cplusplus
extern "C"
{
#endif

	int DBGetReaderList(const char *readerconf,
		/*@out@*/ SerialReader **caller_reader_list);

	int DBGetReaderListDir(const char *readerconf_dir,
		/*@out@*/ SerialReader **caller_reader_list);

#ifdef __cplusplus
}
#endif

#endif							/* __configfile_h__ */
