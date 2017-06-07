#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void interp_decim(int net_rate, int out_rate, double target_rem)
{
	int interp;
	double decim, decim_rem;

	printf("net_rate %d, out_rate %d, target_rem %f\n", net_rate, out_rate, target_rem);
	for (interp = 1; interp <= 1024; interp++) {
		decim = (double) (net_rate * interp) / out_rate;
		decim_rem = fabs(decim - floor(decim));
		if (decim_rem <= target_rem) {
			printf("decim_rem %.6f: interp %d decim %.0f\n", decim_rem, interp, decim);
		}
		if (decim_rem == 0) {
		    printf("EXACT MATCH\n");
		    return;
		}
	}
}

double min_decim_rem;
int min_decim1, min_decim2;

void decim_2stage(int in_rate, int out_rate, double target_rem)
{
	int decim1;
	double decim2, decim_rem, decim2_floor;

	printf("in_rate %d, out_rate %d, decim %.3f\n", in_rate, out_rate, (double) in_rate / out_rate);
	min_decim_rem = 9999;
	for (decim1 = 1; decim1 <= 1024; decim1++) {
		decim2 =  ((double) in_rate / decim1) / out_rate;
		decim2_floor = floor(decim2);
		decim_rem = fabs(decim2 - decim2_floor);
		if (decim_rem < min_decim_rem) {
			min_decim_rem = decim_rem;
			min_decim1 = decim1;
			min_decim2 = (int) decim2_floor;
		}
		if (decim_rem < target_rem) {
			printf("OKAY  decim_rem %.6f %.3f: decim1 %d (%.4f kHz) decim2 %d (%.4f MHz)\n",
				decim_rem, (double)in_rate/decim1/decim2_floor, decim1, in_rate/decim1/1e3, (int) decim2_floor, in_rate/decim2/1e6);
            if (decim_rem == 0) {
                printf("EXACT MATCH\n");
                return;
            }
		}
	}
	printf("\nLEAST decim_rem %.6f %.3f: decim1 %d (%.4f kHz) decim2 %d (%.4f MHz)\n\n",
		min_decim_rem, (double)in_rate/min_decim1/min_decim2, min_decim1, in_rate/min_decim1/1e3, min_decim2, in_rate/min_decim2/1e6);
}

#define WSPR_RATE 375

int main()
{
	#define DECIM_REM 0.0001
	//int net_rate = 12000, out_rate = 375;
	//int net_rate = 9600, out_rate = 375;
	int net_rate = 12000, out_rate = 44100;
	//int net_rate = 9600, out_rate = 44100;
	//int net_rate = 8250, out_rate = 44100;
	//int net_rate = 8250, out_rate = 16000;

#if 1
	printf("integer interp/decim for exact AUDIO out_rate\n");
	interp_decim(net_rate, out_rate, DECIM_REM);
#endif

#if 1
	printf("\nDDC decim1/decim2 for exact AUDIO net_rate\n");
	//#define ADC_CLOCK 66666600
	#define ADC_CLOCK 66665900
	#define AUDIO_RATE 12000
	decim_2stage(ADC_CLOCK, AUDIO_RATE, 0.01);
#endif

#if 0
	printf("\nDDC decim1/decim2 for exact AUDIO net_rate (WSPR)\n");
	#define ADC_CLOCK 66666600
	#define AUDIO_RATE 10500
	//#define ADC_CLOCK 65472000
	//#define AUDIO_RATE 8250
	//for (net_rate = 8250; net_rate <= 15000 ; net_rate += WSPR_RATE) {
	for (net_rate = 9750; net_rate <= 11250 ; net_rate += WSPR_RATE) {
		decim_2stage(ADC_CLOCK, net_rate, 0.01);
	}
#endif

#if 0
	printf("\ninteger interp/decim for WSPR rates\n");
	for (net_rate = 8250; net_rate <= 15000 ; net_rate += WSPR_RATE) {
		printf("WSPR %d = %d * %.1f\n", net_rate, WSPR_RATE, (float) net_rate / WSPR_RATE);
		interp_decim(net_rate, out_rate, DECIM_REM);
		printf("\n");
	}

	int interp;
	double decim;
	#define RATE_REM 1000
	printf("\ninteger interp/decim for fast/slow out_rates\n");
	printf("RATE_REM %d\n", RATE_REM);
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
#endif

	return 0;
}
