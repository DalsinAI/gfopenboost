/* GFOpenBoost: statvfs() on dos.library, for Boost.Filesystem's space().
 * libnix declares statvfs() in sys/statvfs.h but has none. Lock() the
 * path, Info() its volume, give the sizes in the volume's own blocks.
 * Copyright (c) 2026 AmiCradle Team
 * Distributed under the Boost Software License, Version 1.0. */
#include <errno.h>
#include <string.h>
#include <sys/statvfs.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#ifndef ST_RDONLY
#define ST_RDONLY 1
#endif

int statvfs(const char *path, struct statvfs *st)
{
    struct InfoData *info;
    BPTR lock;
    int rc = -1;

    if (!path || !st) {
        errno = EINVAL;
        return -1;
    }
    /* "" and "." are the current directory, as on POSIX */
    lock = Lock((STRPTR)((path[0] == 0 || strcmp(path, ".") == 0) ? "" : path), SHARED_LOCK);
    if (!lock) {
        errno = ENOENT;
        return -1;
    }
    info = (struct InfoData *)AllocVec(sizeof *info, MEMF_PUBLIC | MEMF_CLEAR);
    if (!info) {
        UnLock(lock);
        errno = ENOMEM;
        return -1;
    }
    if (Info(lock, info)) {
        memset(st, 0, sizeof *st);
        st->f_bsize = st->f_frsize = (unsigned long)info->id_BytesPerBlock;
        st->f_blocks = (unsigned long long)info->id_NumBlocks;
        st->f_bfree = st->f_bavail = (unsigned long long)(info->id_NumBlocks - info->id_NumBlocksUsed);
        st->f_namemax = 107;
        if (info->id_DiskState == ID_WRITE_PROTECTED)
            st->f_flag = ST_RDONLY;
        rc = 0;
    } else {
        errno = EIO;
    }
    FreeVec(info);
    UnLock(lock);
    return rc;
}
