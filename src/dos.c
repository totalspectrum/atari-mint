/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
*/

/* miscellaneous DOS functions, and the DOS initialization function */

#include "mint.h"

#define DOS_MAX 0x160

Func dos_tab[DOS_MAX];
short dos_max = DOS_MAX;

static void alarmme P_((PROC *));
static void itimer_real_me P_((PROC *p));
static void itimer_virtual_me P_((PROC *p));
static void itimer_prof_me P_((PROC *p));

long ARGS_ON_STACK 
s_version()
{
	return Sversion();
}

/*
 * Super(new_ssp): change to supervisor mode.
 */

long ARGS_ON_STACK
s_uper(new_ssp)
	long new_ssp;
{
	int in_super;
	long r;

	TRACE(("Super"));
	in_super = curproc->ctxt[SYSCALL].sr & 0x2000;

	if (new_ssp == 1) {
		r = in_super ? -1L : 0;
	}
	else {
		curproc->ctxt[SYSCALL].sr ^= 0x2000;
		r = curproc->ctxt[SYSCALL].ssp;
		if (in_super) {
			if (new_ssp == 0) {
				DEBUG(("bad Super call"));
				raise(SIGSYS);
			}
			else {
				curproc->ctxt[SYSCALL].usp = 
					curproc->ctxt[SYSCALL].ssp;
				curproc->ctxt[SYSCALL].ssp = new_ssp;
			}
		}
		else {
			curproc->ctxt[SYSCALL].ssp = 
			    new_ssp ? new_ssp : curproc->ctxt[SYSCALL].usp;
		}
	}
	return r;
}

/*
 * get/set time and date functions
 */
long ARGS_ON_STACK t_getdate() { return datestamp; }
long ARGS_ON_STACK t_gettime() { return timestamp; }

long ARGS_ON_STACK t_setdate(date)
	int date;
{
	long r;

/* Only the superuser may set date or time */
	if (curproc->euid != 0)
		return EACCDN;
	r = Tsetdate(date);
	datestamp = Tgetdate();
	return r;
}

long ARGS_ON_STACK t_settime(time)
	int time;
{
	long r;

	if (curproc->euid != 0)
		return EACCDN;
	r = Tsettime(time);
	timestamp = Tgettime();
	return r;
}

/*
 * GEMDOS extension: Syield(): give up the processor if any other
 * processes are waiting. Always returns 0.
 */

long ARGS_ON_STACK
s_yield()
{
/* reward the nice process */
	curproc->curpri = curproc->pri;
	sleep(READY_Q, curproc->wait_cond);
	return 0;
}

/*
 * GEMDOS extension:
 * Prenice(pid, delta) sets the process priority level for process pid.
 * A "nice" value < 0 increases priority, one > 0 decreases it.
 * Always returns the new priority (so Prenice(pid, 0) queries the current
 * priority).
 *
 * NOTE: for backward compatibility, Pnice(delta) is provided and is equivalent
 * to Prenice(Pgetpid(), delta)
 */

long ARGS_ON_STACK
p_renice(pid, delta)
	int pid, delta;
{
	PROC *p;

	if (pid <= 0 || 0 == (p = pid2proc(pid))) {
		return EFILNF;
	}

	if (curproc->euid && curproc->euid != p->ruid
	    && curproc->ruid != p->ruid) {
		DEBUG(("Prenice: process ownership error"));
		return EACCDN;
	}
	p->pri -= delta;
	if (p->pri < MIN_NICE) p->pri = MIN_NICE;
	if (p->pri > MAX_NICE) p->pri = MAX_NICE;
	p->curpri = p->pri;
	return ((long)p->pri) & 0x0ffff;
}

long ARGS_ON_STACK
p_nice(delta)
	int delta;
{
	return p_renice(curproc->pid,delta);
}

/*
 * GEMDOS extensions: routines for getting/setting process i.d.'s and
 * user i.d.'s
 */

