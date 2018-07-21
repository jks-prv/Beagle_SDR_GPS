// Copyright (c) 2017 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "net.h"
#include "non_block.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..RX_CHAN
// We need this so the extension can support multiple users, each with their own tdoa[] data structure.

#define TDOA_MAX_HOSTS  6

typedef struct {
	int rx_chan;
	int run;
} tdoa_t;

static tdoa_t tdoa[RX_CHANS];

// fixme remove
static int tdoa_func(void *param)
{
	nbcmd_args_t *args = (nbcmd_args_t *) param;
	int rx_chan = args->func_param;
	char *sp = kstr_sp(args->kstr);

	u4_t val;
	double lat, lon;
	if (sscanf(sp, "status0=%d", &val) == 1) {
	    shmem->status_u4[0][rx_chan] = val;
	    printf("TDoA: tdoa_func rx%d status0=%d\n", rx_chan, val);
	} else
	if (sscanf(sp, "status1=%d", &val) == 1) {
	    shmem->status_u4[1][rx_chan] = val;
	    printf("TDoA: tdoa_func rx%d status1=%d\n", rx_chan, val);
	} else
	if (sscanf(sp, "key=%d", &val) == 1) {
	    shmem->status_u4[2][rx_chan] = val;
	    printf("TDoA: tdoa_func rx%d key=%d\n", rx_chan, val);
	} else
	if (sscanf(sp, "likely=%lf,%lf", &lat, &lon) == 2) {
	    shmem->status_f[0][rx_chan] = lat;
	    shmem->status_f[1][rx_chan] = lon;
	    printf("TDoA: tdoa_func rx%d lat=%.2f lon=%.2f\n", rx_chan, lat, lon);
	} else
	    printf("TDoA: tdoa_func rx%d <%s>\n", sp);

	return 0;
}

bool tdoa_msgs(char *msg, int rx_chan)
{
	tdoa_t *e = &tdoa[rx_chan];
	int i, n;
    char *cmd_p;
	
	//printf("### tdoa_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT ready=%s", "215badfdfd49ea176d9c4c50c1daac0a");
		return true;
	}
	
	// fixme remove
	char *arg = NULL;
	n = sscanf(msg, "SET sample %1024ms", &arg);
	if (n == 1) {
	    printf("TDoA: sample <%s>\n", arg);
        asprintf(&cmd_p, "curl --silent --show-error --ipv4 --connect-timeout 15 "
            "\"http://%s/php/tdoa.php?auth=4cd0d4f2af04b308bb258011e051919c"
            "%s\" 2>&1",
            ddns.ips_kiwisdr_com.backup? ddns.ips_kiwisdr_com.ip_list[0] : "kiwisdr.com",
            kiwi_str_decode_inplace(arg));
	    printf("TDoA: sample <%s>\n", cmd_p);

        shmem->status_u4[0][e->rx_chan] = -1;
        shmem->status_u4[1][e->rx_chan] = -1;
        non_blocking_cmd_func_foreach("kiwi.tda", cmd_p, tdoa_func, e->rx_chan, NO_WAIT);

        do {
            TaskSleepMsec(250);
        } while (shmem->status_u4[0][e->rx_chan] == -1);
        u4_t status0 = shmem->status_u4[0][e->rx_chan];
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT sample_status=%d", status0);
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT key=%05d", shmem->status_u4[2][e->rx_chan]);
		
        do {
            TaskSleepMsec(250);
        } while (shmem->status_u4[1][e->rx_chan] == -1);
        u4_t status1 = shmem->status_u4[1][e->rx_chan];
        double lat = shmem->status_f[0][e->rx_chan];
        double lon = shmem->status_f[1][e->rx_chan];
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT lat=%.2f lon=%.2f submit_status=%d", lat, lon, status1);

        free(arg);
        free(cmd_p);
		return true;
	}
	
	return false;
}

void TDoA_main();

ext_t tdoa_ext = {
	"TDoA",
	TDoA_main,
	NULL,
	tdoa_msgs
};

void TDoA_main()
{
	ext_register(&tdoa_ext);
}
