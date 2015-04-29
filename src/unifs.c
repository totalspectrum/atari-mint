/*
Copyright 1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
 */

/* a simple unified file system */

#include "mint.h"


extern FILESYS bios_filesys, proc_filesys, pipe_filesys, shm_filesys;

static long	ARGS_ON_STACK uni_root	P_((int drv, fcookie *fc));
static long	ARGS_ON_STACK uni_lookup	P_((fcookie *dir, const char *name, fcookie *fc));
static long	ARGS_ON_STACK uni_getxattr	P_((fcookie *fc, XATTR *xattr));
static long	ARGS_ON_STACK uni_chattr	P_((fcookie *fc, int attrib));
static long	ARGS_ON_STACK uni_chown	P_((fcookie *fc, int uid, int gid));
static long	ARGS_ON_STACK uni_chmode	P_((fcookie *fc, unsigned mode));
static long	ARGS_ON_STACK uni_rmdir	P_((fcookie *dir, const char *name));
static long	ARGS_ON_STACK uni_remove	P_((fcookie *dir, const char *name));
static long	ARGS_ON_STACK uni_getname	P_((fcookie *root, fcookie *dir,
						    char *pathname, int size));
static long	ARGS_ON_STACK uni_rename	P_((fcookie *olddir, char *oldname,
				    fcookie *newdir, const char *newname));
static long	ARGS_ON_STACK uni_opendir	P_((DIR *dirh, int flags));
static long	ARGS_ON_STACK uni_readdir	P_((DIR *dirh, char *nm, int nmlen, fcookie *));
static long	ARGS_ON_STACK uni_rewinddir	P_((DIR *dirh));
static long	ARGS_ON_STACK uni_closedir	P_((DIR *dirh));
static long	ARGS_ON_STACK uni_pathconf	P_((fcookie *dir, int which));
static long	ARGS_ON_STACK uni_dfree	P_((fcookie *dir, long *buf));
static DEVDRV *	ARGS_ON_STACK uni_getdev	P_((fcookie *fc, long *devsp));
static long	ARGS_ON_STACK uni_symlink	P_((fcookie *dir, const char *name, const char *to));
static long	ARGS_ON_STACK uni_readlink	P_((fcookie *fc, char *buf, int buflen));
static long	ARGS_ON_STACK uni_fscntl	P_((fcookie *dir, const char *name, int cmd, long arg));

FILESYS uni_filesys = {
	(FILESYS *)0,
	FS_LONGPATH,
	uni_root,
	uni_lookup, nocreat, uni_getdev, uni_getxattr,
	uni_chattr, uni_chown, uni_chmode,
	nomkdir, uni_rmdir, uni_remove, uni_getname, uni_rename,
	uni_opendir, uni_readdir, uni_rewinddir, uni_closedir,
	uni_pathconf, uni_dfree, nowritelabel, noreadlabel,
	uni_symlink, uni_readlink, nohardlink, uni_fscntl, nodskchng
};

/*
 * structure that holds files
 * if (mode & S_IFMT == S_IFDIR), then this is an alias for a drive:
 *	"dev" holds the appropriate BIOS device number, and
 *	"data" is meaningless
 * if (mode & S_IFMT == S_IFLNK), then this is a symbolic link:
 *	"dev" holds the user id of the owner, and
 *	"data" points to the actual link data
 */

typedef struct unifile {
	char name[NAME_MAX+1];
	ushort mode;
	ushort dev;
	FILESYS *fs;
	void *data;
	struct unifile *next;
	short cdate, ctime;
} UNIFILE;

static UNIFILE u_drvs[UNI_NUM_DRVS];
static UNIFILE *u_root = 0;

static long	do_ulookup	P_((fcookie *, const char *, fcookie *, UNIFILE **));

FILESYS *
get_filesys (dev)
     int dev;
{
  UNIFILE *u;

  for (u = u_root; u; u = u->next)
    if (u->dev == dev)
      return u->fs;
  return (FILESYS *) 0L;
}

