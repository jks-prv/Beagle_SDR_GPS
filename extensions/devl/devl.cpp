// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "spi.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own devl[] data structure.

typedef struct {
	int rx_chan;
	int run;
} devl_t;

static devl_t devl[MAX_RX_CHANS];

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
	
	int in_n;
	double in_f;
	n = sscanf(msg, "SET devl.in%d=%lf", &in_n, &in_f);
	if (n == 2) {
	    //printf("DEVL: in%d=%lf\n", in_n, in_f);
	    
        if (in_n == 1) {
	        p0 = (int) lrint(in_f);
        }
	    
	    if (in_n == 2) {
	        p1 = (int) lrint(in_f);
        }
	    
	    if (in_n == 3) {
	        p2 = (int) lrint(in_f);
        }
	    
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
