/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
*/

#include "mint.h"
#include "version.h"
#include "cookie.h"
#include "xbra.h"

/* the kernel's stack size */
#define STACK	8*1024L

/* if the user is holding down the magic shift key, we ask before booting */
#define MAGIC_SHIFT 0x2		/* left shift */

/* magic number to show that we have captured the reset vector */
#define RES_MAGIC 0x31415926L

static void xbra_install P_((xbra_vec *, long, long ARGS_ON_STACK (*)()));
static void init_intr P_((void));
static long getmch P_((void));
static void do_line P_((char *));
static void do_file P_((int));
static void shutmedown P_((PROC *));
void shutdown P_((void));
static void doset P_((char *,char *));
static long ARGS_ON_STACK mint_criticerr P_((long));
static void ARGS_ON_STACK do_exec_os P_((register long basepage));

static int gem_active;	/* 0 if AES has not started, nonzero otherwise */

#define EXEC_OS 0x4feL
static int  check_for_gem P_((void));
static void run_auto_prgs P_((void));

#ifdef LATTICE
/*
 * AGK: this is witchcraft to completely replace the startup code for
 * Lattice; doing so saves around 10K on the final binary and pulls only
 * long division & multitplication from the library (and not even those
 * if you compile for native '030). The drawback of this code is it
 * passes no environment or command line whatsoever. Since I always
 * set MiNT options & environment in 'mint.cnf' this is not a personal
 * downer, however at some point in the future we ought to have a kernel
 * parseargs() like call which sets these things up.
 */ 
BASEPAGE *_base;

static void
start(BASEPAGE *bp)
{
	long shrinklen;
	
	_base = bp;
	shrinklen = bp->p_tlen + bp->p_dlen + bp->p_blen + STACK + 0x100;
	if (bp->p_lowtpa + shrinklen <= bp->p_hitpa) {
		static char null[1] = {""};
		static char *argv[2] = {null, NULL};
		extern __builtin_putreg P_((int, long));	/* totally bogus */

		__builtin_putreg(15, bp->p_lowtpa + shrinklen);
		Mshrink((void *)bp->p_lowtpa, shrinklen);
		main(1, argv);
	}
	Pterm(ENSMEM);
}
#endif

#if defined(__GNUC__) || defined(__MINT__)
long _stksize = STACK;
#ifndef PROFILING
#include <minimal.h>
#endif
#endif

int curs_off = 0;	/* set if we should turn the cursor off when exiting */
int mint_errno = 0;	/* error return from open and creat filesystem calls */

/*
 * AGK: for proper co-processors we must consider saving their context.
 * This variable when non-zero indicates that the BIOS considers a true
 * coprocessor to be present. We use this variable in the context switch
 * code to decide whether to attempt an FPU context save.
 */
short fpu = 0;

/*
 * "mch" holds what kind of machine we are running on
 */
long mch = 0;

/*
 * "screen_boundary+1" tells us how screens must be positioned
 * (to a 256 byte boundary on STs, a 16 byte boundary on other
 * machines; actually, 16 bytes is conservative, 4 is probably
 * OK, but it doesn't hurt to be cautious). The +1 is because
 * we're using this as a mask in the ROUND() macro in mem.h.
 */
int screen_boundary = 255;

/*
 * variable holds processor type
 */
long mcpu = 0;

/*
 * variable holds language preference
 */
int gl_lang = -1;

/*
 * variable set if someone has already installed an flk cookie
 */
int flk = 0;

/*
 * variable set to 1 if the _VDO cookie indicates Falcon style video
 */
int FalconVideo;

/* program to run at startup */
#ifdef MULTITOS
static int init_is_gem = 1;	/* set to 1 if init_prg is GEM */
#else
static int init_is_gem = 0;	/* set to 1 if init_prg is GEM */
BASEPAGE *gem_base = 0;
long gem_start = 0;
#endif
static const char *init_prg = 0;

/* note: init_tail is also used as a temporary stack for resets in
 * intr.spp
 */
char init_tail[256];

/* initial environment for that program */
static char *init_env = 0;
/* temporary pointer into that environment for setenv */
static char *env_ptr;
/* length of the environment */
static long env_len;

/* GEMDOS pointer to current basepage */
BASEPAGE **tosbp;

/* pointer to the BIOS keyboard shift variable */
extern char *kbshft;	/* see bios.c */
extern BCONMAP2_T *bconmap2;	/* bconmap struct, see bios.c */

/* version of TOS we're running over */
int tosvers;

/* structures for keyboard/MIDI interrupt vectors */
KBDVEC *syskey, oldkey;
xbra_vec old_ikbd;			/* old ikbd vector */

/* values the user sees for the DOS, BIOS, and XBIOS vectors */
long save_dos, save_bios, save_xbios;

/* values for original system vectors */
xbra_vec old_dos, old_bios, old_xbios, old_timer, old_vbl, old_5ms;
xbra_vec old_criticerr;
xbra_vec old_execos;

long old_term;

xbra_vec old_resvec;	/* old reset vector */
long old_resval;	/* old reset validation */

#ifdef EXCEPTION_SIGS
/* bus error, address error, illegal instruction, etc. vectors */
xbra_vec old_bus, old_addr, old_ill, old_divzero, old_trace, old_priv;
xbra_vec old_linef, old_chk, old_trapv, old_mmuconf, old_format, old_cpv;
xbra_vec old_uninit, old_spurious, old_fpcp[7], old_pmmuill, old_pmmuacc;
#endif

/* BIOS disk vectors */
xbra_vec old_mediach, old_getbpb, old_rwabs;

/* BIOS drive map */
long olddrvs;

extern Func bios_tab[], dos_tab[];

/* kernel info that is passed to loaded file systems and device drivers */

struct kerinfo kernelinfo = {
	MAJ_VERSION, MIN_VERSION,
	DEFAULT_MODE, 0,
	bios_tab, dos_tab,
	changedrv,
	Trace, Debug, ALERT, FATAL,
	kmalloc, kfree, umalloc, ufree,
	strnicmp, stricmp, strlwr, strupr, ksprintf,
	ms_time, unixtim, dostim,
	nap, sleep, wake, wakeselect,
	denyshare, denylock, addtimeout, canceltimeout,
	addroottimeout, cancelroottimeout,
	ikill,iwake
};

