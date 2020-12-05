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
 */

/**
 *  @file    functions.h
 *  @author  Wolfgang Kampichler (DEC112 2.0)
 *  @date    04-2020
 *  @version 1.0
 *
 *  @brief rngin header file
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

#define TRUE true
#define FALSE false

#define COMMA ","
#define COLON ":"
#define PREFIX '_'

#define SEP_CRLF '\n'

#define SEP_COMMA ","
#define SEP_HDR ": "
#define SEP_SPACE " "

#define CFG_FILE "config.yaml"
#define TPSTR "%s;transport=%s"
#define HINFO "History-Info"
#define HIDX0 "<%s>;index=1.0"
#define ROUTE "Route"
#define FROM "From"
#define TO "To"

#define S_NONE 0
#define S_RULE 1
#define S_RUID 2
#define S_DFLT 3
#define S_TRPT 4
#define S_PRIO 5

#define S_COND 10
#define S_WEEK 11
#define S_TIME 12
#define S_RURI 13
#define S_SHDR 14
#define S_NEXT 15

#define S_QUEUE 20
#define S_QSURI 21
#define S_QSTAT 22
#define S_QSIZE 23
#define S_QPRIO 24

#define S_ACT 100
#define S_ADD 101
#define S_RTE 102

#define T_AT 0
#define T_FT 1

#define MAX_HDR_LINE 256

#define SCAN_STRING(tk, args...) sscanf(tk, "%[^\n]s", ##args)
#define SCAN_INTEGER(tk, args...) sscanf(tk, "%d", ##args)

#define TIME_ATTRIBUTES                                                        \
  {                                                                            \
    {"TIME", "%5s", "hh:mm", 1}, {"RANGE", "%5s-%5s", "hh:mm-hh:mm", 2}, {     \
      NULL, NULL, NULL, 0                                                      \
    }                                                                          \
  }

#define QUEUE_ATTRIBUTES                                                       \
  {                                                                            \
    {"SIZE", "%1s%s", NULL, 2}, { NULL, NULL, NULL, 0 }                        \
  }

#define SIP_URI_SCHEME "sip:"
#define SIPS_URI_SCHEME "sips:"
#define TEL_URI_SCHEME "tel:"

/* RULE ELEMENTS */
#define NODE_RULE(tk) (strstr(tk, "rule"))
#define NODE_RUID(tk) (strstr(tk, "id"))
#define NODE_DFLT(tk) (strstr(tk, "default"))
#define NODE_TRPT(tk) (strstr(tk, "transport"))
#define NODE_PRIO(tk) (strstr(tk, "priority"))

/* CONDITION ELEMENTS */
#define NODE_COND(tk) (strstr(tk, "conditions"))
#define NODE_WEEK(tk) (strstr(tk, "day"))
#define NODE_TIME(tk) (strstr(tk, "time"))
#define NODE_RURI(tk) (strstr(tk, "ruri"))
#define NODE_SHDR(tk) (strstr(tk, "header"))
#define NODE_NEXT(tk) (strstr(tk, "next"))

/* QUEUE ELEMENTS */
#define NODE_QUEUE(tk) (strstr(tk, "queues"))
#define NODE_QSURI(tk) (strstr(tk, "uri"))
#define NODE_QSTAT(tk) (strstr(tk, "state"))
#define NODE_QSIZE(tk) (strstr(tk, "size"))
#define NODE_QPRIO(tk) (strstr(tk, "prio"))

/* ACTION ELEMENTS */
#define NODE_ACT(tk) (strstr(tk, "actions"))
#define NODE_ADD(tk) (strstr(tk, "add"))
#define NODE_RTE(tk) (strstr(tk, "route"))

/* ERROR RESPONSE */
#define ERR_DEFAULT "sip:unknown@domain.invalid"
#define ERR_RESP                                                               \
  "{\"target\":\"%s\",\"statusCode\":500,"                                     \
  "\"additionalHeaders\":[],\"additionalBodyParts\":[],"                       \
  "\"tindex\":0,\"tlabel\":0}"

#define ERR_RESP_STATIC                                                        \
  "{\"target\":\"\",\"statusCode\":500,"                                       \
  "\"additionalHeaders\":[],\"additionalBodyParts\":[],"                       \
  "\"tindex\":0,\"tlabel\":0}"

