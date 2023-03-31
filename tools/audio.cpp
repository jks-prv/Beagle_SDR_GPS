#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define WSPR_RATE 375

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

void decim_search(int in_rate, int decim1, int decim2, int out_rate1, int out_rate2, double target_rem)
{
	int out_rate, i_decim;
	double decim, decim_rem;

	printf("in_rate %d, decim %d:%d, out_rate %d:%d, target_rem %f\n", in_rate, decim1, decim2, out_rate1, out_rate2, target_rem);
    for (out_rate = out_rate1; out_rate <= out_rate2; out_rate++) {
        for (i_decim = decim1; i_decim <= decim2; i_decim++) {
            decim = (double) in_rate / i_decim;
            decim_rem = fabs(decim - out_rate);
            if (decim_rem <= target_rem) {
                printf("decim_rem %.6f: out_rate %d i_decim %d decim %.6f WSPR %.2f ", decim_rem, out_rate, i_decim, decim, (float) out_rate / WSPR_RATE);
                if (decim_rem == 0)
                    printf("EXACT MATCH");
                printf("\n");
            }
        }
    }
}

double min_decim_rem;
int min_decim1, min_decim2;

void decim_2stage(int in_rate, int out_rate, double target_rem)
{
	int decim1, decim2_i;
	double decim2, decim_rem, decim2_round, decim_rate;

	printf("in_rate %d, out_rate %d, decim %.3f\n", in_rate, out_rate, (double) in_rate / out_rate);
	min_decim_rem = 9999;
	for (decim1 = 1; decim1 <= 1024; decim1++) {
		decim2 =  ((double) in_rate / decim1) / out_rate;
		decim2_round = round(decim2);
		decim2_i = (int) decim2_round;
		decim_rem = fabs(decim2 - decim2_round);
		if (decim_rem < min_decim_rem) {
			min_decim_rem = decim_rem;
			min_decim1 = decim1;
			min_decim2 = decim2_i;
		}
		if (decim_rem < target_rem) {
	        decim_rate = (double) in_rate/decim1/decim2_round;
			printf("OKAY  decim_rem %.6f %.3f (%+.3f): %d = decim1 %d (%.4f kHz) decim2 %d (%.4f MHz) ",
				decim_rem, decim_rate, decim_rate - out_rate,
				decim1 * decim2_i, decim1, in_rate/decim1/1e3, decim2_i, in_rate/decim2/1e6);
            if (decim_rem == 0) {
                printf("EXACT MATCH\n");
                //return;
            } else printf("\n");
		}
	}
	decim_rate = (double) in_rate/min_decim1/min_decim2;
	printf("\nLEAST decim_rem %.6f %.3f (%+.3f): %d = decim1 %d (%.4f kHz) decim2 %d (%.4f MHz)\n",
		min_decim_rem, decim_rate, decim_rate - out_rate,
		min_decim1 * min_decim2, min_decim1, in_rate/min_decim1/1e3, min_decim2, in_rate/min_decim2/1e6);
}

int main()
{
    #define ADC_CLOCK 66666600
    //#define ADC_CLOCK 66665900

	#define DECIM_REM 0.0001
	int net_rate = 22250, out_rate = 44100;
	//int net_rate = 12000, out_rate = 375;
	//int net_rate = 9600, out_rate = 375;
	//int net_rate = 12000, out_rate = 44100;
	//int net_rate = 9600, out_rate = 44100;
	//int net_rate = 8250, out_rate = 44100;
	//int net_rate = 8250, out_rate = 16000;

#if 0
	printf("integer interp/decim for exact AUDIO out_rate\n");
	interp_decim(net_rate, out_rate, DECIM_REM);
#endif

#if 1
    #define USE_RX_CICF
    #ifdef USE_RX_CICF
        #define AUDIO_RATE_WIDE 40500
        #define AUDIO_RATE_STD 24000
    #else
        #define AUDIO_RATE_WIDE 20250
        #define AUDIO_RATE_STD 12000
    #endif
    
    printf("\nDDC decim1/decim2 for exact AUDIO net_rate\n");
    decim_2stage(ADC_CLOCK, AUDIO_RATE_WIDE, 0.005);
    printf("WSPR integer decim %.2f\n", (float) AUDIO_RATE_WIDE / WSPR_RATE);

    printf("\nDDC decim1/decim2 for exact AUDIO net_rate\n");
    decim_2stage(ADC_CLOCK, AUDIO_RATE_STD, 0.005);
    printf("WSPR integer decim %.2f\n", (float) AUDIO_RATE_STD / WSPR_RATE);
#endif

#if 0
	printf("\nDDC decim1/decim2 for exact AUDIO net_rate (WSPR)\n");
	int audio_rate;
	for (audio_rate = 19000/WSPR_RATE*WSPR_RATE; audio_rate <= 21000 ; audio_rate += WSPR_RATE) {
		decim_2stage(ADC_CLOCK, audio_rate, 0.01);
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
