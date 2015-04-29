/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
*/

/* MiNT debugging output routines */
/* also, ksprintf is put here, for lack of any better place to put it */

#include "mint.h"
#include <stdarg.h>

static void VDEBUGOUT P_((int, const char *, va_list));
int vksprintf(char *buf, const char *fmt, va_list args);

/*
 * ksprintf implements a very crude sprintf() function that provides only
 * what MiNT needs...
 *
 * NOTE: this sprintf probably doesn't conform to any standard at
 * all. It's only use in life is that it won't overflow fixed
 * size buffers (i.e. it won't try to write more than SPRINTF_MAX
 * characters into a string)
 */

static int
PUTC(char *p, int c, int *cnt, int width) {
	int put = 1;

	if (*cnt <= 0) return 0;
	*p++ = c;
	*cnt -= 1;
	while (*cnt > 0 && --width > 0) {
		*p++ = ' ';
		*cnt -= 1;
		put++;
	}
	return put;
}

static int
PUTS(char *p, const char *s, int *cnt, int width) {
	int put = 0;

	if (s == 0) s = "(null)";

	while (*cnt > 0 && *s) {
		*p++ = *s++;
		put++;
		*cnt -= 1;
		width--;
	}
	while (width-- > 0 && *cnt > 0) {
		*p++ = ' ';
		put++;
		*cnt -= 1;
	}
	return put;
}

static int
PUTL(char *p, unsigned long u, int base, int *cnt, int width, int fill_char)
{
	int put = 0;
	static char obuf[32];
	char *t;

	t = obuf;

	do {
		*t++ = "0123456789ABCDEF"[u % base];
		u /= base;
		width--;
	} while (u > 0);

	while (width-- > 0 && *cnt > 0) {
		*p++ = fill_char;
		put++;
		*cnt -= 1;
	}
	while (*cnt > 0 && t != obuf) {
		*p++ = *--t;
		put++;
		*cnt -= 1;
	}
	return put;
}

int
vksprintf(char *buf, const char *fmt, va_list args)
{
	char *p = buf, c, fill_char;
	char *s_arg;
	int i_arg;
	long l_arg;
	int cnt;
	int width, long_flag;

	cnt = SPRINTF_MAX - 1;
	while( (c = *fmt++) != 0 ) {
		if (c != '%') {
			p += PUTC(p, c, &cnt, 1);
			continue;
		}
		c = *fmt++;
		width = 0;
		long_flag = 0;
		fill_char = ' ';
		if (c == '0') fill_char = '0';
		while (c && isdigit(c)) {
			width = 10*width + (c-'0');
			c = *fmt++;
		}
		if (c == 'l' || c == 'L') {
			long_flag = 1;
			c = *fmt++;
		}
		if (!c) break;

		switch (c) {
		case '%':
			p += PUTC(p, c, &cnt, width);
			break;
		case 'c':
			i_arg = va_arg(args, int);
			p += PUTC(p, i_arg, &cnt, width);
			break;
		case 's':
			s_arg = va_arg(args, char *);
			p += PUTS(p, s_arg, &cnt, width);
			break;
		case 'd':
			if (long_flag) {
				l_arg = va_arg(args, long);
			} else {
				l_arg = va_arg(args, int);
			}
			if (l_arg < 0) {
				p += PUTC(p, '-', &cnt, 1);
				width--;
				l_arg = -l_arg;
			}
			p += PUTL(p, l_arg, 10, &cnt, width, fill_char);
			break;
		case 'o':
			if (long_flag) {
				l_arg = va_arg(args, long);
			} else {
				l_arg = va_arg(args, unsigned int);
			}
			p += PUTL(p, l_arg, 8, &cnt, width, fill_char);
			break;
		case 'x':
			if (long_flag) {
				l_arg = va_arg(args, long);
			} else {
				l_arg = va_arg(args, unsigned int);
			}
			p += PUTL(p, l_arg, 16, &cnt, width, fill_char);
			break;
		case 'u':
			if (long_flag) {
				l_arg = va_arg(args, long);
			} else {
				l_arg = va_arg(args, unsigned int);
			}
			p += PUTL(p, l_arg, 10, &cnt, width, fill_char);
			break;

		}
	}
	*p = 0;
	return (int)(p - buf);
}

int ARGS_ON_STACK 
ksprintf(char *buf, const char *fmt, ...)
{
	va_list args;
	int foo;

	va_start(args, fmt);
	foo = vksprintf(buf, fmt, args);	
	va_end(args);
	return foo;
}

