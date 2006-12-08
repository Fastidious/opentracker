CC?=gcc
CFLAGS+=-I../libowfat -Wall -O2 -pipe
LDFLAGS+=-L../libowfat/ -lowfat -s

SOURCES=opentracker.c trackerlogic.c scan_urlencoded_query.c

opentracker: $(SOURCES)
	$(CC) $(SOURCES) -o opentracker $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf opentracker
