
#pragma once

#include "CHFDLResample.h"

#include "types.h"
#include "kiwi.h"
#include "config.h"
#include "kiwi_assert.h"
#include "mem.h"
#include "coroutines.h"
#include "data_pump.h"
#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()
#include "web.h"

#include <sys/mman.h>

// use of MIN_SND_RATE here gives the largest possible buffer needed
#define HFDL_OUTBUF_SIZE    FASTFIR_OUTBUF_SIZE
#define HFDL_N_SAMPS        ((((HFDL_OUTBUF_SIZE * HFDL_MIN_SRATE) / MIN_SND_RATE) + 1) * NIQ)

typedef struct {
	u4_t nom_rate;
    s2_t *s2p_start, *s2p_end;
    int tsamps;    
} hfdl_t;

typedef struct {
	int rx_chan;
	int run;
	bool reset, test;
	tid_t tid, input_tid, dumphfdl_tid;
	int rd_pos;
	bool seq_init;
	u4_t seq;
	
	CHFDLResample HFDLResample;
    HFDL_resamp_t resamp; 

	double freq;
    double tuned_f;
	
    TYPECPX *samps_c;
    #define N_SAMP_BUF 1024
    s2_t s2in[N_SAMP_BUF];
    TYPECPX fout[N_SAMP_BUF];
    int outputBlockSize;
    
    s2_t *s2p, *s22p, *s2px;
    int nsamps;
    double test_f;
} hfdl_chan_t;

// server => dumphfdl
C_LINKAGE(void dumphfdl_init());
C_LINKAGE(int dumphfdl_main(int argc, char **argv, int rx_chan, int outputBlockSize));
C_LINKAGE(void dumphfdl_set_freq(int rx_chan, double freq_kHz));

// dumphfdl => server
C_LINKAGE(bool hfdl_msgs(char *msg, int rx_chan));
