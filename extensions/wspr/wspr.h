/*
 This file is part of program wsprd, a detector/demodulator/decoder
 for the Weak Signal Propagation Reporter (WSPR) mode.
 
 Copyright 2001-2015, Joe Taylor, K1JT
 
 Much of the present code is based on work by Steven Franke, K9AN,
 which in turn was based on earlier work by K1JT.
 
 Copyright 2014-2015, Steven Franke, K9AN
 
 License: GNU GPL v3
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "types.h"
#include "config.h"
#include "ext.h"
#include "misc.h"
#include "fano.h"
#include "jelinek.h"

#include <time.h>
#include <fftw3.h>

//#define WSPR_DEBUG_MSG	true
#define WSPR_DEBUG_MSG	false

//#define WSPR_APRINTF
#ifdef WSPR_APRINTF
    #define wspr_aprintf(fmt, ...) \
        rcprintf(rx_chan, fmt, ## __VA_ARGS__)
#else
    #define wspr_aprintf(fmt, ...)
#endif

#define WSPR_PRINTF
#ifdef WSPR_PRINTF
    #if 0
        #define wspr_printf(fmt, ...) \
            rcprintf(rx_chan, fmt, ## __VA_ARGS__)
	#else
	    #define wspr_printf(fmt, ...)
    #endif

    #if 1
        // debug (selectable)
        #define wspr_dprintf(fmt, ...) \
            /*printf("%3ds ", timer_sec() - passes_start);*/ \
            if (w->debug) rcprintf(rx_chan, fmt, ## __VA_ARGS__)
	#else
	    #define wspr_dprintf(fmt, ...)
	#endif

    #if 0
        // decode
	    #define wspr_d1printf(fmt, ...) \
	    	rcprintf(rx_chan, fmt, ## __VA_ARGS__)
	#else
	    #define wspr_d1printf(fmt, ...)
	#endif

    #if 0
        // decode more detail
	    #define wspr_d2printf(fmt, ...) \
	    	printf(fmt, ## __VA_ARGS__)
	#else
	    #define wspr_d2printf(fmt, ...)
	#endif

    #if 0
        // upload
	    #define wspr_ulprintf(fmt, ...) \
	    	rcprintf(w->rx_chan, fmt, ## __VA_ARGS__)
	#else
	    #define wspr_ulprintf(fmt, ...)
	#endif

    #if 0
        // general
        #define wspr_gprintf(fmt, ...) \
            printf(fmt, ## __VA_ARGS__)
	#else
	    #define wspr_gprintf(fmt, ...)
	#endif

#else
	#define wspr_printf(fmt, ...)
	#define wspr_d1printf(fmt, ...)
    #define wspr_d2printf(fmt, ...)
	#define wspr_dprintf(fmt, ...)
	#define wspr_ulprintf(fmt, ...)
	#define wspr_gprintf(fmt, ...)
#endif

#define WSPR_CHECKING
#ifdef WSPR_CHECKING
    #define wspr_array_dim(d,l) assert_array_dim(d,l)
    #define WSPR_CHECK(x) x
    #define WSPR_CHECK_ALT(x,y) x
#else
    #define wspr_array_dim(d,l)
    #define WSPR_CHECK(x)
    #define WSPR_CHECK_ALT(x,y) y
#endif

#define WSPR_FLOAT
#ifdef WSPR_FLOAT
	typedef float WSPR_CPX_t;
	#define WSPR_FFTW_COMPLEX fftwf_complex
	#define WSPR_FFTW_MALLOC fftwf_malloc
	#define WSPR_FFTW_FREE fftwf_free
	#define WSPR_FFTW_PLAN fftwf_plan
	#define WSPR_FFTW_PLAN_DFT_1D fftwf_plan_dft_1d
	#define WSPR_FFTW_DESTROY_PLAN fftwf_destroy_plan
	#define WSPR_FFTW_EXECUTE fftwf_execute
#else
	typedef double WSPR_CPX_t;
	#define WSPR_FFTW_COMPLEX fftw_complex
	#define WSPR_FFTW_MALLOC fftw_malloc
	#define WSPR_FFTW_FREE fftw_free
	#define WSPR_FFTW_PLAN fftw_plan
	#define WSPR_FFTW_PLAN_DFT_1D fftw_plan_dft_1d
	#define WSPR_FFTW_DESTROY_PLAN fftw_destroy_plan
	#define WSPR_FFTW_EXECUTE fftw_execute
#endif

#define	SYMTIME		(FSPS / FSRATE)		// symbol time: 256 samps @ 375 srate, 683 ms, 1.46 Hz

#define	SRATE		375					// design sample rate
#define	FSRATE		375.0
#define	CTIME		120					// capture time secs
#define	TPOINTS 	(SRATE * CTIME)     // 45000

#define NBINS       411

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
#define	GROUPS		(TPOINTS/NFFT)      // 87
#define	FPG			4					// FFTs per group

#define	NSYM_162	162
#define	FNSYM_162	162.0
#define	HSYM_81		(NSYM_162/2)
#define	FHSYM_81	(FNSYM_162/2)

#define	NBITS		HSYM_81

#define	LEN_DECODE	((NBITS + 7) / 8)
#define	LEN_CALL	(12 + SPACE_FOR_NULL)		// max of AANLL, ppp/AANLLL, AANLL/ss, plus 2
#define	LEN_C_L_P	(22 + SPACE_FOR_NULL)		// 22 = 12 call, sp, 6 grid, sp, 2 pwr
#define	LEN_GRID	(6 + SPACE_FOR_NULL)
#define	LEN_PWR     (2 + SPACE_FOR_NULL)

#define	WSPR_STACKSIZE	2000

#define NPK 256
#define MAX_NPK 12

typedef struct {
	bool ignore;
	float freq0, snr0, drift0, sync0;
	int shift0, bin0;
	int freq_idx, flags;
	char snr_call[LEN_CALL];
} wspr_pk_t;

#define	WSPR_F_BIN			0x0fff
#define	WSPR_F_DECODING		0x1000
#define	WSPR_F_DELETE		0x2000
#define WSPR_F_DECODED		0x4000
#define	WSPR_F_IMAGE		0x8000

// assigned constants
extern int nffts;
extern int nbins_411;
extern int hbins_205;

typedef struct {
    int r_valid;
	float freq;
	char call[LEN_CALL];
	char grid[LEN_GRID];
	char pwr[LEN_PWR];
	int hour, min;
	float snr, dt_print, drift1;
	double freq_print;
	char c_l_p[LEN_C_L_P];
	int dBm;
} wspr_decode_t;

typedef struct {
    u1_t start, stop;
} send_peaks_q_t;

#define N_PING_PONG 2

typedef struct {
	WSPR_CPX_t i_data[N_PING_PONG][TPOINTS], q_data[N_PING_PONG][TPOINTS];
	float pwr_samp[N_PING_PONG][NFFT][FPG*GROUPS];
	float pwr_sampavg[N_PING_PONG][NFFT];
	float savg[NFFT];
	u1_t ws[NBINS+1];
	
	#define WSPR_RENORM_FFT        0
	#define WSPR_RENORM_DECODE     1
	float smspec[2][NBINS], tmpsort[2][NBINS];

    wspr_pk_t pk_snr[NPK];
	#define MAX_NPKQ 512
	send_peaks_q_t send_peaks_q[MAX_NPKQ];
} wspr_buf_t;

typedef struct {
    WSPR_CHECK(u4_t magic1;)
    
	bool init;
	int rx_chan;
	bool create_tasks;
	int ping_pong, fft_ping_pong, decode_ping_pong;
	int capture;
	int status, status_resume;
	bool send_error, abort_decode;
	int WSPR_FFTtask_id, WSPR_DecodeTask_id;
	int debug;
	bool iwbp;
	
	// options
	int quickmode, medium_effort, more_candidates, stack_decoder, subtraction;
	#define WSPR_TYPE_2MIN 2
	#define WSPR_TYPE_15MIN 15
	int wspr_type;
	
	// autorun
	bool autorun;
	int instance;
	conn_t *arun_csnd, *arun_cext;
	double arun_cf_MHz, arun_deco_cf_MHz;
	char *arun_stat_cmd;
	u4_t arun_last_status_sent;
	int arun_decoded, arun_last_decoded;
	
	// IWBP
	int prev_per;
	
	// sampler
	bool reset, tsync;
	int last_sec;
	int decim, didx, group;
	double fi;
	
	// FFT task
	WSPR_FFTW_COMPLEX *fftin, *fftout;
	WSPR_FFTW_PLAN fftplan;
	int FFTtask_group;
	int fft_init, not_launched;
	u4_t fft_wakeups, fft_runs;
	
	// computed by sampler or FFT task, processed by decode task
	time_t utc[N_PING_PONG];
	wspr_buf_t *buf;        // prevent sizeof(wspr_t) being huge for benefit of debugger

	// decode task
    int bfo;
	float min_snr, snr_scaling_factor;
	struct snode *stack;
	float dialfreq_MHz, centerfreq_MHz, cf_offset;
	u1_t symbols[NSYM_162], decdata[LEN_DECODE], channel_symbols[NSYM_162];
	char callsign[LEN_CALL], call_loc_pow[LEN_C_L_P], grid[LEN_GRID];
	int dBm;
	#define NDECO 32
	wspr_decode_t deco[NDECO];

	// decode task shmem
	int uniques;
	int send_peaks_seq_parent, send_peaks_seq;
	int npk;
    wspr_pk_t pk_freq[MAX_NPK], pk_save[MAX_NPK];
    int send_decode_seq_parent, send_decode_seq;

	bool test;
	int skip_upload;
    s2_t *s2p;
    int nsamps;

    WSPR_CHECK(u4_t magic2;)
} wspr_t;

// configuration
typedef struct {
    u64_t freq_offset_Hz;
    bool arun_restart_offset;

    int num_autorun;
    bool arun_suspend_restart_victims;
    char *rcall;
    char rgrid[LEN_GRID];
    latLon_t r_loc;
	bool GPS_update_grid;
	bool syslog, spot_log;

    s2_t *s2p_start, *s2p_end;
    int tsamps;
} wspr_conf_t;

extern wspr_conf_t wspr_c;

typedef struct {
    wspr_t wspr[MAX_RX_CHANS];
    wspr_buf_t wspr_buf[MAX_RX_CHANS];
} wspr_shmem_t;

#include "shmem_config.h"

#ifdef MULTI_CORE
    //#define WSPR_SHMEM_DISABLE_TEST
    #ifdef WSPR_SHMEM_DISABLE_TEST
        #warning dont forget to remove WSPR_SHMEM_DISABLE_TEST
        #define WSPR_SHMEM_DISABLE
    #else
        // shared memory enabled
    #endif
#else
    #define WSPR_SHMEM_DISABLE
#endif

#include "shmem.h"

#ifdef WSPR_SHMEM_DISABLE
    extern wspr_shmem_t *wspr_shmem_p;
    #define WSPR_SHMEM wspr_shmem_p
#else
    #define WSPR_SHMEM (&shmem->wspr_shmem)
#endif

#define WSPR_YIELD NextTask("wspr")

#ifdef WSPR_SHMEM_DISABLE
    #define WSPR_SHMEM_YIELD NextTask("wspr")
#else
    #define WSPR_SHMEM_YIELD
#endif
#define YIELD_EVERY_N_TIMES 64

void wspr_init();
void wspr_update_rgrid(char *rgrid);
bool wspr_update_vars_from_config(bool called_at_init_or_restart);
void wspr_data(int rx_chan, int instance, int nsamps, TYPECPX *samps);
void wspr_decode(int rx_chan);
void wspr_send_peaks(wspr_t *w, int start, int stop);
void wspr_send_decode(wspr_t *w, int seq);

void wspr_autorun_start(bool initial);
void wspr_autorun_restart();

typedef enum { FIND_BEST_TIME_LAG, FIND_BEST_FREQ, CALC_SOFT_SYMS } wspr_mode_e;

void renormalize(wspr_t *w, float psavg[], float smspec[], float tmpsort[]);

void unpack50(u1_t *dat, u4_t *call_28b, u4_t *grid_pwr_22b, u4_t *grid_15b, u4_t *pwr_7b);
int unpackcall(u4_t call_28b, char *call);
int unpackgrid(u4_t grid_15b, char *grid);
int unpackpfx(int32_t nprefix, char *call);
void deinterleave(unsigned char *sym);
int unpk_(u1_t *decdata, char *call_loc_pow, char *callsign, char *grid, int *dBm);

int snr_comp(const void *elem1, const void *elem2);
int freq_comp(const void *elem1, const void *elem2);

void wspr_set_latlon_from_grid(char *grid);
