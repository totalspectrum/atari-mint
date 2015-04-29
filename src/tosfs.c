/*
Copyright 1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
*/

/* a VERY simple tosfs.c 
 * this one is extremely brain-damaged, but will serve OK for a
 * skeleton in which to put a "real" tosfs.c
 */

#include "mint.h"

/* if NEWWAY is defined, tosfs uses the new dup_cookie/release_cookie
 * protocol to keep track of file cookies, instead of the old
 * method of "timing"
 */
/* #define NEWWAY */
#if 0
#define COOKIE_DB(x) DEBUG(x)
#else
#define COOKIE_DB(x)
#endif

/* if RO_FASCISM is defined, the read/write modes are enforced. This is
 * a Good Thing, not fascist at all. Ask Allan Pratt why he chose
 * that name sometime.
 */
#define RO_FASCISM

/* temporary code for debugging Falcon media change bug */
#if 0
#define MEDIA_DB(x) DEBUG(x)
#else
#define MEDIA_DB(x)
#endif

/* search mask for anything OTHER THAN a volume label */
#define FILEORDIR 0x37

char tmpbuf[PATH_MAX+1];

static long	ARGS_ON_STACK tos_root	P_((int drv, fcookie *fc));
static long	ARGS_ON_STACK tos_lookup	P_((fcookie *dir, const char *name, fcookie *fc));
static long	ARGS_ON_STACK tos_getxattr	P_((fcookie *fc, XATTR *xattr));
static long	ARGS_ON_STACK tos_chattr	P_((fcookie *fc, int attrib));
static long	ARGS_ON_STACK tos_chown	P_((fcookie *fc, int uid, int gid));
static long	ARGS_ON_STACK tos_chmode	P_((fcookie *fc, unsigned mode));
static long	ARGS_ON_STACK tos_mkdir	P_((fcookie *dir, const char *name, unsigned mode));
static long	ARGS_ON_STACK tos_rmdir	P_((fcookie *dir, const char *name));
static long	ARGS_ON_STACK tos_remove	P_((fcookie *dir, const char *name));
static long	ARGS_ON_STACK tos_getname	P_((fcookie *root, fcookie *dir,
						    char *pathname, int size));
static long	ARGS_ON_STACK tos_rename	P_((fcookie *olddir, char *oldname,
				    fcookie *newdir, const char *newname));
static long	ARGS_ON_STACK tos_opendir	P_((DIR *dirh, int flags));
static long	ARGS_ON_STACK tos_readdir	P_((DIR *dirh, char *nm, int nmlen, fcookie *));
static long	ARGS_ON_STACK tos_rewinddir	P_((DIR *dirh));
static long	ARGS_ON_STACK tos_closedir	P_((DIR *dirh));
static long	ARGS_ON_STACK tos_pathconf	P_((fcookie *dir, int which));
static long	ARGS_ON_STACK tos_dfree	P_((fcookie *dir, long *buf));
static long	ARGS_ON_STACK tos_writelabel	P_((fcookie *dir, const char *name));
static long	ARGS_ON_STACK tos_readlabel	P_((fcookie *dir, char *name, int namelen));

static long	ARGS_ON_STACK tos_creat	P_((fcookie *dir, const char *name, unsigned mode,
				    int attrib, fcookie *fc));
static DEVDRV *	ARGS_ON_STACK tos_getdev	P_((fcookie *fc, long *devsp));
static long	ARGS_ON_STACK tos_open	P_((FILEPTR *f));
static long	ARGS_ON_STACK tos_write	P_((FILEPTR *f, const char *buf, long bytes));
static long	ARGS_ON_STACK tos_read	P_((FILEPTR *f, char *buf, long bytes));
static long	ARGS_ON_STACK tos_lseek	P_((FILEPTR *f, long where, int whence));
static long	ARGS_ON_STACK tos_ioctl	P_((FILEPTR *f, int mode, void *buf));
static long	ARGS_ON_STACK tos_datime	P_((FILEPTR *f, short *time, int rwflag));
static long	ARGS_ON_STACK tos_close	P_((FILEPTR *f, int pid));
static long	ARGS_ON_STACK tos_dskchng	P_((int drv));

#ifdef NEWWAY
static long	ARGS_ON_STACK tos_release P_((fcookie *fc));
static long	ARGS_ON_STACK tos_dupcookie P_((fcookie *dst, fcookie *src));
#endif

/* some routines from biosfs.c */
extern long	ARGS_ON_STACK null_select	P_((FILEPTR *f, long p, int mode));
extern void	ARGS_ON_STACK null_unselect	P_((FILEPTR *f, long p, int mode));

DEVDRV tos_device = {
	tos_open, tos_write, tos_read, tos_lseek, tos_ioctl, tos_datime,
	tos_close, null_select, null_unselect
};

FILESYS tos_filesys = {
	(FILESYS *)0,
	FS_KNOPARSE | FS_NOXBIT | FS_LONGPATH,
	tos_root,
	tos_lookup, tos_creat, tos_getdev, tos_getxattr,
	tos_chattr, tos_chown, tos_chmode,
	tos_mkdir, tos_rmdir, tos_remove, tos_getname, tos_rename,
	tos_opendir, tos_readdir, tos_rewinddir, tos_closedir,
	tos_pathconf, tos_dfree, tos_writelabel, tos_readlabel,
	nosymlink, noreadlink, nohardlink, nofscntl, tos_dskchng,
#ifdef NEWWAY
	tos_release, tos_dupcookie
#else
	0, 0
#endif
};

/* some utility functions and variables: see end of file */
static DTABUF 	*lastdta;	/* last DTA buffer we asked TOS about */
static DTABUF	foo;
static void do_setdta P_((DTABUF *dta));
static int executable_extension P_((char *));

/* this array keeps track of which drives have been changed */
/* a nonzero entry means that the corresponding drive has been changed,
 * but GEMDOS doesn't know it yet
 */
static char drvchanged[NUM_DRIVES];

/* force TOS to see a media change */
static void force_mediach P_((int drv));
static long ARGS_ON_STACK Newgetbpb P_((int));
static long ARGS_ON_STACK Newmediach P_((int));
static long ARGS_ON_STACK Newrwabs P_((int, void *, int, int, int, long));

#ifdef NEWWAY
#define NUM_INDICES 64
#else
#define NUM_INDICES 128
#define MIN_AGE 8
#endif

