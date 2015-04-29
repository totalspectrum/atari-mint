/*
Copyright 1991,1992 Eric R. Smith.
Copyright 1993,1994 Atari Corporation.
All rights reserved.
 */

/* simple biosfs.c */

#include "mint.h"

extern struct kerinfo kernelinfo;	/* see main.c */

static long	ARGS_ON_STACK bios_root	P_((int drv, fcookie *fc));
static long	ARGS_ON_STACK bios_lookup	P_((fcookie *dir, const char *name, fcookie *fc));
static long	ARGS_ON_STACK bios_getxattr	P_((fcookie *fc, XATTR *xattr));
static long	ARGS_ON_STACK bios_chattr	P_((fcookie *fc, int attrib));
static long	ARGS_ON_STACK bios_chown	P_((fcookie *fc, int uid, int gid));
static long	ARGS_ON_STACK bios_chmode	P_((fcookie *fc, unsigned mode));
static long	ARGS_ON_STACK bios_rmdir	P_((fcookie *dir, const char *name));
static long	ARGS_ON_STACK bios_remove	P_((fcookie *dir, const char *name));
static long	ARGS_ON_STACK bios_getname	P_((fcookie *root, fcookie *dir, char *pathname, int size));
static long	ARGS_ON_STACK bios_rename	P_((fcookie *olddir, char *oldname,
				    fcookie *newdir, const char *newname));
static long	ARGS_ON_STACK bios_opendir	P_((DIR *dirh, int flags));
static long	ARGS_ON_STACK bios_readdir	P_((DIR *dirh, char *nm, int nmlen, fcookie *fc));
static long	ARGS_ON_STACK bios_rewinddir	P_((DIR *dirh));
static long	ARGS_ON_STACK bios_closedir	P_((DIR *dirh));
static long	ARGS_ON_STACK bios_pathconf	P_((fcookie *dir, int which));
static long	ARGS_ON_STACK bios_dfree	P_((fcookie *dir, long *buf));
static DEVDRV *	ARGS_ON_STACK bios_getdev	P_((fcookie *fc, long *devspecial));
static long	ARGS_ON_STACK bios_fscntl	P_((fcookie *, const char *, int, long));
static long	ARGS_ON_STACK bios_symlink	P_((fcookie *, const char *, const char *));
static long	ARGS_ON_STACK bios_readlink	P_((fcookie *, char *, int));

static long	ARGS_ON_STACK bios_topen	P_((FILEPTR *f));
static long	ARGS_ON_STACK bios_twrite	P_((FILEPTR *f, const char *buf, long bytes));
static long	ARGS_ON_STACK bios_tread	P_((FILEPTR *f, char *buf, long bytes));
static long	ARGS_ON_STACK bios_writeb	P_((FILEPTR *f, const char *buf, long bytes));
static long	ARGS_ON_STACK bios_readb	P_((FILEPTR *f, char *buf, long bytes));
static long	ARGS_ON_STACK bios_nwrite	P_((FILEPTR *f, const char *buf, long bytes));
static long	ARGS_ON_STACK bios_nread	P_((FILEPTR *f, char *buf, long bytes));
static long	ARGS_ON_STACK bios_ioctl	P_((FILEPTR *f, int mode, void *buf));
static long	ARGS_ON_STACK bios_select	P_((FILEPTR *f, long p, int mode));
static void	ARGS_ON_STACK bios_unselect	P_((FILEPTR *f, long p, int mode));
static long	ARGS_ON_STACK bios_tseek	P_((FILEPTR *f, long where, int whence));
static long	ARGS_ON_STACK bios_close	P_((FILEPTR *f, int pid));

long	ARGS_ON_STACK null_open	P_((FILEPTR *f));
long	ARGS_ON_STACK null_write	P_((FILEPTR *f, const char *buf, long bytes));
long	ARGS_ON_STACK null_read	P_((FILEPTR *f, char *buf, long bytes));
long	ARGS_ON_STACK null_lseek	P_((FILEPTR *f, long where, int whence));
long	ARGS_ON_STACK null_ioctl	P_((FILEPTR *f, int mode, void *buf));
long	ARGS_ON_STACK null_datime	P_((FILEPTR *f, short *time, int rwflag));
long	ARGS_ON_STACK null_close	P_((FILEPTR *f, int pid));
long	ARGS_ON_STACK null_select	P_((FILEPTR *f, long p, int mode));
void	ARGS_ON_STACK null_unselect	P_((FILEPTR *f, long p, int mode));

static long ARGS_ON_STACK mouse_open	P_((FILEPTR *f));
static long ARGS_ON_STACK mouse_read	P_((FILEPTR *f, char *buf, long nbytes));
static long ARGS_ON_STACK mouse_ioctl P_((FILEPTR *f, int mode, void *buf));
static long ARGS_ON_STACK mouse_close P_((FILEPTR *f, int pid));
static long ARGS_ON_STACK mouse_select P_((FILEPTR *f, long p, int mode));
static void ARGS_ON_STACK mouse_unselect P_((FILEPTR *f, long p, int mode));

/* device driver for BIOS terminals */

DEVDRV bios_tdevice = {
	bios_topen, bios_twrite, bios_tread, bios_tseek, bios_ioctl,
	null_datime, bios_close, bios_select, bios_unselect,
	bios_writeb, bios_readb
};

/* device driver for BIOS devices that are not terminals */

DEVDRV bios_ndevice = {
	null_open, bios_nwrite, bios_nread, null_lseek, bios_ioctl,
	null_datime, bios_close, bios_select, bios_unselect
};

DEVDRV null_device = {
	null_open, null_write, null_read, null_lseek, null_ioctl,
	null_datime, null_close, null_select, null_unselect
};

DEVDRV mouse_device = {
	mouse_open, null_write, mouse_read, null_lseek, mouse_ioctl,
	null_datime, mouse_close, mouse_select, mouse_unselect
};

/* this special driver is checked for in dosfile.c, and indicates that
 * a dup operation is actually wanted rather than an open
 */
DEVDRV fakedev;

#ifdef FASTTEXT
extern DEVDRV screen_device;	/* see fasttext.c */
#endif

FILESYS bios_filesys = {
	(FILESYS *)0,
	FS_LONGPATH,
	bios_root,
	bios_lookup, nocreat, bios_getdev, bios_getxattr,
	bios_chattr, bios_chown, bios_chmode,
	nomkdir, bios_rmdir, bios_remove, bios_getname, bios_rename,
	bios_opendir, bios_readdir, bios_rewinddir, bios_closedir,
	bios_pathconf, bios_dfree, nowritelabel, noreadlabel,
	bios_symlink, bios_readlink, nohardlink, bios_fscntl, nodskchng
};


struct tty con_tty, aux_tty, midi_tty;
struct tty sccb_tty, scca_tty, ttmfp_tty;

struct bios_file BDEV[] = {

/* "real" bios devices present on all machines */
	{"centr", &bios_ndevice, 0, 0, 0, 0},
	{"console", &bios_tdevice, 2, O_TTY, &con_tty, 0},
	{"midi", &bios_tdevice, 3, O_TTY, &midi_tty, 0},
	{"kbd", &bios_ndevice, 4, 0, 0, 0},
/* devices that duplicate handles */
	{"prn", &fakedev, -3, 0, 0, 0}, /* handle -3 (printer) */
	{"aux", &fakedev, -2, 0, 0, 0}, /* handle -2 (aux. terminal) */
	{"con", &fakedev, -1, 0, 0, 0}, /* handle -1 (control terminal) */
	{"tty", &fakedev, -1, 0, 0, 0}, /* the Unix name for it */
	{"stdin", &fakedev, 0, 0, 0, 0},  /* handle 0 (stdin) */
	{"stdout", &fakedev, 1, 0, 0, 0}, /* handle 1 (stdout) */
	{"stderr", &fakedev, 2, 0, 0, 0}, /* handle 2 (stderr) */
	{"fd", &fakedev, S_IFDIR, 0, 0, 0}, /* file descriptor directory */

/* other miscellaneous devices */
	{"mouse", &mouse_device, 0, 0, 0, 0},
	{"null", &null_device, 0, 0, 0, 0},

#ifdef FASTTEXT
/* alternate console driver */
	{"fasttext", &screen_device, 2, O_TTY, &con_tty, 0},
#endif

/* serial port things *must* come last, because not all of these
 * are present on all machines (except for modem1, which does however
 * have a different device number on TTs and STs)
 */
	{"modem1", &bios_tdevice, 6, O_TTY, &aux_tty, 0},
	{"modem2", &bios_tdevice, 7, O_TTY, &sccb_tty, 0},
	{"serial1", &bios_tdevice, 8, O_TTY, &ttmfp_tty, 0},
	{"serial2", &bios_tdevice, 9, O_TTY, &scca_tty, 0},
	{"", 0, 0, 0, 0, 0}
};

#define xconstat ((long *)0x51eL)
#define xconin 	((long *)0x53eL)
#define xcostat ((long *)0x55eL)
#define xconout	((long *)0x57eL)

extern BCONMAP2_T *bconmap2;		/* bconmap struct */
#define MAPTAB (bconmap2->maptab)

#define MAXBAUD 16

/* keep these sorted in descending order */
static long baudmap[MAXBAUD] = {
19200L, 9600L, 4800L, 3600L, 2400L, 2000L, 1800L, 1200L,
600L, 300L, 200L, 150L, 134L, 110L, 75L, 50L
};

/* set/reset bits in SCC w5 */
INLINE static void scc_set5 P_((volatile char *control, int setp,
	unsigned bits, IOREC_T *iorec));

INLINE static void scc_set5 (control, setp, bits, iorec)
volatile char *control;
int setp;
unsigned bits;
IOREC_T *iorec;
{
	volatile char dummy;

	short sr = spl7();

#if 1
/* sanity check: if the w5 copy at offset 1d has bit 3 off something is wrong */
	if (!(((char *) iorec)[0x1d] & 8)) {
		spl(sr);
		ALERT ("scc_set5: iorec %lx w5 copy has sender enable bit off, w5 not changed", iorec);
		return;
	}
#endif
	dummy = *((volatile char *) 0xfffffa01L);
	*control = 5;
	dummy = *((volatile char *) 0xfffffa01L);
	if (setp)
		*control = (((char *) iorec)[0x1d] |= bits);
	else
		*control = (((char *) iorec)[0x1d] &= ~bits);
	spl(sr);
#ifdef __TURBOC__
	setp = dummy; /* jr: get rid of warning regarding dummy */
#endif
}

#define	MAX_BTTY	4	/* 4 bios_tty structs */

/* find bios_tty struct for a FILEPTR
 */
