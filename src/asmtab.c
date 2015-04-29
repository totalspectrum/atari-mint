#ifndef lint
char yysccsid[] = "@(#)yaccpar	1.3 (Berkeley) 01/21/90";
#endif
#define WORD 257
#define WHITESP 258
#define EOLN 259
#define STRING 260
#define DEFINECMD 261
#define INCLUDECMD 262
#define IFDEFCMD 263
#define IFNDEFCMD 264
#define ELSECMD 265
#define ENDIFCMD 266
#define YYERRCODE 256
#line 3 "asm.y"
#define YYSTYPE char *

#include "asmtrans.h"
#line 19 "y_tab.c"
short yylhs[] = {                                        -1,
    0,    0,    1,    1,    1,    1,    1,    1,    1,    1,
    1,    1,    1,    1,    3,    3,    3,    3,    6,    6,
    5,    2,    4,    4,    4,    4,    4,    4,    4,    4,
    4,    4,    4,    4,    4,    7,    7,    7,    8,    8,
    9,    9,    9,    9,
};
short yylen[] = {                                         2,
    0,    2,    1,    2,    2,    3,    4,    4,    6,    6,
    4,    4,    2,    2,    2,    4,    3,    5,    1,    3,
    1,    2,    1,    2,    3,    4,    4,    4,    4,    6,
    6,   11,   11,    5,    7,    1,    3,    2,    1,    2,
    1,    1,    1,    1,
};
short yydefred[] = {                                      1,
    0,    0,    0,    3,    0,    0,    0,    0,    0,    0,
    2,    0,    0,    0,   22,   21,    0,    0,    0,    0,
    0,   13,   14,    0,    4,    0,    5,    0,    0,    0,
    0,    0,    0,    0,    6,    0,   39,    0,    0,    0,
    0,    0,   16,    0,    0,    0,    8,    7,   11,   12,
   18,    0,   24,    0,    0,    0,   38,   40,    0,    0,
    0,   41,   42,   43,   44,    0,    0,    0,    0,    0,
    0,   20,    0,    0,   37,    9,   10,    0,    0,   29,
   26,   27,    0,   28,    0,    0,    0,   34,    0,    0,
    0,    0,    0,   30,   31,    0,    0,   35,    0,    0,
    0,    0,    0,    0,   33,   32,
};
short yydgoto[] = {                                       1,
   11,   12,   13,   42,   17,   43,   44,   45,   66,
};
short yysindex[] = {                                      0,
 -168,  -47, -235,    0, -226, -224, -217, -215, -210, -202,
    0, -203, -201, -235,    0,    0, -214, -198, -221, -197,
 -195,    0,    0, -193,    0, -196,    0, -192,  -33, -191,
 -189, -185, -184, -180,    0,  -33,    0,  -28,  -30,  -27,
 -177,   24,    0,  -24,  -12,  -35,    0,    0,    0,    0,
    0,  -28,    0,  -28,   40,  -28,    0,    0,  -33,  -28,
  -28,    0,    0,    0,    0,  -28, -175, -174,  -17,  -29,
   41,    0,    4,   28,    0,    0,    0,  -28,    9,    0,
    0,    0,  -28,    0,  -28,  -15,  -28,    0,   51,  -25,
  -28,   57,   62,    0,    0,   12,  -28,    0,   63,   64,
  -28,  -28,   65,   68,    0,    0,
};
short yyrindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0, -149,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0, -148,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0, -147,    0,  -40,  -21,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,  -23,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,
};
short yygindex[] = {                                      0,
    0,    0,  101,   69,  100,   -8,  -14,    0,    0,
};
#define YYTABLESIZE 238
short yytable[] = {                                      38,
   41,   38,   41,   23,   39,   41,   39,   41,   41,   40,
   15,   40,   56,   81,   52,   60,   52,   52,   36,   36,
   25,   16,   36,   53,   55,   57,   78,   51,   91,   64,
   62,   18,   63,   19,   65,   31,   36,   57,   32,   69,
   20,   71,   21,   29,   84,   73,   74,   83,   22,   88,
   72,   75,   87,   24,    3,   25,   23,   27,   30,   33,
   54,   34,   35,   86,   14,   36,   46,   59,   89,   47,
   90,   36,   93,   48,   49,   79,   96,   92,   50,   58,
   70,   82,  100,   76,   77,   85,  103,  104,    2,    3,
    4,   94,    5,    6,    7,    8,    9,   10,   61,   95,
   97,   36,   98,   36,   99,  105,  101,  102,  106,   15,
   17,   19,   26,   28,   68,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
   14,    0,    0,    0,    0,    0,    0,    0,   23,    0,
    0,   37,    0,   37,   67,    0,   37,   80,   37,   37,
    0,    0,    0,    0,    0,   25,    0,   36,
};
short yycheck[] = {                                      35,
   36,   35,   36,   44,   40,   36,   40,   36,   36,   45,
   58,   45,   40,   43,   45,   40,   45,   45,   40,   41,
   44,  257,   44,   38,   39,   40,   44,   36,   44,   42,
   43,  258,   45,  258,   47,  257,   58,   52,  260,   54,
  258,   56,  258,  258,   41,   60,   61,   44,  259,   41,
   59,   66,   44,  257,  258,  259,  259,  259,  257,  257,
   91,  257,  259,   78,  258,  258,  258,   44,   83,  259,
   85,   93,   87,  259,  259,   93,   91,   93,  259,  257,
   41,   41,   97,  259,  259,   58,  101,  102,  257,  258,
  259,   41,  261,  262,  263,  264,  265,  266,  123,  125,
   44,  123,   41,  125,   93,   41,   44,   44,   41,  259,
  259,  259,   12,   14,   46,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
  258,   -1,   -1,   -1,   -1,   -1,   -1,   -1,  259,   -1,
   -1,  257,   -1,  257,  260,   -1,  257,  257,  257,  257,
   -1,   -1,   -1,   -1,   -1,  259,   -1,  259,
};
#define YYFINAL 1
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 266
#if YYDEBUG
char *yyname[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,"'#'","'$'",0,0,0,"'('","')'","'*'","'+'","','","'-'",0,"'/'",0,0,0,0,0,0,0,0,
0,0,"':'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'['",
0,"']'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"'{'",0,"'}'",
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,"WORD","WHITESP","EOLN","STRING","DEFINECMD","INCLUDECMD",
"IFDEFCMD","IFNDEFCMD","ELSECMD","ENDIFCMD",
};
char *yyrule[] = {
"$accept : input",
"input :",
"input : input line",
"line : EOLN",
"line : label EOLN",
"line : opline EOLN",
"line : label opline EOLN",
"line : INCLUDECMD WHITESP STRING EOLN",
"line : INCLUDECMD WHITESP WORD EOLN",
"line : DEFINECMD WHITESP WORD WHITESP STRING EOLN",
"line : DEFINECMD WHITESP WORD WHITESP operand EOLN",
"line : IFDEFCMD WHITESP WORD EOLN",
"line : IFNDEFCMD WHITESP WORD EOLN",
"line : ELSECMD EOLN",
"line : ENDIFCMD EOLN",
"opline : WHITESP opcode",
"opline : WHITESP opcode WHITESP ops",
"opline : WORD WHITESP opcode",
"opline : WORD WHITESP opcode WHITESP ops",
"ops : operand",
"ops : operand ',' ops",
"opcode : WORD",
"label : WORD ':'",
"operand : basic",
"operand : '#' basic",
"operand : '(' basic ')'",
"operand : '(' basic ')' '+'",
"operand : '-' '(' basic ')'",
"operand : basic '(' basic ')'",
"operand : '(' basic ')' WORD",
"operand : basic '(' basic ',' basic ')'",
"operand : basic '{' basic ':' basic '}'",
"operand : '(' '[' basic ',' basic ']' ',' basic ',' basic ')'",
"operand : '(' '[' basic ',' basic ',' basic ']' ',' basic ')'",
"operand : '(' '[' basic ']' ')'",
"operand : '(' '[' basic ']' ',' basic ')'",
"basic : basexpr",
"basic : basexpr op basic",
"basic : '-' basic",
"basexpr : WORD",
"basexpr : '$' WORD",
"op : '+'",
"op : '-'",
"op : '*'",
"op : '/'",
};
#endif
#ifndef YYSTYPE
typedef int YYSTYPE;
#endif
#define yyclearin (yychar=(-1))
#define yyerrok (yyerrflag=0)
#ifndef YYSTACKSIZE
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 300
#endif
#endif
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
#define yystacksize YYSTACKSIZE
short yyss[YYSTACKSIZE];
YYSTYPE yyvs[YYSTACKSIZE];
#line 103 "asm.y"
#include <setjmp.h>

