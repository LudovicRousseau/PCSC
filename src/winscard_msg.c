/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2002-2024
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
#include <poll.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif

#include "misc.h"
#include "pcscd.h"
#include "winscard.h"
#include "debuglog.h"
#include "winscard_msg.h"
#include "sys_generic.h"
#include "utils.h"

#ifdef PCSCD

/* functions used by pcscd only */

#else

/* functions used by libpcsclite only */

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif

#define member_size(type, member) sizeof(((type *)0)->member)

static char SocketName[member_size(struct sockaddr_un, sun_path)];
static pthread_once_t SocketName_init_control = PTHREAD_ONCE_INIT;
static void SocketName_init(void)
{
	/* socket name not yet initialized */
	const char *socketNameEnv;

	socketNameEnv = SYS_GetEnv("PCSCLITE_CSOCK_NAME");
	if (socketNameEnv)
		strncpy(SocketName, socketNameEnv, sizeof SocketName);
	else
		strncpy(SocketName, PCSCLITE_CSOCK_NAME, sizeof SocketName);

	/* Ensure a NUL byte */
	SocketName[sizeof SocketName -1] = '\0';
}

char *getSocketName(void)
{
	pthread_once(&SocketName_init_control, SocketName_init);
	return SocketName;
}

/**
 * @brief Prepares a communication channel for the client to talk to the server.
 *
 * This is called by the application to create a socket for local IPC with the
 * server. The socket is associated to the file \c PCSCLITE_CSOCK_NAME.
 *
 * @param[out] pdwClientID Client Connection ID.
 *
 * @retval 0 Success.
 * @retval -1
 * - Can not create the socket
 * - or the socket can not open a connection
 * - or can not set the socket to non-blocking.
 */
INTERNAL int ClientSetupSession(uint32_t *pdwClientID)
{
	struct sockaddr_un svc_addr;
	int ret;
	char *socketName;

	ret = socket(PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (ret < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Error: create on client socket: %s",
			strerror(errno));
		return -1;
	}
	*pdwClientID = ret;

	socketName = getSocketName();
	svc_addr.sun_family = AF_UNIX;
	strncpy(svc_addr.sun_path, socketName, sizeof(svc_addr.sun_path));

	if (connect(*pdwClientID, (struct sockaddr *) &svc_addr,
			sizeof(svc_addr.sun_family) + strlen(svc_addr.sun_path) + 1) < 0)
	{
		Log3(PCSC_LOG_CRITICAL, "Error: connect to client socket %s: %s",
			socketName, strerror(errno));
		(void)close(*pdwClientID);
		return -1;
	}

	ret = fcntl(*pdwClientID, F_GETFL, 0);
	if (ret < 0)
	{
		Log3(PCSC_LOG_CRITICAL, "Error: cannot retrieve socket %s flags: %s",
			socketName, strerror(errno));
		(void)close(*pdwClientID);
		return -1;
	}

	if (fcntl(*pdwClientID, F_SETFL, ret | O_NONBLOCK) < 0)
	{
		Log3(PCSC_LOG_CRITICAL, "Error: cannot set socket %s nonblocking: %s",
			socketName, strerror(errno));
		(void)close(*pdwClientID);
		return -1;
	}

	return 0;
}

/**
 * @brief Closes the socket used by the client to communicate with the server.
 *
 * @param[in] dwClientID Client socket handle to be closed.
 *
 */
INTERNAL void ClientCloseSession(uint32_t dwClientID)
{
	close(dwClientID);
}

/**
 * @brief Called by the Client to get the response from the server or vice-versa.
 *
 * Reads the message from the file \c filedes.
 *
 * @param[in] command one of the \ref pcsc_msg_commands commands
 * @param[out] buffer_void Message read.
 * @param[in] buffer_size Size to read
 * @param[in] filedes Socket handle.
 * @param[in] timeOut Timeout in milliseconds.
 *
 * @retval SCARD_S_SUCCESS Success.
 * @retval SCARD_E_TIMEOUT Timeout.
 * @retval SCARD_F_COMM_ERROR
 * - Socket is closed
 * - or a signal was received.
 */
