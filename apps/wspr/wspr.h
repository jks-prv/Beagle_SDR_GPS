/*
 * k9an-wspr is a detector/demodulator/decoder for K1JT's 
 * Weak Signal Propagation Reporter (WSPR) mode.
 *
 * Copyright 2014, Steven Franke, K9AN
 * fixme: add GPL v3 notice
*/

#ifndef _WSPR_H_
#define _WSPR_H_

#include "types.h"
#include "datatypes.h"
#include "coroutines.h"
#include <fftw3.h>
#include "misc.h"

#define WSPR_FLOAT
#ifdef WSPR_FLOAT
	typedef float CPX_t;
	#define FFTW_COMPLEX fftwf_complex
	#define FFTW_MALLOC fftwf_malloc
	#define FFTW_FREE fftwf_free
	#define FFTW_PLAN fftwf_plan
	#define FFTW_PLAN_DFT_1D fftwf_plan_dft_1d
	#define FFTW_DESTROY_PLAN fftwf_destroy_plan
	#define FFTW_EXECUTE fftwf_execute
#else
	typedef double CPX_t;
	#define FFTW_COMPLEX fftw_complex
	#define FFTW_MALLOC fftw_malloc
	#define FFTW_FREE fftw_free
	#define FFTW_PLAN fftw_plan
	#define FFTW_PLAN_DFT_1D fftw_plan_dft_1d
	#define FFTW_DESTROY_PLAN fftw_destroy_plan
	#define FFTW_EXECUTE fftw_execute
#endif

//#define	WSPR_NSAMPS 1
#define	WSPR_NSAMPS 45000

#define	SYMTIME		(8192.0/12000.0)	// symbol time: 8192(256) samps @ 12k(375) sps, 683 ms, 1.46 Hz
#define	SRATE		375					// = 12000 kHz decim 32
#define	FSRATE		375.0
#define	CTIME		120					// capture time secs
#define	TPOINTS 	(SRATE * CTIME)

#define	BW_MAX		300.0				// +/- 150 Hz
#define	BFO			1500.0

// samples per symbol (at SRATE)
#define	FSPS		256.0				// round(SYMTIME * SRATE)
#define	SPS			256					// (int) FSPS
#define	NFFT		(SPS*2)
#define	HSPS		(SPS/2)

// groups
#define	GROUPS		(TPOINTS/NFFT)
#define	FPG			4					// FFTs per group

#define	NSYM_162	162
#define	FNSYM_162	162.0
#define	HSYM_81		(NSYM_162/2)
#define	FHSYM_81	(FNSYM_162/2)

extern const unsigned char pr3[NSYM_162];

void wspr_init(conn_t *cw, double frate);
void wspr_data(int run, u4_t freq, int nsamps, TYPECPX *samps);

void getStats(CPX_t *id, CPX_t *qd, long np, double *mi, double *mq, double *mi2, double *mq2, double *miq);
void unpack50(u1_t *dat, u4_t *n28b, u4_t *m22b);
void unpackcall(u4_t ncall, char *call);
void unpackgrid(u4_t ngrid, char *grid);
void deinterleave(unsigned char *sym);
int floatcomp(const void* elem1, const void* elem2);
void usage(void);

#endif
