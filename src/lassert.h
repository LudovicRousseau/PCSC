#ifndef LASSERT_H
#define LASSERT_H

#include <stdio.h>
#include <stdlib.h>

#if 0
#define FAIL exit (1)
#else
#define FAIL
#endif

#define LASSERT(cond)                                                   \
    ({                                                                  \
        if (cond)                                                       \
            ;                                                           \
        else {                                                          \
            fprintf (stderr, "%s:%d: assertion FAILED: " #cond "\n",    \
                     __FILE__, __LINE__);                               \
            FAIL;                                                       \
        }                                                               \
    })

#define LASSERTF(cond, fmt, a...)                                       \
    ({                                                                  \
        if (cond)                                                       \
            ;                                                           \
        else {                                                          \
            fprintf (stderr, "%s:%d: assertion FAILED: " #cond ": " fmt, \
                     __FILE__, __LINE__, ## a);                         \
            FAIL;                                                       \
        }                                                               \
    })

#endif /* LASSERT_H */
