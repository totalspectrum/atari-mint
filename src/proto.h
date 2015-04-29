/* bios.c */
unsigned ionwrite P_((IOREC_T *wrec));
unsigned ionread P_((IOREC_T *irec));
unsigned btty_ionread P_((struct bios_tty *b));
void checkbtty_nsig P_((struct bios_tty *b));
void checkbttys P_((void));
void checkbttys_vbl P_((void));
long ARGS_ON_STACK getmpb P_((void *ptr));
long bconstat P_((int dev));
long bconin P_((int dev));
long bconout P_((int dev, int c));
long ARGS_ON_STACK ubconstat P_((int dev));
long ARGS_ON_STACK ubconin P_((int dev));
long ARGS_ON_STACK ubconout P_((int dev, int c));
long ARGS_ON_STACK rwabs P_((int rwflag, void *buffer, int number, int recno, int dev, long lrecno));
long ARGS_ON_STACK setexc P_((int number, long vector));
long ARGS_ON_STACK tickcal P_((void));
long ARGS_ON_STACK getbpb P_((int dev));
long bcostat P_((int dev));
long ARGS_ON_STACK ubcostat P_((int dev));
long ARGS_ON_STACK mediach P_((int dev));
long ARGS_ON_STACK drvmap P_((void));
long ARGS_ON_STACK kbshift P_((int mode));
long ARGS_ON_STACK bflush P_((void));
void init_bios P_((void));
long ARGS_ON_STACK do_bconin P_((int dev));
int checkkeys P_((void));
void init_vectors P_((void));
void unlink_vectors  P_((long start, long end));

/* xbios.c */
long ARGS_ON_STACK supexec P_((Func funcptr, long a1, long a2, long a3, long a4, long a5));
long ARGS_ON_STACK midiws P_((int, const char *));
int mapin P_((int));
long ARGS_ON_STACK uiorec P_((int));
long ARGS_ON_STACK rsconf P_((int, int, int, int, int, int));
long ARGS_ON_STACK bconmap P_((int));
long ARGS_ON_STACK cursconf P_((int, int));
long ARGS_ON_STACK dosound P_((const char *ptr));
void init_xbios P_((void));

/* console.c */
long file_instat P_((FILEPTR *f));
long file_outstat P_((FILEPTR *f));
long file_getchar P_((FILEPTR *f, int mode));
long file_putchar P_((FILEPTR *f, long c, int mode));
long ARGS_ON_STACK c_conin P_((void));
long ARGS_ON_STACK c_conout P_((int c));
long ARGS_ON_STACK c_auxin P_((void));
long ARGS_ON_STACK c_auxout P_((int c));
long ARGS_ON_STACK c_prnout P_((int c));
long ARGS_ON_STACK c_rawio P_((int c));
long ARGS_ON_STACK c_rawcin P_((void));
long ARGS_ON_STACK c_necin P_((void));
long ARGS_ON_STACK c_conws P_((const char *str));
long ARGS_ON_STACK c_conrs P_((char *buf));
long ARGS_ON_STACK c_conis P_((void));
long ARGS_ON_STACK c_conos P_((void));
long ARGS_ON_STACK c_prnos P_((void));
long ARGS_ON_STACK c_auxis P_((void));
long ARGS_ON_STACK c_auxos P_((void));
long ARGS_ON_STACK f_instat P_((int fh));
long ARGS_ON_STACK f_outstat P_((int fh));
long ARGS_ON_STACK f_getchar P_((int fh, int mode));
long ARGS_ON_STACK f_putchar P_((int fh, long c, int mode));

