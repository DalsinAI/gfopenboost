/* GFOpenBoost: Boost.Asio's Amiga reactor talks to event.library through
 * these calls, so no Amiga header reaches Boost's C++ headers.
 *
 * A round is: the reactor names what it waits for (sockets by libnix
 * descriptor, Exec signals, MsgPorts), gfob_ev_run_once() adds one watch
 * for each, waits once with EV_RunOnce() and takes the watches away again,
 * and the reactor reads what was ready by slot. The loop itself stays from
 * round to round; it is made again only when the program's bsdsocket base
 * changes (a program that opens the stack after its first timer).
 * Copyright (c) 2026 AmiCradle Team
 * Distributed under the Boost Software License, Version 1.0. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <exec/types.h>
#include <exec/ports.h>
#include <utility/hooks.h>
#include <proto/exec.h>

#define EVENT_BASE_NAME gfob_EventBase
#include <libraries/event.h>
#include <clib/event_protos.h>
#include <inline/event.h>

struct Library *gfob_EventBase;
static int gfob_ev_users;

/* The head of libnix's struct SocketSettings (as amiga_select.c) */
struct gfob_socket_settings {
    int lx_network_type;               /* 1: AmiTCP-style bsdsocket.library */
    void *lx_BsdSocketBase;
};
extern __stdargs struct gfob_socket_settings *_lx_get_socket_settings(void);

#define REG(r, decl) register decl __asm(#r)

struct gfob_rw {                       /* one watch for one round */
    struct Hook hook;
    APTR watch;
    int kind;                          /* EVK_SOCKET, EVK_SIGNAL, EVK_PORT */
    LONG sock;                         /* EVK_SOCKET: the stack's descriptor */
    ULONG what;                        /* socket: EVF_*; signals: the mask */
    struct MsgPort *port;              /* EVK_PORT */
    ULONG ready;                       /* what came: EVF_*, signal bits, 1 */
};

struct gfob_ev {
    APTR loop;
    void *sockbase;                    /* the base the loop was made with */
    struct gfob_rw *rw;
    int nrw, maxrw;
    int sig_slot;                      /* -1: no signal watch this round */
};

static void *gfob_sockbase(void)
{
    struct gfob_socket_settings *ss = _lx_get_socket_settings();
    return (ss && ss->lx_network_type == 1) ? ss->lx_BsdSocketBase : 0;
}

static ULONG gfob_rw_hook(REG(a0, struct Hook *h), REG(a2, APTR w), REG(a1, struct EVMessage *m))
{
    struct gfob_rw *rw = (struct gfob_rw *)h->h_Data;
    (void)w;
    if (m->em_Kind == EVK_PORT)
        rw->ready = 1;                 /* the message stays for the program */
    else
        rw->ready |= m->em_Events;
    return 0;
}

static APTR gfob_make_loop(void *sockbase)
{
    return EV_CreateLoop(EVT_SocketBase, (ULONG)sockbase, EVT_BreakStops, FALSE, TAG_DONE);
}

struct gfob_ev *gfob_ev_create(void)
{
    struct gfob_ev *e;
    if (!gfob_EventBase && !(gfob_EventBase = OpenLibrary((CONST_STRPTR)EVENTNAME, 1))) {
        errno = ENOSYS;
        return 0;
    }
    if ((e = (struct gfob_ev *)calloc(1, sizeof *e))) {
        e->sockbase = gfob_sockbase();
        e->sig_slot = -1;
        if ((e->loop = gfob_make_loop(e->sockbase))) {
            gfob_ev_users++;
            return e;
        }
        free(e);
    }
    if (!gfob_ev_users) { CloseLibrary(gfob_EventBase); gfob_EventBase = 0; }
    errno = ENOMEM;
    return 0;
}

void gfob_ev_delete(struct gfob_ev *e)
{
    if (!e) return;
    EV_DeleteLoop(e->loop);
    free(e->rw);
    free(e);
    if (--gfob_ev_users == 0) { CloseLibrary(gfob_EventBase); gfob_EventBase = 0; }
}

/* From any task: end the wait. */
void gfob_ev_wake(struct gfob_ev *e)
{
    if (e) EV_Wake(e->loop);
}

