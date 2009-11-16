/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2002-2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This demarshalls functions over the message queue and keeps
 * track of clients and their handles.
 *
 * Each Client message is deald by creating a thread (\c CreateContextThread).
 * The thread establishes reands and demarshalls the message and calls the
 * appropriate function to threat it.
 */

#include "config.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "pcscd.h"
#include "winscard.h"
#include "debuglog.h"
#include "winscard_msg.h"
#include "winscard_svc.h"
#include "sys_generic.h"
#include "thread_generic.h"
#include "readerfactory.h"
#include "eventhandler.h"

/**
 * @brief Represents an Application Context on the Server side.
 *
 * An Application Context contains Channels (\c hCard).
 */
static struct _psContext
{
	uint32_t hContext;
	uint32_t hCard[PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS];
	uint32_t dwClientID;			/**< Connection ID used to reference the Client. */
	PCSCLITE_THREAD_T pthThread;		/**< Event polling thread's ID */
	int protocol_major, protocol_minor;	/**< Protocol number agreed between client and server*/
} psContext[PCSCLITE_MAX_APPLICATIONS_CONTEXTS];

static LONG MSGCheckHandleAssociation(SCARDHANDLE, DWORD);
static LONG MSGAddContext(SCARDCONTEXT, DWORD);
static LONG MSGRemoveContext(SCARDCONTEXT, DWORD);
static LONG MSGAddHandle(SCARDCONTEXT, SCARDHANDLE, DWORD);
static LONG MSGRemoveHandle(SCARDHANDLE, DWORD);
static LONG MSGCleanupClient(DWORD);

static void ContextThread(LPVOID pdwIndex);

extern READER_STATE readerStates[PCSCLITE_MAX_READERS_CONTEXTS];

LONG ContextsInitialize(void)
{
	memset(psContext, 0, sizeof(struct _psContext)*PCSCLITE_MAX_APPLICATIONS_CONTEXTS);
	return 1;
}

/**
 * @brief Creates threads to handle messages received from Clients.
 *
 * @param[in] pdwClientID Connection ID used to reference the Client.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Success.
 * @retval SCARD_F_INTERNAL_ERROR Exceded the maximum number of simultaneous Application Contexts.
 * @retval SCARD_E_NO_MEMORY Error creating the Context Thread.
 */
