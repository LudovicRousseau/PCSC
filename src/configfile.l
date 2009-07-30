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
#include "wintypes.h"
#include "pcscd.h"
#include "readerfactory.h"
#include "configfile.h"

int evaluatetoken(char *pcToken);

static int iLinenumber;
static int iOldLinenumber;
static char *pcPrevious;
static char *pcCurrent;
static char *pcFriendlyname;
static char *pcDevicename;
static char *pcLibpath;
static char *pcChannelid;
static int badError;
static SerialReader *reader_list;
static int reader_list_size;

void tok_error(char *pcToken_error);

%}

%option nounput
%option noyywrap

%%

#.*                                       {}
"\n"                                      { iLinenumber++; }
(\"[^"\n]*["\n])|(\'[^'\n]*['\n])         { (void)evaluatetoken(yytext); }
[ \t]                                     {}
([A-Z]|[a-z]|[0-9]|[\\\/\-\.\_\@:])+      { (void)evaluatetoken(yytext); }
.                                         { tok_error(yytext); }
%%

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "misc.h"
#include "pcsclite.h"
#include "pcscd.h"
#include "debuglog.h"
#include "config.h"
#include "sys_generic.h"
#include "readerfactory.h"

int evaluatetoken(char *pcToken)
{

	DWORD dwChannelId = 0;
	int p = 0;
	unsigned int n = 0;

	if (pcPrevious == NULL)
	{							/* This is the key */
		pcPrevious = strdup(pcToken);
		iOldLinenumber = iLinenumber;
	}
	else
	{
		/* first and second tokens are not on the same line */
		if (iOldLinenumber != iLinenumber)
		{
			tok_error(pcPrevious);
			pcPrevious = strdup(pcToken);
			iOldLinenumber = iLinenumber;
			return 1;
		}

		pcCurrent = pcToken;
		if (strcmp(pcPrevious, "FRIENDLYNAME") == 0)
		{
			if (pcFriendlyname == NULL)
			{
				pcFriendlyname = malloc(strlen(pcCurrent) - 1);
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
					Log3(PCSC_LOG_CRITICAL, "Error with device %s: %s",
						pcDevicename, strerror(errno));
					Log1(PCSC_LOG_CRITICAL, "You should use 'DEVICENAME /dev/null' if your driver does not use this field");
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
					Log3(PCSC_LOG_CRITICAL, "Error with library %s: %s",
						pcLibpath, strerror(errno));
					badError = 1;
				}

				if (strstr(pcLibpath, ".bundle") != NULL)
				{
					Log1(PCSC_LOG_ERROR, "WARNING *************************************");
					Log2(PCSC_LOG_ERROR, "WARNING: USB drivers SHOULD NOT be declared in reader.conf: %s", pcLibpath);
					Log1(PCSC_LOG_ERROR, "WARNING *************************************");
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
			free(pcPrevious);
			pcPrevious = NULL;
			return 1;
		}

		free(pcPrevious);
		pcPrevious = NULL;
	}

	if (pcFriendlyname != NULL && pcDevicename != NULL &&
		pcLibpath != NULL && pcChannelid != NULL && badError != 1)
	{
		if (0 == reader_list_size)
		{
			/* one real reader and one end marker */
			reader_list_size = 2;
			reader_list = malloc(reader_list_size * sizeof(SerialReader));
		}
		else
		{
			reader_list_size++;
			reader_list = realloc(reader_list, reader_list_size *
				sizeof(SerialReader));
		}

		/* end marker */
		reader_list[reader_list_size-1].pcFriendlyname = NULL;

		dwChannelId = strtoul(pcChannelid, 0, 16);
		reader_list[reader_list_size-2].pcFriendlyname = strdup(pcFriendlyname);
		reader_list[reader_list_size-2].pcDevicename = strdup(pcDevicename);
		reader_list[reader_list_size-2].pcLibpath = strdup(pcLibpath),
		reader_list[reader_list_size-2].dwChannelId = dwChannelId;

		pcFriendlyname = NULL;
		pcDevicename = NULL;
		pcLibpath = NULL;
		pcChannelid = NULL;
	}

	return 0;
}

void tok_error(char *token_error)
{
	Log3(PCSC_LOG_ERROR, "tok_error: invalid value line %d in " PCSCLITE_READER_CONFIG ": %s", iOldLinenumber, token_error);
	badError = 1;
}

int DBGetReaderList(const char *readerconf, SerialReader **caller_reader_list)
{
	FILE *configFile = NULL;

	*caller_reader_list = NULL;	/* no list by default */

	configFile = fopen(readerconf, "r");

	if (configFile == NULL)
		return 1;

	yyin = configFile;

	/* (re)start with a clean state */
	iLinenumber = 1;
	iOldLinenumber = -1;
	reader_list = NULL;
	reader_list_size = 0;
	pcFriendlyname = NULL;
	pcDevicename = NULL;
	pcLibpath = NULL;
	pcChannelid = NULL;
	pcPrevious = NULL;
	pcCurrent = NULL;
	badError = 0;

	do
	{
		(void)yylex();
	}
	while (!feof(configFile));

	(void)fclose(configFile);

	*caller_reader_list = reader_list;

	if (badError == 1)
		return -1;
	else
		return 0;
}								/* End of configfile.c */

