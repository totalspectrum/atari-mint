/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
*/

/* DOS directory functions */

#include "mint.h"

extern int aliasdrv[];	/* in filesys.c */

/* change to a new drive: should always return a map of valid drives */

long ARGS_ON_STACK
d_setdrv(d)
	int d;
{
	long r;
	extern long dosdrvs;	/* in filesys.c */

	r = drvmap() | dosdrvs | PSEUDODRVS;

	TRACE(("Dsetdrv(%d)", d));
	if (d < 0 || d >= NUM_DRIVES || (r & (1L << d)) == 0) {
		DEBUG(("Dsetdrv: invalid drive %d", d));
		return r;
	}

	curproc->base->p_defdrv = curproc->curdrv = d;
	return r;
}


long ARGS_ON_STACK
d_getdrv()
{
	TRACE(("Dgetdrv"));
	return curproc->curdrv;
}

long ARGS_ON_STACK
d_free(buf, d)
	long *buf;
	int d;
{
	fcookie *dir = 0;
	FILESYS *fs;
	fcookie root;
	long r;

	TRACE(("Dfree(%d)", d));

/* drive 0 means current drive, otherwise it's d-1 */
	if (d)
		d = d-1;
	else
		d = curproc->curdrv;

/* If it's not a standard drive or an alias of one, get the pointer to
   the filesystem structure and use the root directory of the
   drive. */
	if (d < 0 || d >= NUM_DRIVES) {
		int i;

		for (i = 0; i < NUM_DRIVES; i++) {
			if (aliasdrv[i] == d) {
				d = i;
				goto aliased;
			}
		}

		fs = get_filesys (d);
		if (!fs)
		  return EDRIVE;
		r = fs->root (d, &root);
		if (r < 0)
		  return r;
		r = (*fs->dfree) (&root, buf);
		release_cookie (&root);
		return r;
	}

/* check for a media change -- we don't care much either way, but it
 * does keep the results more accurate
 */
	(void)disk_changed(d);

aliased:

/* use current directory, not root, since it's more likely that
 * programs are interested in the latter (this makes U: work much
 * better)
 */
	dir = &curproc->curdir[d];
	if (!dir->fs) {
		DEBUG(("Dfree: bad drive"));
		return EDRIVE;
	}

	return (*dir->fs->dfree)(dir, buf);
}

long ARGS_ON_STACK
d_create(path)
	const char *path;
{
	fcookie dir;
	long r;
	char temp1[PATH_MAX];

	TRACE(("Dcreate(%s)", path));

	r = path2cookie(path, temp1, &dir);
	if (r) {
		DEBUG(("Dcreate(%s): returning %ld", path, r));
		return r;	/* an error occured */
	}
/* check for write permission on the directory */
	r = dir_access(&dir, S_IWOTH);
	if (r) {
		DEBUG(("Dcreate(%s): write access to directory denied",path));
		release_cookie(&dir);
		return r;
	}
	r = (*dir.fs->mkdir)(&dir, temp1, DEFAULT_DIRMODE & ~curproc->umask);
	release_cookie(&dir);
	return r;
}

long ARGS_ON_STACK
d_delete(path)
	const char *path;
{
	fcookie parentdir, targdir;
	long r;
	PROC *p;
	int i;
	XATTR xattr;
	char temp1[PATH_MAX];

	TRACE(("Ddelete(%s)", path));

	r = path2cookie(path, temp1, &parentdir);

	if (r) {
		DEBUG(("Ddelete(%s): error %lx", path, r));
		release_cookie(&parentdir);
		return r;
	}
/* check for write permission on the directory which the target
 * is located
 */
	if ((r = dir_access(&parentdir, S_IWOTH)) != 0) {
		DEBUG(("Ddelete(%s): access to directory denied", path));
		release_cookie(&parentdir);
		return r;
	}

/* now get the info on the file itself */

	r = relpath2cookie(&parentdir, temp1, NULL, &targdir, 0);
	if (r) {
bailout:
		release_cookie(&parentdir);
		DEBUG(("Ddelete: error %ld on %s", r, path));
		return r;
	}
	if ((r = (*targdir.fs->getxattr)(&targdir, &xattr)) != 0) {
		release_cookie(&targdir);
		goto bailout;
	}

/* if the "directory" is a symbolic link, really unlink it */
	if ( (xattr.mode & S_IFMT) == S_IFLNK ) {
		r = (*parentdir.fs->remove)(&parentdir, temp1);
	} else if ( (xattr.mode & S_IFMT) != S_IFDIR ) {
		DEBUG(("Ddelete: %s is not a directory", path));
		r = EPTHNF;
	} else {

/* don't delete anyone else's root or current directory */
	    for (p = proclist; p; p = p->gl_next) {
		if (p->wait_q == ZOMBIE_Q || p->wait_q == TSR_Q)
			continue;
		for (i = 0; i < NUM_DRIVES; i++) {
			if (samefile(&targdir, &p->root[i])) {
				DEBUG(("Ddelete: directory %s is a root directory",
					path));
noaccess:
				release_cookie(&targdir);
				release_cookie(&parentdir);
				return EACCDN;
			} else if (samefile(&targdir, &p->curdir[i])) {
				if (i == p->curdrv && p != curproc) {
					DEBUG(("Ddelete: directory %s is in use",
						path));
					goto noaccess;
				} else {
					release_cookie(&p->curdir[i]);
					dup_cookie(&p->curdir[i], &p->root[i]);
				} 
			}
		}
	    }
	    release_cookie(&targdir);
	    r = (*parentdir.fs->rmdir)(&parentdir, temp1);
	}
	release_cookie(&parentdir);
	return r;
}

