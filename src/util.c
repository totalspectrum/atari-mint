/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1992,1993 Atari Corporation.
All rights reserved.
*/

/*
 * misc. utility routines
 */

#include "mint.h"

/*
 * given an address, find the corresponding memory region in this program's
 * memory map
 */

MEMREGION *
addr2mem(a)
	virtaddr a;
{
	int i;

	for (i = 0; i < curproc->num_reg; i++) {
		if (a == curproc->addr[i])
			return curproc->mem[i];
	}
	return 0;
}

/*
 * given a pid, return the corresponding process
 */

PROC *
pid2proc(pid)
	int pid;
{
	PROC *p;

	for (p = proclist; p; p = p->gl_next) {
		if (p->pid == pid)
			return p;
	}
	return 0;
}

/*
 * return a new pid
 */

int
newpid()
{
	static int _maxpid = 1;
	int i;
#ifndef NDEBUG
	int j = 0;
#endif

	do {
		i = _maxpid++;
		if (_maxpid >= 1000) _maxpid = 1;
		assert(j++ < 1000);
	} while (pid2proc(i));

	return i;
}

/*
 * zero out a block of memory, quickly; the block must be word-aligned,
 * and should be long-aligned for speed reasons
 */

void
zero(place, size)
	char *place;
	long size;
{
	long cruft;
	long blocksize;

	cruft = size % 256;	/* quickzero does 256 byte blocks */
	blocksize = size/256;	/* divide by 256 */
	if (blocksize > 0) {
		quickzero(place, blocksize);
		place += (blocksize*256);
	}
	while (cruft > 0) {
		*place++ = 0;
		cruft--;
	}
}

#ifdef JUNK_MEM
void
fillwjunk(place, size)
	long *place;
	long size;
{
	while (size > 0) {
		*place++ = size;
		size -= 4;
	}
}
#endif

/*
 * kernel memory allocation routines
 */

#define KERMEM_THRESHOLD (QUANTUM-8)
#if 0
#define KERMEM_SIZE QUANTUM
#else
#define KERMEM_SIZE ((KERMEM_THRESHOLD+8)*2)
#endif
#define KMAGIC ((MEMREGION *)0x87654321L)
#define NKMAGIC 0x19870425L

void * ARGS_ON_STACK 
kmalloc(size)
	long size;
{
	MEMREGION *m;
	MEMREGION **p;
	long *lp;

	/*
	 * increase size by two pointers' worth: the first contains
	 * a pointer to the region descriptor for this block, and the
	 * second contains KMAGIC.  If the block came from nalloc,
	 * then they both contain NKMAGIC.
	 */
	size += sizeof(m) + sizeof(m);
/*
 * for small requests, we use nalloc first
 */
tryagain:
	if (size < KERMEM_THRESHOLD) {
	    lp = nalloc(size);
	    if (lp) {
		*lp++ = NKMAGIC;
		*lp++ = NKMAGIC;
		TRACELOW(("kmalloc(%lx) -> (nalloc) %lx",size,lp));
		return lp;
	    }
	    else {
		DEBUG(("kmalloc(%lx): nalloc is out of memory",size));

	/* If this is commented out, then we fall through to try_getregion */
		if (0 == (m = get_region(alt, KERMEM_SIZE, PROT_S))) {
		    if (0 == (m = get_region(core, KERMEM_SIZE, PROT_S))) {
			DEBUG(("No memory for another arena"));
			goto try_getregion;
		    }
		}
		lp = (long *)m->loc;
		*lp++ = (long)KMAGIC;
		*lp++ = (long)m;
		nalloc_arena_add((void *)lp,KERMEM_SIZE - 2*SIZEOF(long));
		goto tryagain;
	    }
	}

try_getregion:
	m = get_region(alt, size, PROT_S);

	if (!m) m = get_region(core, size, PROT_S);

	if (m) {
		p = (MEMREGION **)m->loc;
		*p++ = KMAGIC;
		*p++ = m;
		TRACELOW(("kmalloc(%lx) -> (get_region) %lx",size,p));
		return (void *)p;
	}
	else {
		TRACELOW(("kmalloc(%lx) -> (fail)",size));
#if 0
		/* this is a serious offense; I want to hear about it */
		/* maybe Allan wanted to hear about it, but ordinary users
		 * won't! -- ERS
		 */
		NALLOC_DUMP();
		BIG_MEM_DUMP(0,0);
#endif
		return 0;
	}
}