jmp_buf start;

#ifdef NATIVEATARI
#define STACK 32*1024L
#ifdef LATTICE
long _STACK = STACK;
#endif
#ifdef __GNUC__
long _stksize = STACK;
#endif

static void
hit_return()
{
	printf("Hit return to continue\n");
	fflush(stdout);
	getchar();
}
#endif

void usage()
{
	fprintf(stderr, "Usage: asmtrans [-gas][-asm][-o outfile] infile\n");
	exit(2);
}

int errors = 0;

void
do_include(file)
	char *file;
{
	jmp_buf save;
	FILE *oldin, *f;

	f = fopen(file, "rt");
	if (!f) {
		perror(file);
		return;
	}
	bcopy(start, save, sizeof(jmp_buf));
	oldin = infile;
	infile = f;
	setjmp(start);
	yyparse();
	fclose(f);
	infile = oldin;
	bcopy(save, start, sizeof(jmp_buf));
	longjmp(start,1);
}

/* set up initial definitions based on syntax type */

void
do_initial_defs()
{
	if (syntax == GAS) {
		do_define("mmusr", "psr");
		do_define("fpiar", "fpi");
		do_define("XREF", ".globl");
		do_define("XDEF", ".globl");
		do_define("TEXT", ".text");
		do_define("DATA", ".data");
	/* gas doesn't have a .bss directive */
		do_define("BSS", ".data");
		do_define("END", "| END");
		do_define("dc.l", ".long");
		do_define("dc.w", ".word");
		do_define("dc.b", ".byte");
	} else if (syntax == ASM) {
		do_define("TEXT", "SECTION TEXT,CODE");
		do_define("DATA", "SECTION DATA,DATA");
		do_define("BSS", "SECTION BSS,BSS");
	}
}

