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
 *  @file    functions.c
 *  @author  Wolfgang Kampichler (DEC112 2.0)
 *  @date    07-2020
 *  @version 1.0
 *
 *  @brief this file holds qngin function definitions
 */

/******************************************************************* INCLUDE */

#include "functions.h"

/********************************************************************* CONST */

s_state_t state_code[] = {
    {INACTIVE, "inactive"}, {ACTIVE, "active"},   {DISABLED, "disabled"},
    {FULL, "full"},         {STANDBY, "standby"}, {UNDEFINED, "undefined"},
};

/***************************************************************** FUNCTIONS */

/**
 * @name string_replace
 * @brief rallocate memory and replace string
 * @arg char*, char*, size_t
 * @return char* (pointer to replaced string)
 */

char *string_replace(char *src, char *new, size_t len) {
  char *dst = (char *)realloc(src, (len + 1) * sizeof(char));

  if (dst == NULL) {
    LOG4ERROR(pL, "no memory");
    return src;
  }

  memcpy(dst, new, len);
  dst[len] = '\0';

  return dst;
}

/**
 * @name string_copy
 * @brief allocate memory and copy string
 * @arg char*, size_t
 * @return char* (pointer to new string (free memory!))
 */

char *string_copy(const char *src, size_t len) {
  char *dst = (char *)malloc((len + 1) * sizeof(char));

  if (dst == NULL) {
    LOG4ERROR(pL, "no memory");
    return dst;
  }

  memcpy(dst, src, len);
  dst[len] = '\0';

  return dst;
}

/**
 * @name string_change
 * @brief replace pattern by other string
 * @arg const char *, const char *, const char *
 * @return char* (pointer to new string (free memory!))
 */

char *string_change(const char *in, const char *pattern, const char *by) {
  size_t outsize = strlen(in) + 1;

  char *res = malloc(outsize);
  size_t resoffset = 0;

  char *needle;
  while ((needle = strstr(in, pattern))) {
    // copy everything up to the pattern
    memcpy(res + resoffset, in, needle - in);
    resoffset += needle - in;

    // skip the pattern in the input-string
    in = needle + strlen(pattern);

    // adjust space for replacement
    outsize = outsize - strlen(pattern) + strlen(by);
    res = realloc(res, outsize);

    // copy the pattern
    memcpy(res + resoffset, by, strlen(by));
    resoffset += strlen(by);
  }

  // copy the remaining input
  strcpy(res + resoffset, in);

  return res;
}

/**
 * @name state_getstring
 * @brief return state string for a given code
 * @arg state_t code
 * @return char* (state string text phrase)
 */

char *state_getstring(state_t code) {
  int i = 0;

  while (state_code[i].code != code) {
    if (state_code[i].code == -1) {
      break;
    }
    i++;
  }
  return state_code[i].string;
}

/**
 * @name queue_updatebyuri
 * @arg s_quelist_t *queue, const char *uri, char *state
 * @brief update stat and action for given uri
 * @returns int (1 if uri was found and updated, otherwise 0)
 */

