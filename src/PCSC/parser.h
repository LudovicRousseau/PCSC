/*****************************************************************

  File   :   parser.h
  Author :   Toni Andjelkovic
  Date   :   August 11, 2003
  Purpose:   Reads lexical config files and updates database.
             See http://www.linuxnet.com for more information.
  License:   Copyright (C) 2003 Toni Andjelkovic <toni@soth.at>
             Ludovic Rousseau <ludovic.rousseau@free.fr>
$Id$

******************************************************************/

#define TOKEN_MAX_KEY_SIZE   200
#define TOKEN_MAX_VALUE_SIZE 200

#define TOKEN_TYPE_KEY         1
#define TOKEN_TYPE_STRING      2

int LTPBundleFindValueWithKey(char *fileName, char *tokenKey,
                              char *tokenValue, int tokenIndice);

