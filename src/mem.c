/*
Copyright 1990,1991,1992 Eric R. Smith.
Copyright 1992,1993,1994 Atari Corporation.
All rights reserved.
*/

/*
 * mem.c:: routines for managing memory regions
 */

#include "mint.h"
#include "fasttext.h" /* for line A stuff */

#ifndef VgetSize
#if defined(__GNUC__) && defined(trap_14_ww)
#define	VgetSize(mode) (long)trap_14_ww((short)91,(short)(mode))
#define Vsetmode(mode) (short)trap_14_ww((short)88,(short)(mode))
#else
extern long xbios();
#define VgetSize(mode) xbios(91, (short)(mode))
#define Vsetmode(mode) xbios(88, (short)(mode))
#endif
#endif

static long core_malloc P_((long, int));
static void core_free P_((long));
static void terminateme P_((int code));

/* macro for testing whether a memory region is free */
#define ISFREE(m) ((m)->links == 0)

/*
 * list of shared text regions currently being executed
 */
SHTEXT *text_reg = 0;

/*
 * initialize memory routines
 */

/* initial number of memory regions */
#define NREGIONS ((8*1024)/sizeof(MEMREGION))

/* number of new regions to allocate when the initial ones are used up */
#define NEWREGIONS ((8*1024)/sizeof(MEMREGION))

static MEMREGION use_regions[NREGIONS+1];
MEMREGION *rfreelist;

/* variable for debugging purposes; number of times we've needed
 * to get new regions
 */
int num_reg_requests = 0;

/* these variables are set in init_core(), and used in
 * init_mem()
 */
static ulong scrnsize, scrnplace;
static SCREEN *vscreen;

void
init_mem()
{
	int i;
	MEMREGION *r;
	long newbase;

	use_regions[NREGIONS].next = 0;
	for (i = 0; i < NREGIONS; i++) {
		use_regions[i].next = &use_regions[i+1];
	}
	rfreelist = use_regions;

	init_core();
	init_swap();

	init_tables();		    /* initialize MMU constants */

	/* mark all the regions in the core & alt lists as "invalid" */
	for (r = *core; r; r = r->next) {
	    mark_region(r,PROT_I);
	}
	for (r = *alt; r; r = r->next) {
	    mark_region(r,PROT_I);
	}

	/* make sure the screen is set up properly */
	newbase = s_realloc(scrnsize);

	/* if we did get a new screen, point the new screen
	 * at the right place after copying the data
	 * if possible, save the screen to another buffer,
	 * since if the new screen and old screen overlap
	 * the blit will look very ugly.
	 * Note that if the screen isn't moveable, then we set
	 * scrnsize to a ridiculously large value, and so the
	 * s_realloc above failed.
	 */
	if (newbase) {
	/* find a free region for temp storage */
		for (r = *core; r; r = r->next) {
			if (ISFREE(r) && r->len >= scrnsize)
				break;
		}

		if (r) {
			quickmove((char *)r->loc, (char *)scrnplace, scrnsize);
			Setscreen((void *)r->loc, (void *)r->loc, -1);
			Vsync();
			quickmove((char *)newbase, (char *)r->loc, scrnsize);
		} else {
			quickmove((char *)newbase, (char *)scrnplace, scrnsize);
		}
		Setscreen((void *)newbase, (void *)newbase, -1);
	/* fix the cursor */
		Cconws("\r\n"); 
	}
}

void
restr_screen()
{
  long base = (long) Physbase ();
  MEMREGION *r;

  if (base != scrnplace)
    {
      for (r = *core; r; r = r->next)
	{
	  if (ISFREE (r) && r->len >= scrnsize)
	    break;
	}
      if (r)
	{
	  quickmove ((char *) r->loc, (char *) base, scrnsize);
	  Setscreen ((void *) r->loc, (void *) r->loc, -1);
	  Vsync ();
	  quickmove ((char *) scrnplace, (char *) r->loc, scrnsize);
	}
      else
	quickmove ((char *) scrnplace, (char *) base, scrnsize);
      Setscreen ((void *) scrnplace, (void *) scrnplace, -1);
      Cconws ("\r\n"); 
    }
}

/*
 * init_core(): initialize the core memory map (normal ST ram) and also
 * the alternate memory map (fast ram on the TT)
 */

static MEMREGION *_core_regions = 0, *_alt_regions = 0,
	*_ker_regions = 0;

MMAP core = &_core_regions;
MMAP alt = &_alt_regions;
MMAP ker = &_ker_regions;

/* note: add_region must adjust both the size and starting
 * address of the region being added so that memory is
 * always properly aligned
 */

int
add_region(map, place, size, mflags)
	MMAP map;
	ulong place, size;
	unsigned mflags;	/* initial flags for region */
{
  	MEMREGION *m;
	ulong trimsize;

	TRACELOW(("add_region(map=%lx,place=%lx,size=%lx,flags=%x)",
		map,place,size,mflags));

	m = new_region();
	if (m == 0)
		return 0;	/* failure */
	m->links = 0;

	if (place & MASKBITS) {
	    /* increase place & shorten size by the amount we're trimming */
	    trimsize = (MASKBITS+1) - (place & MASKBITS);
	    if (size <= trimsize) goto lose;
	    size -= trimsize;
	    place += trimsize;
	}

	/* now trim size DOWN to a multiple of pages */
	if (size & MASKBITS) size &= ~MASKBITS;

	/* only add if there's anything left */
	if (size) {
	    m->len = size;
	    m->loc = place;
	    m->next = *map;
	    m->mflags = mflags;
	    *map = m;
	}
	else {
	    /* succeed but don't do anything; dispose of region */
lose:	    dispose_region(m);
	}
	return 1;	/* success */
}

static long
core_malloc(amt, mode)
	long amt;
	int mode;
{
	static int mxalloc = -1;	/* does GEMDOS know about Mxalloc? */
	long ret;

	if (mxalloc < 0) {
		ret = (long)Mxalloc(-1L, 0);
		if (ret == -32) mxalloc = 0;	/* unknown function */
		else if (ret >= 0) mxalloc = 1;
		else {
			ALERT("GEMDOS returned %ld from Mxalloc", ret);
			mxalloc = 0;
		}
	}
	if (mxalloc)
		return (long) Mxalloc(amt, mode);
	else if (mode == 1)
		return 0L;
	else
		return (long) Malloc(amt);
}

static void
core_free(where)
	long where;
{
	Mfree((void *)where);
}