long ARGS_ON_STACK p_getpid() { return curproc->pid; }

long ARGS_ON_STACK p_getppid() { return curproc->ppid; }

long ARGS_ON_STACK p_getpgrp() { return curproc->pgrp; }

/* note: Psetpgrp(0, ...) is equivalent to Psetpgrp(Pgetpid(), ...) */
/* also note: Psetpgrp(x, 0) is equivalent to Psetpgrp(x, x) */

long ARGS_ON_STACK p_setpgrp(pid, newgrp)
	int pid, newgrp;
{
	PROC *p;

	if (pid == 0)
		p = curproc;
	else if (0 == (p = pid2proc(pid)))
		return EFILNF;
	if ( (curproc->euid) && (p->ruid != curproc->ruid)
	      && (p->ppid != curproc->pid) )
		return EACCDN;

	if (newgrp < 0)
		return p->pgrp;

	if (newgrp == 0)
		newgrp = p->pid;

	return (p->pgrp = newgrp);
}

long ARGS_ON_STACK p_getuid() { return curproc->ruid; }
long ARGS_ON_STACK p_getgid() { return curproc->rgid; }
long ARGS_ON_STACK p_geteuid() { return curproc->euid; }
long ARGS_ON_STACK p_getegid() { return curproc->egid; }

long ARGS_ON_STACK
p_setuid(id)
	int id;
{
	if (curproc->euid == 0 || curproc->euid == id || curproc->ruid == id) {
		curproc->ruid = curproc->euid = id;
		return id;
	}
	return EACCDN;
}

long ARGS_ON_STACK
p_setgid(id)
	int id;
{
	if (curproc->euid == 0 || curproc->rgid == id) {
		curproc->egid = curproc->rgid = id;
		return id;
	}
	return EACCDN;
}

/* uk: set effective uid/gid but leave the real uid/gid unchanged. */
long ARGS_ON_STACK
p_seteuid(id)
	int id;
{
	if (curproc->euid == 0 || curproc->ruid	== id) {
		curproc->euid = id;
		return id;
	}
	return EACCDN;
}
	
long ARGS_ON_STACK
p_setegid(id)
	int id;
{
	if (curproc->euid == 0 || curproc->egid == 0 || curproc->rgid == id) {
		curproc->egid = id;
		return id;
	}
	return EACCDN;
}

/*  tesche: audit user id functions, these id's never change once set to != 0
 * and can therefore be used to determine who the initially logged in user was.
 */
long ARGS_ON_STACK
p_getauid()
{
	return curproc->auid;
}

long ARGS_ON_STACK
p_setauid(id)
	int id;
{
	if (curproc->auid)
		return EACCDN;	/* this may only be changed once */

	return (curproc->auid = id);
}

/*  tesche: get/set supplemantary group id's.
 */
long ARGS_ON_STACK
p_getgroups(gidsetlen, gidset)
	int gidsetlen;
	int gidset[];
{
	int i;

	if (gidsetlen == 0)
		return curproc->ngroups;

	if (gidsetlen < curproc->ngroups)
		return ERANGE;

	for (i=0; i<curproc->ngroups; i++)
		gidset[i] = curproc->ngroup[i];

	return curproc->ngroups;
}

long ARGS_ON_STACK
p_setgroups(ngroups, gidset)
	int ngroups;
	int gidset[];
{
	int i;

	if (curproc->euid)
		return EACCDN;	/* only superuser may change this */

	if ((ngroups < 0) || (ngroups > NGROUPS_MAX))
		return ERANGE;

	curproc->ngroups = ngroups;
	for (i=0; i<ngroups; i++)
		curproc->ngroup[i] = gidset[i];

	return ngroups;
}

/*
 * a way to get/set process-specific user information. the user information
 * longword is set to "arg", unless arg is -1. In any case, the old
 * value of the longword is returned.
 */