/* table of processor frame sizes in _words_ (not used on MC68000) */
unsigned char framesizes[16] = {
/*0*/	0,	/* MC68010/M68020/M68030/M68040 short */
/*1*/	0,	/* M68020/M68030/M68040 throwaway */
/*2*/	2,	/* M68020/M68030/M68040 instruction error */
/*3*/	2,	/* M68040 floating point post instruction */
/*4*/	3,	/* MC68LC040/MC68EC040 unimplemented floating point instruction */
/*5*/	0,	/* NOTUSED */
/*6*/	0,	/* NOTUSED */
/*7*/	26,	/* M68040 access error */	
/*8*/	25,	/* MC68010 long */	
/*9*/	6,	/* M68020/M68030 mid instruction */
/*A*/	12,	/* M68020/M68030 short bus cycle */
/*B*/	42,	/* M68020/M68030 long bus cycle */
/*C*/	8,	/* CPU32 bus error */
/*D*/	0,	/* NOTUSED */
/*E*/	0,	/* NOTUSED */
/*F*/	13	/* 68070 and 9xC1xx microcontroller address error */
};

/* TOS and MiNT cookie jars, respectively. See the comments and code 
 * after main() for further details
 */

COOKIE *oldcookie, *newcookie;

/*
 * install a new vector for address "addr", using the XBRA protocol.
 * must run in supervisor mode!
 */

static void
xbra_install(xv, addr, func)
	xbra_vec *xv;
	long addr;
	long ARGS_ON_STACK (*func)();
{
	xv->xbra_magic = XBRA_MAGIC;
	xv->xbra_id = MINT_MAGIC;
	xv->jump = JMP_OPCODE;
	xv->this = func;
	xv->next = *((struct xbra **)addr);
	*((short **)addr) = &xv->jump;
}

/*
 * MiNT critical error handler; all it does is to jump through
 * the vector for the current process
 */

static long ARGS_ON_STACK
mint_criticerr(error)
	long error;	/* high word is error, low is drive */
{
	return (*curproc->criticerr)(error);
}

/*
 * if we are MultiTOS, and if we are running from the AUTO folder,
 * then we grab the exec_os vector and use that to start GEM; that
 * way programs that expect exec_os to act a certain way will still
 * work.
 * NOTE: we must use Pexec instead of p_exec here, because we will
 * be running in a user context (that of process 1, not process 0)
 */

static void ARGS_ON_STACK
do_exec_os(basepage)
	register long basepage;
{
	register long r;

/* if the user didn't specify a startup program, jump to the ROM */
	if (!init_prg) {
		register void ARGS_ON_STACK (*f) P_((long));
		f = (void ARGS_ON_STACK (*) P_((long))) old_execos.next;
		(*f)(basepage);
		Pterm0();
	} else {		

/* we have to set a7 to point to lower in our TPA; otherwise we would
 * bus error right after the Mshrink call!
 */
		setstack(basepage+500L);
#if defined(__TURBOC__) && !defined(__MINT__)
		Mshrink(0, (void *)basepage, 512L);
#else
		Mshrink((void *)basepage, 512L);
#endif
		r = Pexec(200, (char *)init_prg, init_tail, init_env);
		Pterm((int)r);
	}
}


/* initialize all interrupt vectors and new trap routines
 * we also get here any TOS variables that we're going to change
 * (e.g. the pointer to the cookie jar) so that rest_intr can
 * restore them.
 */

static void
init_intr()
{
	extern long ARGS_ON_STACK mint_bios();
	extern long ARGS_ON_STACK mint_dos();
	extern long ARGS_ON_STACK mint_timer();
	extern long ARGS_ON_STACK mint_vbl();
	extern long ARGS_ON_STACK mint_5ms();
	extern long ARGS_ON_STACK mint_xbios();
	extern long ARGS_ON_STACK reset();
  	extern long ARGS_ON_STACK new_ikbd();
  	extern long ARGS_ON_STACK new_bus();
  	extern long ARGS_ON_STACK new_addr();
  	extern long ARGS_ON_STACK new_ill();
  	extern long ARGS_ON_STACK new_divzero();
  	extern long ARGS_ON_STACK new_trace();
  	extern long ARGS_ON_STACK new_priv();
  	extern long ARGS_ON_STACK new_linef();
  	extern long ARGS_ON_STACK new_chk();
  	extern long ARGS_ON_STACK new_trapv();
  	extern long ARGS_ON_STACK new_fpcp();
  	extern long ARGS_ON_STACK new_mmu();
  	extern long ARGS_ON_STACK new_format();
  	extern long ARGS_ON_STACK new_cpv();
  	extern long ARGS_ON_STACK new_uninit();
  	extern long ARGS_ON_STACK new_spurious();
  	extern long ARGS_ON_STACK new_pmmuacc();
	short savesr;
	int i;

	syskey = (KBDVEC *)Kbdvbase();
	oldkey = *syskey;

	xbra_install(&old_ikbd, (long)(&syskey->ikbdsys), new_ikbd);

/* gratuitous (void *) for Lattice */
	old_term = (long)Setexc(0x102, (void *)-1UL);

	savesr = spl7();

	xbra_install(&old_dos, 0x84L, mint_dos);
	save_dos = (long)old_dos.next;

	xbra_install(&old_bios, 0xb4L, mint_bios);
	save_bios = (long)old_bios.next;

	xbra_install(&old_xbios, 0xb8L, mint_xbios);
	save_xbios = (long)old_xbios.next;

	xbra_install(&old_timer, 0x400L, mint_timer);
	xbra_install(&old_criticerr, 0x404L, mint_criticerr);
	xbra_install(&old_5ms, 0x114L, mint_5ms);
	xbra_install(&old_vbl, 4*0x1cL, mint_vbl);
	xbra_install(&old_resvec, 0x42aL, reset);
	old_resval = *((long *)0x426L);
	*((long *)0x426L) = RES_MAGIC;

	spl(savesr);

#ifdef EXCEPTION_SIGS
/* set up signal handlers */
	xbra_install(&old_bus, 8L, new_bus);
	xbra_install(&old_addr, 12L, new_addr);
	xbra_install(&old_ill, 16L, new_ill);
	xbra_install(&old_divzero, 20L, new_divzero);
	xbra_install(&old_trace, 36L, new_trace);
	xbra_install(&old_priv, 32L, new_priv);
	if (tosvers >= 0x106)
		xbra_install(&old_linef, 44L, new_linef);
	xbra_install(&old_chk, 24L, new_chk);
	xbra_install(&old_trapv, 28L, new_trapv);
	for (i = (int)(sizeof(old_fpcp) / sizeof(old_fpcp[0])); i--; ) {
		xbra_install(&old_fpcp[i], 192L + i * 4, new_fpcp);
	}
	xbra_install(&old_mmuconf, 224L, new_mmu);
	xbra_install(&old_pmmuill, 228L, new_mmu);
	xbra_install(&old_pmmuacc, 232L, new_pmmuacc);
	xbra_install(&old_format, 56L, new_format);
	xbra_install(&old_cpv, 52L, new_cpv);
	xbra_install(&old_uninit, 60L, new_uninit);
	xbra_install(&old_spurious, 96L, new_spurious);
#endif

/* set up disk vectors */
	xbra_install(&old_mediach, 0x47eL, new_mediach);
	xbra_install(&old_rwabs, 0x476L, new_rwabs);
	xbra_install(&old_getbpb, 0x472L, new_getbpb);
	olddrvs = *((long *)0x4c2L);

/* set up cookie jar */
	oldcookie = *CJAR;	/* CJAR defined in cookie.h */
	install_cookies();
}

