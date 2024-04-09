// Copyright (c) 2017 John Seamons, ZL4VO/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "net.h"
#include "gps.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own tdoa[] data structure.

#define TDOA_MAX_HOSTS  6

typedef struct {
	int rx_chan;
	int run;
} tdoa_t;

static tdoa_t tdoa[MAX_RX_CHANS];

bool tdoa_msgs(char *msg, int rx_chan)
{
	tdoa_t *e = &tdoa[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int i, n;
    char *cmd_p;
	
	//printf("### tdoa_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
        ext_send_msg_encoded(rx_chan, DEBUG_MSG, "EXT", "ready", "<bvwk>7`g3g7e1be37a03;aa16;322f362:2:`,%s", gps.a);
		return true;
	}
	
	return false;
}

void TDoA_main();

ext_t tdoa_ext = {
	"TDoA",
	TDoA_main,
	NULL,
	tdoa_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void TDoA_main()
{
	ext_register(&tdoa_ext);
}
