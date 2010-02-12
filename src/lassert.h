/*
 * Copyright (C) 2007
 *  Jacob Berkman
 * Copyright (C) 2008
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 */

#ifndef LASSERT_H
#define LASSERT_H

#include <stdio.h>
#include <stdlib.h>

#if 0
#define FAIL exit (1)
#else
#define FAIL return 1
#endif

#define LASSERT(cond)                                                   \
    ({                                                                  \
        if (! (cond))                                                     \
        {                                                               \
            fprintf (stderr, "%s:%d: assertion FAILED: " #cond "\n",    \
                     __FILE__, __LINE__);                               \
            FAIL;                                                       \
        }                                                               \
    })

#define LASSERTF(cond, fmt, a...)                                       \
    ({                                                                  \
        if (! (cond))                                                     \
        {                                                               \
            fprintf (stderr, "%s:%d: assertion FAILED: " #cond ": " fmt, \
                     __FILE__, __LINE__, ## a);                         \
            FAIL;                                                       \
        }                                                               \
    })

#endif /* LASSERT_H */