int debug_level = 1;	/* how much debugging info should we print? */
int out_device = 2;	/* BIOS device to write errors to */

/*
 * out_next[i] is the out_device value to use when the current
 * device is i and the user hits F3.
 * Cycle is CON -> PRN -> AUX -> MIDI -> 6 -> 7 -> 8 -> 9 -> CON
 * (Note: BIOS devices 6-8 exist on Mega STe and TT, 9 on TT.)
 *
 * out_device and this table are exported to bios.c and used here in HALT().
 */

/*		    0  1  2  3  4  5  6  7  8  9 */
char out_next[] = { 1, 3, 0, 6, 0, 0, 7, 8, 9, 2 };

/*
 * debug log modes:
 *
 * 0: no logging.
 * 1: log all messages, dump the log any time something happens at
 *    a level that gets shown.  Thus, if you're at debug_level 2,
 *    everything is logged, and if something at levels 1 or 2 happens,
 *    the log is dumped.
 *
 * LB_LINE_LEN is 20 greater than SPRINTF_MAX because up to 20 bytes
 * are prepended to the buffer string passed to ksprintf.
 */

#define LBSIZE 50				/* number of lines */
#define LB_LINE_LEN (SPRINTF_MAX+20)		/* width of a line */
int debug_logging;
int logptr;
static char logbuf[LBSIZE][LB_LINE_LEN];
static short logtime[LBSIZE];	/* low 16 bits of 200Hz: timestamp of msg */

/*
 * Extra terse settings - don't even output ALERTs unless asked to.
 *
 * Things that happen in on an idle Desktop are at LOW_LEVEL:
 * Psemaphore, Pmsg, Syield.
 */
#if 0	/* now in debug.h */
#define FORCE_LEVEL 0
#define ALERT_LEVEL 1
#define DEBUG_LEVEL 2
#define TRACE_LEVEL 3
#define LOW_LEVEL 4
#endif

/*
 * The inner loop does this: at each newline, the keyboard is polled. If
 * you've hit a key, then it's checked: if it's ctl-alt, do_func_key is
 * called to do what it says, and that's that.  If not, then you pause the
 * output.  If you now hit a ctl-alt key, it gets done and you're still
 * paused.  Only hitting a non-ctl-alt key will get you out of the pause. 
 * (And only a non-ctl-alt key got you into it, too!)
 *
 * When out_device isn't the screen, number keys give you the same effects
 * as function keys.  The only way to get into this code, however, is to
 * have something produce debug output in the first place!  This is
 * awkward: Hit a key on out_device, then hit ctl-alt-F5 on the console so
 * bios.c will call DUMPPROC, which will call ALERT, which will call this.
 * It'll see the key you hit on out_device and drop you into this loop.
 * CTL-ALT keys make BIOS call do_func_key even when out_device isn't the
 * console.
 */

void
debug_ws(s)
	const char *s;
{
	long key;
	int scan;
	int stopped;

    while (*s) {
	(void)Bconout(out_device, *s);
	while (*s == '\n' && out_device != 0 && Bconstat(out_device)) {
	    stopped = 0;
	    while (1) {
		if (out_device == 2) {
		/* got a key; if ctl-alt then do it */
		    if ((Kbshift(-1) & 0x0c) == 0x0c) {
			key = Bconin(out_device);
			scan = (int) (((key >> 16) & 0xff));
			do_func_key(scan);
			goto ptoggle;
		    }
		    else goto cont;
		}
		else {
		    key = Bconin(out_device);
		    if (key < '0' || key > '9') {
ptoggle:		/* not a func key */
			if (stopped) break;
			else stopped = 1;
		    }
		    else {
			/* digit key from debug device == Fn */
			if (key == '0') scan = 0x44;
			else scan = (int) (key - '0' + 0x3a);
			do_func_key(scan);
		    }
		}
	    }
	}
cont:
	s++;
    }
}

/*
 * _ALERT(s) returns 1 for success and 0 for failure.
 * It attempts to write the string to "the alert pipe," u:\pipe\alert.
 * If the write fails because the pipe is full, we "succeed" anyway.
 *
 * This is called in vdebugout and also in memprot.c for memory violations.
 * It's also used by the Salert() system call in dos.c.
 */

