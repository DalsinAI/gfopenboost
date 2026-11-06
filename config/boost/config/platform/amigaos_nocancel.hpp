//  GFOpenBoost: drops the POSIX cancellation-point macros that the stoves'
//  pthread.h defines ("#define read(...) (pthread_testcancel(),
//  read(__VA_ARGS__))"). They rewrite every C++ function and member of those
//  names: iostreams' read and close, chrono's read, the C++ library's
//  messages::close. pthread.h is taken in once here; its include guard
//  keeps the macros out after. Programs that include <pthread.h> before any
//  Boost header can force this file first with -include.
//
//  Copyright (c) 2026 AmiCradle Team
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONFIG_PLATFORM_AMIGAOS_NOCANCEL_HPP
#define BOOST_CONFIG_PLATFORM_AMIGAOS_NOCANCEL_HPP
#include <unistd.h>
#include <pthread.h>
#undef accept
#undef close
#undef connect
#undef creat
#undef fcntl
#undef fsync
#undef lseek
#undef nanosleep
#undef open
#undef pause
#undef poll
#undef pread
#undef pwrite
#undef read
#undef readv
#undef recv
#undef recvfrom
#undef recvmsg
#undef select
#undef send
#undef sendmsg
#undef sendto
#undef sleep
#undef system
#undef tcdrain
#undef usleep
#undef wait
#undef waitpid
#undef write
#undef writev

// libnix's time.h declares clock_gettime() outside its extern "C" block and
// sys/statvfs.h has none, so C++ would call them by C++ names; take them in
// as C here, before anything else does. (libboost_chrono and
// libboost_filesystem also carry C++-named forwarders, for programs that
// include those headers before any Boost header.)
#ifdef __cplusplus
extern "C" {
#endif
#include <time.h>
#include <sys/statvfs.h>
// libnix has nanosleep() but its time.h declares it only with _POSIX_TIMERS
__stdargs int nanosleep(const struct timespec *, struct timespec *);
#ifdef __cplusplus
}
#endif
#endif
