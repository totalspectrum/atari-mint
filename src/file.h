/*
Copyright 1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
*/

#ifndef _filesys_h
#define _filesys_h

struct filesys;		/* forward declaration */
struct devdrv;		/* ditto */
struct timeout;		/* and ditto */

typedef struct f_cookie {
	struct filesys *fs;	/* filesystem that knows about this cookie */
	ushort	dev;		/* device info (e.g. Rwabs device number) */
	ushort	aux;		/* extra data that the file system may want */
	long	index;		/* this+dev uniquely identifies a file */
} fcookie;

#define TOS_NAMELEN 13

typedef struct dtabuf {
	short	index;		/* index into arrays in the PROC struct */
	long	magic;
#define SVALID	0x1234fedcL	/* magic for a valid search */
#define EVALID	0x5678ba90L	/* magic for an exhausted search */

	char	dta_pat[TOS_NAMELEN+1];	/* pointer to pattern, if necessary */
	char	dta_sattrib;	/* attributes being searched for */
/* this stuff is returned to the user */
	char	dta_attrib;
	short	dta_time;
	short	dta_date;
	long	dta_size;
	char	dta_name[TOS_NAMELEN+1];
} DTABUF;

/* structure for opendir/readdir/closedir */
typedef struct dirstruct {
	fcookie fc;		/* cookie for this directory */
	ushort	index;		/* index of the current entry */
	ushort	flags;		/* flags (e.g. tos or not) */
#define TOS_SEARCH	0x01
	char	fsstuff[60];	/* anything else the file system wants */
				/* NOTE: this must be at least 45 bytes */
	struct dirstruct *next;	/* linked together so we can close them
				   on process termination */
} DIR;

/* structure for getxattr */
typedef struct xattr {
	ushort	mode;
/* file types */
#define S_IFMT	0170000		/* mask to select file type */
#define S_IFCHR	0020000		/* BIOS special file */
#define S_IFDIR	0040000		/* directory file */
#define S_IFREG 0100000		/* regular file */
#define S_IFIFO	0120000		/* FIFO */
#define S_IFMEM	0140000		/* memory region or process */
#define S_IFLNK	0160000		/* symbolic link */

/* special bits: setuid, setgid, sticky bit */
#define S_ISUID	04000
#define S_ISGID 02000
#define S_ISVTX	01000

/* file access modes for user, group, and other*/
#define S_IRUSR	0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRGRP 0040
#define S_IWGRP	0020
#define S_IXGRP	0010
#define S_IROTH	0004
#define S_IWOTH	0002
#define S_IXOTH	0001
#define DEFAULT_DIRMODE (0777)
#define DEFAULT_MODE	(0666)
	long	index;
	ushort	dev;
	ushort	rdev;		/* "real" device */
	ushort	nlink;
	ushort	uid;
	ushort	gid;
	long	size;
	long	blksize;
	long	nblocks;
	short	mtime, mdate;
	short	atime, adate;
	short	ctime, cdate;
	short	attr;
/* defines for TOS attribute bytes */
#ifndef FA_RDONLY
#define	       FA_RDONLY	       0x01
#define	       FA_HIDDEN	       0x02
#define	       FA_SYSTEM	       0x04
#define	       FA_LABEL		       0x08
#define	       FA_DIR		       0x10
#define	       FA_CHANGED	       0x20
#endif
	short	reserved2;
	long	reserved3[2];
} XATTR;

typedef struct fileptr {
	short	links;	    /* number of copies of this descriptor */
	ushort	flags;	    /* file open mode and other file flags */
	long	pos;	    /* position in file */
	long	devinfo;    /* device driver specific info */
	fcookie	fc;	    /* file system cookie for this file */
	struct devdrv *dev; /* device driver that knows how to deal with this */
	struct fileptr *next; /* link to next fileptr for this file */
} FILEPTR;

struct flock {
	short l_type;			/* type of lock */
#define F_RDLCK		0
#define F_WRLCK		1
#define F_UNLCK		3
	short l_whence;			/* SEEK_SET, SEEK_CUR, SEEK_END */
	long l_start;			/* start of locked region */
	long l_len;			/* length of locked region */
	short l_pid;			/* pid of locking process
						(F_GETLK only) */
};

