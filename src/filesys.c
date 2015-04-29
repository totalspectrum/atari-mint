/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corp.
All rights reserved.
*/

/*
 * various file system interface things
 */

#include "mint.h"

#define PATH2COOKIE_DB(x) TRACE(x)

FILESYS *active_fs;
FILESYS *drives[NUM_DRIVES];
extern FILESYS tos_filesys;	/* declaration needed for debugging only */

/* "aliased" drives are different names
 * for real drives/directories
 * if drive d is an alias for c:\usr,
 * then alias_drv[3] == 2 (the real
 * drive) and aliases has bit (1L << 3)
 * set.
 * NOTE: if aliasdrv[d] is 0, then d is not an aliased drive,
 * otherwise d is aliased to drive aliasdrv[d]-1
 * (e.g. if drive A: is aliased to B:\FOO, then
 * aliasdrv[0] == 'B'-'A'+1 == 2). Always remember to
 * compensate for the extra 1 when dereferencing aliasdrv!
 */
int	aliasdrv[NUM_DRIVES];

FILEPTR *flist;		/* a list of free file pointers */

/* vector of valid drives, according to GEMDOS */
/* note that this isn't necessarily the same as what the BIOS thinks of
 * as valid
 */
long dosdrvs;

/*
 * Initialize a specific drive. This is called whenever a new drive
 * is accessed, or when media change occurs on an old drive.
 * Assumption: at this point, active_fs is a valid pointer
 * to a list of file systems.
 */

/* table of processes holding locks on drives */
extern PROC *dlockproc[];	/* in dosdir.c */

void
init_drive(i)
	int i;
{
	long r;
	FILESYS *fs;
	fcookie root_dir;

	TRACE(("init_drive(%c)", i+'A'));

	drives[i] = 0;		/* no file system */
	if (i >= 0 && i < NUM_DRIVES) {
		if (dlockproc[i]) return;
	}

	for (fs = active_fs; fs; fs = fs->next) {
		r = (*fs->root)(i, &root_dir);
		if (r == 0) {
			drives[i] = root_dir.fs;
			release_cookie(&root_dir);
			break;
		}
	}
}

/*
 * initialize the file system
 */

#define NUMFPS	40	/* initial number of file pointers */

void
init_filesys()
{
	static FILEPTR initial[NUMFPS+1];
	int i;
	extern FILESYS tos_filesys, bios_filesys, pipe_filesys,
		proc_filesys, uni_filesys;

/* get the vector of connected GEMDOS drives */
	dosdrvs = Dsetdrv(Dgetdrv()) | drvmap();

/* set up some initial file pointers */
	for (i = 0; i < NUMFPS; i++) {
		initial[i].devinfo = (ulong) (&initial[i+1]);
	}
	initial[NUMFPS].devinfo = 0;
	flist = initial;

/* set up the file systems */
	tos_filesys.next = 0;
	bios_filesys.next = &tos_filesys;
	pipe_filesys.next = &bios_filesys;
	proc_filesys.next = &pipe_filesys;
	uni_filesys.next = &proc_filesys;

	active_fs = &uni_filesys;

/* initialize the BIOS file system */
	biosfs_init();

/* initialize the unified file system */
	unifs_init();
}

/*
 * load file systems from disk
 * this routine is called after process 0 is set up, but before any user
 * processes are run
 *
 * NOTE that a number of directory changes take place here: we look first
 * in the current directory, then in the directory \mint.
 */

typedef FILESYS * ARGS_ON_STACK (*FSFUNC) P_((struct kerinfo *));

/* uk: made this lie outside of functions, as load_filesys() and
 *     load_devdriver() need access to it.
 */
#define NPATHS 3
static const char *const ext_paths[NPATHS] = {"", "\\MINT", "\\MULTITOS"};

