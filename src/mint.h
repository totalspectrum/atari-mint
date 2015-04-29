/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
*/

#ifndef GENMAGIC
/* use our own library: strongly recommended */
#define OWN_LIB
#endif

#ifdef OWN_LIB
#include "ctype.h"
#else
#include <ctype.h>
#include <string.h>
#endif
#include <osbind.h>

/* configuration options */

/* make real processor exceptions (bus error, etc.) raise a signal */
#define EXCEPTION_SIGS

/* deliberately fill memory with junk when allocating: used for testing */
#undef JUNK_MEM

#if 0
/* other options best set in the makefile */
#define MULTITOS	/* make a MultiTOS kernel */
#define ONLY030		/* make a 68030 only version */
#define DEBUG_INFO	/* include debugging info */
#define FASTTEXT	/* include the fast text device (do NOT do this on Falcons!) */
#endif

/* PATH_MAX is the maximum path allowed. The kernel uses this in lots of
 * places, so there isn't much point in file systems allowing longer
 * paths (they can restrict paths to being shorter if they want).
 * (This is slowly changing, actually... fewer and fewer places use
 *  PATH_MAX, and eventually we should get rid of it)
 */
#define PATH_MAX 128

/* maximum length of a string passed to ksprintf: this should be
 * no more than PATH_MAX
 */
#define SPRINTF_MAX	PATH_MAX

/* NOTE: NAME_MAX is a "suggested" maximum name length only. Individual
 * file systems may choose a longer or shorter NAME_MAX, so do _not_
 * use this in the kernel for anything!
 */
#define NAME_MAX 14

/*
 * configuration section: put compiler specific stuff here
 */

#ifdef __GNUC__
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5)
#  define NORETURN __attribute__ ((noreturn))
# else
#  define EXITING volatile	/* function never returns */
#endif
#endif

#ifndef EXITING
#define EXITING
#endif

#ifndef NORETURN
#define NORETURN
#endif

#ifdef dLibs
#define fullpath full_path
#define SHORT_NAMES
#endif

/* define to indicate unused variables */
#ifdef __TURBOC__
#define UNUSED(x)	(void)x
#else
#define UNUSED(x)
#endif

/* define how to call functions with stack parameter passing */
#ifdef __TURBOC__
#define ARGS_ON_STACK cdecl
#else
#define ARGS_ON_STACK
#endif

/* define to mark a function as inline */
#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

/* WARNING: Bconmap is defined incorrectly
 * in the MiNT library osbind.h at patchlevel
 * <= 19 and in early versions of the GNU C
 * library. So use this binding for safety's sake.
 */

#ifdef __GNUC__
#undef Bconmap
#define Bconmap(dev) (long)trap_14_ww(0x2c, dev)
#ifndef Fcntl
#ifndef trap_1_wwlw

/* see osbind.h for __extension__ and AND_MEMORY */

#define trap_1_wwlw(n, a, b, c)						\
__extension__								\
({									\
	register long retvalue __asm__("d0");				\
	short _a = (short)(a);						\
	long  _b = (long) (b);						\
	short  _c = (short) (c);					\
	    								\
	__asm__ volatile						\
	("\
		movw    %4,sp@-; \
		movl    %3,sp@-; \
		movw    %2,sp@-; \
		movw    %1,sp@-; \
		trap    #1;	\
		lea	sp@(10),sp " \
	: "=r"(retvalue)			/* outputs */		\
	: "g"(n), "r"(_a), "r"(_b), "r"(_c)     /* inputs  */		\
	: "d0", "d1", "d2", "a0", "a1", "a2"    /* clobbered regs */	\
	  AND_MEMORY							\
	);								\
	retvalue;							\
})
#endif

#define Fcntl(f, arg, cmd)					\
		trap_1_wwlw(0x104, (short)(f), (long)(arg), (short)(cmd))
#endif
#endif

#ifndef __TURBOC__
#ifndef Bconmap
extern long xbios();
#define Bconmap(dev) xbios(0x2c, dev)
#endif
#endif

/* Binding for Flock */
#ifndef __TURBOC__
#ifndef Flock
extern long gemdos();
/* this may need to be adjusted for your compiler/library */
#define Flock(handle, mode, start, len) gemdos(0x5c, handle, mode, start, len)
#endif
/* ..and Fcntl */
#ifndef Fcntl
extern long gemdos();
#define Fcntl(f, arg, cmd) gemdos(0x104, (short)(f), (long)(arg), (short)(cmd))
#endif
#endif

#ifdef OWN_LIB
/* Sigh. Some compilers are too clever for their
 * own good; gcc 2.1 now makes strcpy() and some
 * other string functions built-in; the built-in
 * definitions disagree with ours. So we redefine
 * them here. This also helps us to avoid conflict
 * with any library stuff, in the event that we
 * have to link in a library.
 */

#define strlen	MS_len
#define strcpy	MS_cpy
#define strncpy	MS_ncpy
#define strcat	MS_cat
#define strncat	MS_ncat
#define strcmp	MS_cmp
#define strncmp	MS_ncmp
#define strnicmp	MS_nicmp
#define stricmp	MS_icmp
#define strlwr	MS_lwr
#define strupr	MS_upr
#define sleep	M_sleep
#define memcpy	quickmovb
#endif

#ifdef SHORT_NAMES
#define dispose_fileptr ds_fileptr
#define dispose_region ds_region
#define dispose_proc ds_proc
#endif

/* prototype macro thingy */
#ifdef __STDC__
#define P_(x) x
#else
#define P_(x) ()
#define const
#define volatile
#endif

#ifndef GENMAGIC
#include "assert.h"
#endif
#include "atarierr.h"
#include "basepage.h"
#include "types.h"
#include "signal.h"
#include "mem.h"
#include "proc.h"

#ifndef GENMAGIC
#include "proto.h"
#include "sproto.h"
#endif

#ifndef offsetof
#include <stddef.h>
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#include "debug.h"

#define check_addr(x) 0
#define yield() sleep(READY_Q, 0L)

#define CTRL(x) ((x) & 0x1f)

#ifndef GENMAGIC

extern short timestamp, datestamp;	/* in timeout.c */

typedef struct kbdvbase {
	long midivec;
	long vkbderr;
	long vmiderr;
	long statvec;
	long mousevec;
	long clockvec;
	long joyvec;
	long midisys;
	long ikbdsys;
} KBDVEC;

extern KBDVEC *syskey;

#define ST	0
#define STE	0x00010000L
#define MEGASTE 0x00010010L
#define TT	0x00020000L
#define FALCON	0x00030000L

extern long mch;

extern int has_bconmap;	/* set in main() */
extern int curbconmap;  /* see xbios.c */

#define MAXLANG 6	/* languages supported */
extern int gl_lang;	/* set in main.c */

/*
 * load some inline functions, perhaps
 */
#include "inline.h"

#endif /* GENMAGIC */
