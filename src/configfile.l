/*****************************************************************

  File   :   configfile.ll
  Author :   David Corcoran
  Date   :   February 12, 1999 modified 7/28/99
  Purpose:   Reads lexical config files and updates database.
             See http://www.linuxnet.com for more information.
  License:   Copyright (C) 1999 David Corcoran
             <corcoran@linuxnet.com>

******************************************************************/

%{
int evaluatetoken( char *pcToken );

static int iLinenumber      = 1;
static char *pcPrevious     = 0;
static char *pcCurrent      = 0;
static char *pcFriendlyname = 0;
static char *pcDevicename   = 0;
static char *pcLibpath      = 0;
static char *pcChannelid    = 0;
static int   badError       = 0;

void tok_error ( char *pcToken_error );

%}

%%

#.*                                       {}
"\n"                                      { iLinenumber++; }
(\"[^"\n]*["\n])|(\'[^'\n]*['\n])         { evaluatetoken( yytext); } 
[ \t]                     {}
([A-Z]|[a-z]|[0-9]|[\\\/\-\.\_\@])+       { evaluatetoken( yytext ); } 
.                                         { tok_error( yytext ); }
%%

#include <stdio.h>
#include <string.h>
#include <wintypes.h>

#include "pcsclite.h"
#include "sys_generic.h"
#include "readerfactory.h"
#include "debuglog.h"

int evaluatetoken( char *pcToken ) {

  DWORD dwChannelId = 0;
  int p             = 0;
  int n             = 0;

  if ( pcPrevious == 0 ) {       /* This is the key */
    pcPrevious = strdup( pcToken );
  } else {
    pcCurrent = pcToken;
    if ( strcmp( pcPrevious, "FRIENDLYNAME" ) == 0 ) {
       if ( pcFriendlyname == 0 ) {
         pcFriendlyname = (char *)malloc(strlen(pcCurrent)-1);
        for ( n = 0; n < strlen(pcCurrent); n++ ) {
           if ( pcCurrent[n] != '"' ) { /* Strip off the quotes */
             pcFriendlyname[p++] = pcCurrent[n];         
           }
         }
         pcFriendlyname[p++] = 0;
       } else {
         tok_error( pcPrevious ); return 1;
       }
    } else if ( strcmp( pcPrevious, "DEVICENAME" ) == 0 ) {
       if ( pcDevicename == 0 ) {
         pcDevicename = strdup( pcCurrent );
       } else {
         tok_error( pcPrevious ); return 1;
       }
    } else if ( strcmp( pcPrevious, "LIBPATH" ) == 0 ) {
       if ( pcLibpath == 0 ) {
         pcLibpath = strdup( pcCurrent );
       } else {
         tok_error( pcPrevious ); return 1;
       }
    } else if ( strcmp( pcPrevious, "CHANNELID" ) == 0 ) {
       if ( pcChannelid == 0 ) {
         pcChannelid = strdup( pcCurrent );
       } else {
         tok_error( pcPrevious ); return 1;
       }
    } else {
       tok_error( pcPrevious ); return 1;
    }

    free( pcPrevious ); pcPrevious = 0;
  }

  if ( pcFriendlyname != 0 && pcDevicename != 0 &&
       pcLibpath      != 0 && pcChannelid != 0 ) {
     
       dwChannelId = strtoul( pcChannelid, 0, 16 );
       RFAddReader( pcFriendlyname, dwChannelId, pcLibpath );

       free( pcFriendlyname ); free( pcDevicename );
       free( pcLibpath);       free( pcChannelid );
       pcFriendlyname = 0;     pcDevicename = 0;
       pcLibpath      = 0;     pcChannelid  = 0;
  }

  return 0;
}

void tok_error ( char *token_error ) {
  debug_msg("%s:%d tok_error: invalid value in reader.conf", 
             __FILE__, __LINE__);
  badError = 1;
}

int DBUpdateReaders ( char *readerconf ) {

  FILE *configFile;
  configFile = 0;	

  configFile = fopen( readerconf, "r");

  if (configFile == 0) {
    return 1;
  }

  yyin = configFile;

  do {
    yylex();
  }
  while (!feof(configFile));

  fclose(configFile);

  if (badError == 1) {
    return -1;
  } else {
    return 0;
  }
} /* End of configfile.c */

