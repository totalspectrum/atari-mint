/*
Copyright 1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
 */

/* PROC pseudo-filesystem routines */
/* basically just to allow 'ls -l X:' to give a list of active processes
 * some things to note:
 * process names are given as name.XXX, where 'XXX' is the pid of the
 *   process
 * process attributes depend on the run queue as follows:
 *   RUNNING:	0x00		(normal)
 *   READY:	0x01		(read-only)
 *   WAIT:	0x20		(archive bit)
 *   IOBOUND:	0x21		(archive bit+read-only)
 *   ZOMBIE:	0x22		(archive+hidden)
 *   TSR:	0x02		(hidden)
 *   STOP:	0x24		(archive bit+system)
 * the general principle is: inactive processes have the archive bit (0x20)
 * set, terminated processes have the hidden bit (0x02) set, stopped processes
 * have the system bit (0x04) set, and the read-only bit is used to
 * otherwise distinguish states (which is unfortunate, since it would be
 * nice if this bit corresponded with file permissions).
 */

#include "mint.h"


static long	ARGS_ON_STACK proc_root	P_((int drv, fcookie *fc));
static long	ARGS_ON_STACK proc_lookup	P_((fcookie *dir, const char *name, fcookie *fc));
static long	ARGS_ON_STACK proc_getxattr	P_((fcookie *fc, XATTR *xattr));
static long	ARGS_ON_STACK proc_chattr	P_((fcookie *fc, int attrib));
static long	ARGS_ON_STACK proc_chown	P_((fcookie *fc, int uid, int gid));
static long	ARGS_ON_STACK proc_chmode	P_((fcookie *fc, unsigned mode));
static long	ARGS_ON_STACK proc_rmdir	P_((fcookie *dir, const char *name));
static long	ARGS_ON_STACK proc_remove	P_((fcookie *dir, const char *name));
static long	ARGS_ON_STACK proc_getname	P_((fcookie *root, fcookie *dir, char *pathname,
						    int size));
static long	ARGS_ON_STACK proc_rename	P_((fcookie *olddir, char *oldname,
				    fcookie *newdir, const char *newname));
static long	ARGS_ON_STACK proc_opendir	P_((DIR *dirh, int flags));
static long	ARGS_ON_STACK proc_readdir	P_((DIR *dirh, char *nm, int nmlen, fcookie *));
static long	ARGS_ON_STACK proc_rewinddir	P_((DIR *dirh));
static long	ARGS_ON_STACK proc_closedir	P_((DIR *dirh));
static long	ARGS_ON_STACK proc_pathconf	P_((fcookie *dir, int which));
static long	ARGS_ON_STACK proc_dfree	P_((fcookie *dir, long *buf));
static DEVDRV *	ARGS_ON_STACK proc_getdev	P_((fcookie *fc, long *devsp));

static long	ARGS_ON_STACK proc_open	P_((FILEPTR *f));
static long	ARGS_ON_STACK proc_write	P_((FILEPTR *f, const char *buf, long bytes));
static long	ARGS_ON_STACK proc_read	P_((FILEPTR *f, char *buf, long bytes));
static long	ARGS_ON_STACK proc_lseek	P_((FILEPTR *f, long where, int whence));
static long	ARGS_ON_STACK proc_ioctl	P_((FILEPTR *f, int mode, void *buf));
static long	ARGS_ON_STACK proc_datime	P_((FILEPTR *f, short *time, int rwflag));
static long	ARGS_ON_STACK proc_close	P_((FILEPTR *f, int pid));
static long ARGS_ON_STACK proc_readlabel P_((fcookie *dir, char *name, int namelen));

/* dummy routines from biosfs.c */
extern long	ARGS_ON_STACK null_select	P_((FILEPTR *f, long p, int mode));
extern void	ARGS_ON_STACK null_unselect	P_((FILEPTR *f, long p, int mode));

static PROC *	name2proc	P_((const char *name));


DEVDRV proc_device = {
	proc_open, proc_write, proc_read, proc_lseek, proc_ioctl, proc_datime,
	proc_close, null_select, null_unselect
};

