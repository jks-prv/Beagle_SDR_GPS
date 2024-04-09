// Copyright (c) 2020 Kari Karvonen, OH1KK

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "cfg.h"
#include "str.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>

#define IFRAME_DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own iframe[] data structure.

struct iframe_t {
	u1_t rx_chan;
};

static iframe_t iframe[MAX_RX_CHANS];

bool iframe_msgs(char *msg, int rx_chan)
{
	iframe_t *e = &iframe[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number

	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, IFRAME_DEBUG_MSG, "EXT ready");
		return true;
	}
    
    return false;
}

void iframe_close(int rx_chan)
{
    // do nothing
}

void iframe_main();

ext_t iframe_ext = {
	"iframe",
	iframe_main,
	iframe_close,
	iframe_msgs,
};

void iframe_main()
{
	ext_register(&iframe_ext);
}