long ARGS_ON_STACK
p_usrval(arg)
	long arg;
{
	long r;

	TRACE(("Pusrval"));
	r = curproc->usrdata;
	if (arg != -1L)
		curproc->usrdata = arg;
	return r;
}

/*
 * set the file creation mask to "mode". Returns the old value of the
 * mask.
 */
long ARGS_ON_STACK p_umask(mode)
	unsigned mode;
{
	long oldmask = curproc->umask;

	curproc->umask = mode & (~S_IFMT);
	return oldmask;
}

/*
 * get/set the domain of a process. domain 0 is the default (TOS) domain.
 * domain 1 is the MiNT domain. for now, domain affects read/write system
 * calls and filename translation.
 */

long ARGS_ON_STACK
p_domain(arg)
	int arg;
{
	long r;
	TRACE(("Pdomain(%d)", arg));

	r = curproc->domain;
	if (arg >= 0)
		curproc->domain = arg;
	return r;
}

/*
 * get process resource usage. 8 longwords are returned, as follows:
 *     r[0] == system time used by process
 *     r[1] == user time used by process
 *     r[2] == system time used by process' children
 *     r[3] == user time used by process' children
 *     r[4] == memory used by process
 *     r[5] - r[7]: reserved for future use
 */

long ARGS_ON_STACK
p_rusage(r)
	long *r;
{
	r[0] = curproc->systime;
	r[1] = curproc->usrtime;
	r[2] = curproc->chldstime;
	r[3] = curproc->chldutime;
	r[4] = memused(curproc);
	return 0;
}

/*
 * get/set resource limits i to value v. The old limit is always returned;
 * if v == -1, the limit is unchanged, otherwise it is set to v. Possible
 * values for i are:
 *    1:  max. cpu time	(milliseconds)
 *    2:  max. core memory allowed
 *    3:  max. amount of malloc'd memory allowed
 */
long ARGS_ON_STACK
p_setlimit(i, v)
	int i;
	long v;
{
	long oldlimit;

	switch(i) {
	case 1:
		oldlimit = curproc->maxcpu;
		if (v >= 0) curproc->maxcpu = v;
		break;
	case 2:
		oldlimit = curproc->maxcore;
		if (v >= 0) {
			curproc->maxcore = v;
			recalc_maxmem(curproc);
		}
		break;
	case 3:
		oldlimit = curproc->maxdata;
		if (v >= 0) {
			curproc->maxdata = v;
			recalc_maxmem(curproc);
		}
		break;
	default:
		DEBUG(("Psetlimit: invalid mode %d", i));
		return EINVFN;
	}
	TRACE(("p_setlimit(%d, %ld): oldlimit = %ld", i, v, oldlimit));
	return oldlimit;
}

/*
 * pause: just sleeps on IO_Q, with wait_cond == -1. only a signal will
 * wake us up
 */

long ARGS_ON_STACK
p_pause()
{
	TRACE(("Pause"));
	sleep(IO_Q, -1L);
	return 0;
}

/*
 * helper function for t_alarm: this will be called when the timer goes
 * off, and raises SIGALRM
 */

static void
alarmme(p)
	PROC *p;
{
	p->alarmtim = 0;
	post_sig(p, SIGALRM);
}

/*
 * t_alarm(x): set the alarm clock to go off in "x" seconds. returns the
 * old value of the alarm clock
 */

long ARGS_ON_STACK
t_alarm(x)
	long x;
{
	long oldalarm;
	oldalarm = t_malarm(x*1000);
	oldalarm = (oldalarm+999)/1000;		/* convert to seconds */
	return oldalarm;
}

/*
 * t_malarm(x): set the alarm clock to go off in "x" milliseconds. returns
 * the old value ofthe alarm clock
 */

