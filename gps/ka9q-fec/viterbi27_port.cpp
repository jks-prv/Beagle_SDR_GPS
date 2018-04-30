/* K=7 r=1/2 Viterbi decoder in portable C
 * Copyright Feb 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <limits.h>
#include "fec.h"


typedef union { unsigned int w[64]; } metric_t;
typedef union { unsigned long w[2];} decision_t;
static union branchtab27 { unsigned char c[32]; } Branchtab27[2] __attribute__ ((aligned(16)));
static int Init = 0;

/* State info for instance of Viterbi decoder
 * Don't change this without also changing references in [mmx|sse|sse2]bfly29.s!
 */
struct v27 {
  metric_t metrics1; /* path metric buffer 1 */
  metric_t metrics2; /* path metric buffer 2 */
  decision_t *dp;          /* Pointer to current decision */
  metric_t *old_metrics,*new_metrics; /* Pointers to path metrics, swapped on every bit */
  decision_t *decisions;   /* Beginning of decisions for block */
};

/* Initialize Viterbi decoder for start of new frame */
int init_viterbi27_port(void *p,int starting_state){
  struct v27 *vp = (struct v27 *) p;
  int i;

  if(p == NULL)
    return -1;
  for(i=0;i<64;i++)
    vp->metrics1.w[i] = 63;

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->w[starting_state & 63] = 0; /* Bias known start state */
  return 0;
}

void set_viterbi27_polynomial_port(int polys[2]){
  int state;

  for(state=0;state < 32;state++){
    Branchtab27[0].c[state] = (polys[0] < 0) ^ parity((2*state) & abs(polys[0])) ? 255 : 0;
    Branchtab27[1].c[state] = (polys[1] < 0) ^ parity((2*state) & abs(polys[1])) ? 255 : 0;
  }
  Init++;
}

/* Create a new instance of a Viterbi decoder */
void *create_viterbi27_port(int len){
  struct v27 *vp;

  if(!Init){
    int polys[2] = { V27POLYA, V27POLYB };
    set_viterbi27_polynomial_port(polys);
  }
  if((vp = (struct v27 *) malloc(sizeof(struct v27))) == NULL)
     return NULL;
  if((vp->decisions = (decision_t *) malloc((len+6)*sizeof(decision_t))) == NULL){
    free(vp);
    return NULL;
  }
  init_viterbi27_port(vp,0);

  return vp;
}

/* Viterbi chainback */
int chainback_viterbi27_port(
      void *p,
      unsigned char *data, /* Decoded output data */
      unsigned int nbits, /* Number of data bits */
      unsigned int endstate){ /* Terminal encoder state */
  struct v27 *vp = (struct v27 *) p;
  decision_t *d;

  if(p == NULL)
    return -1;
  d = vp->decisions;
  /* Make room beyond the end of the encoder register so we can
   * accumulate a full byte of decoded data
   */
  endstate %= 64;
  endstate <<= 2;

  /* The store into data[] only needs to be done every 8 bits.
   * But this avoids a conditional branch, and the writes will
   * combine in the cache anyway
   */
  d += 6; /* Look past tail */
  while(nbits-- != 0){
    int k;

    k = (d[nbits].w[(endstate>>2)/32] >> ((endstate>>2)%32)) & 1;
    data[nbits>>3] = endstate = (endstate >> 1) | (k << 7);
  }
  return 0;
}

/* Delete instance of a Viterbi decoder */
void delete_viterbi27_port(void *p){
  struct v27 *vp = (struct v27 *) p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}

/* C-language butterfly */
#define BFLY(i) {\
unsigned int metric,m0,m1,decision;\
    metric = (Branchtab27[0].c[i] ^ sym0) + (Branchtab27[1].c[i] ^ sym1);\
    m0 = vp->old_metrics->w[i] + metric;\
    m1 = vp->old_metrics->w[i+32] + (510 - metric);\
    decision = (signed int)(m0-m1) > 0;\
    vp->new_metrics->w[2*i] = decision ? m1 : m0;\
    d->w[i/16] |= decision << ((2*i)&31);\
    m0 -= (metric+metric-510);\
    m1 += (metric+metric-510);\
    decision = (signed int)(m0-m1) > 0;\
    vp->new_metrics->w[2*i+1] = decision ? m1 : m0;\
    d->w[i/16] |= decision << ((2*i+1)&31);\
}

/* Update decoder with a block of demodulated symbols
 * Note that nbits is the number of decoded data bits, not the number
 * of symbols!
 */
int update_viterbi27_blk_port(void *p,unsigned char *syms,int nbits){
  struct v27 *vp = (struct v27 *) p;
  metric_t *tmp;
  decision_t *d;

  if(p == NULL)
    return -1;
  d = (decision_t *)vp->dp;
  while(nbits--){
    unsigned char sym0,sym1;

    d->w[0] = d->w[1] = 0;
    sym0 = *syms++;
    sym1 = *syms++;
    
    BFLY(0);
    BFLY(1);
    BFLY(2);
    BFLY(3);
    BFLY(4);
    BFLY(5);
    BFLY(6);
    BFLY(7);
    BFLY(8);
    BFLY(9);
    BFLY(10);
    BFLY(11);
    BFLY(12);
    BFLY(13);
    BFLY(14);
    BFLY(15);
    BFLY(16);
    BFLY(17);
    BFLY(18);
    BFLY(19);
    BFLY(20);
    BFLY(21);
    BFLY(22);
    BFLY(23);
    BFLY(24);
    BFLY(25);
    BFLY(26);
    BFLY(27);
    BFLY(28);
    BFLY(29);
    BFLY(30);
    BFLY(31);
    d++;
    /* Swap pointers to old and new metrics */
    tmp = vp->old_metrics;
    vp->old_metrics = vp->new_metrics;
    vp->new_metrics = tmp;
  }    
  vp->dp = d;
  return 0;
}
