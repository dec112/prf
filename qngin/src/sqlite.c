/*
 * Copyright (C) 2020  <Wolfgang Kampichler>
 *
 * This file is part of qngin
 *
 * qngin is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * qngin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * requires: liblog4c-dev, sqlite3
 */

/**
 *  @file    sqlite.c
 *  @author  Wolfgang Kampichler (DEC112 2.0)
 *  @date    07-2020
 *  @version 1.0
 *
 *  @brief this file holds sqlite function definitions
 */

/******************************************************************* INCLUDE */

#include "sqlite.h"

/***************************************************************** FUNCTIONS */

/**
 * @name sqlite_CHECK
 * @brief performs open on database
 * @arg const char *dbname
 * @return 1 if ok, otherwise 0
 */

int sqlite_CHECK(const char *dbname) {

  sqlite3 *db;
  sqlite3_stmt *stmt;

  char strQuery[QUERYSIZE];
  int i = 0;

  /* open database */
  CALL_SQLITE(open(dbname == NULL ? SQLITE_DB_PRF : dbname, &db));

  if (!db) {
    LOG4ERROR(pL, "cannot open database: %s", sqlite3_errmsg(db));
    CALL_SQLITE(close(db));
    return 0;
  }

  /* run query */
  snprintf(strQuery, QUERYSIZE - 1,
           "SELECT COUNT(*) FROM (SELECT * FROM queues);");

  LOG4DEBUG(pL, "%s", strQuery);

  CALL_SQLITE(prepare_v2(db, strQuery, strlen(strQuery) + 1, &stmt, NULL));

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    LOG4DEBUG(pL, "queues table has %i rows", (int)sqlite3_column_int(stmt, 3));
    i = 1;
  }

  CALL_SQLITE(finalize(stmt));
  CALL_SQLITE(db_release_memory(db));
  CALL_SQLITE(close(db));

  return i;
}


/**
 * @name sqlite_R
 * @brief performs READ operation on queue table
 * @arg s_quelist_t *lqueue, const char *dbname
 * @return >0 if ok, otherwise 0
 */

int sqlite_R(s_quelist_t *lqueue, const char *dbname) {

  sqlite3 *db;
  sqlite3_stmt *stmt;
  s_queue_t **ptr = NULL;

  size_t len;

  char strQuery[QUERYSIZE];
  int q = -1;
  int i = 0;

  /* open database */
  CALL_SQLITE(open(dbname == NULL ? SQLITE_DB_PRF : dbname, &db));

  if (!db) {
    LOG4ERROR(pL, "cannot open database: %s", sqlite3_errmsg(db));
    CALL_SQLITE(close(db));
    return 0;
  }

  /* run query */
  snprintf(strQuery, QUERYSIZE - 1,
           "SELECT uri, state, dequeuer, max, length "
           "FROM queues "
           "WHERE dequeuer LIKE '%s';",
           lqueue->name);

  LOG4DEBUG(pL, "%s", strQuery);

  CALL_SQLITE(prepare_v2(db, strQuery, strlen(strQuery) + 1, &stmt, NULL));

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    ptr = queue_newlistitem(ptr, ++q);
    if (ptr == NULL) {
      LOG4ERROR(pL, "no memory");
      lqueue->count = 0;
      lqueue->queue = NULL;
      break;
    }
    ptr[q] = queue_newitem();
    if (ptr[q] == NULL) {
      lqueue->count = 0;
      lqueue->queue = ptr;
      break;
    }
    queue_inititem(ptr[q], DELETE);

    len = sqlite3_column_bytes(stmt, 0);
    ptr[q]->uri = string_copy((char *)sqlite3_column_text(stmt, 0), len);
    len = sqlite3_column_bytes(stmt, 1);
    ptr[q]->state = string_copy((char *)sqlite3_column_text(stmt, 1), len);
    len = sqlite3_column_bytes(stmt, 2);
    ptr[q]->dequeuer = string_copy((char *)sqlite3_column_text(stmt, 2), len);

    ptr[q]->max = (int)sqlite3_column_int(stmt, 3);
    ptr[q]->length = (int)sqlite3_column_int(stmt, 4);

    LOG4DEBUG(pL, "\t[%s] [%s] [%s] [%d] [%d] <%d>",
              ptr[q]->dequeuer, ptr[q]->uri, ptr[q]->state, ptr[q]->max,
              ptr[q]->length, ptr[q]->action);

    i++;
  }

  lqueue->count = q + 1;
  lqueue->queue = ptr;

  CALL_SQLITE(finalize(stmt));
  CALL_SQLITE(db_release_memory(db));
  CALL_SQLITE(close(db));

  return i;
}

