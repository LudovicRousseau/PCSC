/*
    Log PC/SC arguments
    Copyright (C) 2011-2013  Ludovic Rousseau

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <dlfcn.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

#include "misc.h"
#include <winscard.h>

#define DEBUG

/* function prototypes */

#define p_SCardEstablishContext(fct) LONG(fct)(DWORD dwScope, LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext)

#define p_SCardReleaseContext(fct) LONG(fct)(SCARDCONTEXT hContext)

#define p_SCardIsValidContext(fct) LONG(fct) (SCARDCONTEXT hContext)

#define p_SCardConnect(fct) LONG(fct) (SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)

#define p_SCardReconnect(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwShareMode, DWORD dwPreferredProtocols, DWORD dwInitialization, LPDWORD pdwActiveProtocol)

#define p_SCardDisconnect(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwDisposition)

#define p_SCardBeginTransaction(fct) LONG(fct) (SCARDHANDLE hCard)

#define p_SCardEndTransaction(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwDisposition)

#define p_SCardStatus(fct) LONG(fct) (SCARDHANDLE hCard, LPSTR mszReaderName, LPDWORD pcchReaderLen, LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)

#define p_SCardGetStatusChange(fct) LONG(fct) (SCARDCONTEXT hContext, DWORD dwTimeout, LPSCARD_READERSTATE rgReaderStates, DWORD cReaders)

#define p_SCardControl(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID pbSendBuffer, DWORD cbSendLength, LPVOID pbRecvBuffer, DWORD cbRecvLength, LPDWORD lpBytesReturned)

#define p_SCardTransmit(fct) LONG(fct) (SCARDHANDLE hCard, const SCARD_IO_REQUEST * pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength, SCARD_IO_REQUEST * pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)

#define p_SCardListReaderGroups(fct) LONG(fct) (SCARDCONTEXT hContext, LPSTR mszGroups, LPDWORD pcchGroups)

#define p_SCardListReaders(fct) LONG(fct) (SCARDCONTEXT hContext, LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders)

#define p_SCardFreeMemory(fct) LONG(fct) (SCARDCONTEXT hContext, LPCVOID pvMem)

#define p_SCardCancel(fct) LONG(fct) (SCARDCONTEXT hContext)

#define p_SCardGetAttrib(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen)

#define p_SCardSetAttrib(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr, DWORD cbAttrLen)

#define p_pcsc_stringify_error(fct) const char *(fct)(const LONG pcscError)

/* fake function to just return en error code */
static LONG internal_error(void)
{
	return SCARD_F_INTERNAL_ERROR;
}

