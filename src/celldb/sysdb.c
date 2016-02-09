/*
 * Copyright (c) 2015 Devoleda Organisation
 * Copyright (c) 2015 Ivan Romanov <ivanro55555@gmail.com>
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

#include <stdio.h>
#include <string.h>
#include <sysdb.h>
#include <stdbool.h>
#include <shared/message.h>
#include <shared/utils.h>

static char *sysdb_default_database = "/etc/hexcell/system.db";

sqlite3 *db = NULL, *repodb = NULL;
char *__sysdb_error = NULL, *__sysdb_current_db = NULL;
char *__repo_current_db = NULL, *__repo_error = NULL;

/*
 * Initialize the database by creating the RECORDS table which keeps track after
 * the systems' cells.
 */
static int _sysdb_cell_init(int type) {
    int rc;
    char *zErrMsg = 0;
    char *sql_query =  NULL;

    if(!type)
        sql_query = "CREATE TABLE IF NOT EXISTS LOCALCELL ("
                    "ID                INTEGER PRIMARY_KEY,"
                    "NAME              CHAR(128) UNIQUE NOT NULL,"
                    "SHORTNAME         CHAR(64),"
                    "VERSION_1         INT,"
                    "VERSION_2         INT,"
                    "VERSION_3         INT,"
                    "VERSION_4         INT,"
                    "STRING_VER        CHAR(64),"
                    "DESCRIPTION       BLOB,"
                    "URL               VARCHAR(255),"
                    "PACKAGER          VARCHAR(255),"
                    "ARCH              INT,"
                    "INSTALLED_SIZE    INT64,"
                    "DEPENDENCY        BLOB,"
                    "PROVIDE           BLOB,"
                    "CONFLICT          BLOB,"
                    "LICENCE           VARCHAR(64),"
                    "INSTALLTIME       INT64,"
                    "UUID              BLOB,"
                    "SCRIPTLET_DATA    BLOB,"
                    "CERTDATA          BLOB,"
                    "TREEDATA          BLOB);";
    else
        sql_query = "CREATE TABLE IF NOT EXISTS CELLINDEX ("
                    "ID                INTEGER PRIMARY_KEY,"
                    "NAME              CHAR(128) UNIQUE NOT NULL,"
                    "SHORTNAME         CHAR(64),"
                    "VERSION_1         INT,"
                    "VERSION_2         INT,"
                    "VERSION_3         INT,"
                    "VERSION_4         INT,"
                    "STRING_VER        CHAR(64),"
                    "DESCRIPTION       TEXT,"
                    "URL               VARCHAR(255),"
                    "PACKAGER          VARCHAR(255),"
                    "ARCH              INT,"
                    "INSTALLED_SIZE    INT64,"
                    "DEPENDENCY        TEXT,"
                    "PROVIDE           TEXT,"
                    "CONFLICT          TEXT,"
                    "BUILDTIME         INT64);";

    /* check whether we have open a handle */
    if(!type) {
        ASSERT(db, return -1);
        if((rc = sqlite3_exec(db, sql_query, NULL, NULL, &zErrMsg))) {
            //sprintf(__sysdb_error, "%s", zErrMsg);
            pushdeb("in %s(local): %s\n", __func__, zErrMsg);
            sqlite3_free(zErrMsg);
            //sqlite3_close(db);
            return rc;
        }
        pushdeb("sysdb: the table initialised successfully\n");
    } else {
        ASSERT(repodb, return -1);
        if((rc = sqlite3_exec(repodb, sql_query, NULL, NULL, &zErrMsg))) {
            //sprintf(__repo_error, "%s", zErrMsg);
            pushdeb("in %s(repo): %s\n", __func__, zErrMsg);
            sqlite3_free(zErrMsg);
            //sqlite3_close(db);
            return rc;
        }
        pushdeb("repodb: the table initialised successfully\n");
    }

    return 0;
}      

