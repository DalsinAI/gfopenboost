/* GFOpenBoost: ioctl() and fcntl() for sockets, in POSIX's form.
 *
 * libnix's ioctl() (libsocket, sources/socket/socket/ioctl.c) takes
 * ixemul's five-argument form, ioctl(fd, cmd, inout, arglen, data). Called
 * as POSIX has it, ioctl(fd, FIONBIO, &on), which is how Boost.Asio makes a
 * socket non-blocking, it passes IoctlSocket() whatever lies on the stack
 * two words after &on as the argument: EFAULT when that is 0 (Team 2's
 * udptest and aMule, 10 Nov 2026), and for FIONREAD a write through a
 * stray pointer when it isn't. libnix's fcntl() (nix/extra/fcntl.c) is a
 * stub: -1 for everything, errno left as it was.
 *
 * These replace both. Linked from libboost_asio ahead of libsocket and
 * libnix, like select() (amiga_select.c).
 *   ioctl(fd, cmd, arg): a socket's request goes to IoctlSocket() with arg;
 *     a file or console is ENOTTY.
 *   fcntl(fd, cmd, ...): F_GETFD gives 0 and F_SETFD is taken (there is no
 *     exec, so close-on-exec has nothing to do); F_SETFL sets or clears
 *     O_NONBLOCK on a socket through FIONBIO; F_GETFL gives O_RDWR, and
 *     O_NONBLOCK only as last set through these two calls on a socket that
 *     is still the same one (bsdsocket.library has no call that reports
 *     it). Other commands are EINVAL. A descriptor that isn't open is EBADF.
 * Copyright (c) 2026 AmiCradle Team
 * Distributed under the Boost Software License, Version 1.0. */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/filio.h>

/* The head of libnix's struct SocketSettings (as amiga_select.c) */
struct gfob_socket_settings {
    int lx_network_type;               /* 1: AmiTCP-style bsdsocket.library */
    void *lx_BsdSocketBase;
};
extern __stdargs struct gfob_socket_settings *_lx_get_socket_settings(void);

static long gfob_ioctlsocket(void *base, long sock, unsigned long req, void *arg)
{
    register unsigned long d0 __asm("d0") = (unsigned long)sock;
    register unsigned long d1 __asm("d1") = req;
    register unsigned long a0 __asm("a0") = (unsigned long)arg;
    register void *a6 __asm("a6") = base;
    __asm__ __volatile__("jsr -114(%%a6)"
        : "+r"(d0), "+r"(d1), "+r"(a0)
        : "r"(a6)
        : "a1", "cc", "memory");
    return (long)d0;
}

/* O_NONBLOCK as last set here, for F_GETFL: by descriptor, with the
 * stack's socket number it was set on (a closed and reused descriptor
 * with another socket reads as blocking again). */
#define GFOB_NB_MAX 256
static int gfob_nb_sock[GFOB_NB_MAX];  /* socket + 1 when O_NONBLOCK */

/* The descriptor's stack socket and base, 0; -1 when it isn't a socket
 * (errno ENOTTY for ioctl's sake, or EBADF when it isn't open) */
static int gfob_sock(int fd, long *sock, void **base)
{
    StdFileDes *d = _lx_fhfromfd(fd);
    struct gfob_socket_settings *ss;
    if (!d || !d->lx_inuse) {
        errno = EBADF;
        return -1;
    }
    if (!(d->lx_flags & LX_SOCKET)) {
        errno = ENOTTY;
        return -1;
    }
    ss = _lx_get_socket_settings();
    if (!ss || ss->lx_network_type != 1 || !ss->lx_BsdSocketBase) {
        errno = ENOSYS;
        return -1;
    }
    *sock = d->lx_sock;
    *base = ss->lx_BsdSocketBase;
    return 0;
}

static void gfob_nb_note(int fd, long sock, int on)
{
    if (fd >= 0 && fd < GFOB_NB_MAX)
        gfob_nb_sock[fd] = on ? (int)sock + 1 : 0;
}

int ioctl(int fd, unsigned long cmd, ...)
{
    va_list va;
    void *arg, *base;
    long sock, r;
    va_start(va, cmd);
    arg = va_arg(va, void *);
    va_end(va);
    if (gfob_sock(fd, &sock, &base) != 0)
        return -1;
    r = gfob_ioctlsocket(base, sock, cmd, arg);   /* errno set by the stack */
    if (r == 0 && cmd == FIONBIO && arg)
        gfob_nb_note(fd, sock, *(int *)arg != 0);
    return (int)r;
}

int fcntl(int fd, int cmd, ...)
{
    va_list va;
    long arg, sock;
    void *base;
    StdFileDes *d = _lx_fhfromfd(fd);
    va_start(va, cmd);
    arg = va_arg(va, long);
    va_end(va);
    if (!d || !d->lx_inuse) {
        errno = EBADF;
        return -1;
    }
    switch (cmd) {
    case F_GETFD:
        return 0;
    case F_SETFD:
        return 0;
    case F_GETFL:
        if ((d->lx_flags & LX_SOCKET) && fd < GFOB_NB_MAX && gfob_nb_sock[fd] == d->lx_sock + 1)
            return O_RDWR | O_NONBLOCK;
        return O_RDWR;
    case F_SETFL:
        if (!(d->lx_flags & LX_SOCKET)) {
            if (arg & O_NONBLOCK) {
                errno = EINVAL;            /* a file can't be made non-blocking */
                return -1;
            }
            return 0;
        }
        if (gfob_sock(fd, &sock, &base) != 0)
            return -1;
        {
            long on = (arg & O_NONBLOCK) ? 1 : 0;
            if (gfob_ioctlsocket(base, sock, FIONBIO, &on) != 0)
                return -1;
            gfob_nb_note(fd, sock, (int)on);
        }
        return 0;
    default:
        errno = EINVAL;
        return -1;
    }
}
