/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
*/

/* routines for handling processes */

#include "mint.h"
#include "xbra.h"

static void do_wakeup_things P_((int sr, int newslice));

extern short proc_clock;

/* global process variables */
PROC *proclist;			/* list of all active processes */
PROC *curproc;			/* current process		*/
PROC *rootproc;			/* pid 0 -- MiNT itself		*/
PROC *sys_q[NUM_QUEUES];

short time_slice = 2;		/* default; actual value comes from mint.cnf */

#if 0
#define TIME_SLICE	2	/* number of 20ms ticks before process is
				   pre-empted */
#else
#define TIME_SLICE time_slice
#endif

/* macro for calculating number of missed time slices, based on a
 * process' priority
 */
#define SLICES(pri)	(((pri) >= 0) ? 0 : -(pri))

extern FILESYS bios_filesys;

/*
 * get a new process struct
 */

PROC *
new_proc()
{
	PROC *p;
	void *pt;

	pt = kmalloc(page_table_size + 16);
	if (!pt) return 0;

	p = (PROC *)kmalloc(SIZEOF(PROC));
	if (!p) {
		kfree(pt);
		return 0;
	}
/* page tables must be on 16 byte boundaries, so we
 * round off by 16 for that; however, we will want to
 * kfree that memory at some point, so we squirrel
 * away the original address for later use
 */
	p->page_table = ROUND16(pt);
	p->pt_mem = pt;
	return p;
}

/*
 * dispose of an old proc
 */

void
dispose_proc(p)
	PROC *p;
{
TRACELOW(("dispose_proc"));
	kfree(p->pt_mem);
	kfree(p);
}

/*
 * create a new process that is (practically) a duplicate of the
 * current one
 */

PROC *
fork_proc()
{
	PROC *p;
	int i;
	FILEPTR *f;
	long_desc *pthold;
	void *ptmemhold;

	if ((p = new_proc()) == 0) {
nomem:
		DEBUG(("fork_proc: insufficient memory"));
		mint_errno = ENSMEM; return 0;
	}

/* child shares most things with parent, but hold on to page table ptr */
	pthold = p->page_table;
	ptmemhold = p->pt_mem;
	*p = *curproc;
	p->page_table = pthold;
	p->pt_mem = ptmemhold;

/* these things are not inherited */
	p->ppid = curproc->pid;
	p->pid = newpid();
	p->sigpending = 0;
	p->nsigs = 0;
	p->sysstack = (long)(p->stack + STKSIZE - 12);
	p->ctxt[CURRENT].ssp = p->sysstack;
	p->ctxt[SYSCALL].ssp = (long)(p->stack + ISTKSIZE);
	p->alarmtim = 0;
	p->curpri = p->pri;
	p->slices = SLICES(p->pri);
	p->starttime = timestamp;
	p->startdate = datestamp;
	p->itimer[0].interval = 0;
	p->itimer[0].reqtime = 0;
	p->itimer[0].timeout = 0;
	p->itimer[1].interval = 0;
	p->itimer[1].reqtime = 0;
	p->itimer[1].timeout = 0;
	p->itimer[2].interval = 0;
	p->itimer[2].reqtime = 0;
	p->itimer[2].timeout = 0;

	((long *)p->sysstack)[1] = FRAME_MAGIC;
	((long *)p->sysstack)[2] = 0;
	((long *)p->sysstack)[3] = 0;

	p->usrtime = p->systime = p->chldstime = p->chldutime = 0;

/* allocate space for memory regions: do it here so that we can fail
 * before we duplicate anything else. The memory regions are
 * actually copied later
 */
	p->mem = (MEMREGION **) kmalloc(p->num_reg * SIZEOF(MEMREGION *));
	if (!p->mem) {
		dispose_proc(p);
		goto nomem;
	}
	p->addr = (virtaddr *)kmalloc(p->num_reg * SIZEOF(virtaddr));
	if (!p->addr) {
		kfree(p->mem);
		dispose_proc(p);
		goto nomem;
	}

/* copy open handles */
	for (i = MIN_HANDLE; i < MAX_OPEN; i++) {
		if ((f = p->handle[i]) != 0) {
			if (f == (FILEPTR *)1 || f->flags & O_NOINHERIT)
		/* oops, we didn't really want to copy this handle */
				p->handle[i] = 0;
			else
				f->links++;
		}
	}

/* copy root and current directories */
	for (i = 0; i < NUM_DRIVES; i++) {
		dup_cookie(&p->root[i], &curproc->root[i]);
		dup_cookie(&p->curdir[i], &curproc->curdir[i]);
	}

/* jr: copy ploadinfo */
	strncpy(p->cmdlin, curproc->cmdlin, 128);
	strcpy(p->fname, curproc->fname);

/* clear directory search info */
	zero((char *)p->srchdta, NUM_SEARCH * SIZEOF(DTABUF *));
	zero((char *)p->srchdir, SIZEOF(p->srchdir));
	p->searches = 0;

/* copy memory */
	for (i = 0; i < curproc->num_reg; i++) {
		p->mem[i] = curproc->mem[i];
		if (p->mem[i] != 0)
			p->mem[i]->links++;
		p->addr[i] = curproc->addr[i];
	}

/* now that memory ownership is copied, fill in page table */
	init_page_table(p);

/* child isn't traced */
	p->ptracer = 0;
	p->ptraceflags = 0;

	p->starttime = Tgettime();
	p->startdate = Tgetdate();

	p->q_next = 0;
	p->wait_q = 0;
	p->gl_next = proclist;
	proclist = p;			/* hook into the process list */
	return p;
}