/* structure for internal kernel locks */
typedef struct ilock {
	struct flock l;		/* the actual lock */
	struct ilock *next;	/* next lock in the list */
	long	reserved[4];	/* reserved for future expansion */
} LOCK;

typedef struct devdrv {
	long ARGS_ON_STACK (*open)	P_((FILEPTR *f));
	long ARGS_ON_STACK (*write)	P_((FILEPTR *f, const char *buf, long bytes));
	long ARGS_ON_STACK (*read)	P_((FILEPTR *f, char *buf, long bytes));
	long ARGS_ON_STACK (*lseek)	P_((FILEPTR *f, long where, int whence));
	long ARGS_ON_STACK (*ioctl)	P_((FILEPTR *f, int mode, void *buf));
	long ARGS_ON_STACK (*datime)	P_((FILEPTR *f, short *timeptr, int rwflag));
	long ARGS_ON_STACK (*close)	P_((FILEPTR *f, int pid));
	long ARGS_ON_STACK (*select)	P_((FILEPTR *f, long proc, int mode));
	void ARGS_ON_STACK (*unselect)	P_((FILEPTR *f, long proc, int mode));
/* extensions, check dev_descr.drvsize (size of DEVDRV struct) before calling:
 * fast RAW tty byte io  */
	long ARGS_ON_STACK (*writeb)	P_((FILEPTR *f, const char *buf, long bytes));
	long ARGS_ON_STACK (*readb)	P_((FILEPTR *f, char *buf, long bytes));
/* what about: scatter/gather io for DMA devices...
 *	long ARGS_ON_STACK (*writev)	P_((FILEPTR *f, const struct iovec *iov, long cnt));
 *	long ARGS_ON_STACK (*readv)	P_((FILEPTR *f, const struct iovec *iov, long cnt));
 */
} DEVDRV;

typedef struct filesys {
	struct	filesys	*next;	/* link to next file system on chain */
	long	fsflags;
#define FS_KNOPARSE	0x01	/* kernel shouldn't do parsing */
#define FS_CASESENSITIVE	0x02	/* file names are case sensitive */
#define FS_NOXBIT	0x04	/* if a file can be read, it can be executed */
#define	FS_LONGPATH	0x08	/* file system understands "size" argument to
				   "getname" */
	long	ARGS_ON_STACK (*root) P_((int drv, fcookie *fc));
	long	ARGS_ON_STACK (*lookup) P_((fcookie *dir, const char *name, fcookie *fc));
	long	ARGS_ON_STACK (*creat) P_((fcookie *dir, const char *name, unsigned mode,
				int attrib, fcookie *fc));
	DEVDRV * ARGS_ON_STACK (*getdev) P_((fcookie *fc, long *devspecial));
	long	ARGS_ON_STACK (*getxattr) P_((fcookie *file, XATTR *xattr));
	long	ARGS_ON_STACK (*chattr) P_((fcookie *file, int attr));
	long	ARGS_ON_STACK (*chown) P_((fcookie *file, int uid, int gid));
	long	ARGS_ON_STACK (*chmode) P_((fcookie *file, unsigned mode));
	long	ARGS_ON_STACK (*mkdir) P_((fcookie *dir, const char *name, unsigned mode));
	long	ARGS_ON_STACK (*rmdir) P_((fcookie *dir, const char *name));
	long	ARGS_ON_STACK (*remove) P_((fcookie *dir, const char *name));
	long	ARGS_ON_STACK (*getname) P_((fcookie *relto, fcookie *dir,
			    char *pathname, int size));
	long	ARGS_ON_STACK (*rename) P_((fcookie *olddir, char *oldname,
			    fcookie *newdir, const char *newname));
	long	ARGS_ON_STACK (*opendir) P_((DIR *dirh, int tosflag));
	long	ARGS_ON_STACK (*readdir) P_((DIR *dirh, char *name, int namelen, fcookie *fc));
	long	ARGS_ON_STACK (*rewinddir) P_((DIR *dirh));
	long	ARGS_ON_STACK (*closedir) P_((DIR *dirh));
	long	ARGS_ON_STACK (*pathconf) P_((fcookie *dir, int which));
	long	ARGS_ON_STACK (*dfree) P_((fcookie *dir, long *buf));
	long	ARGS_ON_STACK (*writelabel) P_((fcookie *dir, const char *name));
	long	ARGS_ON_STACK (*readlabel) P_((fcookie *dir, char *name, int namelen));
	long	ARGS_ON_STACK (*symlink) P_((fcookie *dir, const char *name, const char *to));
	long	ARGS_ON_STACK (*readlink) P_((fcookie *dir, char *buf, int len));
	long	ARGS_ON_STACK (*hardlink) P_((fcookie *fromdir, const char *fromname,
				fcookie *todir, const char *toname));
	long	ARGS_ON_STACK (*fscntl) P_((fcookie *dir, const char *name, int cmd, long arg));
	long	ARGS_ON_STACK (*dskchng) P_((int drv));
	long	ARGS_ON_STACK (*release) P_((fcookie *));
	long	ARGS_ON_STACK (*dupcookie) P_((fcookie *new, fcookie *old));
} FILESYS;