struct tindex {
	char *name;		/* full path name */
	FILEPTR *open;		/* fileptrs for this file; OR
				 * count of number of open directories
				 */
	LOCK *locks;		/* locks on this file */
/* file status */
	long  size;
	short time;
	short date;
	short attr;
	short valid;		/* 1 if the above status is still valid */
#ifdef NEWWAY
	short links;		/* how many times index is in use */
#else
	short stamp;		/* age of this index, for garbage collection */
#endif
} gl_ti[NUM_INDICES];

/* temporary index for files found by readdir */
static struct tindex tmpindex;
static char tmpiname[PATH_MAX];

static struct tindex *tstrindex P_((char *s));
static int tfullpath P_((char *result, struct tindex *base, const char *name));
static struct tindex *garbage_collect P_((void));

#ifndef NEWWAY
static short tclock;		/* #calls to tfullpath since last garbage
				   collection */
#endif


#define ROOTPERMS 32
#ifdef ROOTPERMS
/* jr: save access permissions, owner and group for root directories.
   ROOTPERMS is the number of supported drives; 'mode' has ROOTPERMSET
   if it has been set  */

struct tosrootperm {
	ushort uid, gid, mode;
} root_perms[ROOTPERMS];

#define ROOTPERMSET	0x8000
#endif

/* jr: external table, set up by getbpb in bios.c */
extern unsigned short clsizb[];

#define INODE_PER_CRC

#ifdef INODE_PER_CRC

/* jr: use crc algorithm described in P1003.2, D11.2 with crctab as in
   GNU-cksum. Slightly modified to return 0 for the root (instead of ~0) */

static unsigned long crctab[] = { /* CRC polynomial 0xedb88320 */
0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
0x0edb8832L, 0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L,
0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL, 0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L,
0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L, 0xfa0f3d63L, 0x8d080df5L,
0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L, 0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L,
0x26d930acL, 0x51de003aL, 0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL,
0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L, 0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL,
0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L,
0x7807c9a2L, 0x0f00f934L, 0x9609a88eL, 0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L,
0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L,
0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L, 0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL,
0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L, 0xaa0a4c5fL, 0xdd0d7cc9L,
0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL,
0xedb88320L, 0x9abfb3b6L, 0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L, 0x73dc1683L,
0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L, 0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L,
0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L,
0xfed41b76L, 0x89d32be0L, 0x10da7a5aL, 0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL,
0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L,
0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L, 0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL,
0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL,
0x9b64c2b0L, 0xec63f226L, 0x756aa39cL, 0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L,
0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L, 0x18b74777L,
0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL, 0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L,
0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L, 0x4969474dL, 0x3e6e77dbL,
0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL,
0xb3667a2eL, 0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL
};

#define UPDC32(octet, crc) (((crc) << 8) ^ crctab[((crc) >> 24) ^ (octet)])

static unsigned long
filename_crc (const char *filename)
{
	unsigned long s = 0;
	unsigned int n = 0;
	
	/* skip x: */
	filename += 2;
	
	while (*filename) {
		s = UPDC32 (*filename++, s);
		n++;
	}

	while (n != 0) {
		s = UPDC32 (n & 0377, s);
		n >>= 8;
	}
	
	return s;
}

#endif


/* some extra flags for the attr field */

/*
 * is a string the name of a file with executable extension?
 */
#define FA_EXEC 0x4000
/*
 * should the file be deleted when it is closed?
 */
#define FA_DELETE 0x2000

/*
 * NOTE: call executable_extension only on a DTA name returned from
 * Fsfirst(), not on an arbitrary path, for two reasons: (1) it
 * expects only upper case, and (2) it looks only for the 1st extension,
 * so a folder with a '.' in its name would confuse it.
 */

static int
executable_extension(s)
	char *s;
{
	while (*s && *s != '.') s++;
	if (!*s) return 0;
	s++;
	if (s[0] == 'T') {
		return (s[1] == 'T' && s[2] == 'P') ||
		       (s[1] == 'O' && s[2] == 'S');
	}
	if (s[0] == 'P')
		return s[1] == 'R' && s[2] == 'G';
	if (s[0] == 'A')
		return s[1] == 'P' && s[2] == 'P';
	if (s[0] == 'G')
		return s[1] == 'T' && s[2] == 'P';
	return 0;
}

/*
 * Look in the table of tos indices to see if an index corresponding
 * to this file name already exists. If so, mark it as being used
 * and return it. If not, find an empty slot and make an index for
 * this string. If no empty slots exist, garbage collect and
 * try again.
 *
 * This routine is pretty dumb; we really should use a hash table
 * of some sort
 */

static struct tindex *tstrindex(s)
	char *s;
{
	int i;
	char *r;
	struct tindex *t, *free = 0;

	assert(s != 0);
	t = gl_ti;
	for (i = 0; i < NUM_INDICES; i++, t++) {
		if (t->name && !stricmp(t->name, s)) {
#ifndef NEWWAY
			t->stamp = tclock;	/* update use time */
#endif
			return t;
		}
		else if (!t->name && !free)
			free = t;
	}
	if (!free) {
		free = garbage_collect();
	}
#ifdef NEWWAY
	if (!free) {
		FORCE("tosfs: all slots in use!!");
		FORCE("Links\tName");
		t = gl_ti;
		for (i = 0; i < NUM_INDICES; i++,t++) {
			FORCE("%d\t%s", t->links, t->name);
		}
		FATAL("tosfs: unable to get a file name index");
	}
#else
	if (!free) {
		FATAL("tosfs: unable to get a file name index");
	}
#endif
	r = kmalloc((long)strlen(s)+1);
	if (!r) {
		FATAL("tosfs: unable to allocate space for a file name");
	}
	strcpy(r, s);
	free->name = r;
#ifdef NEWWAY
	free->links = 0;
#else
	free->stamp = tclock;
#endif
	free->open = 0;
	free->locks = 0;

/* check to see if this file was recently returned by opendir() */
#ifndef NEWWAY
	if (tmpindex.valid && tclock - tmpindex.stamp < MIN_AGE &&
	    !stricmp(free->name, tmpindex.name)) {
		free->size = tmpindex.size;
		free->time = tmpindex.time;
		free->date = tmpindex.date;
		free->attr = tmpindex.attr;
		free->valid = 1;
		tmpindex.valid = 0;
	} else
#endif
	{
		free->valid = 0;
		free->attr = 0;
	}
	return free;
}

