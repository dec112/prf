/*
 * Copyright (C) 2020  <Wolfgang Kampichler>
 *
 * This file is part of rngin
 *
 * rngin is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rngin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * requires: libyaml-dev, liblog4c-dev, sqlite3
 */

/**
 *  @file    sqlite.c
 *  @author  Wolfgang Kampichler (DEC112 2.0)
 *  @date    04-2020
 *  @version 1.0
 *
 *  @brief this file holds spatialite/sqlite function definitions
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
  CALL_SQLITE(open(dbname, &db));

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
 *  @brief  DB query to get service mapping (input urn + location)
 *
 *  @arg    p_req_t, char*
 *  @return int
 */

int sqlite_QUERY(s_query_t *query, char *next, const char *dbname) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  char strQuery[QUERYSIZE];

  int len;
  int iRes = -1;

  // open database
  CALL_SQLITE(open_v2(dbname, &db, SQLITE_OPEN_READONLY, NULL));
  // init database connection

  if (next) {
    snprintf(strQuery, QUERYSIZE - 1,
             "SELECT state, max, length FROM queues WHERE uri LIKE '%s';",
             next);
  } else {
    return iRes;
  }

  LOG4DEBUG(pL, " query: [%s]", strQuery);

  CALL_SQLITE(prepare_v2(db, strQuery, strlen(strQuery) + 1, &stmt, NULL));

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    len = sqlite3_column_bytes(stmt, 0);
    query->state = (char *)malloc(len + 1);
    if (query->state != NULL) {
      memcpy(query->state, (char *)sqlite3_column_text(stmt, 0), len);
      query->state[len] = '\0';
    } else {
      LOG4ERROR(pL, "no memory");
    }
    query->max = (int)sqlite3_column_int(stmt, 1);
    query->length = (int)sqlite3_column_int(stmt, 2);
    iRes = 0;
  }

  CALL_SQLITE(finalize(stmt));
  CALL_SQLITE(db_release_memory(db));
  CALL_SQLITE(close(db));

  LOG4DEBUG(pL, "result: [%s / %d / %d]", query->state, query->max, query->length);

  return iRes;
}
