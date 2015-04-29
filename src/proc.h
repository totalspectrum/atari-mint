/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
*/

/* proc.h: defines for various process related things */
#ifndef _proc_h
#define _proc_h

#include "file.h"

/* a process context consists, for now, of its registers */

typedef struct _context {
	long	regs[15];	/* registers d0-d7, a0-a6 */
	long	usp;		/* user stack pointer (a7) */
	short	sr;		/* status register */
	long	pc;		/* program counter */
	long	ssp;		/* supervisor stack pointer */
	long	term_vec;	/* GEMDOS terminate vector (0x102) */
/*
 * AGK: if running on a TT and the user is playing with the FPU then we
 * must save and restore the context. We should also consider this for
 * I/O based co-processors, although this may be difficult due to
 * possibility of a context switch in the middle of an I/O handshaking
 * exchange.
 */
	unsigned char	fstate[216];	/* FPU internal state */
	long	fregs[3*8];	/* registers fp0-fp7 */
	long	fctrl[3];	/* FPCR/FPSR/FPIAR */
	char	ptrace;		/* trace exception is pending */
	char	pad1;		/* junk */
	long	iar;		/* unused */
	short	res[4];		/* unused, reserved */
/*
 * Saved CRP and TC values.  These are necessary for memory protection.
 */

	crp_reg crp;			/* 64 bits */
	tc_reg tc;			/* 32 bits */
/*
 * AGK: for long (time-wise) co-processor instructions (FMUL etc.), the
 * FPU returns NULL, come-again with interrupts allowed primitives. It
 * is highly likely that a context switch will occur in one of these if
 * running a mathematically intensive application, hence we must handle
 * the mid-instruction interrupt stack. We do this by saving the extra
 * 3 long words and the stack format word here.
 */
	unsigned short	sfmt;	/* stack frame format identifier */
	short	internal[42];	/* internal state -- see framesizes[] for size */
} CONTEXT;

#define PROC_CTXTS	2
#define SYSCALL		0	/* saved context from system call	*/
#define CURRENT		1	/* current saved context		*/

/*
 * Timeout events are stored in a list; the "when" field in the event
 * specifies the number of milliseconds *after* the last entry in the
 * list that the timeout should occur, so routines that manipulate
 * the list only need to check the first entry.
 */

typedef struct timeout {
	struct timeout *next;
	struct proc	*proc;
	long	when;
	void	(*func) P_((struct proc *)); /* function to call at timeout */
	short	flags;
	long	arg;
} TIMEOUT;

struct itimervalue {
	TIMEOUT *timeout;
	long interval;
	long reqtime;
	long startsystime;
	long startusrtime;
};

#ifndef GENMAGIC
extern TIMEOUT *tlist;
#endif

#define NUM_REGIONS	64	/* number of memory regions alloced at a time */
#define MIN_HANDLE	(-5)	/* minimum handle number		*/
#define MIN_OPEN	6	/* 0..MIN_OPEN-1 are reserved for system */
#define MAX_OPEN	32	/* max. number of open files for a proc	*/
#define SSTKSIZE	8000	/* size of supervisor stack (in bytes) 	*/
#define ISTKSIZE	2000	/* size of interrupt stack (in bytes)	*/
#define STKSIZE		(ISTKSIZE + SSTKSIZE)

#define FRAME_MAGIC	0xf4a3e000UL
				/* magic for signal call stack */
#define CTXT_MAGIC	0xabcdef98UL
#define CTXT2_MAGIC	0x87654321UL
				/* magic #'s for contexts */

#define PNAMSIZ		8	/* no. of characters in a process name */

#define DOM_TOS		0	/* TOS process domain */
#define DOM_MINT	1	/* MiNT process domain */

