# GFOpenBoost

One Boost for every Amiga port: one pinned release, one Amiga config, and
Boost's separately compiled parts built once as link libraries for
AmigaOS 3 on the 68040 (soft float; the stoves' specs add the run-time FPU
check).

Boost itself is not kept here. `build-amiga.sh` fetches the release that
`boost.lock` pins (1.89.0, checked by SHA-256), puts our Amiga config and
edits over it, and builds.

## Building

    STOVE=.../stoves/os32-gcc16-open6 OUT=out ./build-amiga.sh

The work tree is `~/.cache/gfopenboost/build` (`WORK=` to change it). A run
that reaches the time cap (`BUDGET_TOTAL`, 780 s) stops cleanly and the next
run goes on from its objects. The result is `WORK/sdk`, packed as
`OUT/gfopenboost-1.89.0-sdk.tar.xz`:

| Path | What |
| --- | --- |
| `include/boost/` | Boost 1.89.0's headers with the Amiga config and edits |
| `lib/libboost_<part>.a` | one link library a compiled part |
| `lib/cmake/Boost-1.89.0/` | a CMake package: `find_package(Boost 1.89 CONFIG COMPONENTS ...)` gives `Boost::headers` and `Boost::<part>` |
| `boost.txt` | the parts built, with sizes |

`OUT/boosttest` uses every part; run it in a lab (`T:` must be writable).
`tools/asio-check/build-amiga.sh` also links `UDPLT` (`tests/udplt.cpp`):
UDP as libtorrent-rasterbar uses it (bind with reuse_address, non-blocking,
`async_wait(wait_read)` then `receive_from` until `would_block`, the LSD
multicast group and `SIOCGIFCONF` as INFO lines). It builds on the host as well:
`g++ -std=c++17 tests/udplt.cpp -pthread`. Both need `event.library` in
`LIBS:`; without it every Asio socket call fails with ENOSYS.

## Using it

    m68k-amigaos-g++ -noixemul -m68040 -I$SDK/include ... \
        -L$SDK/lib -lboost_filesystem -lboost_thread ... -lpthread -latomic

- Link `-latomic` with anything that uses Boost.Atomic, Thread or
  Filesystem (see Atomics below), `-lpthread` with Thread, `-lsocket` with
  Asio, and a zlib (z.library's stubs) with Iostreams' zlib/gzip filters.
- Asio: define `BOOST_ASIO_SEPARATE_COMPILATION` to use the compiled Asio in
  `libboost_asio.a` (the CMake target does). Header-only Asio still needs
  `libboost_asio.a` for the network calls libnix lacks.
- Nothing else is needed on the command line: the Amiga config is Boost's
  own `boost/config/platform/amigaos.hpp`, which Boost picks by itself.

## Compiled parts

atomic, charconv, chrono, container, date_time, filesystem, iostreams
(file_descriptor, zlib, gzip), json, locale, program_options, random,
regex (header-only; an empty library so `-lboost_regex` still links),
serialization,
wserialization, stacktrace_noop, thread, timer, url, asio (separate
compilation) and system (header-only; an empty library so `-lboost_system`
still links).

Boost.Regex's C API (`regcomp`/`regexec`, `posix_api.cpp` and
`wide_posix_api.cpp`) is not built: C callers use `<regex.h>` and `-lregex`
from regex.library (GFOpenRegex), so there is one regex engine for C code.

Not built: context, coroutine, fiber and cobalt (need m68k context-switch
code), log, test, wave, graph, graph_parallel, mpi, python, process, nowide
and iostreams' mapped_file, bzip2, lzma and zstd. Ask for one when a port
needs it.

## The Amiga config

`config/boost/config/platform/` replaces Boost's own `amigaos.hpp`, which
turns threads and `std::wstring` off. It merges what each port carried:

| From | What |
| --- | --- |
| KiCad (`compat/boost_amiga_platform.hpp`) | pthreads, gettimeofday, sched_yield; Thread's condition variables on the real-time clock |
| Poedit (`boost/amigaos.hpp`, `amiga-nocancel.h`) | posix_features, and pthread.h's cancellation-point macros (`read`, `close`, ...) dropped before any C++ uses those names |
| aMule | Asio on one task (`BOOST_ASIO_DISABLE_THREADS`: a bsdsocket base belongs to one task), no serial ports, the socket-pair select interrupter, the POSIX network names libnix lacks (IPv4 only), and POSIX `ioctl()`/`fcntl()` for sockets (`compat/src/amiga_ioctl.c`: libnix's `ioctl()` takes ixemul's five-argument form, so Asio's `ioctl(fd, FIONBIO, &on)` failed with EFAULT; its `fcntl()` is a stub) |
| FileZilla | Boost.Regex in place of libstdc++'s `std::regex` (GCC bug 86164) |

`patches/headers/` edits Boost's headers:

- Thread: no `CLOCK_MONOTONIC` (no `pthread_condattr_setclock()`).
- Atomic: 8-, 16- and 32-bit atomics through GCC's libatomic calls. GCC says
  these are only "sometimes" lock free on m68k because the Amiga's chip bus
  cannot do CAS/TAS cycles, so it calls libatomic, which guards each
  operation; Boost.Atomic would otherwise refuse to build.
- Asio: waits through event.library (GFOpenEvent 1.1, `LIBS:`): one
  `WaitSelect()` with a signal mask covers the program's sockets, timers,
  Exec signals and MsgPorts, and a wake is a signal, not a socket pair
  (`detail/amiga_event_reactor.hpp`, built from Asio's select reactor; C
  calls in `compat/src/amiga_event.c`). The network names come from
  `amigaos_net.hpp`. Programs need event.library 1 in `LIBS:`; without it
  the first socket or timer throws `system_error` whose what() starts
  "event.library (version 1) not found in LIBS:" (its code is ENOSYS), and
  that line goes to stderr once as well.
- Asio's io_context executor keeps its two flags apart from the
  io_context pointer (patch 0004): upstream hides them in the pointer's low
  bits, and a 68k io_context can sit on a 2-byte boundary. Accepted and
  assigned sockets use FIONBIO, not fcntl (patch 0003).
- Asio's `async_resolve` on one task (patch 0005): upstream hands each
  lookup to a worker thread, which the one-task build can't start (the
  constructor threw "thread: Operation not supported", or the lookup came
  back `operation_not_supported`). The lookup now runs where it is started,
  blocking as `getaddrinfo()` does, and its handler is posted to the
  io_context like any other completion. libtorrent looks up trackers this
  way.

