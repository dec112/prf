# DEC112/DEC112 PRF qngin service

__Guide to build the PRF qngin service from sources.__

For more about the DEC112 Project visit: [dec112.at](https://dec112.at)

```
Main Author: Wolfgang Kampichler

Support: <info@dec112.at>

Licence: GPLv3
```

## Overview

This is a step by step tutorial about how to build and install the DEC112 PRF qngin service using the sources downloaded from the repository. The PRF qngin is a PRF subservices that collects queue state information (uri, state, dequeuer, max, length) from other services like the DEC112 Border and writes it to a local database. 

## Prerequisites

To be able to compile the DEC112 PRF qngin, make sure that you've installed or dowdowloaded the following:
* libyaml-dev, liblog4c-dev, libsqlite3-dev, sqlite3
* mongoose.c and mongoose.h ([mongoose](https://github.com/cesanta/mongoose))
* cjson.c and cjson.h ([cjson](https://github.com/DaveGamble/cJSON))

## SQLite Database

To create a database, you may want to follow steps as explained in [SQLite tutorial](https://www.sqlitetutorial.net/). Make sure that the table containing queue states has the following structure and name. 

```
CREATE TABLE "queues" (
	"uri"	VARCHAR(64),
	"state"	VARCHAR(16),
	"dequeuer"	VARCHAR(64),
	"max"	INT,
	"length"	INT,
	PRIMARY KEY("uri")
);
```
You may run the following commands to create the database used by qngin

```
cd ../data
sqlite3 prf.sqlite < SQLitePrfDB.sql
```

Additionally, qngin requires a YAML configuration file (`./config/config.yml`) that lists all DEC112 Border services to which qngin subscribes HEALTH information. An example is given below.

```        
websockets:
    - ws://yourservice1:8000/mgmt/api/v1?api_key=yourkey1
    - ws://yourservice2:8000/mgmt/api/v1?api_key=yourkey2
    - ws://yourservice3:8000/mgmt/api/v1?api_key=yourkey3
# ...
```
## Compiling and running the PRF qngin service

1. Have a look at [Clone or download the repository](https://help.github.com/en/articles/cloning-a-repository)
2. `cd src/`
3. `make` and `cp qngin ../bin`
4. `cd ../bin` and `./qngin -v -c ../config/config.yml -d ../../data/prf.sqlite` (usage: `qngin -v -c <config> -d <database>`)
5. `-v` sets qngin to verbose mode (optional)
6. Note: log4crc may require changes (refer to the example below):

```
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
        <category name="qngin.dbg" priority="debug" appender="stdout" />
        <category name="qngin.info" priority="info" appender="logfa" />
</log4c>
```

## Docker

__Guide to build a PRF qngin service docker image.__

An image is simply created using the Dockerfile example [Dockerfile](https://github.com/dec112/lost/blob/master/service/docker/Dockerfile) with the following commands.

```
cd qngin/src
cp ../docker/Dockerfile .
docker build --tag qngin:1.0 .
```

