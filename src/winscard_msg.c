/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2002-2009
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

#include "misc.h"
#include "pcscd.h"
#include "winscard.h"
#include "debug.h"
#include "winscard_msg.h"
#include "sys_generic.h"
#include "utils.h"

/**
 * @brief Prepares a communication channel for the client to talk to the server.
 *
 * This is called by the application to create a socket for local IPC with the
 * server. The socket is associated to the file \c PCSCLITE_CSOCK_NAME.
 *
 * @param[out] pdwClientID Client Connection ID.
 *
 * @retval 0 Success.
 * @retval -1 Can not create the socket.
 * @retval -1 The socket can not open a connection.
 * @retval -1 Can not set the socket to non-blocking.
 */
INTERNAL int SHMClientSetupSession(uint32_t *pdwClientID)
{
	struct sockaddr_un svc_addr;
	int one;
	int ret;

	ret = socket(PF_UNIX, SOCK_STREAM, 0);
	if (ret < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Error: create on client socket: %s",
			strerror(errno));
		return -1;
	}
	*pdwClientID = ret;

	svc_addr.sun_family = AF_UNIX;
	strncpy(svc_addr.sun_path, PCSCLITE_CSOCK_NAME,
		sizeof(svc_addr.sun_path));

	if (connect(*pdwClientID, (struct sockaddr *) &svc_addr,
			sizeof(svc_addr.sun_family) + strlen(svc_addr.sun_path) + 1) < 0)
	{
		Log3(PCSC_LOG_CRITICAL, "Error: connect to client socket %s: %s",
			PCSCLITE_CSOCK_NAME, strerror(errno));
		(void)SYS_CloseFile(*pdwClientID);
		return -1;
	}

	one = 1;
	if (ioctl(*pdwClientID, FIONBIO, &one) < 0)
	{
		Log3(PCSC_LOG_CRITICAL, "Error: cannot set socket %s nonblocking: %s",
			PCSCLITE_CSOCK_NAME, strerror(errno));
		(void)SYS_CloseFile(*pdwClientID);
		return -1;
	}

	return 0;
}

/**
 * @brief Closes the socket used by the client to communicate with the server.
 *
 * @param[in] dwClientID Client socket handle to be closed.
 *
 * @retval 0 Success.
 */
INTERNAL int SHMClientCloseSession(uint32_t dwClientID)
{
	(void)SYS_CloseFile(dwClientID);
	return 0;
}

/**
 * @brief Sends a menssage from client to server or vice-versa.
 *
 * Writes the message in the shared file \c filedes.
 *
 * @param[in] buffer_void Message to be sent.
 * @param[in] buffer_size Size of the message to send
 * @param[in] filedes Socket handle.
 * @param[in] timeOut Timeout in milliseconds.
 *
 * @retval 0 Success
 * @retval -1 Timeout.
 * @retval -1 Socket is closed.
 * @retval -1 A signal was received.
 */
