.TH Psigaction 2 "MiNT Programmer's Manual" "Version 1.0" "Feb. 1, 1993"
.SH NAME
Psigaction \- change the way a signal is handled
.SH SYNOPSIS
.nf
#include <signal.h>

LONG Psigaction(WORD sig, struct sigaction *act, struct sigaction *oact);
.fi
.SH DESCRIPTION

.I Psigaction
changes the handling of the signal indicated by
.I sig
(which must be between 1 and 31; symbolic constants for symbols are defined
in the file signal.h).

.PP
If
.I act
is non-zero, then it is assumed to point to a structure describing
the signal handling behavior. This structure has the following
members:
.nf
struct sigaction {
	LONG	sa_handler;
	LONG	sa_mask;
	WORD	sa_flags;
}
.fi
If
.I sa_handler
is SIG_DFL, then the default action for the signal will occur when the
signal is delivered to the process.

.PP
If
.I sa_handler
is SIG_IGN, then the signal will be ignored by the process, and
delivery of the signal will have no noticeable effect (in particular, the
signal will not interrupt the
.I Pause
or
.I Psigpause
system calls, q.v.). If the signal
is pending at the time of the
.I Psignal
call, it is discarded.

.PP
If
.I sa_handler
is some other value, it is assumed to be the address of a
user function that will be called when the signal is delivered to the
process. The user function is called with a single LONG argument on
the stack, which is the number of the signal being delivered (this is done
so that processes may use the same handler for a number of different
signals). While the signal is being handled, it is blocked from delivery;
thus, signal handling is "reliable" (unlike Version 7 and early System V
Unix implementations, in which delivery of a second signal while it
was being handled could kill the process).
The set of signals specified in
.I sa_mask
are also blocked from delivery while the signal handler is executing.
Note that, unlike in some
versions of Unix, the signal handling is not reset to the default action
before the handler is called; it remains set to the given signal handler.

.PP
The signal handler must either return (via a normal 680x0 rts instruction)
or call the
.I Psigreturn
system call to indicate when signal handling is
complete; in both cases, the signal will be unblocked. Psigreturn also
performs some internal clean-up of the kernel stack that is necessary if
the signal handler is not planning to return (for example, if the C
longjmp() function is to be used to continue execution at another point
in the program).

.PP
Signal handlers may make any GEMDOS, BIOS, or XBIOS system calls freely.
GEM AES and VDI calls should not be made in a signal handler.

.PP
The
.I sa_flags
field specifies additional, signal-specific signal handling behavior.
If
.I sig
is SIGCHLD, and the
.I SA_NOCLDSTOP
bit is set in
.IR sa_flags ,
then the SIGCHLD signal is sent to this process only when one of its
children terminates (and not when a child is suspended by a job control
signal).

.PP
The
.I oact
argument to
.IR Psigaction ,
if non-zero, specifies a structure that will be set to reflect the signal
handling for
.I sig
that was current at the time of the
.I Psigaction
system call.

.PP
Note that calling
.I Psigaction
to change behavior of a signal has the side
effect of unmasking that signal, so that delivery is possible. This is done
so that processes may, while handling a signal, reset the behavior and
send themselves another instance of the signal, for example in order
to suspend themselves while handling a job control signal.
.IR
Signal handling is preserved across
.I Pfork
and
.I Pvfork
calls. Signals
that are ignored by the parent are also ignored by the child after a
.I Pexec
call; signals that were being caught for handling in a function are reset
in the child to the default behavior.

.SH RETURNS

0 on success

.PP
ERANGE if
.I sig
is not a legal signal.

.PP
EACCDN if the signal may not be caught by the user

.SH "SEE ALSO"
.IR Pkill (2),
.IR Psigblock (2),
.IR Psignal (2),
.IR Psigreturn (2)

.SH BUGS
Signal handling can be nested only a small (around 3) number of times,
i.e. if 4 signals are delivered to a process, and the process has established
handlers for all 4, and none of the handlers has returned or called
.IR Psigreturn ,
 then there is a very good chance of a stack overflow killing
the process off. In practice, this is unlikely to happen.

.SH AUTHOR
Alex Kiernan
