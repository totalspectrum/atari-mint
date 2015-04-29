/*
 *	ctype.h		Character classification and conversion
 */

#ifndef _CTYPE_H
#define _CTYPE_H

extern	unsigned char	_ctype[];	/* in lib.c */

#define	_CTc	0x01		/* control character */
#define	_CTd	0x02		/* numeric digit */
#define	_CTu	0x04		/* upper case */
#define	_CTl	0x08		/* lower case */
#define	_CTs	0x10		/* whitespace */
#define	_CTp	0x20		/* punctuation */
#define	_CTx	0x40		/* hexadecimal */

#define	isalnum(c)	(_ctype[(unsigned char)(c)]&(_CTu|_CTl|_CTd))
#define	isalpha(c)	(_ctype[(unsigned char)(c)]&(_CTu|_CTl))
#define	isascii(c)	!((c)&~0x7F)
#define	iscntrl(c)	(_ctype[(unsigned char)(c)]&_CTc)
#define	isdigit(c)	(_ctype[(unsigned char)(c)]&_CTd)
#define	isgraph(c)	(!(_ctype[(unsigned char)(c)]&(_CTc|_CTs)) && (_ctype[(unsigned char)(c)]))
#define	islower(c)	(_ctype[(unsigned char)(c)]&_CTl)
#define isprint(c)      (!(_ctype[(unsigned char)(c)]&_CTc) && (_ctype[(unsigned char)(c)]))
#define	ispunct(c)	(_ctype[(unsigned char)(c)]&_CTp)
#define	isspace(c)	(_ctype[(unsigned char)(c)]&_CTs)
#define	isupper(c)	(_ctype[(unsigned char)(c)]&_CTu)
#define	isxdigit(c)	(_ctype[(unsigned char)(c)]&_CTx)
#define iswhite(c)	isspace(c)

#define	_toupper(c)	((c)^0x20)
#define	_tolower(c)	((c)^0x20)
#define	toascii(c)	((c)&0x7F)

#define toint(c)	( (c) <= '9' ? (c) - '0' : toupper(c) - 'A' )
#define isodigit(c)	( (c)>='0' && (c)<='7' )
#define iscymf(c)	(isalpha(c) || ((c) == '_') )
#define iscym(c)	(isalnum(c) || ((c) == '_') )

#endif /* _CTYPE_H */
