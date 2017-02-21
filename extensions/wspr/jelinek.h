#ifndef JELINEK_H
#define JELINEK_H

#include <stdint.h>

struct snode {
    uint64_t encstate; // Encoder state
    int gamma;		         // Cumulative metric to this node
    unsigned int depth;               // depth of this node
    unsigned int jpointer;
};

int jelinek(unsigned int *metric,
            unsigned int *cycles,
            unsigned char *data,
            unsigned char *symbols,
            unsigned int nbits,
            unsigned int stacksize,
            struct snode *stack,
            int mettab[2][256],
            unsigned int maxcycles);

#endif

