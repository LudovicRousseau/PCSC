/******************************************************************

	Title  : winscard_msg.c
	Package: PC/SC Lite
	Author : David Corcoran
	Date   : 04/19/01
	License: Copyright (C) 2001 David Corcoran
			<corcoran@linuxnet.com>
	Purpose: This is responsible for client/server transport.

$Id$

********************************************************************/

#include "config.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
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
static int appSocket = 0;

struct _clientSockets
{
	int sd;
};

static struct _clientSockets clientSockets[PCSCLITE_MAX_APPLICATIONS];

void SHMCleanupSharedSegment(int, char *);

int SHMClientRead(psharedSegmentMsg msgStruct, int blockamount)
{
	return SHMMessageReceive(msgStruct, appSocket, blockamount);
}

int SHMClientSetupSession(int processID)
{

	struct sockaddr_un svc_addr;
	int one;

	if ((appSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		DebugLogB
			("SHMInitializeSharedSegment: Error: create on client socket: %s",
			strerror(errno));
		return -1;
	}

	svc_addr.sun_family = AF_UNIX;
	strncpy(svc_addr.sun_path, PCSCLITE_CSOCK_NAME,
		sizeof(svc_addr.sun_path));

	if (connect(appSocket, (struct sockaddr *) &svc_addr,
			sizeof(svc_addr.sun_family) + strlen(svc_addr.sun_path) + 1) <
		0)
	{
		DebugLogB
			("SHMInitializeSharedSegment: Error: connect to client socket: %s",
			strerror(errno));

		SYS_CloseFile(appSocket);
		return -1;
	}

	one = 1;
	if (ioctl(appSocket, FIONBIO, &one) < 0)
	{
		DebugLogB("SHMInitializeSharedSegment: Error: cannot set socket "
			"nonblocking: %s", strerror(errno));
		SYS_CloseFile(appSocket);
		return -1;
	}

	return 0;
}

int SHMClientCloseSession()
{
	SYS_CloseFile(appSocket);
	return 0;
}

int SHMInitializeCommonSegment()
{

	int i;
	static struct sockaddr_un serv_adr;

	for (i = 0; i < PCSCLITE_MAX_APPLICATIONS; i++)
	{
		clientSockets[i].sd = -1;
	}

	/*
	 * Create the common shared connection socket 
	 */
	if ((commonSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		DebugLogB
			("SHMInitializeSharedSegment: Unable to create common socket: %s",
			strerror(errno));
		return -1;
	}

	serv_adr.sun_family = AF_UNIX;
	strncpy(serv_adr.sun_path, PCSCLITE_CSOCK_NAME,
		sizeof(serv_adr.sun_path));
	unlink(PCSCLITE_CSOCK_NAME);

	if (bind(commonSocket, (struct sockaddr *) &serv_adr,
			sizeof(serv_adr.sun_family) + strlen(serv_adr.sun_path) + 1) <
		0)
	{
		DebugLogB
			("SHMInitializeSharedSegment: Unable to bind common socket: %s",
			strerror(errno));
		SHMCleanupSharedSegment(commonSocket, PCSCLITE_CSOCK_NAME);
		return -1;
	}

	if (listen(commonSocket, 1) < 0)
	{
		DebugLogB
			("SHMInitializeSharedSegment: Unable to listen common socket: %s",
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

int SHMProcessCommonChannelRequest()
{

	int i, clnt_len;
	int new_sock;
	struct sockaddr_un clnt_addr;
	int one;

	clnt_len = sizeof(clnt_addr);

	if ((new_sock = accept(commonSocket, (struct sockaddr *) &clnt_addr,
				&clnt_len)) < 0)
	{
		DebugLogB
			("SHMProcessCommonChannelRequest: ER: Accept on common socket: %s",
			strerror(errno));
		return -1;
	}

	for (i = 0; i < PCSCLITE_MAX_APPLICATIONS; i++)
	{
		if (clientSockets[i].sd == -1)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_APPLICATIONS)
	{
		SYS_CloseFile(new_sock);
		return -1;
	}

	clientSockets[i].sd = new_sock;

	one = 1;
	if (ioctl(clientSockets[i].sd, FIONBIO, &one) < 0)
	{
		DebugLogB("SHMInitializeSharedSegment: Error: cannot set socket "
			"nonblocking: %s", strerror(errno));
		SYS_CloseFile(clientSockets[i].sd);
		clientSockets[i].sd = -1;
		return -1;
	}

	return 0;
}

int SHMProcessEvents(psharedSegmentMsg msgStruct, int blocktime)
{

	static fd_set read_fd;
	int i, selret, largeSock, rv;
	struct timeval tv;

	largeSock = 0;

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	FD_ZERO(&read_fd);

	/*
	 * Set up the bit masks for select 
	 */
	FD_SET(commonSocket, &read_fd);
	largeSock = commonSocket;

	for (i = 0; i < PCSCLITE_MAX_APPLICATIONS; i++)
	{
		if (clientSockets[i].sd != -1)
		{
			FD_SET(clientSockets[i].sd, &read_fd);
			if (clientSockets[i].sd > largeSock)
			{
				largeSock = clientSockets[i].sd;
			}
		}
	}

	selret = select(largeSock + 1, &read_fd, (fd_set *) NULL,
		(fd_set *) NULL, &tv);

	if (selret < 0)
	{
		DebugLogB("SHMProcessEvents: Select returns with failure: %s",
			strerror(errno));
		return -1;
	}

	if (selret == 0)
		/* timeout */
		return 2;

	/*
	 * A common pipe packet has arrived - it could be a new application or 
	 * it could be a reader event packet coming from another thread 
	 */

	if (FD_ISSET(commonSocket, &read_fd))
	{
		DebugLogA("SHMProcessEvents: Common channel packet arrival");
		if (SHMProcessCommonChannelRequest() == -1)
		{
			return -1;
		} else
		{
			return 0;
		}
	}

	for (i = 0; i < PCSCLITE_MAX_APPLICATIONS; i++)
	{
		if (clientSockets[i].sd != -1)
		{
			if (FD_ISSET(clientSockets[i].sd, &read_fd))
			{
				/*
				 * Return the current handle 
				 */
				rv = SHMMessageReceive(msgStruct, clientSockets[i].sd,
					PCSCLITE_SERVER_ATTEMPTS);

				if (rv == -1)
				{	/* The client has died */
					msgStruct->mtype = CMD_CLIENT_DIED;
					msgStruct->request_id = clientSockets[i].sd;
					msgStruct->command = 0;
					SYS_CloseFile(clientSockets[i].sd);
					clientSockets[i].sd = -1;
					return 0;
				}

				/*
				 * Set the identifier handle 
				 */
				msgStruct->request_id = clientSockets[i].sd;
				return 1;
			}
		}
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
				DebugLogB
					("SHMProcessEvents: Select returns with failure: %s",
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
				DebugLogB
					("SHMProcessEvents: Select returns with failure: %s",
					strerror(errno));
				retval = -1;
				break;
			}
		}
	}

	return retval;
}

int WrapSHMWrite(unsigned int command, unsigned int pid,
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
	msgStruct.request_id = pid;
	msgStruct.date = time(NULL);
	memcpy(msgStruct.data, data, size);

	return SHMMessageSend(&msgStruct, appSocket, blockAmount);
}

void SHMCleanupSharedSegment(int sockValue, char *pcFilePath)
{
	SYS_CloseFile(sockValue);
	SYS_Unlink(pcFilePath);
}