long ARGS_ON_STACK
t_malarm(x)
	long x;
{
	long oldalarm;
	TIMEOUT *t;

/* see how many milliseconds there were to the alarm timeout */
	oldalarm = 0;

	if (curproc->alarmtim) {
		for (t = tlist; t; t = t->next) {
			oldalarm += t->when;
			if (t == curproc->alarmtim)
				goto foundalarm;
		}
		DEBUG(("Talarm: old alarm not found!"));
		oldalarm = 0;
		curproc->alarmtim = 0;
foundalarm:
		;
	}

/* we were just querying the alarm */
	if (x < 0)
		return oldalarm;

/* cancel old alarm */
	if (curproc->alarmtim)
		canceltimeout(curproc->alarmtim);

/* add a new alarm, to occur in x milliseconds */
	if (x)
		curproc->alarmtim = addtimeout(x, alarmme);
	else
		curproc->alarmtim = 0;

	return oldalarm;
}

#define ITIMER_REAL 0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF 2

/*
 * helper function for t_setitimer: this will be called when the ITIMER_REAL
 * timer goes off
 */

static void
itimer_real_me(p)
	PROC *p;
{
	PROC *real_curproc;

	real_curproc = curproc;
	curproc = p;
	if (p->itimer[ITIMER_REAL].interval)
	  p->itimer[ITIMER_REAL].timeout =
		addtimeout(p->itimer[ITIMER_REAL].interval, itimer_real_me);
	else
	  p->itimer[ITIMER_REAL].timeout = 0;

	curproc = real_curproc;
	post_sig(p, SIGALRM);
}

/*
 * helper function for t_setitimer: this will be called when the ITIMER_VIRTUAL
 * timer goes off
 */

static void
itimer_virtual_me(p)
	PROC *p;
{
	PROC *real_curproc;
	long timeleft;

	real_curproc = curproc;
	curproc = p;
	timeleft = p->itimer[ITIMER_VIRTUAL].reqtime
			- (p->systime - p->itimer[ITIMER_VIRTUAL].startsystime);
	if (timeleft > 0) {
		p->itimer[ITIMER_VIRTUAL].timeout =
			addtimeout(timeleft, itimer_virtual_me);
	} else {
		timeleft = p->itimer[ITIMER_VIRTUAL].interval;
		if (timeleft == 0) {
			p->itimer[ITIMER_VIRTUAL].timeout = 0;
		} else {
			p->itimer[ITIMER_VIRTUAL].reqtime = timeleft;
			p->itimer[ITIMER_VIRTUAL].startsystime = p->systime;
			p->itimer[ITIMER_VIRTUAL].startusrtime = p->usrtime;
			p->itimer[ITIMER_VIRTUAL].timeout =
				addtimeout(timeleft, itimer_virtual_me);
		}
		post_sig(p, SIGVTALRM);
	}
	curproc = real_curproc;
}

/*
 * helper function for t_setitimer: this will be called when the ITIMER_PROF
 * timer goes off
 */

static void
itimer_prof_me(p)
	PROC *p;
{
	PROC *real_curproc;
	long timeleft;

	real_curproc = curproc;
	curproc = p;
	timeleft = p->itimer[ITIMER_PROF].reqtime
			- (p->usrtime - p->itimer[ITIMER_PROF].startusrtime);
	if (timeleft > 0) {
		p->itimer[ITIMER_PROF].timeout =
			addtimeout(timeleft, itimer_prof_me);
	} else {
		timeleft = p->itimer[ITIMER_PROF].interval;
		if (timeleft == 0) {
			p->itimer[ITIMER_PROF].timeout = 0;
		} else {
			p->itimer[ITIMER_PROF].reqtime = timeleft;
			p->itimer[ITIMER_PROF].startsystime = p->systime;
			p->itimer[ITIMER_PROF].startusrtime = p->usrtime;
			p->itimer[ITIMER_PROF].timeout =
				addtimeout(timeleft, itimer_prof_me);
		}
		post_sig(p, SIGPROF);
	}
	curproc = real_curproc;
}