FILESYS proc_filesys = {
	(FILESYS *)0,
	0,
	proc_root,
	proc_lookup, nocreat, proc_getdev, proc_getxattr,
	proc_chattr, proc_chown, proc_chmode,
	nomkdir, proc_rmdir, proc_remove, proc_getname, proc_rename,
	proc_opendir, proc_readdir, proc_rewinddir, proc_closedir,
	proc_pathconf, proc_dfree,
	nowritelabel, proc_readlabel, nosymlink, noreadlink, nohardlink,
	nofscntl, nodskchng
};

long ARGS_ON_STACK 
proc_root(drv, fc)
	int drv;
	fcookie *fc;
{
	if (drv == PROCDRV) {
		fc->fs = &proc_filesys;
		fc->dev = drv;
		fc->index = 0L;
		return 0;
	}
	fc->fs = 0;
	return EINTRN;
}

static PROC *
name2proc(name)
	const char *name;
{
	const char *pstr;
	char c;
	int i;

	pstr = name;
	while ( (c = *name++) != 0) {
		if (c == '.')
			pstr = name;
	}
	if (!isdigit(*pstr) && *pstr != '-')
		return 0;
	i = (int)atol(pstr);
	if (i == -1)
		return curproc;
	else if (i == -2)
		i = curproc->ppid;
	return pid2proc(i);
}

static long ARGS_ON_STACK 
proc_lookup(dir, name, fc)
	fcookie *dir;
	const char *name;
	fcookie *fc;
{
	PROC *p;

	if (dir->index != 0) {
		DEBUG(("proc_lookup: bad directory"));
		return EPTHNF;
	}

/* special case: an empty name in a directory means that directory */
/* so does "." */
	if (!*name || (name[0] == '.' && name[1] == 0)) {
		*fc = *dir;
		return 0;
	}

/* another special case: ".." could be a mount point */
	if (!strcmp(name, "..")) {
		*fc = *dir;
		return EMOUNT;
	}

	if (0 == (p = name2proc(name))) {
		DEBUG(("proc_lookup: name not found"));
		return EFILNF;
	} else {
		fc->index = (long)p;
		fc->fs = &proc_filesys;
		fc->dev = PROC_RDEV_BASE | p->pid;
	}
	return 0;
}

static int p_attr[NUM_QUEUES] = {	/* attributes corresponding to queues */
	0,			/* "RUNNING" */
	0x01,			/* "READY" */
	0x20,			/* "WAITING" */
	0x21,			/* "IOBOUND" */
	0x22,			/* "ZOMBIE" */
	0x02,			/* "TSR" */
	0x24,			/* "STOPPED" */
	0x21			/* "SELECT" (same as IOBOUND) */
};

static long ARGS_ON_STACK 
proc_getxattr(fc, xattr)
	fcookie *fc;
	XATTR *xattr;
{
	PROC *p;
	extern int proctime, procdate;	/* see dosmem.c */

	xattr->blksize = 1;
	if (fc->index == 0) {
		/* the root directory */
		xattr->index = 0;
		xattr->dev = xattr->rdev = PROCDRV;
		xattr->nlink = 1;
		xattr->uid = xattr->gid = 0;
		xattr->size = xattr->nblocks = 0;
		xattr->mtime = xattr->atime = xattr->ctime = proctime;
		xattr->mdate = xattr->adate = xattr->cdate = procdate;
		xattr->mode = S_IFDIR | DEFAULT_DIRMODE;
		xattr->attr = FA_DIR;
		return 0;
	}

	p = (PROC *)fc->index;
	xattr->index = p->pid;
	xattr->dev = xattr->rdev = PROC_RDEV_BASE | p->pid;
	xattr->nlink = 1;
	xattr->uid = p->euid; xattr->gid = p->egid;
	xattr->size = xattr->nblocks = memused(p);
	xattr->mtime = xattr->ctime = xattr->atime = p->starttime;
	xattr->mdate = xattr->cdate = xattr->adate = p->startdate;
	xattr->mode = S_IFMEM | S_IRUSR | S_IWUSR;
	xattr->attr = p_attr[p->wait_q];
	return 0;
}

