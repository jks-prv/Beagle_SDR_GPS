// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own gen[] data structure.

typedef struct {
	int rx_chan;
	int run;
} gen_t;

static gen_t gen[MAX_RX_CHANS];

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