/*
 * this is the structure passed to loaded file systems to tell them
 * about the kernel
 */

struct kerinfo {
	short	maj_version;	/* kernel version number */
	short	min_version;	/* minor kernel version number */
	ushort	default_perm;	/* default file permissions */
	short	reserved1;	/* room for expansion */

/* OS functions */
	Func	*bios_tab;	/* pointer to the BIOS entry points */
	Func	*dos_tab;	/* pointer to the GEMDOS entry points */

/* media change vector */
	void	ARGS_ON_STACK (*drvchng) P_((unsigned));

/* Debugging stuff */
	void	ARGS_ON_STACK (*trace) P_((const char *, ...));
	void	ARGS_ON_STACK (*debug) P_((const char *, ...));
	void	ARGS_ON_STACK (*alert) P_((const char *, ...));
	EXITING void ARGS_ON_STACK (*fatal) P_((const char *, ...)) NORETURN;

/* memory allocation functions */
	void *	ARGS_ON_STACK (*kmalloc) P_((long));
	void	ARGS_ON_STACK (*kfree) P_((void *));
	void *	ARGS_ON_STACK (*umalloc) P_((long));
	void	ARGS_ON_STACK (*ufree) P_((void *));

/* utility functions for string manipulation */
	int	ARGS_ON_STACK (*strnicmp) P_((const char *, const char *, int));
	int	ARGS_ON_STACK (*stricmp) P_((const char *, const char *));
	char *	ARGS_ON_STACK (*strlwr) P_((char *));
	char *	ARGS_ON_STACK (*strupr) P_((char *));
	int	ARGS_ON_STACK (*sprintf) P_((char *, const char *, ...));

/* utility functions for manipulating time */
	void	ARGS_ON_STACK (*millis_time) P_((unsigned long, short *));
	long	ARGS_ON_STACK (*unixtim) P_((unsigned, unsigned));
	long	ARGS_ON_STACK (*dostim) P_((long));

/* utility functions for dealing with pauses, or for putting processes
 * to sleep
 */
	void	ARGS_ON_STACK (*nap) P_((unsigned));
	int	ARGS_ON_STACK (*sleep) P_((int que, long cond));
	void	ARGS_ON_STACK (*wake) P_((int que, long cond));
	void	ARGS_ON_STACK (*wakeselect) P_((long param));

/* file system utility functions */
	int	ARGS_ON_STACK (*denyshare) P_((FILEPTR *, FILEPTR *));
	LOCK *	ARGS_ON_STACK (*denylock) P_((LOCK *, LOCK *));

/* functions for adding/cancelling timeouts */
	struct timeout * ARGS_ON_STACK (*addtimeout) P_((long, void (*)()));
	void	ARGS_ON_STACK (*canceltimeout) P_((struct timeout *));
	struct timeout * ARGS_ON_STACK (*addroottimeout) P_((long, void (*)(), short));
	void	ARGS_ON_STACK (*cancelroottimeout) P_((struct timeout *));

/* miscellaneous other things */
	long	ARGS_ON_STACK (*ikill) P_((int, int));
	void	ARGS_ON_STACK (*iwake) P_((int que, long cond, short pid));

/* reserved for future use */
	long	res2[3];
};

