############
# RNGN - A DEC112 2.0 PRF SubService
# (C) Wolfgang Kampichler 2020
# License: GNU GPL Version 3 or later, please see attached gpl-3.0.txt
# or http://www.gnu.org/copyleft/gpl.html
#############

CFLAGS  := -g -O0 -Wall -Werror=implicit-function-declaration -Werror=implicit-int
LDFLAGS := -Wl,--export-dynamic -lrt -lsqlite3 -lm -llog4c -lyaml

qngin: qngin.o functions.o sqlite.o cjson.o mongoose.o
	gcc $(CFLAGS) -o qngin qngin.o sqlite.o cjson.o mongoose.o functions.o $(LDFLAGS)

qngin.o: qngin.c
	gcc $(CFLAGS) -c qngin.c

functions.o: functions.c functions.h
	gcc $(CFLAGS) -c functions.c

cjson.o: cjson.c cjson.h
	gcc $(CFLAGS) -c cjson.c

sqlite.o: sqlite.c sqlite.h
	gcc $(CFLAGS) -c sqlite.c

mongoose.o: mongoose.c mongoose.h
	gcc $(CFLAGS) -c mongoose.c

clean:
	rm *.o
	rm qngin