long ARGS_ON_STACK
d_setpath(path)
	const char *path;
{
	fcookie dir;
	int drv = curproc->curdrv;
	int i;
	char c;
	long r;
	XATTR xattr;

	TRACE(("Dsetpath(%s)", path));

	r = path2cookie(path, follow_links, &dir);

	if (r) {
		DEBUG(("Dsetpath(%s): returning %ld", path, r));
		return r;
	}

	if (path[0] && path[1] == ':') {
		c = *path;
		if (c >= 'a' && c <= 'z')
			drv = c-'a';
		else if (c >= 'A' && c <= 'Z')
			drv = c-'A';
	}

	r = (*dir.fs->getxattr)(&dir, &xattr);

	if (r < 0) {
		DEBUG(("Dsetpath: file '%s': attributes not found", path));
		release_cookie(&dir);
		return r;
	}

	if (!(xattr.attr & FA_DIR)) {
		DEBUG(("Dsetpath(%s): not a directory",path));
		release_cookie(&dir);
		return EPTHNF;
	}

	if (denyaccess(&xattr, S_IROTH|S_IXOTH)) {
		DEBUG(("Dsetpath(%s): access denied", path));
		release_cookie(&dir);
		return EACCDN;
	}
/*
 * watch out for symbolic links; if c:\foo is a link to d:\bar, then
 * "cd c:\foo" should also change the drive to d:
 */
	if (drv != UNIDRV && dir.dev != curproc->root[drv].dev) {
		for (i = 0; i < NUM_DRIVES; i++) {
			if (curproc->root[i].dev == dir.dev &&
			    curproc->root[i].fs == dir.fs) {
				if (drv == curproc->curdrv)
					curproc->curdrv = i;
				drv = i;
				break;
			}
		}
	}
	release_cookie(&curproc->curdir[drv]);
	curproc->curdir[drv] = dir;
	return 0;
}

/* jr: like d_getpath, except that the caller provides a limit
   for the max. number of characters to be put into the buffer.
   Inspired by POSIX.1, getcwd(), 5.2.2 */

long ARGS_ON_STACK
d_getcwd(path, drv, size)
	char *path;
	int drv, size;
{
	fcookie *dir, *root;
	long r;
	char buf[PATH_MAX];
	FILESYS *fs;

	TRACE(("Dgetcwd(%c, %d)", drv + '@', size));
	if (drv < 0 || drv > NUM_DRIVES)
		return EDRIVE;

	drv = (drv == 0) ? curproc->curdrv : drv-1;

	root = &curproc->root[drv];

	if (!root->fs) {	/* maybe not initialized yet? */
		changedrv(drv);
		root = &curproc->curdir[drv];
		if (!root->fs)
			return EDRIVE;
	}
	fs = root->fs;
	dir = &curproc->curdir[drv];

	if (!(fs->fsflags & FS_LONGPATH)) {
		r = (*fs->getname)(root, dir, buf, PATH_MAX);
		if (r) return r;
		if (strlen(buf) < size) {
			strcpy(path, buf);
			return 0;
		} else {
			return ERANGE;
		}
	}
	return (*fs->getname)(root, dir, path, size);
}

long ARGS_ON_STACK
d_getpath(path, drv)
	char *path;
	int drv;
{
	TRACE(("Dgetpath(%c)", drv + '@'));
	return d_getcwd(path, drv, PATH_MAX);
}

long ARGS_ON_STACK
f_setdta(dta)
	DTABUF *dta;
{

	TRACE(("Fsetdta: %lx", dta));
	curproc->dta = dta;
	curproc->base->p_dta = (char *)dta;
	return 0;
}

long ARGS_ON_STACK
f_getdta()
{
	long r;

	r = (long)curproc->dta;
	TRACE(("Fgetdta: returning %lx", r));
	return r;
}

/*
 * Fsfirst/next are actually implemented in terms of opendir/readdir/closedir.
 */