/* restore all interrupt vectors and trap routines */
/*
 * NOTE: This is *not* the approved way of unlinking XBRA trap handlers.
 * Normally, one should trace through the XBRA chain. However, this is
 * a very unusual situation: when MiNT exits, any TSRs or programs running
 * under MiNT will no longer exist, and so any vectors that they have
 * caught will be pointing to never-never land! So we do something that
 * would normally be considered rude, and restore the vectors to
 * what they were before we ran.
 * BUG: we should restore *all* vectors, not just the ones that MiNT caught.
 */

void
restr_intr()
{
	short savesr;
	int i;

	savesr = spl7();
	*syskey = oldkey;		/* restore keyboard vectors */
	*tosbp = _base;			/* restore GEMDOS basepage pointer */
	*CJAR = oldcookie;		/* restore old cookie jar */

#ifdef EXCEPTION_SIGS
	*((long *)0x08L) = (long) old_bus.next;
	*((long *)0x0cL) = (long) old_addr.next;
	*((long *)0x10L) = (long) old_ill.next;
	*((long *)0x14L) = (long) old_divzero.next;
	*((long *)0x20L) = (long) old_priv.next;
	*((long *)0x24L) = (long) old_trace.next;
	if (old_linef.next)
		*((long *)0x2cL) = (long) old_linef.next;
	*((long *)0x18L) = (long) old_chk.next;
	*((long *)0x1cL) = (long) old_trapv.next;
	for (i = (int)(sizeof(old_fpcp) / sizeof(old_fpcp[0])); i--; ) {
		((long *)0xc0L)[i] = (long) old_fpcp[i].next;
	}
	*((long *)0xe0L) = (long) old_mmuconf.next;
	*((long *)0xe4L) = (long) old_pmmuill.next;
	*((long *)0xe8L) = (long) old_pmmuacc.next;
	*((long *)0x38L) = (long) old_format.next;
	*((long *)0x34L) = (long) old_cpv.next;
	*((long *)0x3cL) = (long) old_uninit.next;
	*((long *)0x60L) = (long) old_spurious.next;
#endif
	*((long *)0x84L) = (long) old_dos.next;
	*((long *)0xb4L) = (long) old_bios.next;
	*((long *)0xb8L) = (long) old_xbios.next;
	*((long *)0x408L) = old_term;
	*((long *)0x404L) = (long) old_criticerr.next;
	*((long *)0x114L) = (long) old_5ms.next;
	*((long *)0x400L) = (long) old_timer.next;
	*((long *)0x70L) = (long) old_vbl.next;
	*((long *)0x426L) = old_resval;
	*((long *)0x42aL) = (long) old_resvec.next;
	*((long *)0x476L) = (long) old_rwabs.next;
	*((long *)0x47eL) = (long) old_mediach.next;
	*((long *)0x472L) = (long) old_getbpb.next;
	*((long *)0x4c2L) = olddrvs;

	spl(savesr);
}


/* we save the TOS supervisor stack pointer so that we can reset it when
   calling Pterm() (not that anyone will ever want to leave MiNT :-)).
 */

long tosssp;		/* TOS supervisor stack pointer */


/*
 * enter_kernel: called every time we enter the MiNT kernel via a trap
 * call. Sets up the GEMDOS and BIOS vectors to point to TOS, and
 * sets up other vectors and system variables appropriately. Note that
 * calling enter_kernel multiple times is probably NOT a good idea,
 * but the code will allow it.
 * The parameter is a flag telling us whether or not this is a GEMDOS
 * call; the BIOS uses this for checking security of Rwabs.
 */

short in_kernel = 0;

void ARGS_ON_STACK
enter_kernel(isGEMDOS)
	int isGEMDOS;
{
	short save_sr;

	if (in_kernel) return;

	save_sr = spl7();
	curproc->in_dos = isGEMDOS;
	save_dos = *((long *) 0x84L);
	save_bios = *((long *) 0xb4L);
	save_xbios = *((long *) 0xb8L);
	*((long *) 0x84L) = (long)old_dos.next;
	*((long *) 0xb4L) = (long)old_bios.next;
	*((long *) 0xb8L) = (long)old_xbios.next;
	*tosbp = _base;

	in_kernel = 1;
	spl(save_sr);
}

/*
 * leave_kernel: called before leaving the kernel, either back to
 * user mode or when calling a signal handler or the GEMDOS
 * terminate vector. Note that interrupts should be disabled before
 * this routine is called.
 */

void ARGS_ON_STACK
leave_kernel()
{
	*((long *) 0x84L) = save_dos;
	*((long *) 0xb4L) = save_bios;
	*((long *) 0xb8L) = save_xbios;
	*tosbp = curproc->base;
	in_kernel = 0;
	curproc->in_dos = 0;
}

/*
 * shut down processes; this involves waking them all up, and sending
 * them SIGTERM to give them a chance to clean up after themselves
 */

static void
shutmedown(p)
	PROC *p;
{
	UNUSED(p);
	curproc->wait_cond = 0;
}