void
unifs_init()
{
	UNIFILE *u = u_drvs;
	int i;

	u_root = u;
	for (i = 0; i < UNI_NUM_DRVS; i++,u++) {
		u->next = u+1;
		u->mode = S_IFDIR|DEFAULT_DIRMODE;
		u->dev = i;
		u->cdate = datestamp;
		u->ctime = timestamp;
		if (i == PROCDRV) {
			strcpy(u->name, "proc");
			u->fs = &proc_filesys;
		} else if (i == PIPEDRV) {
			strcpy(u->name, "pipe");
			u->fs = &pipe_filesys;
		} else if (i == BIOSDRV) {
			strcpy(u->name, "dev");
			u->fs = &bios_filesys;
		} else if (i == UNIDRV) {
			(u-1)->next = u->next;	/* skip this drive */
		} else if (i == SHMDRV) {
			strcpy(u->name, "shm");
			u->fs = &shm_filesys;
		} else {
			u->name[0] = i + 'a';
			u->name[1] = 0;
			u->fs = 0;
		}
	}
	--u;	/* oops, we went too far */
	u->next = 0;
}

static long ARGS_ON_STACK 
uni_root(drv, fc)
	int drv;
	fcookie *fc;
{
	if (drv == UNIDRV) {
		fc->fs = &uni_filesys;
		fc->dev = drv;
		fc->index = 0L;
		return 0;
	}
	fc->fs = 0;
	return EINTRN;
}

static long ARGS_ON_STACK 
uni_lookup(dir, name, fc)
	fcookie *dir;
	const char *name;
	fcookie *fc;
{
	return do_ulookup(dir, name, fc, (UNIFILE **)0);
}

/* worker function for uni_lookup; can also return the UNIFILE
 * pointer for the root directory
 */
static long
do_ulookup(dir, name, fc, up)
	fcookie *dir;
	const char *name;
	fcookie *fc;
	UNIFILE **up;
{
	UNIFILE *u;
	long drvs;
	FILESYS *fs;
	fcookie *tmp;
	extern long dosdrvs;

	TRACE(("uni_lookup(%s)", name));

	if (dir->index != 0) {
		DEBUG(("uni_lookup: bad directory"));
		return EPTHNF;
	}
/* special case: an empty name in a directory means that directory */
/* so do "." and ".." */

	if (!*name || !strcmp(name, ".") || !strcmp(name, "..")) {
		dup_cookie(fc, dir);
		return 0;
	}
	drvs = drvmap() | dosdrvs | PSEUDODRVS;
/*
 * OK, check the list of aliases and special directories
 */
	for (u = u_root; u; u = u->next) {
		if (!strnicmp(name, u->name, NAME_MAX)) {
			if ( (u->mode & S_IFMT) == S_IFDIR ) {
				if (u->dev >= NUM_DRIVES) {
					fs = u->fs;
					if (up) *up = u;
					return (*fs->root)(u->dev,fc);
				}
				if ((drvs & (1L << u->dev)) == 0)
					return EPTHNF;
				tmp = &curproc->root[u->dev];
				if (!tmp->fs) {		/* drive changed? */
					changedrv(tmp->dev);
					tmp = &curproc->root[u->dev];
					if (!tmp->fs)
						return EPTHNF;
				}
				dup_cookie(fc, tmp);
			} else {		/* a symbolic link */
				fc->fs = &uni_filesys;
				fc->dev = UNIDRV;
				fc->index = (long)u;
			}
			if (up) *up = u;
			return 0;
		}
	}
	DEBUG(("uni_lookup: name (%s) not found", name));
	return EFILNF;
}

static long ARGS_ON_STACK 
uni_getxattr(fc, xattr)
	fcookie *fc;
	XATTR *xattr;
{
	UNIFILE *u = (UNIFILE *)fc->index;

	if (fc->fs != &uni_filesys) {
		ALERT("ERROR: wrong file system getxattr called");
		return EINTRN;
	}

	xattr->index = fc->index;
	xattr->dev = xattr->rdev = fc->dev;
	xattr->nlink = 1;
	xattr->blksize = 1;

/* If "u" is null, then we have the root directory, otherwise
 * we use the UNIFILE structure to get the info about it
 */
	if (!u || ( (u->mode & S_IFMT) == S_IFDIR )) {
		xattr->uid = xattr->gid = 0;
		xattr->size = xattr->nblocks = 0;
		xattr->mode = S_IFDIR | DEFAULT_DIRMODE;
		xattr->attr = FA_DIR;
	} else {
		xattr->uid = u->dev;
		xattr->gid = 0;
		xattr->size = xattr->nblocks = strlen(u->data) + 1;
		xattr->mode = u->mode;
		xattr->attr = 0;
	}
	xattr->mtime = xattr->atime = xattr->ctime = u->ctime;
	xattr->mdate = xattr->adate = xattr->cdate = u->cdate;
	return 0;
}

