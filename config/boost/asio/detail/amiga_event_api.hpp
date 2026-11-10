//
// detail/amiga_event_api.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// GFOpenBoost: the C calls (compat/src/amiga_event.c, in libboost_asio)
// that the Amiga reactor uses to wait through event.library.
//
// Copyright (c) 2026 AmiCradle Team
// Distributed under the Boost Software License, Version 1.0.
//

#ifndef BOOST_ASIO_DETAIL_AMIGA_EVENT_API_HPP
#define BOOST_ASIO_DETAIL_AMIGA_EVENT_API_HPP

extern "C" {
struct gfob_ev;
struct gfob_ev* gfob_ev_create(void);
// why gfob_ev_create() last failed, e.g. "event.library (version 1) not found in LIBS:"
const char* gfob_ev_error(void);
void gfob_ev_delete(struct gfob_ev*);
void gfob_ev_wake(struct gfob_ev*);
void gfob_ev_begin(struct gfob_ev*);
int gfob_ev_add_socket(struct gfob_ev*, int fd, unsigned events);
int gfob_ev_add_signals(struct gfob_ev*, unsigned long mask);
int gfob_ev_add_port(struct gfob_ev*, void* port);
long gfob_ev_run_once(struct gfob_ev*, long usec);
unsigned long gfob_ev_ready(struct gfob_ev*, int slot);
}

#endif // BOOST_ASIO_DETAIL_AMIGA_EVENT_API_HPP
