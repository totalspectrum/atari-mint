#include "mint.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

static void usage P_((void));
int main(int argc, char **argv);

static void
usage()
{
	fprintf(stderr, "Usage: genmagic outputfile\n");
	exit(2);
}

/* conventions:
 * C_XXX is offset of XXX in CONTEXT struct
 * P_XXX is offset of XXX in PROC struct
 */

struct magics {
	const char *name;
	long value;
} magics[] = {
	{ "C_PTRACE", offsetof(CONTEXT, ptrace)},
	{ "C_SFMT", offsetof(CONTEXT, sfmt)},
	{ "C_INTERNAL", offsetof(CONTEXT, internal)},
	{ "C_SR", offsetof(CONTEXT, sr)},
	{ "C_PC", offsetof(CONTEXT, pc)},
	{ "C_FSTATE", offsetof(CONTEXT, fstate)},
	{ "C_FREGS", offsetof(CONTEXT, fregs)},
	{ "C_FCTRL", offsetof(CONTEXT, fctrl)},
	{ "C_USP", offsetof(CONTEXT, usp)},
	{ "C_SSP", offsetof(CONTEXT, ssp)},
	{ "C_TERM", offsetof(CONTEXT, term_vec)},
	{ "C_D0", offsetof(CONTEXT, regs)},
	{ "C_A0", offsetof(CONTEXT, regs)+32},
	{ "C_CRP", offsetof(CONTEXT, crp)},
	{ "C_TC", offsetof(CONTEXT, tc)},
	{ "P_CTXT0", offsetof(PROC, ctxt)},
	{ "P_SYSTIME", offsetof(PROC, systime)},
	{ "P_USRTIME", offsetof(PROC, usrtime)},
	{ "P_PTRACER", offsetof(PROC, ptracer)},
	{ "P_SYSCTXT", offsetof(PROC, ctxt)},
	{ "P_EXCPC", offsetof(PROC, exception_pc)},
	{ "P_EXCSSP", offsetof(PROC, exception_ssp)},
	{ "P_EXCADDR", offsetof(PROC, exception_addr)},
	{ "P_EXCTBL", offsetof(PROC, exception_tbl)},
	{ "P_EXCMMUSR", offsetof(PROC, exception_mmusr)},
	{ (char *)0, 0 }
};

int
main(argc, argv)
	int argc;
	char **argv;
{
	FILE *f;
	int i;

	if (argc != 2)
		usage();
	f = fopen(argv[1], "w");
	if (!f) {
		perror(argv[1]);
		exit(1);
	}

	for (i = 0; magics[i].name; i++) {
		fprintf(f, "%%define %s %ld\n", magics[i].name,
				magics[i].value);
	}
	fclose(f);
	return 0;
}
