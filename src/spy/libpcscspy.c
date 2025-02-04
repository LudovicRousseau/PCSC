/*
    Log PC/SC arguments
    Copyright (C) 2011-2024  Ludovic Rousseau

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
#include "sys_generic.h"

#define DEBUG

#define DLSYM_DECLARE(symbol)          \
		typeof(symbol)* symbol
#define DLSYM_SET_VALUE(symbol)        \
		.symbol = (typeof(symbol)(*))internal_error

/* fake function to just return en error code */
static LONG internal_error(void)
{
	return SCARD_F_INTERNAL_ERROR;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
/* contains pointers to real functions */
static struct
{
	DLSYM_DECLARE(SCardEstablishContext);
	DLSYM_DECLARE(SCardReleaseContext);
	DLSYM_DECLARE(SCardIsValidContext);
	DLSYM_DECLARE(SCardConnect);
	DLSYM_DECLARE(SCardReconnect);
	DLSYM_DECLARE(SCardDisconnect);
	DLSYM_DECLARE(SCardBeginTransaction);
	DLSYM_DECLARE(SCardEndTransaction);
	DLSYM_DECLARE(SCardStatus);
	DLSYM_DECLARE(SCardGetStatusChange);
	DLSYM_DECLARE(SCardControl);
	DLSYM_DECLARE(SCardTransmit);
	DLSYM_DECLARE(SCardListReaderGroups);
	DLSYM_DECLARE(SCardListReaders);
	DLSYM_DECLARE(SCardFreeMemory);
	DLSYM_DECLARE(SCardCancel);
	DLSYM_DECLARE(SCardGetAttrib);
	DLSYM_DECLARE(SCardSetAttrib);
} spy = {
	/* initialized with the fake internal_error() function */
	DLSYM_SET_VALUE(SCardEstablishContext),
	DLSYM_SET_VALUE(SCardReleaseContext),
	DLSYM_SET_VALUE(SCardIsValidContext),
	DLSYM_SET_VALUE(SCardConnect),
	DLSYM_SET_VALUE(SCardReconnect),
	DLSYM_SET_VALUE(SCardDisconnect),
	DLSYM_SET_VALUE(SCardBeginTransaction),
	DLSYM_SET_VALUE(SCardEndTransaction),
	DLSYM_SET_VALUE(SCardStatus),
	DLSYM_SET_VALUE(SCardGetStatusChange),
	DLSYM_SET_VALUE(SCardControl),
	DLSYM_SET_VALUE(SCardTransmit),
	DLSYM_SET_VALUE(SCardListReaderGroups),
	DLSYM_SET_VALUE(SCardListReaders),
	DLSYM_SET_VALUE(SCardFreeMemory),
	DLSYM_SET_VALUE(SCardCancel),
	DLSYM_SET_VALUE(SCardGetAttrib),
	DLSYM_SET_VALUE(SCardSetAttrib)
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
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
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

	snprintf(threadid, sizeof threadid, "%lX@", (unsigned long)pthread_self());
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
	snprintf(threadid, sizeof threadid, "%lX@", (unsigned long)pthread_self());
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
	spy_line("<|%ld|%ld|%s|0x%08lX", profile_time.tv_sec,
		profile_time.tv_usec, fname, rv);
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

#define LIBPCSC "libpcsclite_real.so.1"

	const char *lib;

	lib = SYS_GetEnv("LIBPCSCLITE_SPY_DELEGATE");
	if (NULL == lib)
		lib = LIBPCSC;

	/* load the normal library */
	Lib_handle = dlopen(lib, RTLD_LAZY);
	if (NULL == Lib_handle)
	{
		log_line("loading \"%s\" failed: %s", lib, dlerror());
		return SCARD_F_INTERNAL_ERROR;
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

	return SCARD_S_SUCCESS;
}

static void init(void)
{
	const char *home;
	char log_pipe[128];

	/* load the real library */
	if (load_lib() != SCARD_S_SUCCESS)
		return;

	/* check if we can log */
	home = SYS_GetEnv("HOME");
	if (NULL == home)
		home = "/tmp";

	snprintf(log_pipe, sizeof log_pipe, "%s/pcsc-spy", home);
	Log_fd = open(log_pipe, O_WRONLY);
	if (Log_fd < 0)
	{
		log_line("open %s failed: %s", log_pipe, strerror(errno));
	}
}

/* exported functions */
PCSC_API LONG SCardEstablishContext(DWORD dwScope,
	LPCVOID pvReserved1,
	LPCVOID pvReserved2,
	LPSCARDCONTEXT phContext)
{
	LONG rv;
	static pthread_once_t once_control = PTHREAD_ONCE_INIT;

	pthread_once(&once_control, init);

	Enter();
	spy_long(dwScope);
	rv = spy.SCardEstablishContext(dwScope, pvReserved1, pvReserved2,
		phContext);
	spy_ptr_long(phContext);
	Quit();
	return rv;
}

PCSC_API LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	LONG rv;

	Enter();
	spy_long(hContext);
	rv = spy.SCardReleaseContext(hContext);
	Quit();
	return rv;
}

PCSC_API LONG SCardIsValidContext(SCARDCONTEXT hContext)
{
	LONG rv;

	Enter();
	spy_long(hContext);
	rv = spy.SCardIsValidContext(hContext);
	Quit();
	return rv;
}

PCSC_API LONG SCardConnect(SCARDCONTEXT hContext,
	LPCSTR szReader,
	DWORD dwShareMode,
	DWORD dwPreferredProtocols,
	LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
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

PCSC_API LONG SCardReconnect(SCARDHANDLE hCard,
	DWORD dwShareMode,
	DWORD dwPreferredProtocols,
	DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
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

PCSC_API LONG SCardDisconnect(SCARDHANDLE hCard,
	DWORD dwDisposition)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	spy_long(dwDisposition);
	rv = spy.SCardDisconnect(hCard, dwDisposition);
	Quit();
	return rv;
}

PCSC_API LONG SCardBeginTransaction(SCARDHANDLE hCard)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	rv = spy.SCardBeginTransaction(hCard);
	Quit();
	return rv;
}

PCSC_API LONG SCardEndTransaction(SCARDHANDLE hCard,
	DWORD dwDisposition)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	spy_long(dwDisposition);
	rv = spy.SCardEndTransaction(hCard, dwDisposition);
	Quit();
	return rv;
}