/*
 * initialize the process table
 */

void
init_proc()
{
	int i;
	FILESYS *fs;
	fcookie dir;
	long_desc *pthold;
	void *ptmemhold;

	rootproc = curproc = new_proc();
	assert(curproc);

	pthold = curproc->page_table;
	ptmemhold = curproc->pt_mem;
	zero((char *)curproc, (long)sizeof(PROC));
	curproc->page_table = pthold;
	curproc->pt_mem = ptmemhold;

	curproc->ppid = -1;		/* no parent */
	curproc->domain = DOM_TOS;	/* TOS domain */
	curproc->sysstack = (long) (curproc->stack+STKSIZE-12);
	curproc->magic = CTXT_MAGIC;
	curproc->memflags = F_PROT_S;	/* default prot mode: super-only */
	((long *)curproc->sysstack)[1] = FRAME_MAGIC;
	((long *)curproc->sysstack)[2] = 0;
	((long *)curproc->sysstack)[3] = 0;

/* NOTE: in main.c this could be changed, later */
	curproc->base = _base;

	strcpy(curproc->name, "MiNT");

/* get some memory */
	curproc->mem = (MEMREGION **)kmalloc(NUM_REGIONS*SIZEOF(MEMREGION *));
	curproc->addr = (virtaddr *)kmalloc(NUM_REGIONS*SIZEOF(virtaddr));
	assert(curproc->mem && curproc->addr);

/* make sure it's filled with zeros */
	zero((char *)curproc->addr, NUM_REGIONS * SIZEOF(virtaddr));
	zero((char *)curproc->mem, NUM_REGIONS * SIZEOF(MEMREGION *));
	curproc->num_reg = NUM_REGIONS;

/* get root and current directories for all drives */
	for (i = 0; i < NUM_DRIVES; i++) {
		if ((fs = drives[i]) != 0 && (*fs->root)(i, &dir) == E_OK) {
				dup_cookie(&curproc->curdir[i], &dir);
				curproc->root[i] = dir;
		} else {
			curproc->root[i].fs = curproc->curdir[i].fs = 0;
			curproc->root[i].dev = curproc->curdir[i].dev = i;
		}
	}

	init_page_table(curproc);

/* Set the correct drive. The current directory we
 * set later, after all file systems have been loaded.
 */

	curproc->curdrv = Dgetdrv();
	proclist = curproc;

	curproc->umask = 0;

/*
 * some more protection against job control; unless these signals are
 * re-activated by a shell that knows about job control, they'll have
 * no effect
 */
	curproc->sighandle[SIGTTIN] = curproc->sighandle[SIGTTOU] =
		curproc->sighandle[SIGTSTP] = SIG_IGN;

/* set up some more per-process variables */
	curproc->starttime = Tgettime();
	curproc->startdate = Tgetdate();
	if (has_bconmap)
/* init_xbios not happened yet */
		curproc->bconmap = (int) Bconmap(-1);
	else
		curproc->bconmap = 1;

	curproc->logbase = (void *)Logbase();
	curproc->criticerr = *((long ARGS_ON_STACK (**) P_((long)))0x404L);
}