/*
 * garbage collection routine: for any TOS index older than MIN_AGE,
 * check through all current processes to see if it's in use. If
 * not, free the corresponding string.
 * Returns: a pointer to a newly freed index, or NULL.
 */

/* it's unlikely that the kernel would need to hold onto a file cookie
   for longer than this many calls to tstrindex() without first
   saving the cookie in a directory or file pointer
 */

static struct tindex *
garbage_collect()
{
	struct tindex *free, *t;
	int i;
#ifndef NEWWAY
	fcookie *fc, *gc;
	PROC *p;
	int j;
	int age;
#endif

	free = 0;
	t = gl_ti;
	for (i = 0; i < NUM_INDICES; i++,t++) {
		if (!t->name) continue;
#ifdef NEWWAY
		if (t->links == 0) {
			kfree(t->name);
			t->name = 0;
			if (!free) free = t;
		}
#else
		age = tclock - t->stamp;
		t->stamp = 0;
		assert(age >= 0);
		if (age > MIN_AGE) {
		/* see if any process is using this index */
			if (t->open)
				goto found_index;
			for (p = proclist; p; p = p->gl_next) {
				fc = p->curdir;
				gc = p->root;
				for (j = 0; j < NUM_DRIVES; j++,fc++,gc++) {
					if (( fc->fs == &tos_filesys &&
					      fc->index == (long)t ) ||
					    ( gc->fs == &tos_filesys &&
					      gc->index == (long)t ) )
						goto found_index;
				}
			}
		/* here, we couldn't find the index in use by any proc. */
			kfree(t->name);
			t->name = 0;
			if (!free)
				free = t;
		found_index:
			;
		} else {
	/* make sure that future garbage collections might look at this file */
			t->stamp = -age;
		}
#endif
	}

#ifndef NEWWAY
	tclock = 0;	/* reset the clock */
	tmpindex.valid = 0; /* expire the temporary Fsfirst buffer */
#endif
	return free;
}

#define DIRSEP(c) ((c) == '\\')

static int
tfullpath(result, basei, path)
	char *result;
	struct tindex *basei;
	const char *path;
{
#define TNMTEMP 32
	char *n, name[TNMTEMP+1];
	int namelen, pathlen;
	char *base = basei->name;
	int r = 0;

#ifndef NEWWAY
	basei->stamp = ++tclock;
	if (tclock > 10000) {
	/* garbage collect every so often whether we need it or not */
		(void)garbage_collect();
	}
#endif
	if (!*path) {
		strncpy(result, base, PATH_MAX-1);
		return r;
	}

	strncpy(result, base, PATH_MAX-1);

	pathlen = strlen(result);

/* now path is relative to what's currently in "result" */

	while(*path) {
/* get next name in path */
		n = name; namelen = 0;
		while (*path && !DIRSEP(*path)) {
/* BUG: we really should to the translation to DOS 8.3
 * format *here*, so that really long names are truncated
 * correctly.
 */
			if (namelen < TNMTEMP) {
				*n++ = toupper(*path); path++; namelen++;
			}
			else
				path++;
		}
		*n = 0;
		while (DIRSEP(*path)) path++;
/* check for "." and ".." */
		if (!strcmp(name, ".")) continue;
		if (!strcmp(name, "..")) {
			n = strrchr(result, '\\');
			if (n) {
				*n = 0;
				pathlen = (int)(n - result);
			}
			else r = EMOUNT;
			continue;
		}
		if (pathlen + namelen < PATH_MAX - 1) {
			strcat(result, "\\");
			pathlen++;

	/* make sure the name is restricted to DOS 8.3 format */
			for (base = result; *base; base++)
				;
			n = name;
			namelen = 0;
			while (*n && *n != '.' && namelen++ < 8) {
				*base++ = *n++;
				pathlen++;
			}
			while (*n && *n != '.') n++;
			if (*n == '.' && *(n+1) != 0) {
				*base++ = *n++;
				pathlen++;
				namelen = 0;
				while (*n && namelen++ < 3) {
					*base++ = *n++;
					pathlen++;
				}
			}
			*base = 0;
		}
	}
	return r;
}

static long ARGS_ON_STACK 
tos_root(drv, fc)
	int drv;
	fcookie *fc;
{
	struct tindex *ti;

	ksprintf(tmpbuf, "%c:", drv+'A');
	fc->fs = &tos_filesys;
	fc->dev = drv;
	ti = tstrindex(tmpbuf);
	ti->size = ti->date = ti->time = 0;
	ti->attr = FA_DIR;
	ti->valid = 1;
	fc->index = (long)ti;

/* if the drive has changed, make sure GEMDOS knows it! */
	if (drvchanged[drv]) {
		force_mediach(drv);
	}
#ifdef NEWWAY
	ti->links++;
#endif
	return 0;
}

static long ARGS_ON_STACK 
tos_lookup(dir, name, fc)
	fcookie *dir;
	const char *name;
	fcookie *fc;
{
	long r;
	struct tindex *ti = (struct tindex *)dir->index;

	r = tfullpath(tmpbuf, ti, name);

/* if the name is empty or otherwise trivial, just return the directory */
	if (!strcmp(ti->name, tmpbuf)) {
		*fc = *dir;
#ifdef NEWWAY
		ti->links++;
		COOKIE_DB(("tos_lookup: %s now has %d links", ti->name, ti->links));
#endif 
		return r;
	}

/* is there already an index for this file?? If so, is it up to date?? */
	ti = tstrindex(tmpbuf);
	if (!ti->valid) {
		if (tmpbuf[1] == ':' && tmpbuf[2] == 0) {
			/* a root directory -- lookup always succeeds */
			foo.dta_size = 0;
			foo.dta_date = foo.dta_time = 0;
			foo.dta_attrib = FA_DIR;
			foo.dta_name[0] = 0;
		} else {
			do_setdta(&foo);
			r = Fsfirst(tmpbuf, FILEORDIR);
			if (r) {
DEBUG(("tos_lookup: Fsfirst(%s) returned %ld", tmpbuf, r));
				return r;
			}
		}
		ti->size = foo.dta_size;
		ti->date = foo.dta_date;
		ti->time = foo.dta_time;
		ti->attr = foo.dta_attrib | (ti->attr & FA_DELETE);
		if (executable_extension(foo.dta_name))
			ti->attr |= FA_EXEC;
		ti->valid = 1;
	}
	fc->fs = &tos_filesys;
	fc->index = (long)ti;
	fc->dev = dir->dev;
#ifdef NEWWAY
	ti->links++;
	COOKIE_DB(("tos_lookup: %s now has %d links", ti->name, ti->links));
#endif
	return r;
}

