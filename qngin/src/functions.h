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

/*************************************************************** PROTOTYPES */

char *read_file(char *name);
char *state_getstring(state_t code);

char *string_copy(const char *src, size_t len);
char *string_replace(char *src, char *new, size_t len);
char *string_change(const char *in, const char *pattern, const char *by);
char *string_queryencode(const char *in);

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