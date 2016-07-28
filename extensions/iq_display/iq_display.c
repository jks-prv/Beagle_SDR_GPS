// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#ifndef EXT_IQ_DISPLAY
	void iq_display_main() {}
#else

#include "kiwi.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define IQ_DISPLAY_DEBUG_MSG	true
#define IQ_DISPLAY_DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..RX_CHAN
// We need this so the extension can support multiple users, each with their own iq_display[] data structure.

struct iq_display_t {
	int rx_chan;
	int run;

	float gain, max;
	int iq_ring;
	#define N_IQ_RING 32
	float iq[N_IQ_RING][IQ];
};

static iq_display_t iq_display[RX_CHANS];

static void draw(float iq[2], int color)
{

}

void iq_display_data(int rx_chan, int nsamps, TYPECPX *samps)
{
	iq_display_t *e = &iq_display[rx_chan];
	int i;

    for (i=0; i<nsamps; i++) {
		float re = (float) samps[i].re;
		float im = (float) samps[i].im;
		
		draw(e->iq[e->iq_ring], 0);
		e->iq[e->iq_ring][I] = re;
		e->iq[e->iq_ring][Q] = im;
		draw(e->iq[e->iq_ring], 1);
		e->iq_ring++;
		if (e->iq_ring > N_IQ_RING) e->iq_ring = 0;
    }
}

bool iq_display_msgs(char *msg, int rx_chan)
{
	iq_display_t *e = &iq_display[rx_chan];
	int n;
	
	printf("### iq_display_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, IQ_DISPLAY_DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
		if (e->run)
			ext_register_receive_iq_samps(iq_display_data, rx_chan);
		else
			ext_unregister_receive_iq_samps(rx_chan);
		return true;
	}
	
	return false;
}

void iq_display_close(int rx_chan)
{

}

void iq_display_main();

ext_t iq_display_ext = {
	"iq_display",
	iq_display_main,
	iq_display_close,
	iq_display_msgs,
};

void iq_display_main()
{
    double frate = ext_get_sample_rateHz();
    printf("iq_display_main audio sample rate = %.1f\n", frate);

	ext_register(&iq_display_ext);
}

#endif
