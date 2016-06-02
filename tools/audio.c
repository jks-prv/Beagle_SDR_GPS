#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main()
{
	#define DECIM_REM 0.0001
	int net_rate = 8250, out_rate = 44100;
	//int net_rate = 8250, out_rate = 16000;
	
	int interp;
	double decim, decim_rem;
	
	printf("net_rate %d, out_rate %d, DECIM_REM %f\n", net_rate, out_rate, DECIM_REM);

	for (interp = 2; interp <= 1024; interp++) {
		decim = (double) (net_rate * interp) / out_rate;
		decim_rem = fabs(decim - floor(decim));
		if (decim_rem <= DECIM_REM) {
			printf("decim_rem %.6f: interp %d decim %.0f\n", decim_rem, interp, decim);
		}
	}

	#define RATE_REM 1000
	printf("\nRATE_REM %d\n", RATE_REM);
	for (interp = 2; interp <= 1024; interp++) {
		decim = (double) (net_rate * interp) / out_rate;
		decim = floor(decim);
		int rate = interp * (int)decim;
		int rate_rem = rate - out_rate;
		float pct = -(1.0 - (float) rate / (float) out_rate)*100.0;
		if (abs(rate_rem) <= RATE_REM) {
			printf("rate_rem %d: rate %d %5.1f%% interp %d decim %.0f\n", rate_rem, rate, pct, interp, decim);
		}
	}

	return 0;
}