int queue_updatebyuri(s_quelist_t *queue, const char *uri, char *state, int lngth) {
  int i = 0;
  int rc = 0;
  s_queue_t **ptr = NULL;

  if (queue == NULL) {
    return rc;
  }

  if (queue->count == 0) {
    return rc;
  }

  if ((uri == NULL) || (state == NULL)) {
    LOG4ERROR(pL, "queue attribute missing");
    return rc;
  }

  ptr = queue->queue;

  while (i < queue->count) {
    if (ptr[i]->uri == NULL) {
      i++;
      continue;
    }
    if (strcmp(ptr[i]->uri, uri) == 0) {
      rc = 1;
      if (ptr[i]->action == NONE) {
        /* no action for this item */
        i++;
        continue;
      }
      if ((ptr[i]->action == UPDATE) || (ptr[i]->action == CREATE)) {
        /* this item was already updated or added, */
        /* only change if new state is active */
        if (strcmp(state, state_getstring(ACTIVE)) == 0) {
          ptr[i]->state = string_replace(ptr[i]->state, state, strlen(state));
          ptr[i]->length = lngth;
        }
      } else {
        if (ptr[i]->state != NULL) {
          if (strcmp(ptr[i]->state, state) == 0) {
            ptr[i]->action = NONE;
          } else {
            ptr[i]->state = string_replace(ptr[i]->state, state, strlen(state));
            ptr[i]->length = lngth;
            ptr[i]->action = UPDATE;
          }
        }
        if (ptr[i]->length != lngth) {
          /* queue length has changed and needs an update */
          ptr[i]->length = lngth;
          ptr[i]->action = UPDATE;
        }
      }
      if (ptr[i]->action == UPDATE) {

        LOG4DEBUG(pL, "\t[%s] [%s] [%s] [%d] [%d] <%d>", ptr[i]->uri,
                  ptr[i]->state, ptr[i]->dequeuer, ptr[i]->max, ptr[i]->length,
                  ptr[i]->action);
      }
      break;
    }
    i++;
  }

  return rc;
}

/**
 * @name queue_newlistitem
 * @brief reallocates memory for next queue data
 * @arg s_queue_t**, int
 * @return s_queue_t* (pointer to list item object)
 */

s_queue_t **queue_newlistitem(s_queue_t **queue, int i) {
  queue = (s_queue_t **)realloc(queue, (i + 1) * sizeof(s_queue_t *));

  if (queue == NULL) {
    LOG4ERROR(pL, "no memory");
  }

  return queue;
}

/**
 * @name queue_inititem
 * @brief initializes list item
 * @arg s_queue_t *ptr, db_t action
 * @return void
 */

void queue_inititem(s_queue_t *ptr, db_t action) {
  /* attributes */
  ptr->uri = NULL;
  ptr->state = NULL;
  ptr->dequeuer = NULL;
  ptr->max = 0;
  ptr->length = 0;
  ptr->action = action;
}

/**
 * @name queue_delete
 * @brief removes queue items and releases memory
 * @arg s_quelist_t *ptr
 * @return void
 */

void queue_delete(s_quelist_t *list) {
  int i = 0;
  s_queue_t *ptr = NULL;

  if (list == NULL) {
    return;
  }

  if (list != NULL) {
    for (i = 0; i < list->count; i++) {
      ptr = list->queue[i];
      if (ptr != NULL) {

        LOG4DEBUG(pL, "RM [%s]", ptr->uri);
        
        free(ptr->uri);
        free(ptr->state);
        free(ptr->dequeuer);
        free(ptr);
        ptr = NULL;
      }
    }
    if (list->queue != NULL) {
      free(list->queue);
    }
    if (list->name != NULL) {
      free(list->name);
    }
    free(list);
    list = NULL;
  }

  return;
}

/**
 * @name queue_new
 * @brief allocate memory to store a queuelist
 * @arg void
 * @return s_quelist_t * (pointer to queue list)
 */

s_quelist_t *queue_new(char *name) {
  s_quelist_t *ptr = NULL;

  ptr = (s_quelist_t *)malloc(sizeof(s_quelist_t));
  if (ptr == NULL) {
    LOG4ERROR(pL, "no memory");
    return ptr;
  }

  ptr->count = 0;
  ptr->queue = NULL;
  ptr->name = NULL;
  if (name != NULL) {
    ptr->name = string_copy(name, strlen(name));
  }

  return ptr;
}

/**
 * @name queue_appenditem
 * @brief appends queue item (uri, state) to list
 * @arg s_quelist_t *phdr, const char *uri, const char *state
 * @return int (1 success, otherwise 0)
 */

