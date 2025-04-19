/**
 * @file libpolyhedron/errno/strerror.c
 * @brief strerror
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of reduceOS.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <errno.h>
#include <stdlib.h>

/* Errno strings */
/* SOURCE: https://man7.org/linux/man-pages/man3/errno.3.html */
static const char *__errno_strings[] = {
    [E2BIG]             = "Argument list too long",
    [EACCES]            = "Permission denied",
    [EADDRINUSE]        = "Address already in use",
    [EADDRNOTAVAIL]     = "Address not available",
    [EAFNOSUPPORT]      = "Address family not supported",
    [EAGAIN]            = "Resource temporarily unavailable",
    [EALREADY]          = "Connection already in progress",
    /* EBADE */
    [EBADF]             = "Bad file descriptor",
    /* EBADFD */
    [EBADMSG]           = "Bad message",
    /* EBADR, EBADRQC, EBADSLT */
    [EBUSY]             = "Device or resource busy",
    [ECANCELED]         = "Operation canceled",
    [ECHILD]            = "No child processes",
    /* ECHRNG, ECOMM */
    [ECONNABORTED]      = "Connection aborted",
    [ECONNREFUSED]      = "Connection refused",
    [ECONNRESET]        = "Connection reset",
    [EDEADLK]           = "Resource deadlock avoided",
    [EDESTADDRREQ]      = "Destination address required",
    [EDOM]              = "Mathematics argument out of domain of function",
    [EDQUOT]            = "Disk quota exceeded",
    [EEXIST]            = "File exists",
    /* EFAULT */
    [EFBIG]             = "File too large",
    [EHOSTDOWN]         = "Host is down",
    [EHOSTUNREACH]      = "Host is unreachable",
    /* EHWPOISON */
    [EIDRM]             = "Identifier removed",
    [EILSEQ]            = "Invalid or incomplete multibyte or wide character",
    [EINPROGRESS]       = "Operation in progress",
    [EINTR]             = "Interrupted function call",
    [EINVAL]            = "Invalid argument",
    [EIO]               = "Input/output error",
    [EISCONN]           = "Socket is connected",
    [EISDIR]            = "Is a directory",
    /* EISNAM, EKEYEXPIRED, EKEYREJECTED, EKEYREVOKED, EL2HLT */
    /* EL3HLT, EL3RST */
    /* ELIBACC, ELIBBAD, ELIBMAX, ELIBEXEC, ELNRNG */
    [ELOOP]             = "Too many levels of symbolic links",
    /* EMEDIUMTYPE */
    [EMFILE]            = "Too many open files",
    [EMLINK]            = "Too many open links",
    [EMSGSIZE]          = "Message too long",
    [EMULTIHOP]         = "Multihop requested",
    [ENAMETOOLONG]      = "Filename too long",
    [ENETDOWN]          = "Network is down",
    [ENETRESET]         = "Connection aborted by network",
    [ENETUNREACH]       = "Network unreachable",
    [ENFILE]            = "Too many open files in system",
    /* ENOANO */
    [ENOBUFS]           = "No buffer space available",
    [ENODATA]           = "The named attribute does not exist, or the process has no access to this attribute.",
    [ENODEV]            = "No such device",
    [ENOENT]            = "No such file or directory",
    [ENOEXEC]           = "Exec format error",
    /* ENOKEY */
    [ENOLCK]            = "No locks available",
    [ENOLINK]           = "Link has been severed",
    /* ENOMEDIUM */
    [ENOMEM]            = "Not enough space/cannot allocate memory",
    [ENOMSG]            = "No message of the desired type",
    /* ENONET, ENOPKG */
    [ENOPROTOOPT]       = "Protocol not available",
    [ENOSPC]            = "No space left on device",
    [ENOSR]             = "No STREAM resources",
    [ENOSTR]            = "Not a STREAM",
    [ENOSYS]            = "Function not implemented",
    /* ENOTBLK */
    [ENOTCONN]          = "The socket is not connected",
    [ENOTDIR]           = "Not a directory",
    [ENOTEMPTY]         = "Directory not empty",
    [ENOTRECOVERABLE]   = "State not recoverable",
    [ENOTSOCK]          = "Not a socket",
    [ENOTSUP]           = "Operation not supported",
    [ENOTTY]            = "Inappropriate I/O control operation",
    /* ENOTUNIQ */
    [ENXIO]             = "No such device or address",
    [EOPNOTSUPP]        = "Operation not supported on socket",
    [EOVERFLOW]         = "Value too large to be stored in data type",
    [EOWNERDEAD]        = "Owner died",
    [EPERM]             = "Operation not permitted",
    [EPIPE]             = "Broken pipe",
    [EPROTO]            = "Protocol error",
    [EPROTONOSUPPORT]   = "Protocol not supported",
    [EPROTOTYPE]        = "Protocol wrong type for socket",
    [ERANGE]            = "Result too large",
    /* EREMOTE, EREMOTEIO */
    [ERESTARTSYS]       = "Interrupted system call should be restarted",
    /* ERFKILL */
    [EROFS]             = "Read-only filesystem",
    /* ESHUTDOWN */
    [ESPIPE]            = "Invalid seek",
    [ESRCH]             = "No such process",
    [ESTALE]            = "Stale file handle",
    /* ESTRPIPE */
    [ETIME]             = "Timer expired",
    [ETIMEDOUT]         = "Connection timed out",
    [ETOOMANYREFS]      = "Too many references: cannot splice",
    [ETXTBSY]           = "Text file busy",
    /* EUCLEAN, EUNATCH, EUSERS */
    // [EWOULDBLOCK]       = "Operation would block", (SAME VALUE AS EAGAIN)
    [EXDEV]             = "Invalid cross-device link",
    /* EXFULL */
};

char *strerror(int errnum) {
    if (errnum >= (int)(sizeof(__errno_strings) / sizeof(const char*))) {
        // might as well return something?
        return "(Bad error number)";
    }

    return (char*)__errno_strings[errnum];
}