/* flags for open() modes */
#define O_RWMODE  	0x03	/* isolates file read/write mode */
#	define O_RDONLY	0x00
#	define O_WRONLY	0x01
#	define O_RDWR	0x02
#	define O_EXEC	0x03	/* execute file; used by kernel only */

/* 0x04 is for future expansion */
#define O_APPEND	0x08	/* all writes go to end of file */

#define O_SHMODE	0x70	/* isolates file sharing mode */
#	define O_COMPAT	0x00	/* compatibility mode */
#	define O_DENYRW	0x10	/* deny both read and write access */
#	define O_DENYW	0x20	/* deny write access to others */
#	define O_DENYR	0x30	/* deny read access to others */
#	define O_DENYNONE 0x40	/* don't deny any access to others */

#define O_NOINHERIT	0x80	/* private file (not passed to child) */

#define O_NDELAY	0x100	/* don't block for i/o on this file */
#define O_CREAT		0x200	/* create file if it doesn't exist */
#define O_TRUNC		0x400	/* truncate file to 0 bytes if it does exist */
#define O_EXCL		0x800	/* fail open if file exists */

#define O_USER		0x0fff	/* isolates user-settable flag bits */

#define O_GLOBAL	0x1000	/* for opening a global file */

/* kernel mode bits -- the user can't set these! */
#define O_TTY		0x2000
#define O_HEAD		0x4000
#define O_LOCK		0x8000

/* GEMDOS file attributes */

/* macros to be applied to FILEPTRS to determine their type */
#define is_terminal(f) (f->flags & O_TTY)

/* lseek() origins */
#define	SEEK_SET	0		/* from beginning of file */
#define	SEEK_CUR	1		/* from current location */
#define	SEEK_END	2		/* from end of file */

/* The requests for Dpathconf() */
#define DP_IOPEN	0	/* internal limit on # of open files */
#define DP_MAXLINKS	1	/* max number of hard links to a file */
#define DP_PATHMAX	2	/* max path name length */
#define DP_NAMEMAX	3	/* max length of an individual file name */
#define DP_ATOMIC	4	/* # of bytes that can be written atomically */
#define DP_TRUNC	5	/* file name truncation behavior */
#	define	DP_NOTRUNC	0	/* long filenames give an error */
#	define	DP_AUTOTRUNC	1	/* long filenames truncated */
#	define	DP_DOSTRUNC	2	/* DOS truncation rules in effect */
#define DP_CASE		6
#	define	DP_CASESENS	0	/* case sensitive */
#	define	DP_CASECONV	1	/* case always converted */
#	define	DP_CASEINSENS	2	/* case insensitive, preserved */
#define DP_MODEATTR		7
#	define	DP_ATTRBITS	0x000000ffL	/* mask for valid TOS attribs */
#	define	DP_MODEBITS	0x000fff00L	/* mask for valid Unix file modes */
#	define	DP_FILETYPS	0xfff00000L	/* mask for valid file types */
#	define	DP_FT_DIR	0x00100000L	/* directories (always if . is there) */
#	define	DP_FT_CHR	0x00200000L	/* character special files */
#	define	DP_FT_BLK	0x00400000L	/* block special files, currently unused */
#	define	DP_FT_REG	0x00800000L	/* regular files */
#	define	DP_FT_LNK	0x01000000L	/* symbolic links */
#	define	DP_FT_SOCK	0x02000000L	/* sockets, currently unused */
#	define	DP_FT_FIFO	0x04000000L	/* pipes */
#	define	DP_FT_MEM	0x08000000L	/* shared memory or proc files */
#define DP_XATTRFIELDS	8
#	define	DP_INDEX	0x0001
#	define	DP_DEV		0x0002
#	define	DP_RDEV		0x0004
#	define	DP_NLINK	0x0008
#	define	DP_UID		0x0010
#	define	DP_GID		0x0020
#	define	DP_BLKSIZE	0x0040
#	define	DP_SIZE		0x0080
#	define	DP_NBLOCKS	0x0100
#	define	DP_ATIME	0x0200
#	define	DP_CTIME	0x0400
#	define	DP_MTIME	0x0800
#define DP_MAXREQ	8	/* highest legal request */

