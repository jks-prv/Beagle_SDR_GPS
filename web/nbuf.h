#ifndef _NBUF_H_
#define _NBUF_H_

#include "types.h"
#include "coroutines.h"     // lock_t
#include "mongoose.h"       // struct mg_connection *

#define NBUF_MAGIC_B	0xbbbbbbbb
#define NBUF_MAGIC_E	0xbbbbeeee

typedef struct nbuf_st {
	#define NB_MAGIC 0xbabecafe
	u4_t magic;
	struct mg_connection *mc;
	char *buf;
	u2_t len, ttl, id;
	bool done, expecting_done, dequeued, isFree;
	u4_t magic_b;
	struct nbuf_st *next, *prev;
	u4_t magic_e;
} nbuf_t;

#define NDESC_MAGIC_B	0xddddbbbb
#define NDESC_MAGIC_E	0xddddeeee

typedef struct {
	struct mg_connection *mc;
	lock_t lock;
	u4_t magic_b;
	nbuf_t *q, *q_head;
	u4_t magic_e;
	u2_t cnt, ttl;
	bool ovfl, dbug;
} ndesc_t;

#define	ND_HIWAT	64
#define	ND_LOWAT	32

void ndesc_init(ndesc_t *nd, struct mg_connection *mc);
void ndesc_register(ndesc_t *nd);

void nbuf_init();
void nbuf_stat();
void nbuf_allocq(ndesc_t *nd, char *s, int sl);
nbuf_t *nbuf_dequeue(ndesc_t *nd);
int nbuf_queued(ndesc_t *nd);
void nbuf_cleanup(ndesc_t *nd);

#endif
