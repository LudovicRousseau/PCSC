/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2003
 *  Toni Andjelkovic <toni@soth.at>
 * Copyright (C) 2003-2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief Reads lexical config files and updates database.
 */

#ifndef __parser_h__
#define __parser_h__

#include "simclist.h"

struct bundleElt
{
	char *key;
	list_t values;
};

int LTPBundleFindValueWithKey(list_t *l, const char *key, list_t **values);
int bundleParse(const char *fileName, list_t *l);
void bundleRelease(list_t *l);

#endif
