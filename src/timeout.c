/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
*/

#include "mint.h"

/*
 * We initialize proc_clock to a very large value so that we don't have
 * to worry about unexpected process switches while starting up
 */

short proc_clock = 0x7fff;

/* used by filesystems for time/date stamps; updated once per second */
short timestamp, datestamp;

extern short in_kernel;	/* in main.c */

static void unnapme P_((PROC *));
static TIMEOUT	*newtimeout P_((short));
static void	disposetimeout P_((TIMEOUT *));
static void	inserttimeout P_ ((TIMEOUT *, long));

#define TIMEOUTS	20	/* # of static timeout structs */
#define TIMEOUT_USED	0x01	/* timeout struct is in use */
#define TIMEOUT_STATIC	0x02	/* this is a static timeout */

/* This gets implizitly initialized to zero, thus the flags are
 * set up correctly.
 */
static TIMEOUT timeouts[TIMEOUTS] = { { 0, }, };
TIMEOUT *tlist = NULL;
TIMEOUT *expire_list = NULL;

/* Number of ticks after that an expired timeout is considered to be old
   and disposed automatically.  */
#define TIMEOUT_EXPIRE_LIMIT 400 /* 2 secs */

static TIMEOUT *
newtimeout(fromlist)
	short fromlist;
{
	TIMEOUT *t;
	short i, sr;

	if (!fromlist) {
		t = kmalloc(SIZEOF(TIMEOUT));
		if (t) {
			t->flags = 0;
			t->arg = 0;
			return t;
		}
	}
	sr = spl7();
	for (i = 0; i < TIMEOUTS; ++i) {
		if (!(timeouts[i].flags & TIMEOUT_USED)) {
			timeouts[i].flags |= (TIMEOUT_STATIC|TIMEOUT_USED);
			spl(sr);
			timeouts[i].arg = 0;
			return &timeouts[i];
		}
	}
	spl(sr);
	return 0;
}

static void
disposetimeout(t)
	TIMEOUT *t;
{
	if (t->flags & TIMEOUT_STATIC) t->flags &= ~TIMEOUT_USED;
	else kfree(t);
}

static void
dispose_old_timeouts ()
{
  TIMEOUT *t, **prev, *old;
  long now = *(long *) 0x4ba;
  short sr = spl7 ();

  for (prev = &expire_list, t = *prev; t; prev = &t->next, t = *prev)
    {
      if (t->when < now)
	{
	  /* This and the following timeouts are too old. Throw them away. */
	  *prev = 0;
	  spl (sr);
	  while (t)
	    {
	      old = t;
	      t = t->next;
	      disposetimeout (old);
	    }
	  return;
	}
    }
  spl (sr);
}

static void
inserttimeout(t, delta)
	TIMEOUT *t;
	long delta;
{
	TIMEOUT **prev, *cur;
	short sr = spl7();

	cur = tlist;
	prev = &tlist;
	while (cur) {
		if (cur->when >= delta) {
			cur->when -= delta;
			t->next = cur;
			t->when = delta;
			*prev = t;
			spl(sr);
			return;
		}
		delta -= cur->when;
		prev = &cur->next;
		cur = cur->next;
	}
	assert(delta >= 0);
	t->when = delta;
	t->next = cur;
	*prev = t;
	spl(sr);
}
	
/*
 * addtimeout(long delta, void (*func)()): schedule a timeout for the current
 * process, to take place in "delta" milliseconds. "func" specifies a
 * function to be called at that time; the function is passed as a parameter
 * the process for which the timeout was specified (i.e. the value of
 * curproc at the time addtimeout() was called; note that this is probably
 * *not* the current process when the timeout occurs).
 *
 * NOTE: if kernel memory is low, newtimeout() will try to get a statically
 * allocated timeout struct (fallback method).
 */

TIMEOUT * ARGS_ON_STACK
addtimeout(delta, func)
	long delta;
	void (*func) P_((PROC *));
{
	TIMEOUT *t;
	TIMEOUT **prev;
	short sr;

	/* Try to reuse an already expired timeout that had the
	   same function attached */
	sr = spl7();
	prev = &expire_list;
	for (t = *prev; t != NULL; prev = &t->next, t = *prev)
	  if (t->proc == curproc && t->func == func)
	    {
	      *prev = t->next;
	      break;
	    }

	spl(sr);
	if (t == NULL)
	  t = newtimeout(0);

/* BUG: we should have some fallback mechanism for timeouts when the
   kernel memory is exhausted
 */
	assert(t);

	t->proc = curproc;
	t->func = func;
	inserttimeout(t, delta);
	return t;
}

