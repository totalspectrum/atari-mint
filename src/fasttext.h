#if 0
#include <osbind.h>
#include <linea.h>
#endif

#define ALT_1 0x780000L
#define ALT_2 0x790000L
#define ALT_0 0x810000L

typedef struct screen {
	short	hidecnt;	/* cursor hide count */
	short	mcurx, mcury;	/* current mouse X, Y position */
	char	mdraw;
	char	mouseflag;
	long	junk1;
	short	savex, savey;	/* saved X, Y position */
	short	msavelen;	/* mouse save stuff */
	long	msaveaddr;
	short	msavestat;
	long	msavearea[64];
	long	user_tim, next_tim; /* time vector stuff */
	long	user_but, user_cur,
		user_mot;	/* more user vectors */
	short	cheight;	/* character height */
	short	maxx;		/* number of characters across - 1 */
	short	maxy;		/* number of characters high - 1 */
	short	linelen;	/* length (in bytes) of a line of characters */	
	short	bgcol;		/* background color */
	short	fgcol;		/* foreground color */
	char	*cursaddr;	/* cursor address */
	short	v_cur_of;	/* ??? */
	short	cx, cy;		/* current (x,y) position of cursor */
	char	period;		/* cursor flash period (in frames) */	
	char	curstimer;	/* cursor flash timer */
	char	*fontdata;	/* pointer to font data */
	short	firstcode;	/* first ASCII code in font */
	short	lastcode;	/* last ASCII code in font */
	short	form_width;	/* # bytes/scanline in font data */
	short	xpixel;
	char	*fontoff;	/* pointer to font offset table */
	char	flags;		/* e.g. cursor on/off */
	char	reserved;
	short	ypixel;
	short	width;		/* length of a screen scan line */
	short	planes;		/* number of planes on screen */
	short	planesiz;	/* length of a screen scan line */
} SCREEN;

#define SCNSIZE(v) ( (((long)v->maxy + hardscroll + 2)) * v->linelen )

/* possible flags for cursor state, etc. */
#define CURS_FLASH	0x01		/* cursor flashing */
#define CURS_FSTATE	0x02		/* cursor in flash state */
#define CURS_ON		0x04		/* cursor on */
#define FWRAP		0x08		/* wrap cursor at end of line */
#define FINVERSE	0x10		/* invert text */
#define CURS_UPD	0x40		/* cursor update flag */
