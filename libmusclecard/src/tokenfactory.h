/*
 * This handles card abstraction attachment.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id$
 */

#ifndef __tokenfactory_h__
#define __tokenfactory_h__

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

#endif							/* __tokenfactory_h__ */
