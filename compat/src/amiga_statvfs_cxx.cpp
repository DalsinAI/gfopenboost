// GFOpenBoost: statvfs() under its C++ name, for C++ that includes
// <sys/statvfs.h> (which has no extern "C") before any Boost header.
// Forwards to amiga_statvfs.c.
// Copyright (c) 2026 AmiCradle Team
// Distributed under the Boost Software License, Version 1.0.
#include <sys/statvfs.h>

extern "C" int gfopenboost_c_statvfs(const char *, struct statvfs *) __asm__("_statvfs");

__stdargs int statvfs(const char *__restrict path, struct statvfs *__restrict buf)
{
    return gfopenboost_c_statvfs(path, buf);
}