long ARGS_ON_STACK
f_sfirst(path, attrib)
	const char *path;
	int attrib;
{
	char *s, *slash;
	FILESYS *fs;
	fcookie dir, newdir;
	DTABUF *dta;
	DIR *dirh;
	XATTR xattr;
	long r;
	int i, havelabel;
	char temp1[PATH_MAX];

	TRACE(("Fsfirst(%s, %x)", path, attrib));

	r = path2cookie(path, temp1, &dir);

	if (r) {
		DEBUG(("Fsfirst(%s): path2cookie returned %ld", path, r));
		return r;
	}

/*
 * we need to split the last name (which may be a pattern) off from
 * the rest of the path, even if FS_KNOPARSE is true
 */
	slash = 0;
	s = temp1;
	while (*s) {
		if (*s == '\\')
			slash = s;
		s++;
	}

	if (slash) {
		*slash++ = 0;	/* slash now points to a name or pattern */
		r = relpath2cookie(&dir, temp1, follow_links, &newdir, 0);
		release_cookie(&dir);
		if (r) {
			DEBUG(("Fsfirst(%s): lookup returned %ld", path, r));
			return r;
		}
		dir = newdir;
	} else {
		slash = temp1;
	}

/* BUG? what if there really is an empty file name? */
	if (!*slash) {
		DEBUG(("Fsfirst: empty pattern"));
		return EFILNF;
	}

	fs = dir.fs;
	dta = curproc->dta;

/* Now, see if we can find a DIR slot for the search. We use the following
 * heuristics to try to avoid destroying a slot:
 * (1) if the search doesn't use wildcards, don't bother with a slot
 * (2) if an existing slot was for the same DTA address, re-use it
 * (3) if there's a free slot, re-use it. Slots are freed when the
 *     corresponding search is terminated.
 */

	for (i = 0; i < NUM_SEARCH; i++) {
		if (curproc->srchdta[i] == dta) {
			dirh = &curproc->srchdir[i];
			if (dirh->fc.fs) {
				(*dirh->fc.fs->closedir)(dirh);
				release_cookie(&dirh->fc);
				dirh->fc.fs = 0;
			}
			curproc->srchdta[i] = 0; /* slot is now free */
		}
	}

/* copy the pattern over into dta_pat into TOS 8.3 form */
/* remember that "slash" now points at the pattern (it follows the last \,
   if any)
 */
	copy8_3(dta->dta_pat, slash);

/* if attrib & FA_LABEL, read the volume label */
/* BUG: the label date and time are wrong. Does it matter?
 */
	havelabel = 0;
	if (attrib & FA_LABEL) {
		r = (*fs->readlabel)(&dir, dta->dta_name, TOS_NAMELEN+1);
		dta->dta_attrib = FA_LABEL;
		dta->dta_time = dta->dta_date = 0;
		dta->dta_size = 0;
		dta->magic = EVALID;
		if (r == 0 && !pat_match(dta->dta_name, dta->dta_pat))
			r = EFILNF;
		if (attrib == FA_LABEL)
			return r;
		else if (r == 0)
			havelabel = 1;
	}

	if (!havelabel && has_wild(slash) == 0) { /* no wild cards in pattern */
		r = relpath2cookie(&dir, slash, follow_links, &newdir, 0);
		if (r == 0) {
			r = (*newdir.fs->getxattr)(&newdir, &xattr);
			release_cookie(&newdir);
		}
		release_cookie(&dir);
		if (r) {
			DEBUG(("Fsfirst(%s): couldn't get file attributes",path));
			return r;
		}
		dta->magic = EVALID;
		dta->dta_attrib = xattr.attr;
		dta->dta_time = xattr.mtime;
		dta->dta_date = xattr.mdate;
		dta->dta_size = xattr.size;
		strncpy(dta->dta_name, slash, TOS_NAMELEN-1);
		dta->dta_name[TOS_NAMELEN-1] = 0;
		if (curproc->domain == DOM_TOS &&
		    !(fs->fsflags & FS_CASESENSITIVE))
			strupr(dta->dta_name);
		return 0;
	}

/* There is a wild card. Try to find a slot for an opendir/readdir
 * search. NOTE: we also come here if we were asked to search for
 * volume labels and found one.
 */
	for (i = 0; i < NUM_SEARCH; i++) {
		if (curproc->srchdta[i] == 0)
			break;
	}
	if (i == NUM_SEARCH) {
		int oldest = 0; long oldtime = curproc->srchtim[0];

		DEBUG(("Fsfirst(%s): having to re-use a directory slot!",path));
		for (i = 1; i < NUM_SEARCH; i++) {
			if (curproc->srchtim[i] < oldtime) {
				oldest = i;
				oldtime = curproc->srchtim[i];
			}
		}
	/* OK, close this directory for re-use */
		i = oldest;
		dirh = &curproc->srchdir[i];
		if (dirh->fc.fs) {
			(*dirh->fc.fs->closedir)(dirh);
			release_cookie(&dirh->fc);
			dirh->fc.fs = 0;
		}
		curproc->srchdta[i] = 0;
	}

/* check to see if we have read permission on the directory (and make
 * sure that it really is a directory!)
 */
	r = dir_access(&dir, S_IROTH);
	if (r) {
		DEBUG(("Fsfirst(%s): access to directory denied (error code %ld)", path, r));
		release_cookie(&dir);
		return r;
	}

/* set up the directory for a search */
	dirh = &curproc->srchdir[i];
	dirh->fc = dir;
	dirh->index = 0;
	dirh->flags = TOS_SEARCH;
	r = (*dir.fs->opendir)(dirh, dirh->flags);
	if (r != 0) {
		DEBUG(("Fsfirst(%s): couldn't open directory (error %ld)",
			path, r));
		release_cookie(&dir);
		return r;
	}

/* mark the slot as in-use */
	curproc->srchdta[i] = dta;

/* set up the DTA for Fsnext */
	dta->index = i;
	dta->magic = SVALID;
	dta->dta_sattrib = attrib;

/* OK, now basically just do Fsnext, except that instead of ENMFIL we
 * return EFILNF.
 * NOTE: If we already have found a volume label from the search above,
 * then we skip the f_snext and just return that.
 */
	if (havelabel)
		return 0;

	r = f_snext();
	if (r == ENMFIL) r = EFILNF;
	if (r)
		TRACE(("Fsfirst: returning %ld", r));
/* release_cookie isn't necessary, since &dir is now stored in the
 * DIRH structure and will be released when the search is completed
 */
	return r;
}