void
load_filesys()
{
	long r;
	BASEPAGE *b;
	FILESYS *fs;
	FSFUNC initf;
	static DTABUF dta;
	int i;
	extern struct kerinfo kernelinfo; /* in main.c */
	char curpath[PATH_MAX];
	MEMREGION *xfsreg;

	curproc->dta = &dta;
	d_getpath(curpath,0);

	for (i = 0; i < NPATHS; i++) {
	    if (*ext_paths[i]) {
/* don't bother checking the current directory twice! */
		    if (!stricmp(ext_paths[i],curpath))
			r = -1;
		    else
			r = d_setpath(ext_paths[i]);
	    }
	    else
		    r = 0;

	    if (r == 0)
		    r = f_sfirst("*.xfs", 0);

	    while (r == 0) {
		b = (BASEPAGE *)p_exec(3, dta.dta_name, (char *)"", (char *)0);
		if ( ((long)b) < 0 ) {
			DEBUG(("Error loading file system %s", dta.dta_name));
			r = f_snext();
			continue;
		}
	/* we leave a little bit of slop at the end of the loaded stuff */
		m_shrink(0, (virtaddr)b, 512 + b->p_tlen + b->p_dlen + b->p_blen);
		initf = (FSFUNC)b->p_tbase;
		TRACE(("initializing %s", dta.dta_name));
		fs = (*initf)(&kernelinfo);

		if (fs) {
			TRACE(("%s loaded OK", dta.dta_name));
	/* put the loaded XFS into super accesible memory */
			xfsreg = addr2region( (long) b );
			mark_region(xfsreg, PROT_S);

	/* link it into the list of drivers */
	/* uk: but only if it has not installed itself via Dcntl()
	 *     after checking if file system is already installed,
	 *     so we know for sure that each file system in at most
	 *     once in the chain (important for removal!)
	 * also note: this doesn't preclude loading two different
	 * instances of the same file system driver, e.g. it's perfectly
	 * OK to have a "cdromy1.xfs" and "cdromz2.xfs"; the check below
	 * just makes sure that a given instance of a file system is
	 * installed at most once. I.e., it prevents cdromy1.xfs from being
	 * installed twice.
	 */
			if ((FILESYS*)1L != fs) {
				FILESYS *f = active_fs;
				for (;  f;  f = f->next)
					if (f == fs)
						break;
				if (!f) {   /* we ran completly through the list */
					fs->next = active_fs;
					active_fs = fs;
				}
			}
		} else {
			DEBUG(("%s returned null", dta.dta_name));
			m_free((virtaddr)b);
		}
		r = f_snext();
	    }
	}

#if 0
/* here, we invalidate all old drives EXCEPT for ones we're already using (at
 * this point, only the bios devices should be open)
 * this gives newly loaded file systems a chance to replace the
 * default tosfs.c
 */
	for (i = 0; i < NUM_DRIVES; i++) {
		if (d_lock(1, i) == 0)	/* lock if possible */
			d_lock(0, i);	/* and then unlock */
	}
#endif
}


/*
 * uk: load device driver in files called *.xdd (external device driver)
 *     from disk
 * maybe this should go into biosfs.c ??
 *
 * this routine is called after process 0 is set up, but before any user
 * processes are run, but before the loadable file systems come in,
 * so they can make use of external device drivers
 *
 * NOTE that a number of directory changes take place here: we look first
 * in the current directory, then in the directory \mint, and finally
 * the d_lock() calls force us into the root directory.
 * ??? what d_lock() calls ???
 */

typedef DEVDRV * ARGS_ON_STACK (*DEVFUNC) P_((struct kerinfo *));

#define DEV_SELFINST  ((DEVDRV*)1L)  /* dev driver did dcntl() already */

void
load_devdriver()
{
	long r;
	BASEPAGE *b;
	DEVDRV *dev;
	DEVFUNC initf;
	struct dev_descr the_dev;
	static DTABUF dta;
	int i;
	extern struct kerinfo kernelinfo; /* in main.c */
	char curpath[PATH_MAX];
	char dev_name[PATH_MAX];  /* a bit long, but one never knows... */
	char ch, *p;
	MEMREGION *xddreg;


	curproc->dta = &dta;
	d_getpath(curpath,0);

	for (i = 0; i < NPATHS; i++) {
	    if (*ext_paths[i]) {
/* don't bother checking the current directory twice! */
		    if (!stricmp(ext_paths[i],curpath))
			r = -1;
		    else
			r = d_setpath(ext_paths[i]);
	    }
	    else
		    r = 0;

	    if (r == 0)
		    r = f_sfirst("*.xdd", 0);

	    while (r == 0) {
		b = (BASEPAGE *)p_exec(3, dta.dta_name, (char *)"", (char *)0);
		if ( ((long)b) < 0 ) {
			DEBUG(("Error loading device driver %s", dta.dta_name));
			r = f_snext();
			continue;
		}
	/* we leave a little bit of slop at the end of the loaded stuff */
		m_shrink(0, (virtaddr)b, 512 + b->p_tlen + b->p_dlen + b->p_blen);
		initf = (DEVFUNC)b->p_tbase;
		TRACE(("initializing %s", dta.dta_name));
		dev = (*initf)(&kernelinfo);

		if (dev) {
			if (DEV_SELFINST != dev) {
	/* we need to install the device driver ourselves */
				the_dev.driver = dev;
				the_dev.dinfo = 0;
				the_dev.flags = 0;
				the_dev.tty = (struct tty*)0L;
				the_dev.reserved[0] = the_dev.reserved[1] = 0;
				the_dev.reserved[2] = 0;
				p = dta.dta_name;
	/* copy the dev. driver name, converting to lower case */
				while (*p && *p != '.') {
					*p = tolower(*p);
					p++;
				}
				ch = *p;
				*p = '\0';  /* we dont want the extension */
				strcpy(dev_name, "u:\\dev\\");
				strcat(dev_name, dta.dta_name);
				*p = ch;
				r = d_cntl(DEV_INSTALL, dev_name, (long)&the_dev);
				if (r <= 0) {
					DEBUG(("Error installing device driver %s", dta.dta_name));
					r = f_snext();
					continue;
				}
			}
			TRACE(("%s loaded OK", dta.dta_name));
	/* put the loaded XDD into super accesible memory */
			xddreg = addr2region( (long) b );
			mark_region(xddreg, PROT_S);
		} else {
			DEBUG(("%s returned null", dta.dta_name));
			m_free((virtaddr)b);
		}
		r = f_snext();
	    }
	}
}
 

