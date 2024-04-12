// Copyright (c) 2017 John Seamons, ZL4VO/KF6VO

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
// We need this so the extension can support multiple users, each with their own colormap[] data structure.

typedef struct {
	int rx_chan;
	int run;
} colormap_t;

static colormap_t colormap[MAX_RX_CHANS];

bool colormap_msgs(char *msg, int rx_chan)
{
	colormap_t *e = &colormap[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	//printf("### colormap_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
		return true;
	}
	
	return false;
}

void colormap_main();

ext_t colormap_ext = {
	"colormap",
	colormap_main,
	NULL,
	colormap_msgs,
};

void colormap_main()
{
	ext_register(&colormap_ext);
}