int queue_appenditem(s_quelist_t *phdr, const char *uri, const char *state, int lngth) {
  int cnt = 0;
  s_queue_t **ptr = NULL;

  cnt = phdr->count;
  ptr = phdr->queue;

  if ((uri == NULL) || (state == NULL)) {
    LOG4ERROR(pL, "queue attribute missing");
    return 0;
  }

  ptr = queue_newlistitem(ptr, cnt);
  if (ptr == NULL) {
    LOG4ERROR(pL, "no memory");
    return 0;
  }
  ptr[cnt] = queue_newitem();
  if (ptr[cnt] == NULL) {
    LOG4ERROR(pL, "no memory");
    phdr->count = cnt;
    phdr->queue = ptr;
    return 0;
  }

  queue_inititem(ptr[cnt], CREATE);

  ptr[cnt]->uri = string_copy(uri, strlen(uri));
  ptr[cnt]->state = string_copy(state, strlen(state));
  ptr[cnt]->dequeuer = string_copy(phdr->name, strlen(phdr->name));
  ptr[cnt]->length = lngth;

  phdr->count = cnt + 1;
  phdr->queue = ptr;

  if (ptr[cnt]->action == CREATE) {

    LOG4DEBUG(pL, "\t[%s] [%s] [%s] [%d] [%d] <%d>", ptr[cnt]->dequeuer,
              ptr[cnt]->uri, ptr[cnt]->state, ptr[cnt]->max, ptr[cnt]->length,
              ptr[cnt]->action);
  }

  return 1;
}

/**
 * @name queue_setCRUD
 * @brief sets database action for each queue list item
 * @arg s_quelist_t *ptr, db_t action
 * @return void
 */

void queue_setCRUD(s_quelist_t *ptr, db_t action) {
  int i = 0;

  if (ptr != NULL) {
    for (i = 0; i < ptr->count; i++) {
      ptr->queue[i]->action = action;
    }
  }

  return;
}

/**
 * @name queue_newitem
 * @brief returns a pointer to a queue item
 * @arg void
 * @return s_queue_t* (pointer to list item)
 */

s_queue_t *queue_newitem(void) {
  s_queue_t *queueitem = NULL;

  queueitem = (s_queue_t *)malloc(sizeof(s_queue_t));
  if (queueitem == NULL) {
    LOG4ERROR(pL, "no memory");
  }

  return queueitem;
}

/**
 * @name queue_JSONmethod
 * @brief reads and parses DEC112 border management API response messages
 * @arg cJSON *jmessage, cJSON **ptr, char **name, js_t *type
 * @return int (JSON response code)
 */

