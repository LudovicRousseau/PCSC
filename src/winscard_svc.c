/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : winscard_svc.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 03/30/01
	    License: Copyright (C) 2001 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This demarshalls functions over the message
	             queue and keeps track of clients and their
                     handles.

********************************************************************/

#include <time.h>
#include <stdio.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "winscard.h"
#include "winscard_msg.h"
#include "winscard_svc.h"
#include "debuglog.h"
#include "sys_generic.h"

static struct _psChannelMap
{
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard[PCSCLITE_MAX_CONTEXTS];
	DWORD dwClientID;
	DWORD dwHandleID;
}
psChannelMap[PCSCLITE_MAX_CHANNELS];

LONG MSGCheckHandleAssociation(DWORD, SCARDHANDLE);

/*
 * A list of local functions used to keep track of clients and their
 * connections 
 */

LONG MSGFunctionDemarshall(psharedSegmentMsg msgStruct)
{

	LONG rv;
	establish_struct *esStr;
	release_struct *reStr;
	connect_struct *coStr;
	reconnect_struct *rcStr;
	disconnect_struct *diStr;
	begin_struct *beStr;
	cancel_struct *caStr;
	end_struct *enStr;
	status_struct *stStr;
	transmit_struct *trStr;

	/*
	 * Zero out everything 
	 */
	rv = 0;

	switch (msgStruct->command)
	{

	case SCARD_ESTABLISH_CONTEXT:
		esStr = ((establish_struct *) msgStruct->data);
		esStr->rv = SCardEstablishContext(esStr->dwScope, 0, 0,
			&esStr->phContext);

		if (esStr->rv == SCARD_S_SUCCESS)
			esStr->rv =
				MSGAddContext(esStr->phContext, msgStruct->request_id);

		break;

	case SCARD_RELEASE_CONTEXT:
		reStr = ((release_struct *) msgStruct->data);
		reStr->rv = SCardReleaseContext(reStr->hContext);

		if (reStr->rv == SCARD_S_SUCCESS)
			reStr->rv =
				MSGRemoveContext(reStr->hContext, msgStruct->request_id);

		break;

	case SCARD_CONNECT:
		coStr = ((connect_struct *) msgStruct->data);
		coStr->rv = SCardConnect(coStr->hContext, coStr->szReader,
			coStr->dwShareMode, coStr->dwPreferredProtocols,
			&coStr->phCard, &coStr->pdwActiveProtocol);

		if (coStr->rv == SCARD_S_SUCCESS)
			coStr->rv =
				MSGAddHandle(coStr->hContext, msgStruct->request_id,
				coStr->phCard);

		break;

	case SCARD_RECONNECT:
		rcStr = ((reconnect_struct *) msgStruct->data);
		rv = MSGCheckHandleAssociation(msgStruct->request_id, rcStr->hCard);
		if (rv != 0) return rv;

		rcStr->rv = SCardReconnect(rcStr->hCard, rcStr->dwShareMode,
			rcStr->dwPreferredProtocols,
			rcStr->dwInitialization, &rcStr->pdwActiveProtocol);
		break;

	case SCARD_DISCONNECT:
		diStr = ((disconnect_struct *) msgStruct->data);
		rv = MSGCheckHandleAssociation(msgStruct->request_id, diStr->hCard);
		if (rv != 0) return rv;
		diStr->rv = SCardDisconnect(diStr->hCard, diStr->dwDisposition);

		if (diStr->rv == SCARD_S_SUCCESS)
			diStr->rv =
				MSGRemoveHandle(0, msgStruct->request_id, diStr->hCard);

		break;

	case SCARD_BEGIN_TRANSACTION:
		beStr = ((begin_struct *) msgStruct->data);
		rv = MSGCheckHandleAssociation(msgStruct->request_id, beStr->hCard);
		if (rv != 0) return rv;
		beStr->rv = SCardBeginTransaction(beStr->hCard);
		break;

	case SCARD_END_TRANSACTION:
		enStr = ((end_struct *) msgStruct->data);
		rv = MSGCheckHandleAssociation(msgStruct->request_id, enStr->hCard);
		if (rv != 0) return rv;
		enStr->rv =
			SCardEndTransaction(enStr->hCard, enStr->dwDisposition);
		break;

	case SCARD_CANCEL_TRANSACTION:
		caStr = ((cancel_struct *) msgStruct->data);
		rv = MSGCheckHandleAssociation(msgStruct->request_id, caStr->hCard);
		if (rv != 0) return rv;
		caStr->rv = SCardCancelTransaction(caStr->hCard);
		break;

	case SCARD_STATUS:
		stStr = ((status_struct *) msgStruct->data);
		rv = MSGCheckHandleAssociation(msgStruct->request_id, stStr->hCard);
		if (rv != 0) return rv;
		stStr->rv = SCardStatus(stStr->hCard, stStr->mszReaderNames,
			&stStr->pcchReaderLen, &stStr->pdwState,
			&stStr->pdwProtocol, stStr->pbAtr, &stStr->pcbAtrLen);

	case SCARD_TRANSMIT:
		trStr = ((transmit_struct *) msgStruct->data);
		rv = MSGCheckHandleAssociation(msgStruct->request_id, trStr->hCard);
		if (rv != 0) return rv;
		trStr->rv = SCardTransmit(trStr->hCard, &trStr->pioSendPci,
			trStr->pbSendBuffer, trStr->cbSendLength,
			&trStr->pioRecvPci, trStr->pbRecvBuffer,
			&trStr->pcbRecvLength);
		break;

	default:
		return -1;
	}

	return 0;
}

