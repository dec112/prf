# !!! UNDER CONSTRUCTION !!! #

# DEC112/DEC112 PRF rngin Subservice

__Guide to build the PRF qngin service from sources.__

For more about the DEC112 Project visit: [dec112.at](https://dec112.at)

```
Main Author: Wolfgang Kampichler

Support: <info@dec112.at>

Licence: GPLv3
```

## Overview

This is a step by step tutorial about how to build and install the DEC112 PRF rngin service using the sources downloaded from the repository. The PRF rqngin is a PRF subservices that gets queue state information (uri, state, dequeuer, max, length) from a local database, processes given rules to answer requests from the ESRP.

## Prerequisites

To be able to compile the DEC112 PRF qngin, make sure that you've installed or dowdowloaded the following:

* libyaml-dev, liblog4c-dev, libsqlite3-dev, sqlite3
* mongoose.c and mongoose.h (mongoose)
* cjson.c and cjson.h (cjson)

## SQLite Database
