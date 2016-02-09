#ifndef SYSDB_INTERNAL_H
#define SYSDB_INTERNAL_H
/*
 * Copyright (c) 2015 Devoleda Organisation
 * Copyright (c) 2015 Ethan Levy <eitanlevy97@yandex.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <cell/layout.h>

typedef enum __reason_t {
	UN_CELL_NOT_FOUND
} reason_t;

typedef struct __unknown_t {
	reason_t reason;
	char name[128];
} unknown_t;

typedef struct __item_t {
	char name[128];
	version_t version;
	char conflict[1024];
} queue_item;

/* NECESSARY VARIABLES EXPORT */
extern sqlite3 *db = NULL, *repodb = NULL;
extern char *__sysdb_error = NULL, *__sysdb_current_db = NULL;
extern char *__repo_current_db = NULL, *__repo_error = NULL;

#endif