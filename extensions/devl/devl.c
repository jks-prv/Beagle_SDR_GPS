// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..RX_CHAN
// We need this so the extension can support multiple users, each with their own devl[] data structure.

struct devl_t {
	int rx_chan;
	int run;
};

static devl_t devl[RX_CHANS];

bool devl_msgs(char *msg, int rx_chan)
{
	devl_t *e = &devl[rx_chan];
	int n;
	
	//printf("### devl_msgs RX%d <%s>\n", rx_chan, msg);
	
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

void devl_main();

ext_t devl_ext = {
	"devl",
	devl_main,
	NULL,
	devl_msgs,
};

void devl_main()
{
	ext_register(&devl_ext);
}
