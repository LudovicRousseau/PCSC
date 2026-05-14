/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2023
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief This keeps track of a list of currently available reader structures.
 */

#ifndef __readerfactory_h__
#define __readerfactory_h__


#include "ifdhandler.h"
#include "pcscd.h"
#include "readers.h"

	typedef struct
	{
		char *pcFriendlyname;	/**< FRIENDLYNAME */
		char *pcDevicename;		/**< DEVICENAME */
		char *pcLibpath;		/**< LIBPATH */
		int channelId;		/**< CHANNELID */
	} SerialReader;

	struct RdrCliHandles
	{
		SCARDHANDLE hCard;		/**< hCard for this connection */
		_Atomic DWORD dwEventStatus;	/**< Recent event that must be sent */
	};

	typedef struct RdrCliHandles RDR_CLIHANDLES;

	LONG _RefReader(READER_CONTEXT * sReader);
	LONG _UnrefReader(READER_CONTEXT * sReader);

#define REF_READER(reader) { LONG rv; Log2(PCSC_LOG_DEBUG, "RefReader() count was: %d", reader->reference); rv = _RefReader(reader); if (rv != SCARD_S_SUCCESS) return rv; }
#define UNREF_READER(reader) {Log2(PCSC_LOG_DEBUG, "UnrefReader() count was: %d", reader->reference); _UnrefReader(reader);}

	LONG RFAllocateReaderSpace(unsigned int);
	LONG RFAddReader(const char *, int, const char *, const char *);
	LONG RFRemoveReader(const char *, int, int);
	LONG RFSetReaderName(READER_CONTEXT *, const char *, const char *, int);
	LONG RFReaderInfo(const char *, /*@out@*/ struct ReaderContext **);
	LONG RFReaderInfoById(SCARDHANDLE, /*@out@*/ struct ReaderContext **);
	LONG RFCheckSharing(SCARDHANDLE, READER_CONTEXT *);
	LONG RFLockSharing(SCARDHANDLE, READER_CONTEXT *);
	LONG RFUnlockSharing(SCARDHANDLE, READER_CONTEXT *);
	LONG RFUnlockAllSharing(SCARDHANDLE, READER_CONTEXT *);
	LONG RFLoadReader(READER_CONTEXT *);
	LONG RFBindFunctions(READER_CONTEXT *);
	LONG RFUnBindFunctions(READER_CONTEXT *);
	LONG RFUnloadReader(READER_CONTEXT *);
	LONG RFInitializeReader(READER_CONTEXT *);
	void RFUnInitializeReader(READER_CONTEXT *);
	SCARDHANDLE RFCreateReaderHandle(READER_CONTEXT *);
	LONG RFAddReaderHandle(READER_CONTEXT *, SCARDHANDLE);
	LONG RFRemoveReaderHandle(READER_CONTEXT *, SCARDHANDLE);
	void RFSetReaderEventState(READER_CONTEXT *, DWORD);
	LONG RFCheckReaderEventState(READER_CONTEXT *, SCARDHANDLE);
	LONG RFClearReaderEventState(READER_CONTEXT *, SCARDHANDLE);
	LONG RFCheckReaderStatus(READER_CONTEXT *);
	void RFCleanupReaders(void);
	void RFWaitForReaderInit(void);
	int RFStartSerialReaders(const char *readerconf);
	void RFReCheckReaderConf(void);
	int RFGetPowerState(READER_CONTEXT *);
	void RFSetPowerState(READER_CONTEXT *, int value);

#define REMOVE_READER_NO_FLAG 0
#define REMOVE_READER_FLAG_REMOVED 1

#endif
