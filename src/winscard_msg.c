/*
 * This is responsible for client/server transport.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *
 * $Id$
 */

#include "config.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#include "wintypes.h"
#include "pcsclite.h"
#include "winscard.h"
#include "winscard_msg.h"
#include "sys_generic.h"
#include "debuglog.h"

static int commonSocket = 0;

int SHMClientRead(psharedSegmentMsg msgStruct, DWORD dwClientID, int blockamount)
{
	return SHMMessageReceive(msgStruct, dwClientID, blockamount);
}

int SHMClientSetupSession(PDWORD pdwClientID)
{

	struct sockaddr_un svc_addr;
	int one;

	if ((*pdwClientID = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		DebugLogB("SHMClientSetupSession: Error: create on client socket: %s",
			strerror(errno));
		return -1;
	}

	svc_addr.sun_family = AF_UNIX;
	strncpy(svc_addr.sun_path, PCSCLITE_CSOCK_NAME,
		sizeof(svc_addr.sun_path));

	if (connect(*pdwClientID, (struct sockaddr *) &svc_addr,
			sizeof(svc_addr.sun_family) + strlen(svc_addr.sun_path) + 1) < 0)
	{
		DebugLogB("SHMClientSetupSession: Error: connect to client socket: %s",
			strerror(errno));
		SYS_CloseFile(*pdwClientID);
		return -1;
	}

	one = 1;
	if (ioctl(*pdwClientID, FIONBIO, &one) < 0)
	{
		DebugLogB("SHMClientSetupSession: Error: cannot set socket nonblocking: %s",
			strerror(errno));
		SYS_CloseFile(*pdwClientID);
		return -1;
	}

	return 0;
}

int SHMClientCloseSession(DWORD dwClientID)
{
	SYS_CloseFile(dwClientID);
	return 0;
}

int SHMInitializeCommonSegment()
{

	static struct sockaddr_un serv_adr;

	/*
	 * Create the common shared connection socket 
	 */
	if ((commonSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		DebugLogB("SHMInitializeCommonSegment: Unable to create common socket: %s",
			strerror(errno));
		return -1;
	}

	serv_adr.sun_family = AF_UNIX;
	strncpy(serv_adr.sun_path, PCSCLITE_CSOCK_NAME,
		sizeof(serv_adr.sun_path));
	SYS_Unlink(PCSCLITE_CSOCK_NAME);

	if (bind(commonSocket, (struct sockaddr *) &serv_adr,
			sizeof(serv_adr.sun_family) + strlen(serv_adr.sun_path) + 1) < 0)
	{
		DebugLogB("SHMInitializeCommonSegment: Unable to bind common socket: %s",
			strerror(errno));
		SHMCleanupSharedSegment(commonSocket, PCSCLITE_CSOCK_NAME);
		return -1;
	}

	if (listen(commonSocket, 1) < 0)
	{
		DebugLogB("SHMInitializeCommonSegment: Unable to listen common socket: %s",
			strerror(errno));
		SHMCleanupSharedSegment(commonSocket, PCSCLITE_CSOCK_NAME);
		return -1;
	}

	/*
	 * Chmod the public entry channel 
	 */
	SYS_Chmod(PCSCLITE_CSOCK_NAME, S_IRWXO | S_IRWXG | S_IRWXU);

	return 0;
}

int SHMProcessCommonChannelRequest(PDWORD pdwClientID)
{

	int clnt_len;
	int new_sock;
	struct sockaddr_un clnt_addr;
	int one;

	clnt_len = sizeof(clnt_addr);

	if ((new_sock = accept(commonSocket, (struct sockaddr *) &clnt_addr,
				&clnt_len)) < 0)
	{
		DebugLogB("SHMProcessCommonChannelRequest: ER: Accept on common socket: %s",
			strerror(errno));
		return -1;
	}

	*pdwClientID = new_sock;

	one = 1;
	if (ioctl(*pdwClientID, FIONBIO, &one) < 0)
	{
		DebugLogB("SHMProcessCommonChannelRequest: Error: cannot set socket "
			"nonblocking: %s", strerror(errno));
		SYS_CloseFile(*pdwClientID);
		*pdwClientID = -1;
		return -1;
	}

	return 0;
}

int SHMProcessEventsServer(PDWORD pdwClientID, int blocktime)
{
	fd_set read_fd;
	int selret;
	struct timeval tv;
	
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	FD_ZERO(&read_fd);

	/*
	 * Set up the bit masks for select 
	 */
	FD_SET(commonSocket, &read_fd);

	selret = select(commonSocket + 1, &read_fd, (fd_set *) NULL,
		(fd_set *) NULL, &tv);

	if (selret < 0)
	{
		DebugLogB("SHMProcessEventsServer: Select returns with failure: %s",
			strerror(errno));
		return -1;
	}

	if (selret == 0)
		/* timeout */
		return 2;
	/*
	 * A common pipe packet has arrived - it could be a new application  
	 */
	if (FD_ISSET(commonSocket, &read_fd))
	{
		DebugLogA("SHMProcessEventsServer: Common channel packet arrival");
		if (SHMProcessCommonChannelRequest(pdwClientID) == -1)
		{
			DebugLogB("SHMProcessEventsServer: error in SHMProcessCommonChannelRequest: %d", *pdwClientID);
			return -1;
		} else
		{
			DebugLogB("SHMProcessEventsServer: SHMProcessCommonChannelRequest detects: %d", *pdwClientID);
			return 0;
		}
	}
	
	return -1;
}

int SHMProcessEventsContext(PDWORD pdwClientID, psharedSegmentMsg msgStruct, int blocktime)
{
	fd_set read_fd;
	int selret, rv;
	struct timeval tv;

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	FD_ZERO(&read_fd);
	FD_SET(*pdwClientID, &read_fd);

	selret = select(*pdwClientID + 1, &read_fd, (fd_set *) NULL,
		(fd_set *) NULL, &tv);

	if (selret < 0)
	{
		DebugLogB("SHMProcessEventsContext: Select returns with failure: %s",
			strerror(errno));
		return -1;
	}

	if (selret == 0)
		/* timeout */
		return 2;

	if (FD_ISSET(*pdwClientID, &read_fd))
	{
		/*
		 * Return the current handle 
		 */
		rv = SHMMessageReceive(msgStruct, *pdwClientID,
				       PCSCLITE_SERVER_ATTEMPTS);
		
		if (rv == -1)
		{	/* The client has died */
			DebugLogB("SHMProcessEventsContext: Client has disappeared: %d", *pdwClientID);
			msgStruct->mtype = CMD_CLIENT_DIED;
			msgStruct->command = 0;
			SYS_CloseFile(*pdwClientID);

			return 0;
		}
		
		/*
		 * Set the identifier handle 
		 */
		DebugLogB("SHMProcessEventsContext: correctly processed client: %d", *pdwClientID);
		return 1;
	}
	
	return -1;

}

int SHMMessageSend(psharedSegmentMsg msgStruct, int filedes,
	           int blockAmount)
{

	/*
	 * default is success 
	 */
	int retval = 0;
	/*
	 * record the time when we started 
	 */
	time_t start = time(0);
	/*
	 * data to be written 
	 */
	unsigned char *buffer = (unsigned char *) msgStruct;
	/*
	 * how many bytes remains to be written 
	 */
	size_t remaining = sizeof(sharedSegmentMsg);

	/*
	 * repeat until all data is written 
	 */
	while (remaining > 0)
	{
		fd_set write_fd;
		struct timeval timeout;
		int selret;

		FD_ZERO(&write_fd);
		FD_SET(filedes, &write_fd);

		timeout.tv_usec = 0;
		if ((timeout.tv_sec = start + blockAmount - time(0)) < 0)
		{
			/*
			 * we already timed out 
			 */
			retval = -1;
			break;
		}

		selret = select(filedes + 1, NULL, &write_fd, NULL, &timeout);

		/*
		 * try to write only when the file descriptor is writable 
		 */
		if (selret > 0)
		{
			int written;

			if (!FD_ISSET(filedes, &write_fd))
			{
				/*
				 * very strange situation. it should be an assert really 
				 */
				retval = -1;
				break;
			}
			written = write(filedes, buffer, remaining);

			if (written > 0)
			{
				/*
				 * we wrote something 
				 */
				buffer += written;
				remaining -= written;
			} else if (written == 0)
			{
				/*
				 * peer closed the socket 
				 */
				retval = -1;
				break;
			} else
			{
				/*
				 * we ignore the signals and socket full situations, all
				 * other errors are fatal 
				 */
				if (errno != EINTR && errno != EAGAIN)
				{
					retval = -1;
					break;
				}
			}
		} else if (selret == 0)
		{
			/*
			 * timeout 
			 */
			retval = -1;
			break;
		} else
		{
			/*
			 * ignore signals 
			 */
			if (errno != EINTR)
			{
				DebugLogB("SHMMessageSend: Select returns with failure: %s",
					strerror(errno));
				retval = -1;
				break;
			}
		}
	}

	return retval;
}

int SHMMessageReceive(psharedSegmentMsg msgStruct, int filedes,
	              int blockAmount)
{

	/*
	 * default is success 
	 */
	int retval = 0;
	/*
	 * record the time when we started 
	 */
	time_t start = time(0);
	/*
	 * buffer where we place the readed bytes 
	 */
	unsigned char *buffer = (unsigned char *) msgStruct;
	/*
	 * how many bytes we must read 
	 */
	size_t remaining = sizeof(sharedSegmentMsg);

	/*
	 * repeate until we get the whole message 
	 */
	while (remaining > 0)
	{
		fd_set read_fd;
		struct timeval timeout;
		int selret;

		FD_ZERO(&read_fd);
		FD_SET(filedes, &read_fd);

		timeout.tv_usec = 0;
		if ((timeout.tv_sec = start + blockAmount - time(0)) < 0)
		{
			/*
			 * we already timed out 
			 */
			retval = -1;
			break;
		}

		selret = select(filedes + 1, &read_fd, NULL, NULL, &timeout);

		/*
		 * try to read only when socket is readable 
		 */
		if (selret > 0)
		{
			int readed;

			if (!FD_ISSET(filedes, &read_fd))
			{
				/*
				 * very strange situation. it should be an assert really 
				 */
				retval = -1;
				break;
			}
			readed = read(filedes, buffer, remaining);

			if (readed > 0)
			{
				/*
				 * we got something 
				 */
				buffer += readed;
				remaining -= readed;
			} else if (readed == 0)
			{
				/*
				 * peer closed the socket 
				 */
				retval = -1;
				break;
			} else
			{
				/*
				 * we ignore the signals and empty socket situations, all
				 * other errors are fatal 
				 */
				if (errno != EINTR && errno != EAGAIN)
				{
					retval = -1;
					break;
				}
			}
		} else if (selret == 0)
		{
			/*
			 * timeout 
			 */
			retval = -1;
			break;
		} else
		{
			/*
			 * we ignore signals, all other errors are fatal 
			 */
			if (errno != EINTR)
			{
				DebugLogB("SHMMessageReceive: Select returns with failure: %s",
					strerror(errno));
				retval = -1;
				break;
			}
		}
	}

	return retval;
}

int WrapSHMWrite(unsigned int command, DWORD dwClientID,
	unsigned int size, unsigned int blockAmount, void *data)
{

	sharedSegmentMsg msgStruct;

	/*
	 * Set the appropriate packet parameters 
	 */

	msgStruct.mtype = CMD_FUNCTION;
	msgStruct.user_id = SYS_GetUID();
	msgStruct.group_id = SYS_GetGID();
	msgStruct.command = command;
	msgStruct.date = time(NULL);
	memcpy(msgStruct.data, data, size);

	return SHMMessageSend(&msgStruct, dwClientID, blockAmount);
}

void SHMCleanupSharedSegment(int sockValue, char *pcFilePath)
{
	SYS_CloseFile(sockValue);
	SYS_Unlink(pcFilePath);
}