/*
 * Counter for Fsfirst/Fsnext, so that we know which search slots are
 * least recently used. This is updated once per second by the code
 * in timeout.c.
 * BUG: 1/second is pretty low granularity
 */

long searchtime;

long ARGS_ON_STACK
f_snext()
{
	char buf[TOS_NAMELEN+1];
	DTABUF *dta = curproc->dta;
	FILESYS *fs;
	fcookie fc;
	int i;
	DIR *dirh;
	long r;
	XATTR xattr;

	TRACE(("Fsnext"));

	if (dta->magic == EVALID) {
		DEBUG(("Fsnext: DTA marked a failing search"));
		return ENMFIL;
	}
	if (dta->magic != SVALID) {
		DEBUG(("Fsnext: dta incorrectly set up"));
		return EINVFN;
	}

	i = dta->index;
	dirh = &curproc->srchdir[i];
	curproc->srchtim[i] = searchtime;

	fs = dirh->fc.fs;
	if (!fs)		/* oops -- the directory got closed somehow */
		return EINTRN;

/* BUG: f_snext and readdir should check for disk media changes */

	for(;;) {
		r = (*fs->readdir)(dirh, buf, TOS_NAMELEN+1, &fc);

		if (r == ENAMETOOLONG) {
			DEBUG(("Fsnext: name too long"));
			continue;	/* TOS programs never see these names */
		}
		if (r != 0) {
baderror:
			if (dirh->fc.fs)
				(void)(*fs->closedir)(dirh);
			release_cookie(&dirh->fc);
			dirh->fc.fs = 0;
			curproc->srchdta[i] = 0;
			dta->magic = EVALID;
			if (r != ENMFIL)
				DEBUG(("Fsnext: returning %ld", r));
			return r;
		}

		if (!pat_match(buf, dta->dta_pat))
		{
			release_cookie(&fc);
			continue;	/* different patterns */
		}

	/* check for search attributes */
		r = (*fc.fs->getxattr)(&fc, &xattr);
		if (r) {
			DEBUG(("Fsnext: couldn't get file attributes"));
			release_cookie(&fc);
			goto baderror;
		}
	/* if the file is a symbolic link, try to find what it's linked to */
		if ( (xattr.mode & S_IFMT) == S_IFLNK ) {
			char linkedto[PATH_MAX];
			r = (*fc.fs->readlink)(&fc, linkedto, PATH_MAX);
			release_cookie(&fc);
			if (r == 0) {
			/* the "1" tells relpath2cookie that we read a link */
			    r = relpath2cookie(&dirh->fc, linkedto,
					follow_links, &fc, 1);
			    if (r == 0) {
				r = (*fc.fs->getxattr)(&fc, &xattr);
				release_cookie(&fc);
			    }
			}
			if (r) {
				DEBUG(("Fsnext: couldn't follow link: error %ld",
					r));
			}
		} else {
			release_cookie(&fc);
		}

	/* silly TOS rules for matching attributes */
		if (xattr.attr == 0) break;
		if (xattr.attr & 0x21) break;
		if (dta->dta_sattrib & xattr.attr)
			break;
	}

/* here, we have a match */
	dta->dta_attrib = xattr.attr;
	dta->dta_time = xattr.mtime;
	dta->dta_date = xattr.mdate;
	dta->dta_size = xattr.size;
	strcpy(dta->dta_name, buf);

	if (curproc->domain == DOM_TOS && !(fs->fsflags & FS_CASESENSITIVE)) {
		strupr(dta->dta_name);
	}
	return 0;
}

long ARGS_ON_STACK
f_attrib(name, rwflag, attr)
	const char *name;
	int rwflag;
	int attr;
{
	fcookie fc;
	XATTR xattr;
	long r;

	TRACE(("Fattrib(%s, %d)", name, attr));

	r = path2cookie(name, follow_links, &fc);

	if (r) {
		DEBUG(("Fattrib(%s): error %ld", name, r));
		return r;
	}

	r = (*fc.fs->getxattr)(&fc, &xattr);

	if (r) {
		DEBUG(("Fattrib(%s): getxattr returned %ld", name, r));
		release_cookie(&fc);
		return r;
	}

	if (rwflag) {
		if (attr & (FA_LABEL|FA_DIR)) {
			DEBUG(("Fattrib(%s): illegal attributes specified",name));
			r = EACCDN;
		} else if (curproc->euid && curproc->euid != xattr.uid) {
			DEBUG(("Fattrib(%s): not the file's owner",name));
			r = EACCDN;
		} else if (xattr.attr & (FA_LABEL|FA_DIR)) {
			DEBUG(("Fattrib(%s): file is a volume label "
			      "or directory",name));
			r = EACCDN;
		} else {
			r = (*fc.fs->chattr)(&fc, attr);
		}
		release_cookie(&fc);
		return r;
	} else {
		release_cookie(&fc);
		return xattr.attr;
	}
}