/*
 * reset all process priorities to their base level
 * called once per second, so that cpu hogs can get _some_ time
 * slices :-).
 */

void
reset_priorities()
{
	PROC *p;

	for (p = proclist; p; p = p->gl_next) {
		if (p->slices >= 0) {
			p->curpri = p->pri;
			p->slices = SLICES(p->curpri);
		}
	}
}

/*
 * more priority code stuff:
 * run_next(p, slices): schedule process "p" to run next, with "slices"
 *       initial time slices; "p" does not actually start running until
 *       the next context switch
 * fresh_slices(slices): give the current process "slices" more slices in
 *       which to run
 */

void
run_next(p, slices)
	PROC *p;
	int slices;
{
	short sr = spl7();
	p->slices = -slices;
	p->curpri = MAX_NICE;
	p->wait_q = READY_Q;
	p->q_next = sys_q[READY_Q];
	sys_q[READY_Q] = p;
	spl(sr);
}

void
fresh_slices(slices)
	int slices;
{
	reset_priorities();
	curproc->slices = 0;
	curproc->curpri = MAX_NICE+1;
	proc_clock = TIME_SLICE+slices;
}

/*
 * add a process to a wait (or ready) queue.
 *
 * processes go onto a queue in first in-first out order
 */

void
add_q(que, proc)
	int que;
	PROC *proc;
{
	PROC *q, **lastq;

/* "proc" should not already be on a list */
	assert(proc->wait_q == 0);
	assert(proc->q_next == 0);

	lastq = &sys_q[que];
	q = *lastq;
	while(q) {
		lastq = &q->q_next;
		q = *lastq;
	}
	*lastq = proc;
	proc->wait_q = que;
	if (que != READY_Q && proc->slices >= 0) {
		proc->curpri = proc->pri;	/* reward the process */
		proc->slices = SLICES(proc->curpri);
	}
}

/*
 * remove a process from a queue
 */

void
rm_q(que, proc)
	int que;
	PROC *proc;
{
	PROC *q;
	PROC *old = 0;

	assert(proc->wait_q == que);

	q = sys_q[que];
	while (q && q != proc) {
		old = q;
		q = q->q_next;
	}
	if (q == 0)
		FATAL("rm_q: unable to remove process from queue");

	if (old)
		old->q_next = proc->q_next;
	else
		sys_q[que] = proc->q_next;

	proc->wait_q = 0;
	proc->q_next = 0;
}

/*
 * preempt(): called by the vbl routine and/or the trap handlers when
 * they detect that a process has exceeded its time slice and hasn't
 * yielded gracefully. For now, it just does sleep(READY_Q); later,
 * we might want to keep track of statistics or something.
 */

void ARGS_ON_STACK
preempt()
{
	extern short bconbsiz;	/* in bios.c */

	if (bconbsiz)
		(void)bflush();
	else {
		/* punish the pre-empted process */
		if (curproc->curpri >= MIN_NICE)
			curproc->curpri -= 1;
	}
	sleep(READY_Q, curproc->wait_cond);
}

/*
 * sleep(que, cond): put the current process on the given queue, then switch
 * contexts. Before a new process runs, give it a fresh time slice. "cond"
 * is the condition for which the process is waiting, and is placed in
 * curproc->wait_cond
 */

