/*****************************************************************

  File   :   configfile.ll
  Author :   David Corcoran
  Date   :   February 12, 1999, modified 4/6/2003
  Purpose:   Reads lexical config files and updates database.
             See http://www.linuxnet.com for more information.
  License:   Copyright (C) 1999 David Corcoran, Ludovic Rousseau
             <corcoran@linuxnet.com>
$Id$

******************************************************************/

%{

void tpevalToken(char *pcToken, int tokType);

static char *pcDesiredKey = 0;
static char pcKey[200];
static char pcValue[200];
static char pcFinValue[200];
static int valueIndex = 0;
static int desiredIndex = 0;

void tperrorCheck (char *pcToken_error);

%}

%%

#.*                                             {}
"\n"                                            {}
\<key\>([A-Z]|[a-z]|[0-9]|[ \t])+\<\/key\>      { valueIndex = 0; tpevalToken(yytext, 1); }
[ \t]                     		        {}
\<string\>([A-Z]|[a-z]|[0-9]|[ \t]|[!@#$%^&*()\-+/_\:?.,=~'"])+\<\/string\> {tpevalToken(yytext, 2); valueIndex += 1;}
.                                               { tperrorCheck( yytext ); }
%%

#include <stdio.h>
#include <string.h>
#include "debuglog.h"
#include "config.h"

int yywrap()
{
	return 1;
}


void tpevalToken(char *pcToken, int tokType)
{
	int len;
	len = 0;

	if (tokType == 1)
	{
		for (len=5; pcToken[len] != '<'; len++)
			;
		strncpy(pcKey, &pcToken[5], len - 5);
		pcKey[len-5] = 0;
	}

	if (tokType == 2)
	{
		for (len=8; pcToken[len] != '<'; len++)
			;
		strncpy(pcValue, &pcToken[8], len - 8);
		pcValue[len-8] = 0;
		if (strcmp(pcKey, pcDesiredKey) == 0)
			if (desiredIndex == valueIndex)
				strcpy(pcFinValue, pcValue);
	}
}

void tperrorCheck (char *token_error)
{
}

int LTPBundleFindValueWithKey(char *fileName, char *tokenKey,
                              char *tokenValue, int tokenIndice)
{
	FILE *file = NULL;
	ret = 0;

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
		strcpy(tokenValue, pcFinValue);

	fclose(file);
	return ret;
}

