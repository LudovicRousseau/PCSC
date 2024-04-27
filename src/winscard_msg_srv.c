/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2002-2023
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
 * @brief client/server communication (on the server side only)
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
#ifdef USE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#include "misc.h"
#include "pcscd.h"
#include "winscard.h"
#include "debuglog.h"
#include "winscard_msg.h"

/**
 * Socket to a file, used for clients-server communication.
 */
static int commonSocket = 0;
extern char AraKiri;

/**
 * @brief Accepts a Client connection.
 *
 * Called by \c ProcessEventsServer().
 *
 * @param[out] pdwClientID Connection ID used to reference the Client.
 *
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 Can not establish the connection.
 */
static int ProcessCommonChannelRequest(/*@out@*/ uint32_t *pdwClientID)
{
	socklen_t clnt_len;
	int new_sock;
	struct sockaddr_un clnt_addr;

	clnt_len = sizeof(clnt_addr);

	if ((new_sock = accept(commonSocket, (struct sockaddr *) &clnt_addr,
				&clnt_len)) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Accept on common socket: %s",
			strerror(errno));
		return -1;
	}

	*pdwClientID = new_sock;

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
 * @retval -1
 * - Can not create the socket
 * - or Can not bind the socket to the file \c PCSCLITE_CSOCK_NAME
 * - or Can not put the socket in listen mode.
 */
INTERNAL int32_t InitializeSocket(void)
{
	union
	{
		struct sockaddr sa;
		struct sockaddr_un un;
	} sa;

	/*
	 * Create the common shared connection socket
	 */
	if ((commonSocket = socket(PF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Unable to create common socket: %s",
			strerror(errno));
		return -1;
	}

	memset(&sa, 0, sizeof sa);
	sa.un.sun_family = AF_UNIX;
	strncpy(sa.un.sun_path, PCSCLITE_CSOCK_NAME, sizeof sa.un.sun_path);
	(void)remove(PCSCLITE_CSOCK_NAME);

	if (bind(commonSocket, &sa.sa, sizeof sa) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Unable to bind common socket: %s",
			strerror(errno));
		return -1;
	}

	if (listen(commonSocket, 1) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Unable to listen common socket: %s",
			strerror(errno));
		return -1;
	}

	/*
	 * Chmod the public entry channel
	 */
	(void)chmod(PCSCLITE_CSOCK_NAME, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	return 0;
}

#ifdef USE_LIBSYSTEMD
/**
 * @brief Acquires a socket passed in from systemd.
 *
 * This is called by the server to start listening on an existing socket for
 * local IPC with the clients.
 *
 * @param fd The file descriptor to start listening on.
 *
 * @return Error code.
 * @retval 0 Success
 * @retval -1 Passed FD is not an UNIX socket.
 */
INTERNAL int32_t ListenExistingSocket(int fd)
{
	if (!sd_is_socket(fd, AF_UNIX, SOCK_STREAM, -1))
	{
		Log1(PCSC_LOG_CRITICAL, "Passed FD is not an UNIX socket");
		return -1;
	}

	commonSocket = fd;
	return 0;
}
#endif

/**
 * @brief Looks for messages sent by clients.
 *
 * This is called by the Server's function \c SVCServiceRunLoop().
 *
 * @param[out] pdwClientID Connection ID used to reference the Client.
 *
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 Error accessing the communication channel.
 * @retval -2 EINTR
 * @retval 2 Timeout.
 */
#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#define DO_TIMEOUT
#endif
INTERNAL int32_t ProcessEventsServer(uint32_t *pdwClientID)
{
	fd_set read_fd;
	int selret;
#ifdef DO_TIMEOUT
	struct timeval tv;

	tv.tv_sec = 1;
	tv.tv_usec = 0;
#endif

	FD_ZERO(&read_fd);

	/*
	 * Set up the bit masks for select
	 */
	FD_SET(commonSocket, &read_fd);

	selret = select(commonSocket + 1, &read_fd, (fd_set *) NULL,
		(fd_set *) NULL,
#ifdef DO_TIMEOUT
		&tv
#else
		NULL
#endif
		);

	if (selret < 0)
	{
		if (EINTR == errno)
			return -2;

		Log2(PCSC_LOG_CRITICAL, "Select returns with failure: %s",
			strerror(errno));
		return -1;
	}

	if (selret == 0)
		/* timeout. On *BSD only */
		return 2;

	/*
	 * A common pipe packet has arrived - it could be a new application
	 */
	if (FD_ISSET(commonSocket, &read_fd))
	{
		Log1(PCSC_LOG_DEBUG, "Common channel packet arrival");
		if (ProcessCommonChannelRequest(pdwClientID) == -1)
		{
			Log2(PCSC_LOG_ERROR,
				"error in ProcessCommonChannelRequest: %d", *pdwClientID);
			return -1;
		}
	}
	else
		return -1;

	Log2(PCSC_LOG_DEBUG,
		"ProcessCommonChannelRequest detects: %d", *pdwClientID);

	return 0;
}

