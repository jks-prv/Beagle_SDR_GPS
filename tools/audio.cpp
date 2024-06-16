#include <stdio.h>
#include <stdlib.h>
#include <math.h>

//#define ADC_CLOCK 66000000

#define ADC_CLOCK 66666584      // wessex.zapto.org:8074 (kiwi-2)

//#define ADC_CLOCK 66672000    // 66.672
//#define ADC_CLOCK 66660000    // 66.66
//#define ADC_CLOCK 66666600    // 66.6666
//#define ADC_CLOCK 66665900    // 66.6659

// action
#define ACTION_FIND_WIDEBAND
//#define ACTION_FIND_DECIM12_EXACT
//#define ACTION_FIND_DECIM_MOD_WSPR

#ifdef ACTION_FIND_WIDEBAND
    #define AUDIO_RATE_WIDE (24 * 12000)
    #define AUDIO_RATE_STD  12000
    #define FIR_DECIM_2 0
#endif

#ifdef ACTION_FIND_DECIM12_EXACT
    //#define USE_RX_CICF
    //#define WB (6 * 12000)
    #define WB (12 * 12000)

    #ifdef USE_RX_CICF
        #define FIR_DECIM_2 1
        #define AUDIO_RATE_WIDE (WB * 2)
        //#define AUDIO_RATE_WIDE (26625 * 2)
        //#define AUDIO_RATE_WIDE (20250 * 2)
        #define AUDIO_RATE_STD (12000 * 2)
    #else
        #define FIR_DECIM_2 0
        #define AUDIO_RATE_WIDE (WB * 1)
        //#define AUDIO_RATE_WIDE 26625
        //#define AUDIO_RATE_WIDE 20250
        #define AUDIO_RATE_STD 12000
    #endif
#endif
    
#ifdef ACTION_FIND_DECIM_MOD_WSPR
    //#define USE_RX_CICF

    #ifdef USE_RX_CICF
        #define FIR_DECIM_2 1
        #define AUDIO_RATE_MAX (24000 * 2)
        #define AUDIO_RATE_MIN (20250 * 2)
    #else
        #define FIR_DECIM_2 0
        #define AUDIO_RATE_MAX 24000
        #define AUDIO_RATE_MIN 20250
    #endif
#endif

#define MAX_INTERP  4096
#define OUT_RATE    44100
#define WSPR_RATE   375
#define DECIM_REM   0.005

void interp_decim(int net_rate, int out_rate, double target_rem)
{
	int interp;
	double decim, decim_rem;

	printf("net_rate %d, out_rate %d, target_rem %f\n", net_rate, out_rate, target_rem);
	for (interp = 1; interp <= MAX_INTERP; interp++) {
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
	double irate = (double)in_rate/1e6, decim, decim_rem;

	printf("in_rate %.6f, decim %d:%d, out_rate %d:%d, target_rem %f\n", irate, decim1, decim2, out_rate1, out_rate2, target_rem);
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

void decim_2stage(int in_rate, int out_rate, double target_rem, int target_ppm = 0)
{
	int decim1, decim2_i;
	double irate = (double)in_rate/1e6, decim2, decim_rem, decim2_round, decim_rate, err;

	printf("in_rate %.6f, out_rate %d%s, decim %.3f\n", irate, out_rate,
	    FIR_DECIM_2? "(doubled)" : "",
	    (double) in_rate / out_rate);
	min_decim_rem = 9999;
	for (decim1 = 1; decim1 <= MAX_INTERP; decim1++) {
		decim2 =  ((double) in_rate / decim1) / out_rate;
		decim2_round = round(decim2);
		decim2_i = (int) decim2_round;
		decim_rem = fabs(decim2 - decim2_round);
		if (decim_rem < min_decim_rem) {
			min_decim_rem = decim_rem;
			min_decim1 = decim1;
			min_decim2 = decim2_i;
		}
		if (target_rem != 0) {
            if (decim_rem < target_rem) {
                decim_rate = (double) in_rate/decim1/decim2_round;
                err = decim_rate - out_rate;
                int ppm = (int) round(abs(err) / decim_rate*1e6);
                printf("OKAY  decim_rem %.6f %.3f (%+11.3f %6d ppm): %4d = decim1 %4d (%10.4f kHz) decim2 %4d (%10.4f MHz) ",
                    decim_rem, decim_rate, err, ppm,
                    decim1 * decim2_i, decim1, in_rate/decim1/1e3, decim2_i, in_rate/decim2_i/1e6);
                if (decim_rem == 0) {
                    printf("EXACT MATCH\n");
                    //return;
                } else printf("\n");
            }
        } else {
            decim_rate = (double) in_rate/decim1/decim2_round;
            err = decim_rate - out_rate;
            int ppm = (int) round(abs(err) / decim_rate*1e6);
            if (ppm < target_ppm) {
                printf("OKAY  decim_rem %.6f %.3f (%+11.3f %6d ppm): %4d = decim1 %4d (%10.4f kHz) decim2 %4d (%10.4f MHz) ",
                    decim_rem, decim_rate, err, ppm,
                    decim1 * decim2_i, decim1, in_rate/decim1/1e3, decim2_i, in_rate/decim2_i/1e6);
                if (decim_rem == 0) {
                    printf("EXACT MATCH\n");
                    //return;
                } else printf("\n");
            }
        }
	}
	decim_rate = (double) in_rate/min_decim1/min_decim2;
    err = decim_rate - out_rate;
	printf("LEAST decim_rem %.6f %.3f (%+8.3f %4d ppm): %d = decim1 %d (%.4f kHz) decim2 %d (%.4f MHz)\n",
		min_decim_rem, decim_rate, err, (int) round(abs(err)/decim_rate*1e6),
		min_decim1 * min_decim2, min_decim1, in_rate/min_decim1/1e3, min_decim2, in_rate/min_decim2/1e6);
}

int main()
{
	int net_rate, out_rate = OUT_RATE;

#if 0
	printf("integer interp/decim for exact AUDIO out_rate\n");
	interp_decim(AUDIO_RATE_NOM, out_rate, DECIM_REM);
#endif

#ifdef ACTION_FIND_WIDEBAND
    printf("\nDDC decim1/decim2 for ACTION_FIND_WIDEBAND\n");
    decim_2stage(ADC_CLOCK, AUDIO_RATE_STD, 0, 1000);
#endif

#ifdef ACTION_FIND_DECIM12_EXACT
    printf("\nDDC decim1/decim2 for exact AUDIO net_rate %s\n",
        FIR_DECIM_2? "(FIR decim2 mode)" : "");
    decim_2stage(ADC_CLOCK, AUDIO_RATE_WIDE, DECIM_REM);
    printf("WSPR integer decim %.2f\n\n", (float) AUDIO_RATE_WIDE / WSPR_RATE);

    printf("\nDDC decim1/decim2 for exact AUDIO net_rate\n");
    decim_2stage(ADC_CLOCK, AUDIO_RATE_STD, DECIM_REM);
    printf("WSPR integer decim %.2f\n", (float) AUDIO_RATE_STD / WSPR_RATE);
#endif

#ifdef ACTION_FIND_DECIM_MOD_WSPR
	printf("\nDDC decim1/decim2 for range of AUDIO net_rate compatible with WSPR\n");
	for (net_rate = AUDIO_RATE_MIN/WSPR_RATE*WSPR_RATE; net_rate <= AUDIO_RATE_MAX; net_rate += WSPR_RATE) {
		decim_2stage(ADC_CLOCK, net_rate, DECIM_REM);
		printf("WSPR %d = %d * %.1f\n", net_rate, WSPR_RATE, (float) net_rate / WSPR_RATE);
		printf("\n");
	}
#endif

#if 0
	printf("\ninteger interp/decim for WSPR rates\n");
	for (net_rate = AUDIO_RATE_MIN; net_rate <= AUDIO_RATE_MAX; net_rate += WSPR_RATE) {
		printf("WSPR %d = %d * %.1f\n", net_rate, WSPR_RATE, (float) net_rate / WSPR_RATE);
		interp_decim(net_rate, out_rate, DECIM_REM);
		printf("\n");
	}

	int interp;
	double decim;
	#define RATE_REM 1000
	printf("\ninteger interp/decim for fast/slow out_rates\n");
	printf("RATE_REM %d\n", RATE_REM);
	for (interp = 2; interp <= MAX_INTERP; interp++) {
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
