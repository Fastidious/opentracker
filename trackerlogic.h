/* This software was written by Dirk Engling <erdgeist@erdgeist.org>
   It is considered beerware. Prost. Skol. Cheers or whatever.

   $id$ */

#ifndef __OT_TRACKERLOGIC_H__
#define __OT_TRACKERLOGIC_H__

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

/* Libowfat */
#include <uint16.h>
#include <uint32.h>

#define READ16(addr,offs)      ((int16_t)uint16_read((offs)+(uint8_t*)(addr)))
#define READ32(addr,offs)      ((int32_t)uint32_read((offs)+(uint8_t*)(addr)))
#define WRITE16(addr,offs,val) uint16_pack((offs)+(uint8_t*)(addr),(val))
#define WRITE32(addr,offs,val) uint32_pack((offs)+(uint8_t*)(addr),(val))

typedef uint8_t        ot_hash[20];
typedef time_t         ot_time;

/* Some tracker behaviour tunable */
#define OT_CLIENT_TIMEOUT 30
#define OT_CLIENT_TIMEOUT_CHECKINTERVAL 10
#define OT_CLIENT_TIMEOUT_SEND (60*15)
#define OT_CLIENT_REQUEST_INTERVAL (60*30)
#define OT_CLIENT_REQUEST_VARIATION (60*6)

#define OT_TORRENT_TIMEOUT_HOURS 24
#define OT_TORRENT_TIMEOUT      (60*OT_TORRENT_TIMEOUT_HOURS)

#define OT_CLIENT_REQUEST_INTERVAL_RANDOM ( OT_CLIENT_REQUEST_INTERVAL - OT_CLIENT_REQUEST_VARIATION/2 + (int)( random( ) % OT_CLIENT_REQUEST_VARIATION ) )

/* If peers come back before 10 minutes, don't live sync them */
#define OT_CLIENT_SYNC_RENEW_BOUNDARY 10

/* We maintain a list of 1024 pointers to sorted list of ot_torrent structs
   Sort key is, of course, its hash */
#define OT_BUCKET_COUNT 1024

/* Number of tracker admin ip addresses allowed */
#define OT_ADMINIP_MAX 64
#define OT_MAX_THREADS 16

#define OT_PEER_TIMEOUT 45

/* From opentracker.c */
extern time_t g_now_seconds;
#define       g_now_minutes (g_now_seconds/60)

extern uint32_t g_tracker_id;
typedef enum { FLAG_TCP, FLAG_UDP, FLAG_MCA } PROTO_FLAG;

typedef struct {
  uint8_t data[8];
} ot_peer;
static const uint8_t PEER_FLAG_SEEDING   = 0x80;
static const uint8_t PEER_FLAG_COMPLETED = 0x40;
static const uint8_t PEER_FLAG_STOPPED   = 0x20;
static const uint8_t PEER_FLAG_FROM_SYNC = 0x10;
static const uint8_t PEER_FLAG_LEECHING  = 0x00;

#define OT_SETIP(peer,ip)     WRITE32((peer),0,READ32((ip),0))
#define OT_SETPORT(peer,port) WRITE16((peer),4,READ16((port),0))
#define OT_PEERFLAG(peer)     (((uint8_t*)(peer))[6])
#define OT_PEERTIME(peer)     (((uint8_t*)(peer))[7])

#define OT_PEER_COMPARE_SIZE ((size_t)6)
#define OT_HASH_COMPARE_SIZE (sizeof(ot_hash))

struct ot_peerlist;
typedef struct ot_peerlist ot_peerlist;
typedef struct {
  ot_hash      hash;
  ot_peerlist *peer_list;
} ot_torrent;

#include "ot_vector.h"

struct ot_peerlist {
  ot_time        base;
  size_t         seed_count;
  size_t         peer_count;
  size_t         down_count;
/* normal peers vector or
   pointer to ot_vector[32] buckets if data != NULL and space == 0
*/
  ot_vector      peers;
};
#define OT_PEERLIST_HASBUCKETS(peer_list) ((peer_list) && ((peer_list)->peers.size > (peer_list)->peers.space))

/*
   Exported functions
*/

#ifdef WANT_SYNC_LIVE
#define WANT_SYNC
#endif

#ifdef WANT_SYNC
#define WANT_SYNC_PARAM( param ) , param
#else
#define WANT_SYNC_PARAM( param )
#endif

int  trackerlogic_init( const char * const serverdir );
void trackerlogic_deinit( void );
void exerr( char * message );

/* add_peer_to_torrent does only release the torrent bucket if from_sync is set,
   otherwise it is released in return_peers_for_torrent */
size_t return_peers_for_torrent( ot_torrent *torrent, size_t amount, char *reply, PROTO_FLAG proto );
ot_torrent *add_peer_to_torrent( ot_hash *hash, ot_peer *peer  WANT_SYNC_PARAM( int from_sync ) );
size_t remove_peer_from_torrent( ot_hash *hash, ot_peer *peer, char *reply, PROTO_FLAG proto );
size_t return_tcp_scrape_for_torrent( ot_hash *hash, int amount, char *reply );
size_t return_udp_scrape_for_torrent( ot_hash *hash, char *reply );

/* Helper, before it moves to its own object */
void free_peerlist( ot_peerlist *peer_list );

#endif