static const char * internal_stringify_error(void)
{
	return "No spy pcsc_stringify_error() function";
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
/* contains pointers to real functions */
static struct
{
	p_SCardEstablishContext(*SCardEstablishContext);
	p_SCardReleaseContext(*SCardReleaseContext);
	p_SCardIsValidContext(*SCardIsValidContext);
	p_SCardConnect(*SCardConnect);
	p_SCardReconnect(*SCardReconnect);
	p_SCardDisconnect(*SCardDisconnect);
	p_SCardBeginTransaction(*SCardBeginTransaction);
	p_SCardEndTransaction(*SCardEndTransaction);
	p_SCardStatus(*SCardStatus);
	p_SCardGetStatusChange(*SCardGetStatusChange);
	p_SCardControl(*SCardControl);
	p_SCardTransmit(*SCardTransmit);
	p_SCardListReaderGroups(*SCardListReaderGroups);
	p_SCardListReaders(*SCardListReaders);
	p_SCardFreeMemory(*SCardFreeMemory);
	p_SCardCancel(*SCardCancel);
	p_SCardGetAttrib(*SCardGetAttrib);
	p_SCardSetAttrib(*SCardSetAttrib);
	p_pcsc_stringify_error(*pcsc_stringify_error);
} spy = {
	/* initialized with the fake internal_error() function */
	.SCardEstablishContext = (p_SCardEstablishContext(*))internal_error,
	.SCardReleaseContext = (p_SCardReleaseContext(*))internal_error,
	.SCardIsValidContext = (p_SCardIsValidContext(*))internal_error,
	.SCardConnect = (p_SCardConnect(*))internal_error,
	.SCardReconnect = (p_SCardReconnect(*))internal_error,
	.SCardDisconnect = (p_SCardDisconnect(*))internal_error,
	.SCardBeginTransaction = (p_SCardBeginTransaction(*))internal_error,
	.SCardEndTransaction = (p_SCardEndTransaction(*))internal_error,
	.SCardStatus = (p_SCardStatus(*))internal_error,
	.SCardGetStatusChange = (p_SCardGetStatusChange(*))internal_error,
	.SCardControl = (p_SCardControl(*))internal_error,
	.SCardTransmit = (p_SCardTransmit(*))internal_error,
	.SCardListReaderGroups = (p_SCardListReaderGroups(*))internal_error,
	.SCardListReaders = (p_SCardListReaders(*))internal_error,
	.SCardFreeMemory = (p_SCardFreeMemory(*))internal_error,
	.SCardCancel = (p_SCardCancel(*))internal_error,
	.SCardGetAttrib = (p_SCardGetAttrib(*))internal_error,
	.SCardSetAttrib = (p_SCardSetAttrib(*))internal_error,
	.pcsc_stringify_error = (p_pcsc_stringify_error(*))internal_stringify_error
};
#pragma GCC diagnostic pop

#define LOG log_line("%s:%d", __FILE__, __LINE__)

static int Log_fd = -1;
static void *Lib_handle = NULL;
static pthread_mutex_t Log_fd_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef DEBUG
static void log_line(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
}
#else
static void log_line(const char *fmt, ...)
{
}
#endif

static void spy_line_direct(char *line)
{
	char threadid[30];
	ssize_t r;

	/* spying disabled */
	if (Log_fd < 0)
		return;

	snprintf(threadid, sizeof threadid, "%lX@", pthread_self());
	pthread_mutex_lock(&Log_fd_mutex);
	r = write(Log_fd, threadid, strlen(threadid));
	r = write(Log_fd, line, strlen(line));
	r = write(Log_fd, "\n", 1);
	(void)r;
	pthread_mutex_unlock(&Log_fd_mutex);
}

static void spy_line(const char *fmt, ...)
{
	va_list args;
	char line[256];
	int size;
	char threadid[30];
	ssize_t r;

	/* spying disabled */
	if (Log_fd < 0)
		return;

	va_start(args, fmt);
	size = vsnprintf(line, sizeof line, fmt, args);
	va_end(args);
	if ((size_t)size >= sizeof line)
	{
		printf("libpcsc-spy: Buffer is too small!\n");
		return;
	}
	snprintf(threadid, sizeof threadid, "%lX@", pthread_self());
	pthread_mutex_lock(&Log_fd_mutex);
	r = write(Log_fd, threadid, strlen(threadid));
	r = write(Log_fd, line, size);
	r = write(Log_fd, "\n", 1);
	(void)r;
	pthread_mutex_unlock(&Log_fd_mutex);
}

static void spy_enter(const char *fname)
{
	struct timeval profile_time;

	gettimeofday(&profile_time, NULL);
	spy_line(">|%ld|%ld|%s", profile_time.tv_sec, profile_time.tv_usec, fname);
}

static void spy_quit(const char *fname, LONG rv)
{
	struct timeval profile_time;

	gettimeofday(&profile_time, NULL);
	spy_line("<|%ld|%ld|%s|%s|0x%08lX", profile_time.tv_sec,
		profile_time.tv_usec, fname, spy.pcsc_stringify_error(rv), rv);
}

#define Enter() spy_enter(__FUNCTION__)
#define Quit() spy_quit(__FUNCTION__, rv)

static void spy_long(long arg)
{
	spy_line("0x%08lX", arg);
}

static void spy_ptr_long(LONG *arg)
{
	if (arg)
		spy_line("0x%08lX", *arg);
	else
		spy_line("NULL");
}

static void spy_ptr_ulong(ULONG *arg)
{
	if (arg)
		spy_line("0x%08lX", *arg);
	else
		spy_line("NULL");
}

static void spy_pvoid(const void *ptr)
{
	spy_line("%p", ptr);
}

static void spy_buffer(const unsigned char *buffer, size_t length)
{
	spy_long(length);

	if (NULL == buffer)
		spy_line("NULL");
	else
	{
		/* "78 79 7A" */
		char log_buffer[length * 3 +1], *p;
		size_t i;

		p = log_buffer;
		log_buffer[0] = '\0';
		for (i=0; i<length; i++)
		{
			snprintf(p, 4, "%02X ", buffer[i]);
			p += 3;
		}
		*p = '\0';

		spy_line_direct(log_buffer);
	}
}

static void spy_str(const char *str)
{
	spy_line("%s", str);
}

static void spy_n_str(const char *str, ULONG *len, int autoallocate)
{
	spy_ptr_ulong(len);
	if (NULL == len)
	{
		spy_line("\"\"");
	}
	else
	{
		if (NULL == str)
		{
			spy_line("NULL");
		}
		else
		{
			const char *s = str;
			unsigned int length = 0;

			if (autoallocate)
				s = *(char **)str;

			do
			{
				spy_line("%s", s);
				length += strlen(s)+1;
				s += strlen(s)+1;
			} while(length < *len);
		}
	}
}


static void spy_readerstate(SCARD_READERSTATE * rgReaderStates, int cReaders)
{
	int i;

	for (i=0; i<cReaders; i++)
	{
		spy_str(rgReaderStates[i].szReader);
		spy_long(rgReaderStates[i].dwCurrentState);
		spy_long(rgReaderStates[i].dwEventState);
		if (rgReaderStates[i].cbAtr <= MAX_ATR_SIZE)
			spy_buffer(rgReaderStates[i].rgbAtr, rgReaderStates[i].cbAtr);
		else
			spy_buffer(NULL, rgReaderStates[i].cbAtr);
	}
}

static LONG load_lib(void)
{

#define LIBPCSC_NOSPY "libpcsclite_nospy.so.1"
#define LIBPCSC "libpcsclite.so.1"

	/* first try to load the NOSPY library
	 * this is used for programs doing an explicit dlopen like
	 * Perl and Python wrappers */
	Lib_handle = dlopen(LIBPCSC_NOSPY, RTLD_LAZY);
	if (NULL == Lib_handle)
	{
		log_line("%s", dlerror());

		/* load the normal library */
		Lib_handle = dlopen(LIBPCSC, RTLD_LAZY);
		if (NULL == Lib_handle)
		{
			log_line("%s", dlerror());
			return SCARD_F_INTERNAL_ERROR;
		}
	}

#define get_symbol(s) do { spy.s = dlsym(Lib_handle, #s); if (NULL == spy.s) { log_line("%s", dlerror()); return SCARD_F_INTERNAL_ERROR; } } while (0)

	if (SCardEstablishContext == dlsym(Lib_handle, "SCardEstablishContext"))
	{
		log_line("Symbols dlsym error");
		return SCARD_F_INTERNAL_ERROR;
	}

	get_symbol(SCardEstablishContext);
	get_symbol(SCardReleaseContext);
	get_symbol(SCardIsValidContext);
	get_symbol(SCardConnect);
	get_symbol(SCardReconnect);
	get_symbol(SCardDisconnect);
	get_symbol(SCardBeginTransaction);
	get_symbol(SCardEndTransaction);
	get_symbol(SCardStatus);
	get_symbol(SCardGetStatusChange);
	get_symbol(SCardControl);
	get_symbol(SCardTransmit);
	get_symbol(SCardListReaderGroups);
	get_symbol(SCardListReaders);
	/* Mac OS X do not have SCardFreeMemory() */
	if (dlsym(Lib_handle, "SCardFreeMemory"))
		get_symbol(SCardFreeMemory);
	get_symbol(SCardCancel);
	get_symbol(SCardGetAttrib);
	get_symbol(SCardSetAttrib);
	get_symbol(pcsc_stringify_error);

	return SCARD_S_SUCCESS;
}


/* exported functions */
PCSC_API p_SCardEstablishContext(SCardEstablishContext)
{
	LONG rv;
	static int init = 0;

	if (!init)
	{
		const char *home;
		char log_pipe[128];

		init = 1;

		/* load the real library */
		rv = load_lib();
		if (rv != SCARD_S_SUCCESS)
			return rv;

		/* check if we can log */
		home = getenv("HOME");
		if (NULL == home)
			home = "/tmp";

		snprintf(log_pipe, sizeof log_pipe, "%s/pcsc-spy", home);
		Log_fd = open(log_pipe, O_WRONLY);
		if (Log_fd < 0)
		{
			log_line("open %s failed: %s", log_pipe, strerror(errno));
		}
	}

	Enter();
	spy_long(dwScope);
	rv = spy.SCardEstablishContext(dwScope, pvReserved1, pvReserved2,
		phContext);
	spy_ptr_long(phContext);
	Quit();
	return rv;
}

PCSC_API p_SCardReleaseContext(SCardReleaseContext)
{
	LONG rv;

	Enter();
	spy_long(hContext);
	rv = spy.SCardReleaseContext(hContext);
	Quit();
	return rv;
}

PCSC_API p_SCardIsValidContext(SCardIsValidContext)
{
	LONG rv;

	Enter();
	spy_long(hContext);
	rv = spy.SCardIsValidContext(hContext);
	Quit();
	return rv;
}

PCSC_API p_SCardConnect(SCardConnect)
{
	LONG rv;

	Enter();
	spy_long(hContext);
	spy_str(szReader);
	spy_long(dwShareMode);
	spy_long(dwPreferredProtocols);
	spy_ptr_long(phCard);
	spy_ptr_ulong(pdwActiveProtocol);
	rv = spy.SCardConnect(hContext, szReader, dwShareMode,
		dwPreferredProtocols, phCard, pdwActiveProtocol);
	spy_ptr_long(phCard);
	spy_ptr_ulong(pdwActiveProtocol);
	Quit();
	return rv;
}

PCSC_API p_SCardReconnect(SCardReconnect)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	spy_long(dwShareMode);
	spy_long(dwPreferredProtocols);
	spy_long(dwInitialization);
	rv = spy.SCardReconnect(hCard, dwShareMode, dwPreferredProtocols,
		dwInitialization, pdwActiveProtocol);
	spy_ptr_ulong(pdwActiveProtocol);
	Quit();
	return rv;
}

