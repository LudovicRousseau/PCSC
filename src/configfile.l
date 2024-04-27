/*
 * Reads lexical config files and updates database.
 *
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2004-2021
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

%{
#include <dirent.h>
#include <sys/stat.h>

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

void tok_error(const char *pcToken_error);

%}

%option nounput
%option noinput
%option noyywrap

%%

#.*                                       {}
"\n"                                      { iLinenumber++; }
(\"[^"\n]*["\n])|(\'[^'\n]*['\n])         { (void)evaluatetoken(yytext); }
[ \t]                                     {}
([A-Z]|[a-z]|[0-9]|[=\\\/\-\.\_\@:])+      { (void)evaluatetoken(yytext); }
.                     { iOldLinenumber = iLinenumber; tok_error(yytext); }
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
				size_t n, p;

				pcFriendlyname = malloc(strlen(pcCurrent) + 1);
				for (n = 0, p = 0; n < strlen(pcCurrent); n++)
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
					Log2(PCSC_LOG_ERROR, "WARNING: USB drivers SHOULD NOT be declared in a reader.conf file: %s", pcLibpath);
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

	/* CHANNELID and DEVICENAME are both optional but not at the same time */
	if (pcFriendlyname && pcLibpath && badError != 1
		&& (pcChannelid || pcDevicename))
	{
		int channelId;
		static char* defaultDeviceName = (char *)"";

		Log2(PCSC_LOG_DEBUG, "Add reader: %s", pcFriendlyname);
		if (0 == reader_list_size)
		{
			/* one real reader and one end marker */
			reader_list_size = 2;
			reader_list = malloc(reader_list_size * sizeof(SerialReader));
		}
		else
		{
			SerialReader *new_reader_list = NULL;
			reader_list_size++;
			new_reader_list = realloc(reader_list, reader_list_size *
				sizeof(SerialReader));
			if (new_reader_list == NULL)
				free(reader_list);
			reader_list = new_reader_list;
		}
		if (reader_list == NULL)
		{
			tok_error("No Memory");
			return 1;
		}

		/* end marker */
		reader_list[reader_list_size-1].pcFriendlyname = NULL;

		/* the DEVICENAME parameter is optional */
		if (NULL == pcDevicename)
			pcDevicename = defaultDeviceName;

		if (pcChannelid)
			channelId = strtoul(pcChannelid, NULL, 0);
		else
			channelId = 0;
		reader_list[reader_list_size-2].pcFriendlyname = strdup(pcFriendlyname);
		reader_list[reader_list_size-2].pcDevicename = strdup(pcDevicename);
		reader_list[reader_list_size-2].pcLibpath = strdup(pcLibpath),
		reader_list[reader_list_size-2].channelId = channelId;

		free(pcFriendlyname);
		pcFriendlyname = NULL;

		if (pcDevicename != defaultDeviceName)
			free(pcDevicename);
		pcDevicename = NULL;

		free(pcLibpath);
		pcLibpath = NULL;

		if (pcChannelid)
			free(pcChannelid);
		pcChannelid = NULL;
	}

	return 0;
}

void tok_error(const char *token_error)
{
#ifdef NO_LOG
	(void)token_error;
#endif
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

			snprintf(filename, sizeof(filename), "%s/%s",
				readerconf_dir, direntry->d_name);

			/* skip non regular files */
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
			if (direntry->d_type == DT_UNKNOWN)
#endif
			{
				struct stat st;

				if (lstat(filename, &st) != 0)
				{
					Log2(PCSC_LOG_DEBUG, "Skipping non statable file: %s",
						direntry->d_name);
					continue;
				}

				if (!S_ISREG(st.st_mode))
				{
					Log2(PCSC_LOG_DEBUG, "Skipping non regular file: %s",
						direntry->d_name);
					continue;
				}
			}
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
			else
				if (direntry->d_type != DT_REG)
				{
					Log2(PCSC_LOG_DEBUG, "Skipping non regular file: %s",
						direntry->d_name);
					continue;
				}
#endif

			/* skip files starting with . like ., .., .svn, etc */
			if ('.' == direntry->d_name[0])
			{
				Log2(PCSC_LOG_DEBUG, "Skipping hidden file: %s",
					direntry->d_name);
				continue;
			}

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
	yylex_destroy();

	(void)fclose(configFile);

	*caller_reader_list = reader_list;

	if (badError == 1)
		return -1;
	else
		return 0;
} /* End of configfile.c */