/*
 * addroottimeout(long delta, void (*)(PROC *), short flags);
 * Same as addtimeout(), except that the timeout is attached to Pid 0 (MiNT).
 * This means the timeout won't be cancelled if the process which was
 * running at the time addroottimeout() was called exits.
 *
 * Currently only bit 0 of `flags' is used. Meaning:
 * Bit 0 set: Call from interrupt (cannot use kmalloc, use statically
 *	allocated `struct timeout' instead).
 * Bit 0 clear: Not called from interrupt, can use kmalloc.
 *
 * Thus addroottimeout() can be called from interrupts (bit 0 of flags set),
 * which makes it *extremly* useful for device drivers.
 * A serial device driver would make an addroottimeout(0, check_keys, 1)
 * if some bytes have arrived.
 * check_keys() is then called at the next context switch, can use all
 * the kernel functions and can do time cosuming jobs.
 */

TIMEOUT * ARGS_ON_STACK
addroottimeout(delta, func, flags)
	long delta;
	void (*func) P_((PROC *));
	short flags;
{
	TIMEOUT *t;
	TIMEOUT **prev;
	short sr;

	/* Try to reuse an already expired timeout that had the
	   same function attached */
	sr = spl7();
	prev = &expire_list;
	for (t = *prev; t != NULL; t = *prev)
	  {
	    if (t->proc == rootproc && t->func == func)
	      {
		*prev = t->next;
		break;
	      }
	    prev = &t->next;
	  }
	spl(sr);

	if (!t)
	  t = newtimeout(flags & 1);

	if (!t) return NULL;
	t->proc = rootproc;
	t->func = func;
	inserttimeout(t, delta);
	return t;
}

/*
 * cancelalltimeouts(): cancels all pending timeouts for the current
 * process
 */

void ARGS_ON_STACK
cancelalltimeouts()
{
	TIMEOUT *cur, **prev, *old;
	long delta;
	short sr = spl7 ();

	cur = tlist;
	prev = &tlist;
	while (cur) {
		if (cur->proc == curproc) {
			delta = cur->when;
			old = cur;
			*prev = cur = cur->next;
			if (cur) cur->when += delta;
			spl(sr);
			disposetimeout(old);
			sr = spl7();
		/* ++kay: just in case an interrupt handler installed a
		 * timeout right after `prev' and before `cur' */
			cur = *prev;
		}
		else {
			prev = &cur->next;
			cur = cur->next;
		}
	}
	prev = &expire_list;
	for (cur = *prev; cur; cur = *prev)
	  {
	    if (cur->proc == curproc)
	      {
		*prev = cur->next;
		spl (sr);
		disposetimeout (cur);
		sr = spl7 ();
	      }
	    else
	      prev = &cur->next;
	  }
	spl (sr);
}

/*
 * Cancel a specific timeout. If the timeout isn't on the list, or isn't
 * for this process, we do nothing; otherwise, we cancel the time out
 * and then free the memory it used. *NOTE*: it's very possible (indeed
 * likely) that "this" was already removed from the list and disposed of
 * by the timeout processing routines, so it's important that we check
 * for it's presence in the list and do absolutely nothing if we don't
 * find it there!
 */

void ARGS_ON_STACK
canceltimeout(this)
	TIMEOUT *this;
{
	TIMEOUT *cur, **prev;
	short sr = spl7();

	/* First look at the list of expired timeouts */
	prev = &expire_list;
	for (cur = *prev; cur; cur = *prev)
	  {
	    if (cur == this && cur->proc == curproc)
	      {
		*prev = cur->next;
		spl (sr);
		disposetimeout (this);
		return;
	      }
	    prev = &cur->next;
	  }

	prev = &tlist;
	for (cur = tlist; cur; cur = cur->next) {
		if (cur == this && cur->proc == curproc) {
			*prev = cur->next;
			if (cur->next) {
				cur->next->when += this->when;
			}
			spl (sr);
			disposetimeout(this);
			return;
		}
		prev = &cur->next;
	}
	spl(sr);
}