int
main (argc, argv)
	int argc; char **argv;
{
	FILE *f;
#ifdef NATIVEATARI
	if (!argv[0] || !argv[0][0])	/* run from desktop? */
		atexit(hit_return);
#endif
	argv++;
	outfile = stdout;

	while (*argv) {
		if (!strcmp(*argv, "-o")) {
			argv++;
			if (*argv == 0) {
				fprintf(stderr, "missing argument to -o\n");
				usage();
			}
			f = fopen(*argv, "wt");
			if (!f)
				perror(*argv);
			else
				outfile = f;
			argv++;
		} else if (!strcmp(*argv, "-gas")) {
			argv++;
			syntax = GAS;
		} else if (!strcmp(*argv, "-asm")) {
			argv++;
			syntax = ASM;
		} else if (!strcmp(*argv, "-purec")) {
			argv++;
			syntax = PUREC;
		} else if (!strncmp(*argv, "-D", 2)) {
			char *word, *defn;
			word = *argv+2;
			defn = index(word,'=');
			if (defn)
				*defn++ = '\0';
			else
				defn = "1";
			if (*word) do_define(word,defn);
			argv++;
		} else if (!strcmp(*argv, "--")) {
			argv++;
			break;
		} else {
			if (**argv == '-') {
				fprintf(stderr, "unknown option: %s\n",
					*argv);
				usage();
			}
			break;
		}
	}

	do_initial_defs();

	if (*argv == 0) {
		setjmp(start);
		infile = stdin;
		yyparse();
	} else {
	    while(*argv) {
		if (!(f = fopen(*argv, "rt")))
			perror(*argv);
		else {
			infile = f;
			setjmp(start);
			yyparse();
			fclose(f);
		}
		argv++;
	    }
	}

	if (ifstkptr != 0) {
		fputs("%ifdef without matching %endif\n", stderr);
		errors++;
	}
	return errors;
}

