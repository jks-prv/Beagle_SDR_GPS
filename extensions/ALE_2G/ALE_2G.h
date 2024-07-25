
#pragma once

// this is here before Kiwi includes to prevent our "#define printf ALT_PRINTF" mechanism being disturbed
#include "decode_ff_impl.h"
#undef PI

#include "types.h"
#include "kiwi.h"
#include "config.h"
#include "kiwi_assert.h"
#include "mem.h"
#include "coroutines.h"
#include "data_pump.h"
#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()
#include "web.h"
#include "rx_util.h"

#include <sys/mman.h>

#define ALE_2G_TEST_FILE
#define ALE_2G_TEST_FILE_RATE 12000

typedef enum { FILE_12k = 0, FILE_8k = 1 } test_fn_sel_e;

typedef struct {
    test_fn_sel_e test_fn_sel;
    
	#ifdef ALE_2G_TEST_FILE
        s2_t *s2p_start, *s2p_end;
        u4_t file_off;
        int tsamps;
    #endif
    
} ale_2g_t;

typedef struct {
	int rx_chan;
	int run;
	int dsp;
	bool reset, test;
	bool task_created;
	tid_t tid;
	int rd_pos;
	bool seq_init;
	u4_t seq;
	
	ale::decode_ff_impl decode;
	double freq;
    double tuned_f;
	
	bool use_new_resampler;
    int ni;
    s2_t *inp;
    s2_t s2in[N_SAMP_BUF];
    float fout[N_SAMP_BUF];
    
	#ifdef ALE_2G_TEST_FILE
        s2_t *s2p, *s22p, *s2px;
        int nsamps;
        double test_f;
    #endif
    
} ale_2g_chan_t;
