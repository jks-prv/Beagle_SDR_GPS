#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include "types.h"
#include "config.h"
#include "wrx.h"
#include "misc.h"
#include "timer.h"
#include "web.h"
#include "coroutines.h"
#include "nbuf.h"

static void nbuf_dumpq(ndesc_t *nd)
{
	nbuf_t *bp;
	
	//lock_enter(&nd->lock);

	if (nd->dbug) printf("[");
	bp = nd->q;
	while (bp) {
		if (nd->dbug) printf("%d%s%s%d ", bp->id, bp->done? "d":".", bp->dequeued? "q":".", bp->ttl);
		bp = bp->next;
	}
	if (nd->dbug) printf("] ");

	//lock_leave(&nd->lock);
}

static bool nbuf_enqueue(ndesc_t *nd, nbuf_t *nb)
{
	nbuf_t **q = &nd->q, **q_head = &nd->q_head;
	nbuf_t *dp;
	bool ovfl = FALSE;
	
	lock_enter(&nd->lock);
	
		// collect done buffers (same thread where they're allocated)
#if 1
		// decrement ttl counters
		if (nd->ttl) {
			dp = *q_head;
			while (dp) {
				if (dp->done) {
					assert(dp->ttl);
					dp->ttl--;
				}
				dp = dp->prev;
			}
		}
		
		while ((dp = *q_head) && (((nd->ttl == 0) && dp->done) || ((nd->ttl != 0) && dp->done && (dp->ttl == 0))) ) {
			if (nd->dbug) printf("R%d ", dp->id);
			if (nd->dbug) nbuf_dumpq(nd);
			assert(dp->magic == NB_MAGIC);
			assert(dp->buf);
			wrx_free("nbuf:buf", dp->buf);
			*q_head = dp->prev;
			if (*q == dp) {
				*q = NULL;
				assert(*q_head == NULL);
			} else {
				(*q_head)->next = NULL;
			}
			dp->magic = 0;
			wrx_free("nbuf", dp);
			if (nd->dbug) nbuf_dumpq(nd);
		}
#endif
		
		if (nd->ovfl && (nd->cnt < ND_LOWAT)) {
			nd->ovfl = FALSE;
		}

		if (nd->ovfl || (nd->cnt > ND_HIWAT)) {
			if (!nd->ovfl && (nd->cnt > ND_HIWAT)) printf("HIWAT\n");
			nd->ovfl = TRUE;
			ovfl = TRUE;
		} else {
			nd->cnt++;
			assert(nb->magic == NB_MAGIC);
			if (*q) (*q)->prev = nb;
			nb->next = *q;
			*q = nb;
			nb->prev = NULL;
			if (!*q_head) *q_head = nb;
		}

	lock_leave(&nd->lock);
	
	return ovfl;
}

void nbuf_allocq(ndesc_t *nd, char *s, int sl)
{
	nbuf_t **q = &nd->q, **q_head = &nd->q_head;
	nbuf_t *nb;
	bool ovfl;
	static int id;
	
	assert(s && sl);
	nb = (nbuf_t*) wrx_malloc("nbuf", sizeof(nbuf_t));
	nb->magic = NB_MAGIC;
	//assert(nd->mc);
	nb->mc = nd->mc;
	nb->buf = (char*) wrx_malloc("nbuf:buf", sl);
	memcpy(nb->buf, s, sl);
	nb->len = sl;
	nb->done = FALSE;
	nb->dequeued = FALSE;
	nb->ttl = nd->ttl;
	if (nd->dbug) nb->id = id++;
	ovfl = nbuf_enqueue(nd, nb);
	if (nd->dbug) printf("A%d ", nb->id);
	if (nd->dbug) nbuf_dumpq(nd);
	
	if (ovfl) {
		nb->magic = 0;
		wrx_free("nbuf:buf", nb->buf);
		wrx_free("nbuf", nb);
	}
}

nbuf_t *nbuf_dequeue(ndesc_t *nd)
{
	nbuf_t **q = &nd->q, **q_head = &nd->q_head;
	nbuf_t *nb;
	
	lock_enter(&nd->lock);
	
		nb = *q_head;
		while (nb && nb->dequeued) {
			nb = nb->prev;
		}
		if (nb) {
			if (nd->dbug) printf("D%d ", nb->id);
			nb->dequeued = TRUE;
			nd->cnt--;
			if (nd->dbug) nbuf_dumpq(nd);
		}
		
	lock_leave(&nd->lock);
	
	assert(!nb || (nb && !nb->done));
	//assert(!nb || ((nb->mc->remote_port == nd->mc->remote_port) && (strcmp(nb->mc->remote_ip, nd->mc->remote_ip) == 0)));
	return nb;
}

int nbuf_queued(ndesc_t *nd)
{
	nbuf_t *bp;
	int queued = 0;
	
	lock_enter(&nd->lock);
		bp = nd->q;
		while (bp) {
			if (!bp->dequeued) {
				queued++;
			}
			bp = bp->next;
		}
	lock_leave(&nd->lock);
	
	return queued;
}

void nbuf_cleanup(ndesc_t *nd)
{
	nbuf_t **q = &nd->q, **q_head = &nd->q_head;
	nbuf_t *dp;
	int i=0;
	
	lock_enter(&nd->lock);
	
		while ((dp = *q_head) != NULL) {
			assert(dp->magic == NB_MAGIC);
			assert(dp->buf);
			wrx_free("nbuf:buf", dp->buf);
			*q_head = dp->prev;
			if (dp == *q) *q = NULL;
			dp->magic = 0;
			wrx_free("nbuf", dp);
			i++;
		}
		
		nd->cnt = 0;
		nd->ovfl = FALSE;
		
	lock_leave(&nd->lock);

	//printf("nbuf_cleanup: removed %d, malloc %d\n", i, wrx_malloc_stat());
}
