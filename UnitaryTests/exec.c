/*
    Check PC/SC file descriptor is closed after an exec
    Copyright (C) 2014   Ludovic Rousseau

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/* Unitary Test for change in revision 6978 */

#include <stdio.h>
#include <unistd.h>

#include <winscard.h>

int main(void)
{
	SCARDCONTEXT hContext;
	LONG rv;

	printf("pid: %d\n", getpid());

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardEstablishContext failed: %s\n", pcsc_stringify_error(rv));
		return -1;
	}

	/*
	rv = SCardReleaseContext(hContext);
	if (rv != SCARD_S_SUCCESS)
		return -1;
	*/

	/* the libpcsclite/pcscd socket should automatically be closed by
	 * the exec */
	printf("You should not see any _socket_ file descriptor below:\n");

	execl("/bin/sh", "sh", "-c", "ls -l /proc/self/fd", NULL);
	perror("exec");

	return 0;
}