static long ARGS_ON_STACK 
tos_getxattr(fc, xattr)
	fcookie *fc;
	XATTR *xattr;
{
	struct tindex *ti = (struct tindex *)fc->index;
	long r;
#ifndef INODE_PER_CRC
	static long junkindex = 0;
#endif
#ifdef ROOTPERMS
	struct tosrootperm *tp = NULL;
	
	if (fc->dev < ROOTPERMS) tp = &root_perms[fc->dev];
#endif

#ifdef INODE_PER_CRC
	xattr->index = filename_crc (ti->name);
#else
	xattr->index = junkindex++;
#endif

	xattr->dev = fc->dev;
	xattr->rdev = fc->dev;
	xattr->nlink = 1;

	xattr->uid = xattr->gid = 0;

#ifdef ROOTPERMS
	if (tp) {
		xattr->uid = tp->uid;
		xattr->gid = tp->gid;
	}
#endif

#ifndef NEWWAY
	ti->stamp = ++tclock;
#endif

	if (!ti->valid) {
		do_setdta(&foo);
		if (ti->name[2] == 0) {		/* a root directory */
/* actually, this can also happen if a program tries to open a file
 * with an empty name... so we should fail gracefully
 */
			ti->attr = FA_DIR;
			ti->size = 0;
			ti->date = ti->time = 0;
			goto around;
		}
	
		r = Fsfirst(ti->name, FILEORDIR);
		if (r) {
			DEBUG(("tosfs: search error %ld on [%s]", r, ti->name));
			return r;
		}
		ti->size = foo.dta_size;
		ti->date = foo.dta_date;
		ti->time = foo.dta_time;
		ti->attr = foo.dta_attrib | (ti->attr & FA_DELETE);
		if (executable_extension(foo.dta_name))
			ti->attr |= FA_EXEC;
around:
		ti->valid = 1;
	}
	xattr->size = ti->size;

	/* jr: if cluster size unknown, do a getbpb once */
	if (fc->dev < 32 && ! clsizb[fc->dev])
		getbpb (fc->dev);

	xattr->blksize = 1024;
	if (fc->dev < 32 && clsizb[fc->dev])
		xattr->blksize = clsizb[fc->dev];
	
	xattr->nblocks = (xattr->size + xattr->blksize - 1) / xattr->blksize;
	xattr->mdate = xattr->cdate = xattr->adate = ti->date;
	xattr->mtime = xattr->ctime = xattr->atime = ti->time;
	xattr->mode = (ti->attr & FA_DIR) ? (S_IFDIR | DEFAULT_DIRMODE) :
			 (S_IFREG | DEFAULT_MODE);

/* TOS files have permissions rwxrwx--- */
	xattr->mode &= ~(S_IROTH|S_IWOTH|S_IXOTH);

#ifdef ROOTPERMS
	/* when root permissions are set, use them. For regular files,
	unmask x bits */

	if (tp && (tp->mode & ROOTPERMSET)) {
		xattr->mode &= ~DEFAULT_DIRMODE;
		xattr->mode |= (tp->mode & DEFAULT_DIRMODE);

		if (! (ti->attr & FA_DIR))
			xattr->mode &= ~(S_IXUSR|S_IXGRP|S_IXOTH);
	}
#endif

	if (ti->attr & FA_RDONLY) {
		xattr->mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
	}

	if (ti->attr & FA_EXEC) {
		xattr->mode |= (S_IXUSR|S_IXGRP|S_IXOTH);
	}

	xattr->attr = ti->attr & 0xff;
	return 0;
}

static long ARGS_ON_STACK
tos_chattr(fc, attrib)
	fcookie *fc;
	int attrib;
{
	struct tindex *ti = (struct tindex *)fc->index;

	if (ti->attr & FA_DIR) {
		DEBUG(("error: attempt to change attributes of a directory"));
		return EACCDN;
	}
	ti->valid = 0;
	(void)tfullpath(tmpbuf, ti, "");
	return Fattrib(tmpbuf, 1, attrib);
}

static long ARGS_ON_STACK 
tos_chown(dir, uid, gid)
	fcookie *dir;
	int uid, gid;
{
#ifdef ROOTPERMS
	struct tindex *ti = (struct tindex *)dir->index;

	if (dir->dev < ROOTPERMS)
		if (ti->name[2] == '\0')	/* root? */
		{
			root_perms[dir->dev].uid = uid;
			root_perms[dir->dev].gid = gid;
			return 0;	
		}
	
	return EINVFN;
	
#else
	UNUSED(dir); UNUSED(uid); UNUSED(gid);
	return EINVFN;
#endif
}

static long ARGS_ON_STACK 
tos_chmode(fc, mode)
	fcookie *fc;
	unsigned mode;
{
	int oldattr, newattr;
	long r;
	struct tindex *ti = (struct tindex *)fc->index;

#ifdef ROOTPERMS
	if (fc->dev < ROOTPERMS) {
		if (ti->name[2] == '\0') {
			/* root? */
			root_perms[fc->dev].mode = ROOTPERMSET | mode;
			return 0;
		}
	}
#endif

	oldattr = Fattrib(ti->name, 0, 0);
	if (oldattr < 0)
		return oldattr;

	ti->valid = 0;

	if (!(mode & S_IWUSR))
		newattr = oldattr | FA_RDONLY;
	else
		newattr = oldattr & ~FA_RDONLY;
	if (newattr != oldattr)
		r = Fattrib(ti->name, 1, newattr);
	else
		r = 0;
	return (r < 0) ? r : 0;
}

static long ARGS_ON_STACK 
tos_mkdir(dir, name, mode)
	fcookie *dir;
	const char *name;
	unsigned mode;		/* ignored under TOS */
{
	UNUSED(mode);

	(void)tfullpath(tmpbuf, (struct tindex *)dir->index, name);
	tmpindex.valid = 0;

	return Dcreate(tmpbuf);
}