/* Start a round: nothing named yet. */
void gfob_ev_begin(struct gfob_ev *e)
{
    void *sb = gfob_sockbase();
    e->nrw = 0;
    e->sig_slot = -1;
    if (sb != e->sockbase) {           /* between rounds there are no watches */
        APTR nl = gfob_make_loop(sb);
        if (nl) { EV_DeleteLoop(e->loop); e->loop = nl; e->sockbase = sb; }
    }
}

static struct gfob_rw *gfob_slot(struct gfob_ev *e, int kind, int *slot)
{
    struct gfob_rw *rw;
    if (e->nrw == e->maxrw) {
        int n = e->maxrw ? e->maxrw * 2 : 16;
        struct gfob_rw *p = (struct gfob_rw *)realloc(e->rw, n * sizeof *p);
        if (!p) return 0;
        e->rw = p; e->maxrw = n;
    }
    *slot = e->nrw;
    rw = &e->rw[e->nrw++];
    memset(rw, 0, sizeof *rw);
    rw->kind = kind;
    return rw;
}

/* events: 1 read, 2 write, 4 except (EVF_*). The slot, or -1 when fd is
 * not a socket of this program's stack, -2 out of memory. */
int gfob_ev_add_socket(struct gfob_ev *e, int fd, unsigned events)
{
    StdFileDes *d = _lx_fhfromfd(fd);
    struct gfob_rw *rw;
    int slot;
    if (!d || !(d->lx_flags & LX_SOCKET) || !e->sockbase)
        return -1;
    if (!(rw = gfob_slot(e, EVK_SOCKET, &slot)))
        return -2;
    rw->sock = d->lx_sock;
    rw->what = events;
    return slot;
}

/* Exec signals: a round's masks all go in one watch. */
int gfob_ev_add_signals(struct gfob_ev *e, unsigned long mask)
{
    struct gfob_rw *rw;
    int slot;
    if (e->sig_slot >= 0) {
        e->rw[e->sig_slot].what |= mask;
        return e->sig_slot;
    }
    if (!(rw = gfob_slot(e, EVK_SIGNAL, &slot)))
        return -2;
    rw->what = mask;
    e->sig_slot = slot;
    return slot;
}

int gfob_ev_add_port(struct gfob_ev *e, void *port)
{
    struct gfob_rw *rw;
    int slot;
    if (!(rw = gfob_slot(e, EVK_PORT, &slot)))
        return -2;
    rw->port = (struct MsgPort *)port;
    return slot;
}

/* One wait of at most usec microseconds (0: just look), then the watches go.
 * Returns how many slots were ready, or -1 (errno ENOMEM, or EIO when
 * WaitSelect() failed: then every socket slot reads as ready, so Asio tries
 * each operation and gets the real error from it). */
long gfob_ev_run_once(struct gfob_ev *e, long usec)
{
    struct timeval tv;
    LONG r;
    int i, n = 0, bad = 0;
    for (i = 0; i < e->nrw; i++) {
        struct gfob_rw *rw = &e->rw[i];
        rw->hook.h_Entry = (HOOKFUNC)gfob_rw_hook;
        rw->hook.h_Data = rw;
        switch (rw->kind) {
        case EVK_SOCKET: rw->watch = EV_AddSocket(e->loop, rw->sock, rw->what, &rw->hook); break;
        case EVK_SIGNAL: rw->watch = EV_AddSignals(e->loop, rw->what, &rw->hook); break;
        default:         rw->watch = EV_AddPort(e->loop, rw->port, &rw->hook); break;
        }
        if (!rw->watch) bad = 1;
    }
    if (bad) {
        r = EVERR_NOMEM;
    } else {
        if (usec < 0) usec = 0;
        tv.tv_sec = usec / 1000000;
        tv.tv_usec = usec % 1000000;
        r = EV_RunOnce(e->loop, &tv);
    }
    for (i = 0; i < e->nrw; i++) {
        struct gfob_rw *rw = &e->rw[i];
        EV_Remove(rw->watch);
        rw->watch = 0;
        if (r == EVERR_SELECT && rw->kind == EVK_SOCKET)
            rw->ready = rw->what;
        if (rw->ready) n++;
    }
    if (r == EVERR_NOMEM) { errno = ENOMEM; return -1; }
    if (r == EVERR_SELECT) { errno = EIO; return -1; }
    return n;
}

/* What slot came: EVF_* for a socket, the signal bits, 1 for a port. */
unsigned long gfob_ev_ready(struct gfob_ev *e, int slot)
{
    return (slot >= 0 && slot < e->nrw) ? e->rw[slot].ready : 0;
}