void
init_core()
{
	extern int FalconVideo;	/* set in main.c */
	int scrndone = 0;
	ulong size;
	ulong place;
	ulong temp;
	void *tossave;

	tossave = (void *)core_malloc((long)TOS_MEM, 0);
	if (!tossave) {
		FATAL("Not enough memory to run MiNT");
	}

/* initialize kernel memory */
	place = (ulong)core_malloc(KERNEL_MEM, 3);
	if (place != 0) {
		nalloc_arena_add((void *)place,KERNEL_MEM);
	}

/*
 * find out where the screen is. We want to manage the screen
 * memory along with all the other memory, so that Srealloc()
 * can be used by the XBIOS to allocate screens from the
 * end of memory -- this avoids fragmentation problems when
 * changing resolutions.
 */
/* Note, however, that some graphics boards (e.g. Matrix)
 * are unable to change the screen address. We fake out the
 * rest of our code by pretending to have a really huge
 * screen that can't be changed.
 */
	scrnplace = (long)Physbase();

	vscreen = (SCREEN *)((char *)lineA0() - 346);
	if (FalconVideo) {
	/* the Falcon can tell us the screen size */
	    scrnsize = VgetSize(Vsetmode(-1));
	} else {
	/* otherwise, use the line A variables */
	    scrnsize = (vscreen->maxy+1)*(long)vscreen->linelen;
	}

/* check for a graphics card with fixed screen location */
#define phys_top_st (*(ulong *)0x42eL)

	if (scrnplace >= phys_top_st) {
/* screen isn't in ST RAM */
		scrnsize = 0x7fffffffUL;
		scrndone = 1;
	} else {
		temp = (ulong)core_malloc(scrnsize+256L, 0);
		if (temp) {
			(void)Setscreen((void *)-1L,
			        (void *)((temp+511)&(0xffffff00L)), -1);
			if ((long)Physbase() != ((temp+511)&(0xffffff00L))) {
				scrnsize = 0x7fffffffUL;
				scrndone = 1;
			}
			(void)Setscreen((void *)-1L, (void *)scrnplace, -1);
			core_free(temp);
		}
	}

/* initialize ST RAM */
	size = (ulong)core_malloc(-1L, 0);
	while (size > 0) {
		place = (ulong)core_malloc(size, 0);
		if (!scrndone && (place + size == scrnplace)) {
			size += scrnsize;
			scrndone = 1;
		}
		if (!add_region(core, place, size, M_CORE))
			FATAL("init_mem: unable to add a region");
		size = (ulong)core_malloc(-1L, 0);
	}

	if (!scrndone) {
		(void)add_region(core, scrnplace, scrnsize, M_CORE);
	}

/* initialize alternate RAM */
	size = (ulong)core_malloc(-1L, 1);
	while (size > 0) {
		place = (ulong)core_malloc(size, 1);
		if (!add_region(alt, place, size, M_ALT))
			FATAL("init_mem: unable to add a region");
		size = (ulong)core_malloc(-1L, 1);
	}

	(void)Mfree(tossave);		/* leave some memory for TOS to use */
}

/*
 * init_swap(): initialize the swap area; for now, this does nothing
 */

MEMREGION *_swap_regions = 0;
MMAP swap = &_swap_regions;

void
init_swap()
{
}

/*
 * routines for allocating/deallocating memory regions
 */

/*
 * new_region returns a new memory region descriptor, or NULL
 */

MEMREGION *
new_region()
{
	MEMREGION *m, *newfrees;
	int i;

	m = rfreelist;
	if (!m) {
		ALERT("new_region: ran out of free regions");
		return 0;
	}
	assert(ISFREE(m));
	rfreelist = m->next;
	m->next = 0;

/* if we're running low on free regions, allocate some more
 * we have to do this with at least 1 free region left so that get_region
 * has a chance of working
 */
	if (rfreelist && !rfreelist->next) {
		MEMREGION *newstuff;

		TRACELOW(("get_region: getting new region descriptors"));
		newstuff = get_region(ker, NEWREGIONS*SIZEOF(MEMREGION), PROT_S);
		if (!newstuff)
		    newstuff = get_region(alt,NEWREGIONS*SIZEOF(MEMREGION), PROT_S);
		if (!newstuff)
		    newstuff = get_region(core, NEWREGIONS*SIZEOF(MEMREGION), PROT_S);
		newfrees = newstuff ? (MEMREGION *)newstuff->loc : 0;
		if (newfrees) {
			num_reg_requests++;
			newfrees[NEWREGIONS-1].next = 0;
			newfrees[NEWREGIONS-1].links = 0;
			for (i = 0; i < NEWREGIONS-1; i++) {
				newfrees[i].next = &newfrees[i+1];
				newfrees[i].links = 0;
			}
			rfreelist = newfrees;
		} else {
			DEBUG(("couldn't get new region descriptors!"));
		}
	}

	return m;
}

/*
 * dispose_region destroys a memory region descriptor
 */

void
dispose_region(m)
	MEMREGION *m;
{
	m->next = rfreelist;
	rfreelist = m;
}

#if 0
/* notused: see dosmem.c */
/*
 * change_prot_status: change the status of a region to 'newmode'.  We're
 * given its starting address, not its region structure pointer, so we have
 * to find the region pointer; since this is illegal if proc doesn't own
 * the region, we know we'll find the region struct pointer in proc->mem.
 *
 * If the proc doesn't own it, you get EACCDN.  There are no other errors.
 * God help you if newmode isn't legal!
 */

long
change_prot_status(proc,start,newmode)
PROC *proc;
long start;
int newmode;
{
    MEMREGION **mr;
    int i;

    /* return EACCDN if you don't own the region in question */
    if (!proc->mem) return EACCDN;

    for (mr = proc->mem, i = 0; i < proc->num_reg; i++, mr++) {
	if ((*mr)->loc == start) goto found;
    }
    return EACCDN;

found:
    mark_region(*mr,newmode);
    return E_OK;
}
#endif

/*
 * virtaddr
 * attach_region(proc, reg): attach the region to the given process:
 * returns the address at which it was attached, or NULL if the process
 * cannot attach more regions. The region link count is incremented if
 * the attachment is successful.
 */

virtaddr
attach_region(proc, reg)
	PROC *proc;
	MEMREGION *reg;
{
	int i;
	MEMREGION **newmem;
	virtaddr *newaddr;

	TRACELOW(("attach_region %lx len %lx to pid %d",
	    reg->loc, reg->len, proc->pid));

	if (!reg || !reg->loc) {
		ALERT("attach_region: attaching a null region??");
		return 0;
	}

again:
	for (i = 0; i < proc->num_reg; i++) {
		if (!proc->mem[i]) {
			assert(proc->addr[i] == 0);
			reg->links++;
			proc->mem[i] = reg;
			proc->addr[i] = (virtaddr) reg->loc;
			mark_proc_region(proc,reg,PROT_P);
			return proc->addr[i];
		}
	}

/* Hmmm, OK, we have to expand the process' memory table */
	TRACELOW(("Expanding process memory table"));
	i = proc->num_reg + NUM_REGIONS;

	newmem = kmalloc(i * SIZEOF(MEMREGION *));
	newaddr = kmalloc(i * SIZEOF(virtaddr));

	if (newmem && newaddr) {
	/*
	 * We have to use temps while allocating and freeing mem
	 * and addr so the memory protection code won't walk this
	 * process' memory list in the middle.
	 */
		void *pmem, *paddr;

	/* copy over the old address mapping */
		for (i = 0; i < proc->num_reg; i++) {
			newmem[i] = proc->mem[i];
			newaddr[i] = proc->addr[i];
			if (newmem[i] == 0)
				assert(newaddr[i] == 0);
		}
	/* initialize the rest of the tables */
		for(; i < proc->num_reg + NUM_REGIONS; i++) {
			newmem[i] = 0;
			newaddr[i] = 0;
		}
	/* free the old tables (carefully! for memory protection) */
		pmem = proc->mem;
		paddr = proc->addr;
		proc->mem = NULL;
		proc->addr = NULL;
		kfree(pmem); kfree(paddr);
		proc->mem = newmem;
		proc->addr = newaddr;
		proc->num_reg += NUM_REGIONS;
	/* this time we will succeed */
		goto again;
	} else {
		if (newmem) kfree(newmem);
		if (newaddr) kfree(newaddr);
		DEBUG(("attach_region: failed"));
		return 0;
	}
}

