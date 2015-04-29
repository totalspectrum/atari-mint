/*
 * C declarations for functions defined in .s files
 */

/* context.s */
long ARGS_ON_STACK build_context P_((CONTEXT *sav, int fmt));
long ARGS_ON_STACK save_context P_((CONTEXT *sav));
void ARGS_ON_STACK restore_context P_((CONTEXT *sav));
void ARGS_ON_STACK change_context P_((CONTEXT *sav));

/* cpu.s */
void ARGS_ON_STACK set_mmu P_((crp_reg, tc_reg));
void ARGS_ON_STACK save_mmu P_((void));
void ARGS_ON_STACK restr_mmu P_((void));
void ARGS_ON_STACK cpush P_((const void *base, long size));
void ARGS_ON_STACK setstack P_((long));
void ARGS_ON_STACK flush_pmmu P_((void));

/* intr.s */
void ARGS_ON_STACK reboot P_((void));
short ARGS_ON_STACK spl7 P_((void));
void ARGS_ON_STACK spl P_((short));
long ARGS_ON_STACK new_rwabs();
long ARGS_ON_STACK new_mediach();
long ARGS_ON_STACK new_getbpb();

/* quickzer.s */
void ARGS_ON_STACK quickzero P_((char *place, long size));

/* quickmov.s */
void ARGS_ON_STACK quickmove P_((void *dst, void *src, long nbytes));
void ARGS_ON_STACK quickmovb P_((void *dst, const void *src, long nbytes));

/* syscall.s */
char * ARGS_ON_STACK lineA0 P_((void));
void ARGS_ON_STACK call_aes P_((short **));
long ARGS_ON_STACK call_dosound P_((const void *));
long ARGS_ON_STACK callout P_((long, ...));
long ARGS_ON_STACK callout1 P_((long, int));
long ARGS_ON_STACK callout2 P_((long, int, int));
long ARGS_ON_STACK callout6 P_((long, int, int, int, int, int, int));
long ARGS_ON_STACK callout6spl7 P_((long, int, int, int, int, int, int));
void ARGS_ON_STACK do_usrcall P_((void));

