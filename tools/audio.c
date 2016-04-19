#include <stdio.h>
#include <math.h>

int main()
{
	int net_rate = 8250, out_rate = 16000;
	
	int interp;
	double decim, decim_rem;
	
	printf("net_rate %d, out_rate %d\n", net_rate, out_rate);
	for (interp = 2; interp <= 1024; interp++) {
		decim = (double) (net_rate * interp) / out_rate;
		decim_rem = fabs(decim - floor(decim));
		if (decim_rem < 0.0001) {
			printf("interp %d decim %.6f\n", interp, decim);
		}
	}
	return(0);
}