PCSC_API p_SCardDisconnect(SCardDisconnect)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	spy_long(dwDisposition);
	rv = spy.SCardDisconnect(hCard, dwDisposition);
	Quit();
	return rv;
}

PCSC_API p_SCardBeginTransaction(SCardBeginTransaction)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	rv = spy.SCardBeginTransaction(hCard);
	Quit();
	return rv;
}

PCSC_API p_SCardEndTransaction(SCardEndTransaction)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	spy_long(dwDisposition);
	rv = spy.SCardEndTransaction(hCard, dwDisposition);
	Quit();
	return rv;
}

PCSC_API p_SCardStatus(SCardStatus)
{
	LONG rv;
	int autoallocate_ReaderName = 0, autoallocate_Atr = 0;

	if (pcchReaderLen)
		autoallocate_ReaderName = *pcchReaderLen == SCARD_AUTOALLOCATE;

	if (pcbAtrLen)
		autoallocate_Atr = *pcbAtrLen == SCARD_AUTOALLOCATE;

	Enter();
	spy_long(hCard);
	spy_ptr_ulong(pcchReaderLen);
	spy_ptr_ulong(pcbAtrLen);
	rv = spy.SCardStatus(hCard, mszReaderName, pcchReaderLen, pdwState,
		pdwProtocol, pbAtr, pcbAtrLen);
	spy_n_str(mszReaderName, pcchReaderLen, autoallocate_ReaderName);
	spy_ptr_ulong(pdwState);
	spy_ptr_ulong(pdwProtocol);
	if (NULL == pcbAtrLen)
		spy_line("NULL");
	else
	{
		LPBYTE buffer;

		if (autoallocate_Atr)
			buffer = *(LPBYTE *)pbAtr;
		else
			buffer = pbAtr;

		spy_buffer(buffer, *pcbAtrLen);
	}
	Quit();
	return rv;
}