void
shutdown()
{
	PROC *p;
	int proc_left = 0;

	curproc->sighandle[SIGCHLD] = SIG_IGN;

	for (p = proclist; p; p = p->gl_next) {
		if (p->pid == 0) continue;
		if (p->wait_q != ZOMBIE_Q && p->wait_q != TSR_Q) {
			if (p->wait_q != READY_Q) {
				short sr = spl7();
				rm_q(p->wait_q, p);
				add_q(READY_Q, p);
				spl(sr);
			}
			post_sig(p, SIGTERM);
			proc_left++;
		}
	}

	if (proc_left) {
		/* sleep a little while, to give the other processes a chance to
		   shut down
		 */

		addtimeout(1000, shutmedown);
		do {
			sleep(WAIT_Q, (long)shutdown);
		} while (curproc->wait_cond == (long)shutdown);
	}
}

#if defined(__GNUC__) || defined(__MINT__)
int
main(argc, argv, envp)
	int argc;
	char **argv, **envp;
#else
int
main(argc, argv)
	int argc;
	char **argv;
#endif
{
	long *sysbase;
	long r;
	extern int debug_level, debug_logging;	/* in debug.c */
	extern int no_mem_prot;		/* memprot.c */
	extern const char *greet1, *greet2;
					/* welcome.c */
	static char buf[SPRINTF_MAX];
	static char curpath[128];
	long yn;
	FILEPTR *f;

#if defined(__GNUC__) || defined(__MINT__)
	UNUSED(envp);
#endif

/* figure out what kind of machine we're running on */
/* biosfs wants to know this; also sets no_mem_prot */
/* 920625 kbad put it here so memprot_warning can be intelligent */
	(void)Supexec(getmch);
#ifdef ONLY030
	if (mcpu != 30) {
		Cconws("\r\nThis version of MiNT requires a 68030.\r\n");
		Cconws("Hit any key to continue.\r\n");
		(void)Cconin();
		Pterm0();
	}
#endif

/* Ask the user if s/he wants to boot MiNT */
	if ((Kbshift(-1) & MAGIC_SHIFT) == MAGIC_SHIFT) {
		yn = boot_kernel_p();
		Cconws("\r\n");
		if (!yn)
			Pterm0();
	}

	if (argv[0][0] == 0) {	/* maybe started from the desktop */
		curs_off = 1;
	}

	yn = 0; /* by default, don't print basepage */
	--argc, ++argv;
	while (argc && **argv == '-') {
		if (argv[0][1] >= '0' && argv[0][1] <= '9') {
		/* a number sets out_device to that device */
			extern int out_device;
			out_device = (int)atol(&argv[0][1]);
		}
		else if (argv[0][1] == 'b') {
		/* print MiNT basepage */
			yn++;
		}
		else if (argv[0][1] == 'd') {
		/* -d increases debugging level */
			debug_level++;
		}
		else if (argv[0][1] == 'm' || argv[0][1] == 'p') {
			int givenotice = (argv[0][2] != 'w');
		/* -m and -p turn off memory protection */
		extern const char *memprot_notice, *memprot_warning;
			if (no_mem_prot) {
			    if (givenotice)
				Cconws(memprot_notice);
			}
			else {
			    no_mem_prot = 1;
			    if (givenotice)
				Cconws(memprot_warning);
			}
		}
		else if (argv[0][1] == 'l') {
		/* -l turns on debug logging */
			debug_logging = 1;
		}
		else {
			Cconws("Unknown argument (ignored): ");
			Cconws(*argv);
			Cconws("\r\n");
		}
		++argv, --argc;
	}
	if (argc) {
		Cconws("Unknown argument ignored: ");
		Cconws(*argv);
		Cconws(" (and all the rest)\r\n");
        }

/* greetings */
	Cconws(greet1);
	ksprintf(buf, VERS_STRING, MAJ_VERSION, MIN_VERSION);
	Cconws(buf);
	Cconws(greet2);

#ifdef __TURBOC__
	Cconws("PRELIMINARY PureC compiled version!\r\n");
#endif

	if (yn)
	{
    	    ksprintf(buf,"MiNT@%lx\r\nhit a key...",_base);
	    Cconws(buf);
	    (void)Crawcin();
	    Cconws("\r\033K");
	}

#ifdef notdef
/* if less than 1 megabyte free, turn off memory protection */
	if (Mxalloc(-1L, 3) < ONE_MEG && !no_mem_prot) {
		extern const char *insuff_mem_warning;
		Cconws(insuff_mem_warning);
		no_mem_prot = 1;
	}
#endif

/* look for ourselves as \AUTO\MINTNP.PRG; if so, we turn memory
 * protection off
 */
	if (!no_mem_prot && Fsfirst("\\AUTO\\MINTNP.PRG",0) == 0)
		no_mem_prot = 1;

/* check for GEM -- this must be done from user mode */
	gem_active = check_for_gem();

/*
 * get the current directory, so that we can switch back to it after
 * the file systems are properly initialized
 */
/* set the current directory for the current process */
	(void)Dgetpath(curpath, 0);
	if (!*curpath) {
		curpath[0] = '\\';
		curpath[1] = 0;
	}
	tosssp = (long)Super(0L);	/* enter supervisor mode */
	if (!no_mem_prot)
		save_mmu();		/* save current MMU setup */

/* get GEMDOS pointer to current basepage */
/* 0x4f2 points to the base of the OS; here we can find the OS compilation
   date, and (in newer versions of TOS) where the current basepage pointer
   is kept; in older versions of TOS, it's at 0x602c
 */
	sysbase = *((long **)(0x4f2L));	/* gets the RAM OS header */
	sysbase = (long *)sysbase[2];	/* gets the ROM one */

	tosvers = (int)(sysbase[0] & 0x0000ffff);
	if (tosvers == 0x100) {
		if ((sysbase[7] & 0xfffe0000L) == 0x00080000L)
			tosbp = (BASEPAGE **)0x873cL;	/* SPANISH ROM */
		else
			tosbp = (BASEPAGE **) 0x602cL;
		kbshft = (char *) 0x0e1bL;
	} else {
		tosbp = (BASEPAGE **) sysbase[10];
		kbshft = (char *) sysbase[9];
	}

	if (tosvers >= 0x0400 && tosvers <= 0x404) {
		bconmap2 = (BCONMAP2_T *)Bconmap(-2);
		if (bconmap2->maptabsize == 1) {
			/* Falcon BIOS Bconmap is busted */
			bconmap2->maptabsize = 3;
		}
		has_bconmap = 1;
	} else {
/* The TT TOS release notes are wrong... this is the real way to test
 * for Bconmap ability
 */
		has_bconmap = (Bconmap(0) == 0);
		if (has_bconmap)
			bconmap2 = (BCONMAP2_T *)Bconmap(-2);
	}

/* initialize memory */
	init_mem();

/* initialize the basic file systems */
	timestamp = Tgettime();
	datestamp = Tgetdate();
	init_filesys();

/* initialize processes */
	init_proc();

/* initialize system calls */
	init_dos();
	init_bios();
	init_xbios();

/* NOTE: there's a call to kmalloc embedded in install_cookies, which
 * is called by init_intr; so make sure this is the last of the
 * init_* things called!
 */
	init_intr();
	enter_kernel(1);	/* we'll be making GEMDOS calls */

#if 0
	if (!gem_active) {
/* make MiNT invisible in the basepage chain, so that
 * programs that rely on a certain basepage chain
 * structure to determine whether or not they were run
 * from the desktop will have a better chance of working.
 * NOTE THAT THIS IS ONLY DONE TO HELP OUT BRAIN-DAMAGED
 * SOFTWARE: do *not* try counting basepages to figure
 * out whether or not you were run from the desktop!!!
 */
		rootproc->base = _base->p_parent;
	} else
#endif
		rootproc->base = _base;

/* set up standard file handles for the current process
 * do this here, *after* init_intr has set the Rwabs vector,
 * so that AHDI doesn't get upset by references to drive U:
 */
	f = do_open("U:\\DEV\\CONSOLE", O_RDWR, 0, (XATTR *)0);
	if (!f) {
		FATAL("unable to open CONSOLE device");
	}
	curproc->control = f;
	curproc->handle[0] = f;
	curproc->handle[1] = f;
	f->links = 3;

	f = do_open("U:\\DEV\\MODEM1", O_RDWR, 0, (XATTR *)0);
	curproc->aux = f;
	((struct tty *)f->devinfo)->aux_cnt = 1;
	f->pos = 1;	/* flag for close to --aux_cnt */
	if (has_bconmap) {
	/* If someone has already done a Bconmap call, then
	 * MODEM1 may no longer be the default
	 */
		bconmap(curbconmap);
		f = curproc->aux;	/* bconmap can change curproc->aux */
	}
	if (f) {
		curproc->handle[2] = f;
		f->links++;
	}
	f = do_open("U:\\DEV\\CENTR", O_RDWR, 0, (XATTR *)0);
	if (f) {
		curproc->handle[3] = curproc->prn = f;
		f->links = 2;
	}
	f = do_open("U:\\DEV\\MIDI", O_RDWR, 0, (XATTR *)0);
	if (f) {
		curproc->midiin = curproc->midiout = f;
		((struct tty *)f->devinfo)->aux_cnt = 1;
		f->pos = 1;	/* flag for close to --aux_cnt */
		f->links = 2;
	}

/* load external file systems */
/* set path first to make sure that MiNT's directory matches
 * GEMDOS's
 */
	d_setpath(curpath);
	
	load_devdriver();

#ifndef PROFILING
/* load_filesys causes media changes :-( */
	load_filesys();
#endif

/* note that load_filesys changed the
 * directory on us!!
 */
	(void)d_setpath(curpath);
	
/* load the configuration file */
	load_config();

	*((long *)0x4c2L) |= PSEUDODRVS;

	if (init_env == 0)
		init_env = (char *)_base->p_env;

/* empty environment? Set the PATH variable to the root of the current drive */
	if (init_env[0] == 0) {
		static char path_env[] = "PATH=\0C:\0";
		path_env[6] = curproc->curdrv + 'A';
		init_env = path_env;
	}

/* if we are MultiTOS, we're running in the AUTO folder, and our INIT is
 * in fact GEM, take the exec_os() vector. (We know that INIT is GEM
 * if the user told us so by using GEM= instead of INIT=.)
 */
	if (!gem_active && init_is_gem) {
		xbra_install(&old_execos, EXEC_OS, (long ARGS_ON_STACK (*)())do_exec_os);
	}

/* run any programs appearing after us in the AUTO folder */
	run_auto_prgs();

/* run the initial program */
/* if that program is in fact GEM, we start it via exec_os, otherwise
 * we do it with Pexec.
 * the logic is: if the user specified init_prg, and it is not
 * GEM, then we try to execute it; if it *is* GEM (e.g. gem.sys),
 * then we try to execute it if gem is already active, otherwise
 * we jump through the exec_os vector (which we grabbed above) in
 * order to start it. We *never* go through exec_os if we're not in
 * the AUTO folder.
 */
	if (init_prg && (!init_is_gem || gem_active)) {
		r = p_exec(0, (char *)init_prg, init_tail, init_env);
	} else if (!gem_active) {   
		BASEPAGE *bp; int pid;
		bp = (BASEPAGE *)p_exec(7,
		  (char *)((long)F_FASTLOAD | F_ALTLOAD | F_ALTALLOC | F_PROT_S),
		  (char *)"\0", init_env);
		bp->p_tbase = *((long *) EXEC_OS );
#ifndef MULTITOS
		if (((long *) sysbase[5])[0] == 0x87654321)
		  gem_start = ((long *) sysbase[5])[2];
		gem_base = bp;
#endif
		r = p_exec(106, (char *)"GEM", bp, 0L);
		pid = (int)r;
		if (pid > 0) {
			do {
				r = p_wait3(0, (long *)0);
			} while(pid != ((r & 0xffff0000L) >> 16));
			r &= 0x0000ffff;
		}
	} else {
Cconws("If MiNT is run after GEM starts, you must specify a program\r\n");
Cconws("to run initially in MINT.CNF, with an INIT= line\r\n");
			r = 0;
	}

	if (r < 0 && init_prg) {
		ksprintf(buf, "FATAL: couldn't run %s\r\n", init_prg);
		Cconws(buf);
	}

	if (r) {
		ksprintf(buf, "exit code: %ld\r\n", r);
		Cconws(buf);
	}

	rootproc->base = _base;

/* shut down all processes gracefully */
	shutdown();

/* put everything back and exit */
	if (!gem_active && init_is_gem) {
	/* we stole exec_os above */
		*((long *)EXEC_OS) = (long)old_execos.next;
	}
	restr_intr();
	close_filesys();
	if (!no_mem_prot)
		restr_mmu();
	restr_screen();

	(void)Super((void *)tosssp);	/* gratuitous (void *) for Lattice */
	Cconws("leaving MiNT\r\n");

	if (curs_off)
		Cconws("\033f");	/* disable cursor */

	return 0;
}


