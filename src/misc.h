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
 */
#if defined __GCC__
#define INTERNAL __attribute__ ((visibility("hidden")))
/* http://docs.sun.com/source/817-6697/sun.specific.html#marker-998544 */
#elif defined __SUNPRO_C
#define INTERNAL __hidden
#else
#define INTERNAL
#endif

#if defined __GCC__

/* GNU Compiler Collection (GCC) */
#define CONSTRUCTOR __attribute__ ((constructor))
#define DESTRUCTOR __attribute__ ((destructor))
#define CONSTRUCTOR_DECLARATION(x)
#define DESTRUCTOR_DECLARATION(x)
	
#elif defined __SUNPRO_C

/* SUN C compiler */
#define CONSTRUCTOR
#define DESTRUCTOR
#define CONSTRUCTOR_DECLARATION(x) #pragma init (x)
#define DESTRUCTOR_DECLARATION(x) #pragma fini (x)
	
#else

/* any other */
#define CONSTRUCTOR
#define DESTRUCTOR
#define CONSTRUCTOR_DECLARATION(x)
#define DESTRUCTOR_DECLARATION(x)

#endif

#ifdef __cplusplus
}
#endif

#endif /* __lcoal_h__ */