void
yyerror (s)  /* Called by yyparse on error */
     char *s;
{
	errors++;
	printf("%s\n", s);
	longjmp(start, 1);
}

void dbgmsg(s) char *s;
{
	fprintf(stderr, "%s\n", s);
}
#line 396 "y_tab.c"
#define YYABORT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;
    extern char *getenv();

    if (yys = getenv("YYDEBUG"))
    {
        yyn = *yys;
        if (yyn == '0')
            yydebug = 0;
        else if (yyn >= '1' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if (yyn = yydefred[yystate]) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("yydebug: state %d, reading %d (%s)\n", yystate,
                    yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("yydebug: state %d, shifting to state %d\n",
                    yystate, yytable[yyn]);
#endif
        if (yyssp >= yyss + yystacksize - 1)
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#ifdef lint
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("yydebug: state %d, error recovery shifting\
 to state %d\n", *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yyss + yystacksize - 1)
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("yydebug: error recovery discarding state %d\n",
                            *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("yydebug: state %d, error recovery discards token %d (%s)\n",
                    yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("yydebug: state %d, reducing by rule %d (%s)\n",
                yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 3:
#line 25 "asm.y"
{ emit(yyvsp[0]); }
break;
case 4:
#line 28 "asm.y"
{ emit(yyvsp[-1]); emit(yyvsp[0]); }
break;
case 5:
#line 29 "asm.y"
{ emit(yyvsp[-1]); emit(yyvsp[0]); }
break;
case 6:
#line 30 "asm.y"
{ emit(yyvsp[-2]); emit(yyvsp[-1]);
			emit(yyvsp[0]); }
break;
case 7:
#line 32 "asm.y"
{ if (!hidecnt) do_include(yyvsp[-1]); free(yyvsp[-1]); }
break;
case 8:
#line 33 "asm.y"
{ if (!hidecnt) do_include(yyvsp[-1]); free(yyvsp[-1]); }
break;
case 9:
#line 34 "asm.y"
{
		if (!hidecnt) do_define(yyvsp[-3], yyvsp[-1]); free(yyvsp[-3]); free(yyvsp[-1]); }
break;
case 10:
#line 36 "asm.y"
{
		if (!hidecnt) do_define(yyvsp[-3], yyvsp[-1]); free(yyvsp[-3]); free(yyvsp[-1]); }
break;
case 11:
#line 38 "asm.y"
{ do_ifdef(yyvsp[-1]); free(yyvsp[-1]); }
break;
case 12:
#line 39 "asm.y"
{ do_ifndef(yyvsp[-1]); free(yyvsp[-1]); }
break;
case 13:
#line 40 "asm.y"
{ do_else(); }
break;
case 14:
#line 41 "asm.y"
{ do_endif(); }
break;
case 15:
#line 44 "asm.y"
{ yyval  = do_ops("", yyvsp[0], "", ""); free(yyvsp[0]); }
break;
case 16:
#line 46 "asm.y"
{ yyval  = do_ops("", yyvsp[-2], yyvsp[-1], yyvsp[0]);
			 free(yyvsp[-2]); free(yyvsp[-1]); free(yyvsp[0]); }
break;
case 17:
#line 48 "asm.y"
{ yyval  = do_ops(yyvsp[-2], yyvsp[0], "", ""); free(yyvsp[-2]); free(yyvsp[0]); }
break;
case 18:
#line 50 "asm.y"
{ yyval  = do_ops(yyvsp[-4], yyvsp[-2], yyvsp[-1], yyvsp[0]);
			 free(yyvsp[-4]); free(yyvsp[-2]); free(yyvsp[-1]); free(yyvsp[0]);}
break;
case 19:
#line 54 "asm.y"
{ yyval  = yyvsp[0]; }
break;
case 20:
#line 55 "asm.y"
{ yyval  = concat3(yyvsp[-2], ",", yyvsp[0]);
				free(yyvsp[-2]); free(yyvsp[0]); }
break;
case 21:
#line 59 "asm.y"
{ yyval  = wordlookup(yyvsp[0]); free(yyvsp[0]); }
break;
case 22:
#line 62 "asm.y"
{ yyval  = concat(yyvsp[-1], ":"); free(yyvsp[-1]); }
break;
case 23:
#line 64 "asm.y"
{yyval  = yyvsp[0]; }
break;
case 24:
#line 65 "asm.y"
{yyval  = immediate(yyvsp[0]); free(yyvsp[0]); }
break;
case 25:
#line 66 "asm.y"
{yyval  = indirect(yyvsp[-1]); free(yyvsp[-1]); }
break;
case 26:
#line 67 "asm.y"
{yyval  = postinc(yyvsp[-2]); free(yyvsp[-2]); }
break;
case 27:
#line 68 "asm.y"
{yyval  = predec(yyvsp[-1]); free(yyvsp[-1]); }
break;
case 28:
#line 69 "asm.y"
{yyval  = indexed(yyvsp[-3], yyvsp[-1]); free(yyvsp[-3]); free(yyvsp[-1]); }
break;
case 29:
#line 70 "asm.y"
{yyval  = sizedop(yyvsp[-2], yyvsp[0]); free(yyvsp[-2]); free(yyvsp[0]); }
break;
case 30:
#line 71 "asm.y"
{yyval  = twoindex(yyvsp[-5], yyvsp[-3], yyvsp[-1]);
				free(yyvsp[-5]); free(yyvsp[-3]); free(yyvsp[-1]); }
break;
case 31:
#line 73 "asm.y"
{yyval  = bitfield(yyvsp[-5], yyvsp[-3], yyvsp[-1]);
			free(yyvsp[-5]); free(yyvsp[-3]); free(yyvsp[-1]); }
break;
case 32:
#line 76 "asm.y"
{ yyval =postindex(yyvsp[-8],yyvsp[-6],yyvsp[-3],yyvsp[-1]); 
	     free(yyvsp[-8]); free(yyvsp[-6]); free(yyvsp[-3]); free(yyvsp[-1]); }
break;
case 33:
#line 79 "asm.y"
{ yyval =preindex(yyvsp[-8],yyvsp[-6],yyvsp[-4],yyvsp[-1]); 
	     free(yyvsp[-8]); free(yyvsp[-6]); free(yyvsp[-4]); free(yyvsp[-1]); }
break;
case 34:
#line 82 "asm.y"
{ yyval =postindex0(yyvsp[-2]); 
	     free(yyvsp[-2]); }
break;
case 35:
#line 85 "asm.y"
{ yyval =postindex1(yyvsp[-4],yyvsp[-1]); 
	     free(yyvsp[-4]); free(yyvsp[-1]); }
break;
case 36:
#line 89 "asm.y"
{ yyval  = yyvsp[0]; }
break;
case 37:
#line 90 "asm.y"
{ yyval  = concat3(yyvsp[-2], yyvsp[-1], yyvsp[0]); free(yyvsp[-2]); free(yyvsp[-1]); free(yyvsp[0]); }
break;
case 38:
#line 91 "asm.y"
{ yyval  = concat("-", yyvsp[0]); free(yyvsp[0]); }
break;
case 39:
#line 93 "asm.y"
{yyval  = wordlookup(yyvsp[0]); free(yyvsp[0]); }
break;
case 40:
#line 94 "asm.y"
{yyval  = hexop(yyvsp[0]); free(yyvsp[0]);}
break;
case 41:
#line 97 "asm.y"
{ yyval  = strdup("+"); }
break;
case 42:
#line 98 "asm.y"
{ yyval  = strdup("-"); }
break;
case 43:
#line 99 "asm.y"
{ yyval  = strdup("*"); }
break;
case 44:
#line 100 "asm.y"
{ yyval  = strdup("/"); }
break;
#line 718 "y_tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#ifdef YYDEBUG
        if (yydebug)
            printf("yydebug: after reduction, shifting from state 0 to\
 state %d\n", YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("yydebug: state %d, reading %d (%s)\n",
                        YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#ifdef YYDEBUG
    if (yydebug)
        printf("yydebug: after reduction, shifting from state %d \
to state %d\n", *yyssp, yystate);
#endif
    if (yyssp >= yyss + yystacksize - 1)
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
