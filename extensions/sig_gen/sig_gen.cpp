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
	#define GEN_CICF_SW     0x08
	#define GEN_CICF_HW     0x10
	#define GEN_CICF_FW     0x20
	int run;
	int attn;

    #define CICH_NTAPS 17
    s4_t cich_buf[CICH_NTAPS][NIQ];     // 24-bits
} gen_t;

static gen_t gen[MAX_RX_CHANS];

extern CFastFIR m_PassbandFIR[MAX_RX_CHANS];

#define CICH_MAG 0x1ffff
#define CICH_18b 0x3ffff
#define CICH_SCALE(f) ((s4_t) round((f) * CICH_MAG))    // 18-bits
const s4_t cich_taps[CICH_NTAPS] = {

        // pyFDA: Kaiser beta=8.95926(computed), Fspecs=0.325(-6 dB Fc point), 90 dB, Order: N=17(gives 17 taps, bug?)
        // Fspecs=0.325 computed from equiripple specifying 0.15(1800)/0.5(6000)
        CICH_SCALE(-2.214650866071277e-05),
        CICH_SCALE( 0.0006470335687791359),
        CICH_SCALE(-0.000984717912504363),
        CICH_SCALE(-0.007159332755111238),
        CICH_SCALE( 0.024558851149450073),
        CICH_SCALE(-0.009030350235043115),
        CICH_SCALE(-0.09846207467656441),
        CICH_SCALE( 0.2654953703945822),
        CICH_SCALE( 0.649914733950145),
        CICH_SCALE( 0.2654953703945822),
        CICH_SCALE(-0.09846207467656441),
        CICH_SCALE(-0.009030350235043115),
        CICH_SCALE( 0.024558851149450073),
        CICH_SCALE(-0.007159332755111238),
        CICH_SCALE(-0.000984717912504363),
        CICH_SCALE( 0.0006470335687791359),
        CICH_SCALE(-2.214650866071277e-05),
};

void gen_inject(int rx_chan, int instance, int ns_out, TYPECPX *samps)
{
	gen_t *e = &gen[rx_chan];

    static bool cich_dumped;
    if (!cich_dumped) {
        for (int t = 0; t < CICH_NTAPS; t++) {
            u4_t tap = cich_taps[t] & CICH_18b;
            real_printf("cich_taps %02d: %6d 0x%05x\n", t, tap, tap);
        }
        cich_dumped = true;
    }

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

        // simulate the FPGA fixed-point CFIR
        if (e->run & GEN_CICF_HW) {
            s64_t acc[NIQ];
            #define tap cich_taps
            #define buf e->cich_buf
        
            acc[I] = tap[0] * buf[0][I];
            acc[Q] = tap[0] * buf[0][Q];
            for (int t = 1; t < CICH_NTAPS; t++) {
                acc[I] += tap[t] * buf[t][I];
                acc[Q] += tap[t] * buf[t][Q];
                #if 0
                    // 42-bits, mask = (1<<42[=24+18])-1 = 0x3ff_ffff_ffff
                    #define CICH_PROD_WIDTH 0x3ffffffffffLL
                    acc[I] &= CICH_PROD_WIDTH;
                    //if (acc[I] & 0x20000000000LL) acc[I] = -acc[I];
                    acc[Q] &= CICH_PROD_WIDTH;
                    //if (acc[Q] & 0x20000000000LL) acc[Q] = -acc[Q];
                #endif
            }
            for (int t = CICH_NTAPS-1; t > 0; t--) {
                buf[t][I] = buf[t-1][I];
                buf[t][Q] = buf[t-1][Q];
            }
            
            const TYPEREAL rescale_up = MPOW(2, RXOUT_SCALE - CUTESDR_SCALE);   // +/- 32768.0 => s24
            #define TYPEREAL_S24(f) ((s4_t) ((f) * rescale_up))
            buf[0][I] = TYPEREAL_S24(samps[n].re);
            buf[0][Q] = TYPEREAL_S24(samps[n].im);

            // 64 = 22|24|18
            const TYPEREAL rescale_down = MPOW(2, -RXOUT_SCALE + CUTESDR_SCALE);    // s24 => +/- 32768.0
            #define S24_TYPEREAL(s24) ((TYPEREAL) (s24) * rescale_down)
            samps[n].re = S24_TYPEREAL(acc[I] >> 18);
            samps[n].im = S24_TYPEREAL(acc[Q] >> 18);
            #undef tap
            #undef buf
        }
    }
}

bool gen_msgs(char *msg, int rx_chan)
{
	gen_t *e = &gen[rx_chan];
	int n;
	
	//printf("### gen_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
        printf("sig_gen run=0x%x%s%s%s%s%s%s CICF=%d\n", e->run,
            (e->run & GEN_RF_TONE)? " RF_TONE" : "",
            (e->run & GEN_AF_NOISE)? " AF_NOISE" : "",
            (e->run & GEN_SELF_TEST)? " SELF_TEST" : "",
            (e->run & GEN_CICF_SW)? " CICF_SW" : "",
            (e->run & GEN_CICF_HW)? " CICF_HW" : "",
            (e->run & GEN_CICF_FW)? " CICF_FW" : "",
            (e->run & GEN_AF_NOISE)? 0:1);

        if (e->run & (GEN_AF_NOISE | GEN_CICF_HW))
            ext_register_receive_iq_samps_raw(gen_inject, rx_chan);
        else
            ext_unregister_receive_iq_samps_raw(rx_chan);

        m_PassbandFIR[rx_chan].SetupCICFilter((e->run & GEN_AF_NOISE)? false : true);

        snd_t *snd = &snd_inst[rx_chan];
        if (e->run & GEN_CICF_SW) {
            snd->cicf_setup = true;
        } else {
            snd->cicf_setup = snd->cicf_run = false;
        }

        ctrl_clr_set(CTRL_GEN_FIR, 0);
        if (e->run & GEN_CICF_FW) ctrl_clr_set(0, CTRL_GEN_FIR);

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