void
close_filesys()
{
	PROC *p;
	FILEPTR *f;
	int i;

	TRACE(("close_filesys"));
/* close every open file */
	for (p = proclist; p; p = p->gl_next) {
		for (i = MIN_HANDLE; i < MAX_OPEN; i++) {
			if ( (f = p->handle[i]) != 0) {
				if (p->wait_q == TSR_Q || p->wait_q == ZOMBIE_Q)
					ALERT("Open file for dead process?");
				do_pclose(p, f);
			}
		}
	}
}

/*
 * "media change" routine: called when a media change is detected on device
 * d, which may or may not be a BIOS device. All handles associated with
 * the device are closed, and all directories invalidated. This routine
 * does all the dirty work, and is called automatically when
 * disk_changed detects a media change.
 */

void ARGS_ON_STACK 
changedrv(d)
	unsigned d;
{
	PROC *p;
	int i;
	FILEPTR *f;
	FILESYS *fs;
	SHTEXT *stext, **old;
	extern SHTEXT *text_reg;	/* in mem.c */
	DIR *dirh;
	fcookie dir;
	int warned = (d & 0xf000) == PROC_RDEV_BASE;
	long r;

/* if an aliased drive, change the *real* device */
	if (d < NUM_DRIVES && aliasdrv[d]) {
		d = aliasdrv[d] - 1;	/* see NOTE above */
	}

/* re-initialize the device, if it was a BIOS device */
	if (d < NUM_DRIVES) {
		fs = drives[d];
		if (fs) {
			(void)(*fs->dskchng)(d);
		}
		init_drive(d);
	}

	for (p = proclist; p; p = p->gl_next) {
	/* invalidate all open files on this device */
		for (i = MIN_HANDLE; i < MAX_OPEN; i++) {
			if (((f = p->handle[i]) != 0) &&
				(f != (FILEPTR *)1) && (f->fc.dev == d)) {
			    if (!warned) {
				ALERT(
"Files were open on a changed drive (0x%x)!", d);
				warned++;
			    }

/* we set f->dev to NULL to indicate to do_pclose that this is an
 * emergency close, and that it shouldn't try to make any
 * calls to the device driver since the file has gone away
 */
			    f->dev = NULL;
			    (void)do_pclose(p, f);
/* we could just zero the handle, but this could lead to confusion if
 * a process doesn't realize that there's been a media change, Fopens
 * a new file, and gets the same handle back. So, we force the
 * handle to point to /dev/null.
 */
			    p->handle[i] =
				do_open("U:\\DEV\\NULL", O_RDWR, 0, (XATTR *)0);
			}
		}

	/* terminate any active directory searches on the drive */
		for (i = 0; i < NUM_SEARCH; i++) {
			dirh = &p->srchdir[i];
			if (p->srchdta[i] && dirh->fc.fs && dirh->fc.dev == d) {
				TRACE(("closing search for process %d", p->pid));
				release_cookie(&dirh->fc);
				dirh->fc.fs = 0;
				p->srchdta[i] = 0;
			}
		}

		for (dirh = p->searches; dirh; dirh = dirh->next) {
		    /* If this search is on the changed drive, release
		       the cookie, but do *not* free it, since the
		       user could later call closedir on it. */
			if (dirh->fc.fs && dirh->fc.dev == d) {
				release_cookie (&dirh->fc);
				dirh->fc.fs = 0;
			}
		}
		    
		if (d >= NUM_DRIVES) continue;

	/* change any active directories on the device to the (new) root */
		fs = drives[d];
		if (fs) {
			r = (*fs->root)(d, &dir);
			if (r != E_OK) dir.fs = 0;
		} else {
			dir.fs = 0; dir.dev = d;
		}

		for (i = 0; i < NUM_DRIVES; i++) {
			if (p->root[i].dev == d) {
				release_cookie(&p->root[i]);
				dup_cookie(&p->root[i], &dir);
			}
			if (p->curdir[i].dev == d) {
				release_cookie(&p->curdir[i]);
				dup_cookie(&p->curdir[i], &dir);
			}
		}
		release_cookie(&dir);
	}

/* free any file descriptors associated with shared text regions */
	for (old = &text_reg; 0 != (stext = *old);) {
		f = stext->f;
		if (f->fc.dev == d) {
			f->dev = NULL;
			do_pclose(rootproc, f);
			stext->f = 0;
/* free region if unattached */
			if (stext->text->links == 0xffff) {
				stext->text->links = 0;
				stext->text->mflags &= ~(M_SHTEXT|M_SHTEXT_T);
				free_region(stext->text);
				*old = stext->next;
				kfree(stext);
				continue;
			}
/* else clear `sticky bit' */
			stext->text->mflags &= ~M_SHTEXT_T;
		}
		old = &stext->next;
	}
}