/*
 * cookie jar handling routines. The "cookie jar" is an area of memory
 * reserved by TOS for TSR's and utility programs; the idea is that
 * you put a cookie in the jar to notify people of available services.
 * The BIOS uses the cookie jar in TOS 1.6 and higher; for earlier versions
 * of TOS, the jar is always empty (unless someone added a cookie before
 * us; POOLFIX does, for example).
 * MiNT establishes an entirely new cookie jar (with the old cookies copied
 * over) and frees it on exit. That's because TSR's run under MiNT
 * will no longer be accessible after MiNT exits.
 * MiNT also puts a cookie in the jar, with tag field 'MiNT' (of course)
 * and with the major version of MiNT in the high byte of the low word,
 * and the minor version in the low byte.
 */

void
install_cookies()
{
	COOKIE *cookie;
	int i, ncookies;
	long ncsize;
	extern long rsvf;

	/* note that init_intr sets oldcookie to the old cookie jar */

	ncookies = 0;
	cookie = oldcookie;
	if (cookie) {
		while (cookie->tag.aslong != 0) {
		/* check for true FPU co-processor */
			if (!strncmp(cookie->tag.aschar, "_FPU",4) &&
				 (cookie->value >> 16) >= 2)
				fpu = 1;
		/* check for _FLK cookie */
			else if (!strncmp(cookie->tag.aschar, "_FLK",4))
				flk = 1;
		/* ..and for RSVF */
			else if (!strncmp(cookie->tag.aschar, "RSVF",4))
				rsvf = cookie->value;
			cookie++; ncookies++;
		}
	}

	/*
	 * We allocate the cookie jar in global memory so anybody can read
	 * it or write it. This code allocates at least 8 more cookies, and
	 * then rounds up to a QUANTUM boundary (that's what ROUND does). 
	 * Probably, nobody will have to allocate another cookie jar :-)
	 */

	/* NOTE: obviously, we can do this only if init_intr is called
	 * _after_ memory, processes, etc. have been initialized
	 */
	ncsize = (ncookies+8)*sizeof(COOKIE);
	ncsize = ROUND(ncsize);
	newcookie = (COOKIE *)alloc_region(core, ncsize, PROT_G);

/* copy the old cookies to the new jar */

	for (i = 0, cookie = oldcookie; i < ncookies;) {
/*
 * but don't copy RSVF, MiNTs /dev is for real...
 * (if you want to know whats in there use ls :)
 */
		if (!strncmp(cookie->tag.aschar, "RSVF",4)) {
			++cookie, --ncookies;
			continue;
		}
		newcookie[i++] = *cookie++;
	}

/* install MiNT cookie */
	strncpy(newcookie[i].tag.aschar, "MiNT", 4);
	newcookie[i].value = (MAJ_VERSION << 8) | MIN_VERSION;
	i++;

/* install _FLK cookie to indicate that file locking works */
	if (!flk) {
		strncpy(newcookie[i].tag.aschar, "_FLK", 4);
		newcookie[i].value = 0;
		i++;
	}

/* jr: install PMMU cookie if memory protection is used */
	if (!no_mem_prot) {
		strncpy(newcookie[i].tag.aschar, "PMMU", 4);
		newcookie[i].value = 0;
		i++;
	}

/* the last cookie should have a 0 tag, and a value indicating the number
 * of slots, total
 */

	newcookie[i].tag.aslong = 0;
	newcookie[i].value = ncsize/sizeof(COOKIE);

	*CJAR = newcookie;

}

