/*
Copyright 1992,1993 Eric R. Smith.
Copyright 1993,1994 Atari Corporation.
All rights reserved.
 */

/* Shared memory file system */

#include "mint.h"


static long	ARGS_ON_STACK shm_root	P_((int drv, fcookie *fc));
static long	ARGS_ON_STACK shm_creat	P_((fcookie *dir, const char *name, unsigned mode,
				    int attrib, fcookie *fc));
static long	ARGS_ON_STACK shm_lookup	P_((fcookie *dir, const char *name, fcookie *fc));
static long	ARGS_ON_STACK shm_getxattr	P_((fcookie *fc, XATTR *xattr));
static long	ARGS_ON_STACK shm_chattr	P_((fcookie *fc, int attrib));
static long	ARGS_ON_STACK shm_chown	P_((fcookie *fc, int uid, int gid));
static long	ARGS_ON_STACK shm_chmode	P_((fcookie *fc, unsigned mode));
static long	ARGS_ON_STACK shm_rmdir	P_((fcookie *dir, const char *name));
static long	ARGS_ON_STACK shm_remove	P_((fcookie *dir, const char *name));
static long	ARGS_ON_STACK shm_getname	P_((fcookie *root, fcookie *dir,
						    char *pathname, int size));
static long	ARGS_ON_STACK shm_rename	P_((fcookie *olddir, char *oldname,
				    fcookie *newdir, const char *newname));
static long	ARGS_ON_STACK shm_opendir	P_((DIR *dirh, int flags));
static long	ARGS_ON_STACK shm_readdir	P_((DIR *dirh, char *nm, int nmlen, fcookie *));
static long	ARGS_ON_STACK shm_rewinddir	P_((DIR *dirh));
static long	ARGS_ON_STACK shm_closedir	P_((DIR *dirh));
static long	ARGS_ON_STACK shm_pathconf	P_((fcookie *dir, int which));
static long	ARGS_ON_STACK shm_dfree	P_((fcookie *dir, long *buf));
static DEVDRV *	ARGS_ON_STACK shm_getdev	P_((fcookie *fc, long *devsp));

static long	ARGS_ON_STACK shm_open	P_((FILEPTR *f));
static long	ARGS_ON_STACK shm_write	P_((FILEPTR *f, const char *buf, long bytes));
static long	ARGS_ON_STACK shm_read	P_((FILEPTR *f, char *buf, long bytes));
static long	ARGS_ON_STACK shm_lseek	P_((FILEPTR *f, long where, int whence));
static long	ARGS_ON_STACK shm_ioctl	P_((FILEPTR *f, int mode, void *buf));
static long	ARGS_ON_STACK shm_datime	P_((FILEPTR *f, short *time, int rwflag));
static long	ARGS_ON_STACK shm_close	P_((FILEPTR *f, int pid));

/* dummy routines from biosfs.c */
extern long	ARGS_ON_STACK null_select	P_((FILEPTR *f, long p, int mode));
extern void	ARGS_ON_STACK null_unselect	P_((FILEPTR *f, long p, int mode));

static short shmtime, shmdate;

#define SHMNAME_MAX 15

typedef struct shmfile {
	struct shmfile *next;
	char filename[SHMNAME_MAX+1];
	int uid, gid;
	short time, date;
	unsigned mode;
	int inuse;
	MEMREGION *reg;
} SHMFILE;

SHMFILE *shmroot = 0;

DEVDRV shm_device = {
	shm_open, shm_write, shm_read, shm_lseek, shm_ioctl, shm_datime,
	shm_close, null_select, null_unselect
};

FILESYS shm_filesys = {
	(FILESYS *)0,
	FS_LONGPATH,
	shm_root,
	shm_lookup, shm_creat, shm_getdev, shm_getxattr,
	shm_chattr, shm_chown, shm_chmode,
	nomkdir, shm_rmdir, shm_remove, shm_getname, shm_rename,
	shm_opendir, shm_readdir, shm_rewinddir, shm_closedir,
	shm_pathconf, shm_dfree,
	nowritelabel, noreadlabel, nosymlink, noreadlink, nohardlink,
	nofscntl, nodskchng
};

long ARGS_ON_STACK 
shm_root(drv, fc)
	int drv;
	fcookie *fc;
{
	if ((unsigned)drv == SHMDRV) {
		fc->fs = &shm_filesys;
		fc->dev = drv;
		fc->index = 0L;
		return 0;
	}
	fc->fs = 0;
	return EINTRN;
}

