/*
Copyright 1992 Eric R. Smith.
Copyright 1993 Atari Corporation.
All rights reserved.
 */

/*
 * inlining of various utility functions, for speed
 * NOTE: ALL functions in this file must also have
 * "normal" equivalents in the .c or .s files;
 * don't put a function just into here!
 */

#ifdef __GNUC__

#define spl7()			\
({  register short retvalue;	\
    __asm__ volatile("		\
	movew sr,%0; 		\
	oriw  #0x0700,sr " 	\
    : "=d"(retvalue) 		\
    ); retvalue; })

#define spl(N)			\
({  				\
    __asm__ volatile("		\
	movew %0,sr " 		\
    :				\
    : "d"(N) ); })


/*
 * note that we must save some registers ourselves,
 * or else gcc will run out of reggies to use
 * and complain
 */

/* On TOS 1.04, when calling Bconout(2,'\a') the VDI jumps directly
   back to the BIOS which expects the register A5 to be set to zero.
   (Specifying the register as clobbered does not work.) */

#define callout1(func, a)			\
({						\
	register long retvalue __asm__("d0");	\
	long _f = func;				\
	short _a = (short)(a);			\
						\
	__asm__ volatile			\
	("  moveml d5-d7/a4-a6,sp@-;		\
	    movew %2,sp@-;			\
	    movel %1,a0;			\
	    subal a5,a5;			\
	    jsr a0@;				\
	    addqw #2,sp;			\
	    moveml sp@+,d5-d7/a4-a6 "		\
	: "=r"(retvalue)	/* outputs */	\
	: "r"(_f), "r"(_a)	/* inputs */	\
	: "d0", "d1", "d2", "d3", "d4",		\
	  "a0", "a1", "a2", "a3" /* clobbered regs */ \
	);					\
	retvalue;				\
})


#define callout2(func, a, b)			\
({						\
	register long retvalue __asm__("d0");	\
	long _f = func;				\
	short _a = (short)(a);			\
	short _b = (short)(b);			\
						\
	__asm__ volatile			\
	("  moveml d5-d7/a4-a6,sp@-;		\
	    movew %3,sp@-;			\
	    movew %2,sp@-;			\
	    movel %1,a0;
	    subal a5,a5;			\
	    jsr a0@;				\
	    addqw #4,sp;			\
	    moveml sp@+,d5-d7/a4-a6 "		\
	: "=r"(retvalue)	/* outputs */	\
	: "r"(_f), "r"(_a), "r"(_b) /* inputs */ \
	: "d0", "d1", "d2", "d3", "d4",		\
	  "a0", "a1", "a2", "a3" /* clobbered regs */ \
	);					\
	retvalue;				\
})

#define flush_pmmu() __asm__ volatile("pflusha");
#endif

#ifdef LATTICE
#pragma inline d0=spl7()	{"40c0007c0700";}
#pragma inline d0=spl(d0)	{"46c0";}
#endif
