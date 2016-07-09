int fano(unsigned long *metric, unsigned long *cycles,
	unsigned char *data,unsigned char *symbols,
	unsigned int nbits,int mettab[2][256],int delta,
	unsigned long maxcycles);
int encode(unsigned char *symbols,unsigned char *data,unsigned int nbytes);
double gen_met(int mettab[2][256],int amp,double noise,double bias,int scale);

extern unsigned char Partab[];