int queue_JSONmethod(cJSON *jmessage, cJSON **ptr, char **name, js_t *type) {
  int code = 0;
  char *ptrdqueue = NULL;

  cJSON *jptr = NULL;
  cJSON *jcode = NULL;
  cJSON *jresponse = NULL;
  cJSON *jhealth = NULL;
  cJSON *jservices = NULL;
  cJSON *jdequeuer = NULL;
  cJSON *jduri = NULL;
  cJSON *jdname = NULL;
  cJSON *jdstate = NULL;

  js_t jst = OTHER;

  /* check JSON response */
  if (jmessage == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      LOG4ERROR(pL, "json error before: %s", error_ptr);
      return code;
    }
  } else {
    jresponse = cJSON_GetObjectItem(jmessage, "method");
    if (jresponse == NULL) {
      jresponse = cJSON_GetObjectItem(jmessage, "event");
    }
    jcode = cJSON_GetObjectItem(jmessage, "code");
    if (jcode == NULL) {
      LOG4WARN(pL, "no response code received");
    } else {
      code = jcode->valueint;
    }

    if (jresponse == NULL) {
      LOG4WARN(pL, "unknown response received");
      return code;
    }

    LOG4DEBUG(pL, "response [%s] code [%d]", jresponse->valuestring, code);

    if (strcmp(jresponse->valuestring, "get_health") == 0) {
      jst = GET;
    } else if (strcmp(jresponse->valuestring, "health") == 0) {
      jst = NOT;
    } else if (strcmp(jresponse->valuestring, "subscribe_health") == 0) {
      jst = SUB;
    } else if (strcmp(jresponse->valuestring, "unsubscribe_health") == 0) {
      jst = UNSUB;
    } else {
      jst = OTHER;
    }

    /* parse health notification or response */
    if (((jst == GET) && (code = 200)) || ((jst == NOT) && (code = 200))) {
      jhealth = cJSON_GetObjectItem(jmessage, "health");
      jservices = cJSON_GetObjectItem(jhealth, "services");
      jdequeuer = cJSON_GetObjectItem(jhealth, "sip");

      if (jdequeuer == NULL) {
        LOG4WARN(pL, "no dequeuer attribute");
        return 0;
      }
      jduri = cJSON_GetObjectItem(jdequeuer, "uri");
      jdname = cJSON_GetObjectItem(jdequeuer, "name");
      jdstate = cJSON_GetObjectItem(jdequeuer, "state");

      if (jduri == NULL) {
        LOG4WARN(pL, "no dequeuer uri object");
        return 0;
      } else {
        ptrdqueue = jduri->valuestring;
        if (ptrdqueue == NULL) {
          LOG4WARN(pL, "no dequeuer uri");
          return 0;
        } else {
          LOG4DEBUG(pL, "received from [%s]", ptrdqueue);
          *name = string_copy(ptrdqueue, strlen(ptrdqueue));
        }
      }

      if ((jdname == NULL) || (jdstate == NULL)) {
        LOG4WARN(pL, "missing dequeuer attributes");
      } else {
        if (strcmp(jdstate->valuestring, REGOK) != 0) {
          LOG4WARN(pL, "dequeuer %s not registered", jdname->valuestring);
        }
      }
      if (jservices == NULL) {
        LOG4WARN(pL, "no services received");
        return 0;
      }

      jptr = jservices->child;
      if (jptr == NULL) {
        LOG4WARN(pL, "no service list received");
        return 0;
      }
    }
  }

  *type = jst;
  *ptr = jptr;

  return code;
}

/**
 * @name queue_JSONservices
 * @brief reads and parses DEC112 get_health and health response messages
 * @arg s_quelist_t *lqueue, cJSON *jptr
 * @return int (number of valid list entries)
 */

int queue_JSONservices(s_quelist_t *lqueue, cJSON *jptr) {
  int i = 0;
  int lngth = 0;
  char *ptruri = NULL;
  char *ptrstate = NULL;

  cJSON *juri = NULL;
  cJSON *jact = NULL;
  cJSON *jcnt = NULL;

  /* check JSON response */
  while (jptr) {

    LOG4DEBUG(pL, "%s", jptr->string);

    juri = cJSON_GetObjectItem(jptr, "queue_uri");
    jact = cJSON_GetObjectItem(jptr, "active");
    jcnt = cJSON_GetObjectItem(jptr, "active_calls");
    if (juri) {
      ptruri = juri->valuestring;
      if (jact) {
        ptrstate = state_getstring(jact->valueint);
      } else {
        ptrstate = state_getstring(UNDEFINED);
      }
      if (jcnt) {
        lngth = jcnt->valueint;
      }
      i++;
      /* check if uri is part of the list */
      if (queue_updatebyuri(lqueue, ptruri, ptrstate, lngth) == 0) {
        /* this is a new queue item ... add to list */
        queue_appenditem(lqueue, ptruri, ptrstate, lngth);
      }
    }
    jptr = jptr->next;
  }

  return i;
}

/**
 * @name event_wsframe
 * @brief handle ws events
 * @arg const char *msg, const char *dbname, struct mg_connection *nc, char **dq
 * @return js_t (JSON method received)
 */

