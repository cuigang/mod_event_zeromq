################################
### FreeSwitch headers files found in libfreeswitch-dev ###
FS_INCLUDES=/usr/local/freeswitch/include/freeswitch
FS_LIB=/usr/local/freeswitch/lib
FS_MODULES=/usr/local/freeswitch/mod
################################


CC=gcc 
CFLAGS=-fPIC -O3 -fomit-frame-pointer -fno-exceptions -Wall -std=c99 -pedantic

INCLUDES=-I/usr/local/include -I$(FS_INCLUDES)
LDFLAGS=-lm -L/usr/local/lib -lzmq -L$(FS_LIB) -lfreeswitch

all: mod_event_zeromq.o
	$(CC) $(CFLAGS) $(INCLUDES) -shared -Xlinker -x -o mod_event_zeromq.so mod_event_zeromq.o $(LDFLAGS)

mod_event_zeromq.o: mod_event_zeromq.c
	$(CC) $(CFLAGS) $(INCLUDES) -c mod_event_zeromq.c

clean:
	rm -f *.o *.so *.a *.la

install: all
	/usr/bin/install -c mod_event_zeromq.so $(FS_MODULES)/mod_event_zeromq.so