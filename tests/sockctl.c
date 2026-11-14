/* GFOpenBoost: ioctl() and fcntl() on libnix socket descriptors, as POSIX
 * ports (Boost.Asio, aMule) call them, with compat/src/amiga_ioctl.c and
 * amiga_select.c linked ahead of libsocket. Built by
 * tools/asio-check/build-amiga.sh as SockCtl, and as SockCtl-libnix with
 * libnix's own ioctl()/fcntl() to show the difference.
 * Prints one PASS/FAIL line each and PASS=n FAIL=n; RC 20 on any FAIL.
 * Copyright (c) 2026 AmiCradle Team
 * Distributed under the Boost Software License, Version 1.0. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int passes, failures;

static void result(const char *name, int ok, int r)
{
    printf("%-46s %s", name, ok ? "PASS" : "FAIL");
    if (!ok) printf(" (rc %d errno %d)", r, errno);
    printf("\n");
    fflush(stdout);
    if (ok) passes++; else failures++;
}

static void loopback(struct sockaddr_in *a, int port)
{
    memset(a, 0, sizeof(*a));
    a->sin_family = AF_INET;
    a->sin_port = htons(port);
    a->sin_addr.s_addr = htonl(0x7f000001UL);
}

static void rcvtimeo(int s)
{
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}

static int wait_one(int s, int write, int secs)
{
    fd_set f;
    struct timeval tv;
    FD_ZERO(&f);
    FD_SET(s, &f);
    tv.tv_sec = secs;
    tv.tv_usec = 0;
    return write ? select(s + 1, NULL, &f, NULL, &tv) : select(s + 1, &f, NULL, NULL, &tv);
}

int main(void)
{
    struct sockaddr_in a, from;
    socklen_t len;
    int s, t, ls, cs, as, r, on = 1, off = 0, n, e;
    char buf[32], h1[4], h2[16];
    struct iovec iov[2];
    struct msghdr m;

    printf("GFOpenBoost ioctl()/fcntl() on sockets\n");
    s = socket(AF_INET, SOCK_DGRAM, 0);
    t = socket(AF_INET, SOCK_DGRAM, 0);
    result("socket UDP", s >= 0 && t >= 0, s);
    if (s < 0 || t < 0) return 20;
    loopback(&a, 23471);
    r = bind(s, (struct sockaddr *)&a, sizeof a);
    result("bind 127.0.0.1:23471", r == 0, r);
    /* a receive timeout, so a FIONBIO that didn't take shows as a FAIL
     * (ETIMEDOUT after 1 s) rather than a hang */
    rcvtimeo(s);

    errno = 0; r = fcntl(s, F_GETFD, 0);
    result("fcntl F_GETFD is 0", r == 0, r);
    errno = 0; r = fcntl(s, F_SETFD, FD_CLOEXEC);
    result("fcntl F_SETFD FD_CLOEXEC", r == 0, r);

    errno = 0; r = ioctl(s, FIONBIO, &on);
    result("ioctl FIONBIO on", r == 0, r);
    errno = 0; r = recv(s, buf, sizeof buf, 0);
    result("recv empty EWOULDBLOCK", r == -1 && errno == EWOULDBLOCK, r);
    n = -1; errno = 0; r = ioctl(s, FIONREAD, &n);
    result("ioctl FIONREAD empty is 0", r == 0 && n == 0, r);
    errno = 0; r = ioctl(s, FIONBIO, &off);
    result("ioctl FIONBIO off", r == 0, r);

    errno = 0; r = fcntl(s, F_SETFL, O_NONBLOCK);
    result("fcntl F_SETFL O_NONBLOCK", r == 0, r);
    errno = 0; r = fcntl(s, F_GETFL, 0);
    result("fcntl F_GETFL has O_NONBLOCK", r >= 0 && (r & O_NONBLOCK), r);
    len = sizeof from; errno = 0;
    r = recvfrom(s, buf, sizeof buf, 0, (struct sockaddr *)&from, &len);
    result("recvfrom empty EWOULDBLOCK (F_SETFL)", r == -1 && errno == EWOULDBLOCK, r);
    memset(&m, 0, sizeof m);
    iov[0].iov_base = buf; iov[0].iov_len = sizeof buf;
    m.msg_name = (void *)&from; m.msg_namelen = sizeof from;
    m.msg_iov = iov; m.msg_iovlen = 1;
    errno = 0; r = recvmsg(s, &m, 0);
    result("recvmsg empty EWOULDBLOCK", r == -1 && errno == EWOULDBLOCK, r);

    memset(&m, 0, sizeof m);
    iov[0].iov_base = "ping"; iov[0].iov_len = 4;
    iov[1].iov_base = "-pong"; iov[1].iov_len = 5;
    m.msg_name = (void *)&a; m.msg_namelen = sizeof a;
    m.msg_iov = iov; m.msg_iovlen = 2;
    errno = 0; r = sendmsg(t, &m, 0);
    result("sendmsg two iovecs", r == 9, r);
    r = wait_one(s, 0, 2);
    result("select sees the datagram", r == 1, r);
    n = -1; r = ioctl(s, FIONREAD, &n);
    result("ioctl FIONREAD sees it", r == 0 && n >= 9, n);
    memset(h1, 0, sizeof h1); memset(h2, 0, sizeof h2);
    memset(&m, 0, sizeof m);
    iov[0].iov_base = h1; iov[0].iov_len = sizeof h1;
    iov[1].iov_base = h2; iov[1].iov_len = sizeof h2;
    m.msg_name = (void *)&from; m.msg_namelen = sizeof from;
    m.msg_iov = iov; m.msg_iovlen = 2;
    errno = 0; r = recvmsg(s, &m, 0);
    result("recvmsg scatters into two iovecs", r == 9 && !memcmp(h1, "ping", 4) && !memcmp(h2, "-pong", 5), r);
    /* the name recvmsg gives is the sender's own port, as its getsockname
     * says: uTP answers to it, so a wrong one is a peer never heard */
    {
        struct sockaddr_in me;
        socklen_t ml = sizeof me;
        memset(&me, 0, sizeof me);
        r = getsockname(t, (struct sockaddr *)&me, &ml);
        result("recvmsg name is the sender's getsockname", r == 0 && m.msg_namelen >= 8
               && from.sin_addr.s_addr == htonl(0x7f000001UL) && from.sin_port == me.sin_port && me.sin_port != 0,
               ntohs(from.sin_port));
    }
    {
        int u = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in ua;
        loopback(&ua, 23473);
        r = u >= 0 ? bind(u, (struct sockaddr *)&ua, sizeof ua) : -1;
        result("bind a sender to 127.0.0.1:23473", r == 0, r);
        memset(&m, 0, sizeof m);
        iov[0].iov_base = "uTP"; iov[0].iov_len = 3;
        m.msg_name = (void *)&a; m.msg_namelen = sizeof a;
        m.msg_iov = iov; m.msg_iovlen = 1;
        errno = 0; r = sendmsg(u, &m, 0);
        result("sendmsg from the bound sender", r == 3, r);
        wait_one(s, 0, 2);
        memset(&from, 0, sizeof from);
        memset(&m, 0, sizeof m);
        iov[0].iov_base = buf; iov[0].iov_len = sizeof buf;
        m.msg_name = (void *)&from; m.msg_namelen = sizeof from;
        m.msg_iov = iov; m.msg_iovlen = 1;
        errno = 0; r = recvmsg(s, &m, 0);
        result("recvmsg name is the bound port 23473", r == 3 && ntohs(from.sin_port) == 23473, ntohs(from.sin_port));
        if (u >= 0) close(u);
    }

    errno = 0; r = fcntl(s, F_SETFL, 0);
    result("fcntl F_SETFL 0 (blocking)", r == 0, r);
    r = fcntl(s, F_GETFL, 0);
    result("fcntl F_GETFL without O_NONBLOCK", r >= 0 && !(r & O_NONBLOCK), r);

    errno = 0; r = ioctl(1, FIONBIO, &on);
    result("ioctl on the console ENOTTY", r == -1 && errno == ENOTTY, r);
    errno = 0; r = fcntl(97, F_GETFD, 0);
    result("fcntl on a closed descriptor EBADF", r == -1 && errno == EBADF, r);
    close(s);
    close(t);

    /* non-blocking connect, as Asio's async_connect does it */
    ls = socket(AF_INET, SOCK_STREAM, 0);
    cs = socket(AF_INET, SOCK_STREAM, 0);
    loopback(&a, 23472);
    r = (ls >= 0 && cs >= 0) ? bind(ls, (struct sockaddr *)&a, sizeof a) | listen(ls, 2) : -1;
    result("TCP listen 127.0.0.1:23472", r == 0, r);
    r = ioctl(cs, FIONBIO, &on);
    result("TCP client FIONBIO on", r == 0, r);
    errno = 0; r = connect(cs, (struct sockaddr *)&a, sizeof a);
    e = r < 0 ? errno : 0;
    result("non-blocking connect: 0 or EINPROGRESS", r == 0 || e == EINPROGRESS, r);
    r = wait_one(cs, 1, 3);
    result("select sees the connect writable", r == 1, r);
    e = -1; len = sizeof e;
    r = getsockopt(cs, SOL_SOCKET, SO_ERROR, &e, &len);
    result("getsockopt SO_ERROR is 0", r == 0 && e == 0, e);
    as = accept(ls, NULL, NULL);
    result("accept", as >= 0, as);
    if (as >= 0) {
        rcvtimeo(as);
        r = fcntl(as, F_SETFL, O_NONBLOCK);
        errno = 0; n = recv(as, buf, sizeof buf, 0);
        result("accepted socket: recv empty EWOULDBLOCK", r == 0 && n == -1 && errno == EWOULDBLOCK, n);
        /* A stream socket takes what fits and says how much: libtorrent
         * writes a 16 KiB block and its header in one sendmsg, and reads
         * into buffers bigger than one transfer. EMSGSIZE is for datagrams. */
        {
            static char big[20000], back[20000];
            struct iovec bv[2];
            int got = 0, tries;
            for (n = 0; n < (int)sizeof big; n++) big[n] = (char)(n * 7);
            bv[0].iov_base = big; bv[0].iov_len = 10000;
            bv[1].iov_base = big + 10000; bv[1].iov_len = 10000;
            memset(&m, 0, sizeof m);
            m.msg_iov = bv; m.msg_iovlen = 2;
            errno = 0; r = sendmsg(cs, &m, 0);
            result("TCP sendmsg 20000 bytes: some, not EMSGSIZE", r > 0 && r <= 20000, r);
            for (tries = 0; r > 0 && got < r && tries < 50; tries++) {
                wait_one(as, 0, 1);
                /* two iovecs over the room left, together over 16 KiB at first */
                bv[0].iov_base = back + got; bv[0].iov_len = (sizeof back - got) / 2;
                bv[1].iov_base = back + got + bv[0].iov_len; bv[1].iov_len = sizeof back - got - bv[0].iov_len;
                memset(&m, 0, sizeof m);
                m.msg_iov = bv; m.msg_iovlen = 2;
                errno = 0; n = recvmsg(as, &m, 0);
                if (n > 0) got += n;
                else if (!(n < 0 && errno == EWOULDBLOCK)) break;
            }
            result("TCP recvmsg 20000-byte buffers: all of it back", r > 0 && got == r && !memcmp(big, back, got), n);
        }
        close(as);
    }
    close(cs);
    close(ls);

    printf("PASS=%d FAIL=%d\n", passes, failures);
    return failures ? 20 : 0;
}