static int sysdb_cell_init(void)
{
    return _sysdb_cell_init(0);
}

static int repodb_init(void)
{
    return _sysdb_cell_init(1);
}

static bool _IsTableExists(sqlite3 *db, const char *table)
{
    char sqlcmd[100] = {'\0'};
    int records = 0, n, j;
    char *zErrMsg = NULL, **Result;

    ASSERT(db && __sysdb_current_db && table, return false);
    // I think no one will stupid as search for 'table' here
    sprintf(sqlcmd, "SELECT COUNT(*) FROM SQLITE_MASTER WHERE NAME=\'%s\';", table);
    if(sqlite3_get_table(db, sqlcmd, &Result, &j, &n, &zErrMsg)) {
        pushdeb("in %s: query table records failed: %s\n", __func__, zErrMsg);
        records = 0;
    }
    records = atoi(Result[1]);
    sqlite3_free(zErrMsg);
    sqlite3_free_table(Result);
    pushdeb("in %s: records = %d\n", __func__, records);

    return records; /* impossible to have records > 1 */
}

char *sysdb_errmsg(void)
{
    return __sysdb_error;
}

char *repodb_errmsg(void)
{
    return __repo_error;
}

Sysdb_CellRecord *sysdb_cell_record_new(DNA_struct *DNA, unsigned char *certdata,
    unsigned long certdata_len, unsigned char *treedata, unsigned long treedata_len)
{
    Sysdb_CellRecord *p = calloc(sizeof(Sysdb_CellRecord), 1);

    if(!p) {
        strcpy(__sysdb_error, "allocate memory failed");
        return NULL;
    }
    p->DNA = DNA ? tpsl_memdup(DNA, sizeof(DNA_struct)) : calloc(sizeof(DNA_struct), 1);
    p->certdata = certdata_len > 0 ? tpsl_memdup(certdata, certdata_len) : NULL;
    p->treedata = treedata_len > 0 ? tpsl_memdup(treedata, treedata_len) : NULL; // compressed filelist
    p->certdata_len = certdata_len;
    p->treedata_len = treedata_len;

    return p;
}

void sysdb_cell_record_destory(Sysdb_CellRecord **sc)
{
    if(sc) {
        free((*sc)->DNA);
        if((*sc)->certdata_len > 0)
            free((*sc)->certdata);
        if((*sc)->treedata_len > 0)
            free((*sc)->treedata);
        free(*sc);
        *sc = NULL;
    }
}

/* ret > 0 = sql_error, ret < 0 = internal_error */
int sysdb_open(const char *database)
{
    int rc = 0;

    /* initialise internal vars */
    if(!__sysdb_error)
        CALLOC(__sysdb_error, 1, 256, return -1);

    /* if there is another database specified, switch to that one */
    if(!__sysdb_current_db) {
        if(!(__sysdb_current_db = strdup(tpsl_strlen(database) ? 
            database : sysdb_default_database))) {
            sprintf("in %s: failed to allocate memory", __func__);
            return -1;
        }
    } else if(strlen(__sysdb_current_db)) {
        pushdeb("in %s: called twice by misoperation, %s\n", __func__,
            __sysdb_current_db);
        return 0; /* error?? */
    }

    if((rc = sqlite3_open(__sysdb_current_db, &db))) {
        sprintf(__sysdb_error, "failed to open database: %s", 
            sqlite3_errmsg(db));
        return sqlite3_errcode(db);
    }
    pushdeb("%s: open database handle successfully (%s)\n", __func__,
        __sysdb_current_db);
    if(!_IsTableExists(db, "LOCALCELL")) {
        if(sysdb_cell_init()) { // create an empty table
            sprintf(__sysdb_error, "Failed to initialise database, %s", 
                __sysdb_error);
            sqlite3_close(db);
            return 1;
        }
    }

    return 0; /* success */
}