static long ARGS_ON_STACK 
tos_rmdir(dir, name)
	fcookie *dir;
	const char *name;
{
	struct tindex *ti;

	(void)tfullpath(tmpbuf, (struct tindex *)dir->index, name);
	ti = tstrindex(tmpbuf);
	ti->valid = 0;

	return Ddelete(tmpbuf);
}

static long ARGS_ON_STACK 
tos_remove(dir, name)
	fcookie *dir;
	const char *name;
{
	struct tindex *ti;

	(void)tfullpath(tmpbuf, (struct tindex *)dir->index, name);

	ti = tstrindex(tmpbuf);
	if (ti->open) {
		DEBUG(("tos_remove: file is open, will be deleted later"));
		if (ti->attr & FA_RDONLY)
			return EACCDN;
		ti->attr |= FA_DELETE;
		return 0;
	}
	ti->valid = 0;
	return Fdelete(tmpbuf);
}

static long ARGS_ON_STACK 
tos_getname(root, dir, pathname, size)
	fcookie *root, *dir;
	char *pathname;
	int size;
{
	char *rootnam = ((struct tindex *)root->index)->name;
	char *dirnam = ((struct tindex *)dir->index)->name;
	int i;

	i = strlen(rootnam);
	if (strlen(dirnam) < i) {
		DEBUG(("tos_getname: root is longer than path"));
		return EINTRN;
	}
	if (strlen(dirnam+i) < size) {
		strcpy(pathname, dirnam + i);
/*
 * BUG: must be a better way to decide upper/lower case
 */
		if (curproc->domain == DOM_MINT)
			strlwr(pathname);
		return 0;
	} else {
		DEBUG(("tosfs: name too long"));
		return ERANGE;
	}
}

static long ARGS_ON_STACK 
tos_rename(olddir, oldname, newdir, newname)
	fcookie *olddir;
	char *oldname;
	fcookie *newdir;
	const char *newname;
{
	char newbuf[128];
	struct tindex *ti;
	long r;

	(void)tfullpath(tmpbuf, (struct tindex *)olddir->index, oldname);
	(void)tfullpath(newbuf, (struct tindex *)newdir->index, newname);
	r = Frename(0, tmpbuf, newbuf);
	if (r == 0) {
		ti = tstrindex(tmpbuf);
		kfree(ti->name);
		ti->name = kmalloc((long)strlen(newbuf)+1);
		if (!ti->name) {
			FATAL("tosfs: unable to allocate space for a name");
		}
		strcpy(ti->name, newbuf);
		ti->valid = 0;
	}
	return r;
}

#define DIR_FLAG(x)	(x->fsstuff[0])
#define STARTSEARCH	0	/* opendir() was just called */
#define INSEARCH	1	/* readdir() has been called at least once */
#define NMFILE		2	/* no more files to read */

#define DIR_DTA(x)	((DTABUF *)(x->fsstuff + 2))
#define DIR_NAME(x)	(x->fsstuff + 32)

/*
 * The directory functions are a bit tricky. What we do is have
 * opendir() do Fsfirst; the first readdir() picks up this name,
 * subsequent readdir()'s have to do Fsnext
 */

static long ARGS_ON_STACK 
tos_opendir(dirh, flags)
	DIR *dirh;
	int flags;
{
	long r;
	struct tindex *t = (struct tindex *)dirh->fc.index;

	UNUSED(flags);

	(void)tfullpath(tmpbuf, t, "*.*");

	do_setdta(DIR_DTA(dirh));

	r = Fsfirst(tmpbuf, FILEORDIR);

	if (r == 0) {
		t->open++;
		DIR_FLAG(dirh) = STARTSEARCH;
		return 0;
	} else if (r == EFILNF) {
		t->open++;
		DIR_FLAG(dirh) = NMFILE;
		return 0;
	}
 	return r;
}

static long ARGS_ON_STACK 
tos_readdir(dirh, name, namelen, fc)
	DIR *dirh;
	char *name;
	int namelen;
	fcookie *fc;
{
	static long index = 0;
	long ret;
	int giveindex = dirh->flags == 0;
	struct tindex *ti;
	DTABUF *dta = DIR_DTA(dirh);

again:
	if (DIR_FLAG(dirh) == NMFILE)
		return ENMFIL;

	if (DIR_FLAG(dirh) == STARTSEARCH) {
		DIR_FLAG(dirh) = INSEARCH;
	} else {
		assert(DIR_FLAG(dirh) == INSEARCH);
		do_setdta(dta);
		ret = Fsnext();
		if (ret) {
			DIR_FLAG(dirh) = NMFILE;
			return ret;
		}
	}

/* don't return volume labels from readdir */
	if (dta->dta_attrib == FA_LABEL) goto again;

	fc->fs = &tos_filesys;
	fc->dev = dirh->fc.dev;

	(void)tfullpath(tmpiname, (struct tindex *)dirh->fc.index, DIR_NAME(dirh));

	ti = &tmpindex;
	ti->name = tmpiname;
	ti->valid = 1;
	ti->size = dta->dta_size;
	ti->date = dta->dta_date;
	ti->time = dta->dta_time;
	ti->attr = dta->dta_attrib;
#ifndef NEWWAY
	ti->stamp = tclock;
#endif
	if (executable_extension(dta->dta_name))
		ti->attr |= FA_EXEC;
	fc->index = (long)ti;

	if (giveindex) {
		namelen -= (int) sizeof(long);
		if (namelen <= 0) return ERANGE;
		*((long *)name) = index++;
		name += sizeof(long);
	}
	strncpy(name, DIR_NAME(dirh), namelen-1);
	name[namelen-1] = 0;

/* BUG: we really should do the "strlwr" operation only
 * for Dreaddir (i.e. if giveindex == 0) but
 * unfortunately some old programs rely on the behaviour
 * below
 */
	if (curproc->domain == DOM_MINT) {
		strlwr(name);
	}
	if (strlen(DIR_NAME(dirh)) >= namelen)
		return ENAMETOOLONG;
#ifdef NEWWAY
	ti->links++;
	COOKIE_DB(("tos_readdir: %s now has %d links", ti->name, ti->links));
#endif
	return 0;
}

static long ARGS_ON_STACK 
tos_rewinddir(dirh)
	DIR *dirh;
{
	struct tindex *ti = (struct tindex *)dirh->fc.index;
	long r;

	(void)tfullpath(tmpbuf, ti, "*.*");
	do_setdta(DIR_DTA(dirh));
	r = Fsfirst(tmpbuf, FILEORDIR);
	if (r == 0) {
		DIR_FLAG(dirh) = STARTSEARCH;
	} else {
		DIR_FLAG(dirh) = NMFILE;
	}
	return r;
}

