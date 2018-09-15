// Copyright (c) 2018 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "uhsdr_cw_decoder.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own cw_decoder[] data structure.

typedef struct {
	int rx_chan;
	int run;
} cw_decoder_t;

static cw_decoder_t cw_decoder[MAX_RX_CHANS];

bool cw_decoder_msgs(char *msg, int rx_chan)
{
	cw_decoder_t *e = &cw_decoder[rx_chan];
	int n;
	
	//printf("### cw_decoder_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	if (strcmp(msg, "SET cw_start") == 0) {
		//printf("CW rx%d start\n", rx_chan);
		CwDecode_Init(rx_chan);
		ext_register_receive_real_samps(CwDecode_RxProcessor, rx_chan);
		return true;
	}
	
	if (strcmp(msg, "SET cw_stop") == 0) {
		//printf("CW rx%d stop\n", rx_chan);
		ext_unregister_receive_real_samps(rx_chan);
		return true;
	}
	
	u4_t pboff;
	if (sscanf(msg, "SET cw_pboff=%d", &pboff) == 1) {
		//printf("CW rx%d pboff %d\n", rx_chan, pboff);
		CwDecode_pboff(rx_chan, pboff);
		return true;
	}
	
	return false;
}

void cw_decoder_main();

ext_t cw_decoder_ext = {
	"cw_decoder",
	cw_decoder_main,
	NULL,
	cw_decoder_msgs
};

void cw_decoder_main()
{
	ext_register(&cw_decoder_ext);
}
