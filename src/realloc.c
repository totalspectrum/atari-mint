/*
 * Copyright 1992 Atari Corporation.
 * All rights reserved.
 */

#include "mint.h"

/* macro for testing whether a memory region is free */
#define ISFREE(m) ((m)->links == 0)

/*
 * long
 * realloc_region(MEMREGION *reg, long newsize):
 * attempt to resize "reg" to the indicated size. If newsize is
 * less than the current region size, the call always
 * succeeds; otherwise, we look for free blocks next to the
 * region, and try to merge these.
 *
 * If newsize == -1L, simply returns the maximum size that
 * the block could be allocated to.
 *
 * Returns: the (physical) address of the new bottom of the
 * region, or 0L if the resize attempt fails.
 *
 * NOTES: if reg == 0, this call does a last-fit allocation
 * of memory of the requested size, and returns a MEMREGION *
 * (cast to a long) pointing at the last region that works
 *
 * This call works ONLY in the "core" memory region (aka ST RAM)
 * and only on non-shared text regions.
 */

long
realloc_region(reg, newsize)
	MEMREGION *reg;
	long newsize;
{
	MMAP map;
	MEMREGION *m,*prevptr;
	long oldsize, trysize;

	if (newsize != -1L)
		newsize = ROUND(newsize);
	oldsize = reg->len;

	if ( reg == 0 || (reg->mflags & M_CORE))
		map = core;
	else {
		return 0;
	}

/* last fit allocation: this is pretty straightforward,
 * we just look for the last block that would work
 * and slice off the top part of it.
 * problem: we don't know what the "last block that would fit"
 * is for newsize == -1L, so we look for the biggest block
 */
	if (reg == 0) {
		MEMREGION *lastfit = 0;
		MEMREGION *newm = new_region();

		for (m = *map; m; m = m->next) {
			if (ISFREE(m)) {
				if (newsize == -1L && lastfit
				    && m->len >= lastfit->len)
					lastfit = m;
				else if (m->len >= newsize)
					lastfit = m;
			}
		}
		if (!lastfit)
			return 0;

		if (newsize == -1L)
			return lastfit->len;

	/* if the sizes match exactly, we save a bit of work */
		if (lastfit->len == newsize) {
			if (newm) dispose_region(newm);
			lastfit->links++;
			mark_region(lastfit, PROT_G);
			return (long)lastfit;
		}
		if (!newm) return 0;	/* can't get a new region */

	/* chop off the top "newsize" bytes from lastfit */
	/* and add it to "newm" */
		lastfit->len -= newsize;
		newm->loc = lastfit->loc + lastfit->len;
		newm->len = newsize;
		newm->mflags = lastfit->mflags & M_MAP;
		newm->links++;
		newm->next = lastfit->next;
		lastfit->next = newm;
		mark_region(newm, PROT_G);
		return (long)newm;
	}

/* check for trivial resize */
	if (newsize == oldsize) {
		return reg->loc;
	}

/*
 * find the block just before ours
 */
	if (*map == reg)
		prevptr = 0;
	else {
		prevptr = *map;
		while (prevptr->next != reg && prevptr) {
			prevptr = prevptr->next;
		}
	}
/*
 * If we're shrinking the block, there's not too much to
 * do (we just free the first "oldsize-newsize" bytes by
 * creating a new region, putting those bytes into it,
 * and freeing it).
 */
	if (newsize < oldsize && newsize != -1L) {

		if (prevptr && ISFREE(prevptr)) {
/* add this memory to the previous free region */
			prevptr->len += oldsize - newsize;
			reg->loc += oldsize - newsize;
			reg->len -= oldsize - newsize;
			mark_region(prevptr, PROT_I);
			mark_region(reg, PROT_G);
			return reg->loc;
		}

/* make a new region for the freed memory */
		m = new_region();
		if (!m) {
		/* oops, couldn't get a region -- we lose */
		/* punt and pretend we succeeded; after all,
		 * we have enough memory!
		 */
			return reg->loc;
		}

	/* set up the fake region */
		m->links = 0;
		m->mflags = reg->mflags & M_MAP;
		m->loc = reg->loc;
		m->len = oldsize - newsize;
	/* update our region (it's smaller now) */
		reg->loc += m->len;
		reg->len -= m->len;
	/* link the region in just ahead of us */
		if (prevptr)
			prevptr->next = m;
		else
			*map = m;
		m->next = reg;
		mark_region(m, PROT_I);
		mark_region(reg, PROT_G);
		return reg->loc;
	}

/* OK, here we have to grow the region: to do this, we first try adding
 * bytes from the region after us (if any) and then the region before
 * us
 */
	trysize = oldsize;
	if (reg->next && ISFREE(reg->next) &&
	    (reg->loc + reg->len == reg->next->loc)) {
		trysize += reg->next->len;
	}
	if (prevptr && ISFREE(prevptr) &&
	    (prevptr->loc + prevptr->len == reg->loc)) {
		trysize += prevptr->len;
	}
	if (trysize < newsize) {
FORCE("realloc_region: need %ld bytes, only have %ld", trysize, newsize);
		return 0;	/* not enough room */
	}

	if (newsize == -1L)	/* size inquiry only?? */
		return trysize;

/* BUG: we can be a bit too aggressive at sweeping up
 * memory regions coming after our region; on the other
 * hand, unless something goes seriously wrong there
 * never should *be* any such regions
 */
	if (reg->next && ISFREE(reg->next) && 
	    (reg->loc + reg->len == reg->next->loc)) {
		MEMREGION *foo = reg->next;

		reg->len += foo->len;
		reg->next = foo->next;
		dispose_region(foo);
		mark_region(reg, PROT_G);
		if (reg->len >= newsize)
			return reg->loc;
		oldsize = reg->len;
	}
	assert(prevptr && ISFREE(prevptr) &&
		prevptr->loc + prevptr->len == reg->loc);

	if (newsize > oldsize) {
		reg->loc -= (newsize - oldsize);
		reg->len += (newsize - oldsize);
		prevptr->len -= (newsize - oldsize);
		if (prevptr->len == 0) {
	/* hmmm, we used up the whole region -- we must dispose of the
	 * region descriptor
	 */
			if (*map == prevptr)
				*map = prevptr->next;
			else {
				for (m = *map; m; m = m->next) {
					if (m->next == prevptr) {
						m->next = prevptr->next;
						break;
					}
				}
			}
			dispose_region(prevptr);
		}
		mark_region(reg, PROT_G);
	}

/* finally! we return the new starting address of "our" region */
	return reg->loc;
}



/*
 * s_realloc emulation: this isn't quite perfect, since the memory
 * used up by the "first" screen will be wasted (we could recover
 * this if we knew the screen start and size, and manually built
 * a region for that screen and linked it into the "core" map
 * (probably at the end))
 * We must always ensure a 256 byte "pad" area is available after
 * the screen (so that it doesn't abut the end of memory)
 */

MEMREGION *screen_region = 0;

#define PAD 256L

long ARGS_ON_STACK
s_realloc(size)
	long size;
{
	long r;

	TRACE(("s_realloc(%ld)", size));

	if (size != -1L)
		size += PAD;

	if (!screen_region) {
		r = realloc_region(screen_region, size);
		if (size == -1L) {	/* inquiry only */
			TRACE(("%ld bytes max srealloc", r-PAD));
			return r - PAD;
		}
		screen_region = (MEMREGION *)r;
		if (!screen_region) {
			DEBUG(("s_realloc: no screen region!!"));
			return 0;
		}
		return screen_region->loc;
	}
	r = realloc_region(screen_region, size);
	if (size == -1L) {
		return r - PAD;
	} else {
		return r;
	}
}
