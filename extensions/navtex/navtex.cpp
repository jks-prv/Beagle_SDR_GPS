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
// We need this so the extension can support multiple users, each with their own navtex[] data structure.

typedef struct {
	int rx_chan;
	int run;
} navtex_t;

static navtex_t navtex[MAX_RX_CHANS];

bool navtex_msgs(char *msg, int rx_chan)
{
	navtex_t *e = &navtex[rx_chan];
	int n;
	
	//printf("### navtex_msgs RX%d <%s>\n", rx_chan, msg);
	
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

void navtex_main();

ext_t navtex_ext = {
	"navtex",
	navtex_main,
	NULL,
	navtex_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void navtex_main()
{
	ext_register(&navtex_ext);
}
