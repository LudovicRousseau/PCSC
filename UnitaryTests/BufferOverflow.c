#include <stdio.h>
#include <winscard.h>
#include <reader.h>

int main(void)
{
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol;
	LONG rv;
	char mszReaders[1024];
	DWORD dwReaders = sizeof(mszReaders);
	unsigned char pbAtr[265];
	DWORD dwAtrLen;
	int i;

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	printf("%lX\n", rv);

	rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
	printf("%lX\n", rv);

	rv = SCardConnect(hContext, mszReaders, SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard,
		&dwActiveProtocol);
	printf("%lX\n", rv);

	dwAtrLen = sizeof(pbAtr);
	rv = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, pbAtr, &dwAtrLen);
	printf("%lX\n", rv);

	if (SCARD_S_SUCCESS == rv)
	{
		for (i=0; i<dwAtrLen; i++)
			printf("%02X ", pbAtr[i]);
		printf("\n");
	}

	return 0;
}
