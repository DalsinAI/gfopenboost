//  GFOpenBoost: Boost's platform config for AmigaOS 3 (68040) with libnix,
//  its newlib headers and the stoves' pthreads. It takes the place of
//  Boost's own amigaos.hpp, which turns threads and std::wstring off; the
//  stoves' C++ library has both (_GLIBCXX_HAS_GTHREADS, _GLIBCXX_USE_WCHAR_T).
//
//  One config for every port. It merges what KiCad
//  (compat/boost_amiga_platform.hpp), Poedit (boost/amigaos.hpp,
//  amiga-nocancel.h), aMule (Asio) and FileZilla (Regex) each carried.
//
//  Copyright (c) 2026 AmiCradle Team
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_PLATFORM "AmigaOS"

// pthread.h's cancellation-point macros ("#define read(...)
// (pthread_testcancel(), read(__VA_ARGS__))") break every C++ member of
// those names (iostreams' read and close, chrono's read). This takes
// unistd.h and pthread.h in once and drops the macros, before
// posix_features.hpp includes unistd.h.
#include <boost/config/platform/amigaos_nocancel.hpp>

#define BOOST_HAS_UNISTD_H
#include <boost/config/detail/posix_features.hpp>

#ifndef BOOST_HAS_PTHREADS
#  define BOOST_HAS_PTHREADS
#endif
#ifndef BOOST_HAS_THREADS
#  define BOOST_HAS_THREADS
#endif
#define BOOST_HAS_GETTIMEOFDAY
#define BOOST_HAS_SCHED_YIELD
#define BOOST_HAS_NANOSLEEP

// Atomics: 8-, 16- and 32-bit go through GCC's libatomic calls (the chip
// bus cannot do CAS), so link -latomic; see patches/headers (caps_gcc_atomic).

// No dlopen() and no backtrace(): stack traces are the noop backend.
#ifndef BOOST_STACKTRACE_USE_NOOP
#  define BOOST_STACKTRACE_USE_NOOP
#endif

// Boost.Asio. A bsdsocket.library base belongs to the task that opened it,
// so Asio runs its io_context on one task: no internal threads or locking.
// There is no termios serial device. Define BOOST_ASIO_AMIGA_THREADS before
// any Boost header to keep Asio's locking (each task then needs its own
// socket base).
#if !defined(BOOST_ASIO_AMIGA_THREADS) && !defined(BOOST_ASIO_DISABLE_THREADS)
#  define BOOST_ASIO_DISABLE_THREADS
#endif
// libnix has signal() but no sigaction()
#ifndef BOOST_ASIO_DISABLE_SIGACTION
#  define BOOST_ASIO_DISABLE_SIGACTION
#endif
#ifndef BOOST_ASIO_DISABLE_SERIAL_PORT
#  define BOOST_ASIO_DISABLE_SERIAL_PORT
#endif
// The reactor waits through event.library 1 (LIBS:): sockets, Exec signals
// and MsgPorts in one wait (boost/asio/amiga/exec_waiter.hpp). Only for the
// one-task build; BOOST_ASIO_AMIGA_NO_EVENT keeps the plain select() reactor.
#if defined(BOOST_ASIO_DISABLE_THREADS) && !defined(BOOST_ASIO_AMIGA_NO_EVENT) \
  && !defined(BOOST_ASIO_HAS_AMIGA_EVENT)
#  define BOOST_ASIO_HAS_AMIGA_EVENT 1
#endif

// libstdc++'s std::regex recurses once a character (GCC bug 86164), so use
// Boost.Regex for anything long; it is header-only here.
