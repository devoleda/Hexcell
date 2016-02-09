#ifndef SYSDB_H
#define SYSDB_H
/*
 * Copyright (c) 2015 Devoleda Organisation
 * Copyright (c) 2015 Ivan Romanov <ivanro55555@gmail.com>
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

#include "sqlite3.h"
#include <cell/layout.h>

typedef struct __sysdb_cell_record {
	DNA_struct *DNA;
	unsigned char *certdata;
	unsigned long certdata_len;
	unsigned char *treedata; /* compressed filelist */
	unsigned long treedata_len;
} Sysdb_CellRecord;

extern char *sysdb_errmsg(void);
extern Sysdb_CellRecord *sysdb_cell_record_new(DNA_struct *DNA, unsigned char *certdata,
    unsigned long certdata_len, unsigned char *treedata, unsigned long treedata_len);
extern void sysdb_cell_record_destory(Sysdb_CellRecord **sc);
extern int sysdb_open(const char *database);
extern int sysdb_cell_delete_record_by_name(const char *name);
extern int sysdb_cell_insert_record(Sysdb_CellRecord *sc);
extern int sysdb_cell_query_by_name(const char *name, Sysdb_CellRecord **sc);
extern void sysdb_close(void);

extern char *repodb_errmsg(void);
extern int repodb_open(const char *database);
extern int repodb_insert_record(DNA_struct *dna);
extern int repodb_query_cell_by_name(const char *name, DNA_struct **DNA);
extern void repodb_close(void);

#endif
