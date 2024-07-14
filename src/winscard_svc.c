/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2002-2023
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 * Copyright (C) 2009
 *  Jean-Luc Giraud <jlgiraud@googlemail.com>
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
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "pcscd.h"
#include "winscard.h"
#include "debuglog.h"
#include "winscard_msg.h"
#include "winscard_svc.h"
#include "sys_generic.h"
#include "utils.h"
#include "readerfactory.h"
#include "eventhandler.h"
#include "simclist.h"
#include "auth.h"

/**
 * @brief Represents an Application Context on the Server side.
 *
 * An Application Context contains Channels (\c hCard).
 */

extern bool AutoExit;
static int contextMaxThreadCounter = PCSC_MAX_CONTEXT_THREADS;
static int contextMaxCardHandles = PCSC_MAX_CONTEXT_CARD_HANDLES;

static list_t contextsList;	/**< Context tracking list */
pthread_mutex_t contextsList_lock;	/**< lock for the above list */

struct _psContext
{
	int32_t hContext;
	list_t cardsList;
	pthread_mutex_t cardsList_lock;	/**< lock for the above list */
	uint32_t dwClientID;	/**< Connection ID used to reference the Client. */
	pthread_t pthThread;	/**< Event polling thread's ID */
};
typedef struct _psContext SCONTEXT;

static LONG MSGCheckHandleAssociation(SCARDHANDLE, SCONTEXT *);
static LONG MSGAddContext(SCARDCONTEXT, SCONTEXT *);
static LONG MSGRemoveContext(SCARDCONTEXT, SCONTEXT *);
static LONG MSGAddHandle(SCARDCONTEXT, SCARDHANDLE, SCONTEXT *);
static LONG MSGRemoveHandle(SCARDHANDLE, SCONTEXT *);
static void MSGCleanupClient(SCONTEXT *);

static void * ContextThread(LPVOID pdwIndex);

extern READER_STATE readerStates[PCSCLITE_MAX_READERS_CONTEXTS];
extern int16_t ReaderEvents;

static int contextsListhContext_seeker(const void *el, const void *key)
{
	const SCONTEXT * currentContext = (SCONTEXT *)el;

	if ((el == NULL) || (key == NULL))
	{
		Log3(PCSC_LOG_CRITICAL, "called with NULL pointer: el=%p, key=%p",
			el, key);
		return 0;
	}

	if (currentContext->hContext == *(int32_t *)key)
		return 1;
	return 0;
}

LONG ContextsInitialize(int customMaxThreadCounter,
	int customMaxThreadCardHandles)
{
	int lrv = 0;

	if (customMaxThreadCounter != 0)
		contextMaxThreadCounter = customMaxThreadCounter;

	if (customMaxThreadCardHandles != 0)
		contextMaxCardHandles = customMaxThreadCardHandles;

	lrv = list_init(&contextsList);
	if (lrv < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "list_init failed with return value: %d", lrv);
		return -1;
	}
	lrv = list_attributes_seeker(& contextsList, contextsListhContext_seeker);
	if (lrv < 0)
	{
		Log2(PCSC_LOG_CRITICAL,
			"list_attributes_seeker failed with return value: %d", lrv);
		return -1;
	}

	(void)pthread_mutex_init(&contextsList_lock, NULL);

	return 1;
}

void ContextsDeinitialize(void)
{
	int listSize;
	(void)pthread_mutex_lock(&contextsList_lock);
	listSize = list_size(&contextsList);
#ifdef NO_LOG
	(void)listSize;
#endif
	Log2(PCSC_LOG_DEBUG, "remaining threads: %d", listSize);

	/* terminate all the client threads */
	int rv = list_iterator_start(&contextsList);
	if (0 == rv)
		Log1(PCSC_LOG_ERROR, "list_iterator_start failed");
	else
	{
		while (list_iterator_hasnext(&contextsList))
		{
			SCONTEXT * elt = list_iterator_next(&contextsList);
			Log3(PCSC_LOG_DEBUG, "Cancel dwClientID=%d hContext: %p",
				elt->dwClientID, elt);
			EHTryToUnregisterClientForEvent(elt->dwClientID);
			close(elt->dwClientID);
			Log2(PCSC_LOG_DEBUG, "Waiting client: %d", elt->dwClientID);
			pthread_join(elt->pthThread, NULL);
			Log2(PCSC_LOG_INFO, "Client %d terminated", elt->dwClientID);
		}
	}
	list_destroy(&contextsList);
	(void)pthread_mutex_unlock(&contextsList_lock);
}