/* allocate from ST memory only */

void *
kcore(size)
	long size;
{
	MEMREGION *m;
	MEMREGION **p;

	size += sizeof(m) + sizeof (m);
	m = get_region(core, size, PROT_S);

	if (m) {
		p = (MEMREGION **)m->loc;
		*p++ = KMAGIC;
		*p++ = m;
		return (void *)p;
	}
	else {
		return 0;
	}
}

void ARGS_ON_STACK 
kfree(place)
	void *place;
{
	MEMREGION **p;
	MEMREGION *m;

	TRACELOW(("kfree(%lx)",place));

	if (!place) return;
	p = place;
	p -= 2;
	if (*p == (MEMREGION *)NKMAGIC) {
	    nfree(p);
	    return;
	}
	else if (*p++ != KMAGIC) {
		FATAL("kfree: memory not allocated by kmalloc");
	}
	m = *p;
	if (--m->links != 0) {
		FATAL("kfree: block has %d links", m->links);
	}
	free_region(m);
}

/*
 * "user" memory allocation routines; the kernel can use these to
 * allocate/free memory that will be attached in some way to a process
 * (and freed automatically when the process exits)
 */
void * ARGS_ON_STACK 
umalloc(size)
	long size;
{
	return (void *)m_xalloc(size, 3);
}

void ARGS_ON_STACK 
ufree(block)
	void *block;
{
	(void)m_free((virtaddr)block);
}

/*
 * convert a time in milliseconds to a GEMDOS style date/time
 * timeptr[0] gets the time, timeptr[1] the date.
 * BUG/FEATURE: in the conversion, it is assumed that all months have
 * 30 days and all years have 360 days.
 */

void ARGS_ON_STACK 
ms_time(ms, timeptr)
	ulong ms;
	short *timeptr;
{
	ulong secs;
	short tsec, tmin, thour;
	short tday, tmonth, tyear;

	secs = ms / 1000;
	tsec = secs % 60;
	secs /= 60;		/* secs now contains # of minutes */
	tmin = secs % 60;
	secs /= 60;		/* secs now contains # of hours */
	thour = secs % 24;
	secs /= 24;		/* secs now contains # of days */
	tday = secs % 30;
	secs /= 30;
	tmonth = secs % 12;
	tyear = secs / 12;
	*timeptr++ = (thour << 11) | (tmin << 5) | (tsec >> 1);
	*timeptr = (tyear << 9) | ((tmonth + 1) << 5) | (tday+1);
}

/*
 * unixtim(time, date): convert a Dos style (time, date) pair into
 * a Unix time (seconds from midnight Jan 1., 1970)
 */

static int const
mth_start[13] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };

long ARGS_ON_STACK 
unixtim(time, date)
	unsigned time, date;
{
	int sec, min, hour;
	int mday, mon, year;
	long y, s;

	sec = (time & 31) << 1;
	min = (time >> 5) & 63;
	hour = (time >> 11) & 31;
	mday = date & 31;
	mon = ((date >> 5) & 15) - 1;
	year = 80 + ((date >> 9) & 255);

/* calculate tm_yday here */
	y = (mday - 1) + mth_start[mon] + /* leap year correction */
		( ( (year % 4) != 0 ) ? 0 : (mon > 1) );

	s = (sec) + (min * 60L) + (hour * 3600L) +
		(y * 86400L) + ((year - 70) * 31536000L) +
		((year - 69)/4) * 86400L;

	return s;
}

/* convert a Unix time into a DOS time. The longword returned contains
   the time word first, then the date word.
   BUG: we completely ignore any time zone information.
*/
#define SECS_PER_MIN    (60L)
#define SECS_PER_HOUR   (3600L)
#define SECS_PER_DAY    (86400L)
#define SECS_PER_YEAR   (31536000L)
#define SECS_PER_LEAPYEAR (SECS_PER_DAY + SECS_PER_YEAR)

static int
days_per_mth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