/*
 * detach_region(proc, reg): remove region from the procedure's address
 * space. If no more processes reference the region, return it to the
 * system. Note that we search backwards, so that the most recent
 * attachment of memory gets detached!
 */

void
detach_region(proc, reg)
	PROC *proc;
	MEMREGION *reg;
{
	int i;

	if (!reg) return;

	TRACELOW(("detach_region %lx len %lx from pid %d",
	    reg->loc, reg->len, proc->pid));

	for (i = proc->num_reg - 1; i >= 0; i--) {
		if (proc->mem[i] == reg) {
			reg->links--;
			proc->mem[i] = 0; proc->addr[i] = 0;
			if (reg->links == 0) {
				free_region(reg);
			}
			else {
				/* cause curproc's table to be updated */
				mark_proc_region(proc,reg,PROT_I);
			}
			return;
		}
	}
	DEBUG(("detach_region: region not attached"));
}

/*
 * get_region(MMAP map, ulong size, int mode) -- allocate a new region of the
 * given size in the given memory map. if no region big enough is available,
 * return NULL, otherwise return a pointer to the region.
 * "mode" tells us about memory protection modes
 *
 * the "links" field in the region is set to 1
 *
 * BEWARE: new_region may call get_region (indirectly), so we have to be
 * _very_ careful with re-entrancy in this function
 */

MEMREGION *
get_region(map, size, mode)
	MMAP map;
	ulong size;
	int mode;
{
	MEMREGION *m, *n, *nlast, *nfirstp, *s;

	TRACELOW(("get_region(%s,%lx,%x)",
		(map == ker ? "ker" : (map == core ? "core" : "alt")),
		size, mode));

/* precautionary measures */
	if (size == 0) {
		DEBUG(("request for 0 bytes??"));
		size = 1;
	}

	size = ROUND(size);

	sanity_check(map);
/* exact matches are likely to be rare, so we pre-allocate a new
 * region here; this helps us to avoid re-entrancy problems
 * when new_region calls get_region
 */
	m = new_region();

/* We come back and try again if we found and freed any unattached shared
 * text regions.
 */
	nfirstp = NULL;
#if 0
retry:
#endif
	n = *map;
retry2:
	s = nlast = NULL;

	while (n) {
		if (ISFREE(n)) {
			if (n->len == size) {
				if (m) dispose_region(m);
				n->links++;
				goto win;
			}
			else if (n->len > size) {
/* split a new region, 'm', which will contain the free bytes after n */
				if (m) {
					m->next = n->next;
					n->next = m;
					m->mflags = n->mflags & M_MAP;
					m->loc = n->loc + size;
					m->len = n->len - size;
					n->len = size;
					n->links++;
					goto win;
				} else {
				    DEBUG(("get_region: no regions left"));
				    return 0;
				}
			}
			nlast = n;
/* If this is an unattached shared text region, leave it as a last resort */
		} else if (n->links == 0xffff && (n->mflags & M_SHTEXT)) {
			if (!s) {
				s = n;
				nfirstp = nlast;
			}
		}
		n = n->next;
	}

/* Looks like we're out of free memory. Try freeing an unattached shared text
 * region, and then try again to fill this request.
 */
#if 1
	if (s && s->len < size) {
		long lastsize = 0, end = 0;

		n = nlast = nfirstp;
		if (!n || n->next != s)
			n = s;
		for (; n; n = n->next) {
			if (ISFREE(n)) {
				if (end == n->loc) {
					lastsize += n->len;
				} else {
					s = NULL;
					nfirstp = nlast;
					lastsize = n->len;
				}
				nlast = n;
				end = n->loc + n->len;
				if (lastsize >= size) {
					break;
				}
			} else if (n->links == 0xffff && (n->mflags & M_SHTEXT)) {
				if (end == n->loc) {
					if (!s)
						s = n;
					lastsize += n->len;
				} else {
					s = n;
					nfirstp = nlast;
					lastsize = n->len;
				}
				end = n->loc + n->len;
				if (lastsize >= size) {
					break;
				}
			}
		}
		if (!n)
			s = NULL;
	}
	if (s) {
		s->links = 0;
		free_region(s);
		if (NULL == (n = nfirstp))
			n = *map;
		goto retry2;
	}
#else
	if (s) {
		s->links = 0;
		free_region(s);
		goto retry;
	}
#endif
		
	if (m)
		dispose_region(m);

	TRACELOW(("get_region: no memory left in this map"));
	return NULL;

win:
	mark_region(n, mode & PROT_PROTMODE);
	if (mode & M_KEEP) n->mflags |= M_KEEP;

	return n;
}

/*
 * free_region(MEMREGION *reg): free the indicated region. The map
 * in which the region is contained is given by reg->mflags.
 * the caller is responsible for making sure that the region
 * really should be freed, i.e. that reg->links == 0.
 *
 * special things to do:
 * if the region is a shared text region, we must close the
 * associated file descriptor
 */

void
free_region(reg)
	MEMREGION *reg;
{
	MMAP map;
	MEMREGION *m;
	SHTEXT *s, **old;

	if (!reg) return;

	assert(ISFREE(reg));

	if (reg->mflags & M_SHTEXT) {
		TRACE(("freeing shared text region"));
		old = &text_reg;
		for(;;) {
			s = *old;
			if (!s) break;
			if (s->text == reg) {
				if (s->f)
					do_close(s->f);
				*old = s->next;
				kfree(s);
				break;
			}
			old = &s->next;
		}
		if (!s) {
		    DEBUG(("No shared text entry for M_SHTEXT region??"));
		}
	}

	if (reg->mflags & M_CORE)
		map = core;
	else if (reg->mflags & M_ALT)
		map = alt;
	else if (reg->mflags & M_KER)
		map = ker;
	else {
		FATAL("free_region: region flags not valid (%x)", reg->mflags);
	}
	reg->mflags &= M_MAP;

/* unhook any vectors pointing into this region */
	unlink_vectors(reg->loc, reg->loc + reg->len);

/* BUG(?): should invalidate caches entries - a copyback cache could stuff
 * things into freed memory.
 *	cinv(reg->loc, reg->len);
 */
	m = *map;
	assert(m);

	/* MEMPROT: invalidate */
	if (map == core || map == alt)
	    mark_region(reg,PROT_I);

	if (m == reg) goto merge_after;

/* merge previous region if it's free and contiguous with 'reg' */

/* first, we find the region */
	while (m && m->next != reg)
		m = m->next;

	if (m == NULL) {
		FATAL("couldn't find region %lx: loc: %lx len: %ld",
			reg, reg->loc, reg->len);
	}

	if (ISFREE(m) && (m->loc + m->len == reg->loc)) {
		m->len += reg->len;
		assert(m->next == reg);
		m->next = reg->next;
		reg->next = 0;
		dispose_region(reg);
		reg = m;
	}

/* merge next region if it's free and contiguous with 'reg' */
merge_after:
	m = reg->next;
	if (m && ISFREE(m) && reg->loc + reg->len == m->loc) {
		reg->len += m->len;
		reg->next = m->next;
		m->next = 0;
		dispose_region(m);
	}

	sanity_check(map);
}

