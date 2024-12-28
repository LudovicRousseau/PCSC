/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2013 Red Hat
 *
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Author: Nikos Mavrogiannopoulos <nmav@redhat.com>
 */

/**
 * @file
 * @brief polkit authorization of clients
 *
 * IsClientAuthorized() checks whether the connecting client is authorized
 * to access the resources using polkit.
 */

#include "config.h"
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <stdio.h>
#include "debuglog.h"
#include "auth.h"

#include <errno.h>

#ifdef HAVE_POLKIT

#if defined(SO_PEERCRED) || defined(LOCAL_PEERCRED)

#include <polkit/polkit.h>
#include <stdbool.h>

#ifdef __FreeBSD__

#include <sys/ucred.h>
typedef struct xucred platform_cred;
#define	CRED_PID(uc)	(uc).cr_pid
#define	CRED_UID(uc)	(uc).cr_uid

#else

typedef struct ucred platform_cred;
#define	CRED_PID(uc)	(uc).pid
#define	CRED_UID(uc)	(uc).uid

#endif

extern bool disable_polkit;

/* Returns non zero when the client is authorized */
unsigned IsClientAuthorized(int socket, const char* action, const char* reader)
{
	platform_cred cr;
	socklen_t cr_len;
	int ret;
	PolkitSubject *subject;
	PolkitAuthority *authority;
	PolkitAuthorizationResult *result;
	PolkitDetails *details;
	GError *error = NULL;
	char action_name[128];

	if (disable_polkit)
		return 1;

	snprintf(action_name, sizeof(action_name), "org.debian.pcsc-lite.%s", action);

	cr_len = sizeof(cr);
#ifdef LOCAL_PEERCRED
	ret = getsockopt(socket, SOL_LOCAL, LOCAL_PEERCRED, &cr, &cr_len);
#else
	ret = getsockopt(socket, SOL_SOCKET, SO_PEERCRED, &cr, &cr_len);
#endif
	if (ret == -1)
	{
#ifndef NO_LOG
		int e = errno;
		Log2(PCSC_LOG_CRITICAL,
		     "Error obtaining client process credentials: %s", strerror(e));
#endif
		return 0;
	}

	authority = polkit_authority_get_sync(NULL, &error);
	if (authority == NULL)
	{
		Log2(PCSC_LOG_CRITICAL, "polkit_authority_get_sync failed: %s",
			error->message);
		g_error_free(error);
		return 0;
	}

	subject = polkit_unix_process_new_for_owner(CRED_PID(cr), 0, CRED_UID(cr));
	if (subject == NULL)
	{
		Log1(PCSC_LOG_CRITICAL, "polkit_unix_process_new_for_owner failed");
		ret = 0;
		goto cleanup1;
	}

	details = polkit_details_new();
	if (details == NULL)
	{
		Log1(PCSC_LOG_CRITICAL, "polkit_details_new failed");
		ret = 0;
		goto cleanup0;
	}

	if (reader != NULL)
		polkit_details_insert(details, "reader", reader);

	result = polkit_authority_check_authorization_sync(authority, subject,
		action_name, details,
		POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
		NULL,
		&error);

	if (result == NULL)
	{
		Log2(PCSC_LOG_CRITICAL, "Error in authorization: %s", error->message);
		g_error_free(error);
		ret = 0;
	}
	else
	{
		if (polkit_authorization_result_get_is_authorized(result))
		{
			ret = 1;
		}
		else
		{
			ret = 0;
		}
	}

	if (ret == 0)
	{
		Log4(PCSC_LOG_CRITICAL,
		     "Process %u (user: %u) is NOT authorized for action: %s",
			(unsigned)CRED_PID(cr), (unsigned)CRED_UID(cr), action);
	}

	if (result)
		g_object_unref(result);

	g_object_unref(subject);
cleanup0:
	g_object_unref(details);
cleanup1:
	g_object_unref(authority);

	return ret;
}

#else

/* Do not enable polkit if it not yet supported on your system.
 * Patches are welcome. */
#error polkit is enabled, but no socket cred implementation for this platform

#endif

#else

unsigned IsClientAuthorized(int socket, const char* action, const char* reader)
{
	(void)socket;
	(void)action;
	(void)reader;

	return 1;
}

#endif
