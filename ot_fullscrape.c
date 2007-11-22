/* This software was written by Dirk Engling <erdgeist@erdgeist.org>
   It is considered beerware. Prost. Skol. Cheers or whatever. */

/* System */
#include <sys/uio.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#ifdef WANT_COMPRESSION_GZIP
#include <zlib.h>
#endif

/* Libowfat */
#include "byte.h"
#include "textcode.h"

/* Opentracker */
#include "trackerlogic.h"
#include "ot_mutex.h"
#include "ot_iovec.h"
#include "ot_fullscrape.h"

/* Fetch full scrape info for all torrents
   Full scrapes usually are huge and one does not want to
   allocate more memory. So lets get them in 512k units
*/
#define OT_SCRAPE_CHUNK_SIZE (512*1024)

/* "d8:completei%zde10:downloadedi%zde10:incompletei%zdee" */
#define OT_FULLSCRAPE_MAXENTRYLEN 256

/* Forward declaration */
static void fullscrape_make( int *iovec_entries, struct iovec **iovector, ot_tasktype mode );

/* Converter function from memory to human readable hex strings
   XXX - Duplicated from ot_stats. Needs fix. */
static char*to_hex(char*d,ot_byte*s){char*m="0123456789ABCDEF";char *t=d;char*e=d+40;while(d<e){*d++=m[*s>>4];*d++=m[*s++&15];}*d=0;return t;}

/* This is the entry point into this worker thread
   It grabs tasks from mutex_tasklist and delivers results back
*/
static void * fullscrape_worker( void * args) {
  int iovec_entries;
  struct iovec *iovector;

  args = args;

  while( 1 ) {
    ot_tasktype tasktype = TASK_FULLSCRAPE;
    ot_taskid   taskid   = mutex_workqueue_poptask( &tasktype );
    fullscrape_make( &iovec_entries, &iovector, tasktype );
    if( mutex_workqueue_pushresult( taskid, iovec_entries, iovector ) )
      iovec_free( &iovec_entries, &iovector );
  }
  return NULL;
}

static pthread_t thread_id;
void fullscrape_init( ) {
  pthread_create( &thread_id, NULL, fullscrape_worker, NULL );
}

void fullscrape_deinit( ) {
  pthread_cancel( thread_id );
}

void fullscrape_deliver( int64 socket, ot_tasktype tasktype ) {
  mutex_workqueue_pushtask( socket, tasktype );
}

