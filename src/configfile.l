/*
 * Reads lexical config files and updates database.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2004-2010
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

%{
#include <dirent.h>

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
const char *ConfFile;

void tok_error(char *pcToken_error);

%}

%option nounput
%option noinput
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

#include "config.h"
#include "misc.h"
#include "pcsclite.h"
#include "pcscd.h"
#include "debuglog.h"
#include "sys_generic.h"
#include "readerfactory.h"

int evaluatetoken(char *pcToken)
{
	int channelId = 0;
	int p = 0;
	unsigned int n = 0;

	if (pcPrevious == NULL)
	{	/* This is the key */
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
					{	/* Strip off the quotes */
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
					&& (stat(pcDevicename, &fStatBuf) != 0))
				{
					Log3(PCSC_LOG_CRITICAL, "Error with device %s: %s",
						pcDevicename, strerror(errno));
					Log1(PCSC_LOG_CRITICAL, "You should remove the DEVICENAME line if your driver does not use this field");
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
				if (stat(pcLibpath, &fStatBuf) != 0)
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

	if (pcFriendlyname != NULL &&
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

		/* the DEVICENAME parameter is optional */
		if (NULL == pcDevicename)
			pcDevicename = "";

		channelId = strtoul(pcChannelid, NULL, 0);
		reader_list[reader_list_size-2].pcFriendlyname = strdup(pcFriendlyname);
		reader_list[reader_list_size-2].pcDevicename = strdup(pcDevicename);
		reader_list[reader_list_size-2].pcLibpath = strdup(pcLibpath),
		reader_list[reader_list_size-2].channelId = channelId;

		pcFriendlyname = NULL;
		pcDevicename = NULL;
		pcLibpath = NULL;
		pcChannelid = NULL;
	}

	return 0;
}

void tok_error(char *token_error)
{
	Log4(PCSC_LOG_ERROR, "tok_error: invalid value line %d in %s: %s",
		iOldLinenumber, ConfFile, token_error);
	badError = 1;
}

int DBGetReaderListDir(const char *readerconf_dir,
	SerialReader **caller_reader_list)
{
	DIR *dir;
	int ret = 0;

	/* (re)start with an empty list */
	reader_list = NULL;
	reader_list_size = 0;

	dir = opendir(readerconf_dir);
	if (dir)
	{
		/* the configuration file is a directory */
		struct dirent *direntry;

		Log2(PCSC_LOG_DEBUG, "Parsing conf directory: %s", readerconf_dir);

		/* for each configuration file */
		while ((direntry = readdir(dir)) != NULL)
		{
			char filename[FILENAME_MAX];
			int r;

			/* skip . and .. */
			if ((strcmp(direntry->d_name, ".") == 0) ||
				(strcmp(direntry->d_name, "..") == 0))
				continue;

			snprintf(filename, sizeof(filename), "%s/%s",
				readerconf_dir, direntry->d_name);

			/* each call to DBGetReaderList() will append to the list */
			r = DBGetReaderList(filename, caller_reader_list);

			/* set the global return value to the latest error */
			if (r)
				ret = r;
		}

		closedir(dir);
	}
	else
		/* the configuration file is really a file */
		ret = DBGetReaderList(readerconf_dir, caller_reader_list);

	return ret;
}

int DBGetReaderList(const char *readerconf, SerialReader **caller_reader_list)
{
	FILE *configFile = NULL;

	*caller_reader_list = NULL;	/* no list by default */

	/* used by tok_error() */
	ConfFile = readerconf;

	Log2(PCSC_LOG_DEBUG, "Parsing conf file: %s", ConfFile);

	configFile = fopen(readerconf, "r");

	if (configFile == NULL)
		return 1;

	yyin = configFile;

	/* (re)start with a clean state */
	iLinenumber = 1;
	iOldLinenumber = -1;
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
} /* End of configfile.c */