/*
 * shrink_region(MEMREGION *reg, ulong newsize):
 *   shrink region 'reg', so that it is now 'newsize' bytes long.
 *   if 'newsize' is bigger than the region's current size, return EGSBF;
 *   otherwise return 0.
 */

long
shrink_region(reg, newsize)
	MEMREGION *reg;
	ulong newsize;
{
	MEMREGION *n;
	ulong diff;


	newsize = ROUND(newsize);

	assert(reg->links > 0);

	if (!(reg->mflags & (M_CORE | M_ALT | M_KER))) {
		FATAL("shrink_region: bad region flags (%x)", reg->mflags);
	}

/* shrinking to 0 is the same as freeing */
	if (newsize == 0) {
		detach_region(curproc, reg);
		return 0;
	}

/* if new size is the same as old size, don't do anything */
	if (newsize == reg->len) {
		return 0;	/* nothing to do */
	}

	if (newsize > reg->len) {
		DEBUG(("shrink_region: request to make region bigger"));
		return EGSBF;	/* growth failure */
	}

/* OK, we're going to free (reg->len - newsize) bytes at the end of
   this block. If the block after us is already free, simply add the
   space to that block.
 */
	n = reg->next;
	diff = reg->len - newsize;

	if (n && ISFREE(n) && reg->loc + reg->len == n->loc) {
		reg->len = newsize;
		n->loc -= diff;
		n->len += diff;
		/* MEMPROT: invalidate the second half */
		/* (part of it is already invalid; that's OK) */
		mark_region(n,PROT_I);

		return 0;
	}
	else {
		n = new_region();
		if (!n) {
			DEBUG(("shrink_region: new_region failed"));
			return EINTRN;
		}
		reg->len = newsize;
		n->loc = reg->loc + newsize;
		n->len = diff;
		n->mflags = reg->mflags & M_MAP;
		n->next = reg->next;
		reg->next = n;
		/* MEMPROT: invalidate the new, free region */
		mark_region(n,PROT_I);
	}
	return 0;
}

/*
 * max_rsize(map, deeded): return the length of the biggest free region
 * in the given memory map, or 0 if no regions remain.
 * needed is minimun amount needed, if != 0 try to keep unattached
 * shared text regions, else count them all as free.
 */

long
max_rsize(map, needed)
	MMAP map;
	long needed;
{
	MEMREGION *m;
	long size = 0, lastsize = 0, end = 0;

	if (needed) {
		for (m = *map; m; m = m->next) {
			if (ISFREE(m) ||
			    (m->links == 0xfffe && !(m->mflags & M_SHTEXT))) {
				if (end == m->loc) {
					lastsize += m->len;
				} else {
					lastsize = m->len;
				}
				end = m->loc + m->len;
				if (lastsize > size) {
					size = lastsize;
				}
			}
		}
		if (size >= needed)
			return size;

		lastsize = end = 0;
	}
	for (m = *map; m; m = m->next) {
		if (ISFREE(m) || m->links == 0xfffe ||
		    (m->links == 0xffff && (m->mflags & M_SHTEXT))) {
			if (end == m->loc) {
				lastsize += m->len;
			} else {
				lastsize = m->len;
			}
			end = m->loc + m->len;
			if (lastsize > size) {
				if (needed && lastsize >= needed)
					return lastsize;
				size = lastsize;
			}
		}
	}
	return size;
}

/*
 * tot_rsize(map, flag): if flag == 1, return the total number of bytes in
 * the given memory map; if flag == 0, return only the number of free
 * bytes
 */

long
tot_rsize(map, flag)
	MMAP map;
	int flag;
{
	MEMREGION *m;
	long size = 0;

	for (m = *map; m; m = m->next) {
		if (flag || ISFREE(m) ||
		    (m->links == 0xffff && (m->mflags & M_SHTEXT))) {
			size += m->len;
		}
	}
	return size;
}

/*
 * alloc_region(MMAP map, ulong size, int mode): allocate a new region and
 * attach it to the current process; returns the address at which the region
 * was attached, or NULL. The mode argument is the memory protection mode to
 * give to get_region, and in turn to mark_region.
 */

virtaddr
alloc_region(map, size, mode)
	MMAP map;
	ulong size;
	int mode;
{
	MEMREGION *m;
	PROC *proc = curproc;
	virtaddr v;

	TRACELOW(("alloc_region(map,size: %lx,mode: %x)",size,mode));
	if (!size) {
	    DEBUG(("alloc_region of zero bytes?!"));
	    return 0;
	}

	m = get_region(map, size, mode);
	if (!m) {
		TRACELOW(("alloc_region: get_region failed"));
		return 0;
	}

/* sanity check: even addresses only, please */
	assert((m->loc & MASKBITS) == 0);

	v = attach_region(proc, m);
/* NOTE: get_region returns a region with link count 1; since attach_region
 * increments the link count, we restore it after calling attach_region
 */
	m->links = 1;
	if (!v) {
		m->links = 0;
		free_region(m);
		TRACE(("alloc_region: attach_region failed"));
		return 0;
	}
	return v;
}

/*
 * routines for creating a copy of an environment, and a new basepage.
 * note that the memory regions created should immediately be attached to
 * a process! Also note that create_env always operates in ST RAM, but
 * create_base might not.
 */

MEMREGION *
create_env(env, flags)
	const char *env;
	ulong flags;
{
	long size;
	MEMREGION *m;
	virtaddr v;
	const char *old;
	char *new;
	short protmode;

	if (!env) {
		env = ((BASEPAGE *)curproc->base)->p_env;
			/* duplicate parent's environment */
	}
	size = 2;
	old = env;
	while (*env || *(env+1))
		env++,size++;

	protmode = (flags & F_PROTMODE) >> F_PROTSHIFT;

	v = alloc_region(core, size, protmode);
	/* if core fails, try alt */
	if (!v)
	    v = alloc_region(alt, size, protmode);

	if (!v) {
		DEBUG(("create_env: alloc_region failed"));
		return (MEMREGION *)0;
	}
	m = addr2mem(v);

/* copy the old environment into the new */
	new = (char *) m->loc;
	TRACE(("copying environment: from %lx to %lx", old, new));
	while (size > 0) {
		*new++ = *old++;
		--size;
	}
	TRACE(("finished copying environment"));

	return m;
}

static void terminateme(code)
	int code;
{
	Pterm (code);
}

