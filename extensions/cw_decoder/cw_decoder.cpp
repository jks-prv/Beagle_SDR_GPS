// Copyright (c) 2018 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "coroutines.h"
#include "data_pump.h"
#include "uhsdr_cw_decoder.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own cw_decoder[] data structure.

typedef struct {
	int rx_chan;
	int run;
	bool task_created;
	tid_t tid;
	int rd_pos;
	bool seq_init;
	u4_t seq;
} cw_decoder_t;

static cw_decoder_t cw_decoder[MAX_RX_CHANS];

void cw_task(void *param)
{
	while (1) {
		
		int rx_chan = (int) FROM_VOID_PARAM(TaskSleepReason("wait for wakeup"));

	    cw_decoder_t *e = &cw_decoder[rx_chan];
        rx_dpump_t *rx = &rx_dpump[rx_chan];

		while (e->rd_pos != rx->real_wr_pos) {
		    if (rx->real_seqnum[e->rd_pos] != e->seq) {
                if (!e->seq_init) {
                    e->seq_init = true;
                } else {
                    u4_t got = rx->real_seqnum[e->rd_pos], expecting = e->seq;
                    printf("CW rx%d SEQ: @%d got %d expecting %d (%d)\n", rx_chan, e->rd_pos, got, expecting, got - expecting);
                }
                e->seq = rx->real_seqnum[e->rd_pos];
            }
            e->seq++;
		    
		    CwDecode_RxProcessor(rx_chan, 0, FASTFIR_OUTBUF_SIZE, &rx->real_samples[e->rd_pos][0]);
			e->rd_pos = (e->rd_pos+1) & (N_DPBUF-1);
		}
    }
}

void cw_close(int rx_chan)
{
	cw_decoder_t *e = &cw_decoder[rx_chan];
    printf("CW: close task_created=%d\n", e->task_created);
    ext_unregister_receive_real_samps_task(rx_chan);
    //ext_unregister_receive_real_samps(rx_chan);

	if (e->task_created) {
        printf("CW: TaskRemove\n");
		TaskRemove(e->tid);
		e->task_created = false;
	}
}

bool cw_decoder_msgs(char *msg, int rx_chan)
{
	cw_decoder_t *e = &cw_decoder[rx_chan];
	int n;
	
	//printf("### cw_decoder_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	if (strcmp(msg, "SET cw_start") == 0) {
		//printf("CW rx%d start\n", rx_chan);
		CwDecode_Init(rx_chan);

        if (!e->task_created) {
			e->tid = CreateTaskF(cw_task, TO_VOID_PARAM(rx_chan), EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL));
            e->task_created = true;
        }
		
        e->seq_init = false;
		ext_register_receive_real_samps_task(e->tid, rx_chan);
		//ext_register_receive_real_samps(CwDecode_RxProcessor, rx_chan);

		return true;
	}
	
	if (strcmp(msg, "SET cw_stop") == 0) {
		//printf("CW rx%d stop\n", rx_chan);
		cw_close(rx_chan);
		return true;
	}
	
	if (strcmp(msg, "SET cw_reset") == 0) {
		//printf("CW rx%d reset\n", rx_chan);
		CwDecode_Init(rx_chan);
		return true;
	}
	
	u4_t pboff;
	if (sscanf(msg, "SET cw_pboff=%d", &pboff) == 1) {
		//printf("CW rx%d pboff %d\n", rx_chan, pboff);
		CwDecode_pboff(rx_chan, pboff);
		return true;
	}
	
	int wsc;
	if (sscanf(msg, "SET cw_wsc=%d", &wsc) == 1) {
		//printf("CW rx%d wsc %d\n", rx_chan, wsc);
		CwDecode_wsc(rx_chan, wsc);
		return true;
	}
	
	int thresh;
	if (sscanf(msg, "SET cw_thresh=%d", &thresh) == 1) {
		//printf("CW rx%d thresh %d\n", rx_chan, thresh);
		CwDecode_thresh(rx_chan, 0, thresh);
		return true;
	}
	
	int threshold;
	if (sscanf(msg, "SET cw_threshold=%d", &threshold) == 1) {
		//printf("CW rx%d threshold %d\n", rx_chan, threshold);
		CwDecode_thresh(rx_chan, 1, threshold);
		return true;
	}
	
	return false;
}

void cw_decoder_main();

ext_t cw_decoder_ext = {
	"cw_decoder",
	cw_decoder_main,
	cw_close,
	cw_decoder_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void cw_decoder_main()
{
	ext_register(&cw_decoder_ext);
}
