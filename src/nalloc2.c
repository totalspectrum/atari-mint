/*
 * Copyright 1992,1993,1994 Atari Corporation.
 * All rights reserved.
 */

/*
 * General-purpose memory allocator, on the MWC arena model, with
 * this added feature:
 *
 * All blocks are coalesced when they're freed.  If this results in
 * an arena with only one block, and that free, it's returned to the
 * OS.
 *
 * The functions here have the same names and bindings as the MWC
 * memory manager, which is the same as the UNIX names and bindings.
 *
 * MiNT version: used for kmalloc to manage kernel memory in small hunks,
 * rather than using a page at a time.
 */

#include "mint.h"

#if 0
#define NALLOC_DEBUG(c) TRACE(("nalloc: %c",c));
#else
#define NALLOC_DEBUG(c) /* nothing */
#endif

#define NKMAGIC 0x19870425L

/*
 * block header: every memory block has one.
 * A block is either allocated or on the free
 * list of the arena.  There is no allocated list: it's not necessary.
 * The next pointer is only valid for free blocks: for allocated blocks
 * maybe it should hold a verification value..?
 *
 * Zero-length blocks are possible and free; they hold space which might 
 * get coalesced usefully later on.
 */

struct block {
    struct block *b_next;   /* NULL for last guy; next alloc or next free */
    long b_size;
};

/*
 * arena header: every arena has one.  Each arena is always completely
 * filled with blocks; the first starts right after this header.
 */

struct arena {
    struct arena *a_next;
    struct block *a_ffirst;
    long a_size;
};

/*
 * Arena linked-list pointer, and block size.
 */

static struct arena *a_first = (struct arena *)NULL;

void
nalloc_arena_add(start,len)
void *start;
long len;
{
    struct arena *a;
    struct block *b;

    for (a = a_first; a && a->a_next; a = a->a_next)
	continue;
    if (a)
        a->a_next = (struct arena *)start;
    else
        a_first = (struct arena *)start;
    a = start;
    a->a_next = NULL;
    a->a_ffirst = b = (struct block *)(a+1);
    a->a_size = len - sizeof(*a);
    b->b_next = NULL;
    b->b_size = (len - sizeof(*a) - sizeof(*b));
}
	

void *
nalloc(size)
long size;
{
    struct arena *a;
    struct block *b, *mb, **q;
    long temp;

    NALLOC_DEBUG('A');

    /* force even-sized alloc's */
    size = (size+1) & ~1;

    for (a = a_first; a; a = a->a_next) {
	for (b = *(q = &a->a_ffirst); b; b = *(q = &b->b_next)) {
	    /* if big enough, use it */
	    if (b->b_size >= (size + sizeof(struct block))) {

		/* got one */
		mb = b;

		/* cut the free block into an allocated part & a free part */
		temp = mb->b_size - size - sizeof(struct block);
		if (temp >= 0) {
		    /* large enough to cut */
		    NALLOC_DEBUG('c');
		    b = (struct block *)(((char *)(b+1)) + size);
		    b->b_size = temp;
		    b->b_next = mb->b_next;
		    *q = b;
		    mb->b_size = size;
		} else {
		    /* not big enough to cut: unlink this from free list */
		    NALLOC_DEBUG('w');
		    *q = mb->b_next;
		}
		mb->b_next = (struct block *)NKMAGIC;
		return (void *)(mb+1);
	    }
	}
    }

    /* no block available: get a new arena */

#if 1
    return NULL;	/* MiNT: fail. */
#else
    if (!minarena) {
	minarena = Malloc(-1L) / 20;
	if (minarena > MAXARENA) minarena = MAXARENA;
    }

    if (size < minarena) {
	NALLOC_DEBUG('m');
	temp = minarena;
    }
    else {
	NALLOC_DEBUG('s');
	temp = size;
    }

    a = (struct arena *)Malloc(temp + 
				sizeof(struct arena) +
				sizeof(struct block));

    /* if Malloc failed return failure */
    if (a == 0) {
    	NALLOC_DEBUG('x');
    	return 0;
    }

    a->a_size = temp + sizeof(struct block);
    a->a_next = a_first;
    a_first = a;
    mb = (struct block *)(a+1);
    mb->b_next = NULL;
    mb->b_size = size;

    if (temp > (size + sizeof(struct block))) {
    	NALLOC_DEBUG('c');
	b = a->a_ffirst = ((char *)(mb+1)) + size;
	b->b_next = NULL;
	b->b_size = temp - size - sizeof(struct block);
    }
    else {
	a->a_ffirst = NULL;
    }
    return (void *)(++mb);
#endif
}

void
nfree(start)
void *start;
{
    struct arena *a, **qa;
    struct block *b;
    struct block *pb;
    struct block *fb = (struct block *)start;

    NALLOC_DEBUG('F');

    /* set fb (and b) to header start */
    b = --fb;

    if (fb->b_next != (struct block *)NKMAGIC) {
	FATAL("nfree: block %lx not allocated by nalloc!",fb);
    }

    /* the arena this block lives in */
    for (a = *(qa = &a_first); a; a = *(qa = &a->a_next)) {
	if ((unsigned long)b >= (unsigned long)(a+1) &&
	    (unsigned long)b < (((unsigned long)(a+1)) + a->a_size)) goto found;
    }
    FATAL("nfree: block %lx not in any arena!",fb);

found:
    /* Found it! */
    /* a is this block's arena */

    /* set pb to the previous free block in this arena, b to next */
    for (pb = NULL, b = a->a_ffirst;
	 b && (b < fb);
	 pb = b, b = b->b_next) ;

    fb->b_next = b;

    /* Coalesce backwards: if any prev ... */
    if (pb) {
	/* if it's adjacent ... */
	if ((((unsigned long)(pb+1)) + pb->b_size) == (unsigned long)fb) {
	    NALLOC_DEBUG('b');
	    pb->b_size += sizeof(struct block) + fb->b_size;
	    fb = pb;
	}
	else {
	    /* ... else not adjacent, but there is a prev free block */
	    /* so set its next ptr to fb */
	    pb->b_next = fb;
	}
    }
    else {
	/* ... else no prev free block: set arena's free list ptr to fb */
	a->a_ffirst = fb;
    }

    /* Coalesce forwards: b holds start of free block AFTER fb, if any */
    if (b && (((unsigned long)(fb+1)) + fb->b_size) == (unsigned long)b) {
	NALLOC_DEBUG('f');
	fb->b_size += sizeof(struct block) + b->b_size;
	fb->b_next = b->b_next;
    }

    /* if, after coalescing, this arena is entirely free, Mfree it! */
    if ((struct arena *)a->a_ffirst == a+1 && 
        (a->a_ffirst->b_size + sizeof(struct block))== a->a_size &&
        a != a_first) {
	    NALLOC_DEBUG('!');
	    *qa = a->a_next;
#if 1
	    kfree(a);	/* MiNT -- give back so it can be used by users */
#else
	    (void)Mfree(a);
#endif
    }

    return;
}

void
NALLOC_DUMP()
{
    struct arena *a;
    struct block *b;

    for (a = a_first; a; a = a->a_next) {
	FORCE("Arena at %lx size %lx: free list:",a,a->a_size);
	for (b = a->a_ffirst; b; b = b->b_next) {
	    FORCE("    %8lx size %8lx",b,b->b_size);
	}
    }
}
