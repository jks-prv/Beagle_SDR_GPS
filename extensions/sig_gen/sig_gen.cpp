// Copyright (c) 2016 John Seamons, ZL4VO/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "data_pump.h"
#include "CuteSDR/fastfir.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

#include "options.h"
#include "rx_sound.h"
#include "fpga.h"

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own gen[] data structure.

typedef struct {
	int rx_chan;
	#define GEN_RF_TONE     0x01
	#define GEN_AF_NOISE    0x02
	#define GEN_SELF_TEST   0x04
	int run;
	int attn;

    #define CICH_NTAPS 17
    s4_t cich_buf[CICH_NTAPS][NIQ];     // 24-bits
} gen_t;

static gen_t gen[MAX_RX_CHANS];

extern CFastFIR m_PassbandFIR[MAX_RX_CHANS];

void gen_inject(int rx_chan, int instance, int ns_out, TYPECPX *samps)
{
	gen_t *e = &gen[rx_chan];

    for (int n = 0; n < ns_out; n++) {
        if (e->run & GEN_AF_NOISE) {
            //samps[n].re = (drand48() - 0.5f) * e->attn / 32768;
            //samps[n].im = (drand48() - 0.5f) * e->attn / 32768;
            //#define SIG_GEN_AF_NOISE_GAIN_ADJ
            #ifdef SIG_GEN_AF_NOISE_GAIN_ADJ
                #define P0_F p_f[0]
            #else
                #define P0_F 0
            #endif
            samps[n].re = (drand48() - 0.5f) * (P0_F? P0_F : ((float) e->attn / 32768.0f));
            samps[n].im = (drand48() - 0.5f) * (P0_F? P0_F : ((float) e->attn / 32768.0f));
        }
    }
}

bool gen_msgs(char *msg, int rx_chan)
{
	gen_t *e = &gen[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	//printf("### gen_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
        printf("sig_gen run=0x%x%s%s%s\n", e->run,
            (e->run & GEN_RF_TONE)? " RF_TONE" : "",
            (e->run & GEN_AF_NOISE)? " AF_NOISE" : "",
            (e->run & GEN_SELF_TEST)? " SELF_TEST" : "");

        if (e->run & GEN_AF_NOISE)
            ext_register_receive_iq_samps_raw(gen_inject, rx_chan);
        else
            ext_unregister_receive_iq_samps_raw(rx_chan);

		return true;
	}
	
	n = sscanf(msg, "SET attn=%d", &e->attn);
	if (n == 1) {
        printf("sig_gen attn=%d (0x%x)\n", e->attn, e->attn);
	    if (e->attn < 2) e->attn = 2;       // prevent FF silence problem
		return true;
	}

	#if 0
        int gen_fix;
        n = sscanf(msg, "SET gen_fix=%d", &gen_fix);
        if (n == 1) {
            printf("gen_fix=%d\n", gen_fix);
            ctrl_clr_set(CTRL_GEN_FIX, 0);
            if (gen_fix == 1) ctrl_clr_set(0, CTRL_GEN_FIX);
            return true;
        }
        
        int rx_fix;
        n = sscanf(msg, "SET rx_fix=%d", &rx_fix);
        if (n == 1) {
            printf("rx_fix=%d\n", rx_fix);
            ctrl_clr_set(CTRL_RX_FIX, 0);
            if (rx_fix == 1) ctrl_clr_set(0, CTRL_RX_FIX);
            return true;
        }
        
        int wf_fix;
        n = sscanf(msg, "SET wf_fix=%d", &wf_fix);
        if (n == 1) {
            printf("wf_fix=%d\n", wf_fix);
            ctrl_clr_set(CTRL_WF_FIX, 0);
            if (wf_fix == 1) ctrl_clr_set(0, CTRL_WF_FIX);
            return true;
        }
        
        int gen_zero;
        n = sscanf(msg, "SET gen_zero=%d", &gen_zero);
        if (n == 1) {
            printf("gen_zero=%d\n", gen_zero);
            ctrl_clr_set(CTRL_GEN_ZERO, 0);
            if (gen_zero == 1) ctrl_clr_set(0, CTRL_GEN_ZERO);
            return true;
        }
	#endif
	
	return false;
}

void sig_gen_main();

ext_t gen_ext = {
	"sig_gen",
	sig_gen_main,
	NULL,
	gen_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void sig_gen_main()
{
	ext_register(&gen_ext);
}