PCSC_API p_SCardGetStatusChange(SCardGetStatusChange)
{
	LONG rv;

	Enter();
	spy_long(hContext);
	spy_long(dwTimeout);
	spy_long(cReaders);
	spy_readerstate(rgReaderStates, cReaders);
	rv = spy.SCardGetStatusChange(hContext, dwTimeout, rgReaderStates,
		cReaders);
	spy_readerstate(rgReaderStates, cReaders);
	Quit();
	return rv;
}

PCSC_API p_SCardControl(SCardControl)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	spy_long(dwControlCode);
	spy_buffer(pbSendBuffer, cbSendLength);
	rv = spy.SCardControl(hCard, dwControlCode, pbSendBuffer, cbSendLength,
		pbRecvBuffer, cbRecvLength, lpBytesReturned);
	if (lpBytesReturned)
		spy_buffer(pbRecvBuffer, *lpBytesReturned);
	else
		spy_buffer(NULL, 0);
	Quit();
	return rv;
}

PCSC_API p_SCardTransmit(SCardTransmit)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	spy_buffer(pbSendBuffer, cbSendLength);
	rv = spy.SCardTransmit(hCard, pioSendPci, pbSendBuffer, cbSendLength,
		pioRecvPci, pbRecvBuffer, pcbRecvLength);
	if (pcbRecvLength)
		spy_buffer(pbRecvBuffer, *pcbRecvLength);
	else
		spy_buffer(NULL, 0);
	Quit();
	return rv;
}

