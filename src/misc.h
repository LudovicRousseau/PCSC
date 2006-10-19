/*
 * This handles GCC attributes
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2005
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

#ifndef __misc_h__
#define __misc_h__

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Declare the function as internal to the library: the function name is
 * not exported and can't be used by a program linked to the library
 *
 * see http://gcc.gnu.org/onlinedocs/gcc-3.3.5/gcc/Function-Attributes.html#Function-Attributes
 * see http://www.nedprod.com/programs/gccvisibility.html
 */
#if defined __GNUC__ && (__GNUC__ >= 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3))
#define INTERNAL __attribute__ ((visibility("hidden")))
#define PCSC_API __attribute__ ((visibility("default")))
#else
#define INTERNAL
#define PCSC_API
#endif
#define EXTERNAL PCSC_API

#if defined __GNUC__

/* GNU Compiler Collection (GCC) */
#define CONSTRUCTOR __attribute__ ((constructor))
#define DESTRUCTOR __attribute__ ((destructor))

#else

/* SUN C compiler does not use __attribute__ but #pragma init (function)
 * We can't use a # inside a #define so it is not possible to use
 * #define CONSTRUCTOR_DECLARATION(x) #pragma init (x)
 * The #pragma is used directly where needed */

/* any other */
#define CONSTRUCTOR
#define DESTRUCTOR

#endif

#ifdef __cplusplus
}
#endif

#endif /* __misc_h__ */