int
_ALERT(s)
char *s;
{
    FILEPTR *f;
    char alertbuf[SPRINTF_MAX+10], *ptr, *lastspace;
    int counter;
    char *alert;
    int olddebug = debug_level;
    int oldlogging = debug_logging;

/* temporarily reduce the debug level, so errors finding
 * u:\pipe\alert don't get reported
 */
    debug_level = debug_logging = 0;
    f = do_open("u:\\pipe\\alert",(O_WRONLY | O_NDELAY),0,(XATTR *)0);
    debug_level = olddebug;
    debug_logging = oldlogging;

    if (f) {
/*
 * format the string into an alert box
 */
	if (*s == '[') {	/* already an alert */
		alert = s;
	} else {
		alert = alertbuf;
		ksprintf(alertbuf, "[1][%s", s);
/*
 * make sure no lines exceed 30 characters; also, filter out any
 * reserved characters like '[' or ']'
 */
		ptr = alertbuf+4;
		counter = 0;
		lastspace = 0;
		while(*ptr) {
			if (*ptr == ' ') {
				lastspace = ptr;
			} else if (*ptr == '[') {
				*ptr = '(';
			} else if (*ptr == ']') {
				*ptr = ')';
			} else if (*ptr == '|') {
				*ptr = ':';
			}
			if (counter++ >= 29) {
				if (lastspace) {
					*lastspace = '|';
					counter = (int) (ptr - lastspace);
					lastspace = 0;
				} else {
					*ptr = '|';
					counter = 0;
				}
			}
			ptr++;
		}
		strcpy(ptr, "][  OK  ]");
	}

	(*f->dev->write)(f,alert,(long)strlen(alert)+1);
	do_close(f);
	return 1;
    }
    else return 0;
}

static void
VDEBUGOUT(level, s, args)
	int level;
	const char *s;
	va_list args;
{
	char *lp;
	char *lptemp;

	logtime[logptr] = (short)(*(long *)0x4baL);
	lp = logbuf[logptr];
	if (++logptr == LBSIZE) logptr = 0;

	if (curproc) {
	    ksprintf(lp,"pid %3d (%s): ", curproc->pid, curproc->name);
	    lptemp = lp+strlen(lp);
	}
	else {
	    lptemp = lp;
	}

	vksprintf(lptemp, s, args);

	/* for alerts, try the alert pipe unconditionally */
	if (level == ALERT_LEVEL && _ALERT(lp)) return;

	if (debug_level >= level) {
		debug_ws(lp);
		debug_ws("\r\n");
	}
}

void ARGS_ON_STACK Tracelow(const char *s, ...)
{
	va_list args;

	if (debug_logging || (debug_level >= LOW_LEVEL)) {
		va_start(args, s);
		VDEBUGOUT(LOW_LEVEL, s, args);
		va_end(args);
	}
}

void ARGS_ON_STACK Trace(const char *s, ...)
{
	va_list args;

	if (debug_logging || (debug_level >= TRACE_LEVEL)) {
		va_start(args, s);
		VDEBUGOUT(TRACE_LEVEL, s, args);
		va_end(args);
	}
}

void ARGS_ON_STACK Debug(const char *s, ...)
{
	va_list args;

	if (debug_logging || (debug_level >= DEBUG_LEVEL)) {
		va_start(args, s);
		VDEBUGOUT(DEBUG_LEVEL, s, args);
		va_end(args);
	}
	if (debug_logging && debug_level >= DEBUG_LEVEL) DUMPLOG();
}

void ARGS_ON_STACK ALERT(const char *s, ...)
{
	va_list args;

	if (debug_logging || debug_level >= ALERT_LEVEL) {
	    va_start(args, s);
	    VDEBUGOUT(ALERT_LEVEL, s, args);
	    va_end(args);
	}
	if (debug_logging && debug_level >= ALERT_LEVEL) DUMPLOG();
}

void ARGS_ON_STACK FORCE(const char *s, ...)
{
	va_list args;

	va_start(args, s);
	VDEBUGOUT(FORCE_LEVEL, s, args);
	va_end(args);
	/* don't dump log here - hardly ever what you mean to do. */
}

