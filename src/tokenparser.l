/*
 * Reads lexical config files and updates database.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2003
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

%{

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "debuglog.h"
#include "parser.h"
#include "strlcpycat.h"

void tpevalToken(char *pcToken, int tokType);

static char *pcDesiredKey = 0;
static char pcKey[TOKEN_MAX_KEY_SIZE];
static char pcValue[TOKEN_MAX_VALUE_SIZE];
static char pcFinValue[TOKEN_MAX_VALUE_SIZE];
static int valueIndex = 0;
static int desiredIndex = 0;

void tperrorCheck (char *pcToken_error);

%}

%%

#.*                                             {}
"\n"                                            {}
\<key\>([A-Z]|[a-z]|[0-9]|[ \t])+\<\/key\>      { valueIndex = 0; tpevalToken(yytext, TOKEN_TYPE_KEY); }
[ \t]                                           {}
\<string\>([A-Z]|[a-z]|[0-9]|[ \t]|[!@#$%^&*()\-+/_\:?.,=~'"])+\<\/string\> {tpevalToken(yytext, TOKEN_TYPE_STRING); valueIndex += 1;}
.                                               { tperrorCheck(yytext); }
%%


int yywrap(void)
{
	return 1;
}


void tpevalToken(char *pcToken, int tokType)
{
	int len;
	len = 0;

	if (tokType == TOKEN_TYPE_KEY)
	{
		/* <key>foobar</key>
		 * 012345 : 5 is the first key character index */

		/* calculate the argument length */
		for (len=0; pcToken[len+5] != '<'; len++)
			;
		len++;	/* final NULL byte */

		if (len > sizeof(pcKey))
			strlcpy(pcKey, &pcToken[5], sizeof(pcKey));
		else
			strlcpy(pcKey, &pcToken[5], len);
	}

	if (tokType == TOKEN_TYPE_STRING)
	{
		/* <string>foobar</string>
		 * 012345678 : 8 is the first string character index */

		/* calculate the argument length */
		for (len=0; pcToken[len+8] != '<'; len++)
			;
		len++;	/* final NULL byte */

		if (len > sizeof(pcValue))
			strlcpy(pcValue, &pcToken[8], sizeof(pcValue));
		else
			strlcpy(pcValue, &pcToken[8], len);

		if (strcmp(pcKey, pcDesiredKey) == 0)
			if (desiredIndex == valueIndex)
				strlcpy(pcFinValue, pcValue, sizeof(pcFinValue));
	}
}

void tperrorCheck (char *token_error)
{
}

int LTPBundleFindValueWithKey(char *fileName, char *tokenKey,
                              char *tokenValue, int tokenIndice)
{
	FILE *file = NULL;
	int ret = 0;

	desiredIndex  = tokenIndice;
	pcDesiredKey  = tokenKey;
	pcFinValue[0] = '\0';

	file = fopen(fileName, "r");

	if (!file)
	{
		DebugLogC("Could not open bundle file %s: %s", fileName,
			strerror(errno));
		return 1;
	}

	yyin = file;

	do
	{
		yylex();
	} while (!feof(file));

	if (pcFinValue[0] == 0)
	{
		if (tokenIndice == 0)
		{
			/* Not defined at all */
			DebugLogC("Value/Key not defined for: %s in %s",
				tokenKey, fileName);
		}
		ret = -1;
	}
	else
		strlcpy(tokenValue, pcFinValue, TOKEN_MAX_VALUE_SIZE);

	fclose(file);
	return ret;
}

