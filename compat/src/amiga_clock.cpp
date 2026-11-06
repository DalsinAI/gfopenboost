// GFOpenBoost: clock_gettime() under its C++ name. libnix's time.h declares
// it outside its extern "C" block, so C++ that includes <time.h> before any
// Boost header calls a C++-named clock_gettime; this forwards to libnix's.
// Copyright (c) 2026 AmiCradle Team
// Distributed under the Boost Software License, Version 1.0.
#include <time.h>

extern "C" __stdargs int gfopenboost_c_clock_gettime(clockid_t, struct timespec *) __asm__("_clock_gettime");

__stdargs int clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    return gfopenboost_c_clock_gettime(clock_id, tp);
}
