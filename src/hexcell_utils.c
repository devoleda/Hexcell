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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ftw.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <curses.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <time.h>

#include "hexcell_message.h"
#include <hexcell_utils.h>

int HCWriteFileX(int fd, void *buffer, size_t size)
{
    return (write(fd, buffer, size) == size) ? 0 : 1;
}

int HCReadFileX(int fd, void *buffer, size_t size)
{
    return (read(fd, buffer, size) == size) ? 0 : 1;
}

/**
 * @brief check whether a file is exist
 * @param filename the name of file to be checked
 * @return 1 on exists, 0 on otherwise
 */
int isFileExists(const char *filename)
{
    ASSERT(access(filename, F_OK | W_OK | R_OK), return 1);
    return 0;
}

/**
 * @brief make directory(recursively)
 * @param s the whole path to be create
 * @param mode attributes of directory
 * @return 0 on success, otherwise are failed
 */
int mkpath(const char *s, mode_t mode)
{
    char *q, *r = NULL, *path = NULL, *up = NULL;
    int rv;

    rv = -1;
    if (strcmp(s, ".") == 0 || strcmp(s, "/") == 0)
        return (0);

    if ((path = strdup(s)) == NULL)
        exit(1);

    if ((q = strdup(s)) == NULL)
        exit(1);

    if ((r = dirname(q)) == NULL)
        goto out;

    if ((up = strdup(r)) == NULL)
        exit(1);

    if ((mkpath(up, mode) == -1) && (errno != EEXIST))
        goto out;

    if ((mkdir(path, mode) == -1) && (errno != EEXIST))
        rv = -1;
    else
        rv = 0;

out:
    if (up != NULL) free(up);
    free(q);
    free(path);
    return (rv);
}

/**
 * @brief duplicate memory
 * @param p the pointer of memory
 * @param plen the size to clone
 * @return if success return the pointer of new cloned memory area, otherwise
 *            returns NULL.
 */
void *HCMemdup(const void *p, size_t plen)
{
    void *newp = NULL;

    ASSERT(p && plen, return NULL);
    ASSERT((newp = malloc(plen)), return NULL);
    memcpy(newp, p, plen);

    return newp;
}

int HCCreateFile(const char *path, size_t size, mode_t mode)
{
    const char endmark = 0x0F;
    int fd = -1;

    if((fd = creat(path, mode)) == -1)
        return 1;
    if(size > 1) {
        lseek(fd, size - sizeof(char), SEEK_END);
        write(fd, (void *)&endmark, sizeof(char));
    }

    close(fd);
    return 0;
}