PCSC_API p_SCardListReaderGroups(SCardListReaderGroups)
{
	LONG rv;
	int autoallocate = 0;

	if (pcchGroups)
		autoallocate = *pcchGroups == SCARD_AUTOALLOCATE;

	Enter();
	spy_long(hContext);
	spy_ptr_ulong(pcchGroups);
	rv = spy.SCardListReaderGroups(hContext, mszGroups, pcchGroups);
	if (SCARD_S_SUCCESS == rv)
		spy_n_str(mszGroups, pcchGroups, autoallocate);
	else
		spy_n_str(NULL, pcchGroups, 0);
	Quit();
	return rv;
}

PCSC_API p_SCardListReaders(SCardListReaders)
{
	LONG rv;
	int autoallocate = 0;

	if (pcchReaders)
		autoallocate = *pcchReaders == SCARD_AUTOALLOCATE;

	Enter();
	spy_long(hContext);
	spy_str(mszGroups);
	rv = spy.SCardListReaders(hContext, mszGroups, mszReaders, pcchReaders);
	if (SCARD_S_SUCCESS == rv)
		spy_n_str(mszReaders, pcchReaders, autoallocate);
	else
		spy_n_str(NULL, pcchReaders, 0);
	Quit();
	return rv;
}

PCSC_API p_SCardFreeMemory(SCardFreeMemory)
{
	LONG rv;

	Enter();
	spy_long(hContext);
	spy_pvoid(pvMem);
	rv = spy.SCardFreeMemory(hContext, pvMem);
	Quit();
	return rv;
}

PCSC_API p_SCardCancel(SCardCancel)
{
	LONG rv;

	Enter();
	spy_long(hContext);
	rv = spy.SCardCancel(hContext);
	Quit();
	return rv;
}

PCSC_API p_SCardGetAttrib(SCardGetAttrib)
{
	LONG rv;
	int autoallocate = 0;

	if (pcbAttrLen)
		autoallocate = *pcbAttrLen == SCARD_AUTOALLOCATE;

	Enter();
	spy_long(hCard);
	spy_long(dwAttrId);
	rv = spy.SCardGetAttrib(hCard, dwAttrId, pbAttr, pcbAttrLen);
	if (NULL == pcbAttrLen)
		spy_buffer(NULL, 0);
	else
	{
		LPBYTE buffer;

		if (autoallocate)
			buffer = *(LPBYTE *)pbAttr;
		else
			buffer = pbAttr;

		spy_buffer(buffer, *pcbAttrLen);
	}
	Quit();
	return rv;
}

PCSC_API p_SCardSetAttrib(SCardSetAttrib)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	spy_long(dwAttrId);
	spy_buffer(pbAttr, cbAttrLen);
	rv = spy.SCardSetAttrib(hCard, dwAttrId, pbAttr, cbAttrLen);
	Quit();
	return rv;
}

PCSC_API p_pcsc_stringify_error(pcsc_stringify_error)
{
	return spy.pcsc_stringify_error(pcscError);
}

PCSC_API const SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, sizeof(SCARD_IO_REQUEST) };
PCSC_API const SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, sizeof(SCARD_IO_REQUEST) };
PCSC_API const SCARD_IO_REQUEST g_rgSCardRawPci = { SCARD_PROTOCOL_RAW, sizeof(SCARD_IO_REQUEST) };
