// Copyright (c) 2021 John Seamons, ZL4VO/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()
#include "prefs.h"

#include "kiwi.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define PREFS_DEBUG_MSG	true
#define PREFS_DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own prefs[] data structure.

typedef struct {
	int rx_chan;
} prefs_t;

static prefs_t prefs[MAX_RX_CHANS];

bool prefs_msgs(char *msg, int rx_chan)
{
	prefs_t *e = &prefs[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	printf("### prefs_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, PREFS_DEBUG_MSG, "EXT ready");
		return true;
	}
	
	return false;
}

void prefs_close(int rx_chan)
{

}

void prefs_main();

ext_t prefs_ext = {
	"prefs",
	prefs_main,
	prefs_close,
	prefs_msgs,
};

void prefs_main()
{
	ext_register(&prefs_ext);
}