INLINE static void
do_wakeup_things(sr, newslice)
int sr;
int newslice;
{
/*
 * check for stack underflow, just in case
 */
	auto int foo;
	PROC *p;

	p = curproc;

	if ((sr & 0x700) < 0x500) {
/* skip all this if int level is too high */
		if ( p->pid != 0 &&
		     ((long)&foo) < (long)p->stack + ISTKSIZE + 512 ) {
			ALERT("stack underflow");
			handle_sig(SIGBUS);
		}

/* see if process' time limit has been exceeded */

		if (p->maxcpu) {
			if (p->maxcpu <= p->systime + p->usrtime) {
				DEBUG(("cpu limit exceeded"));
				raise(SIGXCPU);
			}
		}

/*
 * check for alarms and similar time out stuff (see timeout.c)
 */

		checkalarms();
		if (p->sigpending)
			check_sigs();		/* check for signals */
	}

	if (newslice) {
		if (p->slices >= 0) {
			proc_clock = TIME_SLICE;	/* get a fresh time slice */
		} else {
			proc_clock = TIME_SLICE-p->slices; /* slices set by run_next */
			p->curpri = p->pri;
		}
		p->slices = SLICES(p->curpri);
	}
	p->slices = SLICES(p->curpri);
}

static long sleepcond, iwakecond;

/*
 * sleep: returns 1 if no signals have happened since our last sleep, 0
 * if some have
 */

int ARGS_ON_STACK 
sleep(_que, cond)
	int _que;
	long cond;
{
	PROC *p;
	short sr, que = _que & 0xff;
	ulong onsigs = curproc->nsigs;
	extern short kintr;	/* in bios.c */
	int newslice = 1;
#ifndef MULTITOS
#ifdef FASTTEXT
	extern int hardscroll;	/* in fasttext.c */
#endif
#endif

/* save condition, checkbttys may just wake() it right away...
 * note this assumes the condition will never be waked from interrupts
 * or other than thru wake() before we really went to sleep, otherwise
 * use the 0x100 bit like select
 */
	sleepcond = cond;

/*
 * if there have been keyboard interrupts since our last sleep, check for
 * special keys like CTRL-ALT-Fx
 */

	sr = spl7();
	if ((sr & 0x700) < 0x500) {
/* can't call checkkeys if sleep was called with interrupts off  -nox */
		spl(sr);
		(void)checkbttys();
		if (kintr) {
			(void)checkkeys();
			kintr = 0;
		}
		sr = spl7();
		if ((curproc->sigpending & ~(curproc->sigmask)) &&
		    curproc->pid && que != ZOMBIE_Q && que != TSR_Q) {
			spl(sr);
			check_sigs();
			sr = spl7();
			sleepcond = 0;	/* possibly handled a signal, return */
		}
	}

/*
 * kay: If _que & 0x100 != 0 then take curproc->wait_cond != cond as an
 * indicatation that the wakeup has already happend before we actually
 * go to sleep and return immediatly.
 */

	if ((que == READY_Q && !sys_q[READY_Q]) ||
	    ((sleepcond != cond ||
	      (iwakecond == cond && cond) ||
	      (_que & 0x100 && curproc->wait_cond != cond)) &&
	     (!sys_q[READY_Q] || (newslice = 0, proc_clock)))) {
/* we're just going to wake up again right away! */
		iwakecond = 0;
		spl(sr);
		do_wakeup_things(sr, newslice);
		return (onsigs != curproc->nsigs);
	}

/*
 * unless our time slice has expired (proc_clock == 0) and other
 * processes are ready...
 */
	iwakecond = 0;
	if (!newslice)
		que = READY_Q;
	else
		curproc->wait_cond = cond;
	add_q(que, curproc);

/* alright curproc is on que now...  maybe there's an interrupt pending
 * that will wakeselect or signal someone
 */
	spl(sr);
	if (!sys_q[READY_Q]) {
/* hmm, no-one is ready to run. might be a deadlock, might not.
 * first, try waking up any napping processes; if that doesn't work,
 * run the root process, just so we have someone to charge time
 * to.
 */
		wake(SELECT_Q, (long)nap);
		sr = spl7();
		if (!sys_q[READY_Q]) {
			p = rootproc;		/* pid 0 */
			rm_q(p->wait_q, p);
			add_q(READY_Q, p);
		}
		spl(sr);
	}

/*
 * Walk through the ready list, to find what process should run next.
 * Lower priority processes don't get to run every time through this
 * loop; if "p->slices" is positive, it's the number of times that they
 * will have to miss a turn before getting to run again
 */

/*
 * Loop structure:
 *	while (we haven't picked anybody) {
 *		for (each process) {
 *			if (sleeping off a penalty) {
 *				decrement penalty counter
 *			}
 *			else {
 *				pick this one and break out of both loops
 *			}
 *		}
 *	}
 */
	p = 0;

	while (!p) {
		for (p = sys_q[READY_Q]; p; p = p->q_next) {
			if (p->slices > 0)
				p->slices--;
			else
				break;
		}
	}

	/* p is our victim */

	rm_q(READY_Q, p);

	spl(sr);

	if (save_context(&(curproc->ctxt[CURRENT]))) {
/*
 * restore per-process variables here
 */
#ifndef MULTITOS
#ifdef FASTTEXT
		if (!hardscroll)
			*((void **)0x44eL) = curproc->logbase;
#endif
#endif
		do_wakeup_things(sr, 1);
		return (onsigs != curproc->nsigs);
	}
/*
 * save per-process variables here
 */
#ifndef MULTITOS
#ifdef FASTTEXT
	if (!hardscroll)
		curproc->logbase = *((void **)0x44eL);
#endif
#endif

	curproc->ctxt[CURRENT].regs[0] = 1;
	curproc = p;
	proc_clock = TIME_SLICE;	/* fresh time */
	if ((p->ctxt[CURRENT].sr & 0x2000) == 0) {	/* user mode? */
		leave_kernel();
	}
	assert(p->magic == CTXT_MAGIC);
	change_context(&(p->ctxt[CURRENT]));
	/* not reached */
	return 0;
}