/* Dpathconf and Sysconf return this when a value is not limited
   (or is limited only by available memory) */

#define UNLIMITED	0x7fffffffL

/* various character constants and defines for TTY's */
#define MiNTEOF 0x0000ff1a	/* 1a == ^Z */

/* defines for tty_read */
#define RAW	0
#define COOKED	0x1
#define NOECHO	0
#define ECHO	0x2
#define ESCSEQ	0x04		/* cursor keys, etc. get escape sequences */

/* constants for Fcntl calls */
#define F_DUPFD		0		/* handled by kernel */
#define F_GETFD		1		/* handled by kernel */
#define F_SETFD		2		/* handled by kernel */
#	define FD_CLOEXEC	1	/* close on exec flag */

#define F_GETFL		3		/* handled by kernel */
#define F_SETFL		4		/* handled by kernel */
#define F_GETLK		5
#define F_SETLK		6
#define F_SETLKW	7

/* more constants for various Fcntl's */
#define FSTAT		(('F'<< 8) | 0)		/* handled by kernel */
#define FIONREAD	(('F'<< 8) | 1)
#define FIONWRITE	(('F'<< 8) | 2)
#define FIOEXCEPT	(('F'<< 8) | 5)
#define TIOCGETP	(('T'<< 8) | 0)
#define TIOCSETN	(('T'<< 8) | 1)
#define TIOCGETC	(('T'<< 8) | 2)
#define TIOCSETC	(('T'<< 8) | 3)
#define TIOCGLTC	(('T'<< 8) | 4)
#define TIOCSLTC	(('T'<< 8) | 5)
#define TIOCGPGRP	(('T'<< 8) | 6)
#define TIOCSPGRP	(('T'<< 8) | 7)
#define TIOCFLUSH	(('T'<< 8) | 8)
#define TIOCSTOP	(('T'<< 8) | 9)
#define TIOCSTART	(('T'<< 8) | 10)
#define TIOCGWINSZ	(('T'<< 8) | 11)
#define TIOCSWINSZ	(('T'<< 8) | 12)
#define TIOCGXKEY	(('T'<< 8) | 13)
#define TIOCSXKEY	(('T'<< 8) | 14)
#define TIOCIBAUD	(('T'<< 8) | 18)
#define TIOCOBAUD	(('T'<< 8) | 19)
#define TIOCCBRK	(('T'<< 8) | 20)
#define TIOCSBRK	(('T'<< 8) | 21)
#define TIOCGFLAGS	(('T'<< 8) | 22)
#define TIOCSFLAGS	(('T'<< 8) | 23)
#define TIOCOUTQ	(('T'<< 8) | 24)
#define TIOCSETP	(('T'<< 8) | 25)
#define TIOCHPCL	(('T'<< 8) | 26)
#define TIOCCAR		(('T'<< 8) | 27)
#define TIOCNCAR	(('T'<< 8) | 28)
#define TIOCWONLINE	(('T'<< 8) | 29)
#define TIOCSFLAGSB	(('T'<< 8) | 30)
#define TIOCGSTATE	(('T'<< 8) | 31)
#define TIOCSSTATEB	(('T'<< 8) | 32)
#define TIOCGVMIN	(('T'<< 8) | 33)
#define TIOCSVMIN	(('T'<< 8) | 34)

/* cursor control Fcntls:
 * NOTE THAT THESE MUST BE TOGETHER
 */
#define TCURSOFF	(('c'<< 8) | 0)
#define TCURSON		(('c'<< 8) | 1)
#define TCURSBLINK	(('c'<< 8) | 2)
#define TCURSSTEADY	(('c'<< 8) | 3)
#define TCURSSRATE	(('c'<< 8) | 4)
#define TCURSGRATE	(('c'<< 8) | 5)

