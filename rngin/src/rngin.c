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
 *  @file    rngin.c
 *  @author  Wolfgang Kampichler (DEC112 2.0)
 *  @date    04-2020
 *  @version 1.0
 *
 *  @brief this file holds the main function definition
 */

/******************************************************************* INCLUDE */

#include "functions.h"
#include <signal.h>

/******************************************************************* GLOBALS */

static sig_atomic_t s_signal_received = 0;

/******************************************************************* SIGNALS */

static void signal_handler(int sig_num) {
    signal(sig_num, signal_handler);
    s_signal_received = sig_num;
}

/********************************************************************** MAIN */
int main(int argc, char *argv[]) {
    struct mg_mgr mgr;
    struct mg_connection *nc;
    struct mg_bind_opts bind_opts;

    const char *strHttpPort = NULL;
    const char *strLogCat = NULL;
    const char *strIPAddr = NULL;
    const char *strDBName = NULL;
    const char *strYamlFile = NULL;

    char s_ip_port[256];
    int opt = 0;

    FILE *fh = NULL;
    s_cfg_t *cfg = NULL;

    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    strLogCat = LOGCAT;

    while ((opt = getopt(argc, argv, "i:p:f:d:v")) != -1) {
        switch(opt) {
        case 'v':
            strLogCat = LOGCATDBG;
            break;
        case 'i':
            strIPAddr = optarg;
            break;
        case 'f':
            strYamlFile = optarg;
            break;
        case 'd':
            strDBName = optarg;
            break;
        case 'p':
            strHttpPort = optarg;
            break;
        case '?':
            if (optopt == 'i') {
                ERROR_PRINT("Option -%c requires ip address as argument\n", optopt);
            } else if (optopt == 'p') {
                ERROR_PRINT("Option -%c requires listen port as argument\n", optopt);
            } else if (optopt == 'f') {
                ERROR_PRINT("Option -%c requires rules file as argument\n", optopt);
            } else if (optopt == 'd') {
                ERROR_PRINT("Option -%c requires sqlite database name as argument\n", optopt);
            } else {
                ERROR_PRINT("Unknown option `-%c'.\n", optopt);
            }
            exit(0);
            break;
        }
    }

    if ((strIPAddr == NULL) || (strHttpPort == NULL) || (strDBName == NULL) || (strYamlFile == NULL)) {
        ERROR_PRINT("usage: rngin -i <ip/domain str> -p <listening port> -f <rules file> -d <db file>\n");
        exit(0);
    }

    if (log4c_init()) {
        ERROR_PRINT("can't initialize logging\n");
        exit(0);
    }

    pL = log4c_category_get(strLogCat);

    if (pL == NULL) {
        ERROR_PRINT("can't get log category\n");
        exit(0);
    }

    LOG4INFO(pL, "rngin started");

    LOG4DEBUG(pL, "ip/domain string: %s", strIPAddr);
    LOG4DEBUG(pL, "listening port: %s", strHttpPort);
    LOG4DEBUG(pL, "rules file: %s", strYamlFile);
    LOG4DEBUG(pL, "sqlite database: %s", strDBName);

    fh = fopen(strYamlFile, "r");
    if (fh == NULL) {
        LOG4ERROR(pL, "could not read rules file: %s", strYamlFile);
        log4c_fini();
        exit(0);
    }
    fclose(fh);

    if (sqlite_CHECK(strDBName) == 0) {
        LOG4ERROR(pL, "could not open database: %s", strDBName);
        log4c_fini();
        exit(0);  
    }

// config
    cfg = (s_cfg_t*)malloc(sizeof(s_cfg_t));
    if (cfg == NULL) {
        LOG4ERROR(pL, "could not allocate memory");
        log4c_fini();
        exit(0);  
    }

    cfg->dbfile = strDBName;
    cfg->rulefile = strYamlFile;

    snprintf(s_ip_port, 255, "%s:%s", strIPAddr, strHttpPort);

// initiate mongoose
    mg_mgr_init(&mgr, NULL);
    mgr.user_data = (void *)cfg;

    memset(&bind_opts, 0, sizeof(bind_opts));

    nc = mg_bind_opt(&mgr, s_ip_port, ev_handler, bind_opts);

    if (!nc) {
        ERROR_PRINT("could not bind port: %s\n", strHttpPort);
        exit(0);
    }

// set up HTTP server parameters
    mg_set_protocol_http_websocket(nc);
    //s_http_server_opts.document_root = ".";  // Serve current directory
    //s_http_server_opts.dav_document_root = ".";  // Allow access via WebDav
    //s_http_server_opts.enable_directory_listing = "yes";

// start server
    while (s_signal_received == 0) {
        mg_mgr_poll(&mgr, 1000);
    }

// stop and cleanup
    printf("\n");
    LOG4INFO(pL, "rngin stopped");

    mg_mgr_free(&mgr);
    free(cfg);

    log4c_fini();

    return 0;
}