/*
 * Get the value of the _MCH cookie, if one exists; also set no_mem_prot if
 * there's a _CPU cookie and you're not on an '030, or if there is none.
 * This must be done in a separate routine because the machine type and CPU
 * type are needed when initializing the system, whereas install_cookies is
 * not called until everything is practically up.
 * In fact, getmch() should be called before *anything* else is
 * initialized, so that if we find a MiNT cookie already in the
 * jar we can bail out early and painlessly.
 */

static long
getmch()
{
	COOKIE *jar;
	int foundcpu = 0;
	int i;
	long *sysbase;
	extern int no_mem_prot;

	mcpu = 0;
	jar = *CJAR;	/* CJAR defined in cookie.h */
	if (jar) {
		while (jar->tag.aslong != 0) {
		/* check for machine type */
			if (!strncmp(jar->tag.aschar, "_MCH",4)) {
				mch = jar->value;
			} else if (!strncmp(jar->tag.aschar, "_CPU",4)) {
	    			/* if not '030 then no memory protection */
				mcpu = jar->value;
	    			if (jar->value != 30) no_mem_prot = 1;
	    			foundcpu = 1;
			} else if (!strncmp(jar->tag.aschar, "_VDO",4)) {
				FalconVideo = (jar->value == 0x00030000L);
				if (jar->value & 0xffff0000L)
					screen_boundary = 15;
			} else if (!strncmp(jar->tag.aschar, "MiNT",4)) {
				Cconws("MiNT is already installed!!\r\n");
				Pterm(2);
			} else if (!strncmp(jar->tag.aschar, "_AKP",4)) {
				gl_lang = (int) ((jar->value >> 8) & 0x00ff);
			} else if (!strncmp(jar->tag.aschar, "PMMU",4)) {
				/* jr: if PMMU cookie exists, someone else is
				   already using the PMMU */
				Cconws ("MiNT: PMMU already in use, memory protection turned off.\r\n");
				no_mem_prot = 1;
			}
			jar++;
		}
	}
	if (!foundcpu) no_mem_prot = 1;
/*
 * if no preference found, look at the country code to decide
 */
	if (gl_lang < 0) {
		sysbase = *((long **)(0x4f2L)); /* gets the RAM OS header */
		sysbase = (long *)sysbase[2];	/* gets the ROM one */
		i = (int) ((sysbase[7] & 0x7ffe0000L) >> 17L);
		switch(i) {
		case 1:		/* Germany */
		case 8:		/* Swiss German */
			gl_lang = 1;
			break;
		case 2:		/* France */
		case 7:		/* Swiss French */
			gl_lang = 2;
			break;
		case 4:		/* Spain */
			gl_lang = 4;
			break;
		case 5:		/* Italy */
			gl_lang = 5;
			break;
		default:
			gl_lang = 0;
			break;
		}
	}
	

	if (gl_lang >= MAXLANG || gl_lang < 0)
		gl_lang = 0;
	return 0L;
}