/*
 * check for media change: if the drive has changed, call changedrv to
 * invalidate any open files and file handles associated with it, and
 * call the file system's media change routine.
 * returns: 0 if no change, 1 if change, negative number for error
 */

int
disk_changed(d)
	int d;
{
	short r;
	FILESYS *fs;
	static char tmpbuf[8192];

/* for now, only check BIOS devices */
	if (d < 0 || d >= NUM_DRIVES)
		return 0;
/* watch out for aliased drives */
	if (aliasdrv[d]) {
		d = aliasdrv[d] - 1;
		if (d < 0 || d >= NUM_DRIVES)
			return 0;
	}

/* has the drive been initialized yet? If not, then initialize it and return
 * "no change"
 */
	fs = drives[d];
	if (!fs) {
		TRACE(("drive %c not yet initialized", d+'A'));
		changedrv(d);
		return 0;
	}

/* We have to do this stuff no matter what, because someone may have installed
 * vectors to force a media change...
 * PROBLEM: AHDI may get upset if the drive isn't valid.
 * SOLUTION: don't change the default PSEUDODRIVES setting!
 */

TRACE(("calling mediach(%d)",d));
	r = (int)mediach(d);
TRACE(("mediach(%d) == %d", d, r));

	if (r < 0) {
/* KLUDGE: some .XFS drivers don't install BIOS vectors, and so we'll
 * always get EUNDEV back from them. This isn't recommended (since there
 * are other programs than MiNT that may ask for BIOS functions from
 * any installed drives). This is a temporary work-around until those
 * .XFSes are changed to either install BIOS vectors or to use the
 * new U: Dcntl() calls to install themselves.
 * Note that EUNDEV must be tested for drives A-C, or else booting may
 * not work properly.
 */
		if (d > 2 && r == EUNDEV)
			return 0;	/* assume no change */
		else
			return r;
	}
	if (r == 1) {		/* drive _may_ have changed */
		r = rwabs(0, tmpbuf, 1, 0, d, 0L);	/* check the BIOS */
		if (r != E_CHNG) {			/* nope, no change */
			TRACE(("rwabs returned %d", r));
			return (r < 0) ? r : 0;
		}
		r = 2;			/* drive was definitely changed */
	}
	if (r == 2) {
		TRACE(("definite media change"));
		fs = drives[d];		/* get filesystem associated with drive */
		if ((*fs->dskchng)(d)) { /* does the fs agree that it changed? */
			drives[d] = 0;
			changedrv(d);	/* yes -- do the change */
			return 1;
		}
	}
	return 0;
}

/*
 * routines for parsing path names
 */

#define DIRSEP(p) ((p) == '\\')

/*
 * relpath2cookie converts a TOS file name into a file cookie representing
 * the directory the file resides in, and a character string representing
 * the name of the file in that directory. The character string is
 * copied into the "lastname" array. If lastname is NULL, then the cookie
 * returned actually represents the file, instead of just the directory
 * the file is in.
 *
 * note that lastname, if non-null, should be big enough to contain all the
 * characters in "path", since if the file system doesn't want the kernel
 * to do path name parsing we may end up just copying path to lastname
 * and returning the current or root directory, as appropriate
 *
 * "relto" is the directory relative to which the search should start.
 * if you just want the current directory, use path2cookie instead.
 *
 */

#define MAX_LINKS 4

