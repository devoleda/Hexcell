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
#include <string.h>
#include <hexcell_message.h>

efcl_msg_callback global_cb = NULL;

void set_msg_callback(efcl_msg_callback cb)
{
	if(cb)
		global_cb = cb;
}

static void pushmsg(efcl_msg_t type, efcl_msg_callback cb, 
	const char *msg)
{
	char *strtype = NULL;

	switch(type) {
		case EFCL_MSG_NORMAL:
			strtype = "I: ";
			break;
		case EFCL_MSG_WARNING:
			strtype = "W: ";
			break;
		case EFCL_MSG_ERROR:
			strtype = "E: ";
			break;
		case EFCL_MSG_DEBUG:
			strtype = "D: ";
			break;
		default: /* Impossible */
			strtype = "I: ";
	}

	char buf[strlen(msg) + 4];
	memset(buf, 0, sizeof(buf));
	sprintf(buf, "%s%s", strtype, msg);
	if(global_cb)
		global_cb(type, buf);
}

void pusherr(const char *fmt, ...)
{
	va_list argv;
	char buf[1024] = {'\0'};

	va_start(argv, fmt);
	vsprintf(buf, fmt, argv);
	va_end(argv);

	pushmsg(EFCL_MSG_ERROR, global_cb, buf);
}

void pushinf(const char *fmt, ...)
{
	va_list argv;
	char buf[1024] = {'\0'};

	va_start(argv, fmt);
	vsprintf(buf, fmt, argv);
	va_end(argv);

	pushmsg(EFCL_MSG_NORMAL, global_cb, buf);
}

void pushwar(const char *fmt, ...)
{
	va_list argv;
	char buf[1024] = {'\0'};

	va_start(argv, fmt);
	vsprintf(buf, fmt, argv);
	va_end(argv);

	pushmsg(EFCL_MSG_WARNING, global_cb, buf);
}

void pushdeb(const char *fmt, ...)
{
	va_list argv;
	char buf[1024] = {'\0'};

	va_start(argv, fmt);
	vsprintf(buf, fmt, argv);
	va_end(argv);

	pushmsg(EFCL_MSG_DEBUG, global_cb, buf);
}