/* GFOpenBoost: inet_ntop() and inet_pton() for Boost.Asio, which libnix
 * lacks. The stack is IPv4 only, but the text forms of IPv6 addresses are
 * just text: programs parse IPv6 literals (libtorrent's local service
 * discovery group, at start-up) and print addresses they were given, and an
 * EAFNOSUPPORT there becomes a throw. Sockets stay IPv4.
 * IPv6 per RFC 4291 section 2.2 in, RFC 5952 out (lower case, the longest
 * run of two or more zero groups as "::", IPv4-mapped as ::ffff:a.b.c.d).
 * Plain C with no Amiga calls, so tests/inet_text.c checks it on a host
 * against the C library's.
 * Copyright (c) 2026 AmiCradle Team
 * Distributed under the Boost Software License, Version 1.0. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef AF_INET6
#define AF_INET6 28 /* as boost/config/platform/amigaos_net.hpp */
#endif
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

static int pton4(const char *src, unsigned char *out)
{
    unsigned char b[4]; /* the caller's buffer is left alone on failure */
    int i;
    for (i = 0; i < 4; i++) {
        unsigned v = 0;
        int digits = 0;
        while (*src >= '0' && *src <= '9') {
            if (digits == 1 && v == 0) return 0; /* no leading zeros, as glibc */
            v = v * 10 + (unsigned)(*src++ - '0');
            if (++digits > 3 || v > 255) return 0;
        }
        if (!digits) return 0;
        b[i] = (unsigned char)v;
        if (i < 3 && *src++ != '.') return 0;
    }
    if (*src) return 0;
    memcpy(out, b, 4);
    return 1;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int pton6(const char *src, unsigned char *out)
{
    unsigned char b[16];
    int n = 0, gap = -1;
    memset(b, 0, sizeof b);
    if (src[0] == ':') {
        if (src[1] != ':') return 0;
        src++;
    }
    while (*src) {
        if (*src == ':') {
            if (gap >= 0) return 0; /* a second "::" */
            gap = n;
            src++;
            if (!*src) break;
            continue;
        }
        {
            const char *start = src;
            unsigned v = 0;
            int digits = 0, h;
            while ((h = hexval(*src)) >= 0) {
                v = (v << 4) | (unsigned)h;
                src++;
                if (++digits > 4) return 0;
            }
            if (*src == '.') { /* the IPv4 tail */
                if (n > 12 || !pton4(start, b + n)) return 0;
                n += 4;
                break;
            }
            if (!digits || n > 14) return 0;
            b[n++] = (unsigned char)(v >> 8);
            b[n++] = (unsigned char)v;
            if (*src == ':') {
                src++;
                if (!*src) return 0; /* a lone ':' at the end */
                if (*src == ':') continue; /* "::" next, handled above */
            } else if (*src) {
                return 0;
            }
        }
    }
    if (gap >= 0) {
        int move = n - gap;
        if (n == 16) return 0; /* "::" must stand for at least one group */
        memmove(b + 16 - move, b + gap, (size_t)move);
        memset(b + gap, 0, (size_t)(16 - move - gap));
    } else if (n != 16) {
        return 0;
    }
    memcpy(out, b, 16);
    return 1;
}

int inet_pton(int af, const char *src, void *dst)
{
    if (af == AF_INET) return pton4(src, (unsigned char *)dst);
    if (af == AF_INET6) return pton6(src, (unsigned char *)dst);
    errno = EAFNOSUPPORT;
    return -1;
}

static const char *put(const char *t, char *dst, socklen_t size)
{
    if (strlen(t) + 1 > size) {
        errno = ENOSPC;
        return NULL;
    }
    strcpy(dst, t);
    return dst;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    const unsigned char *b = (const unsigned char *)src;
    if (af == AF_INET) {
        char t[INET_ADDRSTRLEN];
        snprintf(t, sizeof t, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
        return put(t, dst, size);
    }
    if (af == AF_INET6) {
        char t[INET6_ADDRSTRLEN];
        unsigned g[8];
        int i, best = -1, bestlen = 0, cur = -1, curlen = 0;
        size_t len = 0;
        for (i = 0; i < 8; i++) g[i] = ((unsigned)b[2 * i] << 8) | b[2 * i + 1];
        for (i = 0; i < 8; i++) {
            if (g[i] == 0) {
                if (cur < 0) { cur = i; curlen = 0; }
                if (++curlen > bestlen) { best = cur; bestlen = curlen; }
            } else {
                cur = -1;
            }
        }
        if (bestlen < 2) best = -1;
        /* IPv4-compatible (::a.b.c.d) and IPv4-mapped (::ffff:a.b.c.d)
         * with a dotted tail, as the C libraries print them */
        if (best == 0 && (bestlen == 6 || (bestlen == 5 && g[5] == 0xffff))) {
            snprintf(t, sizeof t, "::%s%u.%u.%u.%u", bestlen == 5 ? "ffff:" : "", b[12], b[13], b[14], b[15]);
            return put(t, dst, size);
        }
        for (i = 0; i < 8; i++) {
            if (i == best) {
                t[len++] = ':';
                if (i == 0) t[len++] = ':';
                i += bestlen - 1;
                if (i == 7) break;
                continue;
            }
            len += (size_t)snprintf(t + len, sizeof t - len, "%x", g[i]);
            if (i < 7) t[len++] = ':';
        }
        t[len] = 0;
        return put(t, dst, size);
    }
    errno = EAFNOSUPPORT;
    return NULL;
}
