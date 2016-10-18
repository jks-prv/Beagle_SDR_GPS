#include <stdio.h>
#include <math.h>

int main()
{
	//double clk = 66666600, cdiv = 6944;
	double clk = 66666070, cdiv = 6944;
	double srate_f = clk / cdiv;
	
	#define NFFT 1024
	#define REP 3.6
	//#define REP 7.11
	
	double fft_ms = 1.0 / (srate_f / NFFT);
	int bins = trunc(REP / fft_ms);
	
	printf("srate %.2f fft_ms %.3f bins %.1f/%d\n", srate_f, fft_ms*1e3, REP / fft_ms, bins);
	
	int i, o_ibin = -1, seq = 1;
	double fi=0;
	for (i=0; i < bins*10; i++) {
		double fr = fmod(fi, REP);
		double fb = fr / REP;
		double fbin = fb * bins;
		int ibin = trunc(fbin);
		if (ibin == 0) { printf("\n"); seq = 1; }
		printf("%7.3fs %5.3f %5.3f %6.3f %2d %s %2d %d\n", fi, fr, fb, fbin, ibin,
			(ibin == o_ibin)? "*":" ", seq, (int) trunc(6*fb));
		o_ibin = ibin;
		fi += fft_ms;
		seq++;
	}
	
	return(0);
}