static long ARGS_ON_STACK 
proc_chattr(fc, attrib)
	fcookie *fc;
	int attrib;
{
	UNUSED(fc); UNUSED(attrib);

	return EACCDN;
}

static long ARGS_ON_STACK 
proc_chown(fc, uid, gid)
	fcookie *fc;
	int uid, gid;
{
	UNUSED(fc); UNUSED(uid); UNUSED(gid);
	return EINVFN;
}

static long ARGS_ON_STACK 
proc_chmode(fc, mode)
	fcookie *fc;
	unsigned mode;
{
	UNUSED(fc); UNUSED(mode);
	return EINVFN;
}

static long ARGS_ON_STACK 
proc_rmdir(dir, name)
	fcookie *dir;
	const char *name;
{
	UNUSED(dir); UNUSED(name);
	return EPTHNF;
}

static long ARGS_ON_STACK 
proc_remove(dir, name)
	fcookie *dir;
	const char *name;
{
	PROC *p;

	if (dir->index != 0)
		return EPTHNF;
	p = name2proc(name);
	if (!p)
		return EFILNF;

/* this check is necessary because the Fdelete code checks for
 * write permission on the directory, not on individual
 * files
 */
	if (curproc->euid && curproc->ruid != p->ruid) {
		DEBUG(("proc_remove: wrong user"));
		return EACCDN;
	}
	post_sig(p, SIGTERM);
	check_sigs();		/* it might have been us */
	return 0;
}

static long ARGS_ON_STACK 
proc_getname(root, dir, pathname, size)
	fcookie *root, *dir; char *pathname;
	int size;
{
	PROC *p;
	char buffer[20]; /* enough if proc names no longer than 8 chars */

	UNUSED(root);

	if (dir->index == 0)
		*buffer = 0;
	else {
		p = (PROC *)dir->index;
		ksprintf(buffer, "%s.03d", p->name, p->pid);
	}
	if (strlen(buffer) < size) {
		strcpy(pathname, buffer);
		return 0;
	}
	else
		return ERANGE;
}

static long ARGS_ON_STACK 
proc_rename(olddir, oldname, newdir, newname)
	fcookie *olddir;
	char *oldname;
	fcookie *newdir;
	const char *newname;
{
	PROC *p;
	int i;

	if (olddir->index != 0 || newdir->index != 0)
		return EPTHNF;
	if ((p = name2proc(oldname)) == 0)
		return EFILNF;

	oldname = p->name;
	for (i = 0; i < PNAMSIZ; i++) {
		if (*newname == 0 || *newname == '.') {
			*oldname = 0; break;
		}
		*oldname++ = *newname++;
	}
	return 0;
}

static long ARGS_ON_STACK 
proc_opendir(dirh, flags)
	DIR *dirh;
	int flags;
{
	UNUSED(flags);

	dirh->index = 0;
	return 0;
}

static long ARGS_ON_STACK 
proc_readdir(dirh, name, namelen, fc)
	DIR *dirh;
	char *name;
	int namelen;
	fcookie *fc;
{
	int i;
	int giveindex = (dirh->flags == 0);
	PROC *p;

	do {
		i = dirh->index++;
/* BUG: we shouldn't have the magic number "1000" for maximum proc pid */
		if (i >= 1000) {
			p = 0;
			break;
		}
		p = pid2proc(i);
	} while (!p);

	if (!p)
		return ENMFIL;

	fc->index = (long)p;
	fc->fs = &proc_filesys;
	fc->dev = PROC_RDEV_BASE | p->pid;

	if (giveindex) {
		namelen -= (int)sizeof(long);
		if (namelen <= 0) return ERANGE;
		*((long *)name) = (long)p->pid;
		name += sizeof(long);
	}
	if (namelen < strlen(p->name) + 5)
		return ENAMETOOLONG;

	ksprintf(name, "%s.%03d", p->name, p->pid);
	return 0;
}

static long ARGS_ON_STACK 
proc_rewinddir(dirh)
	DIR *dirh;
{
	dirh->index = 0;
	return 0;
}