long
relpath2cookie(relto, path, lastname, res, depth)
	fcookie *relto;
	const char *path;
	char *lastname;
	fcookie *res;
	int depth;
{
	fcookie dir;
	int drv;
	int len;
	char c, *s;
	XATTR xattr;
	static char newpath[16] = "U:\\DEV\\";
	char temp2[PATH_MAX];
	char linkstuff[PATH_MAX];
	long r;

/* dolast: 0 == return a cookie for the directory the file is in
 *         1 == return a cookie for the file itself, don't follow links
 *	   2 == return a cookie for whatever the file points at
 */
	int dolast = 0;
	int i = 0;

	if (!lastname) {
		dolast = 1;
		lastname = temp2;
	} else if (lastname == follow_links) {
		dolast = 2;
		lastname = temp2;
	}

	*lastname = 0;

PATH2COOKIE_DB(("relpath2cookie(%s, dolast=%d, depth=%d)", path, dolast, depth));

	if (depth > MAX_LINKS) {
		DEBUG(("Too many symbolic links"));
		return ELOOP;
	}
/* special cases: CON:, AUX:, etc. should be converted to U:\DEV\CON,
 * U:\DEV\AUX, etc.
 */
	if (strlen(path) == 4 && path[3] == ':') {
		strncpy(newpath+7, path, 3);
		path = newpath;
	}

/* first, check for a drive letter */
/* BUG: a '\' at the start of a symbolic link is relative to the current
 * drive of the process, not the drive the link is located on
 */
	if (path[1] == ':') {
		c = path[0];
		if (c >= 'a' && c <= 'z')
			drv = c - 'a';
		else if (c >= 'A' && c <= 'Z')
			drv = c - 'A';
		else
			goto nodrive;
		path += 2;
		i = 1;		/* remember that we saw a drive letter */
	} else {
nodrive:
		drv = curproc->curdrv;
	}

/* see if the path is rooted from '\\' */
	if (DIRSEP(*path)) {
		while(DIRSEP(*path))path++;
		dup_cookie(&dir, &curproc->root[drv]);
	} else {
		if (i)	{	/* an explicit drive letter was given */
			dup_cookie(&dir, &curproc->curdir[drv]);
		}
		else
			dup_cookie(&dir, relto);
	}

	if (!dir.fs) {
		changedrv(dir.dev);
		dup_cookie(&dir, &curproc->root[drv]);
	}

	if (!dir.fs) {
		DEBUG(("path2cookie: no file system: returning EDRIVE"));
		return EDRIVE;
	}

	/* here's where we come when we've gone across a mount point */
	
restart_mount:

	if (!*path) {		/* nothing more to do */
PATH2COOKIE_DB(("relpath2cookie: no more path, returning 0"));
		*res = dir;
		return 0;
	}

/* see if there has been a disk change; if so, return E_CHNG.
 * path2cookie will restart the search automatically; other functions
 * that call relpath2cookie directly will have to fail gracefully
 */
	if ((r = disk_changed(dir.dev)) != 0) {
		release_cookie(&dir);
		if (r > 0) r = E_CHNG;
PATH2COOKIE_DB(("relpath2cookie: returning %d", r));
		return r;
	}


	if (dir.fs->fsflags & FS_KNOPARSE) {
		if (!dolast) {
PATH2COOKIE_DB(("fs is a KNOPARSE, nothing to do"));
			strncpy(lastname, path, PATH_MAX-1);
			lastname[PATH_MAX - 1] = 0;
			r = 0;
			*res = dir;
		} else {
PATH2COOKIE_DB(("fs is a KNOPARSE, calling lookup"));
			r = (*dir.fs->lookup)(&dir, path, res);
			if (r == EMOUNT) {	/* hmmm... a ".." at a mount point, maybe */
				fcookie mounteddir;
				r = (*dir.fs->root)(dir.dev, &mounteddir);
				if (r == 0 && drv == UNIDRV) {
					if (dir.fs == mounteddir.fs &&
					    dir.index == mounteddir.index &&
					    dir.dev == mounteddir.dev) {
						release_cookie(&dir);
						release_cookie(&mounteddir);
						dup_cookie(&dir, &curproc->root[UNIDRV]);
						TRACE(("path2cookie: restarting from mount point"));
						goto restart_mount;
					}
				} else {
					if (r == 0)
						release_cookie(&mounteddir);
					r = 0;
				}
			}
			release_cookie(&dir);
		}
		PATH2COOKIE_DB(("relpath2cookie: returning %ld", r));
		return r;
	}


/* parse all but (possibly) the last component of the path name */
/* rules here: at the top of the loop, &dir is the cookie of
 * the directory we're in now, xattr is its attributes, and res is unset
 * at the end of the loop, &dir is unset, and either r is nonzero
 * (to indicate an error) or res is set to the final result
 */
	r = (dir.fs->getxattr)(&dir, &xattr);
	if (r) {
		DEBUG(("couldn't get directory attributes"));
		release_cookie(&dir);
		return EINTRN;
	}

	while (*path) {

	/* now we must have a directory, since there are more things in the path */
		if ((xattr.mode & S_IFMT) != S_IFDIR) {
PATH2COOKIE_DB(("relpath2cookie: not a directory, returning EPTHNF"));
			release_cookie(&dir);
			r = EPTHNF;
			break;
		}
	/* we must also have search permission for the directory */
		if (denyaccess(&xattr, S_IXOTH)) {
			DEBUG(("search permission in directory denied"));
			release_cookie(&dir);
			r = EPTHNF;
			break;
		}

	/* if there's nothing left in the path, we can break here */
		if (!*path) {
PATH2COOKIE_DB(("relpath2cookie: no more path, breaking (1)"));
			*res = dir;
			break;
		}
	/* next, peel off the next name in the path */
		len = 0;
		s = lastname;
		c = *path;
		while (c && !DIRSEP(c)) {
			if (len++ < PATH_MAX)
				*s++ = c;
			c = *++path;
		}
		*s = 0;

	/* if there are no more names in the path, and we don't want
	 * to actually look up the last name, then we're done
	 */
		if (dolast == 0 && !*path) {
			*res = dir;
PATH2COOKIE_DB(("relpath2cookie: no more path, breaking (2)"));
			break;
		}


	/* 
	 * skip trailing slashes
	 */
		while (DIRSEP(*path)) path++;

PATH2COOKIE_DB(("relpath2cookie: looking up [%s]", lastname));

		r = (*dir.fs->lookup)(&dir, lastname, res);
		if (r == EMOUNT) {
			fcookie mounteddir;
			r = (*dir.fs->root)(dir.dev, &mounteddir);
			if (r == 0 && drv == UNIDRV) {
				if (samefile(&dir, &mounteddir)) {
					release_cookie(&dir);
					release_cookie(&mounteddir);
					dup_cookie(&dir, &curproc->root[UNIDRV]);
					TRACE(("path2cookie: restarting from mount point"));
					goto restart_mount;
				} else if (r == 0) {
					r = EINTRN;
					release_cookie(&mounteddir);
					release_cookie(&dir);
					break;
				}
			} else if (r == 0) {
				release_cookie(&mounteddir);
			} else {
				release_cookie(&dir);
				break;
			}
		} else if (r) {
			if (r == EFILNF && *path) {
			/* the "file" we didn't find was treated as a directory */
				r = EPTHNF;
			}
			release_cookie(&dir);
			break;
		}

	/* check for a symbolic link */
		r = (res->fs->getxattr)(res, &xattr);
		if (r != 0) {
			DEBUG(("path2cookie: couldn't get file attributes"));
			release_cookie(&dir);
			release_cookie(res);
			break;
		}

	/* if the file is a link, and we're following links, follow it */
		if ( (xattr.mode & S_IFMT) == S_IFLNK && (*path || dolast > 1)) {
			r = (res->fs->readlink)(res, linkstuff, PATH_MAX);
			release_cookie(res);
			if (r) {
				DEBUG(("error reading symbolic link"));
				release_cookie(&dir);
				break;
			}
			r = relpath2cookie(&dir, linkstuff, follow_links, res,
						depth+1);
			release_cookie(&dir);
			if (r) {
				DEBUG(("error following symbolic link"));
				break;
			}
			dir = *res;
			(void)(res->fs->getxattr)(res, &xattr);
		} else {
			release_cookie(&dir);
			dir = *res;
		}
	}

	PATH2COOKIE_DB(("relpath2cookie: returning %ld", r));
	return r;
}

