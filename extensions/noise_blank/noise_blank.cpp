// Copyright (c) 2017 John Seamons, ZL/KF6VO

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
// We need this so the extension can support multiple users, each with their own noise[] data structure.

typedef struct {
	int rx_chan;
	int run;
} noise_t;

static noise_t noise[MAX_RX_CHANS];

bool noise_blank_msgs(char *msg, int rx_chan)
{
	noise_t *e = &noise[rx_chan];
	int n;
	
	//printf("### noise_msgs RX%d <%s>\n", rx_chan, msg);
	
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

void noise_blank_main();

ext_t noise_blank_ext = {
	"noise_blank",
	noise_blank_main,
	NULL,
	noise_blank_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY     // FIXME: needs to also indicate this when selected from audio tab
};

void noise_blank_main()
{
	ext_register(&noise_blank_ext);
}