static long ARGS_ON_STACK 
proc_closedir(dirh)
	DIR *dirh;
{
	UNUSED(dirh);
	return 0;
}
static long ARGS_ON_STACK 
proc_pathconf(dir, which)
	fcookie *dir;
	int which;
{
	UNUSED(dir);

	switch(which) {
	case -1:
		return DP_MAXREQ;
	case DP_IOPEN:
		return UNLIMITED;	/* no internal limit on open files */
	case DP_MAXLINKS:
		return 1;		/* we don't have hard links */
	case DP_PATHMAX:
		return PATH_MAX;	/* max. path length */
	case DP_NAMEMAX:
		return PNAMSIZ + 4;	/* max. length of individual name */
					/* the "+4" is for the pid: ".123" */
	case DP_ATOMIC:
		return UNLIMITED;	/* all writes are atomic */
	case DP_TRUNC:
		return DP_DOSTRUNC;	/* file names are truncated to 8.3 */
	case DP_CASE:
		return DP_CASEINSENS;	/* case preserved, but ignored */
	case DP_MODEATTR:
		return (0777L << 8)|
				DP_FT_DIR|DP_FT_MEM;
	case DP_XATTRFIELDS:
		return DP_INDEX|DP_DEV|DP_NLINK|DP_UID|DP_GID|DP_BLKSIZE|DP_SIZE|
				DP_NBLOCKS;
	default:
		return EINVFN;
	}
}

static long ARGS_ON_STACK 
proc_dfree(dir, buf)
	fcookie *dir;
	long *buf;
{
	long size;
/* "sector" size is the size of the smallest amount of memory that can be
   allocated. see mem.h for the definition of ROUND
 */
	long secsiz = ROUND(1);

	UNUSED(dir);

	size = tot_rsize(core, 0) + tot_rsize(alt, 0);
	*buf++ = size/secsiz;			/* number of free clusters */
	size = tot_rsize(core, 1) + tot_rsize(alt, 1);
	*buf++ = size/secsiz;			/* total number of clusters */
	*buf++ = secsiz;			/* sector size (bytes) */
	*buf = 1;				/* cluster size (in sectors) */
	return 0;
}

static DEVDRV * ARGS_ON_STACK 
proc_getdev(fc, devsp)
	fcookie *fc;
	long *devsp;
{
	PROC *p;

	p = (PROC *)fc->index;

	*devsp = (long)p;
	return &proc_device;
}

/*
 * PROC device driver
 */

/*
 * BUG: file locking and the O_SHMODE restrictions are not implemented
 * for processes
 */

static long ARGS_ON_STACK 
proc_open(f)
	FILEPTR *f;
{
	UNUSED(f);

	return 0;
}

static long ARGS_ON_STACK 
proc_write(f, buf, nbytes)
	FILEPTR *f; const char *buf; long nbytes;
{
	PROC *p = (PROC *)f->devinfo;
	char *where;
	long bytes_written = 0;
	int prot_hold;

	where = (char *)f->pos;

TRACE(("proc_write to pid %d: %ld bytes to %lx", p->pid, nbytes, where));

	prot_hold = mem_access_for(p, (ulong)where,nbytes);
	if (prot_hold == 0) {
	    DEBUG(("Can't Fwrite that memory: not all the same or not owner."));
	    return EACCDN;
	}
	if (prot_hold == 1) {
	    DEBUG(("Attempt to Fwrite memory crossing a managed boundary"));
	    return EACCDN;
	}

	bytes_written = nbytes;
	while (nbytes-- > 0) {
		*where++ = *buf++;
	}
	cpush((void *)f->pos, bytes_written);	/* flush cached data */

	/* MEMPROT: done with temp mapping (only call if temp'ed above) */
	if (prot_hold != -1) prot_temp((ulong)f->pos,bytes_written,prot_hold);

	f->pos += bytes_written;
	return bytes_written;
}