#define MAX_TRYS 8

long
path2cookie(path, lastname, res)
	const char *path;
	char *lastname;
	fcookie *res;
{
	fcookie *dir;
	long r;
/* AHDI sometimes will keep insisting that a media change occured;
 * we limit the number of retrys to avoid hanging the system
 */
	int trycnt = 0;

	dir = &curproc->curdir[curproc->curdrv];

	do {
		r = relpath2cookie(dir, path, lastname, res, 0);
		if (r == E_CHNG)
			DEBUG(("path2cookie: restarting due to media change"));
	} while (r == E_CHNG && trycnt++ < MAX_TRYS);

	return r;
}

/*
 * release_cookie: tell the file system owner that a cookie is no
 * longer in use by the kernel
 */
void
release_cookie(fc)
	fcookie *fc;
{
	FILESYS *fs;

	if (fc) {
		fs = fc->fs;
		if (fs && fs->release) {
			(void)(*fs->release)(fc);
		}
	}
}

/*
 * Make a new cookie (newc) which is a duplicate of the old cookie
 * (oldc). This may be something the file system is interested in,
 * so we give it a chance to do the duplication; if it doesn't
 * want to, we just copy.
 */

void
dup_cookie(newc, oldc)
	fcookie *newc, *oldc;
{
	FILESYS *fs = oldc->fs;

	if (fs && fs->release && fs->dupcookie) {
		(void)(*fs->dupcookie)(newc, oldc);
	} else {
		*newc = *oldc;
	}
}

/*
 * new_fileptr, dispose_fileptr: allocate (deallocate) a file pointer
 */

FILEPTR *
new_fileptr()
{
	FILEPTR *f;

	if ((f = flist) != 0) {
		flist = f->next;
		f->next = 0;
		return f;
	}
	f = kmalloc(SIZEOF(FILEPTR));
	if (!f) {
		FATAL("new_fileptr: out of memory");
	}
	else {
		f->next = 0;
	}
	return f;
}

void
dispose_fileptr(f)
	FILEPTR *f;
{
	if (f->links != 0) {
		FATAL("dispose_fileptr: f->links == %d", f->links);
	}
	f->next = flist;
	flist = f;
}