/*
 * t_setitimer(which, interval, value, ointerval, ovalue):
 * schedule an interval timer
 * which is ITIMER_REAL (0) for SIGALRM, ITIMER_VIRTUAL (1) for SIGVTALRM,
 * or ITIMER_PROF (2) for SIGPROF.
 * the rest of the parameters are pointers to millisecond values.
 * interval is the value to which the timer will be reset
 * value is the current timer value
 * ointerval and ovalue are the previous values
 */

long ARGS_ON_STACK
t_setitimer(which, interval, value, ointerval, ovalue)
	int which;
	long *interval;
	long *value;
	long *ointerval;
	long *ovalue;
{
	long oldtimer;
	TIMEOUT *t;
	void (*handler)() = 0;
	long tmpold;

	if ((which != ITIMER_REAL) && (which != ITIMER_VIRTUAL)
		&& (which != ITIMER_PROF)) {
			return EINVFN;
	}

/* ensure that any addresses specified by the calling process are in that
   process's address space
*/
	if ((interval && (!(valid_address((long) interval))))
		|| (value && (!(valid_address((long) value))))
		|| (ointerval && (!(valid_address((long) ointerval))))
		|| (ovalue && (!(valid_address((long) ovalue))))) {
			return EIMBA;
	}

/* see how many milliseconds there were to the timeout */
	oldtimer = 0;

	if (curproc->itimer[which].timeout) {
		for (t = tlist; t; t = t->next) {
			oldtimer += t->when;
			if (t == curproc->itimer[which].timeout)
				goto foundtimer;
		}
		DEBUG(("Tsetitimer: old timer not found!"));
		oldtimer = 0;
foundtimer:
		;
	}

	if (ointerval)
		*ointerval = curproc->itimer[which].interval;
	if (ovalue) {
		if (which == ITIMER_REAL) {
			*ovalue = oldtimer;
		} else {
		  tmpold = curproc->itimer[which].reqtime
		    - (curproc->systime - curproc->itimer[which].startusrtime);
		  if (which == ITIMER_PROF)
		    tmpold -=
		      (curproc->systime - curproc->itimer[which].startsystime);
		  if (tmpold <= 0)
			tmpold = 0;
		  *ovalue = tmpold;
		}
	}
	if (interval)
		curproc->itimer[which].interval = *interval;
	if (value) {
/* cancel old timer */
		if (curproc->itimer[which].timeout)
			canceltimeout(curproc->itimer[which].timeout);
		curproc->itimer[which].timeout = 0;

/* add a new timer, to occur in x milliseconds */
		if (*value) {
			curproc->itimer[which].reqtime = *value;
			curproc->itimer[which].startsystime =
				curproc->systime;
			curproc->itimer[which].startusrtime =
				curproc->usrtime;
			switch (which) {
				case ITIMER_REAL:
					handler = itimer_real_me;
					break;
				case ITIMER_VIRTUAL:
					handler = itimer_virtual_me;
					break;
				case ITIMER_PROF:
					handler = itimer_prof_me;
					break;
				default:
					break;
			}
			curproc->itimer[which].timeout =
				addtimeout(*value, handler);
		}
		else
			curproc->itimer[which].timeout = 0;
	}
	return 0;
}

/*
 * sysconf(which): returns information about system configuration.
 * "which" specifies which aspect of the system configuration is to
 * be returned:
 *	-1	max. value of "which" allowed
 *	0	max. number of memory regions per proc
 *	1	max. length of Pexec() execution string {ARG_MAX}
 *	2	max. number of open files per process	{OPEN_MAX}
 *	3	number of supplementary group id's	{NGROUPS_MAX}
 *	4	max. number of processes per uid	{CHILD_MAX}
 *
 * unlimited values (e.g. CHILD_MAX) are returned as 0x7fffffffL
 *
 * See also Dpathconf() in dosdir.c.
 */