long ARGS_ON_STACK 
dostim(t)
	long t;
{
        unsigned long time, date;
	int tm_hour, tm_min, tm_sec;
	int tm_year, tm_mon, tm_mday;
	int i;

	if (t <= 0) return 0;

	tm_year = 70;
	while (t >= SECS_PER_YEAR) {
		if ((tm_year & 0x3) == 0) {
			if (t < SECS_PER_LEAPYEAR)
				break;
			t -= SECS_PER_LEAPYEAR;
		} else {
			t -= SECS_PER_YEAR;
		}
		tm_year++;
	}
	tm_mday = (int)(t/SECS_PER_DAY);
        days_per_mth[1] = (tm_year & 0x3) ? 28 : 29;
        for (i = 0; tm_mday >= days_per_mth[i]; i++)
                tm_mday -= days_per_mth[i];
        tm_mon = i+1;
	tm_mday++;
        t = t % SECS_PER_DAY;
        tm_hour = (int)(t/SECS_PER_HOUR);
        t = t % SECS_PER_HOUR;
        tm_min = (int)(t/SECS_PER_MIN);
        tm_sec = (int)(t % SECS_PER_MIN);

	if (tm_year < 80) {
		tm_year = 80;
		tm_mon = tm_mday = 1;
		tm_hour = tm_min = tm_sec = 0;
	}

	time = (tm_hour << 11) | (tm_min << 5) | (tm_sec >> 1);
	date = ((tm_year - 80) & 0x7f) << 9;
	date |= ((tm_mon) << 5) | (tm_mday);
	return (time << 16) | date;
}

/*
 * Case insensitive string comparison. note that this only returns
 * 0 (match) or nonzero (no match), and that the returned value
 * is not a reliable indicator of any "order".
 */

int ARGS_ON_STACK 
strnicmp(str1, str2, len)
	register const char *str1, *str2;
	register int len;
{
	register char c1, c2;

	do {
		c1 = *str1++; if (isupper(c1)) c1 = tolower(c1);
		c2 = *str2++; if (isupper(c2)) c2 = tolower(c2);
	} while (--len >= 0 && c1 && c1 == c2);

	if (len < 0 || c1 == c2)
		return 0;
	return c1 - c2;
}

int ARGS_ON_STACK 
stricmp(str1, str2)
	const char *str1, *str2;
{
	return strnicmp(str1, str2, 0x7fff);
}


/*
 * string utilities: strlwr() converts a string to lower case, strupr()
 * converts it to upper case
 */

char * ARGS_ON_STACK 
strlwr(s)
	char *s;
{
	char c;
	char *old = s;

	while ((c = *s) != 0) {
		if (isupper(c)) {
			*s = _tolower(c);
		}
		s++;
	}
	return old;
}

char * ARGS_ON_STACK 
strupr(s)
	char *s;
{
	char c;
	char *old = s;

	while ((c = *s) != 0) {
		if (islower(c)) {
			*s = _toupper(c);
		}
		s++;
	}
	return old;
}

#ifdef OWN_LIB

/*
 * Case sensitive comparison functions.
 */

int
strncmp(str1, str2, len)
	register const char *str1, *str2;
	register int len;
{
	register char c1, c2;

	do {
		c1 = *str1++;
		c2 = *str2++;
	} while (--len >= 0 && c1 && c1 == c2);

	if (len < 0) return 0;

	return c1 - c2;
}

int
strcmp(str1, str2)
	const char *str1, *str2;
{
	register char c1, c2;

	do {
		c1 = *str1++;
		c2 = *str2++;
	} while (c1 && c1 == c2);

	return c1 - c2;
}


/*
 * some standard string functions
 */

char *
strcat(dst, src)
	char *dst;
	const char *src;
{
	register char *_dscan;

	for (_dscan = dst; *_dscan; _dscan++) ;
	while ((*_dscan++ = *src++) != 0) ;
	return dst;
}

char *
strcpy(dst, src)
	char *dst;
	const char *src;
{
	register char *_dscan = dst;
	while ((*_dscan++ = *src++) != 0) ;
	return dst;
}

char *
strncpy(dst, src, len)
	char *dst;
	const char *src;
	int len;
{
	register char *_dscan = dst;
	while (--len >= 0 && (*_dscan++ = *src++) != 0)
		continue;
	while (--len >= 0)
		*_dscan++ = 0;
	return dst;
}

int
strlen(scan)
	const char *scan;
{
	register const char *_start = scan+1;
	while (*scan++) ;
	return (int)((long)scan - (long)_start);
}

/*
 * strrchr: find the last occurence of a character in a string
 */