### Asio and the rest of the Amiga

`boost/asio/amiga/exec_waiter.hpp` waits for Exec signals or a MsgPort in
the same io_context, so an Intuition window, an ARexx port or a commodity
runs in the program's one loop, with no polling timer:

    boost::asio::io_context io;
    boost::asio::exec_waiter w(io);
    std::function<void()> wait_window = [&] {
        w.async_wait_port(win->UserPort, [&](boost::system::error_code ec, unsigned long) {
            if (ec) return;
            while (auto *m = (struct IntuiMessage *)GetMsg(win->UserPort)) { /* ... */ ReplyMsg(&m->ExecMessage); }
            wait_window();
        });
    };
    wait_window();
    w.async_wait_signals(SIGBREAKF_CTRL_C, [&](boost::system::error_code, unsigned long) { io.stop(); });
    io.run();

`async_wait_signals(mask, h)` calls `h(ec, got)` with the signals that
came (the loop takes them, as `Wait()` does); `async_wait_port(port, h)`
calls `h(ec, 1)` when a message is there, and leaves it on the port for
the handler. Each wait is one-shot; `cancel()` (and the waiter's
destructor) ends them with `operation_aborted`. C++14 or later. The event
reactor is for the one-task build (the default); with
`BOOST_ASIO_AMIGA_THREADS`, Asio keeps its `select()` reactor.

`patches/src/` edits compiled sources: Atomic's lock pool aligns to 8
bytes, the most an Amiga hunk file allows, and Filesystem's `/` adds no
separator after a volume's colon (`"Work:" / "dir"` is `Work:dir`).

`patches/parts/PART/` edits one part's sources, and a change there rebuilds
only that part: Locale converts charsets without iconv or ICU (UTF-8,
US-ASCII, ISO-8859-1, ISO-8859-15 and windows-1252, from our own tables) in
`conv::to_utf`, `from_utf`, `between` and the converter classes. Timer
takes wall time from `gettimeofday()` (libnix's `times()` returns CPU
milliseconds while `sysconf(_SC_CLK_TCK)` says 50, so `cpu_timer` read
twenty times too long).

`compat/src/` adds what libnix lacks, inside the part that needs it:
`statvfs()` on dos.library `Info()` and `fchmod()` on `NameFromFH()` (Filesystem),
a `clock_gettime()` on `gettimeofday()` (Thread and Chrono: libnix's reads
the local-time Amiga clock without the time zone, so Boost's timed waits
missed the pthread library's clock by the zone offset), C++-named
`clock_gettime()`/`statvfs()` forwarders (libnix's headers
declare them without `extern "C"`), and `inet_ntop`/`inet_pton`/`readv`/
`writev`/`if_nametoindex`/`pause` (Asio), and a `select()` for sockets
straight onto bsdsocket.library's `WaitSelect()` (Asio: libnix's own
`select()` never came back from a timed wait on a socket in the lab). The
event.library headers that `amiga_event.c` uses are in `compat/include/`
(as gfopensdk ships them).

## Known limits

- Locale uses its std backend (C locale); charset conversion covers UTF-8,
  US-ASCII, ISO-8859-1, ISO-8859-15 and windows-1252. Other charsets and a
  backend on unicode.library and locale.library are the next step.
- Filesystem paths are libnix's (on dos.library): `Work:dir/file` works and
  `/` joins after a volume, but `root_name()` does not know volumes yet.
- Asio is IPv4 only, as the stack is.
- `random_device` reads `/dev/urandom`, which AmigaOS does not have.

## Next: a shared library

Stage two puts the compiled parts in one LIBS: library loaded the
openwxtoolkit.library way, so programs stop carrying their own copy. Boost
has no stable ABI between releases, so that library carries the version in
its name.

## Licence

Boost and our files here are under the Boost Software License 1.0
(`LICENSE_1_0.txt`).

Copyright (c) 2026 AmiCradle Team