MEMREGION *
create_base(cmd, env, flags, prgsize, execproc, s, f, fh, xp)
	const char *cmd;
	MEMREGION *env;
	ulong flags, prgsize;
	PROC *execproc;
	SHTEXT *s;
	FILEPTR *f;
	FILEHEAD *fh;
	XATTR *xp;
{
	long len = 0, minalt = 0, coresize, altsize;
	MMAP map;
	MEMREGION *m, *savemem = 0;
	BASEPAGE *b, *bparent = 0;
	PROC *parent = 0;
	short protmode;
	int i, ismax = 1;

/* if we're about to do an exec tell max_rsize which of the exec'ing
   process regions will be freed, but don't free them yet so the process
   can still get an ENOMEM...
*/
	if (execproc) {
		for (i = 0; i < execproc->num_reg; i++) {
			m = execproc->mem[i];
			if (m && m->links == 1)
				m->links = 0xfffe;
		}

/* if parents mem saved because of a blocking fork that can be restored too
*/
		if (NULL != (parent = pid2proc(execproc->ppid)) &&
		    parent->wait_q == WAIT_Q && 
		    parent->wait_cond == (long)execproc) {
			for (i = 0; i < parent->num_reg; i++) {
				m = parent->mem[i];
				if (m && (m->mflags & M_FSAVED)) {
					m->links = 0xfffe;
					savemem = m;
					break;
				}
			}
		}
	}

/* if flags & F_ALTLOAD == 1, then we might decide to load in alternate
   RAM if enough is available. "enough" is: if more alt ram than ST ram,
   load there; otherwise, if more than (minalt+1)*128K alt ram available
   for heap space, load in alt ram ("minalt" is the high byte of flags)
 */
again2:
	if (flags & (F_ALTLOAD|F_SHTEXT)) {
		minalt = (flags & F_MINALT) >> 28L;
		minalt = len = (minalt+1)*128*1024L + prgsize + 256;
		if ((flags & F_MINALT) == F_MINALT)
			len = 0;
		else
			ismax = 0;
	}
again1:
	if (flags & F_ALTLOAD) {
		coresize = max_rsize(core, len);
		altsize = max_rsize(alt, len);
		if (altsize >= coresize) {
			map = alt;
			len = altsize;
		} else {
			if (altsize >= minalt) {
				map = alt;
				len = altsize;
			} else {
				map = core;
				len = coresize;
			}
		}
	}
	else
		len = max_rsize((map = core), len);

	if (savemem)
		savemem->links = 1;

	if (curproc->maxmem && len > curproc->maxmem) {
		if (ismax >= 0)
			len = curproc->maxmem;
		else if (len > curproc->maxmem+fh->ftext)
			len = curproc->maxmem+fh->ftext;
	}

/* make sure that a little bit of memory is left over */
	if (len > 2*KEEP_MEM) {
		len -= KEEP_MEM;
	}

	if (s && !s->text &&
	    (!(flags & F_ALTLOAD) || map == alt || altsize < fh->ftext)) {
		if (len > fh->ftext + KERNEL_MEM)
			len -= fh->ftext + KERNEL_MEM;
		else
			len = 0;
#if 1
		if (prgsize && len < prgsize + 0x400) {
			if (!ismax) {
				len = minalt + fh->ftext;
				ismax = -1;
				goto again1;
			}
			if ((s->text = addr2mem(alloc_region(map, fh->ftext, PROT_P)))) {
				goto again2;
			}
		}
#endif
	}

	if (prgsize && len < prgsize + 0x400) {
		/* can't possibly load this file in its eligible regions */
		DEBUG(("create_base: max_rsize smaller than prgsize"));

		if (execproc) {
/* error, undo the above */
			for (i = 0; i < execproc->num_reg; i++) {
				m = execproc->mem[i];
				if (m && m->links == 0xfffe)
					m->links = 1;
			}
		}
		if (s && !s->text) {
			kfree (s);
		}
		mint_errno = ENSMEM;
		return 0;
	}
	if (execproc) {
/* free exec'ing process memory... if the exec returns after this make it
   _exit (SIGKILL << 8);
*/
		*((short *) (execproc->stack + ISTKSIZE + sizeof (void (*)()))) =
			(SIGKILL << 8);
		execproc->ctxt[SYSCALL].term_vec = (long)rts;
		execproc->ctxt[SYSCALL].pc = (long)terminateme;
		execproc->ctxt[SYSCALL].sr |= 0x2000;
		execproc->ctxt[SYSCALL].ssp = (long)(execproc->stack + ISTKSIZE);

/* save basepage p_parent */
		bparent = execproc->base->p_parent;
		if (parent) {
			if (savemem)
				fork_restore(parent, savemem);
/* blocking forks keep the same basepage for parent and child,
   so this p_parent actually was our grandparents...  */
			bparent = parent->base;
		}
		for (i = 0; i < execproc->num_reg; i++) {
			m = execproc->mem[i];
			if (m && m->links == 0xfffe) {
				execproc->mem[i] = 0;
				execproc->addr[i] = 0;
				if (m->mflags & M_SHTEXT_T) {
					TRACE (("create_base: keeping sticky text segment (%lx, len %lx)",
						m->loc, m->len));
					m->links = 0xffff;
				} else {
					m->links = 0;
					free_region(m);
				}
			}
		}
	}
	protmode = (flags & F_PROTMODE) >> F_PROTSHIFT;

	m = 0;
	if (s && !s->f) {
		if (!s->text) {
			m = addr2mem(alloc_region(map, len + fh->ftext + KERNEL_MEM, protmode));
			if (!m ||
			    (((len > minalt &&
				((flags & F_MINALT) < F_MINALT) &&
				max_rsize (map, -1) < fh->ftext) ||
			      0 == (s->text = addr2mem(alloc_region(map, fh->ftext, PROT_P))) ||
			      (m->next == s->text &&
				!(detach_region (curproc, s->text), s->text = 0))) &&
			     shrink_region(m, fh->ftext))) {
				if (m)
					detach_region(curproc, m);
				kfree (s);
				mint_errno = ENSMEM;
				return 0;
			}
			if (!s->text) {
				s->text = m;
				if (protmode != PROT_P)
					mark_region(m, PROT_P);
				m = 0;
			}
		}
		s = get_text_seg(f, fh, xp, s, 0);
		if (!s) {
			if (m)
				detach_region(curproc, m);
			DEBUG(("create_base: unable to load shared text segment"));
/* mint_errno set in get_text_seg */
			return 0;
		}
	}

	if (!m) {
		m = addr2mem(alloc_region(map, len, protmode));
	}
	if (!m) {
		DEBUG(("create_base: alloc_region failed"));
		mint_errno = ENSMEM;
		return 0;
	}
	b = (BASEPAGE *)(m->loc);

	zero((char *)b, (long)sizeof(BASEPAGE));
	b->p_lowtpa = (long)b;
	b->p_hitpa = m->loc + m->len;
	b->p_env = (char *)env->loc;
	b->p_flags = flags;

	if (execproc) {
		execproc->base = b;
		b->p_parent = bparent;
	}

	if (cmd)
		strncpy(b->p_cmdlin, cmd, 126);
	return m;
}

/*
 * load_region(): loads the program with the given file name
 * into a new region, and returns a pointer to that region. On
 * an error, returns 0 and leaves the error number in mint_errno.
 * "env" points to an already set up environment region, as returned
 * by create_env. On success, "xp" points to the file attributes, which
 * Pexec has already determined, and "fp" points to the programs
 * prgflags. "text" is a pointer to a MEMREGION
 * pointer, which will be set to the region occupied by the shared
 * text segment of this program (if applicable).
 */

