/*****************************************************************

  File   :   tokenparser.l
  Author :   David Corcoran
  Date   :   February 12, 1999, modified August 2003
  Purpose:   Reads lexical config files and updates database.
             See http://www.linuxnet.com for more information.
  License:   Copyright (C) 1999-2003 David Corcoran <corcoran@linuxnet.com>
             Ludovic Rousseau <ludovic.rousseau@free.fr>
$Id$

******************************************************************/

%{

#include "config.h"
#include <stdio.h>
#include <string.h>

#include "debuglog.h"
#include "parser.h"

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
[ \t]                     		        {}
\<string\>([A-Z]|[a-z]|[0-9]|[ \t]|[!@#$%^&*()\-+/_\:?.,=~'"])+\<\/string\> {tpevalToken(yytext, TOKEN_TYPE_STRING); valueIndex += 1;}
.                                               { tperrorCheck( yytext ); }
%%


int yywrap()
{
	return 1;
}


void tpevalToken(char *pcToken, int tokType)
{
	int len;
	len = 0;

	if (tokType == TOKEN_TYPE_KEY)
	{
		for (len=5; pcToken[len] != '<'; len++)
			;
		if (len - 5 > TOKEN_MAX_KEY_SIZE)
		{
			strncpy(pcKey, &pcToken[5], TOKEN_MAX_KEY_SIZE);
			pcKey[TOKEN_MAX_KEY_SIZE - 1] = '\0';
		}
		else
		{
			strncpy(pcKey, &pcToken[5], len - 5);
			pcKey[len-5] = 0;
		}
	}

	if (tokType == TOKEN_TYPE_STRING)
	{
		for (len=8; pcToken[len] != '<'; len++)
			;
		if (len - 8 > TOKEN_MAX_VALUE_SIZE)
		{
			strncpy(pcValue, &pcToken[8], TOKEN_MAX_VALUE_SIZE);
			pcValue[TOKEN_MAX_VALUE_SIZE - 1] = '\0';
		}
		else
		{
			strncpy(pcValue, &pcToken[8], len - 8);
			pcValue[len-8] = 0;
		}
		if (strcmp(pcKey, pcDesiredKey) == 0)
			if (desiredIndex == valueIndex)
			{
				strncpy(pcFinValue, pcValue, TOKEN_MAX_VALUE_SIZE);
				pcFinValue[TOKEN_MAX_VALUE_SIZE - 1] = '\0';
			}
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
		DebugLogB("Could not open bundle file : %s", fileName);
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
			DebugLogC("Value/Key not defined for: %s, indice: %d",
				tokenKey, tokenIndice);
		}
		ret = -1;
	}
	else
	{
		strncpy(tokenValue, pcFinValue, TOKEN_MAX_VALUE_SIZE);
		tokenValue[TOKEN_MAX_VALUE_SIZE - 1] = '\0';
	}

	fclose(file);
	return ret;
}