static long ARGS_ON_STACK 
uni_chattr(dir, attrib)
	fcookie *dir;
	int attrib;
{
	UNUSED(dir); UNUSED(attrib);
	return EACCDN;
}

static long ARGS_ON_STACK 
uni_chown(dir, uid, gid)
	fcookie *dir;
	int uid, gid;
{
	UNUSED(dir); UNUSED(uid);
	UNUSED(gid);
	return EINVFN;
}

static long ARGS_ON_STACK 
uni_chmode(dir, mode)
	fcookie *dir;
	unsigned mode;
{
	UNUSED(dir);
	UNUSED(mode);
	return EINVFN;
}

static long ARGS_ON_STACK 
uni_rmdir(dir, name)
	fcookie *dir;
	const char *name;
{
	long r;

	r = uni_remove(dir, name);
	if (r == EFILNF) r = EPTHNF;
	return r;
}

static long ARGS_ON_STACK 
uni_remove(dir, name)
	fcookie *dir;
	const char *name;
{
	UNIFILE *u, *lastu;

	UNUSED(dir);

	lastu = 0;
	u = u_root;
	while (u) {
		if (!strnicmp(u->name, name, NAME_MAX)) {
			if ( (u->mode & S_IFMT) != S_IFLNK ) return EFILNF;
			if (curproc->euid && (u->dev != curproc->euid))
				return EACCDN;
			kfree(u->data);
			if (lastu)
				lastu->next = u->next;
			else
				u_root = u->next;
			kfree(u);
			return 0;
		}
		lastu = u;
		u = u->next;
	}
	return EFILNF;
}

static long ARGS_ON_STACK 
uni_getname(root, dir, pathname, size)
	fcookie *root, *dir; char *pathname;
	int size;
{
	FILESYS *fs;
	UNIFILE *u;
	char *n;
	fcookie relto;
	char tmppath[PATH_MAX];
	long r;

	UNUSED(root);

	if (size <= 0) return ERANGE;

	fs = dir->fs;
	if (dir->dev == UNIDRV) {
		*pathname = 0;
		return 0;
	}

	for (u = u_root; u; u = u->next) {
		if (dir->dev == u->dev && (u->mode & S_IFMT) == S_IFDIR) {
			*pathname++ = '\\';
			if (--size <= 0) return ERANGE;
			for (n = u->name; *n; ) {
				*pathname++ = *n++;
				if (--size <= 0) return ERANGE;
			}
			break;
		}
	}

	if (!u) {
		ALERT("unifs: couldn't match a drive with a directory");
		return EPTHNF;
	}

	if (dir->dev >= NUM_DRIVES) {
		if ((*fs->root)(dir->dev, &relto) == 0) {
			if (!(fs->fsflags & FS_LONGPATH)) {
				r = (*fs->getname)(&relto, dir, tmppath, PATH_MAX);
				release_cookie(&relto);
				if (r) {
					return r;
				}
				if (strlen(tmppath) < size) {
					strcpy(pathname, tmppath);
					return 0;
				} else {
					return ERANGE;
				}
			}
			r = (*fs->getname)(&relto, dir, pathname, size);
			release_cookie(&relto);
			return r;
		} else {
			*pathname = 0;
			return EINTRN;
		}
	}

	if (curproc->root[dir->dev].fs != fs) {
		ALERT("unifs: drive's file system doesn't match directory's");
		return EINTRN;
	}

	if (!fs) {
		*pathname = 0;
		return 0;
	}
	if (!(fs->fsflags & FS_LONGPATH)) {
		r = (*fs->getname)(&curproc->root[dir->dev], dir, tmppath, PATH_MAX);
		if (r) return r;
		if (strlen(tmppath) < size) {
			strcpy(pathname, tmppath);
			return 0;
		} else {
			return ERANGE;
		}
	}
	return (*fs->getname)(&curproc->root[dir->dev], dir, pathname, size);
}