/*
 * denyshare(list, f): "list" points at the first FILEPTR in a
 * chained list of open FILEPTRS referring to the same file;
 * f is a newly opened FILEPTR. Every FILEPTR in the given list is
 * checked to see if its "open" mode (in list->flags) is compatible with
 * the open mode in f->flags. If not (for example, if f was opened with
 * a "read" mode and some other file has the O_DENYREAD share mode),
 * then 1 is returned. If all the open FILEPTRs in the list are
 * compatible with f, then 0 is returned.
 * This is not as complicated as it sounds. In practice, just keep a
 * list of open FILEPTRs attached to each file, and put something like
 * 	if (denyshare(thisfile->openfileptrlist, newfileptr))
 *		return EACCDN;
 * in the device open routine.
 */

int ARGS_ON_STACK 
denyshare(list, f)
	FILEPTR *list, *f;
{
	int newrm, newsm;	/* new read and sharing mode */
	int oldrm, oldsm;	/* read and sharing mode of already opened file */
	extern MEMREGION *tofreed;
	MEMREGION *m = tofreed;
	int i;

	newrm = f->flags & O_RWMODE;
	newsm = f->flags & O_SHMODE;

/*
 * O_EXEC gets treated the same as O_RDONLY for our purposes
 */
	if (newrm == O_EXEC) newrm = O_RDONLY;

/* New meaning for O_COMPAT: deny write access to all _other_
 * processes.
 */

	for ( ; list; list = list->next) {
		oldrm = list->flags & O_RWMODE;
		if (oldrm == O_EXEC) oldrm = O_RDONLY;
		oldsm = list->flags & O_SHMODE;
		if (oldsm == O_DENYW || oldsm == O_DENYRW) {
		 	if (newrm != O_RDONLY) {
/* conflict because of unattached shared text region? */
				if (!m && NULL != (m = find_text_seg(list))) {
					if (m->links == 0xffff)
						continue;
					m = 0;
				}
				DEBUG(("write access denied"));
				return 1;
			}
		}
		if (oldsm == O_DENYR || oldsm == O_DENYRW) {
			if (newrm != O_WRONLY) {
				DEBUG(("read access denied"));
				return 1;
			}
		}
		if (newsm == O_DENYW || newsm == O_DENYRW) {
			if (oldrm != O_RDONLY) {
				DEBUG(("couldn't deny writes"));
				return 1;
			}
		}
		if (newsm == O_DENYR || newsm == O_DENYRW) {
			if (oldrm != O_WRONLY) {
				DEBUG(("couldn't deny reads"));
				return 1;
			}
		}
/* If either sm == O_COMPAT, then we check to make sure
   that the file pointers are owned by the same process (O_COMPAT means
   "deny writes to any other processes"). This isn't quite the same
   as the Atari spec, which says O_COMPAT means "deny access to other
   processes." We should fix the spec.
 */
		if ((newsm == O_COMPAT && newrm != O_RDONLY && oldrm != O_RDONLY) ||
		    (oldsm == O_COMPAT && newrm != O_RDONLY)) {
			for (i = MIN_HANDLE; i < MAX_OPEN; i++) {
				if (curproc->handle[i] == list)
					goto found;
			}
		/* old file pointer is not open by this process */
			DEBUG(("O_COMPAT file was opened for writing by another process"));
			return 1;
		found:
			;	/* everything is OK */
		}
	}
/* cannot close shared text regions file here... have open do it. */
	if (m)
		tofreed = m;
	return 0;
}

/*
 * denyaccess(XATTR *xattr, unsigned perm): checks to see if the access
 * specified by perm (which must be some combination of S_IROTH, S_IWOTH,
 * and S_IXOTH) should be granted to the current process
 * on a file with the given extended attributes. Returns 0 if access
 * by the current process is OK, 1 if not.
 */

int
ngroupmatch(group)
	int group;
{
	int i;

	for (i=0; i<curproc->ngroups; i++)
		if (curproc->ngroup[i] == group)
			return 1;

	return 0;
}

int
denyaccess(xattr, perm)
	XATTR *xattr;
	unsigned perm;
{
	unsigned mode;

/* the super-user can do anything! */
	if (curproc->euid == 0)
		return 0;

	mode = xattr->mode;
	if (curproc->euid == xattr->uid)
		perm = perm << 6;
	else if (curproc->egid == xattr->gid)
		perm = perm << 3;
	else if (ngroupmatch(xattr->gid))
		perm = perm << 3;

	if ((mode & perm) != perm) return 1;	/* access denied */
	return 0;
}

/*
 * Checks a lock against a list of locks to see if there is a conflict.
 * This is a utility to be used by file systems, somewhat like denyshare
 * above. Returns 0 if there is no conflict, or a pointer to the
 * conflicting LOCK structure if there is.
 *
 * Conflicts occur for overlapping locks if the process id's are
 * different and if at least one of the locks is a write lock.
 *
 * NOTE: we assume before being called that the locks have been converted
 * so that l_start is absolute. not relative to the current position or
 * end of file.
 */