char *
strrchr(str, which)
	const char *str;
	register int which;
{
	register unsigned char c, *s;
	register char *place;

	s = (unsigned char *)str;
	place = 0;
	do {
		c = *s++;
		if (c == which)
			place = (char *)s-1;
	} while (c);
	return place;
}

unsigned char _ctype[256] =
{
	_CTc, _CTc, _CTc, _CTc,				/* 0x00..0x03 */
	_CTc, _CTc, _CTc, _CTc,				/* 0x04..0x07 */
	_CTc, _CTc|_CTs, _CTc|_CTs, _CTc|_CTs,		/* 0x08..0x0B */
	_CTc|_CTs, _CTc|_CTs, _CTc, _CTc,		/* 0x0C..0x0F */

	_CTc, _CTc, _CTc, _CTc,				/* 0x10..0x13 */
	_CTc, _CTc, _CTc, _CTc,				/* 0x14..0x17 */
	_CTc, _CTc, _CTc, _CTc,				/* 0x18..0x1B */
	_CTc, _CTc, _CTc, _CTc,				/* 0x1C..0x1F */

	_CTs, _CTp, _CTp, _CTp,				/* 0x20..0x23 */
	_CTp, _CTp, _CTp, _CTp,				/* 0x24..0x27 */
	_CTp, _CTp, _CTp, _CTp,				/* 0x28..0x2B */
	_CTp, _CTp, _CTp, _CTp,				/* 0x2C..0x2F */

	_CTd|_CTx, _CTd|_CTx, _CTd|_CTx, _CTd|_CTx,	/* 0x30..0x33 */
	_CTd|_CTx, _CTd|_CTx, _CTd|_CTx, _CTd|_CTx,	/* 0x34..0x37 */
	_CTd|_CTx, _CTd|_CTx, _CTp, _CTp,		/* 0x38..0x3B */
	_CTp, _CTp, _CTp, _CTp,				/* 0x3C..0x3F */

	_CTp, _CTu|_CTx, _CTu|_CTx, _CTu|_CTx,		/* 0x40..0x43 */
	_CTu|_CTx, _CTu|_CTx, _CTu|_CTx, _CTu,		/* 0x44..0x47 */
	_CTu, _CTu, _CTu, _CTu,				/* 0x48..0x4B */
	_CTu, _CTu, _CTu, _CTu,				/* 0x4C..0x4F */

	_CTu, _CTu, _CTu, _CTu,				/* 0x50..0x53 */
	_CTu, _CTu, _CTu, _CTu,				/* 0x54..0x57 */
	_CTu, _CTu, _CTu, _CTp,				/* 0x58..0x5B */
	_CTp, _CTp, _CTp, _CTp,				/* 0x5C..0x5F */

	_CTp, _CTl|_CTx, _CTl|_CTx, _CTl|_CTx,		/* 0x60..0x63 */
	_CTl|_CTx, _CTl|_CTx, _CTl|_CTx, _CTl,		/* 0x64..0x67 */
	_CTl, _CTl, _CTl, _CTl,				/* 0x68..0x6B */
	_CTl, _CTl, _CTl, _CTl,				/* 0x6C..0x6F */

	_CTl, _CTl, _CTl, _CTl,				/* 0x70..0x73 */
	_CTl, _CTl, _CTl, _CTl,				/* 0x74..0x77 */
	_CTl, _CTl, _CTl, _CTp,				/* 0x78..0x7B */
	_CTp, _CTp, _CTp, _CTc,				/* 0x7C..0x7F */

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x80..0x8F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x90..0x9F */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xA0..0xAF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xB0..0xBF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xC0..0xCF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xD0..0xDF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0xE0..0xEF */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  /* 0xF0..0xFF */
};

int toupper(c)
	int c;
{
	return(islower(c) ? (c ^ 0x20) : (c));
}

int tolower(c)
	int c;
{
	return(isupper(c) ? (c ^ 0x20) : (c));
}

/*
 * converts a decimal string to an integer
 */

long
atol(s)
	const char *s;
{
	long d = 0;
	int negflag = 0;
	int c;

	while (*s && isspace(*s)) s++;
	while (*s == '-' || *s == '+') {
		if (*s == '-')
			negflag ^= 1;
		s++;
	}
	while ((c = *s++) != 0 && isdigit(c)) {
		d = 10 * d + (c - '0');
	}
	if (negflag) d = -d;
	return d;
}

#endif /* OWN_LIB */
