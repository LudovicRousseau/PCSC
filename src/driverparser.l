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

void evalToken( char *pcToken, int tokType );

static char *pcDesiredKey   = 0;
static char pcKey[200];
static char pcValue[200];
static char pcFinValue[200];

void errorCheck ( char *pcToken_error );

%}

%%

#.*                                             {}
"\n"                                            {}
\<key\>([A-Z]|[a-z]|[0-9]|[ \t])+\<\/key\>      { evalToken(bptext, 1); } 
[ \t]                     		        {}
\<string\>([A-Z]|[a-z]|[0-9]|[ \t]|[!@#$%^&*()\-+/_\:?.,=~'"])+\<\/string\> { evalToken(bptext, 2); } 
.                                               { errorCheck( bptext ); }
%%

#include <stdio.h>
#include <string.h>

#include "wintypes.h"
#include "debuglog.h"

int bpwrap() {
  return 1;
}


void evalToken( char *pcToken, int tokType ) {

  int len;
  len = 0;

  if ( tokType == 1 ) {
   for (len=5; pcToken[len] != '<'; len++);
   strncpy(pcKey, &pcToken[5], len - 5);
   pcKey[len-5] = 0;
  }

  if ( tokType == 2 ) {
   for (len=8; pcToken[len] != '<'; len++);
   strncpy(pcValue, &pcToken[8], len - 8);
   pcValue[len-8] = 0;
    if ( strcmp(pcKey, pcDesiredKey) == 0 ) {
      strcpy(pcFinValue, pcValue);
    }
  }


}

void errorCheck ( char *token_error ) { }

int LCFBundleFindValueWithKey(char *fileName, char *tokenKey, 
                              char *tokenValue ) {

  FILE *file;
  file = 0;
 
  pcDesiredKey  = tokenKey;
  pcFinValue[0] = 0;

  file = fopen(fileName, "r");
 
  if (!file) {
    DebugLogB( "Could not open bundle file : %s", fileName );
    return 1;
  }
 
  bpin = file;
 
  do {
    bplex();
  }
  while (!feof(file));

  if ( pcFinValue[0] == 0 ) {
    DebugLogB( "Value/Key not defined for: %s", tokenKey );
    fclose(file);
    return -1;
  } else {
    strcpy(tokenValue, pcFinValue);
    fclose(file);
    return 0;
  }
 
  fclose(file);  
  return 0;    
}