/**
 * @brief Creates threads to handle messages received from Clients.
 *
 * @param[in] pdwClientID Connection ID used to reference the Client.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Success.
 * @retval SCARD_F_INTERNAL_ERROR Exceeded the maximum number of simultaneous Application Contexts.
 * @retval SCARD_E_NO_MEMORY Error creating the Context Thread.
 */
LONG CreateContextThread(uint32_t *pdwClientID)
{
	int rv;
	int lrv;
	int listSize;
	SCONTEXT * newContext = NULL;
	LONG retval = SCARD_E_NO_MEMORY;

	(void)pthread_mutex_lock(&contextsList_lock);

	listSize = list_size(&contextsList);
	if (listSize >= contextMaxThreadCounter)
	{
		Log2(PCSC_LOG_CRITICAL, "Too many context running: %d", listSize);
		goto out;
	}

	/* Create the context for this thread. */
	newContext = malloc(sizeof(*newContext));
	if (NULL == newContext)
	{
		Log1(PCSC_LOG_CRITICAL, "Could not allocate new context");
		goto out;
	}
	memset(newContext, 0, sizeof(*newContext));

	newContext->dwClientID = *pdwClientID;

	/* Initialise the list of card contexts */
	lrv = list_init(&newContext->cardsList);
	if (lrv < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "list_init failed with return value: %d", lrv);
		goto out;
	}

	/* request to store copies, and provide the metric function */
	list_attributes_copy(&newContext->cardsList, list_meter_int32_t, 1);

	/* Adding a comparator
	 * The stored type is SCARDHANDLE (long) but has only 32 bits
	 * useful even on a 64-bit CPU since the API between pcscd and
	 * libpcscliter uses "int32_t hCard;"
	 */
	lrv = list_attributes_comparator(&newContext->cardsList,
		list_comparator_int32_t);
	if (lrv != 0)
	{
		Log2(PCSC_LOG_CRITICAL,
			"list_attributes_comparator failed with return value: %d", lrv);
		list_destroy(&newContext->cardsList);
		goto out;
	}

	(void)pthread_mutex_init(&newContext->cardsList_lock, NULL);

	lrv = list_append(&contextsList, newContext);
	if (lrv < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "list_append failed with return value: %d",
			lrv);
		list_destroy(&newContext->cardsList);
		goto out;
	}

	rv = ThreadCreate(&newContext->pthThread, THREAD_ATTR_DETACHED,
		(PCSCLITE_THREAD_FUNCTION( )) ContextThread, (LPVOID) newContext);
	if (rv)
	{
		int lrv2;

		Log2(PCSC_LOG_CRITICAL, "ThreadCreate failed: %s", strerror(rv));
		lrv2 = list_delete(&contextsList, newContext);
		if (lrv2 < 0)
			Log2(PCSC_LOG_CRITICAL, "list_delete failed with error %d", lrv2);
		list_destroy(&newContext->cardsList);
		goto out;
	}

	/* disable any suicide alarm */
	if (AutoExit)
		alarm(0);

	retval = SCARD_S_SUCCESS;

out:
	(void)pthread_mutex_unlock(&contextsList_lock);

	if (retval != SCARD_S_SUCCESS)
	{
		if (newContext)
			free(newContext);
		(void)close(*pdwClientID);
	}

	return retval;
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
 * @param[in] dwIndex Index of an available Application Context slot in
 * \c SCONTEXT *.
 */
#ifndef NO_LOG
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
	"CMD_GET_READER_EVENTS",
	"NULL"
};
#endif

