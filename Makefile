#flags
CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lldap -s

#targets
all: webproxy

webproxy: webproxy.o config.o header_parser.o rate_lib.o relay_comms.o
	$(CC) $(CFLAGS) -o $@ $^

webproxy.o: webproxy.c 
	$(CC) $(CFLAGS) -c webproxy.c 

config.o : config.c
	$(CC) $(CFLAGS) -c config.c	

tester: tester.o config.o
	$(CC) $(CFLAGS) -o $@ tester.o config.o

tester.o: tester.c
	$(CC) $(CFLAGS) -c $ tester.c

clean:
	rm -f *.o webproxy tests


rate_lib.o : rate_lib.c rate_lib.h
	$(CC) $(CFLAGS) -c rate_lib.c

header_parser.o : header_parser.c header_parser.h
	$(CC) $(CFLAGS) -c header_parser.c 

tests.o : tests.c 
	$(CC) $(CFLAGS) -c tests.c 

tests : relay_comms.o header_parser.o tests.o rate_lib.o 
	@echo -------- Compiling header_parser --------- 
	$(CC) $(CFLAGS)  header_parser.o tests.o rate_lib.o \
	relay_comms.o -o tests

relay_comms.o : relay_comms.c relay_comms.h
	$(CC) $(CFLAGS) -c relay_comms.c 

