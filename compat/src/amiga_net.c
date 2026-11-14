/* GFOpenBoost: the network calls amigaos_net.hpp declares, and pause(), for Boost.Asio.
 * IPv4 only: the stack has no IPv6. From aMule's port, now shared.
 * The address text conversions are in amiga_inet.c.
 * Copyright (c) 2026 AmiCradle Team
 * Distributed under the Boost Software License, Version 1.0. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>

/* inet_ntop() and inet_pton() are in amiga_inet.c */

unsigned int if_nametoindex(const char *name)
{
    (void)name;
    return 0;
}

char *if_indextoname(unsigned int index, char *name)
{
    (void)index;
    (void)name;
    errno = ENXIO;
    return NULL;
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t total = 0;
    int i;
    for (i = 0; i < iovcnt; i++) {
        ssize_t n = read(fd, iov[i].iov_base, iov[i].iov_len);
        if (n < 0)
            return total ? total : -1;
        total += n;
        if ((size_t)n < iov[i].iov_len)
            break;
    }
    return total;
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t total = 0;
    int i;
    for (i = 0; i < iovcnt; i++) {
        ssize_t n = write(fd, iov[i].iov_base, iov[i].iov_len);
        if (n < 0)
            return total ? total : -1;
        total += n;
        if ((size_t)n < iov[i].iov_len)
            break;
    }
    return total;
}

/* pause(): Asio's single-task scheduler sleeps with it when it has nothing
 * to wait on. Sleep until Ctrl-C, as a signal would end it on POSIX. */
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>

int pause(void)
{
    Wait(SIGBREAKF_CTRL_C);
    errno = EINTR;
    return -1;
}