/* DEBUG MACROS */
#define DEBUG_PRINT(fmt, args...)                                              \
  fprintf(stderr, "DEBUG: %s():%d: " fmt, __func__, __LINE__, ##args)
#define ERROR_PRINT(fmt, args...)                                              \
  fprintf(stderr, "ERROR: %s():%d: " fmt, __func__, __LINE__, ##args)

/************************************************************** DEFINE LOG4C */

#define LOGCATDBG "rngin.dbg"
#define LOGCAT "rngin.info"

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

typedef struct CFG {
  const char *dbfile;
  const char *rulefile;
} s_cfg_t;

typedef struct ATTR {
  char *attr;
  char *format;
  char *str;
  int fields;
} s_attr_t;

typedef struct QUEUE {
  char *uri;
  char *state;
  char *size;
  int prio;
} s_queue_t;

typedef struct HDR {
  char *name;
  char *value;
} s_hdr_t;

typedef struct QUEUELIST {
  s_queue_t **queue;
  int count;
  int maxprio;
} s_quelist_t;

typedef struct HDRLIST {
  s_hdr_t **header;
  int count;
} s_hdrlist_t;

typedef struct RULE {
  char *name;
  char *id;
  char *fallback;
  char *transport;
  char *weekday;
  char *time;
  char *ruri;
  char *header;
  char *next;
  char *add;
  char *route;
  int prio;
  int valid;
  int hits;
  int use;
  s_hdrlist_t *fblst;
  s_hdrlist_t *timelst;
  s_hdrlist_t *addlst;
  s_hdrlist_t *hdrlst;
  s_quelist_t *quelst;
} s_rule_t;

typedef struct RULELIST {
  s_rule_t **rules;
  int count;
  int maxprio;
  int maxhits;
} s_rulelist_t;

typedef struct INPUT {
  char *ruri;
  char *next;
  char *shdr;
} s_input_t;

typedef struct QUERY {
  char *state;
  int max;
  int length;
} s_query_t;

/****************************************************************** GLOBALS */

log4c_category_t *pL;

static const char colon = ':';

static const unsigned char base64_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/****************************************************************PROTOTYPES */

int sqlite_QUERY(s_query_t *, char *, const char *);
int sqlite_CHECK(const char *);

unsigned char *base64_encode(const unsigned char *, size_t, size_t *);
unsigned char *base64_decode(const unsigned char *, size_t, size_t *);

void delete_string(char *);
char *copy_string(const char *, size_t);
char *replace_string(char *, char *, size_t);
char *extract_string(const char *, const char *, const char *);
char *parse_string(char *, size_t, int);
char *extract_sipuri(const char *);
int parse_integer(char *, int);

s_hdr_t **new_list(s_hdr_t **, int);
void init_list(s_hdr_t *);
s_hdr_t *new_listitem(void);
void delete_list(s_hdrlist_t *);

s_query_t *new_query(void);
void init_query(s_query_t *);
void delete_query(s_query_t *);
int remove_list_hdr(s_hdrlist_t *);
int append_list_hdr(s_hdrlist_t *, const char *, const char *);
s_hdrlist_t *parse_list_crlf(const char *, const char *);
s_hdrlist_t *parse_list_comma(const char *, const char *);
s_rule_t **new_rule(s_rule_t **, int);
s_queue_t **new_queue(s_queue_t **, int);
void init_queue(s_queue_t *);
void delete_queue(s_quelist_t *);
s_rule_t *new_ruleitem(void);
s_queue_t *new_queueitem(void);
const s_attr_t *get_scanner(const s_attr_t *, const char *);
char *get_listvalbyname(s_hdrlist_t *, const char *);
int get_queuebyprio(s_quelist_t *, const int);

bool check_time(char *, char *);
bool check_string(const char *, const char *);
bool check_queuestate(const char *, const char *);
bool check_queuesize(char *, int, int);

bool cond_day(const char *, s_rule_t *);
bool cond_nexturi(const char *, s_rule_t *);
bool cond_ruri(const char *, s_rule_t *);
bool cond_header(s_hdrlist_t *, s_rule_t *, s_hdrlist_t *);
bool cond_queue(s_quelist_t *, s_rule_t *, s_input_t *, char *, const char *);
bool cond_time(s_hdrlist_t *, s_rule_t *);
bool cond_setroute(s_quelist_t *, s_rule_t *, s_input_t *, char *);

void set_state(const char *, int *);

s_rulelist_t *parse_rule(const char *);
void init_rule(s_rule_t *);
void delete_rule(s_rulelist_t *);
void print_rule(s_rulelist_t *, bool);
void validate_rule(s_input_t *, s_rulelist_t *, s_hdrlist_t *, const char *);
void select_rule(s_input_t *, s_rulelist_t *, s_hdrlist_t *);

void *get_jsonresponse(s_rulelist_t *, char *, size_t *);
void ev_handler(struct mg_connection *, int, void *);

#endif // FUNCTIONS_H_INCLUDED
