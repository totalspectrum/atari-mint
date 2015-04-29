typedef struct xbra
{
	long		xbra_magic;
	long		xbra_id;
	struct xbra	*next;
	short		jump;
	long ARGS_ON_STACK (*this)();
} xbra_vec;

#define XBRA_MAGIC	0x58425241L /* "XBRA" */
#define MINT_MAGIC	0x4d694e54L /* "MiNT" */
#define JMP_OPCODE	0x4EF9