/**
 * @name sqlite_CUD
 * @brief performs CREATE, UPDATE, DELETE operation on queue table row
 * @arg s_quelist_t *lqueue, const char *dbname
 * @return 1 if ok, otherwise 0
 */

int sqlite_CUD(s_quelist_t *queue, const char *dbname) {

  sqlite3 *db;
  s_queue_t **pqueue = NULL;

  char strQuery[QUERYSIZE];
  char *err_msg = 0;

  int i = 0;
  int rc = 0;

  /* open database */
  CALL_SQLITE(open(dbname == NULL ? SQLITE_DB_PRF : dbname, &db));

  if (!db) {
    LOG4ERROR(pL, "cannot open database: %s", sqlite3_errmsg(db));
    CALL_SQLITE(close(db));
    return i;
  }

  /* run query */
  pqueue = queue->queue;
  for (i = 0; i < queue->count; i++) {

    switch (pqueue[i]->action) {
    case CREATE:
      snprintf(strQuery, QUERYSIZE - 1,
               "BEGIN TRANSACTION;"
               "INSERT INTO queues (uri, state, dequeuer, max, length) "
               "VALUES ('%s', '%s', '%s', %d, %d);"
               "COMMIT;",
               pqueue[i]->uri, pqueue[i]->state, pqueue[i]->dequeuer,
               pqueue[i]->max, pqueue[i]->length);
      pqueue[i]->action = NONE;
      break;
    case UPDATE:
      snprintf(strQuery, QUERYSIZE - 1,
               "BEGIN TRANSACTION;"
               "UPDATE queues "
               "SET state = '%s', max = %d, length = %d "
               "WHERE uri LIKE '%s' "
               "AND dequeuer LIKE '%s';"
               "COMMIT;",
               pqueue[i]->state, pqueue[i]->max, pqueue[i]->length,
               pqueue[i]->uri, pqueue[i]->dequeuer);
      pqueue[i]->action = NONE;
      break;
    case DELETE:
      snprintf(strQuery, QUERYSIZE - 1,
               "BEGIN TRANSACTION;"
               "DELETE FROM queues "
               "WHERE uri LIKE '%s' "
               "AND dequeuer LIKE '%s';"
               "COMMIT;",
               pqueue[i]->uri, pqueue[i]->dequeuer);
      break;
    case PURGE:
      snprintf(strQuery, QUERYSIZE - 1,
               "BEGIN TRANSACTION;"
               "DELETE FROM queues "
               "WHERE dequeuer LIKE '%s';"
               "COMMIT;",
               pqueue[i]->dequeuer);
      break;

    default:
      continue;
      break;
    }

    LOG4DEBUG(pL, "%s", strQuery);

    rc = sqlite3_exec(db, strQuery, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {
      LOG4ERROR(pL, "SQL error: %s", err_msg);
      sqlite3_free(err_msg);
      continue;
    }
  }

  CALL_SQLITE(db_release_memory(db));
  CALL_SQLITE(close(db));

  return i;
}

/**
 * @name sqlite_PURGE
 * @brief performs CREATE, UPDATE, DELETE operation on queue table row
 * @arg s_quelist_t *lqueue, const char *dbname
 * @return 1 if ok, otherwise 0
 */

int sqlite_PURGE(const char *dqname, const char *dbname) {

  sqlite3 *db;

  char strQuery[QUERYSIZE];
  char *err_msg = 0;

  int rc = 0;
  int i = 0;

  /* open database */
  CALL_SQLITE(open(dbname == NULL ? SQLITE_DB_PRF : dbname, &db));

  if (!db) {
    LOG4ERROR(pL, "cannot open database: %s", sqlite3_errmsg(db));
    CALL_SQLITE(close(db));
    return i;
  }

  /* run query */
  snprintf(strQuery, QUERYSIZE - 1,
           "BEGIN TRANSACTION;"
           "DELETE FROM queues "
           "WHERE dequeuer LIKE '%s';"
           "COMMIT;",
           dqname);

  LOG4DEBUG(pL, "%s", strQuery);

  rc = sqlite3_exec(db, strQuery, 0, 0, &err_msg);

  if (rc != SQLITE_OK) {
    LOG4ERROR(pL, "SQL error: %s", err_msg);
    sqlite3_free(err_msg);
  } else {
    i = 1;
  }

  CALL_SQLITE(db_release_memory(db));
  CALL_SQLITE(close(db));

  return i;
}