void
DUMPLOG()
{
	char *end;
	char *start;
	short *timeptr;
	char timebuf[6];

	/* logbuf[logptr] is the oldest string here */

	end = start = logbuf[logptr];
	timeptr = &logtime[logptr];

	do {
	    if (*start) {
		ksprintf(timebuf,"%04x ",*timeptr);
		debug_ws(timebuf);
		debug_ws(start);
		debug_ws("\r\n");
		*start = '\0';
	    }
	    start += LB_LINE_LEN;
	    timeptr++;
#ifdef LATTICE
#pragma ignore 83	/* [reference beyond object size] */
#endif
	    if (start == logbuf[LBSIZE]) {
#ifdef LATTICE
#pragma warning 83	/* [reference beyond object size] */
#endif
		start = logbuf[0];
		timeptr = &logtime[0];
	    }
        } while (start != end);

        logptr = 0;
}

/* wait for a key to be pressed */
void
PAUSE()
{
	debug_ws("Hit a key\r\n");
	(void)Bconin(2);
}
  
EXITING
void ARGS_ON_STACK FATAL(const char *s, ...)
{
	va_list args;

	va_start(args, s);
	VDEBUGOUT(-1, s, args);
	va_end(args);
	if (debug_logging) {
		DUMPLOG();
	}

	HALT();
}


static const char *rebootmsg[MAXLANG] = {
"FATAL ERROR. You must reboot the system.\r\n",
"FATALER FEHLER. Das System muž neu gestartet werden.\r\n",	/* German */
"FATAL ERROR. You must reboot the system.\r\n",		/* French */
"FATAL ERROR. You must reboot the system.\r\n",		/* UK */
"FATAL ERROR. You must reboot the system.\r\n",		/* Spanish */
"FATAL ERROR. You must reboot the system.\r\n"		/* Italian */
};

EXITING 
void HALT()
{
	long r;
	long key;
	int scan;
	extern long tosssp;	/* in main.c */
#ifdef PROFILING
	extern EXITING _exit P_((int)) NORETURN;
#endif
	restr_intr();	/* restore interrupts to normal */
#ifdef DEBUG_INFO
	debug_ws("Fatal MiNT error: adjust debug level and hit a key...\r\n");
#else
	debug_ws(rebootmsg[gl_lang]);
#endif
	sys_q[READY_Q] = 0;	/* prevent context switches */

	for(;;) {
		/* get a key; if ctl-alt then do it, else halt */
		key = Bconin(out_device);
		if ((key & 0x0c000000L) == 0x0c000000L) {
			scan = (int) ((key >> 16) & 0xff);
			do_func_key(scan);
		}
		else {
			break;
		}
	}
	for(;;) {
		debug_ws(rebootmsg[gl_lang]);
		r = Bconin(2);

		if ( (r & 0x0ff) == 'x' ) {
			extern int no_mem_prot;
			close_filesys();
			if (!no_mem_prot)
				restr_mmu();
			restr_screen();
			(void)Super((void *)tosssp);	/* gratuitous (void *) for Lattice */
#ifdef PROFILING
			_exit(0);
#else
			Pterm0();
#endif
		}
	}
}


/* some key definitions */
#define CTRLALT 0xc
#define DEL 0x53	/* scan code of delete key */
#define UNDO 0x61	/* scan code of undo key */

void
do_func_key(scan)
	int scan;
{
	extern struct tty con_tty;

	switch (scan) {
	case DEL:
		reboot();
		break;
	case UNDO:
		killgroup(con_tty.pgrp, SIGQUIT, 1);
		break;
#ifdef DEBUG_INFO
	case 0x3b:		/* F1: increase debugging level */
		debug_level++;
		break;
	case 0x3c:		/* F2: reduce debugging level */
		if (debug_level > 0)
			--debug_level;
		break;
	case 0x3d:		/* F3: cycle out_device */
		out_device = out_next[out_device];
		break;
	case 0x3e:		/* F4: set out_device to console */
		out_device = 2;
		break;
	case 0x3f:		/* F5: dump memory */
		DUMP_ALL_MEM();
		break;
	case 0x58:		/* shift+F5: dump kernel allocated memory */
		NALLOC_DUMP();
		break;
	case 0x40:		/* F6: dump processes */
		DUMPPROC();
		break;
	case 0x41:		/* F7: toggle debug_logging */
		debug_logging ^= 1;
		break;
	case 0x42:		/* F8: dump log */
		DUMPLOG();
		break;
	case 0x43:		/* F9: dump the global memory table */
		QUICKDUMP();
		break;
	case 0x5c:		/* shift-F9: dump the mmu tree */
		BIG_MEM_DUMP(1,curproc);
		break;
	case 0x44:		/* F10: do an annotated dump of memory */
		BIG_MEM_DUMP(0,0);
		break;
#endif
	}
}
