# DEC112/DEC112 2.0 PRF rngin Subservice

__Guide to build the PRF rngin service from sources.__

For more about the DEC112 Project visit: [dec112.at](https://dec112.at)

```
Main Author: Wolfgang Kampichler

Support: <info@dec112.at>

Licence: GPLv3
```

## Overview

This is a step by step tutorial about how to build and install the DEC112 PRF rngin service using the sources downloaded from the repository. The PRF rngin is a PRF subservices that gets queue state information (uri, state, dequeuer, max, length) from a local database, processes given rules to answer requests from the ESRP.

## Prerequisites

To be able to compile the DEC112 PRF rngin, make sure that you've installed or dowdowloaded the following:

* libyaml-dev, liblog4c-dev, libsqlite3-dev, sqlite3
* mongoose.c and mongoose.h (mongoose)
* cjson.c and cjson.h (cjson)

## SQLite Database

To create a database, you may want to follow steps as explained in [SQLite tutorial](https://www.sqlitetutorial.net/). Make sure that the table containing queue states has the following structure and name. 

```c
CREATE TABLE "queues" (
	"uri"	VARCHAR(64),
	"state"	VARCHAR(16),
	"dequeuer"	VARCHAR(64),
	"max"	INT,
	"length"	INT,
	PRIMARY KEY("uri")
);
```
You may run the following commands to create the database used by rngin

```
cd ../data
sqlite3 prf.sqlite < SQLitePrfDB.sql
```

Additionally, rngin requires a YAML rules file (`./rules/rules.yml`) that includes all defined PRF rules. Conditions support a strict match, e.g. `To: sip:9144@root.dects.dec112.eu` requires exactly the same header in the SIP request to match the condition. Whereas `From: _user` just requires `user` anywhere within the `From` header, e.g. both `From: sip:user@root.dects.dec112.eu` and `To: sip:john.dow@user.eu` match the condition. An example is given below.

```        
# prf rule 0
- rule: DECTS default
  id: R0
  priority: 1
  default: sip:border@border.dects.dec112.eu
  transport: tcp
  - actions:
    add: >
      Call-Info: <urn:dec112:endpoint:chat:service.dec112.at>;purpose=dec112-ServiceId
# prf rule 1
- rule: DECTS sos
  id: R1
  priority: 1
  default: sip:border@border.dects.dec112.eu
  transport: tcp
  - conditions:
    ruri: urn:service:sos
    header: >
      To: sip:9144@root.dects.dec112.eu,
      To: _0140,
      From: _user
  - actions:
    add: >
      Call-Info: <urn:dec112:endpoint:chat:service.dec112.at>;purpose=dec112-ServiceId
    route: sip:border-rule1@border.dects.dec112.eu
# prf rule 2
- rule: DECTS sos.ambulance
  id: R2
  priority: 1
  default: sip:border@border.dects.dec112.eu
  transport: tcp
  - conditions:
    ruri: urn:service:sos.ambulance
    header: >
      To: sip:9144@root.dects.dec112.eu
  - actions:
    add: >
      Call-Info: <urn:dec112:endpoint:chat:service.dec112.at>;purpose=dec112-ServiceId
    route: sip:border@border.dects.dec112.eu
# ...
```
## Compiling and running the PRF rngin service

1. Have a look at [Clone or download the repository](https://help.github.com/en/articles/cloning-a-repository)
2. `cd src/`
3. `make` and `cp rngin ../bin`
4. `cd ../bin` and `./rngin -v -i 127.0.0.1 -p 8448 -f ../rules/rules.yml -d ../../data/prf.sqlite`<br/>(usage: `usage: rngin -i <ip/domain str> -p <port> -f <rules> -d <database>`)
5. `-v` sets rngin to verbose mode (optional)
6. Note: log4crc may require changes (refer to the example below):

```c
<?xml version="1.0" encoding="ISO-8859-1"?>
<!DOCTYPE log4c SYSTEM "">
<log4c>
    <config>
        <bufsize>0</bufsize>
        <debug level="0"/>
        <nocleanup>0</nocleanup>
    </config>
        <category name="root" priority="trace"/>
        <rollingpolicy name="logrp" type="sizewin" maxsize="26214400" maxnum="10" />
	<appender name="logfa" type="rollingfile" logdir="/home/log/" prefix="log" layout="dated" rollingpolicy="logrp" />
	<appender name="syslog" type="syslog" layout="basic"/>
        <layout name="basic" type="basic"/>
        <layout name="dated" type="dated"/>
        <category name="rngin.dbg" priority="debug" appender="stdout" />
        <category name="rngin.info" priority="info" appender="logfa" />
</log4c>
```

## Using the PRF rngin service from Kamailio (ESRP)

To utilize the rule engine from Kamailio (ESRP) you may want to edit the (`kamailio.cfg`) and add the following to the configuration file. Basically, this section creates an http request containing a JSON (tindex, tlabel, ruri, next and the whole message base64 encoded). As soon as the PRF returns a response, the result (`tindex, tlabel, statusCode, target, additionalHeaders[], additionalBodyParts[]`) is parsed and used for further request processing. The main attribute for routing is the `target`, which is the SIP URI of the next hop the request is relayed to. 

```c
...
#!substdef "!MY_PRF_URL!prfsrv=>http://127.0.0.1:8448/api/v1/prf/req!"
...
modparam("http_client", "httpcon", "MY_PRF_URL");
...
route[PRFREQUEST] {

    jansson_set("integer", "tindex", $T(id_index), "$var(prfrequest)");
    jansson_set("integer", "tlabel", $T(id_label), "$var(prfrequest)");

    jansson_set("string", "ruri", $ru, "$var(prfrequest)");
    jansson_set("string", "next", $du, "$var(prfrequest)");

    xlog("L_INFO", "$var(prfrequest)\n");

    jansson_set("string", "request", $(msg(hdrs){s.escape.common}), "$var(prfrequestesc)");
    jansson_set("string", "request", $(msg(hdrs){s.encode.base64}), "$var(prfrequest)");

    #       jansson_set("string", "request", $(mb{s.escape.common}), "$var(prfrequestesc)");
    #       jansson_set("string", "request", $(mb{s.encode.base64}), "$var(prfrequest)");

    xlog("L_INFO", "$var(prfrequestesc)\n");

    $var(res) = http_connect("prfsrv", "", "application/json", "$var(prfrequest)", "$var(result)");
    xlog("L_INFO", "PRF returned $var(res)\n");

    switch ($var(res)) {
            case /"^200":   # successful response
                    xlog("L_INFO", "$var(result)\n");

                    jansson_get("tindex", "$var(result)", "$var(tindex)");
                    jansson_get("tlabel", "$var(result)", "$var(tlabel)");

                    jansson_get("statusCode", "$var(result)", "$var(statusCode)");
                    jansson_get("target", "$var(result)", "$var(target)");

                    xlog("L_INFO", "statusCode: $var(statusCode)\n");
                    xlog("L_INFO", "target: $var(target)\n");
                    # xlog("L_INFO", "defaultTarget: $var(defaultTarget)\n");

                    jansson_array_size("additionalHeaders", $var(result), "$var(size)");

                    $var(count) = 0;
                    while($var(count) < $var(size)) {
                            jansson_get("additionalHeaders[$var(count)]", $var(result), "$var(header)");
                            jansson_get("name", $var(header), "$var(header_name)");
                            jansson_get("value", $var(header), "$var(header_value)");
                            append_hf("$var(header_name): $var(header_value)\r\n", "Accept");
                            xlog("L_INFO", "additionalHeaders[$var(count)]: $var(header_name): $var(header_value)\n");
                            $var(count) = $var(count) + 1;
                    }

                    jansson_array_size("additionalBodyParts", $var(result), "$var(size)");

                    $var(count) = 0;
                    while($var(count) < $var(size)) {
                            jansson_get("additionalBodyParts[$var(count)]", $var(result), "$var(bodyPart)");

                            jansson_array_size("additionalBodyParts[$var(count)].headers", $var(result), "$var(headersSize)");
                            $var(headersCount) = 0;
                            while($var(headersCount) < $var(headersSize)) {
                                    jansson_get("additionalBodyParts[$var(count)].headers[$var(headersCount)]", $var(result), "$var(header)");
                                    jansson_get("name", $var(header), "$var(header_name)");
                                    jansson_get("value", $var(header), "$var(header_value)");
                                    xlog("L_INFO", "additionalBodyParts[$var(count)]: headers[$var(headersCount)]: $var(header_name): $var(header_value)\n");
                                    $var(headersCount) = $var(headersCount) + 1;
                            }
                            jansson_get("additionalBodyParts[$var(count)].content", $var(result), "$var(content)");
                            xlog("L_INFO", "additionalBodyParts[$var(count)]: content: $var(content)\n");
                            $var(count) = $var(count) + 1;
                    }

                    # relay to PRF target
                    $du = $var(target); 
            break;
            default:        # any other case
                    xlog("L_INFO", "PRF returned $var(res)\n");
                    $var(target) = $du;
                    # relay to default target
            break;
    }
    return;
}
```
Right after receiving the response from the ECRF, you may add the following:
```
route(PRFREQUEST);
```

## Docker

__Guide to build a PRF rngin service docker image.__

An image is simply created using the Dockerfile example [Dockerfile](https://github.com/dec112/lost/blob/master/service/docker/Dockerfile) with the following commands.

```
cd rngin/src
cp ../docker/Dockerfile .
docker build --tag rngin:1.0 .
```