/* process stuff */
#define PPROCADDR	(('P'<< 8) | 1)
#define PBASEADDR	(('P'<< 8) | 2)
#define PCTXTSIZE	(('P'<< 8) | 3)
#define PSETFLAGS	(('P'<< 8) | 4)
#define PGETFLAGS	(('P'<< 8) | 5)
#define PTRACESFLAGS	(('P'<< 8) | 6)
#define PTRACEGFLAGS	(('P'<< 8) | 7)
#	define	P_ENABLE	(1 << 0)	/* enable tracing */
#ifdef NOTYETDEFINED
#	define	P_DOS		(1 << 1)	/* trace DOS calls - unimplemented */
#	define	P_BIOS		(1 << 2)	/* trace BIOS calls - unimplemented */
#	define	P_XBIOS		(1 << 3)	/* trace XBIOS calls - unimplemented */
#endif

#define PTRACEGO	(('P'<< 8) | 8)	/* these 4 must be together */
#define PTRACEFLOW	(('P'<< 8) | 9)
#define PTRACESTEP	(('P'<< 8) | 10)
#define PTRACE11	(('P'<< 8) | 11)
#define PLOADINFO	(('P'<< 8) | 12)
#define	PFSTAT		(('P'<< 8) | 13)

struct ploadinfo {
	/* passed */
	short fnamelen;
	/* returned */
	char *cmdlin, *fname;
};


#define SHMGETBLK	(('M'<< 8) | 0)
#define SHMSETBLK	(('M'<< 8) | 1)

/* terminal control constants (tty.sg_flags) */
#define T_CRMOD		0x0001
#define T_CBREAK	0x0002
#define T_ECHO		0x0004
/* #define T_XTABS	0x0008  unimplemented*/
#define T_RAW		0x0010
/* #define T_LCASE	0x0020  unimplemented */

#define T_NOFLSH	0x0040		/* don't flush buffer when signals
					   are received */
#define T_TOS		0x0080
#define T_TOSTOP	0x0100
#define T_XKEY		0x0200		/* Fread returns escape sequences for
					   cursor keys, etc. */
#define T_ECHOCTL	0x0400		/* echo ctl chars as ^x */
/* 0x0800 still available */

#define T_TANDEM	0x1000
#define T_RTSCTS	0x2000
#define T_EVENP		0x4000		/* EVENP and ODDP are mutually exclusive */
#define T_ODDP		0x8000

#define TF_FLAGS	0xF000

/* some flags for TIOC[GS]FLAGS */
#define TF_CAR		0x800		/* nonlocal mode, require carrier */
#define TF_NLOCAL	TF_CAR

#define TF_BRKINT	0x80		/* allow breaks interrupt (like ^C) */

#define TF_STOPBITS	0x0003
#define TF_1STOP	0x0001
#define TF_15STOP	0x0002
#define	TF_2STOP	0x0003

#define TF_CHARBITS	0x000C
#define TF_8BIT		0
#define TF_7BIT		0x4
#define TF_6BIT		0x8
#define TF_5BIT		0xC

/* the following are terminal status flags (tty.state) */
/* (the low byte of tty.state indicates a part of an escape sequence still
 * hasn't been read by Fread, and is an index into that escape sequence)
 */
#define TS_ESC		0x00ff
#define TS_BLIND	0x800		/* tty is `blind' i.e. has no carrier
					   (cleared in local mode) */
#define TS_HOLD		0x1000		/* hold (e.g. ^S/^Q) */
#define TS_HPCL		0x4000		/* hang up on close */
#define TS_COOKED	0x8000		/* interpret control chars */

/* structures for terminals */
struct tchars {
	char t_intrc;
	char t_quitc;
	char t_startc;
	char t_stopc;
	char t_eofc;
	char t_brkc;
};

struct ltchars {
	char t_suspc;
	char t_dsuspc;
	char t_rprntc;
	char t_flushc;
	char t_werasc;
	char t_lnextc;
};

struct sgttyb {
	char sg_ispeed;
	char sg_ospeed;
	char sg_erase;
	char sg_kill;
	ushort sg_flags;
};

