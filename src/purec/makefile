#
# Makefile for MiNT using the Pure CC
#

# if you're cross-compiling, define NATIVECC
# to the host's C compiler, NATIVECFLAGS to
# the appropriate flags for it, and NATIVELIBS
# to appropriate libraries
# NATIVECC = cc -DNO_STDLIB
# NATIVECFLAGS = -g
# NATIVELIBS =

NATIVECC = gcc
NATIVECFLAGS = -O
NATIVELIBS = -liio

#
# depending on your version of bison/yacc, you may
# need to change these names, e.g. to
# YACC = yacc
# YTABC = y.tab.c
# YTABH = y.tab.h

YACC = bison -d
YTABC = asm_tab.c
YTABH = asm_tab.h

# here are defs for the cross compiler
# MiNT must be compiled with 16 bit integers (pcc alway uses 16bit ints)

CC = cc -g
AS = cc -Wa,-3 -Wa,-8 -Wa,-S -c
MODEL =
MODEL030 = -W0,-2
LIBS = -lpctoslib.lib

# add -DMULTITOS for a MultiTOS kernel
# add -DONLY030 for a version of MiNT that needs a 680x0, x>=3

DEFS = -DMULTITOS
#DEFS030 = -DMULTITOS -DONLY030
DEFS030 = -DONLY030

CFLAGS =  -D__STDC__ -W0,-P $(MODEL) $(DEFS)
CFLAGS030 =  -D__STDC__ -W0,-P $(MODEL030) $(DEFS)
ASFLAGS =
ASFLAGS030 =

COBJS = bios.o xbios.o console.o dos.o dosdir.o dosfile.o dosmem.o dossig.o \
	filesys.o main.o mem.o proc.o signal.o timeout.o tty.o util.o \
	biosfs.o pipefs.o procfs.o tosfs.o debug.o rendez.o \
	unifs.o shmfs.o fasttext.o welcome.o nalloc2.o memprot.o realloc.o

COBJS030 = bios.o0 xbios.o0 console.o0 dos.o0 dosdir.o0 dosfile.o0 dosmem.o0 dossig.o0 \
	filesys.o0 main.o0 mem.o0 proc.o0 signal.o0 timeout.o0 tty.o0 util.o0 \
	biosfs.o0 pipefs.o0 procfs.o0 tosfs.o0 debug.o0 rendez.o0 \
	unifs.o0 shmfs.o0 fasttext.o0 welcome.o0 nalloc2.o0 memprot.o realloc.o0

CSRCS = bios.c xbios.c console.c dos.c dosdir.c dosfile.c dosmem.c dossig.c \
	filesys.c main.c mem.c proc.c signal.c timeout.c tty.c util.c \
	biosfs.c pipefs.c procfs.c tosfs.c debug.c rendez.c \
	unifs.c shmfs.c fasttext.c welcome.c nalloc2.c memprot.c realloc.c

SOBJS = context.o intr.o syscall.o quickzer.o quickmov.o cpu.o
SOBJS030 = context.o0 intr.o0 syscall.o0 quickzer.o0 quickmov.o0 cpu.o0

OBJS = $(COBJS) $(SOBJS)
OBJS030 = $(COBJS030) $(SOBJS030)

all: mint.prg mint030.prg

mint.prg: $(OBJS)
	$(CC) $(CFLAGS) -o mint.prg $(OBJS) $(LIBS)
	prgflags 7 7 mint.prg
	strip mint.prg

mint030.prg: $(OBJS030)
	$(CC) $(CFLAGS030) -o mint030.prg $(OBJS030) $(LIBS)
	prgflags 7 7 mint030.prg
	strip mint030.prg

$(SOBJS): proc.h
$(COBJS): mint.h proc.h file.h

$(SOBJS030): proc.h
$(COBJS030): mint.h proc.h file.h

main.o: version.h
welcome.o: version.h
bios.o: inline.h

memprot.o: memprot.c mint.h proc.h file.h
	$(CC) $(CFLAGS) -W0,-2 -c memprot.c

main.o0: version.h
welcome.o0: version.h
bios.o0: inline.h

