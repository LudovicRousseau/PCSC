/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *  Ludoic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This is responsible for client/server communication.
 *
 * A file based socket (\c commonSocket) is used to send/receive only messages 
 * among clients and server.\n
 * The messages' data are passed throw a memory mapped file: \c sharedSegmentMsg.
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

#include "pcsclite.h"
#include "winscard.h"
#include "debuglog.h"
#include "winscard_msg.h"
#include "sys_generic.h"
#include "misc.h"

/**
 * Socket to a file, used for clients-server comminication.
 */
static int commonSocket = 0;

/**
 * @brief Wrapper for the \c SHMMessageReceive() function.
 *
 * Called by clients to read the server responses.
 *
 * @param[out] msgStruct Message read.
 * @param[in] dwClientID Client socket handle.
 * @param[in] blockamount Timeout in milliseconds.
 *
 * @return Error code.
 * @retval @see SHMMessageReceive().
 */
INTERNAL int SHMClientRead(psharedSegmentMsg msgStruct, DWORD dwClientID, int blockamount)
{
	return SHMMessageReceive(msgStruct, dwClientID, blockamount);
}

/**
 * @brief Prepares a communication channel for the client to talk to the server.
 *
 * This is called by the application to create a socket for local IPC with the
 * server. The socket is associated to the file \c PCSCLITE_CSOCK_NAME.
 *
 * @param[out] pdwClientID Client Connection ID.
 * 
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 Can not create the socket.
 * @retval -1 The socket can not open a connection.
 * @retval -1 Can not set the socket to non-blocking.
 */
INTERNAL int SHMClientSetupSession(PDWORD pdwClientID)
{
	struct sockaddr_un svc_addr;
	int one;

	if ((*pdwClientID = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Error: create on client socket: %s",
			strerror(errno));
		return -1;
	}

	svc_addr.sun_family = AF_UNIX;
	strncpy(svc_addr.sun_path, PCSCLITE_CSOCK_NAME,
		sizeof(svc_addr.sun_path));

	if (connect(*pdwClientID, (struct sockaddr *) &svc_addr,
			sizeof(svc_addr.sun_family) + strlen(svc_addr.sun_path) + 1) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Error: connect to client socket: %s",
			strerror(errno));
		SYS_CloseFile(*pdwClientID);
		return -1;
	}

	one = 1;
	if (ioctl(*pdwClientID, FIONBIO, &one) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Error: cannot set socket nonblocking: %s",
			strerror(errno));
		SYS_CloseFile(*pdwClientID);
		return -1;
	}

	return 0;
}

/**
 * @brief Closes the socket used by the client to communicate with the server.
 *
 * @param[in] dwClientID Client socket handle to be closed.
 *
 * @return Error code.
 * @retval 0 Success.
 */
INTERNAL int SHMClientCloseSession(DWORD dwClientID)
{
	SYS_CloseFile(dwClientID);
	return 0;
}

/**
 * @brief Prepares the communication channel used by the server to talk to the
 * clients.
 *
 * This is called by the server to create a socket for local IPC with the
 * clients. The socket is associated to the file \c PCSCLITE_CSOCK_NAME.
 * Each client will open a connection to this socket.
 * 
 * @return Error code.
 * @retval 0 Success
 * @retval -1 Can not create the socket.
 * @retval -1 Can not bind the socket to the file \c PCSCLITE_CSOCK_NAME.
 * @retval -1 Can not put the socket in listen mode.
 */
INTERNAL int SHMInitializeCommonSegment(void)
{
	static struct sockaddr_un serv_adr;

	/*
	 * Create the common shared connection socket 
	 */
	if ((commonSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Unable to create common socket: %s",
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
		Log2(PCSC_LOG_CRITICAL, "Unable to bind common socket: %s",
			strerror(errno));
		SHMCleanupSharedSegment(commonSocket, PCSCLITE_CSOCK_NAME);
		return -1;
	}

	if (listen(commonSocket, 1) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Unable to listen common socket: %s",
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

/**
 * @brief Accepts a Client connection.
 *
 * Called by \c SHMProcessEventsServer().
 *
 * @param[out] pdwClientID Connection ID used to reference the Client.
 *
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 Can not establish the connection.
 * @retval -1 Can not set the connection to non-blocking mode.
 */
INTERNAL int SHMProcessCommonChannelRequest(PDWORD pdwClientID)
{
	socklen_t clnt_len;
	int new_sock;
	struct sockaddr_un clnt_addr;
	int one;

	clnt_len = sizeof(clnt_addr);

	if ((new_sock = accept(commonSocket, (struct sockaddr *) &clnt_addr,
				&clnt_len)) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Accept on common socket: %s",
			strerror(errno));
		return -1;
	}

	*pdwClientID = new_sock;

	one = 1;
	if (ioctl(*pdwClientID, FIONBIO, &one) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Error: cannot set socket nonblocking: %s",
			strerror(errno));
		SYS_CloseFile(*pdwClientID);
		*pdwClientID = -1;
		return -1;
	}

	return 0;
}

/**
 * @brief Looks for messages sent by clients.
 *
 * This is called by the Server's function \c SVCServiceRunLoop().
 *
 * @param[out] pdwClientID Connection ID used to reference the Client.
 * @param[in] blockTime Timeout (not used).
 *
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 Error accessing the communication channel.
 * @retval -1 Can not set the connection to non-blocking mode.
 * @retval 2 Timeout.
 */
INTERNAL int SHMProcessEventsServer(PDWORD pdwClientID, int blocktime)
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
		Log2(PCSC_LOG_CRITICAL, "Select returns with failure: %s",
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
		Log1(PCSC_LOG_DEBUG, "Common channel packet arrival");
		if (SHMProcessCommonChannelRequest(pdwClientID) == -1)
		{
			Log2(PCSC_LOG_ERROR,
				"error in SHMProcessCommonChannelRequest: %d", *pdwClientID);
			return -1;
		} else
		{
			Log2(PCSC_LOG_DEBUG,
				"SHMProcessCommonChannelRequest detects: %d", *pdwClientID);
			return 0;
		}
	}
	
	return -1;
}

