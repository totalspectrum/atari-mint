/*
Copyright 1990,1991,1992,1994 Eric R. Smith.
All rights reserved.
*/

/* dossig.c:: dos signal handling routines */

#include "mint.h"

void ARGS_ON_STACK sig_user P_((int vec));

/*
 * send a signal to another process. If pid > 0, send the signal just to
 * that process. If pid < 0, send the signal to all processes whose process
 * group is -pid. If pid == 0, send the signal to all processes with the
 * same process group id.
 *
 * note: post_sig just posts the signal to the process.
 */

long ARGS_ON_STACK
p_kill(pid, sig)
	int pid, sig;
{
	PROC *p;
	long r;

	TRACE(("Pkill(%d, %d)", pid, sig));
	if (sig < 0 || sig >= NSIG) {
		DEBUG(("Pkill: signal out of range"));
		return ERANGE;
	}

	if (pid < 0)
		r = killgroup(-pid, sig, 0);
	else if (pid == 0)
		r = killgroup(curproc->pgrp, sig, 0);
	else {
		p = pid2proc(pid);
		if (p == 0 || p->wait_q == ZOMBIE_Q || p->wait_q == TSR_Q) {
			DEBUG(("Pkill: pid %d not found", pid));
			return EFILNF;
		}
		if (curproc->euid && curproc->ruid != p->ruid) {
			DEBUG(("Pkill: wrong user"));
			return EACCDN;
		}

/* if the user sends signal 0, don't deliver it -- for users, signal
 * 0 is a null signal used to test the existence of a process
 */
		if (sig != 0)
			post_sig(p, sig);
		r = 0;
	}

	if (r == 0) {
		check_sigs();
		TRACE(("Pkill: returning OK"));
	}
	return r;
}

/*
 * set a user-specified signal handler, POSIX.1 style
 * "oact", if non-null, gets the old signal handling
 * behaviour; "act", if non-null, specifies new
 * behaviour
 */

long ARGS_ON_STACK
p_sigaction(sig, act, oact)
	int sig;
	const struct sigaction *act;
	struct sigaction *oact;
{
	TRACE(("Psigaction(%d)", sig));
	if (sig < 1 || sig >= NSIG)
		return ERANGE;
	if (act && (sig == SIGKILL || sig == SIGSTOP))
		return EACCDN;
	if (oact) {
		oact->sa_handler = curproc->sighandle[sig];
		oact->sa_mask = curproc->sigextra[sig];
		oact->sa_flags = curproc->sigflags[sig] & SAUSER;
	}
	if (act) {
		ushort flags;

		curproc->sighandle[sig] = act->sa_handler;
		curproc->sigextra[sig] = act->sa_mask & ~UNMASKABLE;

/* only the flags in SAUSER can be changed by the user */
		flags = curproc->sigflags[sig] & ~SAUSER;
		flags |= act->sa_flags & SAUSER;
		curproc->sigflags[sig] = flags;
 
/* various special things that should happen */
		if (act->sa_handler == SIG_IGN) {
			/* discard pending signals */
			curproc->sigpending &= ~(1L<<sig);
		}

/* I dunno if this is right, but bash seems to expect it */
 		curproc->sigmask &= ~(1L<<sig);
	}
	return 0;
}

/*
 * set a user-specified signal handler
 */

long ARGS_ON_STACK
p_signal(sig, handler)
	int sig;
	long handler;
{
	long oldhandle;

	TRACE(("Psignal(%d, %lx)", sig, handler));
	if (sig < 1 || sig >= NSIG)
		return ERANGE;
	if (sig == SIGKILL || sig == SIGSTOP)
		return EACCDN;
	oldhandle = curproc->sighandle[sig];
	curproc->sighandle[sig] = handler;
	curproc->sigextra[sig] = 0;
	curproc->sigflags[sig] = 0;

/* various special things that should happen */
	if (handler == SIG_IGN) {
		/* discard pending signals */
		curproc->sigpending &= ~(1L<<sig);
	}

/* I dunno if this is right, but bash seems to expect it */
	curproc->sigmask &= ~(1L<<sig);

	return oldhandle;
}

/*
 * block some signals. Returns the old signal mask.
 */

long ARGS_ON_STACK
p_sigblock(mask)
	ulong mask;
{
	ulong oldmask;

	TRACE(("Psigblock(%lx)",mask));
/* some signals (e.g. SIGKILL) can't be masked */
	mask &= ~(UNMASKABLE);
	oldmask = curproc->sigmask;
	curproc->sigmask |= mask;
	return oldmask;
}

