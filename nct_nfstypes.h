/*
 * Copyright (c) 2014-2015 Greg Becker.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: nct_nfstypes.h 292 2015-01-15 13:07:09Z greg $
 */
#ifndef NCT_NFSTYPES_H
#define NCT_NFSTYPES_H

#define NFS3_FHSIZE         (64)

#define NFS3_NULL           (0)
#define NFS3_GETATTR        (1)
#define NFS3_SETATTR        (2)
#define NFS3_LOOKUP         (3)
#define NFS3_ACCESS         (4)
#define NFS3_READLINK       (5)
#define NFS3_READ           (6)
#define NFS3_WRITE          (7)
#define NFS3_CREATE         (8)
#define NFS3_MKDIR          (9)
#define NFS3_SYMLINK        (10)
#define NFS3_MKNOD          (11)
#define NFS3_REMOVE         (12)
#define NFS3_RMDIR          (13)
#define NFS3_RENAME         (14)
#define NFS3_LINK           (15)
#define NFS3_READDIR        (16)
#define NFS3_READDIRPLUS    (17)
#define NFS3_FSSTAT         (18)
#define NFS3_FSINFO         (19)
#define NFS3_PATHCONF       (20)
#define NFS3_COMMIT         (21)

typedef u_quad_t    uint64;
typedef quad_t      int64;
typedef u_long      uint32;
typedef long        int32;

typedef uint64      fileid3;
typedef uint32      uid3;
typedef uint32      gid3;
typedef uint64      size3;
typedef uint64      offset3;
typedef uint32      mode3;
typedef uint32      count3;

enum ftype3 {
    NF3REG      = 1,
    NF3DIR      = 2,
    NF3BLK      = 3,
    NF3CHR      = 4,
    NF3LNK      = 5,
    NF3SOCK     = 6,
    NF3FIFO     = 7,
};

typedef enum ftype3 ftype3;

enum nfsstat3 {
    NFS3_OK             = 0,
    NFS3ERR_PERM        = 1,
    NFS3ERR_NOENT       = 2,
    NFS3ERR_IO          = 5,
    NFS3ERR_NXIO        = 6,
    NFS3ERR_ACCES       = 13,
    NFS3ERR_EXIST       = 17,
    NFS3ERR_XDEV        = 18,
    NFS3ERR_NODEV       = 19,
    NFS3ERR_NOTDIR      = 20,
    NFS3ERR_ISDIR       = 21,
    NFS3ERR_INVAL       = 22,
    NFS3ERR_FBIG        = 27,
    NFS3ERR_NOSPC       = 28,
    NFS3ERR_ROFS        = 30,
    NFS3ERR_MLINK       = 31,
    NFS3ERR_NAMETOOLONG = 63,
    NFS3ERR_NOTEMPTY    = 66,
    NFS3ERR_DQUOT       = 69,
    NFS3ERR_STALE       = 70,
    NFS3ERR_REMOTE      = 71,
    NFS3ERR_BADHANDLE   = 10001,
    NFS3ERR_NOT_SYNC    = 10002,
    NFS3ERR_BAD_COOKIE  = 10003,
    NFS3ERR_NOTSUPP     = 10004,
    NFS3ERR_TOOSMALL    = 10005,
    NFS3ERR_SERVERFAULT = 10006,
    NFS3ERR_BADTYPE     = 10007,
    NFS3ERR_JUKEBOX     = 10008,
};

typedef enum nfsstat3 nfsstat3;

struct nfs_fh3 {
    struct {
        u_int data_len;
        char *data_val;
    } data;
};

struct specdata3 {
    uint32 specdata1;
    uint32 specdata2;
};

typedef struct specdata3 specdata3;

struct nfstime3 {
    uint32 seconds;
    uint32 nseconds;
};

typedef struct nfstime3 nfstime3;



typedef struct nfs_fh3 nfs_fh3;

typedef struct {
    u_int fhandle3_len;
    char *fhandle3_val;
} fhandle3;

struct getattr3_args {
    nfs_fh3 object;
};

struct fattr3 {
    ftype3      type;
    mode3       mode;
    uint32      nlink;
    uid3        uid;
    gid3        gid;
    size3       size;
    size3       used;
    specdata3   rdev;
    uint64      fsid;
    fileid3     fileid;
    nfstime3    atime;
    nfstime3    mtime;
    nfstime3    ctime;
};

typedef struct fattr3 fattr3;



typedef struct getattr3_args getattr3_args;

struct getattr3_resok {
    fattr3 obj_attributes;
};

typedef struct getattr3_resok getattr3_resok;

struct getattr3_res {
    nfsstat3 status;
    union {
        getattr3_resok resok;
    } u;
};

typedef struct getattr3_res getattr3_res;

struct read3_args {
    nfs_fh3     file;
    offset3     offset;
    count3      count;
};

typedef struct read3_args read3_args;


#endif // NCT_NFSTYPES_H