static long ARGS_ON_STACK 
uni_rename(olddir, oldname, newdir, newname)
	fcookie *olddir;
	char *oldname;
	fcookie *newdir;
	const char *newname;
{
	UNIFILE *u;
	fcookie fc;
	long r;

	UNUSED(olddir);

	for (u = u_root; u; u = u->next) {
		if (!strnicmp(u->name, oldname, NAME_MAX))
			break;
	}

	if (!u) {
		DEBUG(("uni_rename: old file not found"));
		return EFILNF;
	}

/* the new name is not allowed to exist! */
	r = uni_lookup(newdir, newname, &fc);
	if (r == 0)
		release_cookie(&fc);

	if (r != EFILNF) {
		DEBUG(("uni_rename: error %ld", r));
		return (r == 0) ? EACCDN : r;
	}

	(void)strncpy(u->name, newname, NAME_MAX);
	return 0;
}

static long ARGS_ON_STACK 
uni_opendir(dirh, flags)
	DIR *dirh;
	int flags;
{
	UNUSED(flags);

	if (dirh->fc.index != 0) {
		DEBUG(("uni_opendir: bad directory"));
		return EPTHNF;
	}
	dirh->index = 0;
	return 0;
}


static long ARGS_ON_STACK 
uni_readdir(dirh, name, namelen, fc)
	DIR *dirh;
	char *name;
	int namelen;
	fcookie *fc;
{
	long map;
	char *dirname;
	int i;
	int giveindex = (dirh->flags == 0);
	UNIFILE *u;
	long index;
	extern long dosdrvs;
	long r;

	map = dosdrvs | drvmap() | PSEUDODRVS;
	i = dirh->index++;
	u = u_root;
	while (i > 0) {
		--i;
		u = u->next;
		if (!u)
			break;
	}
tryagain:
	if (!u) return ENMFIL;

	dirname = u->name;
	index = (long)u;
	if ( (u->mode & S_IFMT) == S_IFDIR ) {
/* make sure the drive really exists */
		if ( u->dev >= NUM_DRIVES) {
		    r = (*u->fs->root)(u->dev,fc);
		    if (r) {
			fc->fs = &uni_filesys;
			fc->index = 0;
			fc->dev = u->dev;
		    }
		} else {
		    if ((map & (1L << u->dev)) == 0 ) {
			dirh->index++;
			u = u->next;
			goto tryagain;
		    }
		    dup_cookie(fc, &curproc->root[u->dev]);
		    if (!fc->fs) {	/* drive not yet initialized */
		/* use default attributes */
			fc->fs = &uni_filesys;
			fc->index = 0;
			fc->dev = u->dev;
		    }
		}
	} else {		/* a symbolic link */
		fc->fs = &uni_filesys;
		fc->dev = UNIDRV;
		fc->index = (long)u;
	}

	if (giveindex) {
		namelen -= (int)sizeof(long);
		if (namelen <= 0) {
			release_cookie(fc);
			return ERANGE;
		}
		*((long *)name) = index;
		name += sizeof(long);
	}
	strncpy(name, dirname, namelen-1);
	if (strlen(name) < strlen(dirname)) {
		release_cookie(fc);
		return ENAMETOOLONG;
	}
	return 0;
}

static long ARGS_ON_STACK 
uni_rewinddir(dirh)
	DIR *dirh;
{
	dirh->index = 0;
	return 0;
}

static long ARGS_ON_STACK 
uni_closedir(dirh)
	DIR *dirh;
{
	UNUSED(dirh);
	return 0;
}