/* dos.c */
long ARGS_ON_STACK s_version P_((void));
long ARGS_ON_STACK s_uper P_((long new_ssp));
long ARGS_ON_STACK t_getdate P_((void));
long ARGS_ON_STACK t_setdate P_((int date));
long ARGS_ON_STACK t_gettime P_((void));
long ARGS_ON_STACK t_settime P_((int time));
long ARGS_ON_STACK s_yield P_((void));
long ARGS_ON_STACK p_renice P_((int pid, int delta));
long ARGS_ON_STACK p_nice P_((int delta));
long ARGS_ON_STACK p_getpid P_((void));
long ARGS_ON_STACK p_getppid P_((void));
long ARGS_ON_STACK p_getpgrp P_((void));
long ARGS_ON_STACK p_setpgrp P_((int pid, int newgrp));
long ARGS_ON_STACK p_getuid P_((void));
long ARGS_ON_STACK p_getgid P_((void));
long ARGS_ON_STACK p_geteuid P_((void));
long ARGS_ON_STACK p_getegid P_((void));
long ARGS_ON_STACK p_setuid P_((int id));
long ARGS_ON_STACK p_setgid P_((int id));
long ARGS_ON_STACK p_seteuid P_((int id));
long ARGS_ON_STACK p_setegid P_((int id));
long ARGS_ON_STACK p_getauid P_((void));
long ARGS_ON_STACK p_setauid P_((int id));
long ARGS_ON_STACK p_getgroups P_((int gidsetlen, int gidset[]));
long ARGS_ON_STACK p_setgroups P_((int ngroups, int gidset[]));
long ARGS_ON_STACK p_usrval P_((long arg));
long ARGS_ON_STACK p_umask P_((unsigned mode));
long ARGS_ON_STACK p_domain P_((int arg));
long ARGS_ON_STACK p_rusage P_((long *r));
long ARGS_ON_STACK p_setlimit P_((int i, long v));
long ARGS_ON_STACK p_pause P_((void));
long ARGS_ON_STACK t_alarm P_((long x));
long ARGS_ON_STACK t_malarm P_((long x));
long ARGS_ON_STACK t_setitimer P_((int which, long *interval, long *value, long *ointerval, long *ovalue));
long ARGS_ON_STACK s_ysconf P_((int which));
long ARGS_ON_STACK s_alert P_((char *msg));
long ARGS_ON_STACK s_uptime P_((unsigned long *cur_uptim, unsigned long loadave[3]));
void init_dos P_((void));

/* dosdir.c */
long ARGS_ON_STACK d_setdrv P_((int d));
long ARGS_ON_STACK d_getdrv P_((void));
long ARGS_ON_STACK d_free P_((long *buf, int d));
long ARGS_ON_STACK d_create P_((const char *path));
long ARGS_ON_STACK d_delete P_((const char *path));
long ARGS_ON_STACK d_setpath P_((const char *path));
long ARGS_ON_STACK d_getpath P_((char *path, int drv));
long ARGS_ON_STACK d_getcwd P_((char *path, int drv, int size));
long ARGS_ON_STACK f_setdta P_((DTABUF *dta));
long ARGS_ON_STACK f_getdta P_((void));
long ARGS_ON_STACK f_sfirst P_((const char *path, int attrib));
long ARGS_ON_STACK f_snext P_((void));
long ARGS_ON_STACK f_attrib P_((const char *name, int rwflag, int attr));
long ARGS_ON_STACK f_delete P_((const char *name));
long ARGS_ON_STACK f_rename P_((int junk, const char *old, const char *new));
long ARGS_ON_STACK d_pathconf P_((const char *name, int which));
long ARGS_ON_STACK d_opendir P_((const char *path, int flags));
long ARGS_ON_STACK d_readdir P_((int len, long handle, char *buf));
long ARGS_ON_STACK d_xreaddir P_((int len, long handle, char *buf, XATTR *xattr, long *xret));
long ARGS_ON_STACK d_rewind P_((long handle));
long ARGS_ON_STACK d_closedir P_((long handle));
long ARGS_ON_STACK f_xattr P_((int flag, const char *name, XATTR *xattr));
long ARGS_ON_STACK f_link P_((const char *old, const char *new));
long ARGS_ON_STACK f_symlink P_((const char *old, const char *new));
long ARGS_ON_STACK f_readlink P_((int buflen, char *buf, const char *linkfile));
long ARGS_ON_STACK d_cntl P_((int cmd, const char *name, long arg));
long ARGS_ON_STACK f_chown P_((const char *name, int uid, int gid));
long ARGS_ON_STACK f_chmod P_((const char *name, unsigned mode));
long ARGS_ON_STACK d_lock P_((int mode, int drv));
long ARGS_ON_STACK d_readlabel P_((const char *path, char *label, int maxlen));
long ARGS_ON_STACK d_writelabel P_((const char *path, const char *label));

/* dosfile.c */
FILEPTR * do_open P_((const char *name, int mode, int attr, XATTR *x));
long do_pclose P_((PROC *p, FILEPTR *f));
long do_close P_((FILEPTR *f));
long ARGS_ON_STACK f_open P_((const char *name, int mode));
long ARGS_ON_STACK f_create P_((const char *name, int attrib));
long ARGS_ON_STACK f_close P_((int fh));
long ARGS_ON_STACK f_read P_((int fh, long count, char *buf));
long ARGS_ON_STACK f_write P_((int fh, long count, const char *buf));
long ARGS_ON_STACK f_seek P_((long place, int fh, int how));
long ARGS_ON_STACK f_dup P_((int fh));
long ARGS_ON_STACK f_force P_((int newh, int oldh));
long ARGS_ON_STACK f_datime P_((short *timeptr, int fh, int rwflag));
long ARGS_ON_STACK f_lock P_((int fh, int mode, long start, long length));
long ARGS_ON_STACK f_pipe P_((short *usrh));
long ARGS_ON_STACK f_cntl P_((int fh, long arg, int cmd));
long ARGS_ON_STACK f_select P_((unsigned timeout, long *rfdp, long *wfdp, long *xfdp));
long ARGS_ON_STACK f_midipipe P_((int pid, int in, int out));

