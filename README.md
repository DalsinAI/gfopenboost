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
| aMule | Asio on one task (`BOOST_ASIO_DISABLE_THREADS`: a bsdsocket base belongs to one task), no serial ports, the socket-pair select interrupter, the POSIX network names libnix lacks (IPv4 only) |
| FileZilla | Boost.Regex in place of libstdc++'s `std::regex` (GCC bug 86164) |

`patches/headers/` edits Boost's headers:

- Thread: no `CLOCK_MONOTONIC` (no `pthread_condattr_setclock()`).
- Atomic: 8-, 16- and 32-bit atomics through GCC's libatomic calls. GCC says
  these are only "sometimes" lock free on m68k because the Amiga's chip bus
  cannot do CAS/TAS cycles, so it calls libatomic, which guards each
  operation; Boost.Atomic would otherwise refuse to build.
- Asio: waits with `select()` (as on Symbian), wakes with a socket pair,
  and takes the network names from `amigaos_net.hpp`.

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
`select()` never came back from a timed wait on a socket in the lab).

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
