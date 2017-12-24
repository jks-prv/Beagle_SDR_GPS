// Copyright (c) 2017 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..RX_CHAN
// We need this so the extension can support multiple users, each with their own lms[] data structure.

struct lms_t {
	int rx_chan;
	int run;
};

static lms_t lms[RX_CHANS];

bool lms_msgs(char *msg, int rx_chan)
{
	lms_t *e = &lms[rx_chan];
	int n;
	
	//printf("### lms_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
		return true;
	}
	
	return false;
}

void lms_filter_main();

ext_t lms_ext = {
	"LMS_filter",
	lms_filter_main,
	NULL,
	lms_msgs,
};

void lms_filter_main()
{
	ext_register(&lms_ext);
}
