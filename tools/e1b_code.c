#include "../types.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

int need[12], iph[12];
double ph[12];

int main()
{
    u64_t clk = 0;
    int i, p = 0, c;
    
    int prt = 0;
    while (1) {
        if ((clk % 1024) == 0) {
            for (i = 0; i < 12; i++) {
                #define	MAX_RANDOM	0x7fffffff
                ph[i] = ((double)random() / MAX_RANDOM) * (1.0/1023000.0);
                //printf("%2d %.12f\n", i, ph[i]);
                need[i] = 0;
            }
        }
    
        if (need[p]) need[p] = 0;
    
        for (c = 0; c < 12; c++) {
            ph[c] += 1.0/16368000.0;
            if (ph[c] >= 1.0/1023000.0) {
                ph[c] -= 1.0/1023000.0;
                if (need[c]) {
                    printf("%llu: c%d still needs!\n", clk, c);
                    return 0;
                }
                need[c] = 1;
            }
            iph[c]++;
            if (iph[c] == 4092) iph[c] = 0;
            if (prt) printf("%d", need[c]);
        }
        if (prt) printf("\n");
        
        p++; if (p == 12) p = 0;
        clk++;
    }

	return 0;
}