#define BTTY(f) ((((struct tty *)(f)->devinfo) == &aux_tty) ? bttys : \
		 ((has_bconmap && (unsigned)(f)->fc.aux-6 < btty_max) ? \
		    bttys+(f)->fc.aux-6 : 0))

struct bios_tty bttys[MAX_BTTY], midi_btty;
short	btty_max;

/* RSVF cookie value (read in main.c) */
long rsvf;

/* try to get a fd for a BIOS tty to pass some ioctls to... */

INLINE static short
rsvf_open(bdev)
	int bdev;
{
	long ret = EUNDEV;
	struct rsvfdev {
		union {
			char *name;
			struct rsvfdev *next;
		} f;
		char flags, unused1, bdev, unused2;
	} *r = (struct rsvfdev *)rsvf;

	while (r) {
		if (r->flags >= 0) {
			r = r->f.next;
			continue;
		}
		if ((r->flags & 0xe0) == 0xe0 && r->bdev == bdev) {
			char rname[0x80] = "u:\\dev\\";

			strncpy (rname + sizeof "u:\\dev\\" - 1, r->f.name,
				sizeof rname - sizeof "u:\\dev\\");
			ret = Fopen (rname, O_RDWR);
			if (ret < MIN_HANDLE || ret > 0x8000) {
				ALERT ("rsvf_open(%d): Fopen %s returned %lx",
					bdev, rname, ret);
				return EUNDEV;
			}
			break;
		}
		++r;
	}
	return ret;
}

INLINE static long
rsvf_close(f)
	int f;
{
	long ret = EIHNDL;
	if (f != EUNDEV) {
		ret = Fclose (f);
		if (ret)
			ALERT ("rsvf_close(%d): Fclose %x returned %lx", f, ret);
	}
	return ret;
}

INLINE long
rsvf_ioctl(f, arg, mode)
	int f;
	void *arg;
	int mode;
{
	if (f == EUNDEV)
		return EINVFN;
	TRACE(("rsvf_ioctl: passing ioctl %x (tosfd=0x%x)", mode, f));
	/* is there a more direct way than this? */
	return Fcntl (f, (long)arg, mode);
}

extern int tosvers;	/* from main.c */

/* Does the fcookie fc refer to the \dev\fd directory? */
#define IS_FD_DIR(fc) ((fc)->aux == S_IFDIR)
/* Does the fcookie fc refer to a file in the \dev\fd directory? */
#define IS_FD_ENTRY(fc) ((fc)->index > 0 && (fc)->index <= MAX_OPEN-MIN_HANDLE)

struct bios_file *broot, *bdevlast;

/* a file pointer for BIOS device 1, provided only for insurance
 * in case a Bconmap happens and we can't allocate a new FILEPTR;
 * in most cases, we'll want to build a FILEPTR in the usual
 * way.
 */

FILEPTR *defaultaux;

/* ts: a xattr field used for the root directory, 'cause there's no
 * bios_file structure for it.
 */
XATTR rxattr;
XATTR fdxattr;

/* ts: a small utility function to set up a xattr structure
 */

static void set_xattr P_((XATTR *xp, ushort mode, int rdev));

void set_xattr(xp, mode, rdev)
	XATTR *xp;
	ushort mode;
	int rdev;
{
	xp->mode = mode;
	xp->index = 0L;
	xp->dev = BIOSDRV;
	xp->rdev = rdev;
	xp->nlink = 1;
	xp->uid = curproc->euid;
	xp->gid = curproc->egid;
	xp->size = 0L;
	xp->blksize = 1024L;
	xp->nblocks = 0L;

	xp->mtime = xp->atime = xp->ctime = timestamp;
	xp->mdate = xp->adate = xp->cdate = datestamp;

/* root directory only */
	if ((mode & S_IFMT) == S_IFDIR)
		xp->attr = FA_DIR;
	else
		xp->attr = 0;
	xp->reserved2 = 0;
	xp->reserved3[0] = 0L;
	xp->reserved3[1] = 0L;
}

void
biosfs_init()
{
	struct bios_file *b, *c;
	int majdev, mindev;
	int i;

	broot = BDEV;

	c = (struct bios_file *)0;
	for (b = broot; b->name[0]; b++) {
		b->next = b+1;

	/* Save a pointer to the first serial port */
		if (b->private == 6)
			c = b;
		if (b->device->readb && b->tty != &con_tty)
	/* device has DEVDRV calls beyond unselect */
			b->drvsize = offsetof (DEVDRV, readb) + sizeof (long);

	/* if not a TT or Mega STE, adjust the MODEM1 device to be BIOS
	 * device 1
	 * and ignore the remaining devices, since they're not present
	 */
		if (b->private == 6 &&
		    (!has_bconmap || bconmap2->maptabsize == 1)) {
			if (!has_bconmap)
				b->private = 1;
			b->next = 0;
			break;
		}
	/* SERIAL1(!) is not present on the Mega STe or Falcon,
	 * device 8 is SCC channel A
	 */
		if (mch != TT && b->private == 8) {
			b->name[6] = '2';	/* "serial2" */
			b->tty = &scca_tty;
			b->next = 0;
			break;
		}
	}
	bdevlast = b;
	if (b->name[0] == 0) {
		--b;
		b->next = 0;
	}
	/* Initialize bios_tty structures */
	for (i=0;c && i<MAX_BTTY;c=c->next, i++) {
		if (has_bconmap)
			bttys[i].irec = MAPTAB[c->private-6].iorec;
		else
			bttys[i].irec = Iorec(0);
		bttys[i].orec = bttys[i].irec+1;
		bttys[i].rsel = &(c->tty->rsel);
		bttys[i].wsel = &(c->tty->wsel);
		bttys[i].baudmap = baudmap;
		bttys[i].maxbaud = MAXBAUD;
		bttys[i].baudx = NULL;
		*c->tty = default_tty;
		bttys[i].tty = c->tty;
		bttys[i].clocal = 1;	/* default off would be better but
					   likely confuses old programs... :/ */
		bttys[i].brkint = 1;
		bttys[i].tosfd = EUNDEV;
		bttys[i].bdev = c->private;
	}
	btty_max = i;

	midi_btty.irec = Iorec(2);
	midi_btty.rsel = &midi_tty.rsel;
	midi_btty.wsel = &midi_tty.wsel;
	midi_btty.tty = &midi_tty;
	midi_tty = default_tty;
	midi_btty.clocal = 1;
	midi_btty.tosfd = EUNDEV;
	midi_btty.bdev = 3;

	defaultaux = new_fileptr();
	defaultaux->links = 1;		/* so it never gets freed */
	defaultaux->flags = O_RDWR;
	defaultaux->pos = 0;
	defaultaux->devinfo = 0;
	defaultaux->fc.fs = &bios_filesys;
	defaultaux->fc.index = 0;
	defaultaux->fc.aux = 1;
	defaultaux->fc.dev = BIOSDRV;
	defaultaux->dev = &bios_ndevice;

/* set up XATTR fields */
	set_xattr(&rxattr, S_IFDIR|DEFAULT_DIRMODE, BIOSDRV);
	set_xattr(&fdxattr, S_IFDIR|DEFAULT_DIRMODE, BIOSDRV);

	for (b = BDEV; b; b = b->next) {
		if (b->device == &bios_ndevice || b->device == &bios_tdevice) {
			majdev = BIOS_RDEV;
			mindev = b->private;
		} else if (b->device == &fakedev) {
			majdev = FAKE_RDEV;
			mindev = b->private;
		} else {
			majdev = UNK_RDEV;
			mindev = b->private;
		}
		set_xattr(&b->xattr, S_IFCHR|DEFAULT_MODE,
			  majdev | (mindev & 0x00ff) );
	}
}

static long ARGS_ON_STACK 
bios_root(drv, fc)
	int drv;
	fcookie *fc;
{
	if (drv == BIOSDRV) {
		fc->fs = &bios_filesys;
		fc->dev = drv;
		fc->index = 0L;
		return 0;
	}
	fc->fs = 0;
	return EINTRN;
}