/* dosmem.c */
long ARGS_ON_STACK m_addalt P_((long start, long size));
long _do_malloc P_((MMAP map, long size, int mode));
long ARGS_ON_STACK m_xalloc P_((long size, int mode));
long ARGS_ON_STACK m_alloc P_((long size));
long ARGS_ON_STACK m_free P_((virtaddr block));
long ARGS_ON_STACK m_shrink P_((int dummy, virtaddr block, long size));
long ARGS_ON_STACK p_exec P_((int mode, void *ptr1, void *ptr2, void *ptr3));
long terminate P_((int code, int que));
long ARGS_ON_STACK p_term P_((int code));
long ARGS_ON_STACK p_term0 P_((void));
long ARGS_ON_STACK p_termres P_((long save, int code));
long ARGS_ON_STACK p_waitpid P_((int pid, int nohang, long *rusage));
long ARGS_ON_STACK p_wait3 P_((int nohang, long *rusage));
long ARGS_ON_STACK p_wait P_((void));
void fork_restore P_((PROC *p, MEMREGION *savemem));
long ARGS_ON_STACK p_vfork P_((void));
long ARGS_ON_STACK p_fork P_((void));
long ARGS_ON_STACK p_sigintr P_((int vec, int sig));
void cancelsigintrs P_((void));

/* dossig.c */
long ARGS_ON_STACK p_kill P_((int pid, int sig));
long ARGS_ON_STACK p_sigaction P_((int sig, const struct sigaction *act,
		     struct sigaction *oact));
long ARGS_ON_STACK p_signal P_((int sig, long handler));
long ARGS_ON_STACK p_sigblock P_((ulong mask));
long ARGS_ON_STACK p_sigsetmask P_((ulong mask));
long ARGS_ON_STACK p_sigpending P_((void));
long ARGS_ON_STACK p_sigpause P_((ulong mask));

/* filesys.c */
void init_drive P_((int drv));
void init_filesys P_((void));
void load_filesys P_((void));
void load_devdriver P_((void));
void close_filesys P_((void));
void ARGS_ON_STACK changedrv P_((unsigned drv));
int disk_changed P_((int drv));
long relpath2cookie
	P_((fcookie *dir, const char *path, char *lastnm, fcookie *res, int depth));
long path2cookie P_((const char *path, char *lastname, fcookie *res));
void release_cookie P_((fcookie *fc));
void dup_cookie P_((fcookie *new, fcookie *old));
FILEPTR *new_fileptr P_((void));
void dispose_fileptr P_((FILEPTR *f));
int ARGS_ON_STACK denyshare P_((FILEPTR *list, FILEPTR *newfileptr));
int ngroupmatch P_((int group));
int denyaccess P_((XATTR *, unsigned));
LOCK * ARGS_ON_STACK denylock P_((LOCK *list, LOCK *newlock));
long dir_access P_((fcookie *, unsigned));
int has_wild P_((const char *name));
void copy8_3 P_((char *dest, const char *src));
int pat_match P_((const char *name, const char *template));
int samefile P_((fcookie *, fcookie *));

/* main.c */
void restr_intr P_((void));
void ARGS_ON_STACK enter_kernel P_((int));
void ARGS_ON_STACK leave_kernel P_((void));
#if defined(__GNUC__) || defined (__MINT__)
int main P_((int argc, char **argv, char **envp));
#else
int main P_((int argc, char **argv));
#endif
void install_cookies P_((void));
void load_config P_((void));

/* mem.c */
void init_mem P_((void));
void restr_screen P_((void));
int add_region P_((MMAP map, ulong place, ulong size, unsigned mflags));
void init_core P_((void));
void init_swap P_((void));
MEMREGION *new_region P_((void));
void dispose_region P_((MEMREGION *m));
long change_prot_status P_((PROC *proc, long start, int newmode));
virtaddr attach_region P_((PROC *proc, MEMREGION *reg));
void detach_region P_((PROC *proc, MEMREGION *reg));
MEMREGION *get_region P_((MMAP map, ulong size, int mode));
void free_region P_((MEMREGION *reg));
long shrink_region P_((MEMREGION *reg, ulong newsize));
long max_rsize P_((MMAP map, long needed));
long tot_rsize P_((MMAP map, int flag));
virtaddr alloc_region P_((MMAP map, ulong size, int mode));
MEMREGION *create_env P_((const char *env, ulong flags));
MEMREGION *create_base P_((const char *cmd, MEMREGION *env, ulong flags, ulong prgsize,
			PROC *execproc, SHTEXT *s, FILEPTR *f, FILEHEAD *fh, XATTR *xp));