/**
 * @brief 
 *
 * Called by \c ContextThread().
 */
INTERNAL int SHMProcessEventsContext(PDWORD pdwClientID, psharedSegmentMsg msgStruct, int blocktime)
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
		Log2(PCSC_LOG_ERROR, "select returns with failure: %s",
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
			Log2(PCSC_LOG_DEBUG, "Client has disappeared: %d",
				*pdwClientID);
			msgStruct->mtype = CMD_CLIENT_DIED;
			msgStruct->command = 0;
			SYS_CloseFile(*pdwClientID);

			return 0;
		}
		
		/*
		 * Set the identifier handle 
		 */
		Log2(PCSC_LOG_DEBUG, "correctly processed client: %d",
			*pdwClientID);
		return 1;
	}
	
	return -1;

}

/**
 * @brief Sends a menssage from client to server or vice-versa.
 *
 * Writes the message in the shared file \c filedes.
 *
 * @param[in] msgStruct Message to be sent.
 * @param[in] filedes Socket handle.
 * @param[in] blockAmount Timeout in milliseconds.
 */
INTERNAL int SHMMessageSend(psharedSegmentMsg msgStruct, int filedes,
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
				Log2(PCSC_LOG_ERROR, "select returns with failure: %s",
					strerror(errno));
				retval = -1;
				break;
			}
		}
	}

	return retval;
}

/**
 * @brief Called by the Client to get the reponse from the server or vice-versa.
 *
 * Reads the message from the file \c filedes.
 *
 * @param[out] msgStruct Message read.
 * @param[in] filedes Socket handle.
 * @param[in] blockAmount Timeout in milliseconds.
 *
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 Timeout.
 * @retval -1 Socket is closed.
 * @retval -1 A signal was received.
 */
INTERNAL int SHMMessageReceive(psharedSegmentMsg msgStruct, int filedes,
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
				Log2(PCSC_LOG_ERROR, "select returns with failure: %s",
					strerror(errno));
				retval = -1;
				break;
			}
		}
	}

	return retval;
}

/**
 * @brief Wrapper for the \c SHMMessageSend() function.
 *
 * Called by clients to send messages to the server.
 * The parameters \p command and \p data are set in the \c sharedSegmentMsg
 * struct in order to be sent.
 *
 * @param[in] command Command to be sent.
 * @param[in] dwClientID Client socket handle.
 * @param[in] size Size of the message (\p data).
 * @param[in] blockAmount Timeout to the operation in ms.
 * @param[in] data Data to be sent.
 *
 * @return Error code.
 * @retval @see SHMMessageSend().
 */
INTERNAL int WrapSHMWrite(unsigned int command, DWORD dwClientID,
	unsigned int size, unsigned int blockAmount, void *data)
{
	sharedSegmentMsg msgStruct;

	/*
	 * Set the appropriate packet parameters 
	 */

	memset(&msgStruct, 0, sizeof(msgStruct));
	msgStruct.mtype = CMD_FUNCTION;
	msgStruct.user_id = SYS_GetUID();
	msgStruct.group_id = SYS_GetGID();
	msgStruct.command = command;
	msgStruct.date = time(NULL);
	memcpy(msgStruct.data, data, size);

	return SHMMessageSend(&msgStruct, dwClientID, blockAmount);
}

/**
 * @brief Closes the communications channel used by the server to talk to the
 * clients.
 *
 * The socket used is closed and the file it is bound to is removed.
 *
 * @param[in] sockValue Socket to be closed.
 * @param[in] pcFilePath File used by the socket.
 */
INTERNAL void SHMCleanupSharedSegment(int sockValue, char *pcFilePath)
{
	SYS_CloseFile(sockValue);
	SYS_Unlink(pcFilePath);
}

