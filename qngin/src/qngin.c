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
 *  @file    qngin.c
 *  @author  Wolfgang Kampichler (DEC112 2.0)
 *  @date    08-2020
 *  @version 1.0
 *
 *  @brief this file holds the main function definition
 */

/******************************************************************* INCLUDE */

#include "functions.h"
#include "mongoose.h"
#include <signal.h>

/******************************************************************* GLOBALS */

static int s_done = 0;
static int s_closed = 0;
static sig_atomic_t s_signal_received = 0;

/******************************************************************* SIGNALS */

static void signal_handler(int sig_num) {
  signal(sig_num, signal_handler);
  s_signal_received = sig_num;
}

/******************************************************************** EVENTS */

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  /* get configured queues via user data */
  p_qlist_t queues = (p_qlist_t)nc->mgr->user_data;

  /* switch socket events */
  switch (ev) {
  case MG_EV_CONNECT: {
    int status = *((int *)ev_data);
    if (status != 0) {
      LOG4ERROR(pL, "connection error: %d [%d]", status, nc->sock);
      queue_setncstate(queues, nc, DISCONNECTED);
    }
    /* store socket descriptor of this connection */
    queue_setncsocket(queues, nc);
    break;
  }
  case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
    struct http_message *hm = (struct http_message *)ev_data;
    /* get websocket string */
    char *wsname = queue_getncwsname(queues, nc);
    /* read HTTP response code */
    if (hm->resp_code == 101) {
      /* connection ok, set state */
      queue_setncstate(queues, nc, CONNECTED);
      LOG4INFO(pL, "connected to %s [%d]", wsname, nc->sock);
      /* request health information */

      LOG4DEBUG(pL, "requesting get_health via [%d]", nc->sock);

      mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, GETHEALTH,
                              strlen(GETHEALTH));
    } else {
      /* connection failed, set state */
      queue_setncstate(queues, nc, DISCONNECTED);
      LOG4ERROR(pL, "connection to %s failed with HTTP code %d", wsname,
                hm->resp_code);
    }
    break;
  }
  case MG_EV_POLL: {
    p_qlist_t head = queues;
    /* user interrupt,  gracefully shutdown all connections */
    if (s_signal_received != 0) {
      s_signal_received = 0;
      printf("\n");
      LOG4INFO(pL, "received interrupt [SIGTERM|SIGINT]");
      while (head) {
        if (head->state == SUBSCRIBED) {
          mg_send_websocket_frame(head->nc, WEBSOCKET_OP_TEXT, UNSHEALTH,
                                  strlen(UNSHEALTH));

          LOG4DEBUG(pL, "unsubscribing health %s", head->dq);

          queue_setncstate(queues, head->nc, CLOSING);
        }
        head = head->next;
      }
    }
    break;
  }
  case MG_EV_WEBSOCKET_FRAME: {
    /* read and parse received data */
    struct websocket_message *wm = (struct websocket_message *)ev_data;
    char *str = string_copy((const char *)wm->data, wm->size);
    char *name = NULL;

    if (str == NULL) {
      LOG4WARN(pL, "empty JSON response");
      break;
    }
    /* get database for this connection */
    char *dbname = queue_getncdbname(queues, nc);
    /* parse frame and write received data to database */
    js_t jst = event_wsframe(str, dbname, nc, &name);
    /* cleanup */
    free(str);

    /* get_health response */
    if (jst == GET) {
      /* store dequeuer name for this connection */
      queue_setncdqname(queues, nc, name);

      LOG4DEBUG(pL, "subscribing health %s", name);

      /* subscribe_health right after get_health */
      mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, SUBHEALTH,
                              strlen(SUBHEALTH));
    }

    /* subscribe_health response */
    if (jst == SUB) {
      /* connection ok, set state */
      queue_setncstate(queues, nc, SUBSCRIBED);

      LOG4DEBUG(pL, "subscribed health %s", queue_getncdqname(queues, nc));
    }

    /* health notification */
    if (jst == NOT) {

      LOG4DEBUG(pL, "health notify %s", queue_getncdqname(queues, nc));
    }

    /* unsubscribe_health response */
    if (jst == UNSUB) {
      conn_t state = queue_getncstate(queues, nc);
      if ((state == SUBSCRIBED) || (state == CLOSING)) {

        LOG4DEBUG(pL, "unsubscribed health %s", queue_getncdqname(queues, nc));

        if (state == SUBSCRIBED) {
          /* unsubscribe ok, set state */
          queue_setncstate(queues, nc, CLOSED);
          s_closed = 1;
        }
      }
    }
    /* cleanup */
    if (name != NULL) {
      free(name);
    }
    break;
  }
  case MG_EV_CLOSE: {
    LOG4WARN(pL, "connection close event");
    if (queue_getncstate(queues, nc) > DISCONNECTED) {
      /* probably lost connection, set state */
      queue_setncstate(queues, nc, CLOSED);
    }
    /* set closed flag */
    s_closed = 1;
    break;
  }
  }
}

/********************************************************************** MAIN */

int main(int argc, char **argv) {
  struct mg_mgr mgr;

  const char *sCfgFile = NULL;
  const char *sDbUrl = NULL;
  const char *sLogCat = NULL;

  int opt = 0;

  p_qlist_t queues;

  // get arguments
  if (argc < 3) {
    printf("usage: qngin -s <websocket> -d <database>\n");
    exit(0);
  }

  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);

  sLogCat = LOGCAT; // LOGCATDBG;

  while ((opt = getopt(argc, argv, "c:d:v")) != -1) {
    switch (opt) {
    case 'v':
      sLogCat = LOGCATDBG;
      break;
    case 'c':
      sCfgFile = optarg;
      break;
    case 'd':
      sDbUrl = optarg;
      break;
    case '?':
      if (optopt == 'c') {
        ERROR_PRINT("missing configuaration file\n");
      } else if (optopt == 'd') {
        ERROR_PRINT("missing database url\n");
      } else {
        ERROR_PRINT("invalid option received\n");
      }
      exit(0);
      break;
    }
  }

  if (log4c_init()) {
    ERROR_PRINT("can't initialize logging\n");
    exit(0);
  }

  pL = log4c_category_get(sLogCat);

  if (pL == NULL) {
    ERROR_PRINT("can't get log category\n");
    exit(0);
  }

  LOG4INFO(pL, "qngin started");

  queues = conf_read(sCfgFile, sDbUrl);
  if (queues == NULL) {
    LOG4ERROR(pL, "could not read configuration");
    log4c_fini();
    exit(0);
  }

  if (sqlite_CHECK(sDbUrl) == 0) {
    LOG4ERROR(pL, "could not open database");
    log4c_fini();
    exit(0);  
  }

  mg_mgr_init(&mgr, NULL);
  mgr.user_data = (void *)queues;

  ws_connect(queues, &mgr, ev_handler);

  while (!s_done) {

    mg_mgr_poll(&mgr, 500);

    /* connection closed event */
    if (s_closed) {
      s_closed = 0;
      /* purge queue database if disconnected or closed */
      ws_purge(queues);
    }

    /* leave event loop if termination is pending */
    if (ws_terminate(queues)) {
      s_done = 1;
      /* purge database */
      ws_purge(queues);
      LOG4WARN(pL, "no active connnection, shutting down qngin ...");
    } else {
      /* immediately reconnect after disconnect */
      ws_reconnect(queues, &mgr, ev_handler);
    }
  }

  /* cleanup */
  mg_mgr_free(&mgr);
  conf_delete(queues);

  LOG4INFO(pL, "qngin terminated");

  log4c_fini();

  return 0;
}