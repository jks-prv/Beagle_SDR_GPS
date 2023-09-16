// Copyright (c) 2021 John Seamons, ZL4VO/KF6VO

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
// We need this so the extension can support multiple users, each with their own waterfall[] data structure.

typedef struct {
	int rx_chan;
	int run;
} waterfall_t;

static waterfall_t waterfall[MAX_RX_CHANS];

bool waterfall_msgs(char *msg, int rx_chan)
{
	waterfall_t *e = &waterfall[rx_chan];
	int n;
	
	//printf("### waterfall_msgs RX%d <%s>\n", rx_chan, msg);
	
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

void waterfall_main();

ext_t waterfall_ext = {
	"waterfall",
	waterfall_main,
	NULL,
	waterfall_msgs,
};

void waterfall_main()
{
	ext_register(&waterfall_ext);
}