/*
 * wake(que, cond): wake up all processes on the given queue that are waiting
 * for the indicated condition
 */

INLINE static void
do_wake(que, cond)
	int que;
	long cond;
{
	PROC *p;
top:
	for(p = sys_q[que]; p;) {
		short s = spl7();
		PROC *q;

/* check p is still on the right queue, maybe an interrupt just woke it... */
		if (p->wait_q != que) {
			spl(s);
			goto top;
		}
		q = p;
		p = p->q_next;
		if (q->wait_cond == cond) {
			rm_q(que, q);
			add_q(READY_Q, q);
		}
		spl(s);
	}
}

void ARGS_ON_STACK 
wake(que, cond)
	int que;
	long cond;
{
	if (que == READY_Q) {
		ALERT("wake: why wake up ready processes??");
		return;
	}
	if (sleepcond == cond)
		sleepcond = 0;
	do_wake(que, cond);
}

/*
 * iwake(que, cond, pid): special version of wake() for IO interrupt
 * handlers and such.  the normal wake() would lose when its
 * interrupt goes off just before a process is calling sleep() on the
 * same condition (similar problem like with wakeselect...)
 *
 * use like this:
 *	static ipid = -1;
 *	static volatile sleepers = 0;	(optional, to save useless calls)
 *	...
 *	device_read(...)
 *	{
 *		ipid = curproc->pid;	(p_getpid() for device drivers...)
 *		while (++sleepers, (not ready for IO...)) {
 *			sleep(IO_Q, cond);
 *			if (--sleepers < 0)
 *				sleepers = 0;
 *		}
 *		if (--sleepers < 0)
 *			sleepers = 0;
 *		ipid = -1;
 *		...
 *	}
 *
 * and in the interrupt handler:
 *	if (sleepers > 0) {
 *		sleepers = 0;
 *		iwake(IO_Q, cond, ipid);
 *	}
 *
 * caller is responsible for not trying to wake READY_Q or other nonsense :)
 * and making sure the passed pid is always -1 when curproc is calling
 * sleep() for another than the waked que/condition.
 */

void ARGS_ON_STACK 
iwake(que, cond, pid)
	int que;
	long cond;
	short pid;
{
	if (pid >= 0) {
		short s = spl7();
		if (iwakecond == cond) {
			spl(s);
			return;
		}
		if (curproc->pid == pid && !curproc->wait_q)
			iwakecond = cond;
		spl(s);
	}
	do_wake(que, cond);
}

