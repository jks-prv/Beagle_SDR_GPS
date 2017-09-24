// Copyright (c) 2017 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

struct timecode_t {
	int rx_chan;
};

static timecode_t timecode[RX_CHANS];

bool timecode_msgs(char *msg, int rx_chan)
{
	timecode_t *e = &timecode[rx_chan];
	int n;
	
	printf("### timecode_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, false, "EXT ready");
		return true;
	}

	return false;
}

void timecode_main();

ext_t timecode_ext = {
	"timecode",
	timecode_main,
	NULL,
	timecode_msgs,
	{ "wwvb.js", "tdf.js", NULL }
};

void timecode_main()
{
	ext_register(&timecode_ext);
}
