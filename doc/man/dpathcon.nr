.TH Dpathconf 2 "MiNT Programmer's Manual" "Version 1.0" "Feb. 1, 1993"
.SH NAME
Dpathconf \- get information about file system configuration
.SH SYNOPSIS
.nf
LONG Dpathconf( char *name, WORD mode );
.fi
.SH DESCRIPTION
.I Dpathconf
returns information about various limits or capabilities of the file system
containing the file named
.IR name .
The variable
.I mode
controls which limit or capability is being queried, as follows:
.nf
       mode	Value Returned

	-1	return max. legal value for n in Dpathconf(n)
	0	return internal limit on the number of open files
	1	return max. number of links to a file
	2	return max. length of a full path name
	3	return max. length of an individual file name
	4	return number of bytes that can be written atomically
	5	return information about file name truncation
	6	return information about case sensitivity
.fi
If any of these items are unlimited, then 0x7fffffffL is returned.
.PP
For mode 5, return information about file name truncation, the returned
value has the following meaning:
.IP 0
File names are never truncated; if the file name in any system call
affecting this directory exceeds the maximum length
(returned by mode 3), then the error value ERANGE is returned from that
system call.
.IP 1
File names are automatically truncated to the maximum length.
.IP 2
File names are truncated according to DOS rules, i.e. to a maximum 8
character base name and a maximum 3 character extension.
.PP
For mode 6, information about case sensitivity, the returned value has
the following meaning:
.IP 0
File system is case sensitive.
.IP 1
File system is case insensitive, and file case information is not
preserved (e.g. file names are always converted to upper case).
.IP 2
File system is case insensitive, but file names are saved with the
case they were originally given (e.g. file "FooBar" will appear
in the directory under that name, and may be accessed as "foobar"
and "fOObaR", etc.).
.SH "SEE ALSO"
.IR Sysconf (2)