MEMREGION *
load_region(filename, env, cmdlin, xp, text, fp, isexec)
	const char *filename;
	MEMREGION *env;
	const char *cmdlin;
	XATTR *xp;		/* attributes for the file just loaded */
	MEMREGION **text;	/* set to point to shared text region,
				   if any */
	long *fp;		/* prgflags for this file */
	int isexec;		/* this is an exec*() (overlay) */
{
	FILEPTR *f;
	DEVDRV *dev;
	MEMREGION *reg, *shtext;
	BASEPAGE *b;
	long size, start;
	FILEHEAD fh;
	SHTEXT *s;

/* bug: this should be O_DENYW mode, not O_DENYNONE */
/* we must use O_DENYNONE because of the desktop and because of the
 * TOS file system brain-damage
 */
	f = do_open(filename, O_DENYNONE | O_EXEC, 0, xp);
	if (!f) {
		return 0;		/* mint_errno set by do_open */
	}

	dev = f->dev;
	size = (*dev->read)(f, (void *)&fh, (long)sizeof(fh));
	if (fh.fmagic != GEMDOS_MAGIC || size != (long)sizeof(fh)) {
		DEBUG(("load_region: file not executable"));
		mint_errno = ENOEXEC;
failed:
		do_close(f);
		return 0;
	}

	if (((fh.flag & F_PROTMODE) >> F_PROTSHIFT) > PROT_MAX_MODE) {
	    DEBUG (("load_region: invalid protection mode changed to private"));
	    fh.flag = (fh.flag & ~F_PROTMODE) | F_PROT_P;
	}
	*fp = fh.flag;

	if (fh.flag & F_SHTEXT) {
		TRACE(("loading shared text segment"));
		s = get_text_seg(f, &fh, xp, 0L, isexec);
		if (!s) {
			DEBUG(("load_region: unable to get shared text segment"));
/* mint_errno set in get_text_seg */
			goto failed;
		}
		size = fh.fdata + fh.fbss;
		shtext = s->text;
	} else {
		size = fh.ftext + fh.fdata + fh.fbss;
		shtext = 0;
		s = 0;
	}

	env->links++;
	if (s && !shtext) {
		reg = create_base(cmdlin, env, fh.flag, size,
			isexec ? curproc : 0L, s, f, &fh, xp);
		shtext = s->text;
	} else {
		if (shtext)
			shtext->links++;
		reg = create_base(cmdlin, env, fh.flag, size,
			isexec ? curproc : 0L, 0L, 0L, 0L, 0L);
		if (shtext) {
			shtext->links--;
#if 1
/* if create_base failed maybe the (sticky) text segment itself is
 * fragmenting memory... force it reloaded and have a second try
 */
			if (!reg && shtext->links == 1 && isexec) {
				s->f = 0;
				f->links--;
				detach_region(curproc, shtext);
				s = get_text_seg(f, &fh, xp, 0L, isexec);
				if (!s) {
					DEBUG(("load_region: unable to get shared text segment"));
					goto failed;
				}
				reg = create_base(cmdlin, env, fh.flag, size,
					curproc, s, f, &fh, xp);
				shtext = s->text;
			}
#endif
		}
	}
	env->links--;
	if (reg && size+1024L > reg->len) {
		DEBUG(("load_region: insufficient memory to load"));
		detach_region(curproc, reg);
		reg = 0;
		mint_errno = ENSMEM;
	}

	if (reg == 0) {
		if (shtext) {
			detach_region(curproc, shtext);
		}
		goto failed;
	}

	b = (BASEPAGE *)reg->loc;
	b->p_flags = fh.flag;
	if (shtext) {
		b->p_tbase = shtext->loc;
		b->p_tlen = 0;
		b->p_dbase = b->p_lowtpa + 256;
	} else {
		b->p_tbase = b->p_lowtpa + 256;
		b->p_tlen = fh.ftext;
		b->p_dbase = b->p_tbase + b->p_tlen;
	}
	b->p_dlen = fh.fdata;
	b->p_bbase = b->p_dbase + b->p_dlen;
	b->p_blen = fh.fbss;

/* if shared text, then we start loading at the end of the
 * text region, since that is already set up
 */
	if (shtext) {
	/* skip over text info */
		size = fh.fdata;
		start = fh.ftext;
	} else {
		size = fh.ftext + fh.fdata;
		start = 0;
	}

	mint_errno = (int)load_and_reloc(f, &fh, (char *)b+256, start,
			size, b);

	if (mint_errno) {
		detach_region(curproc, reg);
		if (shtext) detach_region(curproc, shtext);
		goto failed;
	}

	if (fh.flag & F_FASTLOAD)			/* fastload bit */
		size = b->p_blen;
	else
		size = b->p_hitpa - b->p_bbase;
	if (size > 0) {
		start = b->p_bbase;
		if (start & 1) {
			*(char *)start = 0;
			start++;
			--size;
		}
		zero((char *)start, size);
	}

	do_close(f);
	*text = shtext;
	return reg;
}

/*
 * load_and_reloc(f, fh, where, start, nbytes): load and relocate from
 * the open GEMDOS executable file f "nbytes" bytes starting at offset
 * "start" (relative to the end of the file header, i.e. from the first
 * byte of the actual program image in the file). "where" is the address
 * in (physical) memory into which the loaded image must be placed; it is
 * assumed that "where" is big enough to hold "nbytes" bytes!
 */

long
load_and_reloc(f, fh, where, start, nbytes, base)
	FILEPTR *f;
	FILEHEAD *fh;
	char *where;
	long start;
	long nbytes;
	BASEPAGE *base;
{
	unsigned char c, *next;
	long r;
	DEVDRV *dev;
#define LRBUFSIZ 8196
	static unsigned char buffer[LRBUFSIZ];
	long fixup, size, bytes_read;
	long reloc;


TRACE(("load_and_reloc: %ld to %ld at %lx", start, nbytes+start, where));
	dev = f->dev;

	r = (*dev->lseek)(f, start+sizeof(FILEHEAD), SEEK_SET);
	if (r < 0) return r;
	r = (*dev->read)(f, where, nbytes);
	if (r != nbytes) {
		DEBUG(("load_region: unexpected EOF"));
		return ENOEXEC;
	}

/* now do the relocation */
/* skip over symbol table, etc. */
	r = (*dev->lseek)(f, sizeof(FILEHEAD) + fh->ftext + fh->fdata +
			fh->fsym, SEEK_SET);
	if (r < 0) return ENOEXEC;

	if (fh->reloc != 0 || (*dev->read)(f, (char *)&fixup, 4L) != 4L
	    || fixup == 0) {
		return 0;	/* no relocation to be performed */
	}

	size = LRBUFSIZ;
	bytes_read = 0;
	next = buffer;

	do {
		if (fixup >= nbytes + start) {
			TRACE(("load_region: end of relocation at %ld", fixup));
			break;
		}
		else if (fixup >= start) {
			reloc = *((long *)(where + fixup - start));
			if (reloc < fh->ftext) {
				reloc += base->p_tbase;
			} else if (reloc < fh->ftext + fh->fdata && base->p_dbase) {
				reloc += base->p_dbase - fh->ftext;
			} else if (reloc < fh->ftext + fh->fdata + fh->fbss && base->p_bbase) {
				reloc += base->p_bbase - (fh->ftext + fh->fdata);
			} else {
				DEBUG(("load_region: bad relocation: %ld", reloc));
				if (base->p_dbase)
				    reloc += base->p_dbase - fh->ftext;	/* assume data reloc */
				else if (base->p_bbase)
				    reloc += base->p_bbase - (fh->ftext + fh->fdata);
				else
				    return ENOEXEC;
			}
			*((long *)(where + fixup - start)) = reloc;
		}
		do {
			if (!bytes_read) {
				bytes_read =
				    (*dev->read)(f,(char *)buffer,size);
				next = buffer;
			}
			if (bytes_read < 0) {
				DEBUG(("load_region: EOF in relocation"));
				return ENOEXEC;
			}
			else if (bytes_read == 0)
				c = 0;
			else {
				c = *next++; bytes_read--;
			}
			if (c == 1) fixup += 254;
		} while (c == 1);
		fixup += ( (unsigned) c) & 0xff;
	} while (c);

	return 0;
}

