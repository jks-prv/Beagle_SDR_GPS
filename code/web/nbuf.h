#ifndef _NBUF_H_
#define _NBUF_H_

#include "types.h"
#include "coroutines.h"
#include "mongoose.h"

typedef struct nbuf_s {
	#define NB_MAGIC 0xbabecafe
	u4_t magic;
	struct mg_connection *mc;
	char *buf;
	u2_t len, ttl, id;
	bool done, dequeued;
	struct nbuf_s *next, *prev;
} nbuf_t;

typedef struct {
	struct mg_connection *mc;
	lock_t lock;
	nbuf_t *q, *q_head;
	u2_t cnt, ttl;
	bool ovfl, dbug;
} ndesc_t;

#define	ND_HIWAT	64
#define	ND_LOWAT	32

void nbuf_allocq(ndesc_t *nd, char *s, int sl);
nbuf_t *nbuf_dequeue(ndesc_t *nd);
int nbuf_queued(ndesc_t *nd);
void nbuf_cleanup(ndesc_t *nd);

#endif