/*
 * routines for reading the configuration file
 * we allow the following commands in the file:
 * # anything		-- comment
 * INIT=file		-- specify boot program
 * CON=file		-- specify initial file/device for handles -1, 0, 1
 * PRN=file		-- specify initial file for handle 3
 * BIOSBUF=[yn]		-- if 'n' or 'N' then turn off BIOSBUF feature
 * DEBUG_LEVEL=n	-- set debug level to (decimal number) n
 * DEBUG_DEVNO=n	-- set debug device number to (decimal number) n
 * HARDSCROLL=n		-- set hard-scroll size to n, range 0-99.
 * SLICES=nnn		-- set multitasking granularity
 * echo message		-- print a message on the screen
 * alias drive path	-- make a fake drive pointing at a path
 * cd dir		-- change directory/drive
 * exec cmd args	-- execute a program
 * setenv name val	-- set up environment
 * sln file1 file2	-- create a symbolic link
 * ren file1 file2	-- rename a file
 *
 * BUG: if you use setenv in mint.cnf, *none* of the original environment
 * gets passed to children. This is rarely a problem if mint.prg is
 * in the auto folder.
 */

extern short bconbdev, bconbsiz;	/* from bios.c */

static void
doset(name, val)
	char *name, *val;
{
	char *t;

	if (!strcmp(name, "GEM")) {
		init_is_gem = 1;
		goto setup_init;
	} 
	if (!strcmp(name, "INIT")) {
		init_is_gem = 0;
setup_init:
		if (!*val) return;
		t = kmalloc(strlen(val)+1);
		if (!t) return;
		strcpy(t, val);
		init_prg = t;
		while (*t && !isspace(*t)) t++;
/* get the command tail, too */
		if (*t) {
			*t++ = 0;
			strncpy(init_tail+1, t, 125);
			init_tail[126] = 0;
			init_tail[0] = strlen(init_tail+1);
		}
		return;
	}
	if (!strcmp(name, "CON")) {
		FILEPTR *f;
		int i;

		f = do_open(val, O_RDWR, 0, (XATTR *)0);
		if (f) {
			for (i = -1; i < 2; i++) {
				do_close(curproc->handle[i]);
				curproc->handle[i] = f;
				f->links++;
			}
			f->links--;	/* correct for overdoing it */
		}
		return;
	}
	if (!strcmp(name, "PRN")) {
		FILEPTR *f;

		f = do_open(val, O_RDWR|O_CREAT|O_TRUNC, 0, (XATTR *)0);
		if (f) {
			do_close(curproc->handle[3]);
			do_close(curproc->prn);
			curproc->prn = curproc->handle[3] = f;
			f->links = 2;
		}
		return;
	}
	if (!strcmp(name, "BIOSBUF")) {
		if (*val == 'n' || *val == 'N') {
			if (bconbsiz) bflush();
			bconbdev = -1;
		}
		return;
	}
	if (!strcmp(name, "DEBUG_LEVEL")) {
		extern int debug_level;
		if (*val >= '0' && *val <= '9')
			debug_level = (int)atol(val);
		else ALERT("Bad arg to \"DEBUG_LEVEL\" in cnf file");
		return;
	}
	if (!strcmp(name, "DEBUG_DEVNO")) {
		extern int out_device;
		if (*val >= '0' && *val <= '9')
			out_device= (int)atol(val);
		else ALERT("Bad arg to \"DEBUG_DEVNO\" in cnf file");
		return;
	}

#ifdef FASTTEXT
	if (!strcmp(name, "HARDSCROLL")) {
		int i;
		extern int hardscroll;

		if (!strcmp(val, "AUTO")) {
			hardscroll = -1;
			return;
		}
		i = *val++;
		if (i < '0' || i > '9') return;
		hardscroll = i-'0';
		i = *val;
		if (i < '0' || i > '9') return;
		hardscroll = 10*hardscroll + i - '0';
		return;
	}
#endif
	if (!strcmp(name, "MAXMEM")) {
		long r;

		r = atol(val) * 1024L;
		if (r > 0)
			p_setlimit(2, r);
		return;
	}
	if (!strcmp(name, "SLICES")) {
		extern short time_slice;

		time_slice = atol(val);
		return;
	}

	if (!strcmp(name, "PSEUDODRIVES")) {
		FORCE("PSEUDODRIVES= no longer supported");
		return;
	}
	FORCE("Unknown variable `%s'", name);
}