static long ARGS_ON_STACK 
uni_pathconf(dir, which)
	fcookie *dir;
	int which;
{
	UNUSED(dir);

	switch(which) {
	case -1:
		return DP_MAXREQ;
	case DP_IOPEN:
		return 0;		/* no files to open */
	case DP_MAXLINKS:
		return 1;		/* no hard links available */
	case DP_PATHMAX:
		return PATH_MAX;
	case DP_NAMEMAX:
		return NAME_MAX;
	case DP_ATOMIC:
		return 1;		/* no atomic writes */
	case DP_TRUNC:
		return DP_AUTOTRUNC;
	case DP_CASE:
		return DP_CASEINSENS;
	case DP_MODEATTR:
		return DP_FT_DIR|DP_FT_LNK;
	case DP_XATTRFIELDS:
		return DP_INDEX|DP_DEV|DP_NLINK|DP_SIZE;
	default:
		return EINVFN;
	}
}

static long ARGS_ON_STACK 
uni_dfree(dir, buf)
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

static DEVDRV * ARGS_ON_STACK 
uni_getdev(fc, devsp)
	fcookie *fc;
	long *devsp;
{
	UNUSED(fc);

	*devsp = EACCDN;
	return 0;
}

static long ARGS_ON_STACK 
uni_symlink(dir, name, to)
	fcookie *dir;
	const char *name;
	const char *to;
{
	UNIFILE *u;
	fcookie fc;
	long r;

	r = uni_lookup(dir, name, &fc);
	if (r == 0) {
		release_cookie(&fc);
		return EACCDN;	/* file already exists */
	}
	if (r != EFILNF) return r;	/* some other error */

	if (curproc->egid)
		return EACCDN;	/* only members of admin group may do that */

	u = kmalloc(SIZEOF(UNIFILE));
	if (!u) return EACCDN;

	strncpy(u->name, name, NAME_MAX);
	u->name[NAME_MAX] = 0;

	u->data = kmalloc((long)strlen(to)+1);
	if (!u->data) {
		kfree(u);
		return EACCDN;
	}
	strcpy(u->data, to);
	u->mode = S_IFLNK | DEFAULT_DIRMODE;
	u->dev = curproc->euid;
	u->next = u_root;
	u->fs = &uni_filesys;
	u->cdate = datestamp;
	u->ctime = timestamp;
	u_root = u;
	return 0;
}

static long ARGS_ON_STACK 
uni_readlink(fc, buf, buflen)
	fcookie *fc;
	char *buf;
	int buflen;
{
	UNIFILE *u;

	u = (UNIFILE *)fc->index;
	assert(u);
	assert((u->mode & S_IFMT) == S_IFLNK);
	assert(u->data);
	strncpy(buf, u->data, buflen);
	if (strlen(u->data) >= buflen)
		return ENAMETOOLONG;
	return 0;
}




/* uk: use these Dcntl's to install a new filesystem which is only visible
 *     on drive u:
 *
 *     FS_INSTALL:   let the kernel know about the file system; it does NOT
 *                   get a device number.
 *     FS_MOUNT:     use Dcntl(FS_MOUNT, "u:\\foo", &descr) to make a directory
 *                   foo where the filesytem resides in; the file system now
 *                   gets its device number which is also written into the
 *                   dev_no field of the fs_descr structure.
 *     FS_UNMOUNT:   remove a file system's directory; this call closes all
 *                   open files, directory searches and directories on this
 *                   device. Make sure that the FS will not recognise any
 *                   accesses to this device, as fs->root will be called
 *                   during the reinitalisation!
 *     FS_UNINSTALL: remove a file system completely from the kernel list,
 *                   but that will only be possible if there is no directory
 *                   associated with this file system.
 *                   This function allows it to write file systems as demons
 *                   which stay in memory only as long as needed.
 *
 * BUG: it is not possible yet to lock such a filesystem.
 */

/* here we start with gemdos only file system device numbers */
static curr_dev_no = 0x100;



