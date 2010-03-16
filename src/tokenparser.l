/*
 * Reads lexical config files and updates database.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2003
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2003-2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief provides LTPBundleFindValueWithKey() function on non-MacOS X
 * platforms
 */

%{

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "misc.h"
#include "debuglog.h"
#include "parser.h"
#include "strlcpycat.h"

void tpevalToken(char *pcToken, int tokType);

static const char *pcDesiredKey = NULL;
static char pcKey[TOKEN_MAX_KEY_SIZE];
static char pcValue[TOKEN_MAX_VALUE_SIZE];
static char pcFinValue[TOKEN_MAX_VALUE_SIZE];
static int valueIndex = 0;
static int desiredIndex = 0;

void tperrorCheck (char *pcToken_error);

%}

%option nounput
%option noinput
%option noyywrap

%%

#.*                                             {}
"\n"                                            {}
\<key\>([A-Z]|[a-z]|[0-9]|[ \t])+\<\/key\>      { valueIndex = 0; tpevalToken(yytext, TOKEN_TYPE_KEY); }
[ \t]                                           {}
\<string\>([A-Z]|[a-z]|[0-9]|[ \t]|[!@#$%^&*()\-+/_\:?.,=~'"])+\<\/string\> {tpevalToken(yytext, TOKEN_TYPE_STRING); valueIndex += 1;}
.                                               { tperrorCheck(yytext); }
%%


void tpevalToken(char *pcToken, int tokType)
{
	unsigned int len;
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
			(void)strlcpy(pcKey, &pcToken[5], sizeof(pcKey));
		else
			(void)strlcpy(pcKey, &pcToken[5], len);
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
			(void)strlcpy(pcValue, &pcToken[8], sizeof(pcValue));
		else
			(void)strlcpy(pcValue, &pcToken[8], len);

		if (strcmp(pcKey, pcDesiredKey) == 0)
			if (desiredIndex == valueIndex)
				(void)strlcpy(pcFinValue, pcValue, sizeof(pcFinValue));
	}
}

void tperrorCheck (char *token_error)
{
    (void)token_error;
}

/**
 * Find an optional key in a configuration file
 * No error is logged if the key is not found
 *
 * @param fileName file name
 * @param tokenKey key value
 * @param[out] tokenValue token value (if key found)
 * @param tokenIndice indice of the desired key
 * @retval -1 configuration file not found
 * @retval 0 OK
 * @retval 1 key not found
 */
int LTPBundleFindOptionalValueWithKey(const char *fileName,
	const char *tokenKey, char *tokenValue, int tokenIndice)
{
	FILE *file = NULL;
	int ret = 0;

	desiredIndex  = tokenIndice;
	pcDesiredKey  = tokenKey;
	pcFinValue[0] = '\0';

	file = fopen(fileName, "r");

	if (!file)
		return 1;

	yyin = file;

	do
	{
		(void)yylex();
	} while (!feof(file));

	if ('\0' == pcFinValue[0])
		ret = -1;
	else
		(void)strlcpy(tokenValue, pcFinValue, TOKEN_MAX_VALUE_SIZE);

	(void)fclose(file);
	return ret;
}


/**
 * Find a key in a configuration file
 *
 * @param fileName file name
 * @param tokenKey key value
 * @param[out] tokenValue token value (if key found)
 * @param tokenIndice indice of the desired key
 * @retval -1 configuration file not found
 * @retval 0 OK
 * @retval 1 key not found
 */
int LTPBundleFindValueWithKey(const char *fileName, const char *tokenKey,
                              char *tokenValue, int tokenIndice)
{
	int ret = 0;

	ret = LTPBundleFindOptionalValueWithKey(fileName, tokenKey, tokenValue,
		tokenIndice);

	if (1 == ret)
		Log3(PCSC_LOG_CRITICAL, "Could not open bundle file %s: %s",
			fileName, strerror(errno));

	if ((-1 == ret) && (0 == tokenIndice))
		/* Not defined at all */
		Log3(PCSC_LOG_CRITICAL, "Value/Key not defined for: %s in %s",
			tokenKey, fileName);

	return ret;
}

