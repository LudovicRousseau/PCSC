/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2002-2010
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
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

#include "misc.h"
#include "pcscd.h"
#include "winscard.h"
#include "debuglog.h"
#include "winscard_msg.h"

/**
 * Socket to a file, used for clients-server comminication.
 */
static int commonSocket = 0;
extern char AraKiri;

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
 */
static int SHMProcessCommonChannelRequest(/*@out@*/ uint32_t *pdwClientID)
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
 * @retval -1 Can not create the socket.
 * @retval -1 Can not bind the socket to the file \c PCSCLITE_CSOCK_NAME.
 * @retval -1 Can not put the socket in listen mode.
 */
INTERNAL int32_t SHMInitializeCommonSegment(void)
{
	static struct sockaddr_un serv_adr;

	/*
	 * Create the common shared connection socket
	 */
	if ((commonSocket = socket(PF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "Unable to create common socket: %s",
			strerror(errno));
		return -1;
	}

	serv_adr.sun_family = AF_UNIX;
	strncpy(serv_adr.sun_path, PCSCLITE_CSOCK_NAME,
		sizeof(serv_adr.sun_path));
	(void)remove(PCSCLITE_CSOCK_NAME);

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
	(void)chmod(PCSCLITE_CSOCK_NAME, S_IRWXO | S_IRWXG | S_IRWXU);

	return 0;
}

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
INTERNAL int32_t SHMProcessEventsServer(uint32_t *pdwClientID)
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
		if (SHMProcessCommonChannelRequest(pdwClientID) == -1)
		{
			Log2(PCSC_LOG_ERROR,
				"error in SHMProcessCommonChannelRequest: %d", *pdwClientID);
			return -1;
		}
	}
	else
		return -1;

	Log2(PCSC_LOG_DEBUG,
		"SHMProcessCommonChannelRequest detects: %d", *pdwClientID);

	return 0;
}