LONG MSGAddContext(SCARDCONTEXT hContext, DWORD dwClientID)
{

	int i;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].dwClientID == 0)
		{
			psChannelMap[i].hContext = hContext;
			psChannelMap[i].dwClientID = dwClientID;
			break;
		}
	}

	if (i == PCSCLITE_MAX_CHANNELS)
	{
		return SCARD_F_INTERNAL_ERROR;
	} else
	{
		return SCARD_S_SUCCESS;
	}

}

LONG MSGRemoveContext(SCARDCONTEXT hContext, DWORD dwClientID)
{

	int i, j;
	LONG rv;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].hContext == hContext &&
			psChannelMap[i].dwClientID == dwClientID)
		{

			for (j = 0; j < PCSCLITE_MAX_CONTEXTS; j++)
			{
				/*
				 * Disconnect each of these just in case 
				 */

				if (psChannelMap[i].hCard[j] != 0)
				{

					/*
					 * We will use SCardStatus to see if the card has been 
					 * reset there is no need to reset each time
					 * Disconnect is called 
					 */

					rv = SCardStatus(psChannelMap[i].hCard[j], 0, 0, 0, 0,
						0, 0);

					if (rv == SCARD_W_RESET_CARD
						|| rv == SCARD_W_REMOVED_CARD)
					{
						SCardDisconnect(psChannelMap[i].hCard[j],
							SCARD_LEAVE_CARD);
					} else
					{
						SCardDisconnect(psChannelMap[i].hCard[j],
							SCARD_RESET_CARD);
					}

					psChannelMap[i].hCard[j] = 0;
				}

				psChannelMap[i].hContext = 0;
				psChannelMap[i].dwClientID = 0;

			}

			SCardReleaseContext(hContext);
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_INVALID_VALUE;
}

LONG MSGAddHandle(SCARDCONTEXT hContext, DWORD dwClientID,
	SCARDHANDLE hCard)
{

	int i, j;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].hContext == hContext &&
			psChannelMap[i].dwClientID == dwClientID)
		{

			/*
			 * Find an empty spot to put the hCard value 
			 */
			for (j = 0; j < PCSCLITE_MAX_CONTEXTS; j++)
			{
				if (psChannelMap[i].hCard[j] == 0)
				{
					psChannelMap[i].hCard[j] = hCard;
					break;
				}
			}

			if (j == PCSCLITE_MAX_CONTEXTS)
			{
				return SCARD_F_INTERNAL_ERROR;
			} else
			{
				return SCARD_S_SUCCESS;
			}

		}

	}	/* End of for */

	return SCARD_E_INVALID_VALUE;

}

LONG MSGRemoveHandle(SCARDCONTEXT hContext, DWORD dwClientID,
	SCARDHANDLE hCard)
{

	int i, j;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].dwClientID == dwClientID)
		{
			for (j = 0; j < PCSCLITE_MAX_CONTEXTS; j++)
			{
				if (psChannelMap[i].hCard[j] == hCard)
				{
					psChannelMap[i].hCard[j] = 0;
					return SCARD_S_SUCCESS;
				}
			}
		}
	}

	return SCARD_E_INVALID_VALUE;
}


LONG MSGCheckHandleAssociation(DWORD dwClientID, SCARDHANDLE hCard)
{

	int i, j;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].dwClientID == dwClientID)
		{
			for (j = 0; j < PCSCLITE_MAX_CONTEXTS; j++)
			{
				if (psChannelMap[i].hCard[j] == hCard)
				{
				  return 0;
				}
			}
		}
	}

	/* Must be a rogue client, debug log and sleep a couple of seconds */
	DebugLogA("MSGCheckHandleAssociation: Client failed to authenticate\n");
	SYS_Sleep(2);

	return -1;
}

LONG MSGCleanupClient(psharedSegmentMsg msgStruct)
{

	int i;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].dwClientID == msgStruct->request_id)
		{
			MSGRemoveContext(psChannelMap[i].hContext,
				msgStruct->request_id);
		}
	}
	return 0;
}
