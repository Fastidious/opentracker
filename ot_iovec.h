/* This software was written by Dirk Engling <erdgeist@erdgeist.org>
   It is considered beerware. Prost. Skol. Cheers or whatever. */

#ifndef __OT_IOVEC_H__
#define __OT_IOVEC_H__

#include <sys/uio.h>

void  *iovec_increase( int *iovec_entries, struct iovec **iovector, size_t new_alloc );
void   iovec_fixlast( int *iovec_entries, struct iovec **iovector, size_t new_alloc );
void   iovec_free( int *iovec_entries, struct iovec **iovector );
size_t iovec_length( int *iovec_entries, struct iovec **iovector );

#endif