LOCK * ARGS_ON_STACK 
denylock(list, lck)
	LOCK *list, *lck;
{
	LOCK *t;
	unsigned long tstart, tend;
	unsigned long lstart, lend;
	int pid = curproc->pid;
	int ltype;

	ltype = lck->l.l_type;
	lstart = lck->l.l_start;

	if (lck->l.l_len == 0)
		lend = 0xffffffffL;
	else
		lend = lstart + lck->l.l_len - 1;

	for (t = list; t; t = t->next) {
		tstart = t->l.l_start;
		if (t->l.l_len == 0)
			tend = 0xffffffffL;
		else
			tend = tstart + t->l.l_len - 1;

	/* look for overlapping locks */
		if (tstart <= lstart && tend >= lstart && t->l.l_pid != pid &&
		    (ltype == F_WRLCK || t->l.l_type == F_WRLCK))
			break;
		if (lstart <= tstart && lend >= tstart && t->l.l_pid != pid &&
		    (ltype == F_WRLCK || t->l.l_type == F_WRLCK))
			break;
	}
	return t;
}

/*
 * check to see that a file is a directory, and that write permission
 * is granted; return an error code, or 0 if everything is ok.
 */
long
dir_access(dir, perm)
	fcookie *dir;
	unsigned perm;
{
	XATTR xattr;
	long r;

	r = (*dir->fs->getxattr)(dir, &xattr);
	if (r) {
		DEBUG(("dir_access: file system returned %ld", r));
		return r;
	}
	if ( (xattr.mode & S_IFMT) != S_IFDIR ) {
		DEBUG(("file is not a directory"));
		return EPTHNF;
	}
	if (denyaccess(&xattr, perm)) {
		DEBUG(("no permission for directory"));
		return EACCDN;
	}
	return 0;
}

/*
 * returns 1 if the given name contains a wildcard character 
 */

int
has_wild(name)
	const char *name;
{
	char c;

	while ((c = *name++) != 0) {
		if (c == '*' || c == '?') return 1;
	}
	return 0;
}

/*
 * void copy8_3(dest, src): convert a file name (src) into DOS 8.3 format
 * (in dest). Note the following things:
 * if a field has less than the required number of characters, it is
 * padded with blanks
 * a '*' means to pad the rest of the field with '?' characters
 * special things to watch for:
 *	"." and ".." are more or less left alone
 *	"*.*" is recognized as a special pattern, for which dest is set
 *	to just "*"
 * Long names are truncated. Any extensions after the first one are
 * ignored, i.e. foo.bar.c -> foo.bar, foo.c.bar->foo.c.
 */

void
copy8_3(dest, src)
	char *dest;
	const char *src;
{
	char fill = ' ', c;
	int i;

	if (src[0] == '.') {
		if (src[1] == 0) {
			strcpy(dest, ".       .   ");
			return;
		}
		if (src[1] == '.' && src[2] == 0) {
			strcpy(dest, "..      .   ");
			return;
		}
	}
	if (src[0] == '*' && src[1] == '.' && src[2] == '*' && src[3] == 0) {
		dest[0] = '*';
		dest[1] = 0;
		return;
	}

	for (i = 0; i < 8; i++) {
		c = *src++;
		if (!c || c == '.') break;
		if (c == '*') {
			fill = c = '?';
		}
		*dest++ = toupper(c);
	}
	while (i++ < 8) {
		*dest++ = fill;
	}
	*dest++ = '.';
	i = 0;
	fill = ' ';
	while (c && c != '.')
		c = *src++;

	if (c) {
		for( ;i < 3; i++) {
			c = *src++;
			if (!c || c == '.') break;
			if (c == '*')
				c = fill = '?';
			*dest++ = toupper(c);
		}
	}
	while (i++ < 3)
		*dest++ = fill;
	*dest = 0;
}

/*
 * int pat_match(name, patrn): returns 1 if "name" matches the template in
 * "patrn", 0 if not. "patrn" is assumed to have been expanded in 8.3
 * format by copy8_3; "name" need not be. Any '?' characters in patrn
 * will match any character in name. Note that if "patrn" has a '*' as
 * the first character, it will always match; this will happen only if
 * the original pattern (before copy8_3 was applied) was "*.*".
 *
 * BUGS: acts a lot like the silly TOS pattern matcher.
 */

int
pat_match(name, template)
	const char *name, *template;
{
	register char *s, c;
	char expname[TOS_NAMELEN+1];

	if (*template == '*') return 1;
	copy8_3(expname, name);

	s = expname;
	while ((c = *template++) != 0) {
		if (c != *s && c != '?')
			return 0;
		s++;
	}
	return 1;
}

/*
 * int samefile(fcookie *a, fcookie *b): returns 1 if the two cookies
 * refer to the same file or directory, 0 otherwise
 */

int
samefile(a, b)
	fcookie *a, *b;
{
	if (a->fs == b->fs && a->dev == b->dev && a->index == b->index)
		return 1;
	return 0;
}