PCSC_API LONG SCardStatus(SCARDHANDLE hCard,
	LPSTR mszReaderName,
	LPDWORD pcchReaderLen,
	LPDWORD pdwState,
	LPDWORD pdwProtocol,
	LPBYTE pbAtr,
	LPDWORD pcbAtrLen)
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

PCSC_API LONG SCardGetStatusChange(SCARDCONTEXT hContext,
	DWORD dwTimeout,
	SCARD_READERSTATE *rgReaderStates,
	DWORD cReaders)
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

PCSC_API LONG SCardControl(SCARDHANDLE hCard,
	DWORD dwControlCode,
	LPCVOID pbSendBuffer,
	DWORD cbSendLength,
	LPVOID pbRecvBuffer,
	DWORD cbRecvLength,
	LPDWORD lpBytesReturned)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	spy_long(dwControlCode);
	spy_buffer(pbSendBuffer, cbSendLength);
	rv = spy.SCardControl(hCard, dwControlCode, pbSendBuffer, cbSendLength,
		pbRecvBuffer, cbRecvLength, lpBytesReturned);
	if (lpBytesReturned)
	{
		if (SCARD_S_SUCCESS == rv)
			spy_buffer(pbRecvBuffer, *lpBytesReturned);
		else
			spy_buffer(NULL, *lpBytesReturned);
	}
	else
		spy_buffer(NULL, 0);
	Quit();
	return rv;
}

PCSC_API LONG SCardTransmit(SCARDHANDLE hCard,
	const SCARD_IO_REQUEST *pioSendPci,
	LPCBYTE pbSendBuffer,
	DWORD cbSendLength,
	SCARD_IO_REQUEST *pioRecvPci,
	LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{
	LONG rv;

	Enter();
	spy_long(hCard);
	if (pioSendPci)
	{
		spy_long(pioSendPci->dwProtocol);
		spy_long(pioSendPci->cbPciLength);
	}
	else
	{
		spy_long(-1);
		spy_long(-1);
	}
	spy_buffer(pbSendBuffer, cbSendLength);
	rv = spy.SCardTransmit(hCard, pioSendPci, pbSendBuffer, cbSendLength,
		pioRecvPci, pbRecvBuffer, pcbRecvLength);
	if (pioRecvPci)
	{
		spy_long(pioRecvPci->dwProtocol);
		spy_long(pioRecvPci->cbPciLength);
	}
	else
	{
		spy_long(-1);
		spy_long(-1);
	}
	if (pcbRecvLength)
	{
		if (SCARD_S_SUCCESS == rv)
			spy_buffer(pbRecvBuffer, *pcbRecvLength);
		else
			spy_buffer(NULL, *pcbRecvLength);
	}
	else
		spy_buffer(NULL, 0);
	Quit();
	return rv;
}

PCSC_API LONG SCardListReaderGroups(SCARDCONTEXT hContext,
	LPSTR mszGroups,
	LPDWORD pcchGroups)
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

PCSC_API LONG SCardListReaders(SCARDCONTEXT hContext,
	LPCSTR mszGroups,
	LPSTR mszReaders,
	LPDWORD pcchReaders)
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

PCSC_API LONG SCardFreeMemory(SCARDCONTEXT hContext,
	LPCVOID pvMem)
{
	LONG rv;

	Enter();
	spy_long(hContext);
	spy_pvoid(pvMem);
	rv = spy.SCardFreeMemory(hContext, pvMem);
	Quit();
	return rv;
}

PCSC_API LONG SCardCancel(SCARDCONTEXT hContext)
{
	LONG rv;

	Enter();
	spy_long(hContext);
	rv = spy.SCardCancel(hContext);
	Quit();
	return rv;
}

PCSC_API LONG SCardGetAttrib(SCARDHANDLE hCard,
	DWORD dwAttrId,
	LPBYTE pbAttr,
	LPDWORD pcbAttrLen)
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
		if (rv != SCARD_S_SUCCESS)
			spy_buffer(NULL, *pcbAttrLen);
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

PCSC_API LONG SCardSetAttrib(SCARDHANDLE hCard,
	DWORD dwAttrId,
	LPCBYTE pbAttr,
	DWORD cbAttrLen)
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

