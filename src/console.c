/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1993,1994 Atari Corporation.
All rights reserved.
*/

/* MiNT routines for doing console I/O */

#include "mint.h"

/*
 * These routines are what Cconout, Cauxout, etc. ultimately call.
 * They take an integer argument which is the user's file handle,
 * and do the translation to a file ptr (and return an appropriate error
 * message if necessary.
 * "mode" may be RAW, COOKED, ECHO, or COOKED|ECHO.
 * note that the user may not call them directly!
 */

long
file_instat(f)
	FILEPTR *f;
{
	long r;

	if (!f) {
		return EIHNDL;
	}
	r = 1;		/* default is to assume input waiting (e.g. TOS files)*/
	if (is_terminal(f))
		(void)tty_ioctl(f, FIONREAD, &r);
	else
		(void)(*f->dev->ioctl)(f, FIONREAD, &r);
	return r;
}

long
file_outstat(f)
	FILEPTR *f;
{
	long r;

	if (!f) {
		return EIHNDL;
	}
	r = 1;		/* default is to assume output OK (e.g. TOS files) */
	if (is_terminal(f))
		(void)tty_ioctl(f, FIONWRITE, &r);
	else
		(void)(*f->dev->ioctl)(f, FIONWRITE, &r);
	return r;
}

long
file_getchar(f, mode)
	FILEPTR *f;
	int mode;
{
	char c;
	long r;

	if (!f) {
		return EIHNDL;
	}
	if (is_terminal(f)) {
		return tty_getchar(f, mode);
	}
	r = (*f->dev->read)(f, &c, 1L);
	if (r != 1)
		return MiNTEOF;
	else
		return ((long)c) & 0xff;
}

long
file_putchar(f, c, mode)
	FILEPTR *f;
	long c;
	int mode;
{
	char ch;

	if (!f) {
		return EIHNDL;
	}
	if (is_terminal(f)) {
		return tty_putchar(f, c & 0x7fffffffL, mode);
	}
	ch = c & 0x00ff;
	return (*f->dev->write)(f, &ch, 1L);
}

/*
 * OK, here are the GEMDOS console I/O routines
 */

long ARGS_ON_STACK
c_conin()
{
	return file_getchar(curproc->handle[0], COOKED|ECHO);
}

long ARGS_ON_STACK
c_conout(c)
	int c;
{
	return file_putchar(curproc->handle[1], (long)c, COOKED);
}

long ARGS_ON_STACK
c_auxin()
{
	return file_getchar(curproc->handle[2], RAW);
}

long ARGS_ON_STACK
c_auxout(c)
	int c;
{
	return file_putchar(curproc->handle[2], (long)c, RAW);
}

long ARGS_ON_STACK
c_prnout(c)
	int c;
{
	return file_putchar(curproc->handle[3], (long)c, RAW);
}

long ARGS_ON_STACK
c_rawio(c)
	int c;
{
	long r;
	PROC *p = curproc;

	if (c == 0x00ff) {
		if (!file_instat(p->handle[0]))
			return 0;
		r = file_getchar(p->handle[0], RAW);
		if (r <= 0)
			return 0;
		return r;
	}
	else
		return file_putchar(p->handle[1], (long)c, RAW);
}

long ARGS_ON_STACK
c_rawcin()
{
	return file_getchar(curproc->handle[0], RAW);
}

long ARGS_ON_STACK
c_necin()
{
	return file_getchar(curproc->handle[0],COOKED|NOECHO);
}

long ARGS_ON_STACK
c_conws(str)
	const char *str;
{
	const char *p = str;
	long cnt = 0;

	while (*p++) cnt++;
	return f_write(1, cnt, str);
}

long ARGS_ON_STACK
c_conrs(buf)
	char *buf;
{
	long size, r;
	char *s;

	size = ((long)*buf) & 0xff;
	r = f_read(0, size, buf+2);
	if (r < 0) {
		buf[1] = 0;
		return r;
	}
/* if reading from a file, stop at first CR or LF encountered */
	s = buf+2;
	size = 0;
	while(r-- > 0) {
		if (*s == '\r' || *s == '\n')
			break;
		s++; size++;
	}
	buf[1] = (char)size;
	return 0;
}

long ARGS_ON_STACK
c_conis()
{
	return -(!!file_instat(curproc->handle[0]));
}

long ARGS_ON_STACK
c_conos()
{
	return -(!!file_outstat(curproc->handle[1]));
}

long ARGS_ON_STACK
c_prnos()
{
	return -(!!file_outstat(curproc->handle[3]));
}

long ARGS_ON_STACK
c_auxis()
{
	return -(!!file_instat(curproc->handle[2]));
}

long ARGS_ON_STACK
c_auxos()
{
	return -(!!file_outstat(curproc->handle[2]));
}

/* Extended GEMDOS routines */

long ARGS_ON_STACK
f_instat(h)
	int h;
{
	PROC *proc;
	int fh = h;

#if O_GLOBAL
	if (fh >= 100) {
		proc = rootproc;
		fh -= 100;
	} else
#endif
		proc = curproc;

	if (fh < MIN_HANDLE || fh >=MAX_OPEN) {
		DEBUG(("Finstat: bad handle %d", h));
		return EIHNDL;
	}
	return file_instat(proc->handle[fh]);
}

long ARGS_ON_STACK
f_outstat(h)
	int h;
{
	int fh = h;
	PROC *proc;
#if O_GLOBAL
	if (fh >= 100) {
		fh -= 100;
		proc = rootproc;
	} else
#endif
		proc = curproc;

	if (fh < MIN_HANDLE || fh >=MAX_OPEN) {
		DEBUG(("Foutstat: bad handle %d", h));
		return EIHNDL;
	}
	return file_outstat(proc->handle[fh]);
}

long ARGS_ON_STACK
f_getchar(h, mode)
	int h, mode;
{
	int fh = h;
	PROC *proc;

#if O_GLOBAL
	if (fh >= 100) {
		fh -= 100;
		proc = rootproc;
	} else
#endif
		proc = curproc;
	if (fh < MIN_HANDLE || fh >=MAX_OPEN) {
		DEBUG(("Fgetchar: bad handle %d", h));
		return EIHNDL;
	}
	return file_getchar(proc->handle[fh], mode);
}

long ARGS_ON_STACK
f_putchar(h, c, mode)
	int h;
	long c;
	int mode;
{
	int fh = h;
	PROC *proc;

#if O_GLOBAL
	if (fh >= 100) {
		fh -= 100;
		proc = rootproc;
	} else
#endif
		proc = curproc;

	if (fh < MIN_HANDLE || fh >=MAX_OPEN) {
		DEBUG(("Fputchar: bad handle %d", h));
		return EIHNDL;
	}
	return file_putchar(proc->handle[fh], c, mode);
}