struct winsize {
	short	ws_row;
	short	ws_col;
	short	ws_xpixel;
	short	ws_ypixel;
};

struct xkey {
	short	xk_num;
	char	xk_def[8];
};

struct tty {
	short		pgrp;		/* process group of terminal */
	short		state;		/* terminal status, e.g. stopped */
	short		use_cnt;	/* number of times terminal is open */
	short		aux_cnt;	/* number of times terminal is open as
					   /dev/aux */
	struct sgttyb 	sg;
	struct tchars 	tc;
	struct ltchars 	ltc;
	struct winsize	wsiz;
	long		rsel;		/* selecting process for read */
	long		wsel;		/* selecting process for write */
	char		*xkey;		/* extended keyboard table */
	long		hup_ospeed;	/* saved ospeed while hanging up */
	unsigned short	vmin, vtime;	/* min chars, timeout for RAW reads */
	long		resrvd[1];	/* for future expansion */
};

struct bios_tty {
	IOREC_T		*irec;		/* From XBIOS ... */
	long		*rsel;		/* pointer to field in tty struct */
	IOREC_T		*orec;		/* Same, for output... */
	long		*wsel;
	long		ispeed, ospeed;	/* last speeds set */
	long		*baudmap, maxbaud; /* Rsconf baud word <-> bps table */
	short		*baudx;
	struct tty	*tty;
	long		bticks;		/* when to take a break for real */
	long		vticks;		/* ..check read buf next (vmin/speed) */
	char		clocal, brkint;	/* flags: local mode, break == ^C */
	short		tosfd;		/* if != EUNDEV: fd to pass Fcntl()s */
	short		bdev, unused1;
};

/* Dcntl constants and types */
#define DEV_NEWTTY	0xde00
#define DEV_NEWBIOS	0xde01
#define DEV_INSTALL	0xde02

struct dev_descr {
	DEVDRV	*driver;
	short	dinfo;
	short	flags;
	struct tty *tty;
	long	drvsize;		/* size of DEVDRV struct */
	long	reserved[3];
};


#define FS_INSTALL    0xf001  /* let the kernel know about the file system */
#define FS_MOUNT      0xf002  /* make a new directory for a file system */
#define FS_UNMOUNT    0xf003  /* remove a directory for a file system */
#define FS_UNINSTALL  0xf004  /* remove a file system from the list */


struct fs_descr
{
	FILESYS *file_system;
	short dev_no;    /* this is filled in by MiNT if arg == FS_MOUNT*/
	long flags;
	long reserved[4];
};


/* number of BIOS drives */
#define NUM_DRIVES 32

#define BIOSDRV (NUM_DRIVES)
#define PIPEDRV (NUM_DRIVES+1)
#define PROCDRV (NUM_DRIVES+2)
#define SHMDRV	(NUM_DRIVES+3)

#define UNI_NUM_DRVS (NUM_DRIVES+4)
#define UNIDRV	('U'-'A')

#define PSEUDODRVS ((1L<<UNIDRV))

/* various fields for the "rdev" device numbers */
#define BIOS_DRIVE_RDEV 	0x0000
#define BIOS_RDEV	0x0100
#define FAKE_RDEV	0x0200
#define PIPE_RDEV	0x7e00
#define UNK_RDEV	0x7f00
#define PROC_RDEV_BASE	0xa000

#ifndef GENMAGIC
/* external variables */

extern FILESYS *drives[NUM_DRIVES];
extern struct tty default_tty;
#define follow_links ((char *)-1L)
#endif

/* internal bios file structure */

#define	BNAME_MAX	13

struct bios_file {
	char 	name[BNAME_MAX+1];	/* device name */
	DEVDRV *device;			/* device driver for device */
	short	private;		/* extra info for device driver */
	ushort	flags;			/* flags for device open */
	struct tty *tty;		/* tty structure (if appropriate) */
	struct bios_file *next;
	short	lockpid;		/* owner of the lock */
	XATTR	xattr;			/* guess what... */
	long	drvsize;		/* size of DEVDRV struct */
};

#endif /* _filesys_h */