static long ARGS_ON_STACK 
shm_lookup(dir, name, fc)
	fcookie *dir;
	const char *name;
	fcookie *fc;
{
	SHMFILE *s;

	if (dir->index != 0) {
		DEBUG(("shm_lookup: bad directory"));
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

	for (s = shmroot; s; s = s->next) {
		if (!stricmp(s->filename,name))
			break;
	}

	if (!s) {
		DEBUG(("shm_lookup: name not found"));
		return EFILNF;
	} else {
		fc->index = (long)s;
		fc->fs = &shm_filesys;
		fc->dev = SHMDRV;
	}
	return 0;
}

static long ARGS_ON_STACK 
shm_getxattr(fc, xattr)
	fcookie *fc;
	XATTR *xattr;
{
	SHMFILE *s;

	xattr->blksize = 1;
	if (fc->index == 0) {
		/* the root directory */
		xattr->index = 0;
		xattr->dev = xattr->rdev = SHMDRV;
		xattr->nlink = 1;
		xattr->uid = xattr->gid = 0;
		xattr->size = xattr->nblocks = 0;
		xattr->mtime = xattr->atime = xattr->ctime = shmtime;
		xattr->mdate = xattr->adate = xattr->cdate = shmdate;
		xattr->mode = S_IFDIR | DEFAULT_DIRMODE;
		xattr->attr = FA_DIR;
		return 0;
	}

	s = (SHMFILE *)fc->index;
	xattr->index = (long) s;
	xattr->dev = SHMDRV;
	xattr->rdev = PROC_RDEV_BASE | 0;
	xattr->uid = s->uid; xattr->gid = s->gid;
	if (s->reg) {
		xattr->size = xattr->nblocks = s->reg->len;
		xattr->nlink = s->reg->links + 1;
	 } else {
		xattr->size = xattr->nblocks = 0;
		xattr->nlink = 1;
	}
	xattr->mtime = xattr->ctime = xattr->atime = s->time;
	xattr->mdate = xattr->cdate = xattr->adate = s->date;
	xattr->mode = s->mode;
	xattr->attr = (s->mode & (S_IWUSR|S_IWGRP|S_IWOTH)) ? 0 : 
			FA_RDONLY;
	return 0;
}

static long ARGS_ON_STACK 
shm_chattr(fc, attrib)
	fcookie *fc;
	int attrib;
{
	SHMFILE *s;

	s = (SHMFILE *)fc->index;
	if (!s) return EACCDN;

	if (attrib & FA_RDONLY) {
		s->mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
	} else if ( !(s->mode & (S_IWUSR|S_IWGRP|S_IWOTH)) ) {
		s->mode |= (S_IWUSR|S_IWGRP|S_IWOTH);
	}
	return 0;
}

static long ARGS_ON_STACK 
shm_chown(fc, uid, gid)
	fcookie *fc;
	int uid, gid;
{
	SHMFILE *s;

	s = (SHMFILE *)fc->index;
	if (!s)
		return EACCDN;
	s->uid = uid;
	s->gid = gid;
	return 0;
}

static long ARGS_ON_STACK 
shm_chmode(fc, mode)
	fcookie *fc;
	unsigned mode;
{
	SHMFILE *s;

	s = (SHMFILE *)fc->index;
	if (!s)
		return EINVFN;
	s->mode = mode;
	return 0;
}

static long ARGS_ON_STACK 
shm_rmdir(dir, name)
	fcookie *dir;
	const char *name;
{
	UNUSED(dir); UNUSED(name);

	return EPTHNF;
}

static long ARGS_ON_STACK 
shm_remove(dir, name)
	fcookie *dir;
	const char *name;
{
	SHMFILE *s, **old;

	if (dir->index != 0)
		return EPTHNF;

	old = &shmroot;
	for (s = shmroot; s; s = s->next) {
		if (!stricmp(s->filename, name))
			break;
		old = &s->next;
	}
	if (!s)
		return EFILNF;
	if (s->inuse)
		return EACCDN;
	*old = s->next;

	s->reg->links--;
	if (s->reg->links <= 0) {
		free_region(s->reg);
	}
	kfree(s);
	shmtime = timestamp;
	shmdate = datestamp;
	return 0;
}

static long ARGS_ON_STACK 
shm_getname(root, dir, pathname, size)
	fcookie *root, *dir; char *pathname;
	int size;
{
	UNUSED(root); UNUSED(dir);

/* BUG: 'size' should be used in a more meaningful way */
	if (size <= 0) return ERANGE;
	*pathname = 0;
	return 0;
}

static long ARGS_ON_STACK 
shm_rename(olddir, oldname, newdir, newname)
	fcookie *olddir;
	char *oldname;
	fcookie *newdir;
	const char *newname;
{
	SHMFILE *s;

	if (olddir->index != 0 || newdir->index != 0)
		return EPTHNF;

/* verify that "newname" doesn't exist */
	for (s = shmroot; s; s = s->next)
		if (!stricmp(s->filename, newname))
			return EACCDN;

	for (s = shmroot; s; s = s->next)
		if (!stricmp(s->filename, oldname))
			break;
	if (!s)
		return EFILNF;

	strncpy(s->filename, newname, SHMNAME_MAX);
	shmtime = timestamp;
	shmdate = datestamp;
	return 0;
}

static long ARGS_ON_STACK 
shm_opendir(dirh, flags)
	DIR *dirh;
	int flags;
{
	UNUSED(flags);

	dirh->index = 0;
	return 0;
}

static long ARGS_ON_STACK 
shm_readdir(dirh, name, namelen, fc)
	DIR *dirh;
	char *name;
	int namelen;
	fcookie *fc;
{
	int i;
	int giveindex = (dirh->flags == 0);
	SHMFILE *s;

	s = shmroot;
	i = dirh->index++;
	while (i > 0 && s != 0) {
		s = s->next;
		--i;
	}
	if (!s)
		return ENMFIL;

	fc->index = (long)s;
	fc->fs = &shm_filesys;
	fc->dev = SHMDRV;

	if (giveindex) {
		namelen -= (int)sizeof(long);
		if (namelen <= 0) return ERANGE;
		*((long *)name) = (long)s;
		name += sizeof(long);
	}
	if (namelen < strlen(s->filename))
		return ENAMETOOLONG;
	strcpy(name, s->filename);
	return 0;
}

static long ARGS_ON_STACK 
shm_rewinddir(dirh)
	DIR *dirh;
{
	dirh->index = 0;
	return 0;
}

static long ARGS_ON_STACK 
shm_closedir(dirh)
	DIR *dirh;
{
	UNUSED(dirh);
	return 0;
}

static long ARGS_ON_STACK 
shm_pathconf(dir, which)
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
		return SHMNAME_MAX;	/* max. length of individual name */
	case DP_ATOMIC:
		return UNLIMITED;	/* all writes are atomic */
	case DP_TRUNC:
		return DP_AUTOTRUNC;	/* file names are truncated */
	case DP_CASE:
		return DP_CASEINSENS;	/* case preserved, but ignored */
	case DP_MODEATTR:
		return (0777L << 8)|
				DP_FT_DIR|DP_FT_MEM;
	case DP_XATTRFIELDS:
		return DP_INDEX|DP_DEV|DP_NLINK|DP_UID|DP_GID|DP_BLKSIZE|DP_SIZE|
				DP_NBLOCKS|DP_MTIME;
	default:
		return EINVFN;
	}
}