/*
 * function to check for existence of a shared text region
 * corresponding to file "f", and if none is found, to create one
 * the memory region being returned is attached to the current
 * process
 */

SHTEXT *
get_text_seg(f, fh, xp, s, noalloc)
	FILEPTR *f;
	FILEHEAD *fh;
	XATTR *xp;
	SHTEXT *s;
	int noalloc;
{
	MEMREGION *m;
	long r;
	BASEPAGE b;

	if (s) {
		m = s->text;
	} else {
		s = text_reg;

		while(s) {
			if (s->f && samefile(&f->fc, &s->f->fc) &&
			    xp->mtime == s->mtime &&
			    xp->mdate == s->mdate)
			{
				m = s->text;
/* Kludge for unattached shared region */
				if (m->links == 0xffff)
					m->links = 0;
				if (attach_region(curproc, m)) {
TRACE(("re-using shared text region %lx", m));
					return s;
				}
				else {
					mint_errno = ENSMEM;
					return 0;
				}
			}
			s = s->next;
		}

/* hmmm, not found; OK, we'll have to create a new text region */

		s = kmalloc(SIZEOF(SHTEXT));
		if (!s) {
			mint_errno = ENSMEM;
			return 0;
		}
		if (noalloc) {
			s->f = 0;
			s->text = 0;
			return s;
		}
		m = 0;
	}

	if (!m) {
/* actually, I can't see why loading in TT RAM is ever undesireable,
 * since shared text programs should be very clean (and since only
 * the text segment is going in there). But better safe than sorry.
 */
		if (fh->flag & F_ALTLOAD) {
			m = addr2mem(alloc_region(alt, fh->ftext, PROT_P));
		}
		if (!m)
			m = addr2mem(alloc_region(core, fh->ftext, PROT_P));
	}

	if (!m) {
		kfree(s);
		mint_errno = ENSMEM;
		return 0;
	}

/* set up a fake "basepage" for load_and_reloc
 * note: the 0 values should make load_and_reloc
 * barf on any attempts at data relocation, since we have
 * no data segment
 */
TRACE(("attempting to create shared text region"));

	b.p_tbase = m->loc;
	b.p_tlen = fh->ftext;
	b.p_dbase = 0;
	b.p_dlen = 0;
	b.p_bbase = b.p_blen = 0;

	r = load_and_reloc(f, fh, (char *)m->loc, 0, fh->ftext, &b);
	if (r) {
		detach_region(curproc, m);
		kfree(s);
		return 0;
	}

/* region has valid shared text data */
	m->mflags |= M_SHTEXT;
#if 1
	if (xp->mode & 01000)
#endif
/* make it sticky (this should depend on the files mode!?) */
	m->mflags |= M_SHTEXT_T;

/*
 * KLUDGE: to make sure we always have up to date shared text
 * info, even across a network, we leave the file passed
 * to us open with DENYWRITE mode, so that nobody will
 * modify it.
 */
	f->links++;	/* keep the file open longer */

/* BUG: what if someone already has the file open for
 * writing? Then we could get screwed...
 */
	f->flags = (f->flags & ~O_SHMODE) | O_DENYW;
	s->f = f;
	s->text = m;
	s->next = text_reg;
	s->mtime = xp->mtime;
	s->mdate = xp->mdate;
	text_reg = s;
TRACE(("shared text region %lx created", m));
	return s;
}

/*
 * function to just check for existence of a shared text region
 * corresponding to file "f"
 */

MEMREGION *
find_text_seg(f)
	FILEPTR *f;
{
	SHTEXT *s;

	for (s = text_reg; s; s = s->next) {
		if (s->f && samefile(&f->fc, &s->f->fc))
			return s->text;
	}
	return 0;
}

/*
 * exec_region(p, mem, thread): create a child process out of a mem region
 * "p" is the process structure set up by the parent; it may be "curproc",
 * if we're overlaying. "mem" is the loaded memory region returned by
 * "load region". Any open files (other than the standard handles) owned
 * by "p" are closed, and if thread !=0 all memory is released; the caller
 * must explicitly attach the environment and base region. The caller must
 * also put "p" on the appropriate queue (most likely READY_Q).
 */

extern long mint_dos(), mint_bios();

void rts() {}		/* dummy termination routine */