INTERNAL int32_t SHMMessageSend(void *buffer_void, uint64_t buffer_size,
	int32_t filedes, int32_t timeOut)
{
	char *buffer = buffer_void;

	/* default is success */
	int retval = 0;

	/* record the time when we started */
	struct timeval start;

	/* how many bytes remains to be written */
	size_t remaining = buffer_size;

	gettimeofday(&start, NULL);

	/* repeat until all data is written */
	while (remaining > 0)
	{
		fd_set write_fd;
		struct timeval timeout, now;
		int selret;
		long delta;

		gettimeofday(&now, NULL);
		delta = time_sub(&now, &start);

		if (delta > timeOut*1000)
		{
			/* we already timed out */
			retval = -1;
			break;
		}

		/* remaining time to wait */
		delta = timeOut*1000 - delta;

		FD_ZERO(&write_fd);
		FD_SET(filedes, &write_fd);

		timeout.tv_sec = delta/1000000;
		timeout.tv_usec = delta - timeout.tv_sec*1000000;

		selret = select(filedes + 1, NULL, &write_fd, NULL, &timeout);

		/* try to write only when the file descriptor is writable */
		if (selret > 0)
		{
			int written;

			if (!FD_ISSET(filedes, &write_fd))
			{
				/* very strange situation. it should be an assert really */
				retval = -1;
				break;
			}
			/* since we are a user library we can't play with signals
			 * The signals may already be used by the application */
#ifdef MSG_NOSIGNAL
			/* Get EPIPE return code instead of SIGPIPE signal
			 * Works on Linux */
			written = send(filedes, buffer, remaining, MSG_NOSIGNAL);
#else
			/* we may get a SIGPIPE signal if the other side has closed */
			written = write(filedes, buffer, remaining);
#endif

			if (written > 0)
			{
				/* we wrote something */
				buffer += written;
				remaining -= written;
			} else if (written == 0)
			{
				/* peer closed the socket */
				retval = -1;
				break;
			} else
			{
				/* we ignore the signals and socket full situations, all
				 * other errors are fatal */ 
				if (errno != EINTR && errno != EAGAIN)
				{
					retval = -1;
					break;
				}
			}
		} else if (selret == 0)
		{
			/* timeout */
			retval = -1;
			break;
		} else
		{
			/* ignore signals */
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
 * @param[out] buffer_void Message read.
 * @param[in] buffer_size Size to read
 * @param[in] filedes Socket handle.
 * @param[in] timeOut Timeout in milliseconds.
 *
 * @retval 0 Success.
 * @retval -1 Timeout.
 * @retval -1 Socket is closed.
 * @retval -1 A signal was received.
 */
INTERNAL int32_t SHMMessageReceive(uint32_t command, void *buffer_void,
	uint64_t buffer_size, int32_t filedes, int32_t timeOut)
{
	char *buffer = buffer_void;

	/* default is success */
	int retval = 0;

	/* record the time when we started */
	struct timeval start;

	/* how many bytes we must read */
	size_t remaining = buffer_size;

	gettimeofday(&start, NULL);

	/* repeat until we get the whole message */
	while (remaining > 0)
	{
		fd_set read_fd;
		struct timeval timeout, now;
		int selret;
		long delta;

		gettimeofday(&now, NULL);
		delta = time_sub(&now, &start);

		if (delta > timeOut*1000)
		{
			/* we already timed out */
			retval = -2;
			break;
		}

		/* remaining time to wait */
		delta = timeOut*1000 - delta;

		FD_ZERO(&read_fd);
		FD_SET(filedes, &read_fd);

		timeout.tv_sec = delta/1000000;
		timeout.tv_usec = delta - timeout.tv_sec*1000000;

		selret = select(filedes + 1, &read_fd, NULL, NULL, &timeout);

		/* try to read only when socket is readable */
		if (selret > 0)
		{
			int readed;

			if (!FD_ISSET(filedes, &read_fd))
			{
				/* very strange situation. it should be an assert really */
				retval = -1;
				break;
			}
			readed = read(filedes, buffer, remaining);

			if (readed > 0)
			{
				/* we got something */
				buffer += readed;
				remaining -= readed;
			} else if (readed == 0)
			{
				/* peer closed the socket */
				retval = -1;
				break;
			} else
			{
				/* we ignore the signals and empty socket situations, all
				 * other errors are fatal */
				if (errno != EINTR && errno != EAGAIN)
				{
					retval = -1;
					break;
				}
			}
		} else if (selret == 0)
		{
#ifdef PCSCD
			/* timeout */
			retval = -1;
			break;
#else
			/* is the daemon still there? */
			if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
			{
				/* timeout */
				retval = -1;
				break;
			}

			/* you need to set the env variable PCSCLITE_DEBUG=0 since
			 * this is logged on the client side and not on the pcscd
			 * side*/
			Log2(PCSC_LOG_INFO, "Command 0x%X not yet finished", command);
#endif
		} else
		{
			/* we ignore signals, all other errors are fatal */
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
 * @brief Wrapper for the SHMMessageSend() function.
 *
 * Called by clients to send messages to the server.
 * The parameters \p command and \p data are set in the \c sharedSegmentMsg
 * struct in order to be sent.
 *
 * @param[in] command Command to be sent.
 * @param[in] dwClientID Client socket handle.
 * @param[in] size Size of the message (\p data).
 * @param[in] timeOut Timeout to the operation in ms.
 * @param[in] data_void Data to be sent.
 *
 * @return Same error codes as SHMMessageSend().
 */
INTERNAL int32_t SHMMessageSendWithHeader(uint32_t command, uint32_t dwClientID,
	uint64_t size, uint32_t timeOut, void *data_void)
{
	struct rxHeader header;
	int ret;

	/* header */
	header.command = command;
	header.size = size;
	ret = SHMMessageSend(&header, sizeof(header), dwClientID, timeOut);

	/* command */
	ret = SHMMessageSend(data_void, size, dwClientID, timeOut);

	return ret;
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
INTERNAL void SHMCleanupSharedSegment(int sockValue, const char *pcFilePath)
{
	(void)SYS_CloseFile(sockValue);
	(void)SYS_RemoveFile(pcFilePath);
}