static long ARGS_ON_STACK 
tos_closedir(dirh)
	DIR *dirh;
{
	struct tindex *t = (struct tindex *)dirh->fc.index;

	if (t->open == 0) {
		FATAL("t->open == 0: directory == %s", t->name);
	}
	--t->open;
	DIR_FLAG(dirh) = NMFILE;
	return 0;
}

static long ARGS_ON_STACK 
tos_pathconf(dir, which)
	fcookie *dir;
	int which;
{
	UNUSED(dir);

	switch(which) {
	case -1:
		return DP_MAXREQ;
	case DP_IOPEN:
		return 60;	/* we can only keep about this many open */
	case DP_MAXLINKS:
		 return 1;	/* no hard links */
	case DP_PATHMAX:
		return PATH_MAX;
	case DP_NAMEMAX:
		return 8+3+1;
	case DP_ATOMIC:
		return 512;	/* we can write at least a sector atomically */
	case DP_TRUNC:
		return DP_DOSTRUNC;	/* DOS style file names */
	case DP_CASE:
		return DP_CASECONV;	/* names converted to upper case */
	case DP_MODEATTR:
		return FA_RDONLY|FA_HIDDEN|FA_SYSTEM|FA_LABEL|FA_CHANGED|
#ifdef ROOTPERMS
				0666L << 8|
#endif
				DP_FT_DIR|DP_FT_REG;
	case DP_XATTRFIELDS:
		return DP_DEV|DP_NLINK|DP_BLKSIZE|DP_SIZE|DP_NBLOCKS|DP_MTIME;
	default:
		return EINVFN;
	}
}

long ARGS_ON_STACK 
tos_dfree(dir, buf)
	fcookie *dir;
	long *buf;
{
	return Dfree(buf, (dir->dev)+1);
}

/*
 * writelabel: creates a volume label
 * readlabel: reads a volume label
 * both of these are only guaranteed to work in the root directory
 */

/*
 * BUG: this should first delete any old labels, so that it will
 * work with TOS <1.4
 */

long ARGS_ON_STACK 
tos_writelabel(dir, name)
	fcookie *dir;
	const char *name;
{
	long r;
	struct tindex *ti;

	(void)tfullpath(tmpbuf, (struct tindex *)dir->index, name);
	r = Fcreate(tmpbuf, FA_LABEL);
	if (r < 0) return r;
	(void)Fclose((int)r);
	ti = tstrindex(tmpbuf);
	ti->valid = 0;
	return 0;
}

long ARGS_ON_STACK 
tos_readlabel(dir, name, namelen)
	fcookie *dir;
	char *name;
	int namelen;
{
	long r;
	struct tindex *ti = (struct tindex *)dir->index;

	if (ti->name[2] != 0)		/* not a root directory? */
		return EFILNF;

	(void)tfullpath(tmpbuf, ti, "*.*");
	do_setdta(&foo);
	r = Fsfirst(tmpbuf, FA_LABEL);
	if (r)
		return r;
	strncpy(name, foo.dta_name, namelen-1);
	return (strlen(foo.dta_name) < namelen) ? 0 : ENAMETOOLONG;
}

/*
 * TOS creat: this doesn't actually create the file, rather it
 * sets up a (fake) index for the file that will be used by
 * the later tos_open call.
 */

static long ARGS_ON_STACK 
tos_creat(dir, name, mode, attrib, fc)
	fcookie *dir;
	const char *name;
	unsigned mode;
	int attrib;
	fcookie *fc;
{
	struct tindex *ti;
	UNUSED(mode);

	(void)tfullpath(tmpbuf, (struct tindex *)dir->index, name);

	ti = tstrindex(tmpbuf);
	ti->size = 0;
	ti->date = datestamp;
	ti->time = timestamp;
	ti->attr = attrib;
	ti->valid = 1;

	fc->fs = &tos_filesys;
	fc->index = (long)ti;
	fc->dev = dir->dev;
#ifdef NEWWAY
	ti->links++;
	COOKIE_DB(("tos_creat: %s now has %d links", ti->name, ti->links));
#endif
	return 0;
}
	
/*
 * TOS device driver
 */

static DEVDRV * ARGS_ON_STACK 
tos_getdev(fc, devsp)
	fcookie *fc;
	long *devsp;
{
	UNUSED(fc); UNUSED(devsp);
	return &tos_device;
}

static long ARGS_ON_STACK 
tos_open(f)
	FILEPTR *f;
{
	struct tindex *ti;
	int mode = f->flags;
	int tosmode;
	long r;
	extern int flk;		/* in main.c, set if _FLK cookie already present */

	ti = (struct tindex *)(f->fc.index);
	assert(ti != 0);

#ifndef NEWWAY
	ti->stamp = ++tclock;
#endif
	ti->valid = 0;

#ifndef RO_FASCISM
/* TEMPORARY HACK: change all modes to O_RDWR for files opened in
 * compatibility sharing mode. This is silly, but
 * allows broken TOS programs that write to read-only handles to continue
 * to work (it also helps file sharing, by making the realistic assumption
 * that any open TOS file can be written to). Eventually,
 * this should be tuneable by the user somehow.
 * ALSO: change O_COMPAT opens into O_DENYNONE; again, this may be temporary.
 */
	if ( (mode & O_SHMODE) == O_COMPAT ) {
		f->flags = (mode & ~(O_RWMODE|O_SHMODE)) | O_RDWR | O_DENYNONE;
	}
#endif

/* check to see that nobody has opened this file already in an
 * incompatible mode
 */
	if (denyshare(ti->open, f)) {
		TRACE(("tos_open: file sharing denied"));
		return EACCDN;
	}

/*
 * now open the file; if O_TRUNC was specified, actually
 * create the file anew.
 * BUG: O_TRUNC without O_CREAT doesn't work right. The kernel doesn't
 * use this mode, anyways
 */

	if (mode & O_TRUNC) {
		if (ti->open) {
			DEBUG(("tos_open: attempt to truncate an open file"));
			return EACCDN;
		}
		r = Fcreate(ti->name, ti->attr);
	} else {
		if (flk)
			tosmode = mode & (O_RWMODE|O_SHMODE);
		else
			tosmode = (mode & O_RWMODE);
		if (tosmode == O_EXEC) tosmode = O_RDONLY;

		r = Fopen(ti->name, tosmode );
		if (r == EFILNF && (mode & O_CREAT))
			r = Fcreate(ti->name, ti->attr);
	}

	if (r < 0) {
/* get rid of the index for the file, since it doesn't exist */
		kfree(ti->name);
		ti->name = 0;
		ti->valid = 0;
		return r;
	}

	f->devinfo = r;

	f->next = ti->open;
	ti->open = f;
	return 0;
}

