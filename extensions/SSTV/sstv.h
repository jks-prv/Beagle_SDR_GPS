#pragma once

#include "types.h"
#include "kiwi.h"
#include "coroutines.h"
#include "data_pump.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>

#include <fftw3.h>

#define SSTV_FLOAT
#ifdef SSTV_FLOAT
	typedef float SSTV_REAL;
    #define SSTV_MSIN(x) sinf(x)
    #define SSTV_MCOS(x) cosf(x)
    #define SSTV_MTAN(x) tanf(x)
    #define SSTV_MPOW(x,y) powf(x,y)
    #define SSTV_MEXP(x) expf(x)
    #define SSTV_MFABS(x) fabsf(x)
    #define SSTV_MLOG(x) logf(x)
    #define SSTV_MLOG10(x) log10f(x)
    #define SSTV_MSQRT(x) sqrtf(x)
    #define SSTV_MATAN(x) atanf(x)
    #define SSTV_MFMOD(x,y) fmodf(x,y)
    #define SSTV_MATAN2(x,y) atan2f(x,y)
    #define SSTV_MASIN(x) asinf(x)
    #define SSTV_MROUND(x) roundf(x)

	#define SSTV_FFTW_COMPLEX fftwf_complex
	#define SSTV_FFTW_MALLOC fftwf_malloc
	#define SSTV_FFTW_ALLOC_REAL fftwf_alloc_real
	#define SSTV_FFTW_ALLOC_COMPLEX fftwf_alloc_complex
	#define SSTV_FFTW_FREE fftwf_free
	#define SSTV_FFTW_PLAN fftwf_plan
	#define SSTV_FFTW_PLAN_DFT_1D fftwf_plan_dft_1d
	#define SSTV_FFTW_PLAN_DFT_R2C_1D fftwf_plan_dft_r2c_1d
	#define SSTV_FFTW_DESTROY_PLAN fftwf_destroy_plan
	#define SSTV_FFTW_EXECUTE fftwf_execute
#else
	typedef double SSTV_REAL;
    #define SSTV_MSIN(x) sin(x)
    #define SSTV_MCOS(x) cos(x)
    #define SSTV_MTAN(x) tan(x)
    #define SSTV_MPOW(x,y) pow(x,y)
    #define SSTV_MEXP(x) exp(x)
    #define SSTV_MFABS(x) fabs(x)
    #define SSTV_MLOG(x) log(x)
    #define SSTV_MLOG10(x) log10(x)
    #define SSTV_MSQRT(x) sqrt(x)
    #define SSTV_MATAN(x) atan(x)
    #define SSTV_MFMOD(x,y) fmod(x,y)
    #define SSTV_MATAN2(x,y) atan2(x,y)
    #define SSTV_MASIN(x) asin(x)
    #define SSTV_MROUND(x) round(x)

	#define SSTV_FFTW_COMPLEX fftw_complex
	#define SSTV_FFTW_MALLOC fftw_malloc
	#define SSTV_FFTW_ALLOC_REAL fftw_alloc_real
	#define SSTV_FFTW_ALLOC_COMPLEX fftw_alloc_complex
	#define SSTV_FFTW_FREE fftw_free
	#define SSTV_FFTW_PLAN fftw_plan
	#define SSTV_FFTW_PLAN_DFT_1D fftw_plan_dft_1d
	#define SSTV_FFTW_PLAN_DFT_R2C_1D fftw_plan_dft_r2c_1d
	#define SSTV_FFTW_DESTROY_PLAN fftw_destroy_plan
	#define SSTV_FFTW_EXECUTE fftw_execute
#endif

#define SSTV_FILE

#define MINSLANT 30
#define MAXSLANT 150
#define SYNCPIXLEN 1.5e-3

#define SSTV_MS_2_SAMPS(ms) (sstv.nom_rate * (ms) / 1000)
#define SSTV_MS_2_MAX_SAMPS(ms) (MAX_SND_RATE * (ms) / 1000)