js_t event_wsframe(const char *msg, const char *dbname,
                   struct mg_connection *nc, char **dq) {
  char *name = NULL;

  js_t jst = OTHER;
  cJSON *jmessage = NULL;
  cJSON *jptr = NULL;
  s_quelist_t *lqueue = NULL;

  if (msg == NULL) {
    LOG4WARN(pL, "empty JSON response");
    return OTHER;
  }

  /* parse JSON response */
  jmessage = cJSON_Parse(msg);

  if (queue_JSONmethod(jmessage, &jptr, &name, &jst) == 0) {
    LOG4ERROR(pL, "could not get response method");
    return 0;
  }

  LOG4DEBUG(pL, "JSON msg (truncated)\n%.*s", DBGLINE, msg);

  if ((jst == GET) || (jst == NOT)) {

    /* init queue */
    *dq = name;

    LOG4DEBUG(pL, "queue -> %s", name);

    lqueue = queue_new(name);
    if (lqueue == NULL) {
      LOG4ERROR(pL, "could not create new queue");
      return 0;
    }

    /* read current DB and create queue list */
    if (sqlite_R(lqueue, dbname) == 0) {
      LOG4DEBUG(pL, "empty database");
    } else {
      /* we may delete each element */
      queue_setCRUD(lqueue, DELETE);
    }

    /* check what remains or needs an update */
    if (queue_JSONservices(lqueue, jptr) == 0) {
      LOG4DEBUG(pL, "empty JSON response");
      queue_delete(lqueue);
    } else {
      sqlite_CUD(lqueue, dbname);
      queue_delete(lqueue);

      LOG4DEBUG(pL, "queue -> %s", name);

      lqueue = queue_new(name);
      if (lqueue == NULL) {
        LOG4ERROR(pL, "could not create new queue");
        return OTHER;
      }

      sqlite_R(lqueue, dbname);

      for (int i = 0; i < lqueue->count; i++) {

        LOG4DEBUG(pL, "EVENT [%s] [%s] [%s] [%d] [%d] <%d>",
                  lqueue->queue[i]->dequeuer, lqueue->queue[i]->uri,
                  lqueue->queue[i]->state, lqueue->queue[i]->max,
                  lqueue->queue[i]->length, lqueue->queue[i]->action);
      }
      queue_delete(lqueue);
    }
  }
  /* cleanup */
  cJSON_Delete(jmessage);

  return jst;
}

/**
 * @name conf_delete
 * @brief deletes configuration list and frees memory
 * @arg p_qlist_t list
 * @return void
 */

void conf_delete(p_qlist_t list) {

  p_qlist_t curr;

  while ((curr = list) != NULL) {
    list = curr->next;
    if (curr->db != NULL) {
      free(curr->db);
    }
    if (curr->ws != NULL) {
      free(curr->ws);
    }
    if (curr->sr != NULL) {
      free(curr->sr);
    }
    if (curr->dq != NULL) {
      free(curr->dq);
    }
    if (curr->nc != NULL) {
      curr->nc = NULL;
    }
    free(curr);
  }

  list = NULL;

  return;
}

/**
 * @name conf_read
 * @brief reads configuration (yml) and creats linked list
 * @arg const char *filename, const char *dbname
 * @return p_qlist_t
 */

p_qlist_t conf_read(const char *filename, const char *dbname) {

  int state = 0;
  int dows = 0;
  char **dp = NULL;
  char *tk = NULL;
  char *ws = NULL;
  char *sr = NULL;

  p_qlist_t list = NULL;
  p_qlist_t newlist = NULL;

  FILE *fh = fopen(filename, "r");

  yaml_parser_t parser;
  yaml_token_t token;

  if (fh == NULL) {
    LOG4ERROR(pL, "failed to open yaml file");
    return list;
  }

  if (!yaml_parser_initialize(&parser)) {
    LOG4ERROR(pL, "failed to initialize yaml parser");
    return list;
  }

  yaml_parser_set_input_file(&parser, fh);

  do {
    yaml_parser_scan(&parser, &token);
    switch (token.type) {
    case YAML_KEY_TOKEN:
      state = 0;
      break;
    case YAML_VALUE_TOKEN:
      state = 1;
      break;
    case YAML_SCALAR_TOKEN:
      tk = (char *)token.data.scalar.value;
      if (state == 0) {
        dows = 0;
        if (!strcmp(tk, "websockets")) {
          dp = &sr;
          dows = 1;
        } else {
          LOG4WARN(pL, "unrecognised key: %s\n", tk);
        }
      } else {
        *dp = strdup(tk);
        if (dows) {
          ws = string_change(sr, HASHTAG, HASHTAGENC);
          LOG4DEBUG(pL, "new list added for %s", sr);
          newlist = (p_qlist_t)malloc(sizeof(s_qlist_t));
          if (newlist == NULL) {
            LOG4ERROR(pL, "could not add list item");
            return list;
          }
          newlist->db = string_copy(dbname, strlen(dbname));
          newlist->sr = sr;
          newlist->ws = ws;
          newlist->dq = NULL;
          newlist->state = UNKNOWN;
          newlist->nc = NULL;
          newlist->next = list;
          list = newlist;
        }
      }
      break;
    default:
      break;
    }
    if (token.type != YAML_STREAM_END_TOKEN)
      yaml_token_delete(&token);
  } while (token.type != YAML_STREAM_END_TOKEN);

  yaml_token_delete(&token);
  yaml_parser_delete(&parser);

  fclose(fh);

  return list;
}