#define READ_BODY(v) \
	do { \
		if (header.size != sizeof(v)) \
			goto wrong_length;  \
		ret = MessageReceive(&v, sizeof(v), filedes); \
		if (ret != SCARD_S_SUCCESS) { \
			Log2(PCSC_LOG_DEBUG, "Client die: %d", filedes); \
			goto exit; \
		} \
	} while (0)

#define WRITE_BODY(v) \
	WRITE_BODY_WITH_COMMAND(CommandsText[header.command], v)
#define WRITE_BODY_WITH_COMMAND(command, v) \
	do { \
		LogRv4(PCSC_LOG_DEBUG, v.rv, "%s for client %d", command, filedes); \
		ret = MessageSend(&v, sizeof(v), filedes); \
	} while (0)

static void * ContextThread(LPVOID newContext)
{
	SCONTEXT * threadContext = (SCONTEXT *) newContext;
	int32_t filedes = threadContext->dwClientID;

	if (IsClientAuthorized(filedes, "access_pcsc", NULL) == 0)
	{
		Log1(PCSC_LOG_CRITICAL, "Rejected unauthorized PC/SC client");
		goto exit;
	}
	else
	{
		Log1(PCSC_LOG_DEBUG, "Authorized PC/SC client");
	}

	Log3(PCSC_LOG_DEBUG, "Thread is started: dwClientID=%d, threadContext @%p",
		threadContext->dwClientID, threadContext);

	while (1)
	{
		struct rxHeader header;
		int32_t ret = MessageReceive(&header, sizeof(header), filedes);

		if (ret != SCARD_S_SUCCESS)
		{
			/* Clean up the dead client */
			Log2(PCSC_LOG_DEBUG, "Client die: %d", filedes);
			EHTryToUnregisterClientForEvent(filedes);
			goto exit;
		}

		if ((header.command > CMD_ENUM_FIRST)
			&& (header.command < CMD_ENUM_LAST))
			Log3(PCSC_LOG_DEBUG, "Received command: %s from client %d",
				CommandsText[header.command], filedes);

		switch (header.command)
		{
			/* pcsc-lite client/server protocol version */
			case CMD_VERSION:
			{
				struct version_struct veStr;

				READ_BODY(veStr);

				Log3(PCSC_LOG_DEBUG, "Client is protocol version %d:%d",
					veStr.major, veStr.minor);

				veStr.rv = SCARD_S_SUCCESS;

				/* client and server use different protocol */
				if ((veStr.major != PROTOCOL_VERSION_MAJOR)
					|| (veStr.minor != PROTOCOL_VERSION_MINOR))
				{
					Log1(PCSC_LOG_CRITICAL,
						"Communication protocol mismatch!");
					Log3(PCSC_LOG_ERROR, "Client protocol is %d:%d",
						veStr.major, veStr.minor);
					Log3(PCSC_LOG_ERROR, "Server protocol is %d:%d",
						PROTOCOL_VERSION_MAJOR, PROTOCOL_VERSION_MINOR);
					veStr.rv = SCARD_E_SERVICE_STOPPED;
				}

				/* set the server protocol version */
				veStr.major = PROTOCOL_VERSION_MAJOR;
				veStr.minor = PROTOCOL_VERSION_MINOR;

				/* send back the response */
				WRITE_BODY(veStr);
			}
			break;

			case CMD_GET_READERS_STATE:
			{
				/* nothing to read */

#ifdef USE_USB
				/* wait until all readers are ready */
				RFWaitForReaderInit();
#endif

				/* dump the readers state */
				ret = MessageSend(readerStates, sizeof(readerStates), filedes);
			}
			break;

			case CMD_WAIT_READER_STATE_CHANGE:
			{
				/* nothing to read */

#ifdef USE_USB
				/* wait until all readers are ready */
				RFWaitForReaderInit();
#endif

				/* add the client fd to the list and dump the readers state */
				EHRegisterClientForEvent(filedes);
			}
			break;

			case CMD_STOP_WAITING_READER_STATE_CHANGE:
			{
				struct wait_reader_state_change waStr =
				{
					.timeOut = 0,
					.rv = SCARD_S_SUCCESS
				};
				LONG rv;

				/* remove the client fd from the list */
				rv = EHUnregisterClientForEvent(filedes);

				/* send the response only if the client was still in the
				 * list */
				if (rv != SCARD_F_INTERNAL_ERROR)
				{
					waStr.rv = rv;
					WRITE_BODY(waStr);
				}
			}
			break;

			case CMD_GET_READER_EVENTS:
			{
				/* nothing to read */

				struct get_reader_events readerEvents =
				{
					.readerEvents = ReaderEvents,
					.rv = SCARD_S_SUCCESS
				};

				WRITE_BODY(readerEvents);
			}
			break;

			case SCARD_ESTABLISH_CONTEXT:
			{
				struct establish_struct esStr;
				SCARDCONTEXT hContext;

				READ_BODY(esStr);

				hContext = esStr.hContext;
				esStr.rv = SCardEstablishContext(esStr.dwScope, 0, 0,
					&hContext);
				esStr.hContext = hContext;

				if (esStr.rv == SCARD_S_SUCCESS)
					esStr.rv = MSGAddContext(esStr.hContext, threadContext);

				WRITE_BODY(esStr);
			}
			break;

			case SCARD_RELEASE_CONTEXT:
			{
				struct release_struct reStr;

				READ_BODY(reStr);

				reStr.rv = SCardReleaseContext(reStr.hContext);

				if (reStr.rv == SCARD_S_SUCCESS)
					reStr.rv = MSGRemoveContext(reStr.hContext, threadContext);

				WRITE_BODY(reStr);
			}
			break;

			case SCARD_CONNECT:
			{
				struct connect_struct coStr;
				SCARDHANDLE hCard;
				DWORD dwActiveProtocol;

				READ_BODY(coStr);

				coStr.szReader[sizeof(coStr.szReader)-1] = 0;
				hCard = coStr.hCard;
				dwActiveProtocol = coStr.dwActiveProtocol;

				if (IsClientAuthorized(filedes, "access_card", coStr.szReader) == 0)
				{
					Log2(PCSC_LOG_CRITICAL, "Rejected unauthorized client for '%s'", coStr.szReader);

					coStr.rv = SCARD_W_SECURITY_VIOLATION;
					hCard = -1;
					dwActiveProtocol = -1;
				}
				else
				{
					Log2(PCSC_LOG_DEBUG, "Authorized client for '%s'", coStr.szReader);

					coStr.rv = SCardConnect(coStr.hContext, coStr.szReader,
						coStr.dwShareMode, coStr.dwPreferredProtocols,
						&hCard, &dwActiveProtocol);
				}

				coStr.hCard = hCard;
				coStr.dwActiveProtocol = dwActiveProtocol;

				if (coStr.rv == SCARD_S_SUCCESS)
				{
					coStr.rv = MSGAddHandle(coStr.hContext, coStr.hCard,
						threadContext);

					/* if storing the hCard fails we disconnect */
					if (coStr.rv != SCARD_S_SUCCESS)
						SCardDisconnect(coStr.hCard, SCARD_LEAVE_CARD);
				}

				WRITE_BODY(coStr);
			}
			break;

			case SCARD_RECONNECT:
			{
				struct reconnect_struct rcStr;
				DWORD dwActiveProtocol = SCARD_PROTOCOL_UNDEFINED;

				READ_BODY(rcStr);

				if (MSGCheckHandleAssociation(rcStr.hCard, threadContext))
					goto exit;

				rcStr.rv = SCardReconnect(rcStr.hCard, rcStr.dwShareMode,
					rcStr.dwPreferredProtocols, rcStr.dwInitialization,
					&dwActiveProtocol);
				rcStr.dwActiveProtocol = dwActiveProtocol;

				WRITE_BODY(rcStr);
			}
			break;

			case SCARD_DISCONNECT:
			{
				struct disconnect_struct diStr;

				READ_BODY(diStr);

				if (MSGCheckHandleAssociation(diStr.hCard, threadContext))
					goto exit;

				diStr.rv = SCardDisconnect(diStr.hCard, diStr.dwDisposition);

				if (SCARD_S_SUCCESS == diStr.rv)
					diStr.rv = MSGRemoveHandle(diStr.hCard, threadContext);

				WRITE_BODY(diStr);
			}
			break;

			case SCARD_BEGIN_TRANSACTION:
			{
				struct begin_struct beStr;

				READ_BODY(beStr);

				if (MSGCheckHandleAssociation(beStr.hCard, threadContext))
					goto exit;

				beStr.rv = SCardBeginTransaction(beStr.hCard);

				WRITE_BODY(beStr);
			}
			break;

			case SCARD_END_TRANSACTION:
			{
				struct end_struct enStr;

				READ_BODY(enStr);

				if (MSGCheckHandleAssociation(enStr.hCard, threadContext))
					goto exit;

				enStr.rv = SCardEndTransaction(enStr.hCard,
					enStr.dwDisposition);

				WRITE_BODY(enStr);
			}
			break;

			case SCARD_CANCEL:
			{
				struct cancel_struct caStr;
				SCONTEXT * psTargetContext = NULL;

				READ_BODY(caStr);

				/* find the client */
				(void)pthread_mutex_lock(&contextsList_lock);
				psTargetContext = (SCONTEXT *) list_seek(&contextsList,
					&caStr.hContext);
				(void)pthread_mutex_unlock(&contextsList_lock);

				/* default value = error */
				caStr.rv = SCARD_E_INVALID_HANDLE;

				if (psTargetContext != NULL)
				{
					uint32_t fd = psTargetContext->dwClientID;
					LONG rv;

					/* the client should not receive the event
					 * notification now the waiting has been cancelled */
					rv = EHUnregisterClientForEvent(fd);

					/* signal the client only if it was still waiting */
					if (SCARD_S_SUCCESS == rv)
						caStr.rv = MSGSignalClient(fd, SCARD_E_CANCELLED);
					else
						caStr.rv = SCARD_S_SUCCESS;
				}

				WRITE_BODY(caStr);
			}
			break;

			case SCARD_STATUS:
			{
				struct status_struct stStr;

				READ_BODY(stStr);

				if (MSGCheckHandleAssociation(stStr.hCard, threadContext))
					goto exit;

				/* only hCard and return value are used by the client */
				stStr.rv = SCardStatus(stStr.hCard, NULL, NULL, NULL,
					NULL, 0, NULL);

				WRITE_BODY(stStr);
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

				READ_BODY(trStr);

				if (MSGCheckHandleAssociation(trStr.hCard, threadContext))
					goto exit;

				/* avoids buffer overflow */
				if (trStr.cbSendLength > sizeof(pbSendBuffer))
					goto buffer_overflow;

				/* read sent buffer */
				ret = MessageReceive(pbSendBuffer, trStr.cbSendLength, filedes);
				if (ret != SCARD_S_SUCCESS)
				{
					Log2(PCSC_LOG_DEBUG, "Client die: %d", filedes);
					goto exit;
				}

				ioSendPci.dwProtocol = trStr.ioSendPciProtocol;
				ioSendPci.cbPciLength = trStr.ioSendPciLength;
				ioRecvPci.dwProtocol = trStr.ioRecvPciProtocol;
				ioRecvPci.cbPciLength = trStr.ioRecvPciLength;
				cbRecvLength = sizeof pbRecvBuffer;

				trStr.rv = SCardTransmit(trStr.hCard, &ioSendPci,
					pbSendBuffer, trStr.cbSendLength, &ioRecvPci,
					pbRecvBuffer, &cbRecvLength);

				if (cbRecvLength > trStr.pcbRecvLength)
					/* The client buffer is not large enough.
					 * The pbRecvBuffer buffer will NOT be sent a few
					 * lines below. So no buffer overflow is expected. */
					trStr.rv = SCARD_E_INSUFFICIENT_BUFFER;

				trStr.ioSendPciProtocol = ioSendPci.dwProtocol;
				trStr.ioSendPciLength = ioSendPci.cbPciLength;
				trStr.ioRecvPciProtocol = ioRecvPci.dwProtocol;
				trStr.ioRecvPciLength = ioRecvPci.cbPciLength;
				trStr.pcbRecvLength = cbRecvLength;

				WRITE_BODY(trStr);

				/* write received buffer */
				if (SCARD_S_SUCCESS == trStr.rv)
					ret = MessageSend(pbRecvBuffer, cbRecvLength, filedes);
			}
			break;

			case SCARD_CONTROL:
			{
				struct control_struct ctStr;
				unsigned char pbSendBuffer[MAX_BUFFER_SIZE_EXTENDED];
				unsigned char pbRecvBuffer[MAX_BUFFER_SIZE_EXTENDED];
				DWORD dwBytesReturned;

				READ_BODY(ctStr);

				if (MSGCheckHandleAssociation(ctStr.hCard, threadContext))
					goto exit;

				/* avoids buffer overflow */
				if (ctStr.cbSendLength > sizeof(pbSendBuffer))
				{
					goto buffer_overflow;
				}

				/* read sent buffer */
				ret = MessageReceive(pbSendBuffer, ctStr.cbSendLength, filedes);
				if (ret != SCARD_S_SUCCESS)
				{
					Log2(PCSC_LOG_DEBUG, "Client die: %d", filedes);
					goto exit;
				}

				dwBytesReturned = ctStr.dwBytesReturned;

				ctStr.rv = SCardControl(ctStr.hCard, ctStr.dwControlCode,
					pbSendBuffer, ctStr.cbSendLength,
					pbRecvBuffer, sizeof pbRecvBuffer,
					&dwBytesReturned);

				if (dwBytesReturned > ctStr.cbRecvLength)
					/* The client buffer is not large enough.
					 * The pbRecvBuffer buffer will NOT be sent a few
					 * lines below. So no buffer overflow is expected. */
					ctStr.rv = SCARD_E_INSUFFICIENT_BUFFER;

				ctStr.dwBytesReturned = dwBytesReturned;

				WRITE_BODY(ctStr);

				/* write received buffer */
				if (SCARD_S_SUCCESS == ctStr.rv)
					ret = MessageSend(pbRecvBuffer, dwBytesReturned, filedes);
			}
			break;

			case SCARD_GET_ATTRIB:
			{
				struct getset_struct gsStr;
				DWORD cbAttrLen;

				READ_BODY(gsStr);

				if (MSGCheckHandleAssociation(gsStr.hCard, threadContext))
					goto exit;

				/* avoids buffer overflow */
				if (gsStr.cbAttrLen > sizeof(gsStr.pbAttr))
					goto buffer_overflow;

				cbAttrLen = gsStr.cbAttrLen;

				gsStr.rv = SCardGetAttrib(gsStr.hCard, gsStr.dwAttrId,
					gsStr.pbAttr, &cbAttrLen);

				gsStr.cbAttrLen = cbAttrLen;

				WRITE_BODY(gsStr);
			}
			break;

			case SCARD_SET_ATTRIB:
			{
				struct getset_struct gsStr;

				READ_BODY(gsStr);

				if (MSGCheckHandleAssociation(gsStr.hCard, threadContext))
					goto exit;

				/* avoids buffer overflow */
				if (gsStr.cbAttrLen > sizeof(gsStr.pbAttr))
					goto buffer_overflow;

				gsStr.rv = SCardSetAttrib(gsStr.hCard, gsStr.dwAttrId,
					gsStr.pbAttr, gsStr.cbAttrLen);

				WRITE_BODY(gsStr);
			}
			break;

			default:
				Log2(PCSC_LOG_CRITICAL, "Unknown command: %d", header.command);
				goto exit;
		}

		/* MessageSend() failed */
		if (ret != SCARD_S_SUCCESS)
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
	(void)close(filedes);
	MSGCleanupClient(threadContext);
	(void)pthread_exit((LPVOID) NULL);
}

LONG MSGSignalClient(uint32_t filedes, LONG rv)
{
	uint32_t ret;
	struct wait_reader_state_change waStr =
	{
		.timeOut = 0,
		.rv = SCARD_S_SUCCESS
	};

	Log2(PCSC_LOG_DEBUG, "Signal client: %d", filedes);

	waStr.rv = rv;
	WRITE_BODY_WITH_COMMAND("SIGNAL", waStr);

	return ret;
} /* MSGSignalClient */

LONG MSGSendReaderStates(uint32_t filedes)
{
	uint32_t ret;

	Log2(PCSC_LOG_DEBUG, "Send reader states: %d", filedes);

	/* dump the readers state */
	ret = MessageSend(readerStates, sizeof(readerStates), filedes);

	return ret;
}

static LONG MSGAddContext(SCARDCONTEXT hContext, SCONTEXT * threadContext)
{
	threadContext->hContext = hContext;
	return SCARD_S_SUCCESS;
}

static LONG MSGRemoveContext(SCARDCONTEXT hContext, SCONTEXT * threadContext)
{
	LONG rv;
	int lrv;

	if (0 == threadContext->hContext)
	{
		Log1(PCSC_LOG_ERROR, "Invalidated handle");
		return SCARD_E_INVALID_HANDLE;
	}

	if (threadContext->hContext != hContext)
		return SCARD_E_INVALID_VALUE;

	(void)pthread_mutex_lock(&threadContext->cardsList_lock);
	while (list_size(&threadContext->cardsList) != 0)
	{
		READER_CONTEXT * rContext = NULL;
		SCARDHANDLE hCard;
		void *ptr;

		/*
		 * Disconnect each of these just in case
		 */
		ptr = list_get_at(&threadContext->cardsList, 0);
		if (NULL == ptr)
		{
			Log1(PCSC_LOG_CRITICAL, "list_get_at failed");
			continue;
		}
		hCard = *(int32_t *)ptr;

		/*
		 * Unlock the sharing. If the reader or handle already
		 * disappeared, skip the disconnection part and just delete the
		 * orphan handle.
		 */
		rv = RFReaderInfoById(hCard, &rContext);
		if (rv != SCARD_S_SUCCESS && rv != SCARD_E_INVALID_VALUE
			&& rv != SCARD_E_READER_UNAVAILABLE)
		{
			(void)pthread_mutex_unlock(&threadContext->cardsList_lock);
			return rv;
		}

		if (rContext)
		{
			if (0 == rContext->hLockId)
			{
				/* no lock. Just leave the card */
				(void)SCardDisconnect(hCard, SCARD_LEAVE_CARD);
			}
			else
			{
				if (hCard != rContext->hLockId)
				{
					/*
					 * if the card is locked by someone else we do not reset it
					 */

					/* decrement card use */
					(void)SCardDisconnect(hCard, SCARD_LEAVE_CARD);
				}
				else
				{
					/* release the lock */
					rContext->hLockId = 0;

					/*
					 * We will use SCardStatus to see if the card has been
					 * reset there is no need to reset each time
					 * Disconnect is called
					 */
					rv = SCardStatus(hCard, NULL, NULL, NULL, NULL, NULL, NULL);

					if (rv == SCARD_W_RESET_CARD || rv == SCARD_W_REMOVED_CARD)
						(void)SCardDisconnect(hCard, SCARD_LEAVE_CARD);
					else
						(void)SCardDisconnect(hCard, SCARD_RESET_CARD);
				}
			}
		}

		/* Remove entry from the list */
		lrv = list_delete_at(&threadContext->cardsList, 0);
		if (lrv < 0)
			Log2(PCSC_LOG_CRITICAL,
				"list_delete_at failed with return value: %d", lrv);

		if (rContext) {
			UNREF_READER(rContext)
		}
	}
	(void)pthread_mutex_unlock(&threadContext->cardsList_lock);

	/* We only mark the context as no longer in use.
	 * The memory is freed in MSGCleanupCLient() */
	threadContext->hContext = 0;

	return SCARD_S_SUCCESS;
}

static LONG MSGAddHandle(SCARDCONTEXT hContext, SCARDHANDLE hCard,
	SCONTEXT * threadContext)
{
	LONG retval = SCARD_E_INVALID_VALUE;

	if (0 == threadContext->hContext)
	{
		Log1(PCSC_LOG_ERROR, "Invalidated handle");
		return SCARD_E_INVALID_HANDLE;
	}

	if (threadContext->hContext == hContext)
	{
		/*
		 * Find an empty spot to put the hCard value
		 */
		int listLength;

		(void)pthread_mutex_lock(&threadContext->cardsList_lock);

		listLength = list_size(&threadContext->cardsList);
		if (listLength >= contextMaxCardHandles)
		{
			Log4(PCSC_LOG_DEBUG,
				"Too many card handles for thread context @%p: %d (max is %d). "
				"Restart pcscd with --max-card-handle-per-thread value",
				threadContext, listLength, contextMaxCardHandles);
			retval = SCARD_E_NO_MEMORY;
		}
		else
		{
			int lrv;

			lrv = list_append(&threadContext->cardsList, &hCard);
			if (lrv < 0)
			{
				Log2(PCSC_LOG_CRITICAL,
					"list_append failed with return value: %d", lrv);
				retval = SCARD_E_NO_MEMORY;
			}
			else
				retval = SCARD_S_SUCCESS;
		}

		(void)pthread_mutex_unlock(&threadContext->cardsList_lock);
	}

	return retval;
}

/* Pre-condition: MSGCheckHandleAssociation must succeed. */
static LONG MSGRemoveHandle(SCARDHANDLE hCard, SCONTEXT * threadContext)
{
	int lrv;

	(void)pthread_mutex_lock(&threadContext->cardsList_lock);
	lrv = list_delete(&threadContext->cardsList, &hCard);
	(void)pthread_mutex_unlock(&threadContext->cardsList_lock);
	if (lrv < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "list_delete failed with error %d", lrv);
		return SCARD_E_INVALID_VALUE;
	}

	return SCARD_S_SUCCESS;
}


