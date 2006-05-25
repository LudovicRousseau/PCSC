/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This defines some structures and \#defines to be used over
 * the transport layer.
 */

#ifndef __winscard_msg_h__
#define __winscard_msg_h__

/** Major version of the current message protocol */
#define PROTOCOL_VERSION_MAJOR 2
/** Minor version of the current message protocol */
#define PROTOCOL_VERSION_MINOR 1

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief General structure for client/serve message data exchange.
	 *
	 * It is used in the calls of \c SHMMessageSend and \c SHMMessageReceive.
	 * The field \c data is interpreted according to the values of the fields
	 * \c mtype and \c command. The possible structs the \c data field can
	 * represent are: \c version_struct \c client_struct \c establish_struct
	 * \c release_struct \c connect_struct \c reconnect_struct
	 * \c disconnect_struct \c begin_struct \c end_struct \c cancel_struct
	 * \c status_struct \c transmit_struct \c control_struct \c getset_struct
	 */
	typedef struct rxSharedSegment
	{
		unsigned int mtype;		/** one of the \c pcsc_adm_commands */
		unsigned int user_id;
		unsigned int group_id;
		unsigned int command;	/** one of the \c pcsc_msg_commands */
		unsigned int dummy;	/* was request_id in pcsc-lite <= 1.2.0 */
		time_t date;
		unsigned char key[PCSCLITE_MSG_KEY_LEN];
		unsigned char data[PCSCLITE_MAX_MESSAGE_SIZE];
	}
	sharedSegmentMsg, *psharedSegmentMsg;

	/**
	 * Command types available to use in the field \c sharedSegmentMsg.mtype.
	 */
	enum pcsc_adm_commands
	{
		CMD_FUNCTION = 0xF1,
		CMD_FAILED = 0xF2,
		CMD_SERVER_DIED = 0xF3,
		CMD_CLIENT_DIED = 0xF4,
		CMD_READER_EVENT = 0xF5,
		CMD_SYN = 0xF6,
		CMD_ACK = 0xF7,
		CMD_VERSION = 0xF8
	};

	/**
	 * @brief Commands available to use in the field \c sharedSegmentMsg.command.
	 */
	enum pcsc_msg_commands
	{
		SCARD_ESTABLISH_CONTEXT = 0x01,
		SCARD_RELEASE_CONTEXT = 0x02,
		SCARD_LIST_READERS = 0x03,
		SCARD_CONNECT = 0x04,
		SCARD_RECONNECT = 0x05,
		SCARD_DISCONNECT = 0x06,
		SCARD_BEGIN_TRANSACTION = 0x07,
		SCARD_END_TRANSACTION = 0x08,
		SCARD_TRANSMIT = 0x09,
		SCARD_CONTROL = 0x0A,
		SCARD_STATUS = 0x0B,
		SCARD_GET_STATUS_CHANGE = 0x0C,
		SCARD_CANCEL = 0x0D,
		SCARD_CANCEL_TRANSACTION = 0x0E,
		SCARD_GET_ATTRIB = 0x0F,
		SCARD_SET_ATTRIB = 0x10,
		SCARD_TRANSMIT_EXTENDED = 0x11
	};

	/**
	 * @brief Information transmitted in \c CMD_VERSION Messages.
	 */
	struct version_struct
	{
		int major;
		int minor;
		LONG rv;
	};
	typedef struct version_struct version_struct;

	struct client_struct
	{
		SCARDCONTEXT hContext;
	};
	typedef struct client_struct client_struct;

	/**
	 * @brief Information contained in \c SCARD_ESTABLISH_CONTEXT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct establish_struct
	{
		DWORD dwScope;
		SCARDCONTEXT phContext;
		LONG rv;
	};
	typedef struct establish_struct establish_struct;

	/**
	 * @brief Information contained in \c SCARD_RELEASE_CONTEXT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct release_struct
	{
		SCARDCONTEXT hContext;
		LONG rv;
	};
	typedef struct release_struct release_struct;

	/**
	 * @brief contained in \c SCARD_CONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct connect_struct
	{
		SCARDCONTEXT hContext;
		char szReader[MAX_READERNAME];
		DWORD dwShareMode;
		DWORD dwPreferredProtocols;
		SCARDHANDLE phCard;
		DWORD pdwActiveProtocol;
		LONG rv;
	};
	typedef struct connect_struct connect_struct;

	/**
	 * @brief contained in \c SCARD_RECONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct reconnect_struct
	{
		SCARDHANDLE hCard;
		DWORD dwShareMode;
		DWORD dwPreferredProtocols;
		DWORD dwInitialization;
		DWORD pdwActiveProtocol;
		LONG rv;
	};
	typedef struct reconnect_struct reconnect_struct;

	/**
	 * @brief contained in \c SCARD_DISCONNECT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct disconnect_struct
	{
		SCARDHANDLE hCard;
		DWORD dwDisposition;
		LONG rv;
	};
	typedef struct disconnect_struct disconnect_struct;

	/**
	 * @brief contained in \c SCARD_BEGIN_TRANSACTION Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct begin_struct
	{
		SCARDHANDLE hCard;
		LONG rv;
	};
	typedef struct begin_struct begin_struct;

	/**
	 * @brief contained in \c SCARD_END_TRANSACTION Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct end_struct
	{
		SCARDHANDLE hCard;
		DWORD dwDisposition;
		LONG rv;
	};
	typedef struct end_struct end_struct;

	/**
	 * @brief contained in \c SCARD_CANCEL Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct cancel_struct
	{
		SCARDHANDLE hCard;
		LONG rv;
	};
	typedef struct cancel_struct cancel_struct;

	/**
	 * @brief contained in \c SCARD_STATUS Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct status_struct
	{
		SCARDHANDLE hCard;
		char mszReaderNames[MAX_READERNAME];
		DWORD pcchReaderLen;
		DWORD pdwState;
		DWORD pdwProtocol;
		UCHAR pbAtr[MAX_ATR_SIZE];
		DWORD pcbAtrLen;
		LONG rv;
	};
	typedef struct status_struct status_struct;

	/**
	 * @brief contained in \c SCARD_TRANSMIT Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct transmit_struct
	{
		SCARDHANDLE hCard;
		SCARD_IO_REQUEST pioSendPci;
		UCHAR pbSendBuffer[MAX_BUFFER_SIZE];
		DWORD cbSendLength;
		SCARD_IO_REQUEST pioRecvPci;
		BYTE pbRecvBuffer[MAX_BUFFER_SIZE];
		DWORD pcbRecvLength;
		LONG rv;
	};
	typedef struct transmit_struct transmit_struct;

	/**
	 * @brief contained in \c SCARD_TRANSMIT_EXTENDED Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct transmit_struct_extended
	{
		SCARDHANDLE hCard;
		SCARD_IO_REQUEST pioSendPci;
		DWORD cbSendLength;
		SCARD_IO_REQUEST pioRecvPci;
		DWORD pcbRecvLength;
		LONG rv;
		size_t size;
		BYTE data[0];
	};
	typedef struct transmit_struct_extended transmit_struct_extended;

	/**
	 * @brief contained in \c SCARD_CONTROL Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct control_struct
	{
		SCARDHANDLE hCard;
		DWORD dwControlCode;
		UCHAR pbSendBuffer[MAX_BUFFER_SIZE];
		DWORD cbSendLength;
		UCHAR pbRecvBuffer[MAX_BUFFER_SIZE];
		DWORD cbRecvLength;
		DWORD dwBytesReturned;
		LONG rv;
	};
	typedef struct control_struct control_struct;

	/**
	 * @brief contained in \c SCARD_GET_ATTRIB and \c  Messages.
	 *
	 * These data are passed throw the field \c sharedSegmentMsg.data.
	 */
	struct getset_struct
	{
		SCARDHANDLE hCard;
		DWORD dwAttrId;
		UCHAR pbAttr[MAX_BUFFER_SIZE];
		DWORD cbAttrLen;
		LONG rv;
	};
	typedef struct getset_struct getset_struct;

	/*
	 * Now some function definitions 
	 */

	int SHMClientRead(psharedSegmentMsg, DWORD, int);
	int SHMClientSetupSession(PDWORD);
	int SHMClientCloseSession(DWORD);
	int SHMInitializeCommonSegment(void);
	int SHMProcessEventsContext(PDWORD, psharedSegmentMsg, int);
	int SHMProcessEventsServer(PDWORD, int);
	int SHMMessageSend(void *buffer, size_t buffer_size, int filedes,
		int blockAmount);
	int SHMMessageReceive(void *buffer, size_t buffer_size,
		int filedes, int blockAmount);
	int WrapSHMWrite(unsigned int command, DWORD dwClientID, unsigned int size,
		unsigned int blockAmount, void *data);
	void SHMCleanupSharedSegment(int, char *);

#ifdef __cplusplus
}
#endif

#endif
