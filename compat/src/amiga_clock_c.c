/* GFOpenBoost: clock_gettime() that agrees with gettimeofday().
 * libnix's clock_gettime() reads DateStamp() as it stands: the Amiga clock
 * runs on local time, so CLOCK_REALTIME comes out off by the time zone,
 * while libnix's gettimeofday() (and the pthread library, which times
 * pthread_cond_timedwait() with it) corrects for it. A Boost.Thread timed
 * wait then asked for "now + 100 ms" an hour ahead and slept an hour.
 * This one is linked from libboost_thread and libboost_chrono in place of
 * libnix's, so both clocks are the same clock.
 * Copyright (c) 2026 AmiCradle Team
 * Distributed under the Boost Software License, Version 1.0. */
#include <time.h>
#include <sys/time.h>
#include <errno.h>

int clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    struct timeval tv;
    (void)clock_id;
    if (!tp) {
        errno = EFAULT;
        return -1;
    }
    if (gettimeofday(&tv, 0) != 0)
        return -1;
    tp->tv_sec = tv.tv_sec;
    tp->tv_nsec = tv.tv_usec * 1000L;
    return 0;
}