static long ARGS_ON_STACK 
tos_write(f, buf, bytes)
	FILEPTR *f; const char *buf; long bytes;
{
	struct tindex *ti = (struct tindex *)f->fc.index;

	ti->valid = 0;
	return Fwrite((int)f->devinfo, bytes, buf);
}

static long ARGS_ON_STACK 
tos_read(f, buf, bytes)
	FILEPTR *f; char *buf; long bytes;
{
	return Fread((int)f->devinfo, bytes, buf);
}

static long ARGS_ON_STACK 
tos_lseek(f, where, whence)
	FILEPTR *f; long where; int whence;
{
	long r;

	r = Fseek(where, (int)f->devinfo, whence);
	return r;
}

static long ARGS_ON_STACK 
tos_ioctl(f, mode, buf)
	FILEPTR *f; int mode; void *buf;
{
	LOCK t, *lck, **old;
	struct flock *fl;
	long r;
	struct tindex *ti;
	extern int flk;		/* set in main.c if _FLK already installed */

	switch(mode) {
	case FIONREAD:
		r = Fseek (0L, (int) f->devinfo, 1);
		if (r < 0) return r;
		*(long *) buf = Fseek (0L, (int) f->devinfo, 2) - r;
		(void) Fseek (r, (int) f->devinfo, 0);
		return 0;
	case FIONWRITE:
		*((long *)buf) = 1;
		return 0;
	case FIOEXCEPT:
		*((long *)buf) = 0;
		return 0;
	case F_SETLK:
	case F_SETLKW:
	case F_GETLK:
		fl = ((struct flock *)buf);
		t.l = *fl;
		switch(t.l.l_whence) {
		case 0:
			break;
		case 1:		/* SEEK_CUR */
			r = Fseek(0L, (int)f->devinfo, 1);
			t.l.l_start += r;
			break;
		case 2:
			r = Fseek(0L, (int)f->devinfo, 1);
			t.l.l_start = Fseek(t.l.l_start, (int)f->devinfo, 2);
			(void)Fseek(r, (int)f->devinfo, 0);
			break;
		default:
			DEBUG(("Invalid value for l_whence"));
			return EINVFN;
		}
/* BUG: can't lock a file starting at >2gigabytes from the beginning */
		if (t.l.l_start < 0) t.l.l_start = 0;
		t.l.l_whence = 0;
		ti = (struct tindex *)f->fc.index;

		if (mode == F_GETLK) {
			lck = denylock(ti->locks, &t);
			if (lck)
				*fl = lck->l;
			else
				fl->l_type = F_UNLCK;
			return 0;
		}

		if (t.l.l_type == F_UNLCK) {
		/* try to find the lock */
			old = &ti->locks;
			lck = *old;
			while (lck) {
				if (lck->l.l_pid == curproc->pid &&
				    lck->l.l_start == t.l.l_start &&
				    lck->l.l_len == t.l.l_len) {
		/* found it -- remove the lock */
					*old = lck->next;
					TRACE(("tosfs: unlocked %s: %ld + %ld",
						ti->name, t.l.l_start, t.l.l_len));
					if (flk)
					    (void)Flock((int)f->devinfo, 1,
							t.l.l_start, t.l.l_len);
				/* wake up anyone waiting on the lock */
					wake(IO_Q, (long)lck);
					kfree(lck);
					break;
				}
				old = &lck->next;
				lck = lck->next;
			}
			return lck ?  0 : ENSLOCK;
		}
		TRACE(("tosfs: lock %s: %ld + %ld", ti->name,
			t.l.l_start, t.l.l_len));
		do {
		/* see if there's a conflicting lock */
			while ((lck = denylock(ti->locks, &t)) != 0) {
				DEBUG(("tosfs: lock conflicts with one held by %d",
					lck->l.l_pid));
				if (mode == F_SETLKW) {
					sleep(IO_Q, (long)lck);		/* sleep a while */
				}
				else
					return ELOCKED;
			}
		/* if not, add this lock to the list */
			lck = kmalloc(SIZEOF(LOCK));
			if (!lck) return ENSMEM;
		/* see if other _FLK code might object */
			if (flk) {
				r = Flock((int)f->devinfo, 0, t.l.l_start, t.l.l_len);
				if (r) {
					kfree(lck);
					if (mode == F_SETLKW && r == ELOCKED) {
						yield();
						lck = NULL;
					}
					else
						return r;
				}
			}
		} while (!lck);
		lck->l = t.l;
		lck->l.l_pid = curproc->pid;
		lck->next = ti->locks;
		ti->locks = lck;
	/* mark the file as being locked */
		f->flags |= O_LOCK;
		return 0;
	}
	return EINVFN;
}

static long ARGS_ON_STACK 
tos_datime(f, timeptr, rwflag)
	FILEPTR *f;
	short *timeptr;
	int rwflag;
{
	if (rwflag) {
		struct tindex *ti = (struct tindex *)f->fc.index;
		ti->valid = 0;
	}
	return Fdatime(timeptr, (int)f->devinfo, rwflag);
}

static long ARGS_ON_STACK 
tos_close(f, pid)
	FILEPTR *f;
	int pid;
{
	LOCK *lck, **oldl;
	struct tindex *t;
	FILEPTR **old, *p;
	long r = 0;
	extern int flk;		/* set in main.c */

	t = (struct tindex *)(f->fc.index);
/* if this handle was locked, remove any locks held by the process
 */
	if (f->flags & O_LOCK) {
		TRACE(("tos_close: releasing locks (file mode: %x)", f->flags));
		oldl = &t->locks;
		lck = *oldl;
		while (lck) {
			if (lck->l.l_pid == pid) {
				*oldl = lck->next;
				if (flk)
					(void)Flock((int)f->devinfo, 1,
						lck->l.l_start, lck->l.l_len);
				wake(IO_Q, (long)lck);
				kfree(lck);
			} else {
				oldl = &lck->next;
			}
			lck = *oldl;
		}
	}

	if (f->links <= 0) {
/* remove f from the list of open file pointers on this index */
		t->valid = 0;
		old = &t->open;
		p = t->open;
		while (p && p != f) {
			old = &p->next;
			p = p->next;
		}
		assert(p);
		*old = f->next;
		f->next = 0;
		r = Fclose((int)f->devinfo);

/* if the file was marked for deletion, delete it */
		if (!t->open) {
			if (t->attr & FA_DELETE) {
				(void)Fdelete(t->name);
				t->name = 0;
			}
		}
	}
	return r;
}

