// Copyright (c) 2016 John Seamons, ZL4VO/KF6VO

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
// We need this so the extension can support multiple users, each with their own S_meter[] data structure.

typedef struct {
	int rx_chan;
	int run;
} S_meter_t;

static S_meter_t S_meter[MAX_RX_CHANS];

void S_meter_data(int rx_chan, float S_meter_dBm)
{
	S_meter_t *e = &S_meter[rx_chan];

	ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT smeter=%.1f", S_meter_dBm);
}

bool S_meter_msgs(char *msg, int rx_chan)
{
	S_meter_t *e = &S_meter[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	//printf("### S_meter_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
		if (e->run) {
			ext_register_receive_S_meter(S_meter_data, rx_chan);
		} else {
			ext_unregister_receive_S_meter(rx_chan);
		}
		return true;
	}
	
	return false;
}

void S_meter_main();

ext_t S_meter_ext = {
	"S_meter",
	S_meter_main,
	NULL,
	S_meter_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void S_meter_main()
{
	ext_register(&S_meter_ext);
}
