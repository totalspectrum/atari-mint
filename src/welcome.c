/* welcome.c - MiNT welcome message
 * Copyright 1992,1993,1994 Atari Corp.  All Rights Reserved.
 *=======================================================================
 * 920625 kbad
 */
#include "mint.h"
#include "version.h"

const char *memprot_notice = "\
You have used -m to turn off memory\r\n\
protection.  On a 68000, you don't\r\n\
need to do this because MiNT will\r\n\
do it for you automagically.\r\n";

const char *memprot_warning = "\033p\
            *** WARNING ***            \033q\r\n\
You have turned off memory protection.\r\n\
This is not recommended, and may not be\r\n\
supported in the future.\r\n";

const char *insuff_mem_warning = "\033p\
            *** WARNING ***            \033q\r\n\
Your system's memory is not large enough\r\n\
to permit memory protection to be enabled.\r\n";

const char *greet1 = "\r\n\033p\033f\
 MiNT is Now TOS (" __DATE__ ")         \033q\r\n\
 MiNT v"; /*x.xx prelim version PL xx*/

#ifdef MULTITOS
#define MINT_NAME	"MultiTOS"
#else
#define MINT_NAME	"MiNT"
#endif

const char *greet2 = "\r\n\
 \xbd 1990,1991,1992 Eric R. Smith\r\n\
 MultiTOS kernel\r\n\
 \xbd 1992,1993,1994 Atari Corporation\r\n\
 All Rights Reserved.\r\n\033p\
 Use this program at your own risk!    \033q\r\n\r\n";

/*
 * "boot MiNT?" messages, in various langauges:
 */


struct yn_message {
	const char *message;	/* message to print */
	char	yes_let;	/* letter to hit for yes */
	char	no_let;		/* letter to hit for no */
} boot_it[MAXLANG] = {
{ "Load " MINT_NAME "?   (y)es (n)o ", 'y', 'n' },
{ MINT_NAME " laden?   (j)a (n)ein ", 'j', 'n' },
{ "Charger " MINT_NAME "?   (o)ui (n)on ", 'o', 'n' },
{ "Load " MINT_NAME "?   (y)es (n)o ", 'y', 'n' },		/* reserved */
{ "¨Cargar " MINT_NAME "?   (s)i (n)o ", 's', 'n' },	/* upside down ? is 168 dec. */
{ "Carica " MINT_NAME "?   (s)i (n)o ", 's', 'n' }
};


/*
 * ask the user whether s/he wants to boot MultiTOS; returns 1 if
 * yes, 0 if no
 */

int
boot_kernel_p()
{
	extern int gl_lang;
	struct yn_message *msg;
	int y;

	msg = &boot_it[gl_lang];
	Cconws(msg->message);
	y = (int) Cconin();
	if (tolower(y) == msg->yes_let)
		return 1;
	else
		return 0;
}

