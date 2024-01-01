// Licensed under a Creative Commons Attribution 3.0 Unported License

// Original work Copyright (c) 2012 Rick Lyons (http://www.dsprelated.com/code.php?submittedby=14446)
// Original work: http://www.dsprelated.com/showcode/269.php

// Derivative work Copyright (c) 2014-2022 John Seamons, ZL4VO/KF6VO

/*
 * A C version of Rick Lyon's Matlab implementation of Hogenauer's CIC filter register pruning algorithm.
 * 
 * A Verilog file is generated containing unrolled calls to the integrator and comb sections at the pruned bit widths.
 * Assumes you have a Verilog wrapper that looks like this:
 *
 *		wire signed [IN_WIDTH-1:0] in = ...;
 *		wire signed [OUT_WIDTH-1:0] out;
 *		`include "cic_gen.vh"
 *
 * And the modules "cic_integrator" and "cic_comb" that implement the actual CIC filter internals.
 * Variables "in" and "out" connect to the generated code.
 * Very important that variable "in" be declared signed to allow proper Verilog sign-extension.
 *
 * See Rick's original article at: http://www.dsprelated.com/showcode/269.php
 * Rick's Matlab code is at the end of this file.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#define ONE_BASED_INDEXING 1
#define	ARRAY_LEN(x)	((int) (sizeof (x) / sizeof ((x) [0])))

int debug = 0;

#define assert(e) \
	if (!(e)) { \
		printf("assertion failed: \"%s\" %s line %d\n", #e, __FILE__, __LINE__); \
		exit(-1); \
	}

void sys_panic(char *str)
{
	printf("panic\n");
	perror(str);
	exit(-1);
}

// awful, but it works..
double factd(int n)
{
	int i;
	double d=1;
	for (i = 2; i <= n; i++) {
		d *= i;
		if (isinf(d)) break;
	}
	return d;
}

double nchoosek2(int n, int k) // this version of nchoosek avoids numerical overflow
{
	int i = 0;
	double result = 1;
	for (i = 1; i <= n-k; ++i) {
	result *= (double)(k+i) / (double)(i);
	}
	return result;
}

double nchoosek(int n, int k)
{
	double fn = factd(n);
	double fnk = factd(n-k);
	double fk = factd(k);
	double nck = fn / (fnk * fk);
	return nck;
	// return nchoosek2(n,k);
}

#define MSB(w)  ((w)-1)

// options
#define	EMPTY       0x01    // generate an empty file for Verilog include file compatibility
#define INTEG_COMB  0x02    // prune both integrator and comb sections (usual case)
#define	INTEG_ONLY  0x04    // generate only pruned integrator section (for use with a sequential comb implementation)
#define MODE_REAL   0x08
#define MODE_IQ     0x10    // generate separate I & Q logic
#define	NO_PRUNE    0x20    // generate Verilog that has no pruning for comparison/testing purposes
#define NO_ROUND    0x40    // no rounding of result
#define NO_BUGFIX   0x80    // don't include bug fixes for comparison/testing purposes

#define RIQ_R 0
#define RIQ_I 1
#define RIQ_Q 2
const char *riq_s[] = { "", "_i", "_q" };

// N	number of filter stages (typ 2-7)
// R	decimation factor
// M	differential delay
// Bin	input bit width
// Bout	output bit width

void cic_gen(char *fn, int options, int N, int R, int Bin, int Bout)
{

	// 1..N-1,N integrators
	// N+1..2*N combs
	// 2*N+1	output
	
	#define M 1		// fixed differential delay

	FILE *fp = NULL;
	if (strstr(fn, ".vh"))
		if ((fp = fopen(fn, "w")) == NULL) sys_panic("fopen");
	if (options & EMPTY) { if (fp) fclose(fp); return; }
	char *mode_s;
	asprintf(&mode_s, "%s%s%s%s",
	    (options & INTEG_COMB)? "INTEG_COMB" : "INTEG_ONLY",
	    (options & MODE_REAL)? "|MODE_REAL" : "|MODE_IQ",
	    (options & NO_PRUNE)? "|NO_PRUNE" : "",
	    (options & NO_ROUND)? "|NO_ROUND" : "");
	
	if (fp) fprintf(fp, "// generated file\n\n");
	if (fp) fprintf(fp, "// CIC: %s N=%d R=%d M=%d Bin=%d Bout=%d\n", mode_s, N, R, M, Bin, Bout);

	printf("\n\nfile: \"%s\" %s N=%d R=%d M=%d Bin=%d Bout=%d\n", fn, mode_s, N, R, M, Bin, Bout);
	free(mode_s);

	#define	N2		    (2*N)		// N-integrators + N-combs
	#define	N2P1		(2*N+1)		// N-integrators + N-combs + output register
	#define	N2P1_MAX	(2*MAX_N+1 + ONE_BASED_INDEXING)
	
	int s, i, k, L;
	
	#define MAX_N		7			// max number of stages 
	#define MAX_R		(64*1024)	// max decimation
	assert(N >= 2 && N <= MAX_N);
	assert(R <= MAX_R);
	
	int j;	// stage number: 1..2N+1 (N integrators, N combs, 1 output)
	
	#define	hj_LEN		( (MAX_R*M-1)*MAX_N + (MAX_N-1) )		// R*M-1*N + N-1
	double hj[hj_LEN+1];	// "impulse response coefficients"

	// both are row vectors in Matlab
	double Fj[N2P1_MAX];	// "variance error gain"
	double Bj[N2P1_MAX];	// number of bits to truncate (from maximum) at each stage

	// integrators (except last)
	double Change_to_Result;
	for (j = N-1; j >= 1; j--) {
		i = (R*M-1)*N + j-1+1;
		assert(i <= hj_LEN);
		hj[i] = 0;
	
		// Matlab array indexing is 1-based, so keep k+1 notation
		for (k = 0; k <= (R*M-1)*N +j-1; k++) {
			hj[k+1] = 0;
			double f_k = k;
			for (L = 0; L <= floor(f_k/(R*M)); L++) {
				Change_to_Result = pow(-1,L) * nchoosek(N, L) * nchoosek(N-j+k-R*M*L, k-R*M*L);		// EBH-9b
				assert((k+1) <= hj_LEN);
				hj[k+1] += Change_to_Result;
			}
		}
		
		double sum=0;
		for (k = 0; k <= (R*M-1)*N +j-1; k++) {
			sum += hj[k+1] * hj[k+1];		// EBH-16b
		}
		
		Fj[j] = sqrt(sum);
	}
	
	// up-to MAX_N cascaded combs
	int hj_comb[MAX_N] = {2, 6, 20, 70, 252, 924, 3432};		// pre-computed, see Rick's paper
	assert(MAX_N <= ARRAY_LEN(hj_comb));

	double Fj_for_many_combs[MAX_N];
	for (s = 1; s <= MAX_N; s++) {
		Fj_for_many_combs[s] = sqrt(hj_comb[s-ONE_BASED_INDEXING]);
	}
	
	// last integrator
	assert(N >= 2 && N <= MAX_N);
	Fj[N] = Fj_for_many_combs[N-1] * sqrt(R*M);

	// N cascaded combs
	for (j = N2; j >= N+1; j--) {
		Fj[j] = Fj_for_many_combs[N2 -j + 1];
	}
	
	// output register
	Fj[N2P1] = 1;		// EBH-16b
	
	// in Matlab this is created as a column vector due to ctranspose operator (')
	//		Minus_log2_of_F_sub_j = -log2(F_sub_j)';
	// but this makes no difference as there is no subsequent matrix arithmetic
	double Minus_log2_of_Fj[N2P1_MAX];
	for (j = 1; j <= N2P1; j++) {
		Minus_log2_of_Fj[j] = -log2(Fj[j]);
	}
	
	double CIC_Filter_Gain = pow((R*M), N);
	double Num_of_Bits_Growth = ceil(log2(CIC_Filter_Gain));
    double Num_Output_Bits_With_No_Truncation = Num_of_Bits_Growth + Bin;
	double Num_of_Output_Bits_Truncated = Num_Output_Bits_With_No_Truncation - Bout;
	
	// 4/2023
	// For some configurations it's possible desired Bout is larger than Num_Output_Bits_With_No_Truncation (acc_max).
	int acc_max_set_to_Bout = 0;
	if (Num_of_Output_Bits_Truncated < 0) {
	    Num_Output_Bits_With_No_Truncation = Bout;
	    Num_of_Output_Bits_Truncated = 0;
	    acc_max_set_to_Bout = 1;
	}

	double Output_Truncation_Noise_Variance = pow((pow(2, Num_of_Output_Bits_Truncated)), 2) /12;
	double Output_Truncation_Noise_Standard_Deviation = sqrt(Output_Truncation_Noise_Variance);
	double Log_base2_of_Output_Truncation_Noise_Standard_Deviation = log2(Output_Truncation_Noise_Standard_Deviation);

	double Half_Log_Base2_of_6_over_N = 0.5 * log2(6.0/N);

	double noise_factor = Log_base2_of_Output_Truncation_Noise_Standard_Deviation + Half_Log_Base2_of_6_over_N;

	if (debug) {
        printf("\nCIC_Filter_Gain %e %f\n", CIC_Filter_Gain, CIC_Filter_Gain);
        printf("Num_of_Bits_Growth %f\n", Num_of_Bits_Growth);
        printf("Num_Output_Bits_With_No_Truncation %f\n", Num_Output_Bits_With_No_Truncation);
        printf("Num_of_Output_Bits_Truncated %f\n", Num_of_Output_Bits_Truncated);
        printf("Output_Truncation_Noise_Variance %e %f\n", Output_Truncation_Noise_Variance, Output_Truncation_Noise_Variance);
        printf("Output_Truncation_Noise_Standard_Deviation %e %f\n", Output_Truncation_Noise_Standard_Deviation, Output_Truncation_Noise_Standard_Deviation);
        printf("Log_base2_of_Output_Truncation_Noise_Standard_Deviation %f\n", Log_base2_of_Output_Truncation_Noise_Standard_Deviation);
        printf("Half_Log_Base2_of_6_over_N %f\n", Half_Log_Base2_of_6_over_N);
        printf("noise_factor %f\n", noise_factor);
	}
	
	if (debug) printf("\n stage  ML2Fj  noise     Bj\n");
	//                   123456 123456 123456 123456
	double trunc_extra[N2P1_MAX];
	for (j = 1; j <= N2; j++) {
	
		// FIXME: We seem to need an extra bit in the comb accumulators for N >= 4096. Why is this?
		trunc_extra[j] = (!(options & NO_BUGFIX) && j > N && R >= 4096)? 1:0;
		Bj[j] = Minus_log2_of_Fj[j] + noise_factor;
		if (debug) {
		    printf("%s%d ", (j<=N)? "integ": ( (j<=N2)? " comb" : "  out" ), (j<=N)? j: ( (j<=N2)? j-N:0 ));
		    printf("%6.1f %6.1f %6.1f\n", Minus_log2_of_Fj[j], noise_factor, Bj[j]);
		}
	}
	trunc_extra[N2P1] = 0;
	if (debug) printf("\n");
	
	const char *msg = acc_max_set_to_Bout? "(Note: acc_max increased to Bout)" : "";
	printf("growth %.0f = ceil(N=%d * log2(R=%d)=%.0f)\n", Num_of_Bits_Growth, N, R, log2(R));
	if (fp) fprintf(fp, "// growth %.0f = ceil(N=%d * log2(R=%d)=%.0f)\n", Num_of_Bits_Growth, N, R, log2(R));
	printf("Bin %d + growth %.0f = acc_max %.0f %s\n", Bin, Num_of_Bits_Growth, Num_Output_Bits_With_No_Truncation, msg);
	if (fp) fprintf(fp, "// Bin %d + growth %.0f = acc_max %.0f %s\n\n", Bin, Num_of_Bits_Growth, Num_Output_Bits_With_No_Truncation, msg);
	
	printf("noise_factor %f = %f + %f\n\n", noise_factor, Log_base2_of_Output_Truncation_Noise_Standard_Deviation, Half_Log_Base2_of_6_over_N);

	// verify that: for N=pow2, M=1: N * log2(R) == log2(pow(R*M, N))
	if (debug && !fp) {
		for (i = 2; i <= R; i <<= 1) {
			double growth1 = N * log2(i);
			double growth2 = ceil(log2(pow(i*M, N)));
			printf("R %4d | G %4.1f %4.1f | ACC %4.1f\n", i, growth1, growth2, growth2 + Bin);
		}
	}
	
	// results
	double origBj[N2P1_MAX];
	int pACC[N2P1_MAX];		// accumulator length to print

	pACC[0] = Num_Output_Bits_With_No_Truncation;
	for (s = 1; s <= N2; s++) {
		origBj[s] = Bj[s];
		Bj[s] = (isnan(Bj[s]) || isinf(Bj[s]) || (Bj[s]<0))? 0 : Bj[s];
		Bj[s] = floor(Bj[s]) - trunc_extra[s];
		pACC[s] = (options & NO_PRUNE)? Num_Output_Bits_With_No_Truncation : (Num_Output_Bits_With_No_Truncation - Bj[s]);
	}
	origBj[s] = Bj[N2P1] = Num_of_Output_Bits_Truncated;
	pACC[N2P1] = Bout;
	
    // fix input
    int clamped = 0;
	if (!(options & NO_PRUNE)) {
        if (pACC[1] < Bin) {
            // Increase size of first stage to match input width if truncated width is smaller than input.
            // Extra truncation will be added to stage 2 truncation.
            // See: www.dsprelated.com/showcode/269.php  UPDATE Jan. 23, 2022
            pACC[0] = Bin;
            pACC[1] = Bin;
            clamped = 1;
        } else {
            pACC[0] = pACC[1];
        }
    }
	
	printf(" stage     Fj f(Fj)    nf    Bj i(Bj) extra   acc trunc\n");
	//      123456 123456 12345 12345 12345 12345 12345 12345 12345
	int end = (options & INTEG_COMB)? N2P1 : N;
	int truncFromLastStage[N2P1_MAX];

	for (s = 1; s <= end; s++) {
		truncFromLastStage[s] = pACC[s-1] - pACC[s];
		printf("%s%d ", (s<=N)? "integ": ( (s<=N2)? " comb" : "  out" ), (s<=N)? s: ( (s<=N2)? s-N:0 ));
		if (isnan(Fj[s]) || isinf(Fj[s]) || (Fj[s] < 10000))
			printf("%6.1f ", Fj[s]);
		else
			printf(" large ");
		printf("%5.1f %5.1f %5.1f %5.0f %5.0f %5d %5d  %s\n",
			Minus_log2_of_Fj[s],
			noise_factor, origBj[s], Bj[s],
			trunc_extra[s], pACC[s], truncFromLastStage[s],
			(s == 1 && clamped)? "width clamped to input width" : "");
	}
	
	if (!fp)
		return;
	
	int NBO = Num_Output_Bits_With_No_Truncation-1;
    int riq_b = (options & MODE_REAL)? RIQ_R : RIQ_I;
    int riq_e = (options & MODE_REAL)? RIQ_R : RIQ_Q;

	if (options & INTEG_ONLY) {
		for (s = 0; s <= N; s++) {
            for (i = riq_b; i <= riq_e; i++) {
			    fprintf(fp, "wire signed [%d:0] integrator%d_data%s;\n", MSB(pACC[s]), s, riq_s[i]);
		    }
		}
	
		fprintf(fp, "\n// important that \"in\" be declared signed by wrapper code\n");
		fprintf(fp, "// so this assignment will sign-extend:\n");
        for (i = riq_b; i <= riq_e; i++) {
		    fprintf(fp, "assign integrator0_data%s = in%s;\n", riq_s[i], riq_s[i]);
        }
		fprintf(fp, "\n");
		
		for (s = 1; s <= N; s++) {
		    for (i = riq_b; i <= riq_e; i++) {
		        const char *riq = riq_s[i];
                fprintf(fp, 
                    "cic_integrator #(.WIDTH(%d)) cic_integrator%d_inst%s(\n"
                    "	.clock(clock),\n"
                    "	.reset(reset),\n"
                    "	.strobe(in_strobe),\n"
                    "	.in_data(integrator%d_data%s[%d -:%d]),	// trunc %d bits %s\n"
                    "	.out_data(integrator%d_data%s)\n"
                    ");\n\n",
                    pACC[s], s, riq, s-1, riq, MSB(pACC[s-1]), pACC[s], pACC[s-1]-pACC[s],
                    (s == 1)? "(should always be zero)" : "", s, riq);
            }
		}
	
        for (i = riq_b; i <= riq_e; i++) {
            if (NBO+1 != pACC[N]) {
                fprintf(fp, "assign integ_out%s = { integrator%d_data%s, {%d-%d{1'b0}} };\n",
                    riq_s[i], N, riq_s[i], NBO+1, pACC[N]);
            } else {
                fprintf(fp, "assign integ_out%s = integrator%d_data%s;\n",
                    riq_s[i], N, riq_s[i]);
            }
		}
	} else
	
	if (options & INTEG_COMB) {
		for (s = 0; s <= N; s++) {
            for (i = riq_b; i <= riq_e; i++) {
			    fprintf(fp, "wire signed [%d:0] integrator%d_data%s;\n", MSB(pACC[s]), s, riq_s[i]);
			}
		}
		
		for (s = N; s <= N2; s++) {
            for (i = riq_b; i <= riq_e; i++) {
			    fprintf(fp, "wire signed [%d:0] comb%d_data%s;\n", MSB(pACC[s]), s-N, riq_s[i]);
			}
		}
		
		fprintf(fp, "\n// important that \"in\" be declared signed by wrapper code\n");
		fprintf(fp, "// so this assignment will sign-extend:\n");
        for (i = riq_b; i <= riq_e; i++) {
		    fprintf(fp, "assign integrator0_data%s = in%s;\n", riq_s[i], riq_s[i]);
        }
		fprintf(fp, "\n");
		
		for (s = 1; s <= N; s++) {
            for (i = riq_b; i <= riq_e; i++) {
		        const char *riq = riq_s[i];
                fprintf(fp, 
                    "cic_integrator #(.WIDTH(%d)) cic_integrator%d_inst%s(\n"
                    "	.clock(clock),\n"
                    "	.reset(reset),\n"
                    "	.strobe(in_strobe),\n"
                    "	.in_data(integrator%d_data%s[%d -:%d]),	// trunc %d bits %s\n"
                    "	.out_data(integrator%d_data%s)\n"
                    ");\n\n",
                    pACC[s], s, riq, s-1, riq, MSB(pACC[s-1]), pACC[s], pACC[s-1]-pACC[s],
                    (s == 1)? "(should always be zero)" : "", s, riq);
            }
		}
	
        for (i = riq_b; i <= riq_e; i++) {
		    fprintf(fp, "assign comb0_data%s = integrator%d_data%s;\n\n", riq_s[i], N, riq_s[i]);
		}
		
		for (s = N+1; s <= N2; s++) {
            for (i = riq_b; i <= riq_e; i++) {
		        const char *riq = riq_s[i];
                fprintf(fp, 
                    "cic_comb #(.WIDTH(%d)) cic_comb%d_inst%s(\n"
                    "	.clock(clock),\n"
                    "	.reset(reset),\n"
                    "	.strobe(out_strobe),\n"
                    "	.in_data(comb%d_data%s[%d -:%d]),	// trunc %d bits %s\n"
                    "	.out_data(comb%d_data%s)\n"
                    ");\n\n",
                    pACC[s], s-N, riq, s-N-1, riq, MSB(pACC[s-1]), pACC[s], pACC[s-1]-pACC[s],
                    (s == 1)? "(should always be zero)" : "", s-N, riq);
            }
		}
	
        for (i = riq_b; i <= riq_e; i++) {
		    fprintf(fp, "assign out = comb%d_data[%d -:%d]",
			    N, MSB(pACC[N2]), pACC[N2P1]);
		}

		// add rounding bit if present
		if (!(options & NO_ROUND) && pACC[N2] > Bout) {
		    fprintf(fp, " + comb%d_data[%d]", N, MSB(pACC[N2]) - Bout);
		}
		fprintf(fp, ";	// trunc %d bits, %s\n", pACC[N2] - Bout,
		    (options & NO_ROUND)? "no rounding" : "rounding applied");
	}

	fclose(fp);
}

int main (int argc, char *argv[])
{
    if (argc >= 2) debug = 1;
    
    // usage:
	// cic_gen(generated_filename, options, #stages, decimation, bits_in, bits_out);

#ifndef KIWISDR
        cic_gen("example1.vh", INTEG_COMB|MODE_REAL, 3, 12, 23, 12);
        cic_gen("example2.vh", INTEG_COMB|MODE_IQ, 3, 12, 23, 12);
        cic_gen("example3.vh", INTEG_ONLY|MODE_REAL, 3, 12, 23, 12);
        cic_gen("example4.vh", INTEG_ONLY|MODE_IQ, 3, 12, 23, 12);
        cic_gen("example5.vh", INTEG_COMB|MODE_IQ|NO_PRUNE, 3, 12, 23, 12);

	    cic_gen("Hogenauer_paper.vh", INTEG_COMB|MODE_REAL|NO_ROUND, 4, 25, 16, 16);
#else
    // example of use in the KiwiSDR project: github.com/jks-prv/Beagle_SDR_GPS
    #include "kiwi.gen.h"

        cic_gen("cic_rx1_12k.vh", INTEG_COMB|MODE_REAL, RX1_STAGES, RX1_STD_DECIM,  RX1_BITS, RX2_BITS);
        cic_gen("cic_rx1_20k.vh", INTEG_COMB|MODE_REAL, RX1_STAGES, RX1_WIDE_DECIM, RX1_BITS, RX2_BITS);

    #ifdef USE_RX_SEQ
        // For USE_RX_SEQ the cic_gen() mode is INTEG_ONLY because the comb part of the
        // rx second stage is done by the rx sequential state machine.
        cic_gen("cic_rx2_12k.vh", EMPTY, 0, 0, 0, 0);
        cic_gen("cic_rx2_20k.vh", EMPTY, 0, 0, 0, 0);
        cic_gen("cic_rx3_12k.vh", INTEG_ONLY|MODE_IQ, RX2_STAGES, RX2_STD_DECIM,  RX2_BITS, RXO_BITS);
        cic_gen("cic_rx3_20k.vh", INTEG_ONLY|MODE_IQ, RX2_STAGES, RX2_WIDE_DECIM, RX2_BITS, RXO_BITS);
    #else
        cic_gen("cic_rx2_12k.vh", INTEG_COMB|MODE_REAL, RX2_STAGES, RX2_STD_DECIM,  RX2_BITS, RXO_BITS);
        cic_gen("cic_rx2_20k.vh", INTEG_COMB|MODE_REAL, RX2_STAGES, RX2_WIDE_DECIM, RX2_BITS, RXO_BITS);
        cic_gen("cic_rx3_12k.vh", EMPTY, 0, 0, 0, 0);
        cic_gen("cic_rx3_20k.vh", EMPTY, 0, 0, 0, 0);
    #endif

    #ifdef USE_WF_1CIC
        cic_gen("cic_wf1.vh", INTEG_COMB|MODE_REAL, WF1_STAGES, WF_1CIC_MAXD, WF1_BITS, WFO_BITS);
        cic_gen("cic_wf2.vh", EMPTY, 0, 0, 0, 0);
    #else
        cic_gen("cic_wf1.vh", INTEG_COMB|MODE_REAL, WF1_STAGES, WF_2CIC_MAXD, WF1_BITS, WF2_BITS);
        cic_gen("cic_wf2.vh", INTEG_COMB|MODE_REAL, WF2_STAGES, WF_2CIC_MAXD, WF2_BITS, WFO_BITS);
    #endif
#endif

	return(0);
}


#if 0

%  Computes CIC decimation filter accumulator register  
%  truncation in each filter stage based on Hogenauer's  
%  'accumulator register pruning' technique. 
%
%   Inputs:
%     N = number of decimation CIC filter stages (filter order).
%     R = CIC filter rate change factor (decimation factor).
%     M = differential delay.
%     Bin = number of bits in an input data word.
%     Bout = number of bits in the filter's final output data word.
%   Outputs:
%     Stage number (ranges from 1 -to- 2*N+1).
%     Bj = number of least significant bits that can be truncated
%       at the input of a filter stage.
%     Accumulator widths = number of a stage's necessary accumulator 
%       bits accounting for truncation. 

%  Richard Lyons Feb., 2012

clear, clc

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%  Define CIC filter parameters
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%N = 4; R = 25; M = 1; Bin = 16; Bout = 16; % Hogenauer paper, pp. 159 
%N = 3; R = 32; M = 2; Bin = 8; Bout = 10; % Meyer Baese book, pp. 268
%N = 3; R = 16; M = 1; Bin = 16; Bout = 16; % Thorwartl's PDF file 
%N = 5; R = 1024; M = 1; Bin = 16; Bout = 16; % Meyer Baese - conf. paper

N = 3; R = 8; M = 1; Bin = 12; Bout = 12; % Lyons' blog Figure 2 example 

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Find h_sub_j and "F_sub_j" values for (N-1) cascaded integrators
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    disp(' ')
	for j = N-1:-1:1
        h_sub_j = [];
        h_sub_j((R*M-1)*N + j -1 + 1) = 0;
        for k = 0:(R*M-1)*N + j -1
            for L = 0:floor(k/(R*M)) % Use uppercase "L" for loop variable
                Change_to_Result = (-1)^L*nchoosek(N, L)*nchoosek(N-j+k-R*M*L,k-R*M*L);
                h_sub_j(k+1) =  h_sub_j(k+1) + Change_to_Result;
            end % End "L" loop
        end % End "k" loop
        F_sub_j(j) = sqrt(sum(h_sub_j.^2));
	end % End "j" loop
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Define "F_sub_j" values for up to seven cascaded combs
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
F_sub_j_for_many_combs = sqrt([2, 6, 20, 70, 252, 924, 3432]); 
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%  Compute F_sub_j for last integrator stage
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
F_sub_j(N) = F_sub_j_for_many_combs(N-1)*sqrt(R*M);  % Last integrator   
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%  Compute F_sub_j for N cascaded filter's comb stages
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
for j = 2*N:-1:N+1
    F_sub_j(j) = F_sub_j_for_many_combs(2*N -j + 1);
end
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Define "F_sub_j" values for the final output register truncation
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
F_sub_j(2*N+1) = 1; % Final output register truncation
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Compute column vector of minus log base 2 of "F_sub_j" values
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
Minus_log2_of_F_sub_j = -log2(F_sub_j)';
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Compute total "Output_Truncation_Noise_Variance" terms
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
CIC_Filter_Gain = (R*M)^N;
Num_of_Bits_Growth = ceil(log2(CIC_Filter_Gain));
% The following is from Hogenauer's Eq. (11)
    %Num_Output_Bits_With_No_Truncation = Num_of_Bits_Growth +Bin -1;
    Num_Output_Bits_With_No_Truncation = Num_of_Bits_Growth +Bin;
Num_of_Output_Bits_Truncated = Num_Output_Bits_With_No_Truncation -Bout;
Output_Truncation_Noise_Variance = (2^Num_of_Output_Bits_Truncated)^2/12;
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Compute log base 2 of "Output_Truncation_Noise_Standard_Deviation" terms
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
Output_Truncation_Noise_Standard_Deviation = ...
    sqrt(Output_Truncation_Noise_Variance);
Log_base2_of_Output_Truncation_Noise_Standard_Deviation = ...
    log2(Output_Truncation_Noise_Standard_Deviation);
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Compute column vector of "half log base 2 of 6/N" terms
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    Half_Log_Base2_of_6_over_N = 0.5*log2(6/N);
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Compute desired "B_sub_j" vector
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
B_sub_j = floor(Minus_log2_of_F_sub_j ...
          + Log_base2_of_Output_Truncation_Noise_Standard_Deviation ...
          + Half_Log_Base2_of_6_over_N);
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
disp(' '), disp(' ')
disp(['N = ',num2str(N),',   R = ',num2str(R),',   M = ',num2str(M),...
        ',   Bin = ', num2str(Bin),',   Bout = ',num2str(Bout)])
disp(' ')
disp(['Num of Bits Growth Due To CIC Filter Gain = ', num2str(Num_of_Bits_Growth)])
disp(' ')
disp(['Num of Accumulator Bits With No Truncation = ', num2str(Num_Output_Bits_With_No_Truncation)])
disp(' ')
disp(['Output Truncation Noise Variance = ', num2str(Output_Truncation_Noise_Variance)])
disp(['Log Base2 of Output Truncation Noise Standard Deviation = ',...
        num2str(Log_base2_of_Output_Truncation_Noise_Standard_Deviation)])
disp(['Half Log Base2 of 6/N = ', num2str(Half_Log_Base2_of_6_over_N)])
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Create and display "Results" matrix
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
for Stage = 1:2*N
    Results(Stage,1) = Stage;
    Results(Stage,2) = F_sub_j(Stage);
    Results(Stage,3) = Minus_log2_of_F_sub_j(Stage);
    Results(Stage,4) = B_sub_j(Stage);
    Results(Stage,5) = Num_Output_Bits_With_No_Truncation -B_sub_j(Stage);
end
% Include final output stage truncation in "Results" matrix
    Results(2*N+1,1) = 2*N+1;  % Output stage number
    Results(2*N+1,2) = 1;
    Results(2*N+1,4) = Num_of_Output_Bits_Truncated;
    Results(2*N+1,5) = Bout;
    %Results % Display "Results" matrix in raw float-pt.form

% % Display "F_sub_j" values if you wish
% disp(' ')
% disp(' Stage        Fj        -log2(Fj)    Bj   Accum width')
% for Stage = 1:2*N+1
% 	disp(['  ',sprintf('%2.2g',Results(Stage,1)),sprintf('\t'),sprintf('%12.3g',Results(Stage,2)),...
%         sprintf('\t'),sprintf('%7.5g',Results(Stage,3)),sprintf('\t'),...
%         sprintf('%7.5g',Results(Stage,4)),sprintf('\t'),sprintf('%7.5g',Results(Stage,5))])
% end

% Display Stage number, # of truncated input bits, & Accumulator word widths
disp(' ')
disp(' Stage   Bj   Accum width')
for Stage = 1:2*N+1
	disp(['  ',sprintf('%2.0g',Results(Stage,1)),...
        sprintf('\t'),...
        sprintf('%3.5g',Results(Stage,4)),sprintf('\t'),sprintf('%6.5g',Results(Stage,5))])
end
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#endif