MEMREGION *load_region P_((const char *name, MEMREGION *env, const char *cmdlin, XATTR *x,
			MEMREGION **text, long *fp, int isexec));
SHTEXT *get_text_seg P_((FILEPTR *f, FILEHEAD *fh, XATTR *xp, SHTEXT *s, int noalloc));
MEMREGION *find_text_seg P_((FILEPTR *f));
long load_and_reloc P_((FILEPTR *f, FILEHEAD *fh, char *where, long start,
			long nbytes, BASEPAGE *base));
void rts P_((void));
PROC *exec_region P_((PROC *p, MEMREGION *mem, int thread));
long memused P_((PROC *p));
void recalc_maxmem P_((PROC *p));
int valid_address P_((long addr));
MEMREGION *addr2region P_((long addr));
void DUMP_ALL_MEM P_((void));
void DUMPMEM P_((MMAP map));
void sanity_check P_((MMAP map));

/* proc.c */
PROC *new_proc P_((void));
void dispose_proc P_((PROC *p));
PROC *fork_proc P_((void));
void init_proc P_((void));
void reset_priorities P_((void));
void run_next P_((PROC *p, int slices));
void fresh_slices P_((int slices));
void add_q P_((int que, PROC *proc));
void rm_q P_((int que, PROC *proc));
void ARGS_ON_STACK preempt P_((void));
int ARGS_ON_STACK sleep P_((int que, long cond));
void ARGS_ON_STACK wake P_((int que, long cond));
void ARGS_ON_STACK iwake P_((int que, long cond, short pid));
void ARGS_ON_STACK wakeselect P_((long param));
void DUMPPROC P_((void));
void calc_load_average P_((void));
unsigned long gen_average P_((unsigned long *sum, unsigned char *load_ptr, int max_size));

/* signal.c */
long killgroup P_((int pgrp, int sig, int priv));
void post_sig P_((PROC *p, int sig));
long ARGS_ON_STACK ikill P_((int p, int sig));
void check_sigs P_((void));
void raise P_((int sig));
void bombs P_((int sig));
void handle_sig P_((int sig));
long ARGS_ON_STACK p_sigreturn P_((void));
void stop P_((int sig));
void exception P_((int sig));
void sigbus P_((void));
void sigaddr P_((void));
void sigill P_((void));
void sigpriv P_((void));
void sigfpe P_((void));
void sigtrap P_((void));
void haltformat P_((void));
void haltcpv P_((void));

/* timeout.c */
TIMEOUT * ARGS_ON_STACK addtimeout P_((long delta, void (*func)(PROC *p)));
TIMEOUT * ARGS_ON_STACK addroottimeout P_((long delta, void (*func)(PROC *p), short flags));
void ARGS_ON_STACK cancelalltimeouts P_((void));
void ARGS_ON_STACK canceltimeout P_((TIMEOUT *which));
void ARGS_ON_STACK cancelroottimeout P_((TIMEOUT *which));
void ARGS_ON_STACK timeout P_((void));
void checkalarms P_((void));
void ARGS_ON_STACK nap P_((unsigned n));

/* tty.c */
void tty_checkttou P_((FILEPTR *f, struct tty *tty));
long tty_read P_((FILEPTR *f, void *buf, long nbytes));
long tty_write P_((FILEPTR *f, const void *buf, long nbytes));
long tty_ioctl P_((FILEPTR *f, int mode, void *arg));
long tty_getchar P_((FILEPTR *f, int mode));
long tty_putchar P_((FILEPTR *f, long data, int mode));
long tty_select P_((FILEPTR *f, long proc, int mode));