static long ARGS_ON_STACK 
shm_dfree(dir, buf)
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
shm_getdev(fc, devsp)
	fcookie *fc;
	long *devsp;
{
	SHMFILE *s;

	s = (SHMFILE *)fc->index;

	*devsp = (long)s;
	return &shm_device;
}

/*
 * create a shared memory region
 */

static long ARGS_ON_STACK 
shm_creat(dir, name, mode, attrib, fc)
	fcookie *dir;
	const char *name;
	unsigned mode;
	int attrib;
	fcookie *fc;
{
	SHMFILE *s;

	UNUSED(attrib);
/*
 * see if the name already exists
 */
	for (s = shmroot; s; s = s->next) {
		if (!stricmp(s->filename, name)) {
			DEBUG(("shm_creat: file exists"));
			return EACCDN;
		}
	}

	s = (SHMFILE *)kmalloc(SIZEOF(SHMFILE));
	if (!s)
		return ENSMEM;

	s->inuse = 0;
	strncpy(s->filename, name, SHMNAME_MAX);
	s->filename[SHMNAME_MAX] = 0;
	s->uid = curproc->euid;
	s->gid = curproc->egid;
	s->mode = mode;
	s->next = shmroot;
	s->reg = 0;
	s->time = shmtime = timestamp;
	s->date = shmdate = datestamp;
	shmroot = s;

	fc->fs = &shm_filesys;
	fc->index = (long)s;
	fc->dev = dir->dev;

	return 0;
}

/*
 * Shared memory device driver
 */

/*
 * BUG: file locking and the O_SHMODE restrictions are not implemented
 * for shared memory
 */

static long ARGS_ON_STACK 
shm_open(f)
	FILEPTR *f;
{
	SHMFILE *s;

	s = (SHMFILE *)f->devinfo;
	s->inuse++;
	return 0;
}

static long ARGS_ON_STACK 
shm_write(f, buf, nbytes)
	FILEPTR *f; const char *buf; long nbytes;
{
	SHMFILE *s;
	char *where;
	long bytes_written = 0;

	s = (SHMFILE *)f->devinfo;
	if (!s->reg)
		return 0;

	if (nbytes + f->pos > s->reg->len)
		nbytes = s->reg->len - f->pos;

	where = (char *)s->reg->loc + f->pos;

/* BUG: memory read/writes should check for valid addresses */

TRACE(("shm_write: %ld bytes to %lx", nbytes, where));

	while (nbytes-- > 0) {
		*where++ = *buf++;
		bytes_written++;
	}
	f->pos += bytes_written;
	s->time = timestamp;
	s->date = datestamp;
	return bytes_written;
}