static void fullscrape_make( int *iovec_entries, struct iovec **iovector, ot_tasktype mode ) {
  int   bucket;
  char *r, *re;
#ifdef WANT_COMPRESSION_GZIP
  char  compress_buffer[OT_FULLSCRAPE_MAXENTRYLEN];
  z_stream strm;
#endif

  /* Setup return vector... */
  *iovec_entries = 0;
  *iovector = NULL;
  if( !( r = iovec_increase( iovec_entries, iovector, OT_SCRAPE_CHUNK_SIZE ) ) )
    return;

  /* ... and pointer to end of current output buffer.
     This works as a low watermark */
  re = r + OT_SCRAPE_CHUNK_SIZE;

#ifdef WANT_COMPRESSION_GZIP
  if( mode & TASK_FLAG_GZIP ) {
    byte_zero( &strm, sizeof(strm) );
    strm.next_in = (ot_byte*)r;
    if( deflateInit2(&strm,9,Z_DEFLATED,31,8,Z_DEFAULT_STRATEGY) != Z_OK )
      fprintf( stderr, "not ok.\n" );

    strm.next_out  = (unsigned char*)r;
    strm.avail_out = OT_SCRAPE_CHUNK_SIZE;
    r = compress_buffer;
  }
#endif

  /* Reply dictionary only needed for bencoded fullscrape */
  if( ( mode & TASK_TASK_MASK ) == TASK_FULLSCRAPE ) {
    memmove( r, "d5:filesd", 9 );
    r += 9;
  }

  /* For each bucket... */
  for( bucket=0; bucket<OT_BUCKET_COUNT; ++bucket ) {
    /* Get exclusive access to that bucket */
    ot_vector *torrents_list = mutex_bucket_lock( bucket );
    size_t tor_offset;

    /* For each torrent in this bucket.. */
    for( tor_offset=0; tor_offset<torrents_list->size; ++tor_offset ) {
      /* Address torrents members */
      ot_peerlist *peer_list = ( ((ot_torrent*)(torrents_list->data))[tor_offset] ).peer_list;
      ot_hash     *hash      =&( ((ot_torrent*)(torrents_list->data))[tor_offset] ).hash;

      switch( mode & TASK_TASK_MASK ) {
      case TASK_FULLSCRAPE:
      default:
        /* push hash as bencoded string */
        *r++='2'; *r++='0'; *r++=':';
        memmove( r, hash, 20 ); r+=20;

        /* push rest of the scrape string */
        r += sprintf( r, "d8:completei%zde10:downloadedi%zde10:incompletei%zdee", peer_list->seed_count, peer_list->down_count, peer_list->peer_count-peer_list->seed_count );
        break;
      case TASK_FULLSCRAPE_TPB_ASCII:
        to_hex( r, *hash ); r+=40;
        r += sprintf( r, ":%zd:%zd\n", peer_list->seed_count, peer_list->peer_count-peer_list->seed_count );
        break;
      case TASK_FULLSCRAPE_TPB_BINARY:
        memmove( r, hash, 20 ); r+=20;
        *(ot_dword*)r++ = htonl( (uint32_t)peer_list->seed_count );
        *(ot_dword*)r++ = htonl( (uint32_t)( peer_list->peer_count-peer_list->seed_count) );
        break;
      case TASK_FULLSCRAPE_TPB_URLENCODED:
        r += fmt_urlencoded( r, (char *)*hash, 20 );
        r += sprintf( r, ":%zd:%zd\n", peer_list->seed_count, peer_list->peer_count-peer_list->seed_count );
        break;        
      }

#ifdef WANT_COMPRESSION_GZIP
      if( mode & TASK_FLAG_GZIP ) {
        strm.next_in  = (ot_byte*)compress_buffer;
        strm.avail_in = r - compress_buffer;
        if( deflate( &strm, Z_NO_FLUSH ) != Z_OK )
          fprintf( stderr, "Not ok.\n" );
        r = (char*)strm.next_out;
      }
#endif

      /* If we reached our low watermark in buffer... */
      if( re - r <= OT_FULLSCRAPE_MAXENTRYLEN ) {

        /* crop current output buffer to the amount really used */
        iovec_fixlast( iovec_entries, iovector, OT_SCRAPE_CHUNK_SIZE - ( re - r ) );
        
        /* And allocate a fresh output buffer at the end of our buffers list */
        if( !( r = iovec_increase( iovec_entries, iovector, OT_SCRAPE_CHUNK_SIZE ) ) ) {
        
          /* If this fails: free buffers */
          iovec_free( iovec_entries, iovector );

#ifdef WANT_COMPRESSION_GZIP
          deflateEnd(&strm);
#endif

          /* Release lock on current bucket and return */
          mutex_bucket_unlock( bucket );
          return;
        }
        
        /* Adjust new end of output buffer */
        re = r + OT_SCRAPE_CHUNK_SIZE;

#ifdef WANT_COMPRESSION_GZIP
        if( mode & TASK_FLAG_GZIP ) {
          strm.next_out  = (ot_byte*)r;
          strm.avail_out = OT_SCRAPE_CHUNK_SIZE;
        }
#endif
      }
#ifdef WANT_COMPRESSION_GZIP
      if( mode & TASK_FLAG_GZIP ) {
        r = compress_buffer;
      }
#endif
    }
    
    /* All torrents done: release lock on currenct bucket */
    mutex_bucket_unlock( bucket );
  }

  /* Close bencoded scrape dictionary if necessary */
  if( ( mode & TASK_TASK_MASK ) == TASK_FULLSCRAPE ) {
    *r++='e'; *r++='e';
  }

#ifdef WANT_COMPRESSION_GZIP
  if( mode & TASK_FLAG_GZIP ) {
    strm.next_in  = (ot_byte*) compress_buffer;
    strm.avail_in = r - compress_buffer;
    if( deflate( &strm, Z_FINISH ) != Z_STREAM_END )
      fprintf( stderr, "Not ok.\n" );
    r = (char*)strm.next_out;
	  deflateEnd(&strm);
  }
#endif

  /* Release unused memory in current output buffer */
  iovec_fixlast( iovec_entries, iovector, OT_SCRAPE_CHUNK_SIZE - ( re - r ) );
}