typedef struct proc {
	long	sysstack;		/* must be first		*/
	CONTEXT	ctxt[PROC_CTXTS];	/* must be second		*/

/* this is stuff that the public can know about */
	long	magic;			/* validation for proc struct	*/

	BASEPAGE *base;			/* process base page		*/
	short	pid, ppid, pgrp;
	short	ruid;			/* process real user id 	*/
	short	rgid;			/* process real group id 	*/
	short	euid, egid;		/* effective user and group ids */

	ushort	memflags;		/* e.g. malloc from alternate ram */
	short	pri;			/* base process priority 	*/
	short	wait_q;			/* current process queue	*/

/* note: wait_cond should be unique for each kind of condition we might
 * want to wait for. Put a define below, or use an address in the
 * kernel as the wait condition to ensure uniqueness.
 */
	long	wait_cond;		/* condition we're waiting on	*/
					/* (also return code from wait) */

#define WAIT_MB		0x3a140001L	/* wait_cond for p_msg call	*/
#define WAIT_SEMA	0x3a140003L	/* wait_cond for p_semaphore	*/

	/* (all times are in milliseconds) */
	/* usrtime must always follow systime */
	ulong	systime;		/* time spent in kernel		*/
	ulong	usrtime;		/* time spent out of kernel	*/
	ulong	chldstime;		/* children's kernel time 	*/
	ulong	chldutime;		/* children's user time		*/

	ulong	maxmem;			/* max. amount of memory to use */
	ulong	maxdata;		/* max. data region for process */
	ulong	maxcore;		/* max. core memory for process */
	ulong	maxcpu;			/* max. cpu time to use 	*/

	short	domain;			/* process domain (TOS or UNIX)	*/

	short	curpri;			/* current process priority	*/
#define MIN_NICE -20
#define MAX_NICE 20

/* EVERYTHING BELOW THIS LINE IS SUBJECT TO CHANGE:
 * programs should *not* try to read this stuff via the U:\PROC dir.
 */

	/* jr: two fields to hold information passed to Pexec */
	char	fname[PATH_MAX];	/* name of binary */
	char	cmdlin[128];		/* original command line */

	char	name[PNAMSIZ+1];	/* process name			*/
	TIMEOUT	*alarmtim;		/* alarm() event		*/
	short	slices;			/* number of time slices before this
					   process gets to run again */

	short	bconmap;		/* Bconmap mapping		*/
	FILEPTR *midiout;		/* MIDI output			*/
	FILEPTR *midiin;		/* MIDI input			*/
	FILEPTR	*prn;			/* printer			*/
	FILEPTR *aux;			/* auxiliary tty		*/
	FILEPTR	*control;		/* control tty			*/
	FILEPTR	*handle[MAX_OPEN];	/* file handles			*/

	uchar	fdflags[MAX_OPEN];	/* file descriptor flags	*/

	ushort	num_reg;		/* number of memory regions allocated */
	MEMREGION **mem;		/* allocated memory regions	*/
	virtaddr *addr;			/* addresses of regions		*/

	ulong	sigpending;		/* pending signals		*/
	ulong	sigmask;		/* signals that are masked	*/
	ulong	sighandle[NSIG];	/* signal handlers		*/
	ushort	sigflags[NSIG];		/* signal flags			*/
	ulong	sigextra[NSIG];		/* additional signals to be masked
					   on delivery 	*/
	ulong	nsigs;			/* number of signals delivered 	*/
	char	*mb_ptr;		/* p_msg buffer ptr		*/
	long	mb_long1, mb_long2;	/* p_msg storage		*/
	long	mb_mbid;		/* p_msg id being waited for	*/
	short	mb_mode;		/* p_msg mode being waiting in	*/
	short	mb_writer;		/* p_msg pid of writer of msg	*/

	short	curdrv;			/* current drive		*/
	ushort	umask;			/* file creation mask		*/
	fcookie root[NUM_DRIVES];	/* root directories		*/
	fcookie	curdir[NUM_DRIVES];	/* current directory		*/

	long	usrdata;		/* user-supplied data		*/

	DTABUF	*dta;			/* current DTA			*/
#define NUM_SEARCH	10		/* max. number of searches	*/
	DTABUF *srchdta[NUM_SEARCH];	/* for Fsfirst/next		*/
	DIR	srchdir[NUM_SEARCH];	/* for Fsfirst/next		*/
	long	srchtim[NUM_SEARCH];	/* for Fsfirst/next		*/
	
	DIR	*searches;		/* open directory searches	*/

	long	txtsize;		/* size of text region (for fork()) */

	long ARGS_ON_STACK (*criticerr) P_((long)); /* critical error handler	*/
	void	*logbase;		/* logical screen base		*/

	struct proc *ptracer;		/* process which is tracing this one */
	short	ptraceflags;		/* flags for process tracing */
	short	starttime;		/* time and date when process	*/
	short	startdate;		/* was started			*/

	short	in_dos;			/* flag: 1 = process is executing a GEMDOS call */

	void	*page_table;		/* rounded page table pointer	*/
	void	*pt_mem;		/* original kmalloc'd block for above */

	ulong	exception_pc;		/* pc at time of bombs		*/
	ulong	exception_ssp;		/* ssp at time of bomb (e.g. bus error)	*/
	ulong	exception_tbl;		/* table in use at exception time */
	ulong	exception_addr;		/* access address from stack	*/
	ushort	exception_mmusr;	/* result from ptest insn	*/

	short	fork_flag;		/* flag: set to 1 if process has called Pfork() */

	short	auid;			/* tesche: audit user id */
#define	NGROUPS_MAX	8
	short	ngroups;		/* tesche: number of supplementary groups */
	short	ngroup[NGROUPS_MAX];	/* tesche: supplementary groups */
	struct itimervalue itimer[3];	/* interval timers */

	struct	proc *q_next;		/* next process on queue	*/
	struct 	proc *gl_next;		/* next process in system	*/
	char	stack[STKSIZE+4];	/* stack for system calls	*/
} PROC;


/* different process queues */

#define CURPROC_Q	0
#define READY_Q		1
#define WAIT_Q		2
#define IO_Q		3
#define ZOMBIE_Q	4
#define TSR_Q		5
#define STOP_Q		6
#define SELECT_Q	7

#define NUM_QUEUES	8

#ifndef GENMAGIC
extern PROC *proclist;			/* list of all active processes */
extern PROC *curproc;			/* current process		*/
extern PROC *rootproc;			/* pid 0 -- MiNT itself		*/
extern PROC *sys_q[NUM_QUEUES];		/* process queues		*/

extern long page_table_size;

#endif

#endif /* _proc_h */