#
# assembler source files are now handled in a radically different
# fashion. We build a pre-processor program, asmtrans, that
# takes the .spp files, merges them with an include file that
# gives various offsets into structures of interest, and produces
# the .s files as output. This has two major advantages:
# (1) it lets us use the same source for both the Lattice and
#     gcc assembler files (the translator will convert), and
# (2) if we change the CONTEXT or PROC structures, we don't
#     have to dig through the source code looking for
#     magic numbers

# the asm translator program
# Note that this must be compiled with the native CC of whatever
# system you're using; see the definitions at the top of this
# file.

ATRANSOBJ = asmtab.o trutil.o trans.o
ATRANS = asmtrans.ttp

#$(ATRANS): $(ATRANSOBJ)
#	$(NATIVECC) $(NATIVECFLAGS) -o $(ATRANS) $(ATRANSOBJ) \
#		$(NATIVELIBS)

#asmtab.o: asmtab.c asmtrans.h
#	$(NATIVECC) $(NATIVECFLAGS) -o $@ -c asmtab.c

#trutil.o: trutil.c asmtrans.h
#	$(NATIVECC) $(NATIVECFLAGS) -o $@ -c trutil.c

#trans.o: trans.c asmtrans.h
#	$(NATIVECC) $(NATIVECFLAGS) -o $@ -c trans.c


#asmtab.c asmtab.h:	asm.y
#	$(YACC) asm.y
#	mv $(YTABC) asmtab.c
#	mv $(YTABH) asmtab.h

# the magic number include file is generated automagically
# NOTE that of course, magic.i can only reliably be generated
# on an Atari ST; if you're cross-compiling, you'll have
# to edit magic.i by hand
# for cross compilers; uncomment the following definitions
# and comment out the other ones below

# for cross-compiling

# GENMAGICPRG=echo
# magic.i: proc.h file.h genmagic.c
#	echo "Warning: magic.i may be out of date"

#for native compiling

GENMAGICPRG=genmagic.ttp

$(GENMAGICPRG): genmagic.c proc.h file.h
	$(CC) $(CFLAGS) -DGENMAGIC -o $(GENMAGICPRG) genmagic.c

magic.i: $(GENMAGICPRG)
	$(GENMAGICPRG) $@

#
# assembler source files
#
.SUFFIXES: .spp .o0

.spp.o:
	$(ATRANS) $(DEFS) -purec -o $*.s $<
	$(AS) $(ASFLAGS) -o $@ -c $*.s
	$(RM) $*.s
.spp.o0:
	$(ATRANS) $(DEFS030) -purec -o $*.s $<
	$(AS) $(ASFLAGS0) -o $@ -c $*.s
	$(RM) $*.s
.c.o0:
	$(CC) $(CFLAGS030) -o $@ -c $<

context.o: context.spp magic.i $(ATRANS)
intr.o: intr.spp magic.i $(ATRANS)
syscall.o: syscall.spp magic.i $(ATRANS)
quickzer.o: quickzer.spp $(ATRANS)
quickmov.o: quickmov.spp $(ATRANS)
cpu.o: cpu.spp $(ATRANS)

context.o0: context.spp magic.i $(ATRANS)
intr.o0: intr.spp magic.i $(ATRANS)
syscall.o0: syscall.spp magic.i $(ATRANS)
quickzer.o0: quickzer.spp $(ATRANS)
quickmov.o0: quickmov.spp $(ATRANS)
cpu.o0: cpu.spp $(ATRANS)

#
# mkptypes generates prototypes from C source code. If you don't have it,
# you'll have to add/delete function prototypes by hand.
# also: Sozobon users will have to edit proto.h by hand to change the
#    #if defined(__STDC__) || defined(__cplusplus)
# line into
#    #if __STDC__
#
proto.h: $(CSRCS)
	mkptypes $(CSRCS) >proto.h

#
# macros for cleaning up
#
GENFILES= $(OBJS) $(OBJS030) $(ATRANSOBJ) $(ATRANS) $(GENMAGICPRG)
EXTRAS= asmtab.c asmtab.h mint.prg

clean:
	$(RM) $(GENFILES)

realclean:
	$(RM) $(GENFILES) $(EXTRAS)