int repodb_open(const char *database)
{
    int rc;

    ASSERT(database, return -1);
    if(!__repo_error) // must be NULL after close
        CALLOC(__repo_error, 1, 256, return -1);
    if(__repo_current_db) {
        if(!strcmp(__repo_current_db, database))
            return 0;
        sprintf(__repo_error, "another database is open");
        return 1; /* previous database hasn't be closed yet */
    }
    if(!(__repo_current_db = strdup(database))) {
        sprintf(__repo_error, "failed to allocate memory");
        __repo_current_db = NULL;
        return -2;
    }
    if((rc = sqlite3_open(__repo_current_db, &repodb))) {
        strcpy(__repo_error, sqlite3_errmsg(repodb));
        return sqlite3_errcode(repodb);
    }
    pushdeb("RepoDB: Open successfully (%s)\n", database);
    if(!_IsTableExists(repodb, "CELLINDEX")) {
        if(repodb_init()) {
            sprintf(__repo_error, "failed to initialise repo database");
            return 2;
        }
    }

    return 0;
}

/*
 * Delete a row from the RECORDS table identified by the PRIMARY KEY.
 */
int sysdb_cell_delete_record_by_name(const char *name)
{
    int rc = 0;
    char *zErrMsg = NULL;
    char sql[200] = {"\0"};
      
    ASSERT(db && name, return -1);
    sprintf(sql, "delete from localcell where name=\'%s\';", name);
    if(rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg)) {
        sprintf(__sysdb_error, "failed to delete record, %s\n", zErrMsg);
        pushdeb("delete record: failed\n");
        //sqlite3_close(db);
        sqlite3_free(zErrMsg);
        return rc;
    }

    return 0; /* success */
}

