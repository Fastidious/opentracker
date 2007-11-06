CC?=gcc
FEATURES=-DWANT_TRACKER_SYNC #-DWANT_CLOSED_TRACKER -DWANT_UTORRENT1600_WORKAROUND #-DWANT_IP_FROM_QUERY_STRING -D_DEBUG_HTTPERROR -DWANT_TRACKER_SYNC
OPTS_debug=-g -ggdb #-pg # -fprofile-arcs -ftest-coverage
OPTS_production=-s -Os
CFLAGS+=-I../libowfat -Wall -pipe -Wextra #-pedantic #-ansi
LDFLAGS+=-L../libowfat/ -lowfat
 
BINARY = opentracker
HEADERS=trackerlogic.h scan_urlencoded_query.h ot_mutex.h ot_stats.h ot_sync.h ot_vector.h ot_clean.h
SOURCES=opentracker.c trackerlogic.c scan_urlencoded_query.c ot_mutex.c ot_stats.c ot_sync.c ot_vector.c ot_clean.c
 
all: $(BINARY) $(BINARY).debug

CFLAGS_production = $(CFLAGS) $(OPTS_production) $(FEATURES)
CFLAGS_debug = $(CFLAGS) $(OPTS_debug) $(FEATURES)

$(BINARY): $(SOURCES) $(HEADERS)
	$(CC) -o $@ $(SOURCES) $(CFLAGS_production) $(LDFLAGS)
$(BINARY).debug: $(SOURCES) $(HEADERS)
	$(CC) -o $@ $(SOURCES) $(CFLAGS_debug) $(LDFLAGS)
 
clean:
	rm -rf opentracker opentracker.debug *.o *~