PROC *
exec_region(p, mem, thread)
	PROC	  *p;
	MEMREGION *mem;
	int thread;
{
	BASEPAGE *b;
	FILEPTR *f;
	int i;
	MEMREGION *m;

	TRACE(("exec_region"));

	b = (BASEPAGE *) mem->loc;

	cpush((void *)b->p_tbase, b->p_tlen);	/* flush cached versions of the text */
	
/* set some (undocumented) variables in the basepage */
	b->p_defdrv = p->curdrv;
	for (i = 0; i < 6; i++)
		b->p_devx[i] = i;

	p->dta = (DTABUF *)(b->p_dta = &b->p_cmdlin[0]);
	p->base = b;

/* close extra open files */
	for (i = MIN_OPEN; i < MAX_OPEN; i++) {
		if ( (f = p->handle[i]) != 0 && (p->fdflags[i] & FD_CLOEXEC) ) {
			do_pclose(p, f);
			p->handle[i] = 0;
		}
	}

/* initialize memory */
	recalc_maxmem(p);
	if (p->maxmem) {
		shrink_region(mem, p->maxmem);
		b->p_hitpa = b->p_lowtpa + mem->len;
	}

	p->memflags = b->p_flags;

	if (!thread) {
		for (i = 0; i < p->num_reg; i++) {
			m = p->mem[i];
			if (m) {
				m->links--;
#if 1
				if (m->links <= 0) {
					if (!m->links) {
						if (m->mflags & M_SHTEXT_T) {
							TRACE (("exec_region: keeping sticky text segment (%lx, len %lx)",
								m->loc, m->len));
							m->links = 0xffff;
						} else
							free_region(m);
					} else
						ALERT ("exec_region: region %lx bogus link count %d, not freed (len %lx)",
							m->loc, m->links, m->len);
				}
#else
				if (m->links <= 0)
					free_region(m);
#endif
			}
		}
		if (p->num_reg > NUM_REGIONS) {
			/*
			 * If the proc struct has a larger mem array than
			 * the default, then free it and allocate a
			 * default-sized one.
			 */

			/*
			 * hoo ha! Memory protection problem here. Use
			 * temps and pre-clear p->mem so memprot doesn't try
			 * to walk these structures as we're freeing and
			 * reallocating them!  (Calling kmalloc can cause
			 * a table walk if the alloc results in calling
			 * get_region.)
			 */
			void *pmem, *paddr;

			pmem = p->mem;
			paddr = p->addr;
			p->mem = NULL; p->addr = NULL;
			kfree(pmem); kfree(paddr);

			pmem = kmalloc(NUM_REGIONS * SIZEOF(MEMREGION *));
			paddr = kmalloc(NUM_REGIONS * SIZEOF(virtaddr));
			assert(pmem && paddr);
			p->mem = pmem;
			p->addr = paddr;
			p->num_reg = NUM_REGIONS;
		}
		zero((char *)p->mem, (p->num_reg)*SIZEOF(MEMREGION *));
		zero((char *)p->addr, (p->num_reg)*SIZEOF(virtaddr));
	}

/* initialize signals */
	p->sigmask = 0;
	for (i = 0; i < NSIG; i++) {
		if (p->sighandle[i] != SIG_IGN) {
			p->sighandle[i] = SIG_DFL;
			p->sigflags[i] = 0;
			p->sigextra[i] = 0;
		}
	}

/* zero the user registers, and set the FPU in a "clear" state */
	for (i = 0; i < 15; i++)
		p->ctxt[CURRENT].regs[i] = 0;
	p->ctxt[CURRENT].sr = 0;
	p->ctxt[CURRENT].fstate[0] = 0;

/* set PC, stack registers, etc. appropriately */
	p->ctxt[CURRENT].pc = b->p_tbase;

/* The "-0x20" is to make sure that syscall.s won't run past the end of
 * memory when the user makes a system call and doesn't push very many
 * parameters -- syscall always tries to copy the maximum possible number
 * of parms.
 *
 * NOTE: there's a sanity check here in case programs Mshrink a basepage
 * without fixing the p_hitpa field in the basepage; this is to ensure
 * compatibility with older versions of MiNT, which ignore p_hitpa.
 */
	if (valid_address(b->p_hitpa - 0x20))
		p->ctxt[CURRENT].usp = b->p_hitpa - 0x20;
	else
		p->ctxt[CURRENT].usp = mem->loc + mem->len - 0x20;

	p->ctxt[CURRENT].ssp = (long)(p->stack + ISTKSIZE);
	p->ctxt[CURRENT].term_vec = (long)rts;

/* set up stack for process */
	*((long *)(p->ctxt[CURRENT].usp + 4)) = (long) b;

/* check for a valid text region. some compilers (e.g. Lattice 3) just throw
   everything into the text region, including data; fork() must be careful
   to save the whole region, then. We assume that if the compiler (or
   assembler, or whatever) goes to the trouble of making separate text, data,
   and bss regions, then the text region is code and isn't modified and
   fork doesn't have to save it.
 */
	if (b->p_blen != 0 || b->p_dlen != 0)
		p->txtsize = b->p_tlen;
	else
		p->txtsize = 0;

/*
 * An ugly hack: dLibs tries to poke around in the parent's address space
 * to find stuff. For now, we'll allow this by faking a pointer into
 * the parent's address space in the place in the basepage where dLibs is
 * expecting it. This ugly hack only works correctly if the Pexec'ing
 * program (i.e. curproc) is in user mode.
 */
	if (curproc != rootproc)
		curproc->base->p_usp = curproc->ctxt[SYSCALL].usp - 0x32;

	return p;
}

/*
 * misc. utility routines
 */

/*
 * long memused(p): return total memory allocated to process p
 */

long
memused(p)
	PROC *p;
{
	int i;
	long size;

	/* a ZOMBIE owns no memory and its mem array ptr is zero */
	if (p->mem == NULL) return 0;

	size = 0;
	for (i = 0; i < p->num_reg; i++) {
		if (p->mem[i]) {
			if (p->mem[i]->mflags & (M_SEEN|M_FSAVED))
				continue;	/* count links only once */
			p->mem[i]->mflags |= M_SEEN;
			size += p->mem[i]->len;
		}
	}
	for (i = 0; i < p->num_reg; i++) {
		if (p->mem[i])
			p->mem[i]->mflags &= ~M_SEEN;
	}
	return size;
}

/* 
 * recalculate the maximum memory limit on a process; this limit depends
 * on the max. allocated memory and max. total memory limits set by
 * p_setlimit (see dos.c), and (perhaps) on the size of the program
 * that the process is executing. whenever any of these things
 * change (through p_exec or p_setlimit) this routine must be called
 */

void
recalc_maxmem(p)
	PROC *p;
{
	BASEPAGE *b;
	long siz;

	b = (BASEPAGE *)p->base;
	if (b)
		siz = b->p_tlen + b->p_dlen + b->p_blen;
	else
		siz = 0;
	p->maxmem = 0;
	if (p->maxdata) {
		p->maxmem = p->maxdata + siz;
	}

	if (p->maxcore) {
		if (p->maxmem == 0 || p->maxmem > p->maxcore)
			p->maxmem = p->maxcore;
	}
	if (p->maxmem && p->maxmem < siz)
		p->maxmem = siz;
}

/*
 * valid_address: checks to see if the indicated address falls within
 * memory attached to the current process
 */

int
valid_address(addr)
	long addr;
{
	int i;
	MEMREGION *m;

	for (i = 0; i < curproc->num_reg; i++) {
		if ((m = curproc->mem[i]) != 0) {
			if (addr >= m->loc && addr <= m->loc + m->len)
				return 1;
		}
	}
	return 0;
}

/*
 * convert an address to a memory region; this works only in
 * the ST RAM and TT RAM maps, and will fail for memory that
 * MiNT doesn't own or which is virtualized
 */

MEMREGION *
addr2region(addr)
	long addr;
{
	unsigned long ua = (unsigned long) addr;

	extern ulong mint_top_st, mint_top_tt;
	MEMREGION *r;
	MMAP map;

	if (ua < mint_top_st) {
		map = core;
	} else if (ua < mint_top_tt) {
		map = alt;
	} else {
		return 0;
	}

	for (r = *map; r; r = r->next) {
		if (addr >= r->loc && addr < r->loc + r->len)
			return r;
	}
	return 0;
}

/*
 * some debugging stuff
 */

void
DUMP_ALL_MEM()
{
#ifdef DEBUG_INFO
	DUMPMEM(ker);
	DUMPMEM(core);
	DUMPMEM(alt);
	FORCE("new memory region descriptor pages: %d", num_reg_requests);
#endif
}

void
DUMPMEM(map)
	MMAP map;
{
#ifdef DEBUG_INFO
	MEMREGION *m;

	m = *map;
	FORCE("%s memory dump: starting at region %lx",
		(map == ker ? "ker" : (map == core ? "core" : "alt")), m);
	while (m) {
	    FORCE("%ld bytes at %lx (%d links, mflags %x); next %lx", m->len, m->loc,
		    m->links, m->mflags, m->next);
	    m = m->next;
	}
#else
	UNUSED(map);
#endif
}

void
sanity_check(map)
	MMAP map;
{
#ifdef SANITY_CHECK
	MEMREGION *m, *nxt;
	long end;

	m = *map;
	while (m) {
		nxt = m->next;
		if (nxt) {
			end = m->loc + m->len;
			if (m->loc < nxt->loc && end > nxt->loc) {
				FATAL("MEMORY CHAIN CORRUPTED");
			}
			else if (end == nxt->loc && ISFREE(m) && ISFREE(nxt)) {
				ALERT("Continguous memory regions not merged!");
			}
		}
		m = nxt;
	}
#else
	UNUSED(map);
#endif
}
