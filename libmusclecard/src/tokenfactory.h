/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : cardfactory.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 01/01/00
            Purpose: This handles card abstraction attachment. 
	            
********************************************************************/

#ifndef __cardfactory_h__
#define __cardfactory_h__

#include "mscdefines.h"

#ifdef __cplusplus
extern "C"
{
#endif

	MSCLong32 TPLoadToken(MSCLPTokenConnection);
	MSCLong32 TPUnloadToken(MSCLPTokenConnection);
	MSCLong32 TPBindFunctions(MSCLPTokenConnection);
	MSCLong32 TPUnbindFunctions(MSCLPTokenConnection);
	MSCLong32 TPSearchBundlesForAtr(MSCPUChar8 Atr, MSCULong32 Length,
		MSCLPTokenInfo tokenInfo);

#ifdef __cplusplus
}
#endif

#endif							/* __cardfactory_h__ */