long ARGS_ON_STACK
s_ysconf(which)
	int which;
{
	if (which == -1)
		return 4;

	switch(which) {
		case 0:
			return UNLIMITED;
		case 1:
			return 126;
		case 2:
			return MAX_OPEN;
		case 3:
			return NGROUPS_MAX;
		case 4:
			return UNLIMITED;
		default:
			return EINVFN;
	}
}

/*
 * Salert: send an ALERT message to the user, via the same mechanism
 * the kernel does (i.e. u:\pipe\alert, if it's available
 */

long ARGS_ON_STACK
s_alert(str)
	char *str;
{
/* how's this for confusing code? _ALERT tries to format the
 * string as an alert box; if it fails, we let the full-fledged
 * ALERT function (which will try _ALERT, and fail again)
 * print the alert to the debugging device
 */
	if (_ALERT(str) == 0)
		ALERT(str);
	return 0;
}

/*
 * Suptime: get time in seconds since boot and current load averages from
 * kernel.
 */

#include "loadave.h"

long ARGS_ON_STACK
s_uptime(cur_uptime, loadaverage)
	unsigned long *cur_uptime;
	unsigned long loadaverage[3];
{
	*cur_uptime = uptime;
	loadaverage[0] = avenrun[0];
	loadaverage[1] = avenrun[1];
	loadaverage[2] = avenrun[2];

	return 0;
}

/*
 * routine for initializing DOS
 *
 * NOTE: before adding new functions, check the definition of
 * DOS_MAX at the top of this file to make sure that there
 * is room; if not, increase DOS_MAX.
 */

