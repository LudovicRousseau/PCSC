/*
 * Reads lexical config files and updates database.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

%{
int evaluatetoken(char *pcToken);

static int iLinenumber = 1;
static char *pcPrevious = NULL;
static char *pcCurrent = NULL;
static char *pcFriendlyname = NULL;
static char *pcDevicename = NULL;                
static char *pcLibpath = NULL;
static char *pcChannelid = NULL;
static int badError = 0;

void tok_error(char *pcToken_error);

%}

%%

#.*                                       {}
"\n"                                      { iLinenumber++; }
(\"[^"\n]*["\n])|(\'[^'\n]*['\n])         { evaluatetoken( yytext); } 
[ \t]                                     {}
([A-Z]|[a-z]|[0-9]|[\\\/\-\.\_\@:])+      { evaluatetoken( yytext ); } 
.                                         { tok_error( yytext ); }
%%

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pcsclite.h"
#include "debuglog.h"
#include "config.h"
#include "sys_generic.h"
#include "readerfactory.h"

int evaluatetoken(char *pcToken)
{

	DWORD dwChannelId = 0;
	int p = 0;
	int n = 0;

	if (pcPrevious == NULL)
	{							/* This is the key */
		pcPrevious = strdup(pcToken);
	}
	else
	{
		pcCurrent = pcToken;
		if (strcmp(pcPrevious, "FRIENDLYNAME") == 0)
		{
			if (pcFriendlyname == NULL)
			{
				pcFriendlyname = (char *) malloc(strlen(pcCurrent) - 1);
				for (n = 0; n < strlen(pcCurrent); n++)
				{
					if (pcCurrent[n] != '"')
					{			/* Strip off the quotes */
						pcFriendlyname[p++] = pcCurrent[n];
					}
				}
				pcFriendlyname[p++] = '\0';
			}
			else
			{
				tok_error(pcPrevious);
				return 1;
			}
		}
		else if (strcmp(pcPrevious, "DEVICENAME") == 0)
		{
			if (pcDevicename == NULL)
			{
				struct stat fStatBuf;

				pcDevicename = strdup(pcCurrent);
				if ((NULL == strchr(pcDevicename, ':'))
					&& (SYS_Stat(pcDevicename, &fStatBuf) != 0))
				{
					DebugLogC("Error with device %s: %s",
						pcDevicename, strerror(errno));
					DebugLogA("You should use 'DEVICENAME /dev/null' if your driver does not use this field");
					badError = 1;
				}
			}
			else
			{
				tok_error(pcPrevious);
				return 1;
			}
		}
		else if (strcmp(pcPrevious, "LIBPATH") == 0)
		{
			if (pcLibpath == NULL)
			{
				struct stat fStatBuf;

				pcLibpath = strdup(pcCurrent);
				if (SYS_Stat(pcLibpath, &fStatBuf) != 0)
				{
					DebugLogC("Error with library %s: %s",
						pcLibpath, strerror(errno));
					badError = 1;
				}

				if (strstr(pcLibpath, ".bundle") != NULL)
				{
					DebugLogA("WARNING *************************************");
					DebugLogB("WARNING: USB drivers SHOULD NOT be declared in reader.conf: %s", pcLibpath);
					DebugLogA("WARNING *************************************");
				}
			}
			else
			{
				tok_error(pcPrevious);
				return 1;
			}
		}
		else if (strcmp(pcPrevious, "CHANNELID") == 0)
		{
			if (pcChannelid == NULL)
				pcChannelid = strdup(pcCurrent);
			else
			{
				tok_error(pcPrevious);
				return 1;
			}
		}
		else
		{
			tok_error(pcPrevious);
			return 1;
		}

		free(pcPrevious);
		pcPrevious = NULL;
	}

	if (pcFriendlyname != NULL && pcDevicename != NULL &&
		pcLibpath != NULL && pcChannelid != NULL && badError != 1)
	{

		dwChannelId = strtoul(pcChannelid, 0, 16);
		RFAddReader(pcFriendlyname, dwChannelId, pcLibpath, pcDevicename);

		free(pcFriendlyname);
		free(pcDevicename);
		free(pcLibpath);
		free(pcChannelid);
		pcFriendlyname = NULL;
		pcDevicename = NULL;
		pcLibpath = NULL;
		pcChannelid = NULL;
	}

	return 0;
}

void tok_error(char *token_error)
{
	DebugLogB("tok_error: invalid value in reader.conf: %s", token_error);
	badError = 1;
}

int DBUpdateReaders(char *readerconf)
{

	FILE *configFile = NULL;

	configFile = fopen(readerconf, "r");

	if (configFile == NULL)
		return 1;

	yyin = configFile;

	do
	{
		yylex();
	}
	while (!feof(configFile));

	fclose(configFile);

	if (badError == 1)
		return -1;
	else
		return 0;
}								/* End of configfile.c */