LONG CreateContextThread(uint32_t *pdwClientID)
{
	long i;
	int rv;

	for (i = 0; i < PCSCLITE_MAX_APPLICATIONS_CONTEXTS; i++)
	{
		if (psContext[i].dwClientID == 0)
		{
			psContext[i].dwClientID = *pdwClientID;
			*pdwClientID = 0;
			break;
		}
	}

	if (i == PCSCLITE_MAX_APPLICATIONS_CONTEXTS)
	{
		Log2(PCSC_LOG_CRITICAL, "No more context available (max: %d)",
			PCSCLITE_MAX_APPLICATIONS_CONTEXTS);
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = SYS_ThreadCreate(&psContext[i].pthThread, THREAD_ATTR_DETACHED,
		(PCSCLITE_THREAD_FUNCTION( )) ContextThread, (LPVOID) i);
	if (rv)
	{
		(void)SYS_CloseFile(psContext[i].dwClientID);
		psContext[i].dwClientID = 0;
		Log2(PCSC_LOG_CRITICAL, "SYS_ThreadCreate failed: %s", strerror(rv));
		return SCARD_E_NO_MEMORY;
	}

	return SCARD_S_SUCCESS;
}

/*
 * A list of local functions used to keep track of clients and their
 * connections
 */

/**
 * @brief Handles messages received from Clients.
 *
 * For each Client message a new instance of this thread is created.
 *
 * @param[in] dwIndex Index of an avaiable Application Context slot in
 * \c psContext.
 */
static const char *CommandsText[] = {
	"NULL",
	"ESTABLISH_CONTEXT",	/* 0x01 */
	"RELEASE_CONTEXT",
	"LIST_READERS",
	"CONNECT",
	"RECONNECT",			/* 0x05 */
	"DISCONNECT",
	"BEGIN_TRANSACTION",
	"END_TRANSACTION",
	"TRANSMIT",
	"CONTROL",				/* 0x0A */
	"STATUS",
	"GET_STATUS_CHANGE",
	"CANCEL",
	"CANCEL_TRANSACTION",
	"GET_ATTRIB",			/* 0x0F */
	"SET_ATTRIB",
	"CMD_VERSION",
	"CMD_GET_READERS_STATE",
	"CMD_WAIT_READER_STATE_CHANGE",
	"CMD_STOP_WAITING_READER_STATE_CHANGE",	/* 0x14 */
	"NULL"
};

#define READ_BODY(v) \
	if (header.size != sizeof(v)) { goto wrong_length; } \
	ret = SHMMessageReceive(&v, sizeof(v), filedes, PCSCLITE_READ_TIMEOUT); \
	if (ret < 0) { Log2(PCSC_LOG_DEBUG, "Client die: %d", filedes); goto exit; }

#define WRITE_BODY(v) \
	ret = SHMMessageSend(&v, sizeof(v), filedes, PCSCLITE_WRITE_TIMEOUT);

static void ContextThread(LPVOID dwIndex)
{
	DWORD dwContextIndex = (DWORD)dwIndex;
	int32_t filedes = psContext[dwContextIndex].dwClientID;

	Log2(PCSC_LOG_DEBUG, "Thread is started: %d",
		psContext[dwContextIndex].dwClientID);

	while (1)
	{
		struct rxHeader header;
		int32_t ret = SHMMessageReceive(&header, sizeof(header), filedes, PCSCLITE_READ_TIMEOUT);

		if (ret < 0)
		{
			/* Clean up the dead client */
			Log2(PCSC_LOG_DEBUG, "Client die: %d", filedes);
			EHTryToUnregisterClientForEvent(filedes);
			goto exit;
		}

		Log3(PCSC_LOG_DEBUG, "Received command: %s from client %d",
			CommandsText[header.command], filedes);

		switch (header.command)
		{
			/* pcsc-lite client/server protocol version */
			case CMD_VERSION:
			{
				struct version_struct veStr;

				READ_BODY(veStr)

				/* get the client protocol version */
				psContext[dwContextIndex].protocol_major = veStr.major;
				psContext[dwContextIndex].protocol_minor = veStr.minor;

				Log3(PCSC_LOG_DEBUG,
						"Client is protocol version %d:%d",
						veStr.major, veStr.minor);

				veStr.rv = SCARD_S_SUCCESS;

				/* client is newer than server */
				if ((veStr.major > PROTOCOL_VERSION_MAJOR)
						|| (veStr.major == PROTOCOL_VERSION_MAJOR
							&& veStr.minor > PROTOCOL_VERSION_MINOR))
				{
					Log3(PCSC_LOG_CRITICAL,
							"Client protocol is too new %d:%d",
							veStr.major, veStr.minor);
					Log3(PCSC_LOG_CRITICAL,
							"Server protocol is %d:%d",
							PROTOCOL_VERSION_MAJOR, PROTOCOL_VERSION_MINOR);
					veStr.rv = SCARD_E_NO_SERVICE;
				}

				/* set the server protocol version */
				veStr.major = PROTOCOL_VERSION_MAJOR;
				veStr.minor = PROTOCOL_VERSION_MINOR;

				/* send back the response */
				WRITE_BODY(veStr)
			}
			break;

			case CMD_GET_READERS_STATE:
			{
				/* nothing to read */

				/* dump the readers state */
				ret = SHMMessageSend(readerStates, sizeof(readerStates), filedes, PCSCLITE_WRITE_TIMEOUT);
			}
			break;

			case CMD_WAIT_READER_STATE_CHANGE:
			{
				struct wait_reader_state_change waStr;

				READ_BODY(waStr)

				/* add the client fd to the list */
				EHRegisterClientForEvent(filedes);

				/* We do not send anything here.
				 * Either the client will timeout or the server will
				 * answer if an event occurs */
			}
			break;

			case CMD_STOP_WAITING_READER_STATE_CHANGE:
			{
				struct wait_reader_state_change waStr;

				READ_BODY(waStr)

				/* add the client fd to the list */
				waStr.rv = EHUnregisterClientForEvent(filedes);

				WRITE_BODY(waStr)
			}
			break;

			case SCARD_ESTABLISH_CONTEXT:
			{
				struct establish_struct esStr;
				SCARDCONTEXT hContext;

				READ_BODY(esStr)

				hContext = esStr.hContext;
				esStr.rv = SCardEstablishContext(esStr.dwScope, 0, 0, &hContext);
				esStr.hContext = hContext;

				if (esStr.rv == SCARD_S_SUCCESS)
					esStr.rv =
						MSGAddContext(esStr.hContext, dwContextIndex);

				WRITE_BODY(esStr)
			}
			break;

			case SCARD_RELEASE_CONTEXT:
			{
				struct release_struct reStr;

				READ_BODY(reStr)

				reStr.rv = SCardReleaseContext(reStr.hContext);

				if (reStr.rv == SCARD_S_SUCCESS)
					reStr.rv =
						MSGRemoveContext(reStr.hContext, dwContextIndex);

				WRITE_BODY(reStr)
			}
			break;

			case SCARD_CONNECT:
			{
				struct connect_struct coStr;
				SCARDHANDLE hCard;
				DWORD dwActiveProtocol;

				READ_BODY(coStr)

				hCard = coStr.hCard;
				dwActiveProtocol = coStr.dwActiveProtocol;

				coStr.rv = SCardConnect(coStr.hContext, coStr.szReader,
						coStr.dwShareMode, coStr.dwPreferredProtocols,
						&hCard, &dwActiveProtocol);

				coStr.hCard = hCard;
				coStr.dwActiveProtocol = dwActiveProtocol;

				if (coStr.rv == SCARD_S_SUCCESS)
					coStr.rv =
						MSGAddHandle(coStr.hContext, coStr.hCard, dwContextIndex);

				WRITE_BODY(coStr)
			}
			break;

			case SCARD_RECONNECT:
			{
				struct reconnect_struct rcStr;
				DWORD dwActiveProtocol;

				READ_BODY(rcStr)

				if (MSGCheckHandleAssociation(rcStr.hCard, dwContextIndex))
					goto exit;

				rcStr.rv = SCardReconnect(rcStr.hCard, rcStr.dwShareMode,
						rcStr.dwPreferredProtocols,
						rcStr.dwInitialization, &dwActiveProtocol);
				rcStr.dwActiveProtocol = dwActiveProtocol;

				WRITE_BODY(rcStr)
			}
			break;

			case SCARD_DISCONNECT:
			{
				struct disconnect_struct diStr;
				LONG rv;

				READ_BODY(diStr)

				rv = MSGCheckHandleAssociation(diStr.hCard, dwContextIndex);
				if (0 == rv)
				{
					diStr.rv = SCardDisconnect(diStr.hCard, diStr.dwDisposition);

					if (SCARD_S_SUCCESS == diStr.rv)
						diStr.rv =
							MSGRemoveHandle(diStr.hCard, dwContextIndex);
				}

				WRITE_BODY(diStr)
			}
			break;

			case SCARD_BEGIN_TRANSACTION:
			{
				struct begin_struct beStr;
				LONG rv;

				READ_BODY(beStr)

				rv = MSGCheckHandleAssociation(beStr.hCard, dwContextIndex);
				if (0 == rv)
					beStr.rv = SCardBeginTransaction(beStr.hCard);

				WRITE_BODY(beStr)
			}
			break;

			case SCARD_END_TRANSACTION:
			{
				struct end_struct enStr;
				LONG rv;

				READ_BODY(enStr)

				rv = MSGCheckHandleAssociation(enStr.hCard, dwContextIndex);
				if (0 == rv)
					enStr.rv =
						SCardEndTransaction(enStr.hCard, enStr.dwDisposition);

				WRITE_BODY(enStr)
			}
			break;

			case SCARD_CANCEL_TRANSACTION:
			{
				struct cancel_transaction_struct caStr;
				LONG rv;

				READ_BODY(caStr)

				rv = MSGCheckHandleAssociation(caStr.hCard, dwContextIndex);
				if (0 == rv)
					caStr.rv = SCardCancelTransaction(caStr.hCard);

				WRITE_BODY(caStr)
			}
			break;

			case SCARD_CANCEL:
			{
				struct cancel_struct caStr;
				uint32_t fd = 0;
				int i;

				READ_BODY(caStr)

				/* find the client */
				for (i=0; i<PCSCLITE_MAX_APPLICATIONS_CONTEXTS; i++)
				{
					if (psContext[i].hContext == caStr.hContext)
					{
						fd = psContext[i].dwClientID;
						break;
					}
				}

				if (fd)
					caStr.rv = MSGSignalClient(fd, SCARD_E_CANCELLED);
				else
					caStr.rv = SCARD_E_INVALID_VALUE;

				WRITE_BODY(caStr)
			}
			break;

			case SCARD_STATUS:
			{
				struct status_struct stStr;
				LONG rv;

				READ_BODY(stStr)

				rv = MSGCheckHandleAssociation(stStr.hCard, dwContextIndex);
				if (0 == rv)
				{
					DWORD cchReaderLen;
					DWORD dwState;
					DWORD dwProtocol;
					DWORD cbAtrLen;

					cchReaderLen = stStr.pcchReaderLen;
					dwState = stStr.dwState;
					dwProtocol = stStr.dwProtocol;
					cbAtrLen = stStr.pcbAtrLen;

					/* avoids buffer overflow */
					if ((cchReaderLen > sizeof(stStr.mszReaderNames))
						|| (cbAtrLen > sizeof(stStr.pbAtr)))
					{
						stStr.rv = SCARD_E_INSUFFICIENT_BUFFER ;
					}
					else
					{
						stStr.rv = SCardStatus(stStr.hCard,
							stStr.mszReaderNames,
							&cchReaderLen, &dwState,
							&dwProtocol, stStr.pbAtr, &cbAtrLen);

						stStr.pcchReaderLen = cchReaderLen;
						stStr.dwState = dwState;
						stStr.dwProtocol = dwProtocol;
						stStr.pcbAtrLen = cbAtrLen;
					}
				}

				WRITE_BODY(stStr)
			}
			break;

			case SCARD_TRANSMIT:
			{
				struct transmit_struct trStr;
				unsigned char pbSendBuffer[MAX_BUFFER_SIZE_EXTENDED];
				unsigned char pbRecvBuffer[MAX_BUFFER_SIZE_EXTENDED];
				SCARD_IO_REQUEST ioSendPci;
				SCARD_IO_REQUEST ioRecvPci;
				DWORD cbRecvLength;

				READ_BODY(trStr)

				if (MSGCheckHandleAssociation(trStr.hCard, dwContextIndex))
					goto exit;

				/* avoids buffer overflow */
				if ((trStr.pcbRecvLength > sizeof(pbRecvBuffer))
					|| (trStr.cbSendLength > sizeof(pbSendBuffer)))
					goto exit;

				/* read sent buffer */
				ret = SHMMessageReceive(pbSendBuffer, trStr.cbSendLength,
					filedes, PCSCLITE_READ_TIMEOUT);
				if (ret < 0)
				{
					Log2(PCSC_LOG_DEBUG, "Client die: %d", filedes);
					goto exit;
				}

				ioSendPci.dwProtocol = trStr.ioSendPciProtocol;
				ioSendPci.cbPciLength = trStr.ioSendPciLength;
				ioRecvPci.dwProtocol = trStr.ioRecvPciProtocol;
				ioRecvPci.cbPciLength = trStr.ioRecvPciLength;
				cbRecvLength = trStr.pcbRecvLength;

				trStr.rv = SCardTransmit(trStr.hCard, &ioSendPci,
					pbSendBuffer, trStr.cbSendLength, &ioRecvPci,
					pbRecvBuffer, &cbRecvLength);

				trStr.ioSendPciProtocol = ioSendPci.dwProtocol;
				trStr.ioSendPciLength = ioSendPci.cbPciLength;
				trStr.ioRecvPciProtocol = ioRecvPci.dwProtocol;
				trStr.ioRecvPciLength = ioRecvPci.cbPciLength;
				trStr.pcbRecvLength = cbRecvLength;

				WRITE_BODY(trStr)

				/* write received buffer */
				if (SCARD_S_SUCCESS == trStr.rv)
					ret = SHMMessageSend(pbRecvBuffer, cbRecvLength,
						filedes, PCSCLITE_WRITE_TIMEOUT);
			}
			break;

			case SCARD_CONTROL:
			{
				struct control_struct ctStr;
				unsigned char pbSendBuffer[MAX_BUFFER_SIZE_EXTENDED];
				unsigned char pbRecvBuffer[MAX_BUFFER_SIZE_EXTENDED];
				DWORD dwBytesReturned;

				READ_BODY(ctStr)

				if (MSGCheckHandleAssociation(ctStr.hCard, dwContextIndex))
					goto exit;

				/* avoids buffer overflow */
				if ((ctStr.cbRecvLength > sizeof(pbRecvBuffer))
					|| (ctStr.cbSendLength > sizeof(pbSendBuffer)))
				{
					goto exit;
				}

				/* read sent buffer */
				ret = SHMMessageReceive(pbSendBuffer, ctStr.cbSendLength,
					filedes, PCSCLITE_READ_TIMEOUT);
				if (ret < 0)
				{
					Log2(PCSC_LOG_DEBUG, "Client die: %d", filedes);
					goto exit;
				}

				dwBytesReturned = ctStr.dwBytesReturned;

				ctStr.rv = SCardControl(ctStr.hCard, ctStr.dwControlCode,
					pbSendBuffer, ctStr.cbSendLength,
					pbRecvBuffer, ctStr.cbRecvLength,
					&dwBytesReturned);

				ctStr.dwBytesReturned = dwBytesReturned;

				WRITE_BODY(ctStr)

				/* write received buffer */
				if (SCARD_S_SUCCESS == ctStr.rv)
					ret = SHMMessageSend(pbRecvBuffer, dwBytesReturned,
						filedes, PCSCLITE_WRITE_TIMEOUT);
			}
			break;

			case SCARD_GET_ATTRIB:
			{
				struct getset_struct gsStr;
				DWORD cbAttrLen;

				READ_BODY(gsStr)

				if (MSGCheckHandleAssociation(gsStr.hCard, dwContextIndex))
					goto exit;

				/* avoids buffer overflow */
				if (gsStr.cbAttrLen > sizeof(gsStr.pbAttr))
					goto buffer_overflow;

				cbAttrLen = gsStr.cbAttrLen;

				gsStr.rv = SCardGetAttrib(gsStr.hCard, gsStr.dwAttrId,
						gsStr.pbAttr, &cbAttrLen);

				gsStr.cbAttrLen = cbAttrLen;

				WRITE_BODY(gsStr)
			}
			break;

			case SCARD_SET_ATTRIB:
			{
				struct getset_struct gsStr;

				READ_BODY(gsStr)

				if (MSGCheckHandleAssociation(gsStr.hCard, dwContextIndex))
					goto buffer_overflow;

				/* avoids buffer overflow */
				if (gsStr.cbAttrLen > sizeof(gsStr.pbAttr))
					goto buffer_overflow;

				gsStr.rv = SCardSetAttrib(gsStr.hCard, gsStr.dwAttrId,
					gsStr.pbAttr, gsStr.cbAttrLen);

				WRITE_BODY(gsStr)
			}
			break;

			default:
				Log2(PCSC_LOG_CRITICAL, "Unknown command: %d", header.command);
				goto exit;
		}

		/* SHMMessageSend() failed */
		if (-1 == ret)
		{
			/* Clean up the dead client */
			Log2(PCSC_LOG_DEBUG, "Client die: %d", filedes);
			goto exit;
		}
	}

buffer_overflow:
	Log2(PCSC_LOG_DEBUG, "Buffer overflow detected: %d", filedes);
	goto exit;
wrong_length:
	Log2(PCSC_LOG_DEBUG, "Wrong length: %d", filedes);
exit:
	(void)SYS_CloseFile(filedes);
	(void)MSGCleanupClient(dwContextIndex);
	(void)SYS_ThreadExit((LPVOID) NULL);
}

LONG MSGSignalClient(uint32_t filedes, LONG rv)
{
	uint32_t ret;
	struct wait_reader_state_change waStr;

	Log2(PCSC_LOG_DEBUG, "Signal client: %d", filedes);

	waStr.rv = rv;
	WRITE_BODY(waStr)

	return ret;
} /* MSGSignalClient */

static LONG MSGAddContext(SCARDCONTEXT hContext, DWORD dwContextIndex)
{
	psContext[dwContextIndex].hContext = hContext;
	return SCARD_S_SUCCESS;
}

static LONG MSGRemoveContext(SCARDCONTEXT hContext, DWORD dwContextIndex)
{
	int i;
	LONG rv;

	if (psContext[dwContextIndex].hContext == hContext)
	{
		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
		{
			/*
			 * Disconnect each of these just in case
			 */

			if (psContext[dwContextIndex].hCard[i] != 0)
			{
				PREADER_CONTEXT rContext = NULL;
				DWORD dwLockId;

				/*
				 * Unlock the sharing
				 */
				rv = RFReaderInfoById(psContext[dwContextIndex].hCard[i],
					&rContext);
				if (rv != SCARD_S_SUCCESS)
					return rv;

				dwLockId = rContext->dwLockId;
				rContext->dwLockId = 0;

				if (psContext[dwContextIndex].hCard[i] != dwLockId)
				{
					/*
					 * if the card is locked by someone else we do not reset it
					 * and simulate a card removal
					 */
					rv = SCARD_W_REMOVED_CARD;
				}
				else
				{
					/*
					 * We will use SCardStatus to see if the card has been
					 * reset there is no need to reset each time
					 * Disconnect is called
					 */
					rv = SCardStatus(psContext[dwContextIndex].hCard[i], NULL,
						NULL, NULL, NULL, NULL, NULL);
				}

				if (rv == SCARD_W_RESET_CARD || rv == SCARD_W_REMOVED_CARD)
					(void)SCardDisconnect(psContext[dwContextIndex].hCard[i],
						SCARD_LEAVE_CARD);
				else
					(void)SCardDisconnect(psContext[dwContextIndex].hCard[i],
						SCARD_RESET_CARD);

				psContext[dwContextIndex].hCard[i] = 0;
			}
		}

		psContext[dwContextIndex].hContext = 0;
		return SCARD_S_SUCCESS;
	}

	return SCARD_E_INVALID_VALUE;
}

static LONG MSGAddHandle(SCARDCONTEXT hContext, SCARDHANDLE hCard,
	DWORD dwContextIndex)
{
	int i;

	if (psContext[dwContextIndex].hContext == hContext)
	{

		/*
		 * Find an empty spot to put the hCard value
		 */
		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
		{
			if (psContext[dwContextIndex].hCard[i] == 0)
			{
				psContext[dwContextIndex].hCard[i] = hCard;
				break;
			}
		}

		if (i == PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS)
		{
			return SCARD_F_INTERNAL_ERROR;
		} else
		{
			return SCARD_S_SUCCESS;
		}

	}

	return SCARD_E_INVALID_VALUE;
}

static LONG MSGRemoveHandle(SCARDHANDLE hCard, DWORD dwContextIndex)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
	{
		if (psContext[dwContextIndex].hCard[i] == hCard)
		{
			psContext[dwContextIndex].hCard[i] = 0;
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_INVALID_VALUE;
}


static LONG MSGCheckHandleAssociation(SCARDHANDLE hCard, DWORD dwContextIndex)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
	{
		if (psContext[dwContextIndex].hCard[i] == hCard)
		{
			return 0;
		}
	}

	/* Must be a rogue client, debug log and sleep a couple of seconds */
	Log1(PCSC_LOG_ERROR, "Client failed to authenticate");
	(void)SYS_Sleep(2);

	return -1;
}

static LONG MSGCleanupClient(DWORD dwContextIndex)
{
	if (psContext[dwContextIndex].hContext != 0)
	{
		(void)SCardReleaseContext(psContext[dwContextIndex].hContext);
		(void)MSGRemoveContext(psContext[dwContextIndex].hContext,
			dwContextIndex);
	}

	psContext[dwContextIndex].dwClientID = 0;
	psContext[dwContextIndex].protocol_major = 0;
	psContext[dwContextIndex].protocol_minor = 0;

	return 0;
}