/* util.c */
MEMREGION *addr2mem P_((virtaddr a));
PROC *pid2proc P_((int pid));
int newpid P_((void));
void zero P_((char *place, long size));
void * ARGS_ON_STACK kmalloc P_((long size));
void *kcore P_((long size));
void ARGS_ON_STACK kfree P_((void *place));
void * ARGS_ON_STACK umalloc P_((long size));
void ARGS_ON_STACK ufree P_((void *block));
void ARGS_ON_STACK ms_time P_((ulong ms, short *timeptr));
long ARGS_ON_STACK unixtim P_((unsigned time, unsigned date));
long ARGS_ON_STACK dostim P_((long t));
int ARGS_ON_STACK strnicmp P_((const char *str1, const char *str2, int len));
int ARGS_ON_STACK stricmp P_((const char *str1, const char *str2));
char * ARGS_ON_STACK strlwr P_((char *s));
char * ARGS_ON_STACK strupr P_((char *s));

#ifdef OWN_LIB
int strncmp P_((const char *str1, const char *str2, int len));
int strcmp P_((const char *str1, const char *str2));
char *strcat P_((char *, const char *));
char *strcpy P_((char *, const char *));
char *strncpy P_((char *, const char *, int));
int strlen P_((const char *));
char *strrchr P_((const char *, int));
int toupper P_((int));
int tolower P_((int));
long atol P_((const char *));
#endif /* OWN_LIB */

/* biosfs.c */
long rsvf_ioctl P_((int f, void *arg, int mode));
void biosfs_init P_((void));
long iwrite	P_((int bdev, const char *buf, long bytes, int ndelay, struct bios_file *b));
long iread	P_((int bdev, char *buf, long bytes, int ndelay, struct bios_file *b));
long iocsbrk	P_((int bdev, int mode, struct bios_tty *t));
void ARGS_ON_STACK mouse_handler P_((const char *buf));
long ARGS_ON_STACK nocreat P_((fcookie *dir, const char *name, unsigned mode, int attrib,
		 fcookie *fc));
long ARGS_ON_STACK nomkdir	P_((fcookie *dir, const char *name, unsigned mode));
long ARGS_ON_STACK nowritelabel P_((fcookie *dir, const char *name));
long ARGS_ON_STACK noreadlabel P_((fcookie *dir, char *name, int namelen));
long ARGS_ON_STACK nosymlink P_((fcookie *dir, const char *name, const char *to));
long ARGS_ON_STACK noreadlink P_((fcookie *dir, char *buf, int buflen));
long ARGS_ON_STACK nohardlink P_((fcookie *, const char *, fcookie *, const char *));
long ARGS_ON_STACK nofscntl P_((fcookie *dir, const char *name, int cmd, long arg));
long ARGS_ON_STACK nodskchng P_((int drv));
int set_auxhandle P_((PROC *, int));

/* pipefs.c */

/* procfs.c */

/* tosfs.c */

/* unifs.c */
FILESYS *get_filesys P_((int));
void unifs_init P_((void));

/* debug.c */
int ARGS_ON_STACK ksprintf P_((char *, const char *, ...));
void debug_ws P_((const char *s));
int _ALERT P_((char *));
void ARGS_ON_STACK Tracelow P_((const char *s, ...));
void ARGS_ON_STACK Trace P_((const char *s, ...));
void ARGS_ON_STACK Debug P_((const char *s, ...));
void ARGS_ON_STACK ALERT P_((const char *s, ...));
void ARGS_ON_STACK FORCE P_((const char *s, ...));
void PAUSE P_((void));
EXITING void ARGS_ON_STACK FATAL P_((const char *s, ...)) NORETURN;
EXITING void HALT P_((void)) NORETURN;
void DUMPLOG P_((void));
void do_func_key P_((int));

/* rendez.c */
long ARGS_ON_STACK p_msg P_((int mode, long ARGS_ON_STACK mbid, char *ptr));
long ARGS_ON_STACK p_semaphore P_((int mode, long ARGS_ON_STACK id, long timeout));
void free_semaphores P_((int pid));

/* memprot.c */
void init_tables P_((void));
int get_prot_mode P_((MEMREGION *));
void mark_region P_((MEMREGION *region, int mode));
void mark_proc_region P_((PROC *proc, MEMREGION *region, int mode));
int prot_temp P_((ulong loc, ulong len, int mode));
void init_page_table P_((PROC *proc));
void mem_prot_special P_((PROC *proc));
void QUICKDUMP P_((void));
void report_buserr P_((void));
void BIG_MEM_DUMP P_((int bigone, PROC *proc));
int mem_access_for P_((PROC *p, ulong where, long len));

/* nalloc2.c */
void nalloc_arena_add P_((void *start, long len));
void *nalloc P_((long size));
void nfree P_((void *start));
void NALLOC_DUMP P_((void));

/* realloc.c */
long realloc_region P_((MEMREGION *, long));
long ARGS_ON_STACK s_realloc P_((long));

/* welcome.c */
int boot_kernel_p P_((void));