void ARGS_ON_STACK
cancelroottimeout(this)
	TIMEOUT *this;
{
	TIMEOUT *cur, **prev;
	short sr = spl7();

	/* First look at the list of expired timeouts */
	prev = &expire_list;
	for (cur = *prev; cur; cur = *prev)
	  {
	    if (cur == this && cur->proc == rootproc)
	      {
		*prev = cur->next;
		spl (sr);
		disposetimeout (this);
		return;
	      }
	    prev = &cur->next;
	  }
	
	prev = &tlist;
	for (cur = tlist; cur; cur = cur->next) {
		if (cur == this && (cur->proc == rootproc)) {
			*prev = cur->next;
			if (cur->next) {
				cur->next->when += this->when;
			}
			spl (sr);
			disposetimeout(this);
			return;
		}
		prev = &cur->next;
	}
	spl(sr);
}

/*
 * timeout: called every 20 ms or so by GEMDOS, this routine
 * is responsible for maintaining process times and such.
 * it should also decrement the "proc_clock" variable, but
 * should *not* take any action when it reaches 0 (the state of the
 * stack is too uncertain, and time is too critical). Instead,
 * a vbl routine checks periodically and if "proc_clock" is 0
 * suspends the current process
 */

volatile int our_clock = 1000;

/* variables for monitoring the keyboard */
extern IOREC_T	*keyrec;	/* keyboard i/o record pointer */
extern short	kintr;		/* keyboard interrupt pending (see intr.s) */

void ARGS_ON_STACK
timeout()
{
	int ms;		/* time between ticks */

	kintr = keyrec->head != keyrec->tail;

	ms = *((short *)0x442L);
	if (proc_clock > 0)
		proc_clock--;

	our_clock -= ms;
	if (tlist) {
		tlist->when -= ms;
	}
}

/*
 * sleep() calls this routine to check on alarms and other sorts
 * of time-outs on every context switch.
 */

void
checkalarms()
{
	extern long searchtime;		/* in dosdir.c */
	PROC *p;
	long delta;
	void (*evnt) P_((PROC *, long arg));
	TIMEOUT *old;
	short sr;
	long arg;

/* do the once per second things */
	while (our_clock < 0) {
		our_clock += 1000;
		timestamp = Tgettime();
		datestamp = Tgetdate();
		searchtime++;
		reset_priorities();
	}

	sr = spl7();
/* see if there are outstanding timeout requests to do */
	while (tlist && ((delta = tlist->when) <= 0)) {
		p = tlist->proc;
/* hack: pass an extra long as arg, those intrested in it will need
 * a cast and have to place it in t->arg themselves but that way
 * everything else still works without change	-nox */
		arg = tlist->arg;
		evnt = (void (*)P_((PROC *, long)))tlist->func;
		old = tlist;
		tlist = tlist->next;
/* if delta < 0, it's possible that the time has come for the next timeout
 * to occur.
 * ++kay: moved this before the timeout fuction is called, in case the
 * timeout function installes a new timeout. */
		if (tlist)
			tlist->when += delta;
		old->next = expire_list;
		old->when = *(long *) 0x4ba + TIMEOUT_EXPIRE_LIMIT;
		expire_list = old;
		spl(sr);
/* ++kay: debug output at spl7 hangs the system, so moved it here */
		TRACE(("doing timeout code for pid %d", p->pid));

	/* call the timeout function */
		(*evnt)(p, arg);
		sr = spl7();
	}
	spl(sr);
	/* Now look at the expired timeouts if some are getting old */
	dispose_old_timeouts ();
}

/*
 * nap(n): nap for n milliseconds. Used in loops where we're waiting for
 * an event. If we expect the event *very* soon, we should use yield
 * instead.
 * NOTE: we may not sleep for exactly n milliseconds; signals can wake
 * us earlier, and the vagaries of process scheduling may cause us to
 * oversleep...
 */

static void
unnapme(p)
	PROC *p;
{
	if (p->wait_q == SELECT_Q && p->wait_cond == (long)nap) {
		short sr = spl7();
		rm_q(SELECT_Q, p);
		add_q(READY_Q, p);
		spl(sr);
		p->wait_cond = 0;
	}
}

void ARGS_ON_STACK 
nap(n)
	unsigned n;
{
	TIMEOUT *t;

	t = addtimeout((long)n, unnapme);
	sleep(SELECT_Q, (long)nap);
	canceltimeout(t);
}