/**
 * @name queue_setncdqname
 * @brief copies state attribute of connection (nc) to linked list
 * @arg p_qlist_t queues, struct mg_connection *nc, conn_t state
 * @return void
 */

void queue_setncstate(p_qlist_t queues, struct mg_connection *nc,
                      conn_t state) {
  p_qlist_t head = queues;

  if (nc == NULL) {
    return;
  }

  while (head) {
    if (nc == head->nc) {
      head->state = state;
      break;
    }
    head = head->next;
  }

  return;
}

/**
 * @name queue_setncdqname
 * @brief copies dq attribute of connection (nc) to linked list
 * @arg p_qlist_t queues, struct mg_connection *nc, char *name
 * @return void
 */

void queue_setncdqname(p_qlist_t queues, struct mg_connection *nc, char *name) {
  p_qlist_t head = queues;

  if ((nc == NULL) || (queues == NULL)) {
    return;
  }

  while (head) {
    if (nc == head->nc) {
      head->dq = string_copy(name, strlen(name));
      break;
    }
    head = head->next;
  }

  return;
}

/**
 * @name queue_setncsocket
 * @brief copies socket FD attribute of connection (nc) to linked list
 * @arg p_qlist_t queues, struct mg_connection *nc
 * @return void
 */

void queue_setncsocket(p_qlist_t queues, struct mg_connection *nc) {
  p_qlist_t head = queues;

  if ((nc == NULL) || (queues == NULL)) {
    return;
  }

  while (head) {
    if (nc == head->nc) {
      head->socket = nc->sock;
      break;
    }
    head = head->next;
  }

  return;
}

/**
 * @name queue_getncdbname
 * @brief reads dbname from linked list of given connection
 * @arg p_qlist_t queues, sock_t s
 * @return char*
 */

char *queue_getncdbname(p_qlist_t queues, struct mg_connection *nc) {
  p_qlist_t head = queues;
  char *name = NULL;

  if (queues == NULL) {
    return name;
  }

  while (head) {
    if (nc == head->nc) {
      name = head->db;
      break;
    }
    head = head->next;
  }

  return name;
}

/**
 * @name queue_getncdqname
 * @brief reads dqname from linked list of given connection
 * @arg p_qlist_t queues, sock_t s
 * @return char*
 */

char *queue_getncdqname(p_qlist_t queues, struct mg_connection *nc) {
  p_qlist_t head = queues;
  char *name = NULL;

  if (queues == NULL) {
    return name;
  }

  while (head) {
    if (nc == head->nc) {
      name = head->dq;
      break;
    }
    head = head->next;
  }

  return name;
}

/**
 * @name queue_getncwsname
 * @brief reads ws string (encoded) from linked list of given connection
 * @arg p_qlist_t queues, struct mg_connection *nc
 * @return char*
 */

