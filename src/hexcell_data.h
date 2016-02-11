/*
 * Copyright (c) 2016 Devoleda Organisation. All rights reserved.
 * Copyright (c) Ethan Levy <eitanlevy97@yandex.com>
 *
 * @DEVOLEDA_OSES_BSD_LICENCE_START@
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
 * THIS SOFTWARE IS PROVIDED BY DEVOLEDA ORGANISATION ``AS IS'' AND
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
 * @DEVOLEDA_OSES_BSD_LICENCE_END@
 *
 */

#ifndef _HEXCELL_DATA_H_
#define _HEXCELL_DATA_H_

typedef struct __HCDataInfoBlock {
    unsigned long long fsSize;    // 8
    unsigned long long realSize;  // 8
    unsigned long      blocks;    // 4
} HCDataInfoBlock;

/* Cell Data Storage Unit:
   +--------------------------------------------+------------+----+------------+----+
   | InfoBlock |           Body Unit0           |    BU1     |....|   BU(N)    |....|
   |  20 Bytes |  (14 + BLKLEN + DATLEN) bytes  |     ~      |....|     ~      |....|
   +-----------+--------------------------------+------------+----+------------+----+
   (*) 'N' must less than 2 ^ 32 - 1 (4,294,967,295)
   Body Unit Structure:
   +---------+------+------+----+------+--------+----+------+--------+----+---------+
   |BID_BEGIN|BLKLEN|DATLEN|BID0|B0_LEN|PROPDATA|BID1|B1_LEN|PROPDATA|....|CELL_DATA|
   |---------+------+------+----+------+--------+----+------+--------+----+---------+
   |    2    |   4  |   8  |  2 |   4  | B0_LEN |  2 |  4   | B1_LEN |....|  DATLEN | 
   +---------+------+------+----+------+--------+----+------+--------+----+---------+
    BLKLEN equals to   =   |<------------------BLKLEN-------------------->|         */

const short BID_PROP_BEGIN            =   0x1DF0
const short BID_PROP_TYPE             =   0x1D0A
const short BID_PROP_SIZE_INCELL      =   0x1D1C
const short BID_PROP_SIZE_ORIGINAL    =   0x1D1D
const short BID_PROP_PATHNAME         =   0x1D2A
const short BID_PROP_LINKNAME         =   0x1D2B
const short BID_PROP_MODE             =   0x1D30
const short BID_PROP_UID              =   0x1D41
const short BID_PROP_GID              =   0x1D52
const short BID_PROP_DEV1             =   0x1D6A
const short BID_PROP_DEV2             =   0x1D6B
//const short BID_PROP_DATA_NULL        =   0x30FF

/* Block types */
//enum { BLK_REG, BLK_HARDLINK, BLK_SYMLINK, BLK_CHARDEV, BLK_BLOCKDEV, BLK_DIR, BLK_FIFO };
const short BLK_REG                   =   0x200B
const short BLK_HARDLINK              =   0x200C
const short BLK_SYMLINK               =   0x200D
const short BLK_CHARDEV               =   0x200E
const short BLK_BLOCKDEV              =   0x201A
const short BLK_DIR                   =   0x201F
const short BLK_FIFO                  =   0x202C

/* Block Property Structure */
typedef struct _HCBlockProperty {
    unsigned char       pathName[1024];
    unsigned char       linkName[1024];
    short               fType;
    unsigned long long  fSize1; // In Cell
    unsigned long long  fSize2; // Original
    mode_t              fMode;
    uid_t               fUID;
    gid_t               fGID;
    unsigned int        dev1;   // Maj
    unsigned int        dev2;   // Min
} HCBlockProperty;

/* Reader thread callback status code */
enum { CB_OK = 0, CB_FINISH, CB_FORCE_QUITED };

/* Progress Callback */
typedef void (*HCReaderThreadCallback) (int, const char *, unsigned long, unsigned long);

/* Function Export */
extern int HCImportPathToCell(int cellfd, const char *path, unsigned long offset, 
    unsigned long long *outFsSize, unsigned long long *outRealSize, unsigned long *outBlocks);

#endif /* _HEXCELL_DATA_H_ */