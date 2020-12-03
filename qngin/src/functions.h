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
 */

/**
 *  @file    functions.h
 *  @author  Wolfgang Kampichler (DEC112 2.0)
 *  @date    06-2020
 *  @version 1.0
 *
 *  @brief functions.c header file
 */

#ifndef FUNCTIONS_H_INCLUDED
#define FUNCTIONS_H_INCLUDED

/******************************************************************* INCLUDE */

#include "cjson.h"
#include "mongoose.h"
#include <log4c.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <yaml.h>

/******************************************************************** DEFINE */

#define DBGLINE 200

#define TRUE true
#define FALSE false

#define _______ "\0\0\0\0"

#define WSPROTO "dec112-mgmt"

#define REGOK "registered"
#define METHOD "method"
#define HEALTH "health"
#define SIP "sip"

#define HASHTAG "#"
#define HASHTAGENC "%23"

#define GETHEALTH "{ \"method\": \"get_health\" }"
#define SUBHEALTH "{ \"method\": \"subscribe_health\" }"
#define UNSHEALTH "{ \"method\": \"unsubscribe_health\" }"

/* DEBUG MACROS */
#define DEBUG_PRINT(fmt, args...)                                              \
  fprintf(stderr, "DEBUG: %s():%d: " fmt, __func__, __LINE__, ##args)
#define ERROR_PRINT(fmt, args...)                                              \
  fprintf(stderr, "ERROR: %s():%d: " fmt, __func__, __LINE__, ##args)

/************************************************************** DEFINE LOG4C */

#define LOGCATDBG "qngin.dbg"
#define LOGCAT "qngin.info"

#define __SHORT_FILE__                                                         \
  (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#define __LOG4C__(category, format, loglevel, ...)                             \
  log4c_category_log(category, loglevel, format, ##__VA_ARGS__)
#define __LOGDB__(category, format, loglevel, ...)                             \
  log4c_category_log(category, loglevel, "[%s] [%s:%d] " format, __func__,     \
                     __SHORT_FILE__, __LINE__, ##__VA_ARGS__)

#define LOG4FATAL(category, format, ...)                                       \
  __LOG4C__(category, format, LOG4C_PRIORITY_FATAL, ##__VA_ARGS__)
#define LOG4ALERT(category, format, ...)                                       \
  __LOG4C__(category, format, LOG4C_PRIORITY_ALERT, ##__VA_ARGS__)
#define LOG4CRIT(category, format, ...)                                        \
  __LOG4C__(category, format, LOG4C_PRIORITY_CRIT, ##__VA_ARGS__)
#define LOG4ERROR(category, format, ...)                                       \
  __LOG4C__(category, format, LOG4C_PRIORITY_ERROR, ##__VA_ARGS__)
#define LOG4WARN(category, format, ...)                                        \
  __LOG4C__(category, format, LOG4C_PRIORITY_WARN, ##__VA_ARGS__)
#define LOG4NOTICE(category, format, ...)                                      \
  __LOG4C__(category, format, LOG4C_PRIORITY_NOTICE, ##__VA_ARGS__)
#define LOG4INFO(category, format, ...)                                        \
  __LOG4C__(category, format, LOG4C_PRIORITY_INFO, ##__VA_ARGS__)

#define LOG4DEBUG(category, format, ...)                                       \
  __LOGDB__(category, format, LOG4C_PRIORITY_DEBUG, ##__VA_ARGS__)
#define LOG4TRACE(category, format, ...)                                       \
  __LOGDB__(category, format, LOG4C_PRIORITY_TRACE, ##__VA_ARGS__)

/******************************************************************* TYPEDEF */

typedef enum QCONN {
  CLOSED,
  CLOSING,
  DISCONNECTED,
  PENDING,
  CONNECTED,
  SUBSCRIBED,
  UNKNOWN = -1
} conn_t;

typedef enum QSTATE {
  INACTIVE,
  ACTIVE,
  DISABLED,
  FULL,
  STANDBY,
  UNDEFINED = -1
} state_t;

typedef enum DBACTION {
  CREATE,
  READ,
  UPDATE,
  DELETE,
  PURGE,
  NONE = -1
} db_t;

typedef enum JSACTION {
  GET,
  SUB,
  NOT,
  UNSUB,
  OTHER = -1
} js_t;

typedef struct STATE {
  state_t code;
  char *string;
} s_state_t;

typedef struct QUEUE {
  char *uri;
  char *state;
  char *dequeuer;
  int max;
  int length;
  db_t action;
} s_queue_t;

typedef struct QUEUELIST {
  s_queue_t **queue;
  int count;
  char *name;
} s_quelist_t;

typedef struct QUEUES {
  char *db;
  char *ws;
  char *dq;
  char *sr;
  conn_t state;
  sock_t socket;
  struct mg_connection *nc;
  struct QUEUES *next;
} s_qlist_t, *p_qlist_t;

/****************************************************************** GLOBALS */

log4c_category_t *pL;

static const char uri_encode_tbl[ sizeof(int32_t) * 0x100 ] = {
/*  0       1       2       3       4       5       6       7       8       9       a       b       c       d       e       f                        */
    "%00\0" "%01\0" "%02\0" "%03\0" "%04\0" "%05\0" "%06\0" "%07\0" "%08\0" "%09\0" "%0A\0" "%0B\0" "%0C\0" "%0D\0" "%0E\0" "%0F\0"  /* 0:   0 ~  15 */
    "%10\0" "%11\0" "%12\0" "%13\0" "%14\0" "%15\0" "%16\0" "%17\0" "%18\0" "%19\0" "%1A\0" "%1B\0" "%1C\0" "%1D\0" "%1E\0" "%1F\0"  /* 1:  16 ~  31 */
    "%20\0" "%21\0" "%22\0" "%23\0" "%24\0" "%25\0" "%26\0" "%27\0" "%28\0" "%29\0" "%2A\0" "%2B\0" "%2C\0" _______ _______ "%2F\0"  /* 2:  32 ~  47 */
    _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ "%3A\0" "%3B\0" "%3C\0" "%3D\0" "%3E\0" "%3F\0"  /* 3:  48 ~  63 */
    "%40\0" _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______  /* 4:  64 ~  79 */
    _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ "%5B\0" "%5C\0" "%5D\0" "%5E\0" _______  /* 5:  80 ~  95 */
    "%60\0" _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______  /* 6:  96 ~ 111 */
    _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ _______ "%7B\0" "%7C\0" "%7D\0" _______ "%7F\0"  /* 7: 112 ~ 127 */
    "%80\0" "%81\0" "%82\0" "%83\0" "%84\0" "%85\0" "%86\0" "%87\0" "%88\0" "%89\0" "%8A\0" "%8B\0" "%8C\0" "%8D\0" "%8E\0" "%8F\0"  /* 8: 128 ~ 143 */
    "%90\0" "%91\0" "%92\0" "%93\0" "%94\0" "%95\0" "%96\0" "%97\0" "%98\0" "%99\0" "%9A\0" "%9B\0" "%9C\0" "%9D\0" "%9E\0" "%9F\0"  /* 9: 144 ~ 159 */
    "%A0\0" "%A1\0" "%A2\0" "%A3\0" "%A4\0" "%A5\0" "%A6\0" "%A7\0" "%A8\0" "%A9\0" "%AA\0" "%AB\0" "%AC\0" "%AD\0" "%AE\0" "%AF\0"  /* A: 160 ~ 175 */
    "%B0\0" "%B1\0" "%B2\0" "%B3\0" "%B4\0" "%B5\0" "%B6\0" "%B7\0" "%B8\0" "%B9\0" "%BA\0" "%BB\0" "%BC\0" "%BD\0" "%BE\0" "%BF\0"  /* B: 176 ~ 191 */
    "%C0\0" "%C1\0" "%C2\0" "%C3\0" "%C4\0" "%C5\0" "%C6\0" "%C7\0" "%C8\0" "%C9\0" "%CA\0" "%CB\0" "%CC\0" "%CD\0" "%CE\0" "%CF\0"  /* C: 192 ~ 207 */
    "%D0\0" "%D1\0" "%D2\0" "%D3\0" "%D4\0" "%D5\0" "%D6\0" "%D7\0" "%D8\0" "%D9\0" "%DA\0" "%DB\0" "%DC\0" "%DD\0" "%DE\0" "%DF\0"  /* D: 208 ~ 223 */
    "%E0\0" "%E1\0" "%E2\0" "%E3\0" "%E4\0" "%E5\0" "%E6\0" "%E7\0" "%E8\0" "%E9\0" "%EA\0" "%EB\0" "%EC\0" "%ED\0" "%EE\0" "%EF\0"  /* E: 224 ~ 239 */
    "%F0\0" "%F1\0" "%F2\0" "%F3\0" "%F4\0" "%F5\0" "%F6\0" "%F7\0" "%F8\0" "%F9\0" "%FA\0" "%FB\0" "%FC\0" "%FD\0" "%FE\0" "%FF"    /* F: 240 ~ 255 */
};

/*************************************************************** PROTOTYPES */

char *read_file(char *name);
char *state_getstring(state_t code);

char *string_copy(const char *src, size_t len);
char *string_replace(char *src, char *new, size_t len);
char *string_change(const char *in, const char *pattern, const char *by);

char *uri_pathencode (const char *src);

void queue_inititem(s_queue_t *ptr, db_t action);
void queue_delete(s_quelist_t *queues);
void queue_setCRUD(s_quelist_t *ptr, db_t action);
int queue_JSONmethod(cJSON *jrequest, cJSON **ptr, char **name, js_t *type);
int queue_JSONservices(s_quelist_t *lqueue, cJSON *jrequest);
int queue_appenditem(s_quelist_t *phdr, const char *uri, const char *state, int lngth);
int queue_updatebyuri(s_quelist_t *queue, const char *uri, char *state, int lngth);
s_quelist_t *queue_new(char* name);
s_queue_t *queue_newitem(void);
s_queue_t **queue_newlistitem(s_queue_t **queue, int i);

int sqlite_CUD(s_quelist_t *queue, const char *dbname);
int sqlite_R(s_quelist_t *lqueue, const char *dbname);
int sqlite_PURGE(const char *dqname, const char *dbname);
int sqlite_CHECK(const char *dbname);

void conf_delete(p_qlist_t);
p_qlist_t conf_read(const char *filename, const char *dbname);

void queue_setncdqname(p_qlist_t queues, struct mg_connection *nc, char *name);
void queue_setncstate(p_qlist_t queues, struct mg_connection *nc, conn_t state);
void queue_setncsocket(p_qlist_t queues, struct mg_connection *nc);
char *queue_getncdbname(p_qlist_t queues, struct mg_connection *nc);
char *queue_getncwsname(p_qlist_t queues, struct mg_connection *nc);
char *queue_getncwsstring(p_qlist_t queues, struct mg_connection *nc);
char *queue_getncdqname(p_qlist_t queues, struct mg_connection *nc);
conn_t queue_getncstate(p_qlist_t queues, struct mg_connection *nc);

void ws_purge(p_qlist_t queues);
void ws_connect(p_qlist_t queues, struct mg_mgr *mgr, mg_event_handler_t event_handler);
void ws_reconnect(p_qlist_t queues, struct mg_mgr *mgr, mg_event_handler_t event_handler);
int ws_terminate(p_qlist_t queues);

js_t event_wsframe(const char*, const char*, struct mg_connection*, char**);

#endif // FUNCTIONS_H_INCLUDED