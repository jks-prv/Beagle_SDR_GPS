// Licensed under a Creative Commons Attribution 3.0 Unported License

// Original work Copyright (c) 2012 Rick Lyons (http://www.dsprelated.com/code.php?submittedby=14446)
// Original work: http://www.dsprelated.com/showcode/269.php

// Derivative work Copyright (c) 2014 John Seamons, ZL/KF6VO

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
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

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
	for (i=2; i<=n; i++) {
		d *= i;
		if (isinf(d)) break;
	}
	return d;
}

float nchoosek(int n, int k)
{
	double fn = factd(n);
	double fnk = factd(n-k);
	double fk = factd(k);
	double nck = fn / (fnk * fk);
	return nck;
}

#define	EMPTY		0		// generate an empty file for Verilog include file compatibility
#define	NO_PRUNE	1		// generate Verilog that has no pruning for comparison purposes
#define INTEG_COMB	2		// prune both integrator and comb sections (usual case)
#define	INTEG_ONLY	3		// generate only pruned integrator section (for use with a sequential comb implementation)

void cic_gen(char *fn, int prune, int N, int R, int Bin, int Bout)
{
	// 1..N-1,N integrators
	// N+1..2*N combs
	// 2*N+1	output
	
	#define M 1		// fixed differential delay

	const char *mode[4] = { "EMPTY", "NO_PRUNE", "INTEG_COMB", "INTEG_ONLY" };

	FILE *fp;
	if ((fp = fopen(fn, "w")) == NULL) sys_panic("fopen");
	if (prune == EMPTY) { fclose(fp); return; }
	
	fprintf(fp, "// generated file\n\n");
	fprintf(fp, "// CIC: %s N=%d R=%d M=%d Bin=%d Bout=%d\n", mode[prune], N, R, M, Bin, Bout);

	printf("\n\"%s\" %s N=%d R=%d M=%d Bin=%d Bout=%d\n", fn, mode[prune], N, R, M, Bin, Bout);

	#define	NX	(2*N+1)		// N-integrators + N-combs + output register
	#define	MAX	(2*MAX_N+1)
	
	int s, i, j, k, L;
	
	#define MAX_N		7		// max number of stages 
	#define MAX_R		8192	// max decimation
	assert(N <= MAX_N);
	assert(R <= MAX_R);
	
	#define	HJ_LEN		( (MAX_R*M-1)*MAX_N + (MAX_N-1) )
	float h_sub_j[HJ_LEN+1];

	float F_sub_j[MAX+1];
	float B_sub_j[MAX+1];

	// integrators (except last)
	float Change_to_Result;
	for (j = N-1; j >= 1; j--) {
		i = (R*M-1)*N + j-1+1;
		assert(i <= HJ_LEN);
		h_sub_j[i] = 0;
	
		for (k = 0; k <= (R*M-1)*N +j-1; k++) {
			h_sub_j[k+1] = 0;
			float f_k = k;
			for (L = 0; L <= floorf(f_k/(R*M)); L++) {
				Change_to_Result = powf(-1,L) * nchoosek(N, L) * nchoosek(N-j+k-R*M*L, k-R*M*L);
				assert((k+1) <= HJ_LEN);
				h_sub_j[k+1] += Change_to_Result;
			}
		}
		
		float sum=0;
		for (k = 0; k <= (R*M-1)*N +j-1; k++) {
			sum += h_sub_j[k+1]*h_sub_j[k+1];
		}
		
		F_sub_j[j] = sqrtf(sum);
	}
	
	// up-to MAX_N cascaded combs
	int h_sub_j_comb[MAX_N+1] = {0, 2, 6, 20, 70, 252, 924, 3432};		// pre-computed, see Rick's paper
	float F_sub_j_for_many_combs[MAX_N];
	for (s = 1; s <= MAX_N; s++) {
		F_sub_j_for_many_combs[s] = sqrtf(h_sub_j_comb[s]);
	}
	
	// last integrator
	assert(N <= MAX_N);
	F_sub_j[N] = F_sub_j_for_many_combs[N-1] * sqrtf(R*M);

	// N cascaded combs
	for (j = 2*N; j >= N+1; j--) {
		F_sub_j[j] = F_sub_j_for_many_combs[2*N -j + 1];
	}
	
	// output register
	F_sub_j[2*N+1] = 1;
	
	float Minus_log2_of_F_sub_j[MAX+1];
	for (j = 1; j <= NX; j++) {
		Minus_log2_of_F_sub_j[j] = -log2f(F_sub_j[j]);
	}
	
	float CIC_Filter_Gain = powf((R*M), N);
	float Num_of_Bits_Growth = ceilf(log2f(CIC_Filter_Gain));
    float Num_Output_Bits_With_No_Truncation = Num_of_Bits_Growth + Bin;
	float Num_of_Output_Bits_Truncated = Num_Output_Bits_With_No_Truncation - Bout;
	float Output_Truncation_Noise_Variance = powf((powf(2, Num_of_Output_Bits_Truncated)), 2) /12;
	float Output_Truncation_Noise_Standard_Deviation = sqrtf(Output_Truncation_Noise_Variance);
	float Log_base2_of_Output_Truncation_Noise_Standard_Deviation = log2f(Output_Truncation_Noise_Standard_Deviation);
	float Half_Log_Base2_of_6_over_N = 0.5 * log2f(6.0/N);

	for (j = 1; j <= (NX-1); j++) {
		B_sub_j[j] = Minus_log2_of_F_sub_j[j] + Log_base2_of_Output_Truncation_Noise_Standard_Deviation + Half_Log_Base2_of_6_over_N;
	}
	
	printf("growth %.0f = ceil(N=%d * log2(R=%d))\n", Num_of_Bits_Growth, N, R);
	fprintf(fp, "// growth %.0f = ceil(N=%d * log2(R=%d))\n", Num_of_Bits_Growth, N, R);

	printf("Bin %d + growth %.0f = acc_max %.0f\n", Bin, Num_of_Bits_Growth, Num_Output_Bits_With_No_Truncation);
	fprintf(fp, "// Bin %d + growth %.0f = acc_max %.0f\n\n", Bin, Num_of_Bits_Growth, Num_Output_Bits_With_No_Truncation);
	
	// results
	int pB_sub_j[MAX+1], pACC[MAX+1];
	float Bj;

	pACC[0] = Num_Output_Bits_With_No_Truncation;
	for (s=1; s<=(NX-1); s++) {
		Bj = (isnan(B_sub_j[s]) || (B_sub_j[s]<0))? 0 : B_sub_j[s];
		pB_sub_j[s] = Bj;
		pACC[s] = (prune != NO_PRUNE)? ceilf(Num_Output_Bits_With_No_Truncation - Bj) : Num_Output_Bits_With_No_Truncation;
	}
	pB_sub_j[NX] = Num_of_Output_Bits_Truncated;
	pACC[NX] = Bout;
	
	// fix input
	pACC[0] = pACC[1];
	
	printf(" stage     Fj    Bj   acc trunc\n");
	int end = (prune == INTEG_ONLY)? N : NX;

	for (s=1; s<=end; s++) {
		printf("%s%d ", (s<=N)? "integ": ( (s<=2*N)? " comb" : "  out" ), (s<=N)? s: ( (s<=2*N)? s-N:0 ));
		if (isnan(F_sub_j[s]) || isinf(F_sub_j[s]) || (F_sub_j[s] < 1000))
			printf("%6.1f ", F_sub_j[s]);
		else
			printf(" large ");
		printf("%5d %5d %5d\n", pB_sub_j[s], pACC[s], pACC[s-1]-pACC[s]);
	}
	
	int NBO = Num_Output_Bits_With_No_Truncation-1;
	
	if (prune == INTEG_ONLY) {
		for (s=0; s<=N; s++) {
			fprintf(fp, "wire signed [%d:0] integrator%d_data_i, integrator%d_data_q;\n", pACC[s]-1, s, s);
		}
	
		fprintf(fp, "\nassign integrator0_data_i = in_i;\n");
		fprintf(fp, "assign integrator0_data_q = in_q;\n\n");
		
		for (s=1; s<=N; s++) {
			fprintf(fp, 
				"cic_integrator #(.WIDTH(%d)) cic_integrator%d_inst_i(\n"
				"	.clock(clock),\n"
				"	.reset(reset),\n"
				"	.strobe(in_strobe),\n"
				"	.in_data(integrator%d_data_i[%d -:%d]),	// trunc %d bits\n"
				"	.out_data(integrator%d_data_i)\n"
				");\n\n", pACC[s], s, s-1, pACC[s-1]-1, pACC[s], pACC[s-1]-pACC[s], s);
			fprintf(fp, 
				"cic_integrator #(.WIDTH(%d)) cic_integrator%d_inst_q(\n"
				"	.clock(clock),\n"
				"	.reset(reset),\n"
				"	.strobe(in_strobe),\n"
				"	.in_data(integrator%d_data_q[%d -:%d]),	// trunc %d bits\n"
				"	.out_data(integrator%d_data_q)\n"
				");\n\n", pACC[s], s, s-1, pACC[s-1]-1, pACC[s], pACC[s-1]-pACC[s], s);
		}
	
		fprintf(fp, "assign integ_out_i = { integrator%d_data_i, {%d-%d{1'b0}} };\n", N, NBO+1, pACC[N]);
		fprintf(fp, "assign integ_out_q = { integrator%d_data_q, {%d-%d{1'b0}} };\n\n", N, NBO+1, pACC[N]);
	} else {

		for (s=0; s<=N; s++) {
			fprintf(fp, "wire signed [%d:0] integrator%d_data;\n", pACC[s]-1, s);
		}
		
		for (s=N; s<=2*N; s++) {
			fprintf(fp, "wire signed [%d:0] comb%d_data;\n", pACC[s]-1, s-N);
		}
		
		fprintf(fp, "\n// important that \"in\" be declared signed by wrapper code\n");
		fprintf(fp, "// so this assignment will sign-extend:\n");
		fprintf(fp, "assign integrator0_data = in;\n\n");
		
		for (s=1; s<=N; s++) {
			fprintf(fp, 
				"cic_integrator #(.WIDTH(%d)) cic_integrator%d_inst(\n"
				"	.clock(clock),\n"
				"	.reset(reset),\n"
				"	.strobe(in_strobe),\n"
				"	.in_data(integrator%d_data[%d -:%d]),	// trunc %d bits\n"
				"	.out_data(integrator%d_data)\n"
				");\n\n", pACC[s], s, s-1, pACC[s-1]-1, pACC[s], pACC[s-1]-pACC[s], s);
		}
	
		fprintf(fp, "assign comb0_data = integrator%d_data;\n\n", N);
		
		for (s=N+1; s<=2*N; s++) {
			fprintf(fp, 
				"cic_comb #(.WIDTH(%d)) cic_comb%d_inst(\n"
				"	.clock(clock),\n"
				"	.reset(reset),\n"
				"	.strobe(out_strobe),\n"
				"	.in_data(comb%d_data[%d -:%d]),	// trunc %d bits\n"
				"	.out_data(comb%d_data)\n"
				");\n\n", pACC[s], s-N, s-N-1, pACC[s-1]-1, pACC[s], pACC[s-1]-pACC[s], s-N);
		}
	
		fprintf(fp, "assign out = comb%d_data[%d -:%d];	// trunc %d bits\n",
			N, pACC[2*N]-1, pACC[NX], pACC[NX-1]-pACC[NX]);
	}
	fclose(fp);
}