static long ARGS_ON_STACK 
proc_read(f, buf, nbytes)
	FILEPTR *f; char *buf; long nbytes;
{
	PROC *p = (PROC *)f->devinfo;
	char *where;
	long bytes_read = 0;
	int prot_hold;

	where = (char *)f->pos;

TRACE(("proc_read from pid %d: %ld bytes from %lx", p->pid, nbytes, where));

	prot_hold = mem_access_for(p, (ulong)where,nbytes);
	if (prot_hold == 0) {
	    DEBUG(("Can't Fread that memory: not all the same."));
	    return EACCDN;
	}
	if (prot_hold == 1) {
	    DEBUG(("Attempt to Fread memory crossing a managed boundary"));
	    return EACCDN;
	}

	bytes_read = nbytes;
	while (nbytes-- > 0) {
		*buf++ = *where++;
	}

	/* MEMPROT: done with temp mapping (only call if temp'ed above) */
	if (prot_hold != -1) prot_temp((ulong)f->pos,bytes_read,prot_hold);

	f->pos += bytes_read;
	return bytes_read;
}

/*
 * proc_ioctl: currently, the only IOCTL's available are:
 * PPROCADDR: get address of PROC structure's "interesting" bits
 * PCTXTSIZE: get the size of the CONTEXT structure
 * PBASEADDR: get address of process basepage
 * PSETFLAGS: set the memory allocation flags (e.g. to malloc from fastram)
 * PGETFLAGS: get the memory allocation flags
 * PTRACESFLAGS: set the process tracing flags
 * PTRACEGFLAGS: get the process tracing flags
 * PTRACEGO: restart the process (T1=0/T1=0)
 * PTRACEFLOW: restart the process (T1=0/T0=1)
 * PTRACESTEP: restart the process (T1=1/T0=0)
 * PTRACE11: restart the process (T1=1/T0=1)
 * PLOADINFO: get information about the process name and command line
 */