void
init_dos()
{
/* miscellaneous initialization goes here */

/* dos table initialization */
	dos_tab[0x00] = p_term0;
	dos_tab[0x01] = c_conin;
	dos_tab[0x02] = c_conout;
	dos_tab[0x03] = c_auxin;
	dos_tab[0x04] = c_auxout;
	dos_tab[0x05] = c_prnout;
	dos_tab[0x06] = c_rawio;
	dos_tab[0x07] = c_rawcin;
	dos_tab[0x08] = c_necin;
	dos_tab[0x09] = c_conws;
	dos_tab[0x0a] = c_conrs;
	dos_tab[0x0b] = c_conis;
	dos_tab[0x0e] = d_setdrv;
	dos_tab[0x10] = c_conos;
	dos_tab[0x11] = c_prnos;
	dos_tab[0x12] = c_auxis;
	dos_tab[0x13] = c_auxos;
	dos_tab[0x14] = m_addalt;
	dos_tab[0x15] = s_realloc;
	dos_tab[0x19] = d_getdrv;
	dos_tab[0x1a] = f_setdta;
	dos_tab[0x20] = s_uper;
	dos_tab[0x2a] = t_getdate;
	dos_tab[0x2b] = t_setdate;
	dos_tab[0x2c] = t_gettime;
	dos_tab[0x2d] = t_settime;
	dos_tab[0x2f] = f_getdta;
	dos_tab[0x30] = s_version;
	dos_tab[0x31] = p_termres;
	dos_tab[0x36] = d_free;
	dos_tab[0x39] = d_create;
	dos_tab[0x3a] = d_delete;
	dos_tab[0x3b] = d_setpath;
	dos_tab[0x3c] = f_create;
	dos_tab[0x3d] = f_open;
	dos_tab[0x3e] = f_close;
	dos_tab[0x3f] = f_read;
	dos_tab[0x40] = f_write;
	dos_tab[0x41] = f_delete;
	dos_tab[0x42] = f_seek;
	dos_tab[0x43] = f_attrib;
	dos_tab[0x44] = m_xalloc;
	dos_tab[0x45] = f_dup;
	dos_tab[0x46] = f_force;
	dos_tab[0x47] = d_getpath;
	dos_tab[0x48] = m_alloc;
	dos_tab[0x49] = m_free;
	dos_tab[0x4a] = m_shrink;
	dos_tab[0x4b] = p_exec;
	dos_tab[0x4c] = p_term;
	dos_tab[0x4e] = f_sfirst;
	dos_tab[0x4f] = f_snext;
	dos_tab[0x56] = f_rename;
	dos_tab[0x57] = f_datime;
	dos_tab[0x5c] = f_lock;

/* MiNT extensions to GEMDOS */

	dos_tab[0xff] = s_yield;
	dos_tab[0x100] = f_pipe;
	dos_tab[0x104] = f_cntl;
	dos_tab[0x105] = f_instat;
	dos_tab[0x106] = f_outstat;
	dos_tab[0x107] = f_getchar;
	dos_tab[0x108] = f_putchar;
	dos_tab[0x109] = p_wait;
	dos_tab[0x10a] = p_nice;
	dos_tab[0x10b] = p_getpid;
	dos_tab[0x10c] = p_getppid;
	dos_tab[0x10d] = p_getpgrp;
	dos_tab[0x10e] = p_setpgrp;
	dos_tab[0x10f] = p_getuid;
	dos_tab[0x110] = p_setuid;
	dos_tab[0x111] = p_kill;
	dos_tab[0x112] = p_signal;
	dos_tab[0x113] = p_vfork;
	dos_tab[0x114] = p_getgid;
	dos_tab[0x115] = p_setgid;

	dos_tab[0x116] = p_sigblock;
	dos_tab[0x117] = p_sigsetmask;
	dos_tab[0x118] = p_usrval;
	dos_tab[0x119] = p_domain;
	dos_tab[0x11a] = p_sigreturn;
	dos_tab[0x11b] = p_fork;
	dos_tab[0x11c] = p_wait3;
	dos_tab[0x11d] = f_select;
	dos_tab[0x11e] = p_rusage;
	dos_tab[0x11f] = p_setlimit;
	dos_tab[0x120] = t_alarm;
	dos_tab[0x121] = p_pause;
	dos_tab[0x122] = s_ysconf;
	dos_tab[0x123] = p_sigpending;
	dos_tab[0x124] = d_pathconf;
	dos_tab[0x125] = p_msg;
	dos_tab[0x126] = f_midipipe;
	dos_tab[0x127] = p_renice;
	dos_tab[0x128] = d_opendir;
	dos_tab[0x129] = d_readdir;
	dos_tab[0x12a] = d_rewind;
	dos_tab[0x12b] = d_closedir;
	dos_tab[0x12c] = f_xattr;
	dos_tab[0x12d] = f_link;
	dos_tab[0x12e] = f_symlink;
	dos_tab[0x12f] = f_readlink;
	dos_tab[0x130] = d_cntl;
	dos_tab[0x131] = f_chown;
	dos_tab[0x132] = f_chmod;
	dos_tab[0x133] = p_umask;
	dos_tab[0x134] = p_semaphore;
	dos_tab[0x135] = d_lock;
	dos_tab[0x136] = p_sigpause;
	dos_tab[0x137] = p_sigaction;
	dos_tab[0x138] = p_geteuid;
	dos_tab[0x139] = p_getegid;
	dos_tab[0x13a] = p_waitpid;
	dos_tab[0x13b] = d_getcwd;
	dos_tab[0x13c] = s_alert;
	dos_tab[0x13d] = t_malarm;
	dos_tab[0x13e] = p_sigintr;
	dos_tab[0x13f] = s_uptime;
	dos_tab[0x142] = d_xreaddir;
	dos_tab[0x143] = p_seteuid;
	dos_tab[0x144] = p_setegid;
	dos_tab[0x145] = p_getauid;
	dos_tab[0x146] = p_setauid;
	dos_tab[0x147] = p_getgroups;
	dos_tab[0x148] = p_setgroups;
	dos_tab[0x149] = t_setitimer;

	/* 0x14a-0x151 reserved */

	dos_tab[0x152] = d_readlabel;
	dos_tab[0x153] = d_writelabel;
}
