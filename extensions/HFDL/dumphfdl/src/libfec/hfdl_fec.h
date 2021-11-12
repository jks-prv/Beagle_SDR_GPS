/* User include file for libfec
 * Copyright 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */

#ifndef _FEC_H_
#define _FEC_H_

/* r=1/2 k=7 convolutional encoder polynomials
 * The NASA-DSN convention is to use V27POLYA inverted, then V27POLYB
 * The CCSDS/NASA-GSFC convention is to use V27POLYB, then V27POLYA inverted
 */
#define	V27POLYA	0x6d
#define	V27POLYB	0x4f

void *create_viterbi27(int len);
void set_viterbi27_polynomial(int polys[2]);
int init_viterbi27(void *vp,int starting_state);
int update_viterbi27_blk(void *vp,unsigned char sym[],int npairs);
int chainback_viterbi27(void *vp, unsigned char *data,unsigned int nbits,unsigned int endstate);
void delete_viterbi27(void *vp);

#endif /* _FEC_H_ */
