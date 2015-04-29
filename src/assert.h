/*
Copyright 1990,1991 Eric R. Smith. All rights reserved.
*/

#ifdef NDEBUG
#define assert(expression)
#else
# ifdef __STDC__
#define assert(expression) \
	((expression) ? (void)0 : FATAL("assert(`%s') failed at line %ld of %s.", \
	    #expression, (long)__LINE__, __FILE__))
# else
#define assert(expression) if(expression) FATAL("assert(%s) failed", \
	    "expression")
# endif

#endif /* NDEBUG */
