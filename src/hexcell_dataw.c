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

#include <unistd.h>
#include <string.h>
#include <ftw.h>
#include <zlib.h>

#include <hexcell_utils.h>
#include <hexcell_data.h>
#include <hexcell_message.h>

/* Internal Helper Functions Export */
static char *__HCEncodeString(char *pStr);
static int   __HCDataProcessFromPathW(const char *fPath, const struct stat *fStat,
    int typeFlag);
static int __HCLoadFile(const char *path, unsigned long size, unsigned char *buffer);

/* Some important global variables... */
static unsigned long _BeginOffset = -1;
static int curFileHandle = -1;
//static HCDataInfoBlock *curInfoBlock = NULL;
static char *curRootPath = NULL;
static int _HCWLocked = 0; // Prevent chaos from multithreading

static unsigned long long curFsSize = 0L, curRealSize = 0L;
static unsigned long curBlocks = 0L;

int HCImportPathToCell(int cellfd, const char *path, unsigned long offset, 
    unsigned long long *outFsSize, unsigned long long *outRealSize, unsigned long *outBlocks)
{
    HCDataInfoBlock *curInfoBlock = NULL;
    struct stat st;
    int res = 0;

    HCAssert(cellfd > -1 && path && !_HCWLocked, _HCWLocked = 0; return -1);
    if(lstat(path, &st)) {
        pushdeb("in %s: cannot stat source directory\n", __func__);
        res = 2; /* Failed to stat */
        goto __HCIPTC_FAILED;
    }

    /* Initialise all internal variables */
    _BeginOffset = offset;
    curFileHandle = cellfd;
    curRootPath = path;
    curFsSize = 0; curRealSize = 0; curBlocks = 0;
    _HCWLocked = 1;

    if(S_ISLNK(st.st_mode)) {
        pushdeb("in %s: argument \'path\' is a symbolic link which is not supported\n", __func__);
        res = 3; /* Type not expected */
        goto __HCIPTC_FAILED;

    } else if(S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)) {
        lseek(curFileHandle, offset + sizeof(HCDataInfoBlock), SEEK_SET);
        if(S_ISDIR(st.st_mode))
            res = ftw(path, __HCDataProcessFromPathW, 4096);
        else
            res = __HCDataProcessFromPathW(path, &st, 0);
        if(res) {
            pushdeb("in %s: failed to scan the path\n", __func__);
            goto __HCIPTC_FAILED;
        }

        HCCalloc(curInfoBlock, 1, sizeof(HCDataInfoBlock), res = 5; goto __HCIPTC_FAILED);
        curInfoBlock->fsSize = curFsSize;
        curInfoBlock->realSize = curRealSize;
        curInfoBlock->blocks = curBlocks;

        /* Seek back to the original position and write info block */
        lseek(curFileHandle, _BeginOffset, SEEK_SET);
        if(HCWriteFileX(curFileHandle, curInfoBlock, sizeof(HCDataInfoBlock))) {
            pushdeb("in %s: failed to write info block\n", __func__);
            free(curInfoBlock);
            goto __HCIPTC_FAILED;
        }
        if(*outFsSize) *outFsSize = curFsSize;
        if(*outRealSize) *outRealSize = curRealSize;
        if(*outBlocks) *outBlocks = curBlocks;
    }

__HCIPTC_FAILED:
    _HCWLocked = 0;
    return res;
}

static int __HCLoadFile(const char *path, unsigned long size, unsigned char *buffer)
{
    FILE *fp = NULL;
    unsigned long sizeRead = 0L;

    if(!(fp = fopen(path, "rb")) || !size) return -1;
    if((sizeRead = fread(buffer, 1, size, fp)) != size) {
        fclose(fp);
        return -2;
    }

    return 0;
}