static long ARGS_ON_STACK 
proc_ioctl(f, mode, buf)
	FILEPTR *f; int mode; void *buf;
{
	PROC *p;
	extern long mcpu;	/* in main.c */
	short sr;

	p = (PROC *)f->devinfo;
	switch(mode) {
	case PPROCADDR:
		*((long *)buf) = (long)&p->magic;
		return 0;
	case PBASEADDR:
		if (p == rootproc)
			*((long *)buf) = (long)_base;
		else
			*((long *)buf) = (long)p->base;
		return 0;
	case PCTXTSIZE:
		*((long *)buf) = sizeof(CONTEXT);
		return 0;
	case PFSTAT:
	    {
		FILEPTR *pf;
		int pfd = (*(ushort *)buf);
		if (pfd < MIN_HANDLE || pfd >= MAX_OPEN ||
		    (pf = p->handle[pfd]) == 0)
			return EIHNDL;
		return (*pf->fc.fs->getxattr)(&pf->fc, (XATTR *)buf);
	    }
	case PSETFLAGS:
	    {
		int newflags = (ushort)(*(long *)buf);
		if ((newflags & F_OS_SPECIAL) &&
		    (!(p->memflags & F_OS_SPECIAL))) {
			/* you're making the process OS_SPECIAL */
			TRACE(("Fcntl OS_SPECIAL pid %d",p->pid));
			p->memflags = newflags;
			mem_prot_special(p);
		}
		/* note: only the low 16 bits are actually used */
		p->memflags = *((long *)buf);
		return 0;
	    }
	case PGETFLAGS:
		*((long *)buf) = p->memflags;
		return 0;
	case PTRACESFLAGS:
		if (p->ptracer == curproc || p->ptracer == 0) {
			p->ptraceflags = *(ushort *)buf;
			if (p->ptraceflags == 0) {
				p->ptracer = 0;
				p->ctxt[CURRENT].ptrace = 0;
				p->ctxt[SYSCALL].ptrace = 0;
		/* if the process is stopped, restart it */
				if (p->wait_q == STOP_Q) {
					p->sigpending &= ~STOPSIGS;
					post_sig(p, SIGCONT);
				}
			} else if (p == curproc) {
				p->ptracer = pid2proc(p->ppid);
			} else {
				p->ptracer = curproc;
			}
		} else {
			DEBUG(("proc_ioctl: process already being traced"));
			return EACCDN;
		}
		return 0;
	case PTRACEGFLAGS:
		if (p->ptracer == curproc) {
			*(ushort *)buf = p->ptraceflags;
			return 0;
		} else {
			return EACCDN;
		}
	case PTRACE11:
		return EINVFN;
	case PTRACEFLOW:
		if (mcpu < 20) {
			DEBUG(("proc_ioctl: wrong processor"));
			return EINVFN;
		}
		/* fall through */
	case PTRACEGO:
	case PTRACESTEP:
		if (!p->ptracer) {
			DEBUG(("proc_ioctl(PTRACE): process not being traced"));
			return EACCDN;
		}
		else if (p->wait_q != STOP_Q) {
			DEBUG(("proc_ioctl(PTRACE): process not stopped"));
			return EACCDN;
		}
		else if (p->wait_cond &&
		    (1L << ((p->wait_cond >> 8) & 0x1f)) & STOPSIGS) {
			DEBUG(("proc_ioctl(PTRACE): process stopped by job control"));
			return EACCDN;
		}
		if (buf && *(ushort *)buf >= NSIG) {
			DEBUG(("proc_ioctl(PTRACE): illegal signal number"));
			return ERANGE;
		}
		p->ctxt[SYSCALL].sr &= 0x3fff;	/* clear both trace bits */
		p->ctxt[SYSCALL].sr |= (mode - PTRACEGO) << 14;
		/* Discard the saved frame */
		p->ctxt[SYSCALL].sfmt = 0;
		p->sigpending = 0;
		if (buf && *(ushort *)buf != 0) {
TRACE(("PTRACEGO: sending signal %d to pid %d", *(ushort *)buf, p->pid));
			post_sig(p, *(ushort *)buf);

/* another SIGNULL hack... within check_sigs() we watch for a pending
 * SIGNULL, if we see this then we allow delivery of a signal to the
 * process, rather than telling the parent.
 */
			p->sigpending |= 1L;
		} else {
TRACE(("PTRACEGO: no signal"));
		}
/* wake the process up */
		sr = spl7();
		rm_q(p->wait_q, p);
		add_q(READY_Q, p);
		spl(sr);
		return 0;
	/* jr: PLOADINFO returns information about params passed to Pexec */
	case PLOADINFO:
		{
			struct ploadinfo *pl = buf;

			if (!p->fname[0]) return EFILNF;
			strncpy (pl->cmdlin, p->cmdlin, 128);
			if (strlen (p->fname) <= pl->fnamelen)
				strcpy (pl->fname, p->fname);
			else
				return ENAMETOOLONG;
		}
		return 0;

	case FIONREAD:
	case FIONWRITE:
		*((long *)buf) = 1L;	/* we're always ready for i/o */
		return 0;
	case FIOEXCEPT:
		*((long *)buf) = 0L;
		return 0;
	default:
		DEBUG(("procfs: bad Fcntl command"));
	}
	return EINVFN;
}

static long ARGS_ON_STACK 
proc_lseek(f, where, whence)
	FILEPTR *f; long where; int whence;
{
	switch(whence) {
	case 0:
	case 2:
		f->pos = where;
		break;
	case 1:
		f->pos += where;
		break;
	default:
		return EINVFN;
	}
	return f->pos;
}

static long ARGS_ON_STACK 
proc_datime(f, timeptr, rwflag)
	FILEPTR *f;
	short *timeptr;
	int rwflag;
{
	PROC *p;

	p = (PROC *)f->devinfo;
	if (rwflag) {
		return EACCDN;
	}
	else {
		*timeptr++ = p->starttime;
		*timeptr = p->startdate;
	}
	return 0;
}

static long ARGS_ON_STACK 
proc_close(f, pid)
	FILEPTR *f;
	int pid;
{
	UNUSED(f); UNUSED(pid);
	return 0;
}

static long ARGS_ON_STACK 
proc_readlabel(dir, name, namelen)
	fcookie *dir;
	char *name;
	int namelen;
{
	UNUSED(dir);

	strncpy(name, "Processes", namelen-1);
	return (strlen("Processes") < namelen) ? 0 : ENAMETOOLONG;
}
