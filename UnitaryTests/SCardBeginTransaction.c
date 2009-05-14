#include <stdio.h>
#include <unistd.h>
#include <winscard.h>

int main(void)
{
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol;
	LONG rv;
	char mszReaders[1024];
	DWORD dwReaders = sizeof(mszReaders);

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	printf("%lX\n", rv);

	rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
	printf("%lX\n", rv);

	rv = SCardConnect(hContext, mszReaders, SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard,
		&dwActiveProtocol);
	printf("%lX\n", rv);

	printf("remove/insert card\n");
	sleep(3);

	rv = SCardBeginTransaction(hCard);
	printf("%lX\n", rv);

	return 0;
}