/* Before calling this function, make sure we have correct offset for infoblock */
static int __HCDataProcessFromPathW(const char *fPath, const struct stat *fStat,
    int typeFlag)
{
    HCblockProperty *tProperty = NULL;
    mode_t fMode = fStat->st_mode;
    unsigned long _BlockLen = 0;
    unsigned long long _DataLen = 0;
    int _RtcCounter = 0;
    unsigned char *tempBuffer = NULL;

    HCAssert(_BeginOffset > -1 && curFileHandle > -1 && curInfoBlock && curRootPath, return -1);
    HCCalloc(tProperty, 1, sizeof(HCBlockProperty), return -2); // Memory failure
    //tProperty->begin = BID_PROP_BEGIN;

    if(curRootPath[strlen(curRootPath)] != '/')
        snprintf(tProperty->pathName, 1024, "%s", fPath + strlen(curRootPath) + 1);
    else
        snprintf(tProperty->pathName, 1024, "%s", fPath + strlen(curRootPath));
    tProperty->fMode = fMode & 0777;
    tProperty->fUID = fStat->st_uid;
    tProperty->fGID = fStat->st_gid;
    tProperty->fSize2 = fStat->st_size;

    if(S_ISREG(fMode)) {
        tProperty->fType = BLK_REG;
        {
            unsigned long long SourceLen = fStat->st_size;
            unsigned long      CompressedLen = 0L;
            unsigned char *_SourceBuffer = calloc(1, SourceLen),
                           _CompressBuffer = NULL;
            if(!_SourceBuffer) {
                pushdeb("in %s: failed to allocate memory\n", __func__);
                free(tProperty);
                return -2;
            }
            /* Predict how many memory we need */
            CompressedLen = compressBound(SourceLen);
            HCCalloc(_CompressBuffer, 1, CompressedLen,
                pushdeb("in %s: failed to allocate memory\n", __func__);
                free(tProperty);
                free(_SourceBuffer);
                return -2);
            int zRes = compress2(_CompressBuffer, &CompressedLen, _SourceBuffer, SourceLen, 9);
            if(zRes) {
                pushdeb("in %s: Failed to compress data, compressor returned 0x%8x\n", zRes);
                free(tProperty);
                free(_SourceBuffer);
                return -3;
            }
            tProperty->fSize1 = CompressedLen;

            /* Calculate BLKLEN and DATLEN */
            _BlockLen = sizeof(short) + sizeof(int) + sizeof(short) + // BID_TYPE
                        sizeof(short) + sizeof(int) + sizeof(unsigned long) + // BID_INCELL
                        sizeof(short) + sizeof(int) + sizeof(unsigned long long) + //BID_ORIGINAL
                        sizeof(short) + sizeof(int) + sizeof(char) * strlen(tProperty->pathName) + //BID_PATHNAME
                        //sizeof(short) + sizeof(int) + sizeof(short) + //BID_LINKNAME
                        sizeof(short) + sizeof(int) + sizeof(mode_t) + //BID_MODE
                        sizeof(short) + sizeof(int) + sizeof(uid_t) + //BID_UID
                        sizeof(short) + sizeof(int) + sizeof(gid_t); //BID_GID
                        //sizeof(short) + sizeof(int) + sizeof(short) + //BID_DEV1
                        //sizeof(short) + sizeof(int) + sizeof(short);  //BID_DEV2
            _DataLen = CompressedLen;

            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_BEGIN, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_BlockLen, sizeof(unsigned long));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_DataLen, sizeof(unsigned long long));

            /* BID_INCELL */
            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_SIZE_INCELL, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(unsigned long), sizeof(int));
            _RtcCounter += HCWriteFileX(curFileHandle, tProperty->fSize1, sizeof(unsigned long)); // CompressedLen

            /* BID_ORIGINAL */
            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_SIZE_ORIGINAL, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(unsigned long long), sizeof(int));
            _RtcCounter += HCWriteFileX(curFileHandle, tProperty->fSize2, sizeof(unsigned long long));

            _RtcCounter += HCWriteFileX(curFileHandle, _CompressBuffer, _DataLen);

            /* Clean up ... */
            free(_CompressBuffer);
            free(_SourceBuffer);
        }

    } else if(S_ISDIR(fMode)) {
        tProperty->fType = BLK_DIR;
        /* Nothing to do, just write */
        _BlockLen = sizeof(short) + sizeof(int) + sizeof(short) + // BID_TYPE
                    sizeof(short) + sizeof(int) + sizeof(unsigned long long) + //BID_ORIGINAL
                    sizeof(short) + sizeof(int) + sizeof(char) * strlen(tProperty->pathName) + //BID_PATHNAME
                    sizeof(short) + sizeof(int) + sizeof(mode_t) + //BID_MODE
                    sizeof(short) + sizeof(int) + sizeof(uid_t) + //BID_UID
                    sizeof(short) + sizeof(int) + sizeof(gid_t); //BID_GID
        _DataLen = 0;

        {
            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_BEGIN, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_BlockLen, sizeof(unsigned long));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_DataLen, sizeof(unsigned long long));

            /* BID_ORIGINAL */
            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_SIZE_ORIGINAL, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(unsigned long long), sizeof(int));
            _RtcCounter += HCWriteFileX(curFileHandle, tProperty->fSize2, sizeof(unsigned long long));
        }

    } else if(S_ISLNK(fMode)) {
        tProperty->fType = BLK_SYMLINK;
        /* Here count symbolic link size as 0 */
        CALLOC(tempBuffer, 1, 1024, free(tProperty); return -2);
        if(readlink(fPath, tempBuffer, 1024) < 0) {
            pushdeb("in %s: Failed to read link\n", __func__);
            free(tProperty);
            return -4;
        }
        if(!strncmp(curRootPath, tempBuffer, strlen(curRootPath)))
            /* We need fix the target to right place */
            if(curRootPath[strlen(curRootPath)] != '/')
                snprintf(tProperty->linkName, 1024, "%s", tempBuffer + strlen(curRootPath) + 1);
            else
                snprintf(tProperty->linkName, 1024, "%s", tempBuffer + strlen(curRootPath));
        else
            strcpy(tProperty->linkName, tempBuffer);

        free(tempBuffer);
        {
            _BlockLen = sizeof(short) + sizeof(int) + sizeof(short) + // BID_TYPE
                        //sizeof(short) + sizeof(int) + sizeof(unsigned long) +
                        //sizeof(short) + sizeof(int) + sizeof(unsigned long long) +
                        sizeof(short) + sizeof(int) + sizeof(char) * strlen(tProperty->pathName) + //BID_PATHNAME
                        sizeof(short) + sizeof(int) + sizeof(char) * strlen(tProperty->linkName) +//BID_LINKNAME
                        sizeof(short) + sizeof(int) + sizeof(mode_t) + //BID_MODE
                        sizeof(short) + sizeof(int) + sizeof(uid_t) + //BID_UID
                        sizeof(short) + sizeof(int) + sizeof(gid_t);  //BID_GID
                        //sizeof(short) + sizeof(int) + sizeof(short) + //BID_DEV1
                        //sizeof(short) + sizeof(int) + sizeof(short);  //BID_DEV2
            _DataLen = 0;
            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_BEGIN, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_BlockLen, sizeof(unsigned long));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_DataLen, sizeof(unsigned long long));

            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_LINKNAME, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (int *)&strlen(tProperty->linkName), sizeof(int));
            _RtcCounter += HCWriteFileX(curFileHandle, HCCharSwap(tProperty->linkName), strlen(tProperty->linkName));
        }

    } else if(S_ISCHR(fMode)) {
        tProperty->fType = BLK_CHARDEV;
        tProperty->dev1 = major(fStat->st_dev);
        tProperty->dev2 = minor(fStat->st_dev);
        {
            _BlockLen = sizeof(short) + sizeof(int) + sizeof(short) + // BID_TYPE
                        //sizeof(short) + sizeof(int) + sizeof(unsigned long) + // BID_INCELL
                        //sizeof(short) + sizeof(int) + sizeof(unsigned long long) + //BID_ORIGINAL
                        sizeof(short) + sizeof(int) + sizeof(char) * strlen(tProperty->pathName) + //BID_PATHNAME
                        //sizeof(short) + sizeof(int) + sizeof(short) + //BID_LINKNAME
                        sizeof(short) + sizeof(int) + sizeof(mode_t) + //BID_MODE
                        sizeof(short) + sizeof(int) + sizeof(uid_t) + //BID_UID
                        sizeof(short) + sizeof(int) + sizeof(gid_t) + //BID_GID
                        sizeof(short) + sizeof(int) + sizeof(unsigned int) + //BID_DEV1
                        sizeof(short) + sizeof(int) + sizeof(unsigned int);  //BID_DEV2
            _DataLen = 0;
            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_BEGIN, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_BlockLen, sizeof(unsigned long));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_DataLen, sizeof(unsigned long long));

            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_DEV1, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(unsigned int), sizeof(int));
            _RtcCounter += HCWriteFileX(curFileHandle, tProperty->dev1, sizeof(unsigned int));

            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_DEV2, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(unsigned int), sizeof(int));
            _RtcCounter += HCWriteFileX(curFileHandle, tProperty->dev2, sizeof(unsigned int));
        }

    } else if(S_ISBLK(fMode)) {
        tProperty->fType = BLK_BLOCKDEV;
        tProperty->dev1 = major(fStat->st_dev);
        tProperty->dev2 = minor(fStat->st_dev);
        {
            _BlockLen = sizeof(short) + sizeof(int) + sizeof(short) + // BID_TYPE
                        //sizeof(short) + sizeof(int) + sizeof(unsigned long) + // BID_INCELL
                        //sizeof(short) + sizeof(int) + sizeof(unsigned long long) + //BID_ORIGINAL
                        sizeof(short) + sizeof(int) + sizeof(char) * strlen(tProperty->pathName) + //BID_PATHNAME
                        //sizeof(short) + sizeof(int) + sizeof(short) + //BID_LINKNAME
                        sizeof(short) + sizeof(int) + sizeof(mode_t) + //BID_MODE
                        sizeof(short) + sizeof(int) + sizeof(uid_t) + //BID_UID
                        sizeof(short) + sizeof(int) + sizeof(gid_t) + //BID_GID
                        sizeof(short) + sizeof(int) + sizeof(unsigned int) + //BID_DEV1
                        sizeof(short) + sizeof(int) + sizeof(unsigned int);  //BID_DEV2
            _DataLen = 0;
            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_BEGIN, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_BlockLen, sizeof(unsigned long));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_DataLen, sizeof(unsigned long long));

            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_DEV1, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(unsigned int), sizeof(int));
            _RtcCounter += HCWriteFileX(curFileHandle, tProperty->dev1, sizeof(unsigned int));

            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_DEV2, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(unsigned int), sizeof(int));
            _RtcCounter += HCWriteFileX(curFileHandle, tProperty->dev2, sizeof(unsigned int));
        }

    } else if(S_ISFIFO(fMode)) {
        tProperty->fType = BLK_FIFO;
        {
            _BlockLen = sizeof(short) + sizeof(int) + sizeof(short) + // BID_TYPE
                        //sizeof(short) + sizeof(int) + sizeof(unsigned long) + // BID_INCELL
                        //sizeof(short) + sizeof(int) + sizeof(unsigned long long) + //BID_ORIGINAL
                        sizeof(short) + sizeof(int) + sizeof(char) * strlen(tProperty->pathName) + //BID_PATHNAME
                        //sizeof(short) + sizeof(int) + sizeof(short) + //BID_LINKNAME
                        sizeof(short) + sizeof(int) + sizeof(mode_t) + //BID_MODE
                        sizeof(short) + sizeof(int) + sizeof(uid_t) + //BID_UID
                        sizeof(short) + sizeof(int) + sizeof(gid_t); //BID_GID
                        //sizeof(short) + sizeof(int) + sizeof(unsigned int) + //BID_DEV1
                        //sizeof(short) + sizeof(int) + sizeof(unsigned int);  //BID_DEV2
            _DataLen = 0;
            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_BEGIN, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_BlockLen, sizeof(unsigned long));
            _RtcCounter += HCWriteFileX(curFileHandle, (void *)&_DataLen, sizeof(unsigned long long));

            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_DEV1, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(unsigned int), sizeof(int));
            _RtcCounter += HCWriteFileX(curFileHandle, tProperty->dev1, sizeof(unsigned int));

            _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_DEV2, sizeof(short));
            _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(unsigned int), sizeof(int));
            _RtcCounter += HCWriteFileX(curFileHandle, tProperty->dev2, sizeof(unsigned int));

            curFsSize += sizeof(short) + sizeof(unsigned long) + sizeof(unsigned long long) +
                _BlockLen + _DataLen;
            curRealSize += tProperty->fSize2;
        }
    }

    /* Update global counters ... */
    curFsSize += sizeof(short) + sizeof(unsigned long) + sizeof(unsigned long long) +
        _BlockLen + _DataLen;
    curRealSize += tProperty->fSize2;
    curBlocks++;

    /* BID_TYPE */
    _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_TYPE, sizeof(short));
    _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(short), sizeof(int));
    _RtcCounter += HCWriteFileX(curFileHandle, tProperty->fType, sizeof(short));

    /* BID_PATHNAME */
    _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_PATHNAME, sizeof(short));
    _RtcCounter += HCWriteFileX(curFileHandle, (int *)&strlen(tProperty->pathName), sizeof(int));
    _RtcCounter += HCWriteFileX(curFileHandle, __HCEncodeString(tProperty->pathName), strlen(tProperty->pathName));

    /* BID_MODE */
    _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_MODE, sizeof(short));
    _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(mode_t), sizeof(int));
    _RtcCounter += HCWriteFileX(curFileHandle, tProperty->fMode, sizeof(mode_t));

    /* BID_UID */
    _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_UID, sizeof(short));
    _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(uid_t), sizeof(int));
    _RtcCounter += HCWriteFileX(curFileHandle, tProperty->fUID, sizeof(uid_t));

    /* BID_GID */
    _RtcCounter += HCWriteFileX(curFileHandle, &BID_PROP_GID, sizeof(short));
    _RtcCounter += HCWriteFileX(curFileHandle, (int *)&sizeof(gid_t), sizeof(int));
    _RtcCounter += HCWriteFileX(curFileHandle, tProperty->fGID, sizeof(gid_t));

    if(_RtcCounter) {
        pushdeb("in %s: Failed to write properties, IO error\n", __func__);
        free(tProperty);
        return -3;
    }

    free(tProperty);
    return 0;
}

static char *__HCEncodeString(char *pStr)
{
    int i;

    if(pStr && strlen(pStr))
        for(i = 0; i < strlen(pStr); i++)
            pStr[i] = (HCCharSwap(block->ename[i]) ^ 0x0A5E921F);

    return pStr;
}