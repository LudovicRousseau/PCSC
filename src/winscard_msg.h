/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : winscard_msg.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 03/30/01
	    License: Copyright (C) 2001 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This defines some structures and defines to
	             be used over the transport layer.

********************************************************************/

#ifndef __winscard_msg_h__
#define __winscard_msg_h__

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct rxSharedSegment
	{
		unsigned int mtype;
		unsigned int user_id;
		unsigned int group_id;
		unsigned int command;
		unsigned int request_id;
		time_t date;
		unsigned char key[PCSCLITE_MSG_KEY_LEN];
		unsigned char data[PCSCLITE_MAX_MESSAGE_SIZE];
	}
	sharedSegmentMsg, *psharedSegmentMsg;

	enum pcsc_adm_commands
	{
		CMD_FUNCTION = 0xF1,
		CMD_FAILED = 0xF2,
		CMD_SERVER_DIED = 0xF3,
		CMD_CLIENT_DIED = 0xF4,
		CMD_READER_EVENT = 0xF5,
		CMD_SYN = 0xF6,
		CMD_ACK = 0xF7
	};

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
		SCARD_CANCEL_TRANSACTION = 0x0E
	};

	struct client_struct
	{
		SCARDCONTEXT hContext;
	};
	typedef struct client_struct client_struct;

	struct establish_struct
	{
		DWORD dwScope;
		SCARDCONTEXT phContext;
		LONG rv;
	};
	typedef struct establish_struct establish_struct;

	struct release_struct
	{
		SCARDCONTEXT hContext;
		LONG rv;
	};
	typedef struct release_struct release_struct;

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

	struct disconnect_struct
	{
		SCARDHANDLE hCard;
		DWORD dwDisposition;
		LONG rv;
	};
	typedef struct disconnect_struct disconnect_struct;

	struct begin_struct
	{
		SCARDHANDLE hCard;
		LONG rv;
	};
	typedef struct begin_struct begin_struct;

	struct end_struct
	{
		SCARDHANDLE hCard;
		DWORD dwDisposition;
		LONG rv;
	};
	typedef struct end_struct end_struct;

	struct cancel_struct
	{
		SCARDHANDLE hCard;
		LONG rv;
	};
	typedef struct cancel_struct cancel_struct;

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

	/*
	 * Now some function definitions 
	 */

	int SHMClientRead(psharedSegmentMsg, int);
	int SHMClientSetupSession(int);
	int SHMClientCloseSession();
	int SHMInitializeCommonConnect();
	int SHMInitializeCommonSegment();
	int SHMProcessCommonChannelRequest();
	int SHMProcessEvents(psharedSegmentMsg, int);
	int SHMMessageSend(psharedSegmentMsg, int, int);
	int SHMMessageReceive(psharedSegmentMsg, int, int);
	int WrapSHMWrite(unsigned int, unsigned int, unsigned int,
		unsigned int, void *);
	int WrapSHMWriteCommon(unsigned int, unsigned qint, unsigned int,
		unsigned int, void *);
	void SHMCleanupSharedSegment(int, char *);

#ifdef __cplusplus
}
#endif

#endif
