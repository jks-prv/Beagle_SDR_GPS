// Copyright (c) 2017 Peter Jennings, VE3SUN

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

typedef struct {
	int rx_chan;
} ibp_scan_t;

static ibp_scan_t ibp_scan[MAX_RX_CHANS];

bool ibp_scan_msgs(char *msg, int rx_chan)
{
	ibp_scan_t *e = &ibp_scan[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	//printf("### ibp_scan_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, false, "EXT ready");
		return true;
	}

	return false;
}

void IBP_scan_main();

ext_t ibp_scan_ext = {
	"IBP_scan",
	IBP_scan_main,
	NULL,
	ibp_scan_msgs,
};

void IBP_scan_main()
{
	ext_register(&ibp_scan_ext);
}
