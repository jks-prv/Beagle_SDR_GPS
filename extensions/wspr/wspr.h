/*
 * k9an-wspr is a detector/demodulator/decoder for K1JT's 
 * Weak Signal Propagation Reporter (WSPR) mode.
 *
 * Copyright 2014, Steven Franke, K9AN
 * fixme: add GPL v3 notice
*/

#pragma once

#include "config.h"
#include "ext.h"
#include "misc.h"
#include "types.h"
#include "datatypes.h"

#include "fano.h"
#include "jelinek.h"

#include <fftw3.h>

//#define WSPR_DEBUG_MSG	true
#define WSPR_DEBUG_MSG	false

//jksd
#define WSPR_PRINTF
#ifdef WSPR_PRINTF
	#define wprintf(fmt, ...) printf(fmt, ## __VA_ARGS__)
#else
	#define wprintf(fmt, ...)
#endif

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

#define	SYMTIME		(FSPS / FSRATE)		// symbol time: 256 samps @ 375 srate, 683 ms, 1.46 Hz

#define	SRATE		375					// design sample rate
#define	FSRATE		375.0
#define	CTIME		120					// capture time secs
#define	TPOINTS 	(SRATE * CTIME)

#define	FMIN		-110				// frequency range to search
#define	FMAX		110
//#define	FMIN		-150				// frequency range to search
//#define	FMAX		150
#define	BW_MAX		300.0				// +/- 150 Hz

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

#define	NBITS		HSYM_81

#define	LEN_DECODE	((NBITS + 7) / 8)
#define	LEN_CALL	(12 + SPACE_FOR_NULL)
#define	LEN_C_L_P	(22 + SPACE_FOR_NULL)		// 22 = 12 call, sp, 6 grid, sp, 2 pwr
#define	LEN_GRID	(6 + SPACE_FOR_NULL)

#define	STACKSIZE	20000

#define NPK 256
#define MAX_NPK 8

typedef struct {
	float freq0, snr0, drift0, sync0;
	int shift0, bin0;
	int freq_idx, flags;
	char snr_call[LEN_CALL];
} pk_t;

#define	WSPR_F_BIN			0x0fff
#define	WSPR_F_DECODING		0x1000
#define	WSPR_F_DELETE		0x2000

#define NTASK 64
#define	NT() NextTask("wspr")

// defined constants
extern int nffts;
extern int nbins_411;
extern int hbins_205;

struct wspr_t {
	int rx_chan;
	bool create_tasks;
	int ping_pong, fft_ping_pong, decode_ping_pong;
	int capture, demo;
	int status, status_resume;
	bool send_error, abort_decode;
	int WSPR_FFTtask_id, WSPR_DecodeTask_id;
	
	// options
	int deco;
	int quickmode, medium_effort, more_candidates, stackdecoder, subtraction;
	#define WSPR_TYPE_2MIN 2
	#define WSPR_TYPE_15MIN 15
	int wspr_type;
	
	// sampler
	bool reset, tsync;
	int last_sec;
	int decim, didx, group;
	int demo_sn, demo_rem;
	double fi;
	
	// FFT task
	FFTW_COMPLEX *fftin, *fftout;
	FFTW_PLAN fftplan;
	int FFTtask_group;
	
	// computed by sampler or FFT task, processed by decode task
	time_t utc[2];
	CPX_t i_data[2][TPOINTS], q_data[2][TPOINTS];
	float pwr_samp[2][NFFT][FPG*GROUPS];
	float pwr_sampavg[2][NFFT];

	// decode task
	float min_snr, snr_scaling_factor;
	struct snode *stack;
	float dialfreq;
	#define LEN_HASHTAB 32768
	char *hashtab;
	u1_t symbols[NSYM_162], decdata[LEN_DECODE], channel_symbols[NSYM_162];
	char callsign[LEN_CALL], call_loc_pow[LEN_C_L_P], grid[LEN_GRID];
	float allfreqs[NPK];
	char allcalls[NPK][LEN_CALL];
};

// configuration
extern int bfo;

void wspr_init(conn_t *cw, double frate);
void wspr_data(int run, u4_t freq, int nsamps, TYPECPX *samps);
void wspr_decode_old(wspr_t *w);
void wspr_decode(wspr_t *w);
void wspr_send_peaks(wspr_t *w, pk_t *pk, int npk);

void sync_and_demodulate_old(
	CPX_t *id, CPX_t *qd, long np,
	unsigned char *symbols, float *f1, float fstep,
	int *shift1,
	int lagmin, int lagmax, int lagstep,
	float drift1, float *sync, int mode);

void sync_and_demodulate(
	CPX_t *id, CPX_t *qd, long np,
	unsigned char *symbols, float *f1, int ifmin, int ifmax, float fstep,
	int *shift1,
	int lagmin, int lagmax, int lagstep,
	float drift1, int symfac, float *sync, int mode);

void renormalize(wspr_t *w, float psavg[], float smspec[]);

void unpack50(u1_t *dat, u4_t *call_28b, u4_t *grid_pwr_22b, u4_t *grid_15b, u4_t *pwr_7b);
int unpackcall(u4_t call_28b, char *call);
int unpackgrid(u4_t grid_15b, char *grid);
int unpackpfx(u4_t nprefix, char *call);
void deinterleave(unsigned char *sym);
int unpk_(u1_t *decdata, char *hashtab, char *call_loc_pow, char *callsign, char *grid, int *dBm);
void subtract_signal(float *id, float *qd, long np,
	float f0, int shift0, float drift0, unsigned char* channel_symbols);
void subtract_signal2(float *id, float *qd, long np,
	float f0, int shift0, float drift0, unsigned char* channel_symbols);

int snr_comp(const void *elem1, const void *elem2);
int freq_comp(const void *elem1, const void *elem2);

#define WSPR_DEMO_NSAMPS 45000
extern TYPECPX wspr_demo_samps[WSPR_DEMO_NSAMPS];

void set_reporter_grid(char *grid);
double grid_to_distance_km(char *grid);
