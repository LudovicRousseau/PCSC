/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2002-2010
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
 * @brief This defines some structures and \#defines to be used over
 * the transport layer.
 */

#ifndef __winscard_msg_h__
#define __winscard_msg_h__

#include <stdint.h>

#include "pcsclite.h"
#include "wintypes.h"

/** Major version of the current message protocol */
#define PROTOCOL_VERSION_MAJOR 4
/** Minor version of the current message protocol */
#define PROTOCOL_VERSION_MINOR 4

	/**
	 * @brief Information transmitted in \ref CMD_VERSION Messages.
	 */
	struct version_struct
	{
		int32_t major;	/**< IPC major \ref PROTOCOL_VERSION_MAJOR */
		int32_t minor;	/**< IPC minor \ref PROTOCOL_VERSION_MINOR */
		uint32_t rv;
	};

	/**
	 * @brief header structure for client/server message data exchange.
	 */
	struct rxHeader
	{
		uint32_t size;		/**< size of the message excluding this header */
		uint32_t command;	/**< one of the \c pcsc_msg_commands */
	};

	/**
	 * @brief Commands available to use in the field \c sharedSegmentMsg.command.
	 */
	enum pcsc_msg_commands
	{
		CMD_ENUM_FIRST,
		SCARD_ESTABLISH_CONTEXT = 0x01,	/**< used by SCardEstablishContext() */
		SCARD_RELEASE_CONTEXT = 0x02,	/**< used by SCardReleaseContext() */
		SCARD_LIST_READERS = 0x03,		/**< used by SCardListReaders() */
		SCARD_CONNECT = 0x04,			/**< used by SCardConnect() */
		SCARD_RECONNECT = 0x05,			/**< used by SCardReconnect() */
		SCARD_DISCONNECT = 0x06,		/**< used by SCardDisconnect() */
		SCARD_BEGIN_TRANSACTION = 0x07,	/**< used by SCardBeginTransaction() */
		SCARD_END_TRANSACTION = 0x08,	/**< used by SCardEndTransaction() */
		SCARD_TRANSMIT = 0x09,			/**< used by SCardTransmit() */
		SCARD_CONTROL = 0x0A,			/**< used by SCardControl() */
		SCARD_STATUS = 0x0B,			/**< used by SCardStatus() */
		SCARD_GET_STATUS_CHANGE = 0x0C,	/**< not used */
		SCARD_CANCEL = 0x0D,			/**< used by SCardCancel() */
		SCARD_CANCEL_TRANSACTION = 0x0E,/**< not used */
		SCARD_GET_ATTRIB = 0x0F,		/**< used by SCardGetAttrib() */
		SCARD_SET_ATTRIB = 0x10,		/**< used by SCardSetAttrib() */
		CMD_VERSION = 0x11,				/**< get the client/server protocol version */
		CMD_GET_READERS_STATE = 0x12,	/**< get the readers state */
		CMD_WAIT_READER_STATE_CHANGE = 0x13,	/**< wait for a reader state change */
		CMD_STOP_WAITING_READER_STATE_CHANGE = 0x14,	/**< stop waiting for a reader state change */
		CMD_ENUM_LAST
	};

	struct client_struct
	{
		uint32_t hContext;
	};

	/**
	 * @brief Information contained in \ref CMD_WAIT_READER_STATE_CHANGE Messages.
	 */
	struct wait_reader_state_change
	{
		uint32_t timeOut;	/**< timeout in ms */
		uint32_t rv;
	};

	/**
	 * @brief Information contained in \ref SCARD_ESTABLISH_CONTEXT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct establish_struct
	{
		uint32_t dwScope;
		uint32_t hContext;
		uint32_t rv;
	};

	/**
	 * @brief Information contained in \ref SCARD_RELEASE_CONTEXT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct release_struct
	{
		uint32_t hContext;
		uint32_t rv;
	};

	/**
	 * @brief contained in \ref SCARD_CONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct connect_struct
	{
		uint32_t hContext;
		char szReader[MAX_READERNAME];
		uint32_t dwShareMode;
		uint32_t dwPreferredProtocols;
		int32_t hCard;
		uint32_t dwActiveProtocol;
		uint32_t rv;
	};

	/**
	 * @brief contained in \ref SCARD_RECONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct reconnect_struct
	{
		int32_t hCard;
		uint32_t dwShareMode;
		uint32_t dwPreferredProtocols;
		uint32_t dwInitialization;
		uint32_t dwActiveProtocol;
		uint32_t rv;
	};

	/**
	 * @brief contained in \ref SCARD_DISCONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct disconnect_struct
	{
		int32_t hCard;
		uint32_t dwDisposition;
		uint32_t rv;
	};

	/**
	 * @brief contained in \ref SCARD_BEGIN_TRANSACTION Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct begin_struct
	{
		int32_t hCard;
		uint32_t rv;
	};

	/**
	 * @brief contained in \ref SCARD_END_TRANSACTION Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct end_struct
	{
		int32_t hCard;
		uint32_t dwDisposition;
		uint32_t rv;
	};

	/**
	 * @brief contained in \ref SCARD_CANCEL Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct cancel_struct
	{
		int32_t hContext;
		uint32_t rv;
	};

	/**
	 * @brief contained in \ref SCARD_STATUS Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct status_struct
	{
		int32_t hCard;
		uint32_t rv;
	};

	/**
	 * @brief contained in \ref SCARD_TRANSMIT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct transmit_struct
	{
		int32_t hCard;
		uint32_t ioSendPciProtocol;
		uint32_t ioSendPciLength;
		uint32_t cbSendLength;
		uint32_t ioRecvPciProtocol;
		uint32_t ioRecvPciLength;
		uint32_t pcbRecvLength;
		uint32_t rv;
	};

	/**
	 * @brief contained in \ref SCARD_CONTROL Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct control_struct
	{
		int32_t hCard;
		uint32_t dwControlCode;
		uint32_t cbSendLength;
		uint32_t cbRecvLength;
		uint32_t dwBytesReturned;
		uint32_t rv;
	};

	/**
	 * @brief contained in \ref SCARD_GET_ATTRIB and \c  Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct getset_struct
	{
		int32_t hCard;
		uint32_t dwAttrId;
		uint8_t pbAttr[MAX_BUFFER_SIZE];
		uint32_t cbAttrLen;
		uint32_t rv;
	};

	/*
	 * Now some function definitions
	 */

#ifdef PCSCD
	int32_t InitializeSocket(void);
	int32_t ListenExistingSocket(int fd);
	int32_t ProcessEventsServer(/*@out@*/ uint32_t *);
#else
	char *getSocketName(void);
	int32_t ClientSetupSession(uint32_t *);
	void ClientCloseSession(uint32_t);
	LONG MessageReceiveTimeout(uint32_t command, /*@out@*/ void *buffer,
		uint64_t buffer_size, int32_t filedes, long timeOut);
	LONG MessageSendWithHeader(uint32_t command, uint32_t dwClientID,
		uint64_t size, void *data);
#endif
	LONG MessageSend(void *buffer, uint64_t buffer_size, int32_t filedes);
	LONG MessageReceive(/*@out@*/ void *buffer, uint64_t buffer_size,
		int32_t filedes);

#endif
