/* Test viterbi decoder speeds */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <memory.h>
#include <sys/time.h>
#include <sys/resource.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include "fec.h"

#define	MAX_RANDOM	0x7fffffff

double normal_rand(){
  const double k1 = sqrt(2/M_E);
  const double s = 0.449871;
  const double a = 0.19600;
  const double t = -0.386595;
  const double b = 0.25472;
  const double r1 = 0.27597;
  const double r2 = 0.27846;
  double u,v,x,y,Q;

  while(1){
    do {
      u = (double)random() / MAX_RANDOM;
    } while (u == 0.);
    v = k1 * 2.0 * (double)random() / MAX_RANDOM - 1;
    x = u - s;
    y = fabs(v) - t;
    Q = x*x + y*(a*y - b*x);
    if(Q < r1)
      return v/u;

    if(Q < r2 && v*v < -4*u*u*log(u))
	return v/u;
  }
}

void addnoise(unsigned char *symbols,int length,int ampl,double noise){
  float r;
  unsigned char c;

  while(length-- > 0){
    r = normal_rand();
//jks
noise=0;
printf("%d ", *symbols);
    r = noise * r + 127.5 + (*symbols ? ampl : -ampl);

    c = r > 255 ? 255 : (r < 0 ? 0 : r);
    *symbols++ = c;
  }
}

int popcnt(unsigned int v)
{
    int n;
    
	for (n = 0; v != 0; v >>= 1)
	    if (v & 1) n++;
	
	return n;
}

#if HAVE_GETOPT_LONG
struct option Options[] = {
  {"frame-length",1,NULL,'l'},
  {"frame-count",1,NULL,'n'},
  {"ebn0",1,NULL,'e'},
  {"gain",1,NULL,'g'},
  {"verbose",0,NULL,'v'},
  {NULL},
};
#endif

#define RATE (1./2.)
#define MAXBYTES 10000

double Gain = 100.0;
int Verbose = 0;

int main(int argc,char *argv[]){
  int i,d,tr;
  int sr=0,trials = 10000,errcnt,framebits=2048;
  long long int tot_errs=0;
  unsigned char bits[MAXBYTES];
  unsigned char data[MAXBYTES];
  unsigned char xordata[MAXBYTES];
  unsigned char symbols[8*2*(MAXBYTES+6)];
  void *vp;
  extern char *optarg;
  struct rusage start,finish;
  double extime;
  double noise,esn0,ebn0;
  time_t t;
  int badframes=0;

  time(&t);
  srandom(t);
  ebn0 = -100;

#if HAVE_GETOPT_LONG
  while((d = getopt_long(argc,argv,"l:n:e:g:v",Options,NULL)) != EOF){
#else
  while((d = getopt(argc,argv,"l:n:e:g:v")) != EOF){
#endif
    switch(d){
    case 'l':
      framebits = atoi(optarg);
      break;
    case 'n':
      trials = atoi(optarg);
      break;
    case 'e':
      ebn0 = atof(optarg);
      break;
    case 'g':
      Gain = atof(optarg);
      break;
    case 'v':
      Verbose++;
      break;
    }
  }
  if(framebits > 8*MAXBYTES){
    fprintf(stderr,"Frame limited to %d bits\n",MAXBYTES*8);
    framebits = MAXBYTES*8;
  }
  if((vp = create_viterbi27_port(framebits)) == NULL){
    printf("create_viterbi27 failed\n");
    exit(1);
  }
  if(ebn0 != -100){
    esn0 = ebn0 + 10*log10((double)RATE); /* Es/No in dB */
    /* Compute noise voltage. The 0.5 factor accounts for BPSK seeing
     * only half the noise power, and the sqrt() converts power to
     * voltage.
     */
    noise = Gain * sqrt(0.5/pow(10.,esn0/10.));
    
    printf("Viterbi27 test: nframes = %d framesize = %d ebn0 = %.2f dB gain = %g noise = %g\n",trials,framebits,ebn0,Gain,noise);
    
    for(tr=0;tr<trials;tr++){
      /* Encode a frame of random data */
      for(i=0;i<framebits+6;i++){
	int bit = (i < framebits) ? (random() & 1) : 0;
	
	sr = (sr << 1) | bit;
	bits[i/8] = sr & 0xff;
	symbols[2*i+0] = parity(sr & V27POLYA);
	symbols[2*i+1] = parity(sr & V27POLYB);
      }	
      addnoise(symbols,2*(framebits+6),Gain,noise);
      /* Decode it and make sure we get the right answer */
      /* Initialize Viterbi decoder */
      init_viterbi27_port(vp,0);
      
      /* Decode block */
for(i=0;i<framebits+6;i++) printf("%d %d ", symbols[2*i+0], symbols[2*i+1]);
printf("\n");
      update_viterbi27_blk_port(vp,symbols,framebits+6);
      
      /* Do Viterbi chainback */
for(i=0;i<framebits/8;i++) data[i] = 0xaa;
      chainback_viterbi27_port(vp,data,framebits,0);
for(i=0;i<framebits/8;i++) printf("0x%08x ", data[i]);
printf("\n");
      errcnt = 0;
      for(i=0;i<framebits/8;i++){
	int e = popcnt(xordata[i] = data[i] ^ bits[i]);
	errcnt += e;
	printf("trial %d framebits %d errors %d\n", tr, framebits, e);
	tot_errs += e;
      }
      if(errcnt != 0)
	badframes++;
      if(Verbose > 1 && errcnt != 0){
	printf("frame %d, %d errors: ",tr,errcnt);
	for(i=0;i<framebits/8;i++){
	  printf("%02x",xordata[i]);
	}
	printf("\n");
      }
      if(Verbose)
	printf("BER %lld/%lld (%10.3g) FER %d/%d (%10.3g)\r",
	       tot_errs,(long long)framebits*(tr+1),tot_errs/((double)framebits*(tr+1)),
	       badframes,tr+1,(double)badframes/(tr+1));
      fflush(stdout);
    }
    if(Verbose > 1)
      printf("nframes = %d framesize = %d ebn0 = %.2f dB gain = %g\n",trials,framebits,ebn0,Gain);
    else if(Verbose == 0)
      printf("BER %lld/%lld (%.3g) FER %d/%d (%.3g)\n",
	     tot_errs,(long long)framebits*trials,tot_errs/((double)framebits*trials),
	     badframes,tr+1,(double)badframes/(tr+1));
    else
      printf("\n");

  } else {
    /* Do time trials */
    memset(symbols,127,sizeof(symbols));
    printf("Starting time trials\n");
    getrusage(RUSAGE_SELF,&start);
    for(tr=0;tr < trials;tr++){
      /* Initialize Viterbi decoder */
      init_viterbi27_port(vp,0);
      
      /* Decode block */
      update_viterbi27_blk_port(vp,symbols,framebits);
      
      /* Do Viterbi chainback */
      chainback_viterbi27_port(vp,data,framebits,0);
    }
    getrusage(RUSAGE_SELF,&finish);
    extime = finish.ru_utime.tv_sec - start.ru_utime.tv_sec + 1e-6*(finish.ru_utime.tv_usec - start.ru_utime.tv_usec);
    printf("Viterbi27 execution time for %d %d-bit frames: %.2f sec\n",trials,
	   framebits,extime);
    printf("decoder speed: %g bits/s\n",trials*framebits/extime);
  }
  exit(0);
}
