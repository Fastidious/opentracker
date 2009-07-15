/* This software was written by Dirk Engling <erdgeist@erdgeist.org>
   It is considered beerware. Prost. Skol. Cheers or whatever.

   $id$ */

/* System */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

/* Libowfat */
#include "byte.h"
#include "scan.h"
#include "ip6.h"
#include "mmap.h"

/* Opentracker */
#include "trackerlogic.h"
#include "ot_accesslist.h"
#include "ot_vector.h"

/* GLOBAL VARIABLES */
#ifdef WANT_ACCESSLIST
char *g_accesslist_filename;
static ot_vector accesslist;

static void accesslist_reset( void ) {
  free( accesslist.data );
  byte_zero( &accesslist, sizeof( accesslist ) );
}

void accesslist_deinit( void ) {
  accesslist_reset( );
}

static int accesslist_addentry( ot_vector *al, ot_hash infohash ) {
  int eger;
  void *insert = vector_find_or_insert( al, infohash, OT_HASH_COMPARE_SIZE, OT_HASH_COMPARE_SIZE, &eger );

  if( !insert )
    return -1;

  memcpy( insert, infohash, OT_HASH_COMPARE_SIZE );

  return 0;
}

/* Read initial access list */
static void accesslist_readfile( int sig ) {
  ot_hash   infohash;
  ot_vector accesslist_tmp;
  void     *olddata;
  char     *map, *map_end, *read_offs;
  size_t    maplen;

  if( sig != SIGHUP ) return;

  if( ( map = mmap_read( g_accesslist_filename, &maplen ) ) == NULL ) {
    char *wd = getcwd( NULL, 0 );
    fprintf( stderr, "Warning: Can't open accesslist file: %s (but will try to create it later, if necessary and possible).\nPWD: %s\n", g_accesslist_filename, wd );
    free( wd );
    return;
  }

  /* Initialise an empty accesslist vector */
  memset( &accesslist_tmp, 0, sizeof(accesslist_tmp));

  /* No use */
  map_end = map + maplen - 40;
  read_offs = map;

  /* We do ignore anything that is not of the form "^[:xdigit:]{40}[^:xdigit:].*" */
  while( read_offs < map_end ) {
    int i;
    for( i=0; i<(int)sizeof(ot_hash); ++i ) {
      int eger = 16 * scan_fromhex( read_offs[ 2*i ] ) + scan_fromhex( read_offs[ 1 + 2*i ] );
      if( eger < 0 )
        continue;
      infohash[i] = eger;
    }

    read_offs += 40;

    /* Append accesslist to accesslist vector */
    if( scan_fromhex( *read_offs ) < 0 )
      accesslist_addentry( &accesslist_tmp, infohash );

    /* Find start of next line */
    while( read_offs < map_end && *(read_offs++) != '\n' );
  }
#ifdef _DEBUG
  fprintf( stderr, "Added %zd info_hashes to accesslist\n", accesslist_tmp.size );
#endif

  mmap_unmap( map, maplen);

  /* Now exchange the accesslist vector in the least race condition prone way */
  accesslist.size = 0;
  olddata = accesslist.data;
  memcpy( &accesslist, &accesslist_tmp, sizeof( &accesslist_tmp ));
  free( olddata );  
}

int accesslist_hashisvalid( ot_hash hash ) {
  int exactmatch;
  binary_search( hash, accesslist.data, accesslist.size, OT_HASH_COMPARE_SIZE, OT_HASH_COMPARE_SIZE, &exactmatch );

#ifdef WANT_ACCESSLIST_BLACK
  exactmatch = !exactmatch;
#endif

  return exactmatch;
}

void accesslist_init( ) {
  byte_zero( &accesslist, sizeof( accesslist ) );

  /* Passing "0" since read_blacklist_file also is SIGHUP handler */
  if( g_accesslist_filename ) {
    accesslist_readfile( SIGHUP );
    signal( SIGHUP, accesslist_readfile );
  }
}
#endif

static ot_ip6         g_adminip_addresses[OT_ADMINIP_MAX];
static ot_permissions g_adminip_permissions[OT_ADMINIP_MAX];
static unsigned int   g_adminip_count = 0;

int accesslist_blessip( ot_ip6 ip, ot_permissions permissions ) {
  if( g_adminip_count >= OT_ADMINIP_MAX )
    return -1;

  memcpy(g_adminip_addresses + g_adminip_count,ip,sizeof(ot_ip6));
  g_adminip_permissions[ g_adminip_count++ ] = permissions;

#ifdef _DEBUG
  {
    char _debug[512];
    int off = snprintf( _debug, sizeof(_debug), "Blessing ip address " );
    off += fmt_ip6c(_debug+off, ip );

    if( permissions & OT_PERMISSION_MAY_STAT       ) off += snprintf( _debug+off, 512-off, " may_fetch_stats" );
    if( permissions & OT_PERMISSION_MAY_LIVESYNC   ) off += snprintf( _debug+off, 512-off, " may_sync_live" );
    if( permissions & OT_PERMISSION_MAY_FULLSCRAPE ) off += snprintf( _debug+off, 512-off, " may_fetch_fullscrapes" );
    if( permissions & OT_PERMISSION_MAY_PROXY      ) off += snprintf( _debug+off, 512-off, " may_proxy" );
    if( !permissions ) off += snprintf( _debug+off, sizeof(_debug)-off, " nothing\n" );
    _debug[off++] = '.';
    write( 2, _debug, off );
  }
#endif

  return 0;
}

int accesslist_isblessed( ot_ip6 ip, ot_permissions permissions ) {
  unsigned int i;
  for( i=0; i<g_adminip_count; ++i )
    if( !memcmp( g_adminip_addresses + i, ip, sizeof(ot_ip6)) && ( g_adminip_permissions[ i ] & permissions ) )
      return 1;
  return 0;
}

const char *g_version_accesslist_c = "$Source: /home/cvsroot/opentracker/ot_accesslist.c,v $: $Revision: 1.24 $\n";