/*
 * check for disk change: called by the kernel if Mediach returns a
 * non-zero value
 */

long ARGS_ON_STACK 
tos_dskchng(drv)
	int drv;
{
	char dlet;
	int i;
	struct tindex *ti;
	FILEPTR *f, *nextf;

	dlet = 'A' + drv;
MEDIA_DB(("tos_dskchng(%c)", dlet));
	ti = gl_ti;
	for (i = 0; i < NUM_INDICES; i++, ti++) {
		if (ti->name && ti->name[0] == dlet) {
#ifdef NEWWAY
	/* only free the name if this index not used by any cookie */
			if (ti->links != 0)
				ti->valid = 0;
			else
#endif /* NEWWAY */
			{
				kfree(ti->name);
				ti->name = 0;
			}
			if (!(ti->attr & FA_DIR)) {
				nextf = ti->open;
				while ( (f = nextf) != 0 ) {
					nextf = f->next;
					f->next = 0;
				}
			}
			ti->open = 0;
	/* if there are any cookies pointing at this name, they're not
	 * valid any more, so we will *want* to get an error if they're
	 * used.
	 */
		}
	}
	tmpindex.valid = 0;
/*
 * OK, make sure that GEMDOS knows to look for a change if we
 * ever use this drive again.
 */
	drvchanged[drv] = 1;
	return 1;
}

#ifdef NEWWAY
/* release/copy file cookies; these functions exist to keep
 * track of whether or not the kernel is still using a file
 */
long
tos_release(fc)
	fcookie *fc;
{
	struct tindex *ti = (struct tindex *)fc->index;

	if (ti->links <= 0) {
		FATAL("tos_release: link count of `%s' is %d", ti->name, ti->links);
	}
	ti->links--;
	COOKIE_DB(("tos_release: %s now has %d links", ti->name, ti->links));
	return 0;
}

long
tos_dupcookie(dest, src)
	fcookie *dest, *src;
{
	struct tindex *ti = (struct tindex *)src->index;

	if (ti->links <= 0) {
		FATAL("tos_dupcookie: link count of %s is %d", ti->name, ti->links);
	}
	ti->links++;
	COOKIE_DB(("tos_dupcookie: %s now has %d links", ti->name, ti->links));
	*dest = *src;
	return 0;
}
#endif

/*
 * utility function: sets the TOS DTA, and also records what directory
 * this was in. This is just to save us a call into the kernel if the
 * correct DTA has already been set.
 */

static void
do_setdta(dta)
	DTABUF *dta;
{
	if (dta != lastdta) {
		Fsetdta(dta);
		lastdta = dta;
	}
}

/*
 * routines for forcing a media change on drive "drv"
 */

static int chdrv;

/* new Getbpb function: when this is called, all the other
 * vectors can be un-installed
 */

static long ARGS_ON_STACK (*Oldgetbpb) P_((int));
static long ARGS_ON_STACK (*Oldmediach) P_((int));
static long ARGS_ON_STACK (*Oldrwabs) P_((int, void *, int, int, int, long));

static long  ARGS_ON_STACK
Newgetbpb(d)
	int d;
{
	if (d == chdrv) {
MEDIA_DB(("Newgetbpb(%c)", d+'A'));
if (Oldgetbpb == Newgetbpb) {
MEDIA_DB(("AARGH!!! BAD BPBs"));
}
		*((Func *)0x472L) = Oldgetbpb;
		*((Func *)0x476L) = Oldrwabs;
		*((Func *)0x47eL) = Oldmediach;
	}
	return (*Oldgetbpb)(d);
}

static long ARGS_ON_STACK
Newmediach(d)
	int d;
{
	if (d == chdrv) {
MEDIA_DB(("Newmediach(%c)", d+'A'));
		return 2;
	}
	return (*Oldmediach)(d);
}

static long ARGS_ON_STACK
Newrwabs(mode, buf, num, recno, dev, l)
	int mode;
	void *buf;
	int num, recno, dev;
	long l;
{
	if (dev == chdrv) {
MEDIA_DB(("Newrwabs"));
		return E_CHNG;
	}
	return (*Oldrwabs)(mode, buf, num, recno, dev, l);
}

static void
force_mediach(d)
	int d;
{
#ifdef FSFIRST_MEDIACH
	static char fname[] = "X:\\*.*";
#else
	long r;
	static char fname[] = "X:\\X";
#endif
	TRACE(("tosfs: disk change drive %c", d+'A'));
MEDIA_DB(("forcing media change on %c", d+'A'));

	chdrv = d;
	Oldrwabs = *((Func *)0x476L);
	Oldgetbpb = *((Func *)0x472L);
	Oldmediach = *((Func *)0x47eL);

	if (Oldrwabs == Newrwabs || Oldgetbpb == Newgetbpb ||
	    Oldmediach == Newmediach) {
		FORCE("tosfs: error in media change code");
	} else {
		*((Func *)0x476L) = Newrwabs;
		*((Func *)0x472L) = Newgetbpb;
		*((Func *)0x47eL) = Newmediach;
	}

	fname[0] = d + 'A';
MEDIA_DB(("calling GEMDOS"));
#ifdef FSFIRST_MEDIACH
	(void)Fsfirst(fname, 8);
#else	
	r = Fopen(fname, 0);
	if (r >= 0) Fclose((int)r);
#endif
MEDIA_DB(("done calling GEMDOS"));
	drvchanged[d] = 0;
	if ( *((Func *)0x476L) == Newrwabs ) {
		DEBUG(("WARNING: media change not performed correctly"));
		*((Func *)0x472L) = Oldgetbpb;
		*((Func *)0x476L) = Oldrwabs;
		*((Func *)0x47eL) = Oldmediach;
	}
}