/*
** Insert a record to the RECORDS table.
** The function creates a sql query "strcated" with the proper attributes.
*/
int sysdb_cell_insert_record(Sysdb_CellRecord *sc)
{
    int rc = 0;
    //size_t rsize = 0;
    char sql2[4096] = {'\0'}, UUID[32] = {'\0'};
    sqlite3_stmt *stmt = NULL;

    ASSERT(db && __sysdb_current_db && sc && sc->certdata && sc->DNA &&
        sc->treedata, return -1);

    sprintf(sql2, "insert into localcell values(null, \'%s\', \'%s\', %d, %d, %d, "
        "%d, \'%s\', ?, \'%s\', \'%s\', %d, ?, ?, ?, ?, \'%s\', %ld, ?, ?, ?, ?);", 
        sc->DNA->name, sc->DNA->shortName, sc->DNA->version[0], sc->DNA->version[1],
        sc->DNA->version[2], sc->DNA->version[3], sc->DNA->versionString, 
        sc->DNA->url, sc->DNA->packager, sc->DNA->arch, sc->DNA->licence, 
        sc->DNA->buildtime);
    //pushdeb("UUID: %s\n", sc->DNA->UUID);
    //pushdeb("COMMAND: %s\n", sql2);
    if((rc = sqlite3_prepare_v2(db, sql2, -1, &stmt, NULL))) {
        sprintf(__sysdb_error, "failed to prepare sql command, %s, errcode=%d", 
            sqlite3_errmsg(db), rc);
        goto scir_finished;
    }

    /* bind binary data into table */
    if((rc = sqlite3_bind_blob(stmt, 1, sc->DNA->description, strlen(sc->DNA->description),
        SQLITE_STATIC))) {
        sprintf(__sysdb_error, "failed to bind data(%d), %s, errcode=%d",
            __LINE__, sqlite3_errmsg(db), rc);
        goto scir_finished;
    }
    if((rc = sqlite3_bind_int64(stmt, 2, sc->DNA->installedSize))) {
        sprintf(__sysdb_error, "failed to bind data(%d), %s, errcode=%d",
            __LINE__, sqlite3_errmsg(db), rc);
        goto scir_finished;
    }
    if((rc = sqlite3_bind_blob(stmt, 3, sc->DNA->dependency, strlen(sc->DNA->dependency),
        NULL))) {
        sprintf(__sysdb_error, "failed to bind data(%d), %s, errcode=%d",
            __LINE__, sqlite3_errmsg(db), rc);
        goto scir_finished;
    }
    if((rc = sqlite3_bind_blob(stmt, 4, sc->DNA->provide, strlen(sc->DNA->provide),
        NULL))) {
        sprintf(__sysdb_error, "failed to bind data(%d), %s, errcode=%d",
            __LINE__, sqlite3_errmsg(db), rc);
        goto scir_finished;
    }
    if((rc = sqlite3_bind_blob(stmt, 5, sc->DNA->conflict, strlen(sc->DNA->conflict),
        NULL))) {
        sprintf(__sysdb_error, "failed to bind data(%d), %s, errcode=%d",
            __LINE__, sqlite3_errmsg(db), rc);
        goto scir_finished;
    }
    if((rc = sqlite3_bind_blob(stmt, 6, sc->DNA->UUID, sizeof(uuid_t), NULL))) {
        sprintf(__sysdb_error, "failed to bind data(%d), %s, errcode=%d",
           __LINE__,  sqlite3_errmsg(db), rc);
        goto scir_finished;
    }
    if((rc = sqlite3_bind_blob(stmt, 7, sc->DNA->scriptlet_data, 
        strlen(sc->DNA->scriptlet_data), NULL))) {
        sprintf(__sysdb_error, "failed to bind data(%d), %s, errcode=%d",
            __LINE__, sqlite3_errmsg(db), rc);
        goto scir_finished;
    }
    if((rc = sqlite3_bind_blob(stmt, 8, sc->certdata, sc->certdata_len, NULL))) {
        sprintf(__sysdb_error, "failed to bind data(%d), %s, errcode=%d",
            __LINE__, sqlite3_errmsg(db), rc);
        goto scir_finished;
    }
    if((rc = sqlite3_bind_blob(stmt, 9, sc->treedata, sc->treedata_len, NULL))) {
        sprintf(__sysdb_error, "failed to bind data(%d), %s, errcode=%d",
            __LINE__, sqlite3_errmsg(db), rc);
        goto scir_finished;
    }
    if((rc = sqlite3_step(stmt)) != SQLITE_DONE)
        sprintf(__sysdb_error, "failed to step data, errcode=%d, msg=%s", rc,
            sqlite3_errmsg(db));
    else
        /* special case: SQLITE_DONE = 101, but here it's stand for success */
        rc = 0;

scir_finished:
    //FREE(sql);
    sqlite3_finalize(stmt);
    return rc;
}

int repodb_insert_record(DNA_struct *dna)
{
    int rc = 0;
    char sql_command[65535] = {'\0'}, *zErrMsg = NULL;

    ASSERT(repodb && dna && __repo_current_db, return -1);

    sprintf(sql_command, "insert into CELLINDEX values(null, \'%s\', \'%s\',
        %d, %d, %d, %d, \'%s\', \'%s\', \'%s\', \'%s\', %d, %ld, \'%s\', 
        \'%s\', \'%s\', %ld);",
        dna->name, dna->shortName, dna->version[0], dna->version[1], 
        dna->version[2], dna->version[3], dna->versionString, dna->description,
        dna->url, dna->packager, dna->arch, dna->installedSize, dna->dependency,
        dna->provide, dna->conflict, dna->buildtime);
    if((rc = sqlite3_exec(repodb, sql_command, NULL, NULL, &zErrMsg))) {
        strcpy(__repo_error, zErrMsg);
        return rc;
    }

    return 0;
}