static long ARGS_ON_STACK 
bios_lookup(dir, name, fc)
	fcookie *dir;
	const char *name;
	fcookie *fc;
{
	struct bios_file *b;

	if (dir->index != 0) {
	/* check for \dev\fd directory */
		if (!IS_FD_DIR(dir)) {
			DEBUG(("bios_lookup: bad directory"));
			return EPTHNF;
		}
		if (!*name || (name[0] == '.' && name[1] == 0)) {
			*fc = *dir;
			return 0;
		}
		if (!strcmp(name, "..")) {
		/* root directory */
			fc->fs = &bios_filesys;
			fc->dev = dir->dev;
			fc->index = 0L;
			return 0;
		}
		if (isdigit(*name) || *name == '-') {
			int fd = (int) atol(name);
			if (fd >= MIN_HANDLE && fd < MAX_OPEN) {
				fc->fs = &bios_filesys;
				fc->dev = dir->dev;
				fc->aux = fd;
				fc->index = fd - MIN_HANDLE + 1;
				return 0;
			}
		}
		DEBUG(("bios_lookup: name (%s) not found", name));
		return EFILNF;
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

	for (b = broot; b; b = b->next) {
		if (!stricmp(b->name, name)) {
			fc->fs = &bios_filesys;
			fc->index = (long)b;
			fc->aux = b->private;
			fc->dev = dir->dev;
			return 0;
		}
	}
	DEBUG(("bios_lookup: name(%s) not found", name));
	return EFILNF;
}

static long ARGS_ON_STACK 
bios_getxattr(fc, xattr)
	fcookie *fc;
	XATTR *xattr;
{
#ifdef FOLLOW_XATTR_CHAIN
	FILEPTR *f;
	long r;
#endif
	struct bios_file *b = (struct bios_file *)fc->index;
	int majdev, mindev;

	majdev = UNK_RDEV;
	mindev = 0;

	if (fc->index == 0) {			/* root directory? */
		*xattr = rxattr;
		xattr->index = fc->index;
		xattr->dev = fc->dev;
	} else if (IS_FD_DIR(fc)) {		/* fd directory? */
		*xattr = fdxattr;
		xattr->index = fc->index;
		xattr->dev = fc->dev;
	} else if (IS_FD_ENTRY(fc)) {
		/* u:\dev\fd\n */
#ifdef FOLLOW_XATTR_CHAIN
		f = curproc->handle[(int)fc->aux];
		if (f) {
			r = (*f->fc.fs->getxattr)(&f->fc, xattr);
			if (r < 0)
				return r;
		} else {
#endif
			majdev = FAKE_RDEV;
			mindev = ((int)fc->aux) & 0x00ff;
			set_xattr(xattr, S_IFCHR | DEFAULT_MODE, majdev|mindev);
#ifndef FOLLOW_XATTR_CHAIN
			xattr->index = fc->index;
#else
		}
#endif
	} else if (b->device == &fakedev) {
#ifdef FOLLOW_XATTR_CHAIN
		if ((f = curproc->handle[b->private]) != 0) {
		    /* u:\dev\stdin, u:\dev\stdout, etc. */
		    r = (*f->fc.fs->getxattr) (&f->fc, xattr);
		    if (r < 0) return r;
		} else {
#endif
			majdev = FAKE_RDEV;
			mindev = ((int)b->private) & 0x00ff;
			set_xattr(xattr, S_IFCHR|DEFAULT_MODE, majdev|mindev);
#ifndef FOLLOW_XATTR_CHAIN
			xattr->index = fc->index;
#else
		}
#endif
	} else {
		*xattr = b->xattr;
		xattr->index = fc->index;
		xattr->dev = fc->dev;
	}
	return 0;
}

static long ARGS_ON_STACK 
bios_chattr(fc, attrib)
	fcookie *fc;
	int attrib;
{
	UNUSED(fc); UNUSED(attrib);
	return EACCDN;
}

static long ARGS_ON_STACK 
bios_chown(fc, uid, gid)
	fcookie *fc;
	int uid, gid;
{
	struct bios_file *b = (struct bios_file *)fc->index;

	if (!(curproc->euid)) {
		if (!b) {
			/* a directory */
			rxattr.uid = uid;
			rxattr.gid = gid;
		} else if (IS_FD_DIR(fc)) {
			fdxattr.uid = uid;
			fdxattr.gid = gid;
		} else if (!IS_FD_ENTRY(fc)) {
			/* any other entry */
			b->xattr.uid = uid;
			b->xattr.gid = gid;
		}
		return 0;
	}

	return EACCDN;
}

static long ARGS_ON_STACK 
bios_chmode(fc, mode)
	fcookie *fc;
	unsigned mode;
{
	struct bios_file *b = (struct bios_file *)fc->index;

	if (!b) {
		/* root directory */
		if (!curproc->euid || (curproc->euid == rxattr.uid)) {
			rxattr.mode = (rxattr.mode & S_IFMT) | mode;
			return 0;
		}
	} else if (IS_FD_DIR(fc)) {
		if (!curproc->euid || (curproc->euid == fdxattr.uid)) {
			fdxattr.mode = (fdxattr.mode & S_IFMT) | mode;
			return 0;
		}
	} else if (!IS_FD_ENTRY(fc)) {
		if (!curproc->euid && (curproc->euid == b->xattr.uid)) {
			b->xattr.mode = (b->xattr.mode & S_IFMT) | mode;
			return 0;
		}
	}

	return EACCDN;
}

long ARGS_ON_STACK 
nomkdir(dir, name, mode)
	fcookie *dir;
	const char *name;
	unsigned mode;
{
	UNUSED(dir); UNUSED(name);
	UNUSED(mode);
	return EACCDN;
}

static long ARGS_ON_STACK 
bios_rmdir(dir, name)
	fcookie *dir;
	const char *name;
{
	return bios_remove(dir, name);
}

/*
 * MAJOR BUG: we don't check here for removal of devices for which there
 * are still open files
 */

static long ARGS_ON_STACK 
bios_remove(dir, name)
	fcookie *dir;
	const char *name;
{
	struct bios_file *b, **lastb;

	if (curproc->euid)
		return EACCDN;

/* don't allow removal in the fd directory */
	if (IS_FD_DIR(dir))
		return EACCDN;

	lastb = &broot;
	for (b = broot; b; b = *(lastb = &b->next)) {
		if (!stricmp(b->name, name)) break;
	}
	if (!b) return EFILNF;

/* don't allow removal of the device if we don't own it */
	if (curproc->euid && (curproc->euid != b->xattr.uid)) {
		return EACCDN;
	}

/* don't allow removal of the basic system devices */
	if (b >= BDEV && b <= bdevlast) {
		return EACCDN;
	}
	*lastb = b->next;

	if (b->device == 0 || b->device == &bios_tdevice)
		kfree(b->tty);

	kfree(b);
	return 0;
}

static long ARGS_ON_STACK 
bios_getname(root, dir, pathname, size)
	fcookie *root, *dir; char *pathname;
	int size;
{
	char *foo;

	if (size == 0)
	  return ERANGE;
	if (root->index == dir->index) {
	    *pathname = 0;
	    return 0;
	}
	/* DIR must point to the fd directory */
	if (!IS_FD_DIR (dir))
		return EINTRN;
	*pathname++ = '\\';
	size--;
	foo = ((struct bios_file *)dir->index)->name;
	if (strlen(foo) < size)
		strcpy(pathname, foo);
	else
		return ERANGE;
	return 0;
}

static long ARGS_ON_STACK 
bios_rename(olddir, oldname, newdir, newname)
	fcookie *olddir;
	char *oldname;
	fcookie *newdir;
	const char *newname;
{
	struct bios_file *b, *be = 0;

	UNUSED(olddir); UNUSED(newdir);

	if (curproc->euid)
		return EACCDN;

	for (b = broot; b; b = b->next) {
		if (!stricmp(b->name, newname)) {
			return EACCDN;
		}
		if (!stricmp(b->name, oldname)) {
			be = b;
		}
	}
	if (be) {
		strncpy(be->name, newname, BNAME_MAX);
		return 0;
	}
	return EFILNF;
}

static long ARGS_ON_STACK 
bios_opendir(dirh, flags)
	DIR *dirh;
	int flags;
{
	UNUSED(flags);

	if (dirh->fc.index != 0 && !IS_FD_DIR(&dirh->fc)) {
		DEBUG(("bios_opendir: bad directory"));
		return EPTHNF;
	}
	return 0;
}

static long ARGS_ON_STACK 
bios_readdir(dirh, name, namelen, fc)
	DIR *dirh;
	char *name;
	int namelen;
	fcookie *fc;
{
	struct bios_file *b;
	int giveindex = dirh->flags == 0;
	int i;
	char buf[5];

	if (IS_FD_DIR(&dirh->fc)) {
		i = dirh->index++;
		if (i+MIN_HANDLE >= MAX_OPEN)
			return ENMFIL;
		fc->fs = &bios_filesys;
		fc->index = i+1;
		fc->aux = i+MIN_HANDLE;
		fc->dev = dirh->fc.dev;
		if (giveindex) {
			namelen -= (int)sizeof(long);
			if (namelen <= 0)
				return ERANGE;
			*(long *)name = (long)i + 1;
			name += sizeof(long);
		}
		ksprintf(buf, "%d", i+MIN_HANDLE);
		strncpy(name, buf, namelen - 1);
		if (strlen(buf) >= namelen)
			return ENAMETOOLONG;
		return 0;
	}

	b = broot;
	i = dirh->index++;
	while(i-- > 0) {
		if (!b) break;
		b = b->next;
	}
	if (!b) {
		return ENMFIL;
	}
	fc->fs = &bios_filesys;
	fc->index = (long)b;
	fc->aux = b->private;
	fc->dev = dirh->fc.dev;
	if (giveindex) {
		namelen -= (int)sizeof(long);
		if (namelen <= 0)
			return ERANGE;
		*((long *)name) = (long) b;
		name += sizeof(long);
	}
	strncpy(name, b->name, namelen-1);
	if (strlen(b->name) >= namelen)
		return ENAMETOOLONG;
	return 0;
}

static long ARGS_ON_STACK 
bios_rewinddir(dirh)
	DIR *dirh;
{
	dirh->index = 0;
	return 0;
}

static long ARGS_ON_STACK 
bios_closedir(dirh)
	DIR *dirh;
{
	UNUSED(dirh);
	return 0;
}

static long ARGS_ON_STACK 
bios_pathconf(dir, which)
	fcookie *dir;
	int which;
{
	UNUSED(dir);

	switch(which) {
	case -1:
		return DP_MAXREQ;
	case DP_IOPEN:
		return UNLIMITED;	/* no limit on BIOS file descriptors */
	case DP_MAXLINKS:
		return 1;		/* no hard links available */
	case DP_PATHMAX:
		return PATH_MAX;
	case DP_NAMEMAX:
		return BNAME_MAX;
	case DP_ATOMIC:
		return 1;		/* no atomic writes */
	case DP_TRUNC:
		return DP_AUTOTRUNC;	/* names are truncated */
	case DP_CASE:
		return DP_CASEINSENS;	/* not case sensitive */
	case DP_MODEATTR:
		return (0777L << 8)|
				DP_FT_DIR|DP_FT_CHR;
	case DP_XATTRFIELDS:
		return DP_INDEX|DP_DEV|DP_RDEV|DP_NLINK|DP_UID|DP_GID;
	default:
		return EINVFN;
	}
}

static long ARGS_ON_STACK 
bios_dfree(dir, buf)
	fcookie *dir;
	long *buf;
{
	UNUSED(dir);

	buf[0] = 0;	/* number of free clusters */
	buf[1] = 0;	/* total number of clusters */
	buf[2] = 1;	/* sector size (bytes) */
	buf[3] = 1;	/* cluster size (sectors) */
	return 0;
}

/*
 * BIOS Dcntl() calls:
 * Dcntl(0xde02, "U:\DEV\FOO", &foo_descr): install a new device called
 *     "FOO", which is described by the dev_descr structure "foo_descr".
 *     this structure has the following fields:
 *         DEVDRV *driver		the device driver itself
 *	   short  dinfo			info for the device driver
 *	   short  flags			flags for the file (e.g. O_TTY)
 *	   struct tty *tty		tty structure, if appropriate
 *	   long   drvsize;		size of driver struct
 *
 * Dcntl(0xde00, "U:\DEV\BAR", n): install a new BIOS terminal device, with
 *     BIOS device number "n".
 * Dcntl(0xde01, "U:\DEV\BAR", n): install a new non-tty BIOS device, with
 *     BIOS device number "n".
 */

static long ARGS_ON_STACK
bios_fscntl(dir, name, cmd, arg)
	fcookie *dir;
	const char *name;
	int cmd;
	long arg;
{
	struct bios_file *b;
	static int devindex = 0;

	if (curproc->euid) {
		DEBUG(("biosfs: Dcntl() by non-privileged process"));
		return ((unsigned)cmd == DEV_INSTALL) ? 0 : EACCDN;
	}

	if (IS_FD_DIR(dir))
		return EACCDN;

	/* ts: let's see if such an entry already exists */
	for (b=broot; b; b=b->next)
		if (!strcmp(b->name, name))
			break;

	switch((unsigned)cmd) {
	case DEV_INSTALL:
	    {
		struct dev_descr *d = (struct dev_descr *)arg;

		if (!b) {
			b = kmalloc(SIZEOF(struct bios_file));
			if (!b) return 0;
			b->next = broot;
			broot = b;
			strncpy(b->name, name, BNAME_MAX);
			b->name[BNAME_MAX] = 0;
		}
		b->device = d->driver;
		b->private = d->dinfo;
		b->flags = d->flags;
		b->tty = d->tty;
		b->drvsize = d->drvsize;
		set_xattr(&(b->xattr), S_IFCHR|DEFAULT_MODE, UNK_RDEV|devindex);
		devindex = (devindex+1) & 0x00ff;
		return (long)&kernelinfo;
	    }
	case DEV_NEWTTY:
		if (!b) {
			b = kmalloc(SIZEOF(struct bios_file));
			if (!b)
				return ENSMEM;
			strncpy(b->name, name, BNAME_MAX);
			b->name[BNAME_MAX] = 0;
			b->tty = kmalloc(SIZEOF(struct tty));
			if (!b->tty) {
				kfree(b);
				return ENSMEM;
			}
			b->next = broot;
			broot = b;
		} else {
			/*  ts: it's probably better to use a new tty
			 * structure here, but don't touch the old
			 * pointers until we know we've got enough
			 * memory to do it!
			 */
			struct tty *ttyptr;

			if ((ttyptr = kmalloc(SIZEOF(struct tty))) == 0)
				return ENSMEM;
			b->tty = ttyptr;
		}
		b->drvsize = 0;
		b->device = &bios_tdevice;
		b->private = arg;
		b->flags = O_TTY;
		*b->tty = default_tty;
		set_xattr(&(b->xattr), S_IFCHR|DEFAULT_MODE, BIOS_RDEV|(b->private&0x00ff));
		return 0;

	case DEV_NEWBIOS:
		if (!b) {
			b = kmalloc(SIZEOF(struct bios_file));
			if (!b) return ENSMEM;
			b->next = broot;
			broot = b;
			strncpy(b->name, name, BNAME_MAX);
			b->name[BNAME_MAX] = 0;
		}
		b->drvsize = 0;
		/*  ts: it's probably better not to free an old tty
		 * structure here, cause we don't know if any process
		 * who didn't recognize this change is still using it.
		 */
		b->tty = 0;
		b->device = &bios_ndevice;
		b->private = arg;
		b->flags = 0;
		set_xattr(&(b->xattr), S_IFCHR|DEFAULT_MODE, BIOS_RDEV|(b->private&0x00ff));
		return 0;
	default:
		return EINVFN;
	}
}

static long ARGS_ON_STACK 
bios_symlink(dir, name, to)
	fcookie *dir;
	const char *name, *to;
{
	struct bios_file *b;
	long r;
	fcookie fc;

	if (curproc->euid)
		return EACCDN;

	if (IS_FD_DIR(dir))
		return EACCDN;

	r = bios_lookup(dir, name, &fc);
	if (r == 0) return EACCDN;	/* file already exists */
	if (r != EFILNF) return r;	/* some other error */

	b = kmalloc(SIZEOF(struct bios_file));
	if (!b) return EACCDN;

	strncpy(b->name, name, BNAME_MAX);
	b->name[BNAME_MAX] = 0;
	b->device = 0;
	b->private = EINVFN;
	b->flags = 0;
	b->tty = kmalloc((long)strlen(to)+1);
	if (!b->tty) {
		kfree(b);
		return EACCDN;
	}
	strcpy((char *)b->tty, to);

	set_xattr(&b->xattr, S_IFLNK|DEFAULT_DIRMODE, BIOSDRV);
	b->xattr.size = strlen(to)+1;

	b->next = broot;
	broot = b;
	return 0;
}

static long ARGS_ON_STACK 
bios_readlink(fc, buf, buflen)
	fcookie *fc;
	char *buf;
	int buflen;
{
	struct bios_file *b = (struct bios_file *)fc->index;

	if (IS_FD_DIR(fc) || IS_FD_ENTRY(fc))
		return EINVFN;
	if (!b) return EINVFN;
	if (b->device) return EINVFN;

	strncpy(buf, (char *)b->tty, buflen);
	if (strlen((char *)b->tty) >= buflen)
		return ENAMETOOLONG;
	return 0;
}


/*
 * routines for file systems that don't support volume labels
 */

long ARGS_ON_STACK 
nowritelabel(dir, name)
	fcookie *dir;
	const char *name;
{
	UNUSED(dir);
	UNUSED(name);
	return EACCDN;
}

long ARGS_ON_STACK 
noreadlabel(dir, name, namelen)
	fcookie *dir;
	char *name;
	int namelen;
{
	UNUSED(dir);
	UNUSED(name);
	UNUSED(namelen);
	return EFILNF;
}

/*
 * routines for file systems that don't support links
 */

long ARGS_ON_STACK 
nosymlink(dir, name, to)
	fcookie *dir;
	const char *name, *to;
{
	UNUSED(dir); UNUSED(name);
	UNUSED(to);
	return EINVFN;
}

long ARGS_ON_STACK 
noreadlink(dir, buf, buflen)
	fcookie *dir;
	char *buf;
	int buflen;
{
	UNUSED(dir); UNUSED(buf);
	UNUSED(buflen);
	return EINVFN;
}

long ARGS_ON_STACK 
nohardlink(fromdir, fromname, todir, toname)
	fcookie *fromdir, *todir;
	const char *fromname, *toname;
{
	UNUSED(fromdir); UNUSED(todir);
	UNUSED(fromname); UNUSED(toname);
	return EINVFN;
}

/* dummy routine for file systems with no Fscntl commands */

long ARGS_ON_STACK 
nofscntl(dir, name, cmd, arg)
	fcookie *dir;
	const char *name;
	int cmd;
	long arg;
{
	UNUSED(dir); UNUSED(name);
	UNUSED(cmd); UNUSED(arg);
	return EINVFN;
}

/*
 * Did the disk change? Not on this drive!
 * However, we have to do Getbpb anyways, because someone has decided
 * to force a media change on our (non-existent) drive.
 */
long ARGS_ON_STACK 
nodskchng(drv)
	int drv;
{
	(void)getbpb(drv);
	return 0;
}

long ARGS_ON_STACK 
nocreat(dir, name, mode, attrib, fc)
	fcookie *dir, *fc;
	const char *name;
	unsigned mode;
	int attrib;
{
	UNUSED(dir); UNUSED(fc);
	UNUSED(name); UNUSED(mode);
	UNUSED(attrib);
	return EACCDN;
}

static DEVDRV * ARGS_ON_STACK 
bios_getdev(fc, devsp)
	fcookie *fc;
	long *devsp;
{
	struct bios_file *b;

	/* Check for \dev\fd\... */
	if (IS_FD_ENTRY(fc)) {
	    *devsp = (int) fc->aux;
	    return &fakedev;
	}

	b = (struct bios_file *)fc->index;

	if (b->device && b->device != &fakedev)
		*devsp = (long)b->tty;
	else
		*devsp = b->private;

	return b->device;	/* return the device driver */
}

/*
 * NULL device driver
 */

long ARGS_ON_STACK 
null_open(f)
	FILEPTR *f;
{
	UNUSED(f);
	return 0;
}

long ARGS_ON_STACK 
null_write(f, buf, bytes)
	FILEPTR *f; const char *buf; long bytes;
{
	UNUSED(f); UNUSED(buf);
	return bytes;
}

long ARGS_ON_STACK 
null_read(f, buf, bytes)
	FILEPTR *f; char *buf; long bytes;
{
	UNUSED(f); UNUSED(buf);
	UNUSED(bytes);
	return 0;
}

long ARGS_ON_STACK 
null_lseek(f, where, whence)
	FILEPTR *f; long where; int whence;
{
	UNUSED(f); UNUSED(whence);
	return (where == 0) ? 0 : ERANGE;
}

long ARGS_ON_STACK 
null_ioctl(f, mode, buf)
	FILEPTR *f; int mode; void *buf;
{
	UNUSED(f);

	switch(mode) {
	case FIONREAD:
		*((long *)buf) = 0;
		break;
	case FIONWRITE:
		*((long *)buf) = 1;
		break;
	case FIOEXCEPT:
		*((long *)buf) = 0;
	default:
		return EINVFN;
	}
	return 0;
}

long ARGS_ON_STACK 
null_datime(f, timeptr, rwflag)
	FILEPTR *f;
	short *timeptr;
	int rwflag;
{
	UNUSED(f);
	if (rwflag)
		return EACCDN;
	*timeptr++ = timestamp;
	*timeptr = datestamp;
	return 0;
}

long ARGS_ON_STACK 
null_close(f, pid)
	FILEPTR *f;
	int pid;
{
	UNUSED(f);
	UNUSED(pid);
	return 0;
}

long ARGS_ON_STACK 
null_select(f, p, mode)
	FILEPTR *f; long p;
	int mode;
{
	UNUSED(f); UNUSED(p);
	if ((mode == O_RDONLY) || (mode == O_WRONLY))
		return 1;	/* we're always ready to read/write */

	return 0;	/* other things we don't care about */
}

void ARGS_ON_STACK 
null_unselect(f, p, mode)
	FILEPTR *f;
	long p;
	int mode;
{
	UNUSED(f); UNUSED(p);
	UNUSED(mode);
	/* nothing to do */
}

/*
 * BIOS terminal device driver
 */

static long ARGS_ON_STACK 
bios_topen(f)
	FILEPTR *f;
{
	struct tty *tty = (struct tty *)f->devinfo;
	int bdev = f->fc.aux;
	struct bios_tty *b;

	f->flags |= O_TTY;
	if (!tty->use_cnt && ((b = BTTY(f))) && b->tosfd == EUNDEV)
		b->tosfd = rsvf_open (bdev);
	return 0;
}

/*
 * Note: when a BIOS device is a terminal (i.e. has the O_TTY flag
 * set), bios_read and bios_write will only ever be called indirectly, via
 * tty_read and tty_write. That's why we can afford to play a bit fast and
 * loose with the pointers ("buf" is really going to point to a long) and
 * why we know that "bytes" is divisible by 4.
 */

static long ARGS_ON_STACK 
bios_twrite(f, buf, bytes)
	FILEPTR *f; const char *buf; long bytes;
{
	long *r;
	long ret = 0;
	int bdev = f->fc.aux;
	struct bios_file *b = (struct bios_file *)f->fc.index;

	r = (long *)buf;

/* Check for control characters on any newline output.
 * Note that newlines are always output through tty_putchar,
 * so they'll always be the first thing in the buffer (at least,
 * for cooked TTY output they will, which is the only sort that
 * control characters affect anyways).
 */
	if (bdev == 2 && bytes > 0 && (*r & 0x000000ffL) == '\n')
 		(void) checkkeys();

	if (f->flags & O_NDELAY) {
		while (bytes > 0) {
		    if (!bcostat(bdev)) break;
		    if (bconout(bdev, (int)*r) == 0)
			break;
		    r++; bytes -= 4; ret+= 4;
		}
	} else {
		while (bytes > 0) {
		    if (bconout(bdev, (int)*r) == 0)
			break;
		    r++; bytes -= 4; ret+= 4;
		}
	}

	if (ret > 0) {
		b->xattr.mtime = b->xattr.atime = timestamp;
		b->xattr.mdate = b->xattr.adate = datestamp;
	}
	return ret;
}

static long ARGS_ON_STACK 
bios_tread(f, buf, bytes)
	FILEPTR *f; char *buf; long bytes;
{
	long *r, ret = 0;
	int bdev = f->fc.aux;
	struct  bios_file *b = (struct bios_file *)f->fc.index;

	r = (long *)buf;

	if ((f->flags & O_NDELAY)) {
		while (bytes > 0) {
		    if ( !bconstat(bdev) )
			break;
		    *r++ = bconin(bdev) & 0x7fffffffL;
		    bytes -= 4; ret += 4;
		}
	} else {
		while (bytes > 0) {
		    *r++ = bconin(bdev) & 0x7fffffffL;
		    bytes -= 4; ret += 4;
		}
	}
	if (ret > 0) {
		b->xattr.atime = timestamp;
		b->xattr.adate = datestamp;
	}
	return ret;
}

/*
 * wakewrite(p): wake process p sleeping in write (timeout)
 */

static void ARGS_ON_STACK
wakewrite(p)
	PROC *p;
{
	short s;

	s = spl7();	/* block interrupts */
	p->wait_cond = 0;
	if (p->wait_q == IO_Q) {
		rm_q(IO_Q, p);
		add_q(READY_Q, p);
	}
	spl(s);
}

/*
 * fast RAW byte IO for BIOS ttys
 * without this a RAW tty read goes thru bios_tread for every single
 * char, calling BIOS 3 times per byte at least...  a poor 8 MHz ST
 * just can't move real 19200 bits per second that way, before a
 * byte crawled thru all this the next has already arrived.
 * if the device has xcon* calls and a `normal' iorec these functions
 * access the buffers directly using as little CPU time as possible,
 * for other devices they return EUNDEV (== do the slow thing then).
 * yes it is a hack but better one hack here than hacks in every
 * user process that wants good RAW IO performance...
 */

static long ARGS_ON_STACK 
bios_writeb(f, buf, bytes)
	FILEPTR *f; const char *buf; long bytes;
{
	int bdev = f->fc.aux;
	struct bios_file *b = (struct bios_file *)f->fc.index;

/* do the slow thing if tty is not in RAW mode until serial lines
 * handle control chars properly
 */
	if (!(((struct tty *)f->devinfo)->sg.sg_flags & T_RAW))
		return EUNDEV;
	return iwrite (bdev, buf, bytes, (f->flags & O_NDELAY), b);
}

/* FILEPTR-less entrypoint for bflush etc. */

long
iwrite(bdev, buf, bytes, ndelay, b)
	int bdev; const char *buf; long bytes; int ndelay; struct bios_file *b;
{
	IOREC_T *ior = 0;
	long *cout = 0;	/* keep compiler happy */
	long *ospeed;
	struct tty *tty = 0; /* still not happy yet? */
	const char *p = buf;
	int slept = 0;

#if 1
#define _hz_200 (*((long *)0x4baL))

	if (bdev == 3 && tosvers >= 0x0102) {
		/* midi */
		long ret = 0, tick = _hz_200 + 1;

		cout = &xconout[3];
		while (bytes > 0) {
			while (!(int)callout1(xcostat[4], 4)) {
				if (ndelay)
					break;
				if (_hz_200 - tick > 0) {
					yield();
					tick = _hz_200 + 1;
				}
			}
			(void) callout2(*cout, bdev, (unsigned char) *p++);
			bytes--;
		}
		if (ret > 0) {
			b->xattr.mtime = b->xattr.atime = timestamp;
			b->xattr.mdate = b->xattr.adate = datestamp;
		}
		return p - buf;
	}
#endif

	if (has_bconmap) {
		if ((unsigned)bdev-6 < btty_max) {
			ior = MAPTAB[bdev-6].iorec + 1;
			cout = &MAPTAB[bdev-6].bconout;
			ospeed = &bttys[bdev-6].ospeed;
			tty = bttys[bdev-6].tty;
		}
	} else if (bdev == 1 && tosvers >= 0x0102) {
		ior = bttys[0].orec;
		cout = &xconout[1];
		ospeed = &bttys[0].ospeed;
		tty = bttys[0].tty;
	}

/* no iorec, fall back to the slow way... */
	if (!ior)
		return EUNDEV;

	if (!buf) {
		/* flush send buffer...  should be safe to just set the
		   tail pointer. */
		ior->tail = ior->head;
		return 0;
	}

	if (!bytes)
		/* nothing to do... */
		return 0;

	do {
		char ch;
		unsigned short tail, bsize, wrap, newtail;
		long free;

		if (tty->state & TS_BLIND)
			/* line disconnected... */
			return p - buf;

		tail = ior->tail;
		bsize = ior->buflen;
		if ((free = (long)ior->head - tail - 1) < 0)
			free += bsize;

		/* if buffer is full or we're blocking and can't write enuf */
		if ((unsigned long)free < 2 ||
		    (!ndelay && free < bytes && free < bsize/2)) {
			long sleepchars;
			unsigned long isleep = 0;

			/* if the write should not block thats it. */
			if (ndelay)
				return p - buf;

			/* else sleep the (minimum) time it takes until
			   the buffer is either half-empty or has space
			   enough for the write, wichever is smaller. */
			if ((sleepchars = bsize/2) > bytes)
				sleepchars = bytes;
			sleepchars -= free;

			if (*ospeed > 0)
				isleep = (unsigned long)
					((sleepchars * 10000L) / *ospeed);

			/* except that if we already slept and the buffer
			   still was full we sleep for at least 60
			   milliseconds. (driver must be waiting for
			   some handshake signal and we don't want to
			   hog the processor.) */
			if (slept && isleep < 60)
				isleep = 60;

			if (isleep < 5)
				/* if it still would be less than 5
				   milliseconds then just give up this
				   timeslice */
				yield();
			else if (isleep < 20)
				nap (isleep);
			else {
				TIMEOUT *t;

				if (isleep > 200)
					isleep = 200;
				curproc->wait_cond = (long)&tty->state;
				t = addtimeout((long)isleep, wakewrite);
				if (t) {
					TRACE(("sleeping in iwrite"));
					sleep(IO_Q|0x100, (long)&tty->state);
					canceltimeout(t);
				}
			}

			/* loop and try again. */
			slept = (unsigned long)free < 2;
			continue;
		}
		slept = 0;

		/* save the 1st char, we could need it later. */
		ch = *p;
		wrap = bsize - tail;
		if (--free > bytes)
			free = bytes;
		bytes -= free;

		/* now copy to buffer.  if its just a few then do it here... */
		if (free < 5) {
			char *q = ior->bufaddr + tail;

			while (free--) {
				if (!--wrap)
					q -= bsize;
				*++q = *p++;
			}
			newtail = q - ior->bufaddr;

		/* else use memcpy.  */
		} else {
			/* --wrap and tail+1 because tail is `inc before access' */
			if (--wrap < free) {
				memcpy (ior->bufaddr + tail + 1, p, wrap);
				memcpy (ior->bufaddr, p + wrap, free - wrap);
				newtail = free - wrap - 1;
			} else {
				memcpy (ior->bufaddr + tail + 1, p, free);
				newtail = tail + free;
			}
			p += free;
		}

		/* the following has to be done with interrupts off to avoid
		   race conditions. */
		{
			short sr = spl7();

			/* if the buffer is empty there might be no
			   interrupt that sends the next char, so we
			   send it thru the xcon* vector. */
			if (ior->head == ior->tail) {
				(void) callout2(*cout, bdev, (unsigned char) ch);

				/* if the buffer now is still empty set
				   the head pointer to skip the 1st char
				   (that we just sent). */
				if (ior->head == ior->tail) {
					if (++tail >= bsize)
						tail = 0;
					ior->head = tail;
				}
			}
			ior->tail = newtail;

			spl(sr);
			if (b) {
				b->xattr.mtime = b->xattr.atime = timestamp;
				b->xattr.mdate = b->xattr.adate = datestamp;
			}
		}
	/* if we're blocking loop until everything is written */
	} while (bytes && !ndelay);

	return p - buf;
}

/*
 * fast RAW BIOS tty read
 * this really works like a RAW tty read i.e. only blocks until _some_
 * data is ready.
 */

static long ARGS_ON_STACK 
bios_readb(f, buf, bytes)
	FILEPTR *f; char *buf; long bytes;
{
	int bdev = f->fc.aux;
	struct bios_file *b = (struct bios_file *)f->fc.index;
	struct tty *tty = (struct tty *)f->devinfo;

	if (!(tty->sg.sg_flags & T_RAW) || (tty->sg.sg_flags & T_ECHO))
		return EUNDEV;
	/* if VTIME is set tty_read already select()ed the tty
	 * so from here on the read should not block anymore */
	return iread (bdev, buf, bytes, (f->flags & O_NDELAY) || tty->vtime, b);
}

long
iread(bdev, buf, bytes, ndelay, b)
	int bdev; char *buf; long bytes; int ndelay; struct bios_file *b;
{
	IOREC_T *ior = 0;
	long *cin = 0;	/* keep compiler happy */
	long *cinstat;
	struct bios_tty *t = 0;
	char *p;
	unsigned short head, bsize, wrap;
	long left;

	if (bdev == 3 && tosvers >= 0x0102) {
		/* midi */
		t = &midi_btty;
		ior = t->irec;
		cin = &xconin[3];
		cinstat = &xconstat[3];
	} else if (has_bconmap) {
		if ((unsigned)bdev-6 < btty_max) {
			t = bttys+bdev-6;
			ior = MAPTAB[bdev-6].iorec;
			cin = &MAPTAB[bdev-6].bconin;
			cinstat = &MAPTAB[bdev-6].bconstat;
		}
	} else if (bdev == 1 && tosvers >= 0x0102) {
		t = bttys;
		ior = t->irec;
		cin = &xconin[1];
		cinstat = &xconstat[1];
	}

/* no iorec, fall back to the slow way... */
	if (!ior)
		return EUNDEV;

	if (buf && !bytes)
		/* nothing to do... */
		return 0;

	/* if the read should block sleep until VMIN chars ready (or disconnect)
	 */
	if (buf && !ndelay) {
		while (t ? (!(t->tty->state & TS_BLIND) &&
			    btty_ionread(t) < (long)t->tty->vmin)
			 : !(int)callout1(*cinstat, bdev)) {
			if (t)
				sleep(IO_Q, (long)t);
			else
				nap(60);
		}
	}

	if (t && (t->tty->state & TS_BLIND))
		/* line disconnected... */
		return 0;

	head = ior->head;
	if (0 == (left = ((unsigned long) ior->tail) - head)) {
		/* if the buffer is still empty we're finished... */
		return 0;
	}

	/* now copy the data out of the buffer */
	bsize = ior->buflen;
	if (left < 0)
		left += bsize;
	wrap = bsize - head;

	/* if we should flush input pretend we want to read it all */
	if (!buf && !bytes)
		bytes = left;

	if (left > bytes)
		left = bytes;

	/* if its just a few then do it here... */
	if (buf && left <= 5) {
		char *q = ior->bufaddr + head;

		/* the --left in the while() makes us get one char less
		   because we want to get the last one thru the driver
		   so that it gets a chance to raise RTS or send XON... */
		p = buf;
		while (--left) {
			if (!--wrap)
				q -= bsize;
			*p++ = *++q;
		}
		ior->head = q - ior->bufaddr;

	/* else memcpy is faster. */
	} else {
		/* --wrap and head+1 because head is `inc before access' */
		if (--wrap < --left) {
			if (buf) {
				memcpy (buf, ior->bufaddr + head + 1, wrap);
				memcpy (buf + wrap, ior->bufaddr, left - wrap);
			}
			ior->head = left - wrap - 1;
		} else {
			if (buf)
				memcpy (buf, ior->bufaddr + head + 1, left);
			ior->head = head + left;
		}
		/* p points to last char */
		p = buf + left;
	}

	{
		short sr = spl7();

		/* xconin[] are always blocking, and we don't want to
		   hang at ipl7 even if something impossible like a
		   `magically' empty buffer happens.  so check again. */
		if (ior->tail != ior->head)
			if (buf)
				*p++ = callout1(*cin, bdev);
			else
				(void) callout1(*cin, bdev);
		spl(sr);
		if (b) {
			b->xattr.atime = timestamp;
			b->xattr.adate = datestamp;
		}
	}
	if (!buf)
		return 0;
	return p - buf;
}

/*
 * read/write routines for BIOS devices that aren't terminals (like the
 * printer & IKBD devices)
 */

static long ARGS_ON_STACK 
bios_nwrite(f, buf, bytes)
	FILEPTR *f; const char *buf; long bytes;
{
	long ret = 0;
	int bdev = f->fc.aux;
	int c;
	struct bios_file *b = (struct bios_file *)f->fc.index;

	while (bytes > 0) {
		if ( (f->flags & O_NDELAY) && !bcostat(bdev) )
			break;

		c = *buf++ & 0x00ff;

		if (bconout(bdev, c) == 0)
			break;

		bytes--; ret++;
	}
	if (ret > 0) {
		b->xattr.mtime = b->xattr.atime = timestamp;
		b->xattr.mdate = b->xattr.adate = datestamp;
	}
	return ret;
}

static long ARGS_ON_STACK 
bios_nread(f, buf, bytes)
	FILEPTR *f; char *buf; long bytes;
{
	long ret = 0;
	int bdev = f->fc.aux;
	struct bios_file *b = (struct bios_file *)f->fc.index;

	while (bytes > 0) {
		if ( (f->flags & O_NDELAY) && !bconstat(bdev) )
			break;
		*buf++ = bconin(bdev) & 0xff;
		bytes--; ret++;
	}
	if (ret > 0) {
		b->xattr.atime = timestamp;
		b->xattr.adate = datestamp;
	}
	return ret;
}

/*
 * BIOS terminal seek code -- this has to match the documented
 * way to do isatty()
 */

static long ARGS_ON_STACK 
bios_tseek(f, where, whence)
	FILEPTR *f;
	long where;
	int whence;
{
	UNUSED(f); UNUSED(where);
	UNUSED(whence);
/* terminals always are at position 0 */
	return 0;
}

/*
 * ioctl TIOC[CS]BRK, checkbtty calls it without a FILEPTR
 */

long
iocsbrk (bdev, mode, t)
	int bdev, mode;
	struct bios_tty *t;
{
	unsigned long bits;
	int oldmap = curproc->bconmap;

	if (has_bconmap) {
/* YABB (Yet Another BIOS Bug):
 * SCC Rsconf looks for the break bit in the wrong place...  if this
 * dev's Rsconf is in ROM do it ourselves, otherwise assume the user
 * has installed a fix.
 */
		if (t && (bdev == 7 || t->tty == &scca_tty) &&
		    MAPTAB[bdev-6].rsconf > 0xe00000L &&
		    MAPTAB[bdev-6].rsconf < 0xefffffL) {
			scc_set5 ((volatile char *) (bdev == 7 ?
				    0xffff8c85L : 0xffff8c81L),
				  (mode == TIOCSBRK), (1 << 4), t->irec);
			return 0;
		}
		if (bdev >= 6)
			curproc->bconmap = bdev;
	}
	bits = rsconf(-1, -1, -1, -1, -1, -1);	/* get settings */
	bits = (bits >> 8) & 0x0ff;		/* isolate TSR byte */
	if (mode == TIOCCBRK)
		bits &= ~8;
	else
		bits |= 8;
	(void)rsconf(-1, -1, -1, -1, (int)bits, -1);
	curproc->bconmap = oldmap;
	return 0;
}

/*
 * ioctl TIOCSFLAGSB, combined TIOC[GS]FLAGS with bitmask to leave
 * parts of flags alone...
 */

INLINE static long
iocsflagsb (bdev, flags, mask, tty, t)
	int bdev;
	unsigned long flags;	/* new flags, hi bit set == read only */
	unsigned long mask;	/* what flags to change/read */
	struct tty *tty;
	struct bios_tty *t;
{
	unsigned long oflags;
	unsigned short *sgflags;
	unsigned long bits;
	unsigned char ucr, oucr;
	short flow;

	sgflags = &tty->sg.sg_flags;
	if (t)
		oflags = (((char *) t->irec)[0x20] << 12) & (T_TANDEM|T_RTSCTS);
	else
		oflags = *sgflags & (T_TANDEM|T_RTSCTS);
	if ((long)flags >= 0)
		/* clear unused bits */
		flags &= (TF_STOPBITS|TF_CHARBITS|TF_BRKINT|TF_CAR|
			T_RTSCTS|T_TANDEM|T_EVENP|T_ODDP);
	if (t && (mask & (TF_BRKINT|TF_CAR))) {
		if (t->brkint)
			oflags |= TF_BRKINT;
		if (!t->clocal)
			oflags |= TF_CAR;
		if ((long)flags >= 0) {
			if (mask & TF_CAR) {
				t->clocal = !(flags & TF_CAR);
/* update TS_BLIND without signalling */
				checkbtty_nsig (t);
				if ((bdev == 7 || tty == &scca_tty) &&
				    !(mask & (T_RTSCTS|T_TANDEM))) {
/* force rsconf be called so it can adjust w3 bit 5 (see xbios.c) */
					mask |= (T_RTSCTS|T_TANDEM);
					flags = (flags & ~(T_RTSCTS|T_TANDEM)) |
						(oflags & (T_RTSCTS|T_TANDEM));
				}
			} else {
				flags = (flags & ~TF_CAR) |
					(oflags & TF_CAR);
			}
			if (mask & TF_BRKINT) {
				t->brkint = flags & TF_BRKINT;
			} else {
				flags = (flags & ~TF_BRKINT) |
					(oflags & TF_BRKINT);
			}
		}
	} else {
		flags &= ~(TF_BRKINT|TF_CAR);
	}
	if (mask & (TF_FLAGS|TF_STOPBITS|TF_CHARBITS)) {
		int oldmap = curproc->bconmap;

		if (has_bconmap && bdev >= 6)
			curproc->bconmap = bdev;
		bits = rsconf(-1, -1, -1, -1, -1, -1);	/* get settings */
		oucr = ucr = (bits >> 24L) & 0x0ff;	/* isolate UCR byte */
		oflags |= (ucr >> 3) & (TF_STOPBITS|TF_CHARBITS);
		if (ucr & 0x4) {			/* parity on? */
			oflags |= (ucr & 0x2) ? T_EVENP : T_ODDP;
		}
		if ((long)flags >= 0) {
			if ((mask & (T_RTSCTS|T_TANDEM)) == (T_RTSCTS|T_TANDEM)) {
				flow = (flags & (T_RTSCTS|T_TANDEM)) >> 12L;
			} else {
				flow = -1;
				flags = (flags & ~(T_RTSCTS|T_TANDEM)) |
					(oflags & (T_RTSCTS|T_TANDEM));
			}
			if ((mask & (T_EVENP|T_ODDP)) == (T_EVENP|T_ODDP)) {
				if (flags & T_EVENP) {
					ucr |= 0x6;
					flags &= ~T_ODDP;
				} else if (flags & T_ODDP) {
					ucr &= ~2;
					ucr |= 0x4;
				} else {
					ucr &= ~6;
				}
			} else {
				flags = (flags & ~(T_EVENP|T_ODDP)) |
					(oflags & (T_EVENP|T_ODDP));
			}
			if ((mask & TF_STOPBITS) == TF_STOPBITS &&
			    (flags & TF_STOPBITS)) {
				ucr &= ~(0x18);
				ucr |= (flags & TF_STOPBITS) << 3;
			} else {
				flags = (flags & ~TF_STOPBITS) |
					(oflags & TF_STOPBITS);
			}
			if ((mask & TF_CHARBITS) == TF_CHARBITS) {
				ucr &= ~(0x60);
				ucr |= (flags & TF_CHARBITS) << 3;
			} else {
				flags = (flags & ~TF_CHARBITS) |
					(oflags & TF_CHARBITS);
			}
			if (ucr != oucr)
				rsconf(-1, flow, ucr, -1, -1, -1);
			else if (flow >= 0)
				rsconf(-1, flow, -1, -1, -1, -1);
			if (flow >= 0) {
				if (t)
					flags = (flags & ~(T_RTSCTS|T_TANDEM)) |
						((((char *) t->irec)[0x20] << 12) &
						 (T_RTSCTS|T_TANDEM));
				*sgflags &= ~(T_RTSCTS|T_TANDEM);
				*sgflags |= flags & (T_RTSCTS|T_TANDEM);
			}
		}
		curproc->bconmap = oldmap;
	}
	return (long)flags >= 0 ? flags : oflags;
}

static long ARGS_ON_STACK 
bios_ioctl(f, mode, buf)
	FILEPTR *f; int mode; void *buf;
{
	long *r = (long *)buf;
	struct winsize *ws;
	char *aline;
	short dev;
	int i;
	struct bios_file *b;
	struct bios_tty *t = BTTY(f);

	switch(mode) {
	case FIONREAD:
		if (t)
			*r = btty_ionread(t);
		else if (f->fc.aux == 3)
			*r = ionread(midi_btty.irec);
		else if (bconstat(f->fc.aux))
			*r = 1;
		else
			*r = 0;
		break;
	case FIONWRITE:
		if (t)
			*r = ionwrite (t->orec);
		else if (bcostat(f->fc.aux))
			*r = 1;
		else
			*r = 0;
		break;
	case FIOEXCEPT:
		*r = 0;
		break;
	case TIOCFLUSH:
	    {
		IOREC_T *ior;
		int flushtype;
		short sr;

		dev = f->fc.aux;

		if ((!r) || (!(*r & 3))) {
			flushtype = 3;
		} else {
			flushtype = (int) *r;
		}
		if (dev == 1 || dev >= 6) {
			b = (struct bios_file *)f->fc.index;
			if (t)
				ior = t->irec;
			else
				ior = (IOREC_T *) uiorec(0);
	/* just resetting iorec pointers here can hang a flow controlled port,
	 * iread can do better...
	 */
			if ((flushtype & 1) &&
			    iread (dev, (char *) NULL, 0, 1, b) == EUNDEV) {
				sr = spl7();
				ior->head = ior->tail = 0;
				spl(sr);
			}
	/* sender should be ok but iwrite also sets the dev's ctime */
			if ((flushtype & 2) &&
			    iwrite (dev, (char *) NULL, 0, 1, b) == EUNDEV) {
				ior++; /* output record */
				sr = spl7();
				ior->head = ior->tail = 0;
				spl(sr);
			}
		} else if (dev == 3 || dev == 2 || dev == 5) {
			if (dev == 3) {
				/* midi */
				ior = midi_btty.irec;
			} else {
				/* ikbd */
				ior = (IOREC_T *) uiorec(1);
			}
			if (flushtype & 1) {
				sr = spl7();
				ior->head = ior->tail = 0;
				spl(sr);
			}
		}
		return 0;
	    }
#if 1
#ifndef TIONOTSEND
#define TIONOTSEND (('T'<<8) | 134)
#endif
	case TIONOTSEND:
#endif
	case TIOCOUTQ:
	    {
		IOREC_T *ior;

		dev = f->fc.aux;

		if (dev == 1 || dev >= 6) {
			long ret;
#ifdef TIONOTSEND
/* try trap #1 first, to know about the last char too :) */
			if (t &&
			    (ret = rsvf_ioctl (t->tosfd, r, TIONOTSEND)) != EINVFN)
				return ret;
#endif
			if (t)
				ior = t->orec;
			else
				ior = (IOREC_T *) uiorec(0) + 1;
			*r = ior->tail - ior->head;
			if (*r < 0)
				*r += ior->buflen;
		}
 		else
			*r = 0;
		break;
	    }
	case TIOCGWINSZ:
		if (f->fc.aux == 2) {
			aline = lineA0();
			ws = (struct winsize *)buf;
			ws->ws_row = *((short *)(aline - 42)) + 1;
			ws->ws_col = *((short *)(aline - 44)) + 1;
		} else {
			return EINVFN;
		}
		break;
	case TIOCIBAUD:
	case TIOCOBAUD:
	    {
		long oldbaud, newbaud;
		int oldmap;

		dev = f->fc.aux;

		newbaud = *r;
		if (dev == 1 || dev >= 6) {
/* can we pass it to trap #1 and set speeds rsconf doesn't know about? */
			if (t &&
			    (oldbaud = rsvf_ioctl (t->tosfd, r, mode)) != EINVFN) {
/* looks good...  save result and return */
				if (newbaud < 0)
					newbaud = *r;
				if (newbaud && !oldbaud) {
					if (mode == TIOCIBAUD)
						t->ispeed = newbaud;
					else
						t->ospeed = newbaud;
				}
				t->vticks = 0;
				return oldbaud;
			}
/* trick rsconf into setting the correct port (it uses curproc->bconmap) */
			oldmap = curproc->bconmap;
			if (has_bconmap && dev >= 6)
				curproc->bconmap = dev;
			i = (int)rsconf(-2, -1, -1, -1, -1, -1);

			if (i < 0 || i >= MAXBAUD)
				oldbaud = -1L;
			else
				oldbaud = baudmap[i];
			*r = oldbaud;
			if (newbaud > 0) {
	/* assert DTR works only on modem1 and SCC lines */
				if (dev == 1 || dev == 6) {
					Offgibit(0xef);
				} else if (t && (dev == 7 ||
					   ((struct tty *)f->devinfo) == &scca_tty)) {
					scc_set5 ((volatile char *) (dev == 7 ?
						    0xffff8c85L : 0xffff8c81L),
						  1, (1 << 7), t->irec);
				}
				if (newbaud == oldbaud ||
				    ((struct tty *)f->devinfo)->hup_ospeed) {
					curproc->bconmap = oldmap;
					return 0;
				}
				for (i = 0; i < MAXBAUD; i++) {
					if (baudmap[i] == newbaud) {
						rsconf(i, -1, -1, -1, -1, -1);
						curproc->bconmap = oldmap;
						return 0;
					} else if (baudmap[i] < newbaud) {
						*r = baudmap[i];
						break;
					}
				}
				curproc->bconmap = oldmap;
				return ERANGE;
			} else if (newbaud == 0L) {
	/* drop DTR: works only on modem1 and SCC lines */
				if (dev == 1 || dev == 6) {
					Ongibit(0x10);
				} else if (t && (dev == 7 ||
					   ((struct tty *)f->devinfo) == &scca_tty)) {
					scc_set5 ((volatile char *) (dev == 7 ?
						    0xffff8c85L : 0xffff8c81L),
						  0, (1 << 7), t->irec);
				}
			}
			curproc->bconmap = oldmap;
			return 0;
		} else if (dev == 2 || dev == 5) {
			/* screen: assume 9600 baud */
			oldbaud = 9600L;
		} else if (dev == 3) {
			/* midi */
			oldbaud = 31250L;
		} else {
			oldbaud = -1L;	/* unknown speed */
		}
		*r = oldbaud;
		if (newbaud > 0 && newbaud != oldbaud)
			return ERANGE;
		break;
	    }
	case TIOCCBRK:
	case TIOCSBRK:
		dev = f->fc.aux;
		if (dev != 1 && dev < 6)
			return EINVFN;
		return iocsbrk (dev, mode, t);
	case TIOCSFLAGSB:
		dev = f->fc.aux;
		if (dev != 1 && dev < 6)
			return EINVFN;
		*((long *)buf) = iocsflagsb (dev, ((long *)buf)[0], ((long *)buf)[1],
					(struct tty *)f->devinfo, t);
		break;
	case TIOCGVMIN:
	case TIOCSVMIN:
	    {
		unsigned short *v = buf;
		struct tty *tty = (struct tty *)f->devinfo;

		if (f->fc.aux == 3)
			t = &midi_btty;
		if (!tty || !t)
			return EINVFN;
		if (mode == TIOCGVMIN) {
			v[0] = tty->vmin;
			v[1] = tty->vtime;
		} else {
			if (v[0] > t->irec->buflen/2)
				v[0] = t->irec->buflen/2;
			tty->vmin = v[0];
			tty->vtime = v[1];
			t->vticks = 0;
		}
		return 0;
	    }
	case TIOCWONLINE:
	    {
		struct tty *tty = (struct tty *)f->devinfo;

		if (!tty)
			return EINVFN;
		while (tty->state & TS_BLIND)
			sleep (IO_Q, (long)&tty->state);
		return 0;
	    }
	case TCURSOFF:
	case TCURSON:
	case TCURSBLINK:
	case TCURSSTEADY:
		if (f->fc.aux != 2)
			return EINVFN;
		return Cursconf(mode - TCURSOFF, 0);
	case TCURSSRATE:
	case TCURSGRATE:
	    {
		long r;

		if (f->fc.aux != 2)
			return EINVFN;
		r = Cursconf(mode - TCURSOFF, *((short *)buf));
		if (r >= 0 && mode == TCURSGRATE) {
			*(short *)buf = r;
			r = 0;
		}
		return r;
	    }
	case F_SETLK:
	case F_SETLKW:
	    {
		struct flock *lck = (struct flock *)buf;

		b = (struct bios_file *)f->fc.index;
		while (b->lockpid && b->lockpid != curproc->pid) {
			if (mode == F_SETLKW && lck->l_type != F_UNLCK)
				sleep(IO_Q, (long)b);
			else
				return ELOCKED;
		}
		if (lck->l_type == F_UNLCK) {
			if (!(f->flags & O_LOCK)) {
				DEBUG(("bios_ioctl: wrong file descriptor for UNLCK"));
				return ENSLOCK;
			}
			if (b->lockpid != curproc->pid)
				return ENSLOCK;
			b->lockpid = 0;
			f->flags &= ~O_LOCK;
			wake(IO_Q, (long)b);	/* wake anyone waiting for this lock */
		} else {
			b->lockpid = curproc->pid;
			f->flags |= O_LOCK;
		}
		break;
	    }
	case F_GETLK:
	    {
		struct flock *lck = (struct flock *)buf;

		b = (struct bios_file *)f->fc.index;
		if (b->lockpid) {
			lck->l_type = F_WRLCK;
			lck->l_start = lck->l_len = 0;
			lck->l_pid = b->lockpid;
		} else {
			lck->l_type = F_UNLCK;
		}
		break;
	    }
#ifndef TIOCCTLMAP
#define TIOCCTLMAP (('T'<<8) | 129)
#define TIOCCTLGET (('T'<<8) | 130)
#define TIOCCTLSET (('T'<<8) | 131)
#define TIONOTSEND (('T'<<8) | 134)
#define TIOCERROR (('T'<<8) | 135)
#endif
	case TIOCCTLMAP:
	case TIOCCTLGET:
	case TIOCCTLSET:
	case TIOCERROR:
	    {
		long ret;

		if (t &&
		    (ret = rsvf_ioctl (t->tosfd, r, mode)) != EINVFN) {
			if (mode == TIOCCTLMAP)
/* user processes can get signals but not callbacks from real interrupts... */
				r[1] = r[2] = 0;
			return ret;
		}
		/*FALLTHRU*/
	    }
	default:
	/* Fcntl will automatically call tty_ioctl to handle
	 * terminal calls that we didn't deal with
	 */
		return EINVFN;
	}
	return 0;
}

static long ARGS_ON_STACK 
bios_select(f, p, mode)
	FILEPTR *f; long p; int mode;
{
	struct tty *tty = (struct tty *)f->devinfo;
	int dev = f->fc.aux;

	if (mode == O_RDONLY) {
		struct bios_tty *t = &midi_btty;
		if (tty && (dev == 3 || ((t = BTTY(f))))) {
			if (!(tty->state & TS_BLIND) &&
			    (dev == 3 ? ionread(t->irec) :
					btty_ionread(t)) >= (long)tty->vmin) {
				return 1;
			}
		} else if (bconstat(dev)) {
			TRACE(("bios_select: data present for device %d", dev));
			return 1;
		}
		if (tty) {
		/* avoid collisions with other processes */
			if (tty->rsel)
				return 2;	/* collision */
			tty->rsel = p;
		}
		return 0;
	} else if (mode == O_WRONLY) {
		if ((!tty || !(tty->state & (TS_BLIND|TS_HOLD))) &&
		    (dev == 3 || bcostat(dev))) {
			TRACE(("bios_select: ready to output on %d", dev));
			return 1;
		}
		if (tty) {
			if (tty->wsel)
				return 2;	/* collision */
			tty->wsel = p;
		}
		return 0;
	}
	/* default -- we don't know this mode, return 0 */
	return 0;
}

static void ARGS_ON_STACK 
bios_unselect(f, p, mode)
	FILEPTR *f;
	long p;
	int mode;
{
	struct tty *tty = (struct tty *)f->devinfo;

	if (tty) {
		if (mode == O_RDONLY && tty->rsel == p)
			tty->rsel = 0;
		else if (mode == O_WRONLY && tty->wsel == p)
			tty->wsel = 0;
	}
}

static long ARGS_ON_STACK 
bios_close(f, pid)
	FILEPTR *f;
	int pid;
{
	struct tty *tty = (struct tty *)f->devinfo;
	struct bios_file *b;
	struct bios_tty *t;

	b = (struct bios_file *)f->fc.index;
	if ((f->flags & O_LOCK) && (b->lockpid == pid)) {
		b->lockpid = 0;
		f->flags &= ~O_LOCK;
		wake(IO_Q, (long)b);	/* wake anyone waiting for this lock */
	}
	if (tty && f->links <= 0 && f->pos)
/* f->pos used as flag that f came from Bconmap (/dev/aux) */
		tty->aux_cnt--;
	if (tty && !tty->use_cnt && ((t = BTTY(f)) && t->tosfd != EUNDEV)) {
		rsvf_close (t->tosfd);
		t->tosfd = EUNDEV;
	}
	return 0;
}

/*
 * mouse device driver
 */

#define MOUSESIZ 128*3
static unsigned char mousebuf[MOUSESIZ];
static int mousehead, mousetail;

long mousersel;	/* is someone calling select() on the mouse? */

char mshift;		/* shift key status; set by checkkeys() in bios.c */
short *gcurx = 0,
      *gcury = 0;	/* mouse pos. variables; used by big screen emulators */

void ARGS_ON_STACK 
mouse_handler(buf)
	const char *buf;	/* must be a *signed* character */
{
	unsigned char *mbuf, buttons;
	int newmtail;
	short dx, dy;

/* the Sun mouse driver has 0=down, 1=up, while the atari hardware gives
   us the reverse. also, we have the "middle" button and the "left"
   button reversed; so we use this table to convert (and also to add the
   0x80 to indicate a mouse packet)
 */
	static int _cnvrt[8] = {
		0x87, 0x86, 0x83, 0x82, 0x85, 0x84, 0x81, 0x80
	};

	mbuf = &mousebuf[mousetail];
	newmtail = mousetail + 3;
	if (newmtail >= MOUSESIZ)
		newmtail = 0;
	if (newmtail == mousehead)
		return;			/* buffer full */

	buttons = *buf++ & 0x7;		/* convert to SUN format */
	if (mshift & 0x3) {		/* a shift key held down? */
	/* if so, convert shift+button to a "middle" button */
		if (buttons == 0x1 || buttons == 0x2)
			buttons = 0x4;
		else if (buttons == 0x3)
			buttons = 0x7;
	}
	*mbuf++ = _cnvrt[buttons];	/* convert to Sun format */
	dx = *buf++;
	*mbuf++ = dx;			/* copy X delta */
	dy = *buf;
	*mbuf = -dy;			/* invert Y delta for Sun format */
	mousetail = newmtail;
	*gcurx += dx;			/* update line A variables */
	*gcury += dy;
/*
 * if someone has called select() waiting for mouse input, wake them
 * up
 */
	if (mousersel) {
		wakeselect(mousersel);
	}
}

extern void newmvec(), newjvec();	/* in intr.s */
static long oldvec = 0;
long oldjvec = 0;

static long ARGS_ON_STACK 
mouse_open(f)
	FILEPTR *f;
{
	char *aline;

	static char parameters[] = {
		0, 		/* Y=0 in lower corner */
		0,		/* normal button handling */
		1, 1		/* X, Y scaling factors */
	};

	UNUSED(f);

	if (oldvec)		/* mouse in use */
		return EACCDN;

/* initialize pointers to line A variables */
	if (!gcurx) {
		aline = lineA0();
		if (aline == 0)	{	/* should never happen */
			ALERT("unable to read line A variables");
			return -1;
		}
		gcurx = (short *)(aline - 0x25a);
		gcury = (short *)(aline - 0x258);
		*gcurx = *gcury = 32;	/* magic number -- what MGR uses */
	}

	oldvec = syskey->mousevec;
	oldjvec = syskey->joyvec;	/* jr: save old joystick vector */
	Initmous(1, parameters, newmvec);
	syskey->joyvec = (long)newjvec;	/* jr: set up new joystick handler */
	mousehead = mousetail = 0;
	return 0;
}

static long ARGS_ON_STACK 
mouse_close(f, pid)
	FILEPTR *f;
	int pid;
{
	static char parameters[] = {
		0, 		/* Y=0 in lower corner */
		0,		/* normal button handling */
		1, 1		/* X, Y scaling factors */
	};

	UNUSED(pid);
	if (!f) return EIHNDL;
	if (f->links <= 0) {
		if (!oldvec) {
			DEBUG(("Mouse not open!!"));
			return -1;
		}
		Initmous(1, parameters, (void *)oldvec);	/* gratuitous (void *) for Lattice */
		syskey->joyvec = oldjvec;	/* jr: restore old joystick handler */
		oldvec = 0;
	}
	return 0;
}

static long ARGS_ON_STACK 
mouse_read(f, buf, nbytes)
	FILEPTR *f;
	char *buf;
	long nbytes;
{
	long count = 0;
	int mhead;
	unsigned char *foo;
	struct bios_file *b = (struct bios_file *)f->fc.index;

	mhead = mousehead;
	foo = &mousebuf[mhead];

	if (mhead == mousetail) {
		if (f->flags & O_NDELAY)
			return 0;
		do {
			yield();
		} while (mhead == mousetail);
	}

	while ( (mhead != mousetail) && (nbytes > 0)) {
		*buf++ = *foo++;
		mhead++;
		if (mhead >= MOUSESIZ) {
			mhead = 0;
			foo = mousebuf;
		}
		count++;
		--nbytes;
	}
	mousehead = mhead;
	if (count > 0) {
		b->xattr.atime = timestamp;
		b->xattr.adate = datestamp;
	}
	return count;
}

static long ARGS_ON_STACK 
mouse_ioctl(f, mode, buf)
	FILEPTR *f;
	int mode;
	void *buf;
{
	long r;

	UNUSED(f);
	if (mode == FIONREAD) {
		r = mousetail - mousehead;
		if (r < 0) r += MOUSESIZ;
		*((long *)buf) = r;
	} else if (mode == FIONWRITE)
		*((long *)buf) = 0;
	else if (mode == FIOEXCEPT)
		*((long *)buf) = 0;
	else
		return EINVFN;
	return 0;
}

static long ARGS_ON_STACK 
mouse_select(f, p, mode)
	FILEPTR *f;
	long p;
	int mode;
{
	UNUSED(f);

	if (mode != O_RDONLY) {
		if (mode == O_WRONLY)
			return 1;	/* we can always take output :-) */
		else
			return 0;	/* but don't care for anything else */
	}

	if (mousetail - mousehead)
		return 1;	/* input waiting already */

	if (mousersel)
		return 2;	/* collision */
	mousersel = p;
	return 0;
}

static void ARGS_ON_STACK 
mouse_unselect(f, p, mode)
	FILEPTR *f;
	long p;
	int mode;
{
	UNUSED(f);

	if (mode == O_RDONLY && mousersel == p)
		mousersel = 0;
}


/*
 * UTILITY ROUTINE called by Bconmap() in xbios.c:
 * this sets handle -1 of process p to a file handle
 * that has BIOS device "dev". Returns 0 on failure,
 * non-zero on success.
 */

int
set_auxhandle(p, dev)
	PROC *p;
	int dev;
{
	FILEPTR *f;
	struct bios_file *b;

	f = new_fileptr();
	if (f) {
		struct tty *tty;
		f->links = 1;
		f->flags = O_RDWR;
		f->pos = 0;
		f->devinfo = 0;
		f->fc.fs = &bios_filesys;
		f->fc.aux = dev;
		f->fc.dev = BIOSDRV;
		for (b = broot; b; b = b->next) {
			if (b->private == dev &&
			    (b->device == &bios_tdevice ||
			     b->device == &bios_ndevice)) {
				f->fc.index = (long)b;
				f->dev = b->device;
				if (b->device != &fakedev)
					f->devinfo = (long)b->tty;
#if 1
/* don't close and reopen the same device again
 */
				if (p->aux && p->aux->fc.fs == &bios_filesys &&
				    p->aux->fc.index == f->fc.index) {
					f->links = 0;
					dispose_fileptr(f);
					return 1;
				}
#endif
				goto found_device;
			}
		}
		f->fc.index = 0;
		f->dev = &bios_ndevice;
found_device:
		if ((*f->dev->open)(f) < 0) {
			f->links = 0;
			dispose_fileptr(f);
			return 0;
		}
/* special code for opening a tty */
		if (NULL != (tty = (struct tty *)f->devinfo)) {
			extern struct tty default_tty;	/* in tty.c */

			tty->use_cnt++;
			while (tty->hup_ospeed) {
				sleep (IO_Q, (long)&tty->state);
			}
			/* first open for this device? */
			if (tty->use_cnt == 1) {
				short s = tty->state & TS_BLIND;
				*tty = default_tty;
				tty->state = s;
				tty->use_cnt = 1;
				tty_ioctl(f, TIOCSTART, 0);
			}
			tty->aux_cnt++;
			f->pos = 1;	/* flag for close to --aux_cnt */
		}
	} else {
/* no memory! use the fake FILEPTR we
 * set up in biosfs_init
 */
		f = defaultaux;
		f->links++;
	}

	(void)do_pclose(p, p->aux);
	p->aux = f;

	return 1;
}