/* Execute a line from the config file */
static void
do_line(line)
	char *line;
{
	char *cmd, *arg1, *arg2;
	char *newenv;
	char *t;
	int i;
	char delim;

	while (*line == ' ') line++;	/* skip whitespace at start of line */
	if (*line == '#') return;	/* ignore comments */
	if (!*line) return;		/* and also blank lines */

	cmd = line;
/* check for variable assignments (e.g. INIT=, etc.) */
/*
 * AGK: note we check for spaces whilst scanning so that an environment
 * variable may include an =, this has the unfortunate side effect that
 * the '=' _has_ to be concatenated to the variable name (INIT etc.)
 */
	for (t = cmd; *t && *t != ' '; t++) {
		if (*t == '=') {
			*t++ = 0;
			doset(cmd, t);
			return;
		}
	}

/* OK, assume a regular command; break it up into 'cmd', 'arg1', arg2' */

	while (*line && *line != ' ') line++;
	delim = ' ';
	if (*line) {
		*line++ = 0;
		while (*line == ' ') line++;
		if (*line == '"') {
			delim = '"';
			line++;
		}
	}

	if (!strcmp(cmd, "echo")) {
		c_conws(line); c_conws("\r\n");
		return;
	}
	arg1 = line;
	while (*line && *line != delim) line++;
	delim = ' ';
	if (*line) {
		*line++ = 0;
		while (*line == ' ') line++;
		if (*line == '"') {
			delim = '"';
			line++;
		}
	}
	if (!strcmp(cmd, "cd")) {
		int drv;
		(void)d_setpath(arg1);
		drv = toupper(*arg1) - 'A';
		if (arg1[1] == ':') (void)d_setdrv(drv);
		return;
	}
	if (!strcmp(cmd, "exec")) {
		char cmdline[128];
		int i;

		i = strlen(line);
		if (i > 126) i = 126;
		cmdline[0] = i;
		strncpy(cmdline+1, line, i);
		cmdline[i+1] = 0;
		i = (int)p_exec(0, arg1, cmdline, init_env);
		if (i == -33) {
			FORCE("%s: file not found", arg1);
		} else if (i < 0) {
			FORCE("%s: error while attempting to execute", arg1);
		}
		return;
	}
	if (!strcmp(cmd, "setenv")) {
		if (strlen(arg1) + strlen(line) + 4 + (env_ptr - init_env) >
							 env_len) {
			long j;

			env_len += 1024;
			newenv = (char *)m_xalloc(env_len, 0x13);
			if (init_env) {
				t = init_env;
				j = env_ptr - init_env;
				env_ptr = newenv;
				for (i = 0; i < j; i++)
					*env_ptr++ = *t++;
				if (init_env)
					m_free((virtaddr)init_env);
			} else {
				env_ptr = newenv;
			}
			init_env = newenv;
		}
		while (*arg1) {
			*env_ptr++ = *arg1++;
		}
		*env_ptr++ = '=';
		while (*line) {
			*env_ptr++ = *line++;
		}
		*env_ptr++ = 0;
		*env_ptr = 0;
		return;
	}
	if (!strcmp (cmd, "include")) {
	    long fd = f_open (arg1, 0);
	    if (fd < 0) {
		ALERT ("include: cannot open file %s", arg1);
		return;
	    }
	    do_file ((int)fd);
	    f_close ((int)fd);
	    return;
	}
	arg2 = line;
	while (*line && *line != delim) line++;
	if (*line) {
		*line = 0;
	}
	if (!strcmp(cmd, "alias")) {
		int drv;
		long r;
		fcookie root_dir;
		extern int aliasdrv[];

		drv = toupper(*arg1) - 'A';
		if (drv < 0 || drv >= NUM_DRIVES) {
			ALERT("Bad drive (%c:) in alias", drv+'A');
			return;
		}
		r = path2cookie(arg2, NULL, &root_dir);
		if (r) {
			ALERT("alias: TOS error %ld while looking for %s",
				r, arg2);
			return;
		}
		aliasdrv[drv] = root_dir.dev + 1;
		*((long *)0x4c2L) |= (1L << drv);
		release_cookie(&curproc->curdir[drv]);
		dup_cookie(&curproc->curdir[drv], &root_dir);
		release_cookie(&curproc->root[drv]);
		curproc->root[drv] = root_dir;
		return;
	}
	if (!strcmp(cmd, "sln")) {
		(void)f_symlink(arg1, arg2);
		return;
	}
	if (!strcmp(cmd, "ren")) {
		(void)f_rename(0, arg1, arg2);
		return;
	}
	FORCE("syntax error in mint.cnf near: %s", cmd);
}

#undef BUF
#undef LINE

#define BUF 512
#define LINE 256

static void
do_file(fd)
	int fd;
{
	long r;
	char buf[BUF+1], c;
	char line[LINE+1];
	char *from;
	int count = 0;

 	buf[BUF] = 0;
	from = &buf[BUF];
	line[LINE] = 0;

	for(;;) {
		c = *from++;
		if (!c) {
			r = f_read(fd, (long)BUF, buf);
			if (r <= 0) break;
			buf[r] = 0;
			from = buf;
		} else if (c == '\r') {
			continue;
		} else if (c == '\n') {
			line[count] = 0;
			do_line(line);
			count = 0;
		} else {
			if (count < LINE) {
				line[count++] = c;
			}
		}
	}
	if (count) {
		line[count] = 0;
		do_line(line);
	}
}

void
load_config()
{
	int fd;

	fd = (int) f_open("mint.cnf", 0);
	if (fd < 0)
		fd = (int) f_open("\\mint\\mint.cnf", 0);
	if (fd < 0)
		fd = (int) f_open("\\multitos\\mint.cnf", 0);
	if (fd < 0) return;
	do_file(fd);
	f_close(fd);
}

/*
 * run programs in the AUTO folder that appear after MINT.PRG
 * some things to watch out for:
 * (1) make sure GEM isn't active
 * (2) make sure there really is a MINT.PRG in the auto folder
 */

/*
 * some global variables used to see if GEM is active
 */
static short aes_intout[64];
static short aes_dummy[64];
static short aes_globl[15];
static short aes_cntrl[6] = { 10, 0, 1, 0, 0 };

short *aes_pb[6] = { aes_cntrl, aes_globl, aes_dummy, aes_intout,
		     aes_dummy, aes_dummy };

/* check for whether GEM is active; remember, this *must* be done in
 * user mode
 */

static int
check_for_gem()
{
	call_aes(aes_pb);	/* does an appl_init */
	return aes_globl[0];
}

static void
run_auto_prgs()
{
	DTABUF *dta;
	long r;
	static char pathspec[32] = "\\AUTO\\";
	short runthem = 0;	/* set to 1 after we find MINT.PRG */

/* if the AES is running, don't check AUTO */

	if (gem_active) {
		return;
	}

/* OK, now let's run through \\AUTO looking for
 * programs...
 */
	dta = (DTABUF *)f_getdta();
	r = f_sfirst("\\AUTO\\*.PRG", 0);
	while (r >= 0) {
		if (!strcmp(dta->dta_name, "MINT.PRG") ||
		    !strcmp(dta->dta_name, "MINTNP.PRG"))
			runthem = 1;
		else if (runthem) {
			strcpy(pathspec+6, dta->dta_name);
			(void)p_exec(0, pathspec, (char *)"", init_env);
		}
		r = f_snext();
	}
}