/*
 * set the signals that we're blocking. Some signals (e.g. SIGKILL)
 * can't be masked.
 * Returns the old mask.
 */

long ARGS_ON_STACK
p_sigsetmask(mask)
	ulong mask;
{
	ulong oldmask;

	TRACE(("Psigsetmask(%lx)",mask));
	oldmask = curproc->sigmask;
	curproc->sigmask = mask & ~(UNMASKABLE);
	check_sigs();	/* maybe we unmasked something */
	return oldmask;
}

/*
 * p_sigpending: return which signals are pending delivery
 */

long ARGS_ON_STACK
p_sigpending()
{
	TRACE(("Psigpending()"));
	check_sigs();	/* clear out any that are going to be delivered soon */

/* note that signal #0 is used internally, so we don't tell the process
 * about it
 */
	return curproc->sigpending & ~1L;
}

/*
 * p_sigpause: atomically set the signals that we're blocking, then pause.
 * Some signals (e.g. SIGKILL) can't be masked.
 */

long ARGS_ON_STACK
p_sigpause(mask)
	ulong mask;
{
	ulong oldmask;

	TRACE(("Psigpause(%lx)", mask));
	oldmask = curproc->sigmask;
	curproc->sigmask = mask & ~(UNMASKABLE);
	if (curproc->sigpending & ~(curproc->sigmask))
		check_sigs();	/* a signal is immediately pending */
	else
		sleep(IO_Q, -1L);
	curproc->sigmask = oldmask;
	check_sigs();	/* maybe we unmasked something */
	TRACE(("Psigpause: returning OK"));
	return 0;
}

/*
 * p_sigintr: Set an exception vector to send us the specified signal.
 */

typedef struct usig {
	int vec;		/* exception vector number */
	int sig;		/* signal to send */
	PROC *proc;		/* process to get signal */
	long oldv;		/* old exception vector value */
	struct usig *next;	/* next entry ... */
} usig;

static usig *usiglst;
extern long mcpu;

long ARGS_ON_STACK
p_sigintr(vec, sig)
	int vec;
	int sig;
{
	extern void new_intr();	/* in intr.spp */
	long vec2;
	usig *new;

	if (!sig)		/* ignore signal 0 */
		return 0;

	vec2 = (long) new_intr;

#ifndef ONLY030
	if (mcpu == 0)			
		/* put vector number in high byte of vector address */
		vec2 |= ((long) vec) << 24;
#endif
	new = kmalloc(sizeof(usig));
	if (!new)			/* hope this never happens...! */
		return ENSMEM;
	new->vec = vec;
	new->sig = sig;
	new->proc = curproc;
	new->next = usiglst;		/* simple unsorted list... */
	usiglst = new;

	new->oldv = setexc(vec, vec2);
	return new->oldv;
}

/*
 * Find the process that requested this interrupt, and send it a signal.
 * Called at interrupt time by new_intr() from intr.spp, with interrupt
 * vector number on the stack.
 */

void ARGS_ON_STACK
sig_user(vec)
	int vec;
{
	usig *ptr;

	for (ptr = usiglst; ptr; ptr=ptr->next)
		if (vec == ptr->vec) {
			if (ptr->proc->wait_q != ZOMBIE_Q &&
			    ptr->proc->wait_q != TSR_Q) {
				post_sig(ptr->proc, ptr->sig);
			}
#if 0	/* Search entire list, to allow multiple processes to respond to
	   the same interrupt. (Why/when would you want that?) */
			break;
#endif
		}
	/*
	 * Clear in-service bit for ST MFP interrupts
	 */
	if (vec >= 64 && vec < 80) {
		char *mfp, c;

		if (vec < 72)		/* Register B */
			mfp = (char *)0xfffffa11L;
		else			/* Register A */
			mfp = (char *)0xfffffa0fL;
		c = 1 << (vec & 7);

		*mfp = ~c;
	}
}

/*
 * cancelsigintrs: remove any interrupts requested by this process, called
 * at process termination.
 */
void
cancelsigintrs()
{
	usig *ptr, **old, *nxt;
	short s = spl7();

	for (old=&usiglst, ptr=usiglst; ptr; ) {
		nxt = ptr->next;
		if (ptr->proc == curproc) {
			setexc(ptr->vec, ptr->oldv);
			*old = nxt;
			kfree(ptr);
		} else {
			old = &(ptr->next);
		}
		ptr = nxt;
	}
	spl(s);
}
