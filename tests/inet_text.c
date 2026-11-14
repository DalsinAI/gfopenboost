/* GFOpenBoost host test: compat/src/amiga_inet.c's inet_pton()/inet_ntop()
 * against the host C library's, for IPv4 and IPv6 text.
 *   gcc -std=gnu99 tests/inet_text.c compat/src/amiga_inet.c -ldl
 * Our functions override the C library's in this program; the C library's
 * are reached with dlsym(RTLD_NEXT). One line a check, "inet_text: N failed"
 * at the end.
 * Copyright (c) 2026 AmiCradle Team
 * Distributed under the Boost Software License, Version 1.0. */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static int failed = 0;
static void check(const char *what, int ok, const char *detail)
{
    printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", what, detail && *detail ? ": " : "", detail ? detail : "");
    fflush(stdout);
    if (!ok) failed++;
}

typedef int (*pton_fn)(int, const char *, void *);
typedef const char *(*ntop_fn)(int, const void *, char *, socklen_t);

int main(void)
{
    pton_fn libc_pton = (pton_fn)dlsym(RTLD_NEXT, "inet_pton");
    ntop_fn libc_ntop = (ntop_fn)dlsym(RTLD_NEXT, "inet_ntop");
    static const char *in[] = {
        "ff15::efc0:988f", "::", "::1", "1::", "2001:db8::1", "2001:DB8:0:0:8:800:200C:417A",
        "fe80::1:2:3:4", "1:2:3:4:5:6:7:8", "1:0:0:2:0:0:0:3", "0:0:1:0:0:1:0:0",
        "::ffff:192.168.0.195", "::192.0.2.1", "64:ff9b::192.0.2.33", "1:2:3:4:5:6:1.2.3.4",
        /* refused */
        "", ":", ":::", "1:::2", "1::2::3", "12345::", "1:2:3:4:5:6:7:8:9", "1:2:3:4:5:6:7", "g::",
        "1:", ":1", "::1:", "1::2:3:4:5:6:7:8", "::256.1.1.1", "::1.2.3", "1.2.3.4", "::01.2.3.4",
        "1:2:3:4:5:6:7:1.2.3.4",
    };
    static const char *in4[] = { "0.0.0.0", "127.0.0.1", "255.255.255.255", "192.168.0.195",
        "256.1.1.1", "1.2.3", "1.2.3.4.5", "01.2.3.4", "1.2.3.4 ", "", "a.b.c.d" };
    unsigned i;
    char d[256];
    if (!libc_pton || !libc_ntop) { check("dlsym the C library's", 0, dlerror()); return 1; }

    for (i = 0; i < sizeof in / sizeof *in; i++) {
        unsigned char a[16], b[16];
        memset(a, 0xaa, 16); memset(b, 0xaa, 16);
        int ra = inet_pton(AF_INET6, in[i], a), rb = libc_pton(AF_INET6, in[i], b);
        snprintf(d, sizeof d, "\"%s\" ours %d, C library %d", in[i], ra, rb);
        check("pton6 as the C library", ra == rb && (ra != 1 || !memcmp(a, b, 16)), d);
        if (ra == 1) {
            char ta[64], tb[64];
            const char *pa = inet_ntop(AF_INET6, a, ta, sizeof ta), *pb = libc_ntop(AF_INET6, b, tb, sizeof tb);
            snprintf(d, sizeof d, "\"%s\" ours \"%s\", C library \"%s\"", in[i], pa ? pa : "(null)", pb ? pb : "(null)");
            check("ntop6 as the C library", pa && pb && !strcmp(pa, pb), d);
        }
    }
    for (i = 0; i < sizeof in4 / sizeof *in4; i++) {
        unsigned char a[4], b[4];
        int ra = inet_pton(AF_INET, in4[i], a), rb = libc_pton(AF_INET, in4[i], b);
        snprintf(d, sizeof d, "\"%s\" ours %d, C library %d", in4[i], ra, rb);
        check("pton4 as the C library", ra == rb && (ra != 1 || !memcmp(a, b, 4)), d);
    }
    {
        /* 20000 addresses, zero groups made likely, through both */
        unsigned long x = 12345, bad = 0, k;
        for (k = 0; k < 20000 && bad < 3; k++) {
            unsigned char a[16], r[16];
            char ta[64], tb[64];
            int j;
            for (j = 0; j < 16; j += 2) {
                x = x * 6364136223846793005UL + 1442695040888963407UL;
                unsigned v = (unsigned)(x >> 33);
                if (v % 3) v = 0; /* two in three groups zero */
                a[j] = (unsigned char)(v >> 8); a[j + 1] = (unsigned char)v;
            }
            const char *pa = inet_ntop(AF_INET6, a, ta, sizeof ta), *pb = libc_ntop(AF_INET6, a, tb, sizeof tb);
            if (!pa || !pb || strcmp(pa, pb) || inet_pton(AF_INET6, pa, r) != 1 || memcmp(a, r, 16)) {
                snprintf(d, sizeof d, "ours \"%s\", C library \"%s\"", pa ? pa : "(null)", pb ? pb : "(null)");
                check("random address", 0, d);
                bad++;
            }
        }
        check("20000 random addresses print as the C library's and parse back", bad == 0, "");
    }
    {
        unsigned char a[16] = {0};
        char t[8];
        errno = 0;
        check("ntop6 too small a buffer: ENOSPC", inet_ntop(AF_INET6, a, t, 2) == NULL && errno == ENOSPC, "");
        errno = 0;
        {
            static const char *bad[] = { "1.2.x", "1.2.3.4x", "1.2.3", "9.9.9.256" };
            unsigned char g[16], o[16];
            int k;
            for (k = 0; k < 4; k++) {
                memset(g, 0xAA, sizeof g);
                memcpy(o, g, sizeof g);
                check("refused v4 leaves the buffer alone", inet_pton(AF_INET, bad[k], g) == 0 && !memcmp(g, o, 16), bad[k]);
                check("refused v6 leaves the buffer alone", inet_pton(AF_INET6, bad[k], g) == 0 && !memcmp(g, o, 16), bad[k]);
            }
        }
        check("another family: EAFNOSUPPORT", inet_pton(12345, "1", a) == -1 && errno == EAFNOSUPPORT, "");
    }
    printf("inet_text: %d failed\n", failed);
    return failed ? 20 : 0;
}