long ARGS_ON_STACK
f_delete(name)
	const char *name;
{
	fcookie dir, fc;
	long r;
	char temp1[PATH_MAX];
	XATTR	xattr;

	TRACE(("Fdelete(%s)", name));

/* get a cookie for the directory the file is in */
	if (( r = path2cookie(name, temp1, &dir) ) != 0)
	{
		DEBUG(("Fdelete: couldn't get directory cookie: error %ld", r));
		return r;
	}

/* check for write permission on directory */
	r = dir_access(&dir, S_IWOTH);
	if (r) {
		DEBUG(("Fdelete(%s): write access to directory denied",name));
		release_cookie(&dir);
		return EACCDN;
	}

/* now get the file attributes */
/* TOS domain processes can only delete files if they have write permission
 * for them
 */
	if (curproc->domain == DOM_TOS) {
		if ( (r = (*dir.fs->lookup)(&dir, temp1, &fc) ) != 0) {
			DEBUG(("Fdelete: error %ld while looking for %s", r, temp1));
			release_cookie(&dir);
			return r;
		}

		if (( r = (*fc.fs->getxattr)(&fc, &xattr)) < 0 )
		{
			release_cookie(&dir);
			release_cookie(&fc);
			DEBUG(("Fdelete: couldn't get file attributes: error %ld", r));
			return r;
		}
	/* see if we're allowed to kill it */
		if (denyaccess(&xattr, S_IWOTH)) {
			release_cookie(&dir);
			release_cookie(&fc);
			DEBUG(("Fdelete: file access denied"));
			return EACCDN;
		}
		release_cookie(&fc);
	}

	r = (*dir.fs->remove)(&dir,temp1);

	release_cookie(&dir);
	return r;
}

long ARGS_ON_STACK
f_rename(junk, old, new)
	int junk;		/* ignored, for TOS compatibility */
	const char *old, *new;
{
	fcookie olddir, newdir, oldfil;
	XATTR xattr;
	char temp1[PATH_MAX], temp2[PATH_MAX];
	long r;

	UNUSED(junk);

	TRACE(("Frename(%s, %s)", old, new));

	r = path2cookie(old, temp2, &olddir);
	if (r) {
		DEBUG(("Frename(%s,%s): error parsing old name",old,new));
		return r;
	}
/* check for permissions on the old file
 * GEMDOS doesn't allow rename if the file is FA_RDONLY
 * we enforce this restriction only on regular files; processes,
 * directories, and character special files can be renamed at will
 */
	r = relpath2cookie(&olddir, temp2, (char *)0, &oldfil, 0);
	if (r) {
		DEBUG(("Frename(%s,%s): old file not found",old,new));
		release_cookie(&olddir);
		return r;
	}
	r = (*oldfil.fs->getxattr)(&oldfil, &xattr);
	release_cookie(&oldfil);
	if (r ||
	    ((xattr.mode & S_IFMT) == S_IFREG && (xattr.attr & FA_RDONLY)) )
	{
		DEBUG(("Frename(%s,%s): access to old file not granted",old,new));
		release_cookie(&olddir);
		return EACCDN;
	}
	r = path2cookie(new, temp1, &newdir);
	if (r) {
		DEBUG(("Frename(%s,%s): error parsing new name",old,new));
		release_cookie(&olddir);
		return r;
	}

	if (newdir.fs != olddir.fs) {
		DEBUG(("Frename(%s,%s): different file systems",old,new));
		release_cookie(&olddir);
		release_cookie(&newdir);
		return EXDEV;	/* cross device rename */
	}

/* check for write permission on both directories */
	r = dir_access(&olddir, S_IWOTH);
	if (!r) r = dir_access(&newdir, S_IWOTH);
	if (r) {
		DEBUG(("Frename(%s,%s): access to a directory denied",old,new));
	} else {
		r = (*newdir.fs->rename)(&olddir, temp2, &newdir, temp1);
	}
	release_cookie(&olddir);
	release_cookie(&newdir);
	return r;
}

/*
 * GEMDOS extension: Dpathconf(name, which)
 * returns information about filesystem-imposed limits; "name" is the name
 * of a file or directory about which the limit information is requested;
 * "which" is the limit requested, as follows:
 *	-1	max. value of "which" allowed
 *	0	internal limit on open files, if any
 *	1	max. number of links to a file	{LINK_MAX}
 *	2	max. path name length		{PATH_MAX}
 *	3	max. file name length		{NAME_MAX}
 *	4	no. of bytes in atomic write to FIFO {PIPE_BUF}
 *	5	file name truncation rules
 *	6	file name case translation rules
 *
 * unlimited values are returned as 0x7fffffffL
 *
 * see also Sysconf() in dos.c
 */