/*
 * wakeselect(p): wake process p from a select() system call
 * may be called by an interrupt handler or whatever
 */

void ARGS_ON_STACK 
wakeselect(param)
	long param;
{
	PROC *p = (PROC *)param;
	short s;
	extern short select_coll;	/* in dosfile.c */

	s = spl7();	/* block interrupts */
	if(p->wait_cond == (long)wakeselect ||
	   p->wait_cond == (long)&select_coll) {
		p->wait_cond = 0;
	}
	if (p->wait_q == SELECT_Q) {
		rm_q(SELECT_Q, p);
		add_q(READY_Q, p);
	}
	spl(s);
}

/*
 * dump out information about processes
 */

/*
 * kludge alert! In order to get the right pid printed by FORCE, we use
 * curproc as the loop variable.
 *
 * I have changed this function so it is more useful to a user, less to
 * somebody debugging MiNT.  I haven't had any stack problems in MiNT
 * at all, so I consider all that stack info wasted space.  -- AKP
 */

#ifdef DEBUG_INFO
static const char *qstring[] = {
	"run", "ready", "wait", "iowait", "zombie", "tsr", "stop", "select"
};

/* UNSAFE macro for qname, evaluates x 1, 2, or 3 times */
#define qname(x) ((x >= 0 && x < NUM_QUEUES) ? qstring[x] : "unkn")
#endif

#include "loadave.h"

unsigned long uptime = 0;
unsigned long avenrun[3] = {0,0,0};
short uptimetick = 200;
static int number_running;
static int one_min_ptr = 0, five_min_ptr = 0, fifteen_min_ptr = 0;
static unsigned long sum1 = 0, sum5 = 0, sum15 = 0;
static unsigned char one_min[SAMPS_PER_MIN];
static unsigned char five_min[SAMPS_PER_5MIN];
static unsigned char fifteen_min[SAMPS_PER_15MIN];

void
DUMPPROC()
{
#ifdef DEBUG_INFO
	PROC *p = curproc;

	FORCE("Uptime: %ld seconds Loads: %ld %ld %ld Processes running: %d",
		uptime,
		(avenrun[0]*100)/2048 , (avenrun[1]*100)/2048, (avenrun[2]*100/2048),
 		number_running);

	for (curproc = proclist; curproc; curproc = curproc->gl_next) {
	    FORCE("state %s PC: %lx BP: %lx",
		qname(curproc->wait_q),
		curproc->ctxt[SYSCALL].pc,
		curproc->base);
	}
	curproc = p;		/* restore the real curproc */
#endif
}

unsigned long
gen_average(sum, load_ptr, max_size)
	unsigned long *sum;
	unsigned char *load_ptr;
	int max_size;
{
	int old_load, new_load;

	old_load = (int)*load_ptr;
	new_load = number_running;
	*load_ptr = (char)new_load;

	*sum += ((long) (new_load - old_load) * LOAD_SCALE);

	return (*sum / max_size);
}

void
calc_load_average()
{
	PROC *p;

	uptime++;
	uptimetick += 200;

	if (uptime % 5) return;

	number_running = 0;
	
	for (p = proclist; p; p = p->gl_next)
		if (p != rootproc)
			if ((p->wait_q == 0) || (p->wait_q == 1))
				number_running++;

	avenrun[0] = gen_average(&sum1, &one_min[one_min_ptr++],
				 SAMPS_PER_MIN);

	if (one_min_ptr == SAMPS_PER_MIN)
		one_min_ptr = 0;

	avenrun[1] = gen_average(&sum5, &five_min[five_min_ptr++],
				 SAMPS_PER_5MIN);

	if (five_min_ptr == SAMPS_PER_5MIN)
		five_min_ptr = 0;

	avenrun[2] = gen_average(&sum15, &fifteen_min[fifteen_min_ptr++],
				 SAMPS_PER_15MIN);

	if (fifteen_min_ptr == SAMPS_PER_15MIN)
		fifteen_min_ptr = 0;

}