static LONG MSGCheckHandleAssociation(SCARDHANDLE hCard,
	SCONTEXT * threadContext)
{
	int list_index = 0;

	if (0 == threadContext->hContext)
	{
		/* the handle is no more valid. After SCardReleaseContext() for
		 * example */
		Log1(PCSC_LOG_CRITICAL, "Invalidated handle");
		return -1;
	}

	(void)pthread_mutex_lock(&threadContext->cardsList_lock);
	list_index = list_locate(&threadContext->cardsList, &hCard);
	(void)pthread_mutex_unlock(&threadContext->cardsList_lock);
	if (list_index >= 0)
		return 0;

	/* Must be a rogue client, debug log and sleep a couple of seconds */
	Log1(PCSC_LOG_ERROR, "Client failed to authenticate");
	(void)SYS_Sleep(2);

	return -1;
}


/* Should be called just prior to exiting the thread as it de-allocates
 * the thread memory structures
 */
static void MSGCleanupClient(SCONTEXT * threadContext)
{
	int lrv;
	int listSize;

	if (threadContext->hContext != 0)
	{
		(void)SCardReleaseContext(threadContext->hContext);
		(void)MSGRemoveContext(threadContext->hContext, threadContext);
	}

	(void)pthread_mutex_lock(&threadContext->cardsList_lock);
	list_destroy(&threadContext->cardsList);
	(void)pthread_mutex_unlock(&threadContext->cardsList_lock);

	Log3(PCSC_LOG_DEBUG,
		"Thread is stopping: dwClientID=%d, threadContext @%p",
		threadContext->dwClientID, threadContext);

	/* Clear the struct to ensure that we detect
	 * access to de-allocated memory
	 * Hopefully the compiler won't optimise it out */
	memset((void*) threadContext, 0, sizeof(SCONTEXT));
	Log2(PCSC_LOG_DEBUG, "Freeing SCONTEXT @%p", threadContext);

	(void)pthread_mutex_lock(&contextsList_lock);
	lrv = list_delete(&contextsList, threadContext);
	listSize = list_size(&contextsList);
	(void)pthread_mutex_unlock(&contextsList_lock);
	if (lrv < 0)
		Log2(PCSC_LOG_CRITICAL, "list_delete failed with error %x", lrv);

	free(threadContext);

	/* start a suicide alarm */
	if (AutoExit && (listSize < 1))
	{
		Log2(PCSC_LOG_DEBUG, "Starting suicide alarm in %d seconds",
			TIME_BEFORE_SUICIDE);
		alarm(TIME_BEFORE_SUICIDE);
	}

	return;
}