INTERNAL LONG MessageReceiveTimeout(uint32_t command, void *buffer_void,
	uint64_t buffer_size, int32_t filedes, long timeOut)
{
	char *buffer = buffer_void;

	/* default is success */
	LONG retval = SCARD_S_SUCCESS;

	/* record the time when we started */
	struct timeval start;

	/* how many bytes we must read */
	size_t remaining = buffer_size;

	gettimeofday(&start, NULL);

	/* repeat until we get the whole message */
	while (remaining > 0)
	{
		struct pollfd read_fd;
		struct timeval now;
		int pollret;
		long delta;

		gettimeofday(&now, NULL);
		delta = time_sub(&now, &start) / 1000;

		if (delta > timeOut)
		{
			/* we already timed out */
			retval = SCARD_E_TIMEOUT;
			break;
		}

		/* remaining time to wait */
		delta = timeOut - delta;

		read_fd.fd = filedes;
		read_fd.events = POLLIN;
		read_fd.revents = 0;

		pollret = poll(&read_fd, 1, delta);

		/* try to read only when socket is readable */
		if (pollret > 0)
		{
			int bytes_read;

			if (!(read_fd.revents & POLLIN))
			{
				/* very strange situation. it should be an assert really */
				retval = SCARD_F_COMM_ERROR;
				break;
			}
			bytes_read = read(filedes, buffer, remaining);

			if (bytes_read > 0)
			{
				/* we got something */
				buffer += bytes_read;
				remaining -= bytes_read;
			} else if (bytes_read == 0)
			{
				/* peer closed the socket */
				retval = SCARD_F_COMM_ERROR;
				break;
			} else
			{
				/* we ignore the signals and empty socket situations, all
				 * other errors are fatal */
				if (errno != EINTR && errno != EAGAIN)
				{
					retval = SCARD_F_COMM_ERROR;
					break;
				}
			}
		} else if (pollret == 0)
		{
			/* is the daemon still there? */
			retval  =  SCardCheckDaemonAvailability();
			if (retval != SCARD_S_SUCCESS)
			{
				/* timeout */
				break;
			}

			/* you need to set the env variable PCSCLITE_DEBUG=0 since
			 * this is logged on the client side and not on the pcscd
			 * side*/
#ifdef NO_LOG
			(void)command;
#endif
			Log2(PCSC_LOG_INFO, "Command 0x%X not yet finished", command);
		} else
		{
			/* we ignore signals, all other errors are fatal */
			if (errno != EINTR)
			{
				Log2(PCSC_LOG_ERROR, "select returns with failure: %s",
					strerror(errno));
				retval = SCARD_F_COMM_ERROR;
				break;
			}
		}
	}

	return retval;
}

/**
 * @brief Wrapper for the MessageSend() function.
 *
 * Called by clients to send messages to the server.
 * The parameters \p command and \p data are set in the \c sharedSegmentMsg
 * struct in order to be sent.
 *
 * @param[in] command Command to be sent.
 * @param[in] dwClientID Client socket handle.
 * @param[in] size Size of the message (\p data).
 * @param[in] data_void Data to be sent.
 *
 * @return Same error codes as MessageSend().
 */
INTERNAL LONG MessageSendWithHeader(uint32_t command, uint32_t dwClientID,
	uint64_t size, void *data_void)
{
	struct rxHeader header;
	LONG ret;

	/* header */
	header.command = command;
	header.size = size;
	ret = MessageSend(&header, sizeof(header), dwClientID);

	/* command */
	if (size > 0)
		ret = MessageSend(data_void, size, dwClientID);

	return ret;
}

#endif

/* functions used by pcscd and libpcsclite */

/**
 * @brief Sends a menssage from client to server or vice-versa.
 *
 * Writes the message in the shared file \c filedes.
 *
 * @param[in] buffer_void Message to be sent.
 * @param[in] buffer_size Size of the message to send
 * @param[in] filedes Socket handle.
 *
 * @retval SCARD_S_SUCCESS Success
 * @retval SCARD_E_TIMEOUT Timeout.
 * @retval SCARD_F_COMM_ERROR
 * - Socket is closed
 * - or a signal was received.
 */
