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
#include <pthread.h>
#include <zlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include <hexcell_utils.h>
#include <hexcell_message.h>
#include <hexcell_data.h>
#include <hexcell_progress.h>

/* Type Definitions */
typedef struct _HCQueue {
    int size;
    int writep;
    int readp;
    pthread_mutex_t mutex;
    pthread_cond_t  full;
    pthread_cond_t  empty;
    void **data;
} HCQueue;

typedef struct _HCReaderThreadParam {
    unsigned long totalBlocks;
    int fd;
    HCQueue *queue;
} HCReaderThreadParam;

typedef struct _HCWriterThreadParam {
    pthread_t ofReader;
    HCQueue * frmReader;
    HCReaderThreadCallback callback;
} HCWriterThreadParam;

typedef struct _HCWriterQueueDataParam {
    int status;
    HCBlockProperty *property;
    void *data;
} HCWriterQueueDataParam;

static char    *__HCDecodeString(char *pStr);
static HCQueue *__HCQueueInitialise(int size);
static void     __HCQueueDestroy(HCQueue **queue);
static void     __HCWriterQueueDataParamDestroy(HCWriterQueueDataParam **param);

/* Internal Reader Thread Kill Signal */
static int __InternalReaderKillRequestCounter = 0;

int HCExportPathFromCell(int cellfd, const char *prefix, unsigned long offset)
{
#if defined(LINUX) || defined(FREEBSD) || defined(PATRON) || defined(DARWIN)
    int cores = (int)sysconf(_SC_NPROCESSORS_CONF);
    pthread_t tids[cores + 1];
#else
    int cores = -1;
#endif
    HCQueue *aWriterQueue = NULL;
    HCReaderThreadParam *ReaderParam = NULL;
    HCWriterThreadParam *WriterParam = NULL;
    HCDataInfoBlock *InfoBlock = NULL;

    HCAssert(cellfd > -1, return -1); // Bad file descriptor
    if(offset > 0)
        lseek(cellfd, offset, SEEK_SET);
    pushdeb("Current available CPU cores: %d %s\n", cores, cores == -1 ? "[Platform Not Supported]" : "");
    if(cores <= 0) {
        pushdeb("in %s: Invalid CPU configuration, exit now\n", __func__);
        return -2; /* ERR_SYSTEM */
    }

    if(!(ReaderParam = calloc(1, sizeof(HCReaderThreadParam)))) {
        pushdeb("in %s: failed to allocate memory for thread parameters\n", __func__);
        return -3 /* ERR_MEM */
    }
    if(!(WriterParam = calloc(1, sizeof(HCWriterThreadParam)))) {
        pushdeb("in %s: failed to allocate memory for thread parameters\n", __func__);
        return -3 /* ERR_MEM */
    }
    if(!(aWriterQueue = __HCQueueInitialise(cores * 2))) {
        pushdeb("in %s: failed to allocate memory for internal queue\n", __func__);
        return -3;
    }

    /* Read InfoBlock and setting up parameters */
    InfoBlock = malloc(sizeof(HCDataInfoBlock));
    if(HCReadFileX(cellfd, InfoBlock, sizeof(HCDataInfoBlock))) {
        pushdeb("in %s: failed to read infoblock, I/O error\n", __func__);
        free(ReaderParam); free(WriterParam);
        return -4; /* ERR_IO */
    }
    ReaderParam->totalBlocks = InfoBlock->blocks;
    pushdeb("Totally %lu blocks in the cell, Install size: %llu\n", InfoBlock->blocks, InfoBlock->realSize);
    ReaderParam->fd = cellfd;
    ReaderParam->queue = aWriterQueue;

}

#define __HCReaderExitActions(full, mutex, ep) do { pthread_cond_broadcast(full); \
    pthread_mutex_unlock(mutex); goto ep; } while(0)
static void __HCReaderThreadImpl(void *param)
{
    int i = 0, fd = (HCReaderThreadParam *)param->fd;
    HCQueue *toWriter = (HCReaderThreadParam *)param->queue;
    HCWriterQueueDataParam *aWriterParam = NULL;
    //HCReaderThreadCallback callback = (HCReaderThreadParam *)param->callback;
    short bid = 0x0000;
    unsigned long restBlocks = (HCReaderThreadParam *)param->totalBlocks;
    unsigned long totalBlocks = restBlocks;
    unsigned long _BlockLen = 0L, _BlockByteRead = 0L;
    unsigned long long _DataLen = 0LL;
    int _PropLen = 0, _RtcCounter = 0;

    while(1) {
        pthread_mutex_lock(&toWriter->mutex);
        while(toWriter->readp < toWriter->writep) {
            if(__InternalReaderKillRequestCounter) {
                pushdeb("reader: terminal signal received (%d times), exit now\n", __InternalReaderKillRequestCounter);
                //pthread_mutex_unlock(&toWriter->mutex);
                goto __ReaderExitPoint;
            }
            pthread_cond_wait(&toWriter->empty, &toWriter->mutex);
        }
        /* Fill the queue */
        for(i = 0; i < toWriter->size, restBlocks > 0; i++, restBlocks--) {
            /* Reset if it holds old data */
            if(toWriter->data[i]) 
                __HCWriterQueueDataParamDestroy((HCWriterQueueDataParam *)toWriter->data[i]);
            HCCalloc(aWriterParam, 1, sizeof(HCWriterQueueDataParam),
                __HCReadExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint));
            HCCalloc(aWriterParam->property, 1, sizeof(HCBlockProperty),
                __HCReadExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint));

            /* Check about BID_BEGIN */
            if(HCReadFileX(fd, &bid, sizeof(short))) {
                __HCWriterQueueDataParamDestroy(&aWriterParam);
                __HCReaderExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint);
            }
            if(HCSwapBytes(bid) != BID_BEGIN) {
                /* Oops ... */
                pushdeb("reader: Unknown BID_BEGIN, the block may broken\n");
                __HCWriterQueueDataParamDestroy(&aWriterParam);
                __HCReaderExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint);
            }

            /* BLKLEN & DATLEN */
            if(HCReadFileX(fd, &_BlockLen, sizeof(unsigned long))) {
                __HCWriterQueueDataParamDestroy(&aWriterParam);
                __HCReaderExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint);
            }
            if(HCReadFileX(fd, &_DataLen, sizeof(unsigned long long))) {
                __HCWriterQueueDataParamDestroy(&aWriterParam);
                __HCReaderExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint);
            }
            pushdeb("reader: block@%05d(%lu): datalen = %llu\n", totalBlocks - restBlocks, _BlockLen, _DataLen);

            /* Loop read the whole block property */
            while(_BlockByteRead < _BlockLen) {
                if(HCReadFileX(fd, &bid, sizeof(short))) {
                    __HCWriterQueueDataParamDestroy(&aWriterParam);
                    __HCReaderExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint);
                }
                if(HCReadFileX(fd, &_PropLen, sizeof(int))) {
                    __HCWriterQueueDataParamDestroy(&aWriterParam);
                    __HCReaderExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint);
                }

                switch(HCSwapBytes(bid)) {
                    case BID_PROP_TYPE:
                        _RtcCounter += HCReadFileX(fd, aWriterParam->property->fType, _PropLen);
                        break;
                    case BID_PROP_SIZE_INCELL:
                        _RtcCounter += HCReadFileX(fd, aWriterParam->property->fSize1, _PropLen);
                        break;
                    case BID_PROP_SIZE_ORIGINAL:
                        _RtcCounter += HCReadFileX(fd, aWriterParam->property->fSize2, _PropLen);
                        break;
                    case BID_PROP_PATHNAME:
                        _RtcCounter += HCReadFileX(fd, aWriterParam->property->pathName, _PropLen);
                        break;
                    case BID_PROP_LINKNAME:
                        _RtcCounter += HCReadFileX(fd, aWriterParam->property->linkName, _PropLen);
                        break;
                    case BID_PROP_MODE:
                        _RtcCounter += HCReadFileX(fd, aWriterParam->property->fMode, _PropLen);
                        break;
                    case BID_PROP_UID:
                        _RtcCounter += HCReadFileX(fd, aWriterParam->property->fUID, _PropLen);
                        break;
                    case BID_PROP_GID:
                        _RtcCounter += HCReadFileX(fd, aWriterParam->property->fGID, _PropLen);
                        break;
                    case BID_PROP_DEV1:
                        _RtcCounter += HCReadFileX(fd, aWriterParam->property->dev1, _PropLen);
                        break;
                    case BID_PROP_DEV2:
                        _RtcCounter += HCReadFileX(fd, aWriterParam->property->dev2, _PropLen);
                        break;
                    default:
                        pushdeb("reader: unknown BID: 0x%02x\n", HCSwapBytes(bid));
                        __HCWriterQueueDataParamDestroy(&aWriterParam);
                        __HCReaderExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint);
                }
                if(_RtcCounter) {
                    __HCWriterQueueDataParamDestroy(&aWriterParam);
                    __HCReaderExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint);
                }

                /* Read data to memory stream */
                if(_DataLen > 0) {
                    aWriterParam->status = 0;
                    HCCalloc(aWriterParam->data, 1, _DataLen, 
                        __HCWriterQueueDataParamDestroy(&aWriterParam);
                        __HCReaderExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint));
                    if(HCReadFileX(fd, aWriterParam->data, _DataLen)) {
                        __HCWriterQueueDataParamDestroy(&aWriterParam);
                        __HCReaderExitActions(&toWriter->full, &toWriter->mutex, __ReaderExitPoint);
                    }
                } else if(!_DataLen && HCSwapBytes(aWriterParam->property->fType) == BLK_REG) {
                    aWriterParam->status = 1; // an empty file
                    aWriterParam->data = NULL;
                }
            }
            /* Push the writer param to the queue */
            toWriter->data[i] = (void *)aWriterParam;
            toWriter->writep = i + 1;
        }
        /* reset queue indicator */
        toWriter->readp = 0;
        //pthread_cond_broadcast(&queue->full);
        //pthread_mutex_unlock(&queue->mutex);
    }

__ReaderExitPoint:
    pushdeb("reader thread exited\n");
    pthread_cond_broadcast(&toWriter->full);
    pthread_mutex_unlock(&toWriter->mutex);

    pthread_exit(NULL);
}

static void __HCWriterThreadImpl(void *param)
{
    pthread_t reader_pid = ((HCWriterThreadParam *)param)->ofReader;
    HCQueue *queue = ((HCWriterThreadParam *)param)->frmReader;
    HCReaderThreadCallback callback = ((HCWriterThreadParam *)param)->callback;
    HCWriterQueueDataParam *aWriterParam = NULL;
    HCBlockProperty *curProp = NULL;
    int _ErrorOccurred = 0, fd = -1, curStatus = 0;
    char *curPathName = NULL;
    /* Decompress related */
    unsigned char *DecompBuffer = NULL, *DataBuffer = NULL;
    unsigned long long DecompSize = 0LL;
    int zRes = Z_OK;

    while(1) {
        pthread_mutex_lock(&queue->mutex);
        while(queue->readp > queue->writep)
            pthread_cond_wait(&queue->full, &queue->mutex);
        if((aWriterParam = (HCWriterQueueDataParam *)queue->data[queue->readp])) {
            /* Clone the data then release */
            curStatus = aWriterParam->status;
            curProp = HCMemdup(aWriterParam->property, sizeof(_HCWriterQueueDataParam));
            /* It is not recommended to process with large file */
            DataBuffer = HCSwapBytes(curProp->fType) == BLK_REG ? HCMemdup(aWriterParam->data, curProp->fSize1) : NULL;
            curPathName = __HCDecodeString(curProp->pathName);

            /* Release the resource and lock so that other threads can fetch the data
               as soon as possible */
            __HCWriterQueueDataParamDestroy(&((HCWriterQueueDataParam *)queue->data[queue->readp]));
            queue->data[queue->readp] = NULL; /* Just for sure */
            queue->readp++; // Push the indicator to next data slot
            pthread_mutex_unlock(&queue->mutex);

            /* Reserved: call progress callback */
            // ...

            switch(HCSwapBytes(curProp->fType)) {
                case BLK_REG: {
                    /* Force override */
                    if(isFileExists(curPathName))
                        remove(curPathName);
                    /* Uncompress data stream */
                    /* Before we formally start, we must check status code,
                       If this is just an empty file, close the handle now */
                    if(curStatus = 1) {
                        if((fd = creat(curPathName, curProp->fMode)) == -1) {
                            pushdeb("writer: failed to create file, %s\n", strerror(errno));
                            _ErrorOccurred = 1; /* ERR_CREAT */
                            goto __WriterBlockSkipProcess_Reg;
                        }
                        close(fd); // curProp->fSize2 == 0
                    } else {
                        //HCCalloc(DecompBuffer, 1, curProp->fSize2, _ErrorOccurred = 1; goto __WriterExitPoint);
                        //if((fd = creat(curPathName, curProp->fMode)) == -1) {
                        if(HCCreateFile(curPathName, curProp->fSize2, curProp->fMode)) {
                            pushdeb("writer: failed to create file \'%s\', %s\n", curPathName, strerror(errno));
                            _ErrorOccurred = 1;
                            goto __WriterBlockSkipProcess_Reg;
                        }
                        if((fd = open(curPathName, O_RDWR)) == -1 || 
                            (DecompBuffer = mmap(NULL, curProp->fSize2, PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
                            _ErrorOccurred = 2; /* ERR_OPEN_BIND_MEM */
                            pushdeb("writer: failed to open file, %s\n", strerror(errno));
                            remove(curPathName);
                            goto __WriterBlockSkipProcess_Reg;
                        }

                        DecompSize = curProp->fSize2;

                        if((zRes = uncompress(DecompBuffer, &DecompSize, aWriterParam->data, curProp->fSize1))) {
                            pushdeb("write: decompressor returned 0x%08x\n", zRes);
                            _ErrorOccurred = 3; /* ERR_DECOMP */
                            goto __WriterBlockSkipProcess_Reg;
                        }
                        if(DecompSize != curProp->fSize2) {
                            pushdeb("writer: failed to decompress data, size mismatched\n");
                            _ErrorOccurred = 4; /* ERR_DECOMP_SIZE_MISMATCH */
                            goto __WriterBlockSkipProcess_Reg;
                        }

__WriterBlockSkipProcess_REG:
                        if(DecompBuffer && DecompBuffer != MAP_FAILED)
                            if(munmap(DecompBuffer, curProp->fsize2) == -1)
                                pushdeb("writer: failed to unbind memory, ignored\n");
                        if(DataBuffer) free(DataBuffer);
                        close(fd);
                    }
                    break;
                } /* End of BLK_REG */

                case BLK_HARDLINK: {
                    if(link(__HCDecodeString(curProp->linkName), curPathName)) {
                        pushdeb("writer: failed to create hard link \'%s\'-->\'%s\', %s\n", curPathName,
                            __HCDecodeString(curProp->linkName), strerror(errno));
                        _ErrorOccurred = 5; /* ERR_CREAT_HARDLINK */
                    }
                    break;
                } /* End of BLK_HARDLINK */

                case BLK_SYMLINK: {
                    if(symlink(__HCDecodeString(curProp->linkName), curPathName)) {
                        pushdeb("writer: failed to create symbolic link \'%s\'-->\'%s\', %s\n", curPathName,
                            __HCDecodeString(curProp->linkName), strerror(errno));
                        _ErrorOccurred = 6; /* ERR_CREAT_SYMLINK */
                    }
                    break;
                } /* End of BLK_SYMLINK */

                case BLK_BLOCKDEV: {}
                case BLK_CHARDEV: {
                    if(mknod(curPathName, 
                        (HCSwapBytes(curProp->fType) == BLK_CHARDEV ? S_IFCHR : S_IFBLK) | curProp->mode, 
                        makedev(curProp->dev1, curProp->dev2))) {
                        pushdeb("writer: failed to create device, %s\n", strerror(errno));
                        _ErrorOccurred = 7; /* ERR_CREAT_DEVICE */
                    }
                    break;
                } /* End of BLK_BLOCKDEV, BLK_CHARDEV */

                case BLK_DIR: {
                    if(mkpath(curPathName, curProp->mode)) {
                        pushdeb("writer: failed to create dir \'%s\'\n", curPathName);
                        _ErrorOccurred = 8; /* ERR_CREAT_DIR */
                    }
                    break;
                } /* End of BLK_DIR */

                case BLK_FIFO: {
                    if(mknod(curPathName, S_IFIFO | curProp->mode, 0)) {
                        pushdeb("writer: failed to create fifo, %s\n", strerror(errno));
                        _ErrorOccurred = 9; /* ERR_CREAT_FIFO */
                    }
                    break;
                } /* End of BLK_FIFO */
                default:
                    pusherr("Unknown block type, skipped\n");
            }

            /* Clean up now */
            free(curProp);
            if(_ErrorOccurred) {
                /* Check whether the reader process still alive, if yes kill it */
                /* Only the reader thread dead, other worker threads will stop  */
                pushdeb("writer@%ld: critical error occurred, trying to kill the reader thread...\n", pthread_self());
                if(pthread_kill(reader_pid) != ESRCH && pthread_kill(reader_pid) != EINVAL) {
                    pushdeb("writer@%ld: reader thread exists, setting up terminal signal...\n");
                    __InternalReaderKillRequestCounter++;
                }
                goto __WriterExitPoint;
            }

        } else {
            /* Can't get any data from the queue, if the reader thread has exited,
              exit as well. */
            if(pthread_kill(reader_pid, 0) == ESRCH) {
                /* Reader thread has dead */
                pushdeb("writer@%ld: exited without any errors\n", pthread_self());
                pthread_mutex_unlock(&queue->mutex);
                goto __WriterExitPoint;
            } else {
                if(queue->readp < queue->writep) {
                    pushdeb("writer: exited due to a critical error occurred in the queue\n");
                    __InternalReaderKillRequestCounter++;
                    break;
                }
            }
            pthread_cond_signal(&queue->empty); // contact to reader thread
        }
        //pthread_mutex_unlock(&queue->mutex);
    }
__WriterExitPoint:
    pushdeb("worker@%lu: exited%s\n", pthread_self(), _ErrorOccurred ? " error" : "");
    pthread_exit(NULL);
}

static void __HCWriterQueueDataParamDestroy(HCWriterQueueDataParam **param)
{
    HCWriterQueueDataParam *p = *param;

    if(*param) {
        if(p->property) free(p->property);
        if(p->data) free(p->data);
        free(*param);
        *param = NULL;
    }
}

/* Basic Queue Allocations */
static HCQueue *__HCQueueInitialise(int size)
{
    HCQueue *queue = calloc(1, sizeof(HCQueue));

    if(!queue) return NULL;
    queue->data = calloc(size, sizeof(void *));
    if(!queue->data) {
        free(queue);
        return NULL;
    }
    queue->size = size;
    queue->writep = queue->readp = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->full, NULL);
    pthread_cond_init(&queue->empty, NULL);

    return queue;
}

static void __HCQueueDestroy(HCQueue **queue)
{
    int i;
    HCQueue *pQueue = *queue;

    if(*queue) {
        for(i = 0; i < pQueue->size; i++)
            if(pQueue->data[i]) free(pQueue->data[i]);
        free(pQueue->data);
        free(pQueue);
        *queue = NULL;
    }
}

static char *__HCDecodeString(char *pStr)
{
    int i;

    if(pStr && strlen(pStr))
        for(i = 0; i < strlen(pStr); i++) {
            pStr[i] ^= 0x0A5E921F;
            pStr[i] = HCCharSwap(pStr[i]);
        }

    return pStr;
}