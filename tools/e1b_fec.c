#include "types.h"
#include "fec.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>

extern int encode(
unsigned char *symbols,
unsigned char *data,
unsigned int nbytes,
unsigned int startstate,
unsigned int endstate);

extern int init_viterbi();
extern int viterbi(char *logfile, unsigned char *ibuf, int ilen, unsigned char *obuf, int *olen);

int main()
{
    int i, j;
    
    const int input[121] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,0,0,0,0, 1,1,0,0,1,1,0,0, 1,0,1,0,1,0,1,0, 0,0,0,0,0,0,0,0, 0,0,0,0,1,1,1,1,
        0,0,1,1,0,0,1,1, 0,1,0,1,0,1,0,1, 1,1,1,0,0,0,1,1, 1,1,1,0,1,1,0,0, 1,1,0,1,1,1,1,1, 1,0,0,0,1,0,1,0,
        0,0,0,1,1,1,0,0, 0,0,0,1,0,0,1,1, 0,1,0,0,0,0,0,0,
        -1
    };
    
    int out[240];
    int op=0;
    
    int q[6];
    for (j=0; j<6; j++) q[j] = 0;
    
    // [2,1,7] CC(171, 133-inverted)
    #define G2_INVERTED 1
    int in;
    for (i=0; (in = input[i]) != -1; i++) {
        u1_t g1 = in ^ q[0] ^ q[1] ^ q[2] ^ q[5];
        u1_t g2 = in ^ q[1] ^ q[2] ^ q[4] ^ q[5] ^ G2_INVERTED;
        //printf("%3d: ", i);
        //for (j=0; j<6; j++) printf("%d", q[j]);
        //printf(" g1=%d g2=%d\n", g1, g2);
        out[op++] = g1;
        out[op++] = g2;
        
        for (j=5; j>0; j--) {
            q[j] = q[j-1];
        }
        q[0] = in;
    }
    //printf("\n");
    
    printf("our encode:\n");
    for (i=0; i<240; i++) {
        printf("%d", out[i]);
        if ((i%8) == 7) printf(" ");
    }
    printf("\n");
    
    ////////////////////////////////
    
    u1_t data[120] = {
        1,1,1,1,1,1,1,1, 1,1,1,1,0,0,0,0, 1,1,0,0,1,1,0,0, 1,0,1,0,1,0,1,0, 0,0,0,0,0,0,0,0, 0,0,0,0,1,1,1,1,
        0,0,1,1,0,0,1,1, 0,1,0,1,0,1,0,1, 1,1,1,0,0,0,1,1, 1,1,1,0,1,1,0,0, 1,1,0,1,1,1,1,1, 1,0,0,0,1,0,1,0,
        0,0,0,1,1,1,0,0, 0,0,0,1,0,0,1,1, 0,1,0,0,0,0,0,0,
    };
    
    u1_t syms[240+12];
    
    #if 1
        printf("viterbi.c encode:\n");
        encode(syms, data, 120/8, 0, 0);
    #else
        printf("copy of our encode:\n");
        for (i=0; i<(240+12); i++) syms[i] = out[i];
    #endif
    
    for (i=0; i<240; i++) {
        printf("%d", syms[i]);
        if ((i%8) == 7) printf(" ");
    }
    printf("\n");
    
    printf("diffs:\n");
    for (i=0; i<240; i++) {
        printf("%c", (out[i] != syms[i])? 'X':'.');
        if ((i%8) == 7) printf(" ");
    }
    printf("\n");
    
    printf("\ninput:\n");
    for (i=0; i<120; i++) {
        printf("%d", data[i]);
        if ((i%8) == 7) printf(" ");
    }
    printf("\n\n");

    u1_t deco[120];
    for (i=0; i<120; i++) deco[i] = 8;
    int decolen;
    u1_t deco8[120/8];
    
    printf("viterbi.c decode:\n");
    init_viterbi();
    viterbi(NULL, syms, (120+6)*2, deco, &decolen);
    //printf("decolen=%d\n", decolen);
    for (i=0; i<120; i++) {
        printf("%d", deco[i]);
        if ((i%8) == 7) printf(" ");
    }
    printf("\n");

    printf("diffs:\n");
    for (i=0; i<120; i++) {
        printf("%c", (data[i] != deco[i])? 'X':'.');
        if ((i%8) == 7) printf(" ");
    }
    printf("\n");
    
    printf("\nviterbi27.c decode:\n");
    int polys[2] = { 0x4f, -0x6d };         // k=7; Galileo E1B; "-" means G2 inverted
    set_viterbi27_polynomial_port(polys);
    int framebits = 120;
    void *v27 = create_viterbi27_port(/*len*/ framebits);
    for (i=0; i<240; i++) syms[i] = syms[i]? 255:0;
    update_viterbi27_blk_port(v27, syms, /*npairs*/ framebits+6);
    chainback_viterbi27_port(v27, deco8, /*nbits*/ framebits, /*endstate*/ 0);
    for (i=0; i<120/8; i++) {
        u1_t b8 = deco8[i];
        for (j=0; j<8; j++) {
            u1_t b1 = (b8 >> (7-j)) & 1;
            printf("%d", b1);
            deco[i*8+j] = b1;
        }
        printf(" ");
    }
    printf("\n");
    
    printf("diffs:\n");
    for (i=0; i<120; i++) {
        printf("%c", (data[i] != deco[i])? 'X':'.');
        if ((i%8) == 7) printf(" ");
    }
    printf("\n");
    
    // block interleaver: 30 cols (r++), 8 rows (w++)
    printf("\nencoded & interleaved:\n");
    int blk[8][30];
    op=0;
    for (int c=0; c < 30; c++) {
        for (int r=0; r < 8; r++) {
            blk[r][c] = out[op++];
        }
    }
    
    op=0;
    for (int r=0; r < 8; r++) {
        for (int c=0; c < 30; c++) {
            printf("%d", blk[r][c]);
            if ((op%8) == 7) printf(" ");
            op++;
        }
    }
    printf("\n");
    
	return 0;
}
