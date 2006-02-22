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

#ifndef __local_h__
#define __local_h__

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
#if defined __GNUC__
#define INTERNAL __attribute__ ((visibility("hidden")))
#define PCSC_API __attribute__ ((visibility("default")))
/* http://docs.sun.com/source/817-6697/sun.specific.html#marker-998544 */
#elif defined __SUNPRO_C
#define INTERNAL __hidden
#define PCSC_API
#else
#define INTERNAL
#define PCSC_API
#endif

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

#endif /* __lcoal_h__ */
