/* GFOpenBoost: fchmod() for Boost.Filesystem's copy_file(), which libnix
 * lacks. Finds the open file's name with dos.library NameFromFH() and sets
 * the protection bits by name with libnix's chmod().
 * Copyright (c) 2026 AmiCradle Team
 * Distributed under the Boost Software License, Version 1.0. */
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dos/dos.h>
#include <proto/dos.h>

extern BPTR _lx_fhfromfd(int fd);

int fchmod(int fd, mode_t mode)
{
    char name[512];
    BPTR fh = _lx_fhfromfd(fd);
    if (!fh) {
        errno = EBADF;
        return -1;
    }
    if (!NameFromFH(fh, (STRPTR)name, sizeof name)) {
        errno = EIO;
        return -1;
    }
    return chmod(name, mode);
}
