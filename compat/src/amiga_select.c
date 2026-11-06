/* GFOpenBoost: select() for sockets, straight onto bsdsocket.library's
 * WaitSelect(). libnix's select() (libsocket) mixes files and sockets
 * through its own timer loop and, in the lab, never came back from a 50 ms
 * wait on a UDP socket, so Boost.Asio's reactor hung. Linked from
 * libboost_asio ahead of libsocket. Every descriptor in the sets must be a
 * socket (ENOTSOCK otherwise); with none, it just waits out the timeout.
 * Ctrl-C ends the wait with EINTR, as WaitSelect() does.
 * Copyright (c) 2026 AmiCradle Team
 * Distributed under the Boost Software License, Version 1.0. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

/* The head of libnix's struct SocketSettings (libnix socket.h) */
struct gfob_socket_settings {
    int lx_network_type;               /* 1: AmiTCP-style bsdsocket.library */
    void *lx_BsdSocketBase;
};
extern __stdargs struct gfob_socket_settings *_lx_get_socket_settings(void);

static long gfob_waitselect(void *base, long nfds, void *r, void *w, void *e, struct timeval *tv)
{
    register unsigned long d0 __asm("d0") = (unsigned long)nfds;
    register unsigned long d1 __asm("d1") = 0;
    register unsigned long a0 __asm("a0") = (unsigned long)r;
    register unsigned long a1 __asm("a1") = (unsigned long)w;
    register unsigned long a2 __asm("a2") = (unsigned long)e;
    register unsigned long a3 __asm("a3") = (unsigned long)tv;
    register void *a6 __asm("a6") = base;
    __asm__ __volatile__("jsr -126(%%a6)"
        : "+r"(d0), "+r"(d1), "+r"(a0), "+r"(a1)
        : "r"(a2), "r"(a3), "r"(a6)
        : "cc", "memory");
    return (long)d0;
}

#define GFOB_SETBITS ((int)(sizeof(fd_set) * 8))

/* fd -> socket for every descriptor set in any of the three sets; -1 when a
 * set descriptor is not a socket */
static int gfob_map(int fd, int *sock)
{
    StdFileDes *d = _lx_fhfromfd(fd);
    if (!d || !(d->lx_flags & LX_SOCKET))
        return -1;
    *sock = d->lx_sock;
    return 0;
}

int select(int nfds, fd_set *r, fd_set *w, fd_set *e, struct timeval *tv)
{
    struct gfob_socket_settings *ss = _lx_get_socket_settings();
    fd_set sr, sw, se;
    int fd, s, maxs = -1, n, count = 0;

    if (!ss || ss->lx_network_type != 1 || !ss->lx_BsdSocketBase) {
        errno = ENOSYS;
        return -1;
    }
    if (nfds > GFOB_SETBITS)
        nfds = GFOB_SETBITS;
    FD_ZERO(&sr);
    FD_ZERO(&sw);
    FD_ZERO(&se);
    for (fd = 0; fd < nfds; fd++) {
        int in_r = r && FD_ISSET(fd, r), in_w = w && FD_ISSET(fd, w), in_e = e && FD_ISSET(fd, e);
        if (!(in_r || in_w || in_e))
            continue;
        if (gfob_map(fd, &s) != 0) {
            errno = ENOTSOCK;
            return -1;
        }
        if (s < 0 || s >= GFOB_SETBITS) {
            errno = EINVAL;
            return -1;
        }
        if (in_r) FD_SET(s, &sr);
        if (in_w) FD_SET(s, &sw);
        if (in_e) FD_SET(s, &se);
        if (s > maxs)
            maxs = s;
    }
    n = gfob_waitselect(ss->lx_BsdSocketBase, maxs + 1, r ? &sr : 0, w ? &sw : 0, e ? &se : 0, tv);
    if (n < 0)
        return -1;      /* errno set by the stack (libnix registers it) */
    for (fd = 0; fd < nfds; fd++) {
        int in_r = r && FD_ISSET(fd, r), in_w = w && FD_ISSET(fd, w), in_e = e && FD_ISSET(fd, e);
        if (!(in_r || in_w || in_e))
            continue;
        gfob_map(fd, &s);
        if (in_r) { if (n > 0 && FD_ISSET(s, &sr)) count++; else FD_CLR(fd, r); }
        if (in_w) { if (n > 0 && FD_ISSET(s, &sw)) count++; else FD_CLR(fd, w); }
        if (in_e) { if (n > 0 && FD_ISSET(s, &se)) count++; else FD_CLR(fd, e); }
    }
    return count;
}