static long ARGS_ON_STACK 
shm_read(f, buf, nbytes)
	FILEPTR *f; char *buf; long nbytes;
{
	SHMFILE *s;
	char *where;
	long bytes_read = 0;

	s = (SHMFILE *)f->devinfo;
	if (!(s->reg))
		return 0;

	if (nbytes + f->pos > s->reg->len)
		nbytes = s->reg->len - f->pos;

	where = (char *)s->reg->loc + f->pos;

TRACE(("shm_read: %ld bytes from %lx", nbytes, where));

	while (nbytes-- > 0) {
		*buf++ = *where++;
		bytes_read++;
	}
	f->pos += bytes_read;
	return bytes_read;
}

/*
 * shm_ioctl: currently, the only IOCTL's available are:
 * SHMSETBLK:  set the address of the shared memory file. This
 *             call may only be made once per region, and then only
 *	       if the region is open for writing.
 * SHMGETBLK:  get the address of the shared memory region. This
 *             call fails (returns 0) if SHMSETBLK has not been
 *             called yet for this shared memory file.
 */

static long ARGS_ON_STACK 
shm_ioctl(f, mode, buf)
	FILEPTR *f; int mode; void *buf;
{
	SHMFILE *s;
	MEMREGION *m;
	int i;
	long r;

	s = (SHMFILE *)f->devinfo;
	switch(mode) {
	case SHMSETBLK:
		if (s->reg) {
			DEBUG(("Fcntl: SHMSETBLK already performed for %s",
				s->filename));
			return ERANGE;
		}
		if ((f->flags & O_RWMODE) == O_RDONLY) {
			DEBUG(("Fcntl: SHMSETBLK: %s was opened read-only",
				s->filename));
			return EACCDN;
		}
	/* find the memory region to be attached */
		m = 0;
		for (i = curproc->num_reg - 1; i >= 0; i--) {
			if (curproc->addr[i] == (virtaddr)buf) {
				m = curproc->mem[i];
				break;
			}
		}
		if (!m || !buf) {
			DEBUG(("Fcntl: SHMSETBLK: bad address %lx", buf));
			return EIMBA;
		}
		m->links++;
		s->reg = m;
		return 0;
	case SHMGETBLK:
		if ((m = s->reg) == 0) {
			DEBUG(("Fcntl: no address for SHMGETBLK"));
			return 0;
		}
	/* check for memory limits */
		if (curproc->maxmem) {
			if (m->len > curproc->maxmem - memused(curproc)) {
				DEBUG(("Fcntl: SHMGETBLK would violate memory limits"));
				return 0;
			}
		}
		return (long)attach_region(curproc, m);
	case FIONREAD:
	case FIONWRITE:
		if (s->reg == 0) {
			r = 0;
		} else {
			r = s->reg->len - f->pos;
			if (r < 0) r = 0;
		}
		*((long *)buf) = r;
		return 0;
	default:
		DEBUG(("shmfs: bad Fcntl command"));
	}
	return EINVFN;
}

static long ARGS_ON_STACK 
shm_lseek(f, where, whence)
	FILEPTR *f; long where; int whence;
{
	long newpos, maxpos;
	SHMFILE *s;

	s = (SHMFILE *)f->devinfo;

	if (s->reg)
		maxpos = s->reg->len;
	else
		maxpos = 0;

	switch(whence) {
	case 0:
		newpos = where;
		break;
	case 1:
		newpos = f->pos + where;
		break;
	case 2:
		newpos = maxpos + where;
		break;
	default:
		return EINVFN;
	}

	if (newpos < 0 || newpos > maxpos)
		return ERANGE;

	f->pos = newpos;
	return newpos;
}

static long ARGS_ON_STACK 
shm_datime(f, timeptr, rwflag)
	FILEPTR *f;
	short *timeptr;
	int rwflag;
{
	SHMFILE *s;

	s = (SHMFILE *)f->devinfo;
	if (rwflag) {
		s->time = *timeptr++;
		s->date = *timeptr;
	} else {
		*timeptr++ = s->time;
		*timeptr = s->date;
	}
	return 0;
}

static long ARGS_ON_STACK 
shm_close(f, pid)
	FILEPTR *f;
	int pid;
{
	SHMFILE *s;

	UNUSED(pid);
	if (f->links <= 0) {
		s = (SHMFILE *)f->devinfo;
		s->inuse--;
	}
	return 0;
}