#define GET_BIN(freq, FFTLen) ((u4_t) ( ((SSTV_REAL) (freq)) / sstv.nom_rate * (FFTLen)) )
//u4_t GET_BIN(SSTV_REAL Freq, u4_t FFTLen);

//SSTV_REAL POWER(SSTV_FFTW_COMPLEX coeff);
#define POWER(coeff) (coeff[0]*coeff[0] + coeff[1]*coeff[1])
//#define POWER(coeff) (SSTV_MPOW(coeff[0],2) + SSTV_MPOW(coeff[1],2))

typedef struct {
    int X;
    int Y;
    int Time;
    u1_t Channel;
    bool Last;
} PixelGrid_t;

typedef struct {
	u4_t nom_rate;

	#ifdef SSTV_FILE
        s2_t *s2p_start, *s2p_end;
    #endif
    
} sstv_t;
extern sstv_t sstv;

typedef struct {
	int rx_chan;
	int run;
	bool test;
	
	bool task_created;
	tid_t tid;
	int rd_pos, rd_idx;
	bool seq_init;
	u4_t seq;
	
    struct {
        SSTV_REAL           *in;
        SSTV_FFTW_COMPLEX   *out;
        SSTV_FFTW_PLAN      Plan1024;
        SSTV_FFTW_PLAN      Plan2048;
    } fft;

	struct {
	    #define PCM_BUFLEN 4096
	    s2_t *Buffer;
	    int WindowPtr;
	} pcm;

    struct {
        s2_t HedrShift;
        u1_t Mode;
        SSTV_REAL Rate;
        int Skip;
    } pic;

    int Image_len;
    u1_t *Image;
    
    int StoredLum_len;
    u1_t *StoredLum;
    
    int HasSync_len;
    bool *HasSync;
    
    int PixelGrid_len;
    PixelGrid_t *PixelGrid;
    
    int pixels_len;
    u1_t *pixels;
    
    int fm_sample_interval;

	#ifdef SSTV_FILE
        s2_t *s2p, *s22p;
    #endif
    
} sstv_chan_t;
extern sstv_chan_t sstv_chan[MAX_RX_CHANS];

// SSTV modes
enum {
  UNKNOWN=0,
  M1,    M2,   M3,    M4,
  S1,    S2,   SDX,
  R72,   R36,  R24,   R24BW, R12BW, R8BW,
  W2120, W2180,
  PD50,  PD90, PD120, PD160, PD180, PD240, PD290,
  P3,    P5,   P7
};

// Color encodings
enum { GBR, RGB, YUV, BW };

typedef struct {
  char   *Name;
  char   *ShortName;
  SSTV_REAL  SyncTime;
  SSTV_REAL  PorchTime;
  SSTV_REAL  SeptrTime;
  SSTV_REAL  PixelTime;
  SSTV_REAL  LineTime;
  u2_t ImgWidth;
  u2_t NumLines;
  u1_t  LineHeight;
  u1_t  ColorEnc;
} ModeSpec_t;
extern ModeSpec_t ModeSpec[];

extern u1_t VISmap[];

extern bool   Abort;
extern bool   Adaptive;
extern bool   ManualActivated;
extern bool   ManualResync;

void sstv_pcm_once(sstv_chan_t *e);
void sstv_pcm_init(sstv_chan_t *e);
void sstv_pcm_read(sstv_chan_t *e, s4_t numsamples);

u1_t sstv_get_vis(sstv_chan_t *e);

void sstv_video_once(sstv_chan_t *e);
void sstv_video_init(sstv_chan_t *e, SSTV_REAL rate, u1_t mode);
bool sstv_video_get(sstv_chan_t *e, int Skip, bool Redraw);
void sstv_video_done(sstv_chan_t *e);

SSTV_REAL sstv_sync_find(sstv_chan_t *e, int *Skip);

void sstv_get_fsk(sstv_chan_t *e, char *dest);

SSTV_REAL deg2rad(SSTV_REAL Deg);
u1_t clip(SSTV_REAL a);
