#ifndef __RESOLVER_H__
#define __RESOLVER_H__
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

typedef struct __relationship_t {
	char cell1[128];
	char cell2[128];
	version_t ver1;
	version_t ver2;
	int i2; /* installed? */
} relationship_t;
typedef struct relationship_t conflict_t;

typedef enum __verrela_t {
	VER_ANY,
	VER_HIGHER_EQUAL,
	VER_HIGHER,
	VER_LOWER_EQUAL,
	VER_LOWER,
	VER_EQUAL
} verrela_t;

#endif