long ARGS_ON_STACK
d_pathconf(name, which)
	const char *name;
	int which;
{
	fcookie dir;
	long r;

	r = path2cookie(name, (char *)0, &dir);
	if (r) {
		DEBUG(("Dpathconf(%s): bad path",name));
		return r;
	}
	r = (*dir.fs->pathconf)(&dir, which);
	if (which == DP_CASE && r == EINVFN) {
	/* backward compatibility with old .XFS files */
		r = (dir.fs->fsflags & FS_CASESENSITIVE) ? DP_CASESENS :
				DP_CASEINSENS;
	}
	release_cookie(&dir);
	return r;
}

/*
 * GEMDOS extension: Opendir/Readdir/Rewinddir/Closedir offer a new,
 * POSIX-like alternative to Fsfirst/Fsnext, and as a bonus allow for
 * arbitrary length file names
 */

long ARGS_ON_STACK
d_opendir(name, flag)
	const char *name;
	int flag;
{
	DIR *dirh;
	fcookie dir;
	long r;

	r = path2cookie(name, follow_links, &dir);
	if (r) {
		DEBUG(("Dopendir(%s): error %ld", name, r));
		return r;
	}
	r = dir_access(&dir, S_IROTH);
	if (r) {
		DEBUG(("Dopendir(%s): read permission denied", name));
		release_cookie(&dir);
		return r;
	}

	dirh = (DIR *)kmalloc(SIZEOF(DIR));
	if (!dirh) {
		release_cookie(&dir);
		return ENSMEM;
	}

	dirh->fc = dir;
	dirh->index = 0;
	dirh->flags = flag;
	r = (*dir.fs->opendir)(dirh, flag);
	if (r) {
		DEBUG(("d_opendir(%s): opendir returned %ld", name, r));
		release_cookie(&dir);
		kfree(dirh);
		return r;
	}

/* we keep a chain of open directories so that if a process
 * terminates without closing them all, we can clean up
 */
	dirh->next = curproc->searches;
	curproc->searches = dirh;

	return (long)dirh;
}

long ARGS_ON_STACK
d_readdir(len, handle, buf)
	int len;
	long handle;
	char *buf;
{
	DIR *dirh = (DIR *)handle;
	fcookie fc;
	long r;

	if (!dirh->fc.fs)
		return EIHNDL;
	r = (*dirh->fc.fs->readdir)(dirh, buf, len, &fc);
	if (r == 0)
		release_cookie(&fc);
	return r;
}

/* jr: just as d_readdir, but also returns XATTR structure (not
   following links). Note that the return value reflects the
   result of the Dreaddir operation, the result of the Fxattr
   operation is stored in long *xret */

long ARGS_ON_STACK
d_xreaddir(len, handle, buf, xattr, xret)
	int len;
	long handle;
	char *buf;
	XATTR *xattr;
	long *xret;
{
	DIR *dirh = (DIR *)handle;
	fcookie fc;
	long r;

	if (!dirh->fc.fs) return EIHNDL;
	r = (*dirh->fc.fs->readdir)(dirh, buf, len, &fc);
	if (r != E_OK) return r;

	*xret = (*fc.fs->getxattr)(&fc, xattr);
	
	release_cookie(&fc);
	return r;
}


long ARGS_ON_STACK
d_rewind(handle)
	long handle;
{
	DIR *dirh = (DIR *)handle;

	if (!dirh->fc.fs)
		return EIHNDL;
	return (*dirh->fc.fs->rewinddir)(dirh);
}

/*
 * NOTE: there is also code in terminate() in dosmem.c that
 * does automatic closes of directory searches.
 * If you change d_closedir(), you may also need to change
 * terminate().
 */

long ARGS_ON_STACK
d_closedir(handle)
	long handle;
{
	long r;
	DIR *dirh = (DIR *)handle;
	DIR **where;

	where = &curproc->searches;
	while (*where && *where != dirh) {
		where = &((*where)->next);
	}
	if (!*where) {
		DEBUG(("Dclosedir: not an open directory"));
		return EIHNDL;
	}

/* unlink the directory from the chain */
	*where = dirh->next;

	if (dirh->fc.fs) {
		r = (*dirh->fc.fs->closedir)(dirh);
		release_cookie(&dirh->fc);
	} else {
		r = 0;
	}

	if (r) {
		DEBUG(("Dclosedir: error %ld", r));
	}
	kfree(dirh);
	return r;
}

/*
 * GEMDOS extension: Fxattr gets extended attributes for a file. "flag"
 * is 0 if symbolic links are to be followed (like stat), 1 if not (like
 * lstat).
 */

long ARGS_ON_STACK
f_xattr(flag, name, xattr)
	int flag;
	const char *name;
	XATTR *xattr;
{
	fcookie fc;
	long r;

	TRACE(("Fxattr(%d, %s)", flag, name));

	r = path2cookie(name, flag ? (char *)0 : follow_links, &fc);
	if (r) {
		DEBUG(("Fxattr(%s): path2cookie returned %ld", name, r));
		return r;
	}
	r = (*fc.fs->getxattr)(&fc, xattr);
	if (r) {
		DEBUG(("Fxattr(%s): returning %ld", name, r));
	}
	release_cookie(&fc);
	return r;
}

/*
 * GEMDOS extension: Flink(old, new) creates a hard link named "new"
 * to the file "old".
 */

long ARGS_ON_STACK
f_link(old, new)
	const char *old, *new;
{
	fcookie olddir, newdir;
	char temp1[PATH_MAX], temp2[PATH_MAX];
	long r;

	TRACE(("Flink(%s, %s)", old, new));

	r = path2cookie(old, temp2, &olddir);
	if (r) {
		DEBUG(("Flink(%s,%s): error parsing old name",old,new));
		return r;
	}
	r = path2cookie(new, temp1, &newdir);
	if (r) {
		DEBUG(("Flink(%s,%s): error parsing new name",old,new));
		release_cookie(&olddir);
		return r;
	}

	if (newdir.fs != olddir.fs) {
		DEBUG(("Flink(%s,%s): different file systems",old,new));
		release_cookie(&olddir);
		release_cookie(&newdir);
		return EXDEV;	/* cross device link */
	}

/* check for write permission on the destination directory */

	r = dir_access(&newdir, S_IWOTH);
	if (r) {
		DEBUG(("Flink(%s,%s): access to directory denied",old,new));
	} else
		r = (*newdir.fs->hardlink)(&olddir, temp2, &newdir, temp1);
	release_cookie(&olddir);
	release_cookie(&newdir);
	return r;
}

/*
 * GEMDOS extension: Fsymlink(old, new): create a symbolic link named
 * "new" that contains the path "old".
 */

long ARGS_ON_STACK
f_symlink(old, new)
	const char *old, *new;
{
	fcookie newdir;
	long r;
	char temp1[PATH_MAX];

	TRACE(("Fsymlink(%s, %s)", old, new));

	r = path2cookie(new, temp1, &newdir);
	if (r) {
		DEBUG(("Fsymlink(%s,%s): error parsing %s", old,new,new));
		return r;
	}
	r = dir_access(&newdir, S_IWOTH);
	if (r) {
		DEBUG(("Fsymlink(%s,%s): access to directory denied",old,new));
	} else
		r = (*newdir.fs->symlink)(&newdir, temp1, old);
	release_cookie(&newdir);
	return r;
}

/*
 * GEMDOS extension: Freadlink(buflen, buf, linkfile):
 * read the contents of the symbolic link "linkfile" into the buffer
 * "buf", which has length "buflen".
 */

long ARGS_ON_STACK
f_readlink(buflen, buf, linkfile)
	int buflen;
	char *buf;
	const char *linkfile;
{
	fcookie file;
	long r;
	XATTR xattr;

	TRACE(("Freadlink(%s)", linkfile));

	r = path2cookie(linkfile, (char *)0, &file);
	if (r) {
		DEBUG(("Freadlink: unable to find %s", linkfile));
		return r;
	}
	r = (*file.fs->getxattr)(&file, &xattr);
	if (r) {
		DEBUG(("Freadlink: unable to get attributes for %s", linkfile));
	} else if ( (xattr.mode & S_IFMT) == S_IFLNK )
		r = (*file.fs->readlink)(&file, buf, buflen);
	else {
		DEBUG(("Freadlink: %s is not a link", linkfile));
		r = EACCDN;
	}
	release_cookie(&file);
	return r;
}

/*
 * GEMDOS extension: Dcntl(): do file system specific functions
 */

long ARGS_ON_STACK
d_cntl(cmd, name, arg)
	int cmd;
	const char *name;
	long arg;
{
	fcookie dir;
	long r;
	char temp1[PATH_MAX];

	TRACE(("Dcntl(cmd=%x, file=%s, arg=%lx)", cmd, name, arg));

	r = path2cookie(name, temp1, &dir);
	if (r) {
		DEBUG(("Dcntl: couldn't find %s", name));
		return r;
	}
	r = (*dir.fs->fscntl)(&dir, temp1, cmd, arg);
	release_cookie(&dir);
	return r;
}

/*
 * GEMDOS extension: Fchown(name, uid, gid) changes the user and group
 * ownerships of a file to "uid" and "gid" respectively.
 */

long ARGS_ON_STACK
f_chown(name, uid, gid)
	const char *name;
	int uid, gid;
{
	fcookie fc;
	XATTR xattr;
	long r;

	TRACE(("Fchown(%s, %d, %d)", name, uid, gid));

	r = path2cookie(name, NULL, &fc);
	if (r) {
		DEBUG(("Fchown(%s): error %ld", name, r));
		return r;
	}

/* MiNT acts like _POSIX_CHOWN_RESTRICTED: a non-privileged process can
 * only change the ownership of a file that is owned by this user, to
 * the effective group id of the process
 */
	if (curproc->euid) {
		if (curproc->egid != gid)
			r = EACCDN;
		else
			r = (*fc.fs->getxattr)(&fc, &xattr);
		if (r) {
			DEBUG(("Fchown(%s): unable to get file attributes",name));
			release_cookie(&fc);
			return r;
		}
		if (xattr.uid != curproc->euid || xattr.uid != uid) {
			DEBUG(("Fchown(%s): not the file's owner",name));
			release_cookie(&fc);
			return EACCDN;
		}
	}
	r = (*fc.fs->chown)(&fc, uid, gid);
	release_cookie(&fc);
	return r;
}

/*
 * GEMDOS extension: Fchmod(file, mode) changes a file's access
 * permissions.
 */

long ARGS_ON_STACK
f_chmod(name, mode)
	const char *name;
	unsigned mode;
{
	fcookie fc;
	long r;
	XATTR xattr;

	TRACE(("Fchmod(%s, %o)", name, mode));
	r = path2cookie(name, follow_links, &fc);
	if (r) {
		DEBUG(("Fchmod(%s): error %ld", name, r));
		return r;
	}
	r = (*fc.fs->getxattr)(&fc, &xattr);
	if (r) {
		DEBUG(("Fchmod(%s): couldn't get file attributes",name));
	}
	else if (curproc->euid && curproc->euid != xattr.uid) {
		DEBUG(("Fchmod(%s): not the file's owner",name));
		r = EACCDN;
	} else {
		r = (*fc.fs->chmode)(&fc, mode & ~S_IFMT);
		if (r) DEBUG(("Fchmod: error %ld", r));
	}
	release_cookie(&fc);
	return r;
}

/*
 * GEMDOS extension: Dlock(mode, dev): locks or unlocks access to
 * a BIOS device. "mode" bit 0 is 0 for unlock, 1 for lock; "dev" is a
 * BIOS device (0 for A:, 1 for B:, etc.).
 *
 * Returns: 0 if the operation was successful
 *          EACCDN if a lock attempt is made on a drive that is being
 *            used
 *	    ELOCKED if the drive is locked by another process
 *	    ENSLOCK if a program attempts to unlock a drive it
 *            hasn't locked.
 * ++jr: if mode bit 1 is set, then instead of returning ELOCKED the
 * pid of the process which has locked the drive is returned (unless
 * it was locked by pid 0, in which case ELOCKED is still returned).
 */

PROC *dlockproc[NUM_DRIVES];

long ARGS_ON_STACK
d_lock(mode, dev)
	int mode, dev;
{
	PROC *p;
	FILEPTR *f;
	int i;

	TRACE(("Dlock(%x,%c:)", mode, dev+'A'));
	if (dev < 0 || dev >= NUM_DRIVES) return EDRIVE;
	if (aliasdrv[dev]) {
		dev = aliasdrv[dev] - 1;
		if (dev < 0 || dev >= NUM_DRIVES)
			return EDRIVE;
	}
	if ( (mode&1) == 0) {	/* unlock */
		if (dlockproc[dev] == curproc) {
			dlockproc[dev] = 0;
			changedrv(dev);
			return 0;
		}
		DEBUG(("Dlock: no such lock"));
		return ENSLOCK;
	}

/* code for locking */
/* is the drive already locked? */
	if (dlockproc[dev]) {
		DEBUG(("Dlock: drive already locked"));
#if 0
		if (dlockproc[dev] == curproc) return 0;
#endif
		if (dlockproc[dev]->pid == 0) return ELOCKED;
		return (mode & 2) ? dlockproc[dev]->pid : ELOCKED;
	}
/* see if the drive is in use */
	for (p = proclist; p; p = p->gl_next) {
		if (p->wait_q == ZOMBIE_Q || p->wait_q == TSR_Q)
			continue;
		for (i = MIN_HANDLE; i < MAX_OPEN; i++) {
			if ( ((f = p->handle[i]) != 0) && 
			     (f != (FILEPTR *)1) && (f->fc.dev == dev) ) {
		DEBUG(("Dlock: process %d has an open handle on the drive", p->pid));
				if (p->pid == 0) return EACCDN;
				return (mode & 2) ? p->pid : EACCDN;
			}
		}
	}

/* if we reach here, the drive is not in use */
/* we lock it by setting dlockproc and by setting all root and current
 * directories referring to the device to a null file system
 */
	for (p = proclist; p; p = p->gl_next) {
		for (i = 0; i < NUM_DRIVES; i++) {
			if (p->root[i].dev == dev) {
				release_cookie(&p->root[i]);
				p->root[i].fs = 0;
			}
			if (p->curdir[i].dev == dev) {
				release_cookie(&p->curdir[i]);
				p->curdir[i].fs = 0;
			}
		}
	}

	dlockproc[dev] = curproc;
	return 0;
}

/* jr: GEMDOS-extensions Dreadlabel() and Dwritelabel() */

long ARGS_ON_STACK
d_readlabel(name, label, namelen)
	const char *name;
	char *label;
	int namelen;
{
	fcookie dir;
	long r;

	r = path2cookie(name, (char *)0, &dir);
	if (r) {
		DEBUG(("Dreadlabel(%s): bad path",name));
		return r;
	}
	r = (*dir.fs->readlabel)(&dir, label, namelen);

	release_cookie(&dir);
	return r;
}

long ARGS_ON_STACK
d_writelabel(name, label)
	const char *name;
	const char *label;
{
	fcookie dir;
	long r;

	r = path2cookie(name, (char *)0, &dir);
	if (r) {
		DEBUG(("Dwritelabel(%s): bad path",name));
		return r;
	}
	r = (*dir.fs->writelabel)(&dir, label);

	release_cookie(&dir);
	return r;
}