INTERNAL LONG MessageSend(void *buffer_void, uint64_t buffer_size,
	int32_t filedes)
{
	char *buffer = buffer_void;

	/* default is success */
	LONG retval = SCARD_S_SUCCESS;

	/* how many bytes remains to be written */
	size_t remaining = buffer_size;

	/* repeat until all data is written */
	while (remaining > 0)
	{
		struct pollfd write_fd;
		int pollret;

		write_fd.fd = filedes;
		write_fd.events = POLLOUT;
		write_fd.revents = 0;

		pollret = poll(&write_fd, 1, -1);

		/* try to write only when the file descriptor is writable */
		if (pollret > 0)
		{
			int written;

			if (!(write_fd.revents & POLLOUT))
			{
				/* very strange situation. it should be an assert really */
				retval = SCARD_F_COMM_ERROR;
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
				retval = SCARD_F_COMM_ERROR;
				break;
			} else
			{
				/* we ignore the signals and socket full situations, all
				 * other errors are fatal */
				if (errno != EINTR && errno != EAGAIN)
				{
					retval = SCARD_E_NO_SERVICE;
					break;
				}
			}
		} else if (pollret == 0)
		{
			/* timeout */
			retval = SCARD_E_TIMEOUT;
			break;
		} else
		{
			/* ignore signals */
			if (errno != EINTR)
			{
				Log2(PCSC_LOG_ERROR, "select returns with failure: %s",
					strerror(errno));
				retval = SCARD_F_COMM_ERROR;
				break;
			}
		}
	}

	return retval;
}

/**
 * @brief Called by the Client to get the response from the server or vice-versa.
 *
 * Reads the message from the file \c filedes.
 *
 * @param[out] buffer_void Message read.
 * @param[in] buffer_size Size to read
 * @param[in] filedes Socket handle.
 *
 * @retval SCARD_S_SUCCESS Success.
 * @retval SCARD_F_COMM_ERROR
 * - Socket is closed
 * - or a signal was received.
 */
INTERNAL LONG MessageReceive(void *buffer_void, uint64_t buffer_size,
	int32_t filedes)
{
	char *buffer = buffer_void;

	/* default is success */
	LONG retval = SCARD_S_SUCCESS;

	/* how many bytes we must read */
	size_t remaining = buffer_size;

	/* repeat until we get the whole message */
	while (remaining > 0)
	{
		struct pollfd read_fd;
		int pollret;

		read_fd.fd = filedes;
		read_fd.events = POLLIN;
		read_fd.revents = 0;

		pollret = poll(&read_fd, 1 , -1);

		/* try to read only when socket is readable */
		if (pollret > 0)
		{
			int bytes_read;

			if (!(read_fd.revents & POLLIN))
			{
				/* very strange situation. it should be an assert really */
				retval = SCARD_F_COMM_ERROR;
				break;
			}
			bytes_read = read(filedes, buffer, remaining);

			if (bytes_read > 0)
			{
				/* we got something */
				buffer += bytes_read;
				remaining -= bytes_read;
			} else if (bytes_read == 0)
			{
				/* peer closed the socket */
				retval = SCARD_F_COMM_ERROR;
				break;
			} else
			{
				/* we ignore the signals and empty socket situations, all
				 * other errors are fatal */
				if (errno != EINTR && errno != EAGAIN)
				{
					/* connection reset by pcscd? */
					if (ECONNRESET == errno)
						retval = SCARD_W_SECURITY_VIOLATION;
					else
						retval = SCARD_F_COMM_ERROR;
					break;
				}
			}
		}
		else
		{
			/* we ignore signals, all other errors are fatal */
			if (errno != EINTR)
			{
				Log2(PCSC_LOG_ERROR, "select returns with failure: %s",
					strerror(errno));
				retval = SCARD_F_COMM_ERROR;
				break;
			}
		}
	}

	return retval;
}