char *queue_getncwsname(p_qlist_t queues, struct mg_connection *nc) {
  p_qlist_t head = queues;
  char *name = NULL;

  if ((nc == NULL) || (queues == NULL)) {
    return name;
  }

  while (head) {
    if (nc == head->nc) {
      name = head->ws;
      break;
    }
    head = head->next;
  }

  return name;
}

/**
 * @name queue_getncwsstring
 * @brief reads ws string from linked list of given connection
 * @arg p_qlist_t queues, struct mg_connection *nc
 * @return char*
 */

char *queue_getncwsstring(p_qlist_t queues, struct mg_connection *nc) {
  p_qlist_t head = queues;
  char *name = NULL;

  if ((nc == NULL) || (queues == NULL)) {
    return name;
  }

  while (head) {
    if (nc == head->nc) {
      name = head->sr;
      break;
    }
    head = head->next;
  }

  return name;
}

/**
 * @name queue_getncstate
 * @brief reads state from linked list of given connection
 * @arg p_qlist_t queues, struct mg_connection *nc
 * @return conn_t
 */

conn_t queue_getncstate(p_qlist_t queues, struct mg_connection *nc) {
  p_qlist_t head = queues;
  conn_t state = UNKNOWN;

  if ((nc == NULL) || (queues == NULL)) {
    return state;
  }

  while (head) {
    if (nc == head->nc) {
      state = head->state;
      break;
    }
    head = head->next;
  }

  return state;
}

/**
 * @name ws_purge
 * @brief deletes database entries of dequeuer
 * @arg p_qlist_t queues
 * @return void
 */

void ws_purge(p_qlist_t queues) {

  p_qlist_t head = queues;

  while (head) {
    if ((head->state == CLOSED) || (head->state == CLOSING)) {
      LOG4WARN(pL, "socket [%d] closed, purging database", head->socket);
      char *dbname = queue_getncdbname(queues, head->nc);
      char *dqname = queue_getncdqname(queues, head->nc);

      LOG4DEBUG(pL, "purge %s %s", dbname, dqname);

      if (sqlite_PURGE(dqname, dbname) == 0) {
        LOG4ERROR(pL, "database purge failed for %s [%s]", dqname, dbname);
      }
    }
    head = head->next;
  }
}

/**
 * @name ws_reconnect
 * @brief reconnects all ws of configured queues
 * @arg p_qlist_t queues, struct mg_mgr *mgr, mg_event_handler_t ev_handler
 * @return void
 */

void ws_reconnect(p_qlist_t queues, struct mg_mgr *mgr,
                  mg_event_handler_t ev_handler) {

  p_qlist_t head = queues;

  while (head) {
    if ((head->state == DISCONNECTED) || (head->state == CLOSED)) {
      head->state = PENDING;

      LOG4DEBUG(pL, "reconnecting %s [%s]", head->ws, head->db);

      head->nc = mg_connect_ws(mgr, ev_handler, head->ws, WSPROTO, NULL);
    }
    head = head->next;
  }
}

/**
 * @name ws_connect
 * @brief connects all ws of configured queues
 * @arg p_qlist_t queues, struct mg_mgr *mgr, mg_event_handler_t ev_handler
 * @return void
 */

void ws_connect(p_qlist_t queues, struct mg_mgr *mgr,
                mg_event_handler_t ev_handler) {

  p_qlist_t head = queues;

  LOG4DEBUG(pL, "ws_connect");

  while (head) {
    if (head->state == UNKNOWN) {
      head->state = PENDING;

      LOG4DEBUG(pL, "connecting %s [%s]", head->sr, head->db);

      head->nc = mg_connect_ws(mgr, ev_handler, head->ws, WSPROTO, NULL);
    }
    head = head->next;
  }
}

/**
 * @name ws_terminate
 * @brief terminates qngin if there is no active connection left
 * @arg p_qlist_t queues
 * @return int
 */

int ws_terminate(p_qlist_t queues) {

  p_qlist_t head = queues;
  int done = 1;

  while (head) {
    if (head->state > DISCONNECTED) {
      done = 0;
    }
    head = head->next;
  }

  return done;
}