static long ARGS_ON_STACK
uni_fscntl(dir, name, cmd, arg)
	fcookie *dir;
	const char *name;
	int cmd;
	long arg;
{
	fcookie fc;
	long r;

	extern struct kerinfo kernelinfo;
	extern FILESYS *active_fs;

	if (cmd == (int)FS_INSTALL) { /* install a new filesystem */
		struct fs_descr *d = (struct fs_descr*)arg;
		FILESYS *fs;

	/* check if FS is installed already */
		for (fs = active_fs;  fs;  fs = fs->next)
			if (d->file_system == fs)  return 0L;
	/* include new file system into chain of file systems */
		d->file_system->next = active_fs;
		active_fs = d->file_system;
		return (long)&kernelinfo;  /* return pointer to kernel info as OK */
	} else if (cmd == (int)FS_MOUNT) {  /* install a new gemdos-only device for this FS */
		struct fs_descr *d = (struct fs_descr*)arg;
		FILESYS *fs;
		UNIFILE *u;

	/* first check for existing names */
		r = uni_lookup(dir, name, &fc);
		if (r == 0) {
			release_cookie(&fc);
			return EACCDN;   /* name exists already */
		}
		if (r != EFILNF) return r; /* some other error */
		if (!d) return EACCDN;
		if (!d->file_system) return EACCDN;
	/* check if FS is installed */
		for (fs = active_fs;  fs;  fs = fs->next)
			if (d->file_system == fs)  break;
		if (!fs) return EACCDN;  /* not installed, so return an error */
		u = kmalloc(SIZEOF(UNIFILE));
		if (!u) return EACCDN;
		strncpy(u->name, name, NAME_MAX);
		u->name[NAME_MAX] = 0;
		u->mode = S_IFDIR|DEFAULT_DIRMODE;
		u->data = 0;
		u->fs = d->file_system;
	/* now get the file system its own device number */
		u->dev = d->dev_no = curr_dev_no++;
	/* chain new entry into unifile list */
		u->next = u_root;
		u_root = u;
		return (long)u->dev;
	} else if (cmd == (int)FS_UNMOUNT) {  /* remove a file system's directory */
		struct fs_descr *d = (struct fs_descr*)arg;
		FILESYS *fs;
		UNIFILE *u;

	/* first check that directory exists */
	/* use special uni_lookup mode to get the unifile entry */
		r = do_ulookup(dir, name, &fc, &u);
		if (r != 0)  return EFILNF;   /* name does not exist */
		if (!d) return EFILNF;
		if (!d->file_system) return EFILNF;
		if (d->file_system != fc.fs)
			return EFILNF;  /* not the right name! */
		release_cookie(&fc);

		if (!u || (u->fs != d->file_system))
			return EFILNF;
	/* check if FS is installed */
		for (fs = active_fs;  fs;  fs = fs->next)
			if (d->file_system == fs)  break;
		if (!fs) return EACCDN;  /* not installed, so return an error */

	/* here comes the difficult part: we have to close all files on that
	 * device, so we have to call changedrv(). The file system driver
	 * has to make sure that further calls to fs.root() with this device
	 * number will fail!
	 *
	 * Kludge: mark the directory as a link, so uni_remove will remove it.
	 */
		changedrv(u->dev);
		u->mode &= ~S_IFMT;
		u->mode |= S_IFLNK;
		return uni_remove(dir, name);
	} else if (cmd == (int)FS_UNINSTALL) {    /* remove file system from kernel list */
		struct fs_descr *d = (struct fs_descr*)arg;
		FILESYS *fs, *last_fs;
		UNIFILE *u;

	/* first check if there are any files or directories associated with
	 * this file system
	 */
		for (u = u_root;  u;  u = u->next)
			if (u->fs == d->file_system)
				return EACCDN;   /* we cannot remove it before unmount */
		last_fs = 0;
		fs = active_fs;
		while (fs)  {   /* go through the list and remove the file system */
			if (fs == d->file_system)  {
				if (last_fs)
					last_fs->next = fs->next;
				else
					active_fs = fs->next;
				d->file_system->next = 0;
				return 0;
			}
			last_fs = fs;
			fs = fs->next;
		}
		return EFILNF;
	} else {
	/* see if we should just pass this along to another file system */
		r = uni_lookup(dir, name, &fc);
		if (r == 0) {
			if (fc.fs != &uni_filesys) {
				r = (*fc.fs->fscntl)(&fc, ".", cmd, arg);
				release_cookie(&fc);
				return r;
			}
			release_cookie(&fc);
		}
	}
	return EINVFN;
}