/* fetch local cell data by name */
int sysdb_cell_query_by_name(const char *name, Sysdb_CellRecord **sc)
{
    sqlite3_stmt *stmt = NULL;
    char *sql = NULL;
    int rc = 0;
    size_t datasize = 0;

    ASSERT(db && __sysdb_current_db && sc, return -1);
    sql = calloc(strlen(name) + 40, 1);
    sprintf(sql, "select * from localcell where name=\'%s\'", name);
    if((rc = sqlite3_prepare(db, sql, -1, &stmt, NULL))) {
        sprintf(__sysdb_error, "failed to prepare: %s, errcode=%d", 
            sqlite3_errmsg(db), rc);
        rc = 1; /* SQL_PREPARE_FAILED */
        goto scqn_v0;
    }
    if((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        /* ALLOCATED ? */
        if(!*sc)
            *sc = sysdb_cell_record_new(NULL, NULL, 0, NULL, 0);

        /* NAMES */
        strcpy((*sc)->DNA->name, sqlite3_column_text(stmt, 1));
        strcpy((*sc)->DNA->shortName, sqlite3_column_text(stmt, 2));

        /* VERSION */
        (*sc)->DNA->version[0] = sqlite3_column_int(stmt, 3);
        (*sc)->DNA->version[1] = sqlite3_column_int(stmt, 4);
        (*sc)->DNA->version[2] = sqlite3_column_int(stmt, 5);
        (*sc)->DNA->version[3] = sqlite3_column_int(stmt, 6);
        strcpy((*sc)->DNA->versionString, sqlite3_column_text(stmt, 7));

        /* DESCRIPTION */
        datasize = sqlite3_column_bytes(stmt, 8);
        if(datasize > 0)
            memcpy((*sc)->DNA->description, sqlite3_column_blob(stmt, 8), datasize);

        /* URL, PACKAGER, ARCH, SIZE */
        strcpy((*sc)->DNA->url, sqlite3_column_text(stmt, 9));
        strcpy((*sc)->DNA->packager, sqlite3_column_text(stmt, 10));
        (*sc)->DNA->arch = sqlite3_column_int(stmt, 11);
        (*sc)->DNA->installedSize = sqlite3_column_int64(stmt, 12);

        /* DEPENDENCY & PROVIDE & CONFLICT */
        datasize = sqlite3_column_bytes(stmt, 13);
        if(datasize > 0)
            memcpy((*sc)->DNA->dependency, sqlite3_column_blob(stmt, 13), datasize);
        datasize = sqlite3_column_bytes(stmt, 14);
        if(datasize > 0)
            memcpy((*sc)->DNA->provide, sqlite3_column_blob(stmt, 14), datasize);
        datasize = sqlite3_column_bytes(stmt, 15);
        if(datasize > 0)
            memcpy((*sc)->DNA->conflict, sqlite3_column_blob(stmt, 15), datasize);

        /* MISC */
        strcpy((*sc)->DNA->licence, sqlite3_column_text(stmt, 16));
        (*sc)->DNA->buildtime = sqlite3_column_int64(stmt, 17);
        //if(uuid_parse(sqlite3_column_text(stmt, 18), (*sc)->DNA->UUID)){
        //    sprintf(__sysdb_error, "failed to parse UUID");
        //    rc = 2; /* UUID_PARSE_FAILED */
        //    goto scqn_v1;
        //}

        datasize = sqlite3_column_bytes(stmt, 18);
        if(datasize > 0)
            memcpy((*sc)->DNA->UUID, sqlite3_column_blob(stmt, 18), datasize);

        datasize = sqlite3_column_bytes(stmt, 19);
        if(datasize > 0)
            memcpy((*sc)->DNA->scriptlet_data, sqlite3_column_blob(stmt, 19), datasize);

        (*sc)->certdata_len = sqlite3_column_bytes(stmt, 20);
        if((*sc)->certdata_len > 0) {
            if(!(*sc)->certdata)
                (*sc)->certdata = calloc((*sc)->certdata_len, 1);
            memcpy((*sc)->certdata, sqlite3_column_blob(stmt, 20),
                sqlite3_column_bytes(stmt, 20));
        }
        (*sc)->treedata_len = sqlite3_column_bytes(stmt, 21);
        if((*sc)->treedata_len > 0) {
            if(!(*sc)->treedata)
                (*sc)->treedata = calloc((*sc)->treedata_len, 1);
            memcpy((*sc)->treedata, sqlite3_column_blob(stmt, 21), (*sc)->treedata_len);
        }
        rc = 0; /* success */
    } //else if(rc == SQLITE_DONE) {
      //  strcpy(__sysdb_error, "bad duplications");
      //  rc = 3; /* BAD_DUPLICATE_RECORDS */
      //  goto scqn_v1;
    else {
        strcpy(__sysdb_error, "no records found");
        rc = 4; /* NO_RECORDS */
        free(*sc);
        *sc = NULL;
        goto scqn_v1;
    }

scqn_v1:
    sqlite3_finalize(stmt);
scqn_v0:
    FREE(sql);
    return rc;
}

int repodb_query_cell_by_name(const char *name, DNA_struct **DNA)
{
    sqlite3_stmt *stmt = NULL;
    int rc = 0;
    char *sql = NULL;
    DNA_struct *ndna = NULL;

    ASSERT(repodb && __repo_current_db && name, return -1);
    if(!(sql = calloc(1, strlen(name) + 40))) {
        strcpy(__repo_error, "failed to allocate memory");
        return -2; /* ERR_MEMORY */
    }
    sprintf(sql, "select * from cellindex where name=\'%s\';", name);
    if((rc = sqlite3_prepare_v2(repodb, sql, -1, &stmt, NULL))) {
        strcpy(__repo_error, sqlite3_errmsg(repodb));
        return 1; /* ERR_SQL_PREPARE */
    }
    if((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if(!*DNA) {
            ndna = calloc(1, sizeof(DNA_struct));
            if(!ndna) {
                strcpy(__repo_error, "failed to allocate memory");
                free(sql);
                return -2;
            }
            *DNA = ndna;
        } else
            ndna = *DNA;
        strcpy(ndna->name, sqlite3_column_text(stmt, 1));
        strcpy(ndna->shortName, sqlite3_column_text(stmt, 2));
        ndna->version[0] = sqlite3_column_int(stmt, 3);
        ndna->version[1] = sqlite3_column_int(stmt, 4);
        ndna->version[2] = sqlite3_column_int(stmt, 5);
        ndna->version[3] = sqlite3_column_int(stmt, 6);
        strcpy(ndna->versionString, sqlite3_column_text(stmt, 7));
        strcpy(ndna->description, sqlite3_column_text(stmt, 8));
        strcpy(ndna->url, sqlite3_column_text(stmt, 9));
        strcpy(ndna->packager, sqlite3_column_text(stmt, 10));
        ndna->arch = sqlite3_column_int(stmt, 11);
        ndna->installedSize = sqlite3_column_int64(stmt, 12);
        strcpy(ndna->dependency, sqlite3_column_text(stmt, 13));
        strcpy(ndna->provide, sqlite3_column_text(stmt, 14));
        strcpy(ndna->conflict, sqlite3_column_text(stmt, 15));
        ndna->buildtime = sqlite3_column_int64(stmt, 16);
    } else {
        /* no records */
        strcpy(__repo_error, "no records found in the repo database");
        free(sql);
        return 2; /* ERR_NO_RECORD */
    }

    free(sql);
    return 0;
}

void sysdb_close(void)
{
    if(__sysdb_error)
        FREE(__sysdb_error);
    if(strlen(__sysdb_current_db)) FREE(__sysdb_current_db);
    if(db) sqlite3_close(db);
    db = NULL;
}

void repodb_close(void)
{
    if(__repo_error)
        FREE(__repo_error);
    if(__repo_current_db) FREE(__repo_current_db);
    if(repodb) sqlite3_close(repodb);
    repodb = NULL;
}