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

#ifndef _HEXCELL_UTILS_H_
#define _HEXCELL_UTILS_H_

#if defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
#define HCSwapBytes(a) \
    ((unsigned int)a >> 24) | ((unsigned int)a >> 8) & 0x0000ff00 | (a << 8) & \
    0x00ff0000 | (a << 24)
#else
#define HCSwapBytes(a) a
#endif
#define HCCharSwap(ch) (                        \
    ((ch & 1) << 7)                |            \
    ((ch & (0b10)) << 5)           |            \
    ((ch & (0b100)) << 3)          |            \
    ((ch & (0b1000)) << 1)         |            \
    ((ch & (0b10000)) >> 1)        |            \
    ((ch & (0b100000)) >> 3)       |            \
    ((ch & (0b1000000)) >> 5)      |            \
    ((ch & (0b10000000)) >> 7))
#define HCAssert(cond, action) do { if(!(cond)) { action; } } while(0)
#define HCCalloc(p, l, s, action) \
        do { p = calloc(l, s); if(!p) { action; } } while(0)

extern int HCWriteFileX(int fd, void *buffer, size_t size);
extern int HCReadFileX(int fd, void *buffer, size_t size);
extern int isFileExists(const char *filename);
extern int mkpath(const char *s, mode_t mode);

#endif