int main (int argc, char *argv[])
{
	//cic_gen(generated_filename, mode, #stages, decimation, bits_in, bits_out);

#include "kiwi.gen.h"

	cic_gen("cic_rx1.vh", INTEG_COMB, RX1_STAGES, RX1_DECIM, RX1_BITS, RX2_BITS);
	//cic_gen("cic_rx1.vh", NO_PRUNE, RX1_STAGES, RX1_DECIM, RX1_BITS, RX2_BITS);

#ifdef USE_RX_SEQ
	// For USE_RX_SEQ the cic_gen() mode is INTEG_ONLY because the comb part of the
	// rx second stage is done by the rx sequential state machine.
	cic_gen("cic_rx2.vh", EMPTY, 0, 0, 0, 0);
	cic_gen("cic_rx3.vh", INTEG_ONLY, RX2_STAGES, RX2_DECIM, RX2_BITS, RXO_BITS);
#else
	cic_gen("cic_rx2.vh", INTEG_COMB, RX2_STAGES, RX2_DECIM, RX2_BITS, RXO_BITS);
	cic_gen("cic_rx3.vh", EMPTY, 0, 0, 0, 0);
#endif

#ifdef USE_WF_PRUNE
	#ifdef USE_WF_1CIC
		cic_gen("cic_wf1.vh", INTEG_COMB, WF1_STAGES, WF_1CIC_MAXD, WF1_BITS, WFO_BITS);
		cic_gen("cic_wf2.vh", EMPTY, 0, 0, 0, 0);
	#else
		cic_gen("cic_wf1.vh", INTEG_COMB, WF1_STAGES, WF_1CIC_MAXD, WF1_BITS, WF2_BITS);
		cic_gen("cic_wf2.vh", INTEG_COMB, WF2_STAGES, WF_2CIC_MAXD, WF2_BITS, WFO_BITS);
	#endif
#else
	cic_gen("cic_wf1.vh", EMPTY, 0, 0, 0, 0);
	cic_gen("cic_wf2.vh", EMPTY, 0, 0, 0, 0);
#endif

	return(0);
}
