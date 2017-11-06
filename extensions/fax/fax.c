// Copyright (c) 2017 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "coroutines.h"
#include "data_pump.h"
#include "str.h"
#include "debug.h"
#include "misc.h"
#include "non_block.h"
#include "FaxDecoder.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

struct fax_t {
	int rx_chan;
	bool capture, task_created;
	tid_t tid;
	int rd_pos, wrL;
	u4_t timeL;
	bool seq_init;
	u4_t seq;
	float shift;
};

static fax_t fax[RX_CHANS];

//static void fax_data(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps)
void fax_task(void *param)
{
	//printf("fax_task running..\n");

	while (1) {
		
		//printf("fax_task sleep..\n");
		int rx_chan = (int) FROM_VOID_PARAM(TaskSleepReason("wait for wakeup"));

	    fax_t *e = &fax[rx_chan];
        rx_dpump_t *rx = &rx_dpump[rx_chan];
        
        #if 0
            //printf("fax_task wr_pos=%d\n", rx->real_wr_pos);
            int wr = rx->real_wr_pos;
            int nbuf;
            if (wr < e->wrL)
                nbuf = N_DPBUF - e->wrL + wr;
            else
                nbuf = wr - e->wrL;
            e->wrL = wr;
            u4_t time = timer_us();
            real_printf("%d %.3f ", nbuf, (float)(time - e->timeL)/1e3); fflush(stdout);
            e->timeL = time;
        #endif
		
		/*
        if (!e->seq_init) {
            real_printf("wr %d: ", rx->real_wr_pos);
            for (int i=0; i<N_DPBUF; i++)
                real_printf("%d|%d ", i, rx->real_seqnum[i]);
            real_printf("\n");
        }
        */

		while (e->rd_pos != rx->real_wr_pos) {
		    if (rx->real_seqnum[e->rd_pos] != e->seq) {
                if (!e->seq_init) {
                    e->seq_init = true;
                } else {
                    u4_t got = rx->real_seqnum[e->rd_pos], expecting = e->seq;
                    printf("FAX rx%d SEQ: @%d got %d expecting %d (%d)\n", rx_chan, e->rd_pos, got, expecting, got - expecting);
                    if (p0 && ev_dump) evLatency(EC_DUMP, EV_EXT, ev_dump, "EXT", evprintf("DUMP in %.3f sec", ev_dump/1000.0));
                }
                e->seq = rx->real_seqnum[e->rd_pos];
            }
            e->seq++;
		    
		    m_FaxDecoder[rx_chan].ProcessSamples(&rx->real_samples[e->rd_pos][0], FASTFIR_OUTBUF_SIZE, e->shift);
		    e->shift = 0;
			e->rd_pos = (e->rd_pos+1) & (N_DPBUF-1);
		}
	}
}

void fax_close(int rx_chan)
{
	fax_t *e = &fax[rx_chan];
	if (e->task_created) {
		TaskRemove(e->tid);
		e->task_created = false;
	}
	memset(e, 0, sizeof(*e));
	printf("FAX rx%d close\n", rx_chan);
}

bool fax_msgs(char *msg, int rx_chan)
{
	fax_t *e = &fax[rx_chan];
	int n;
	
	printf("FAX rx%d msg <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		
		ext_send_msg(rx_chan, false, "EXT ready=%d", rx_chan);
		return true;
	}

	if (strcmp(msg, "SET fax_start") == 0) {
		printf("FAX rx%d configure/start\n", rx_chan);

        m_FaxDecoder[rx_chan].Configure(
            rx_chan,
            1024,       // int imagewidth
            8,          // int BitsPerPixel
            1900,       // int carrier
            400,        // int deviation, black/white freq +/- deviation from carrier
            FaxDecoder::firfilter::MIDDLE,     // bandwidth
            15.0,       // double minus_saturation_threshold
            //false,      // bool bSkipHeaderDetection
            true,      // bool bSkipHeaderDetection
            true,       // bool bIncludeHeadersInImages
            true        // bool reset
        );
        
        if (!e->task_created) {
			e->tid = CreateTaskF(fax_task, TO_VOID_PARAM(rx_chan), EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL), 0);
            e->task_created = true;
        }
		
		e->capture = true;
        e->seq_init = false;
		ext_register_receive_real_samps_task(e->tid, rx_chan);
		//ext_register_receive_real_samps(fax_data, rx_chan);
		return true;
	}

	float shift;
	n = sscanf(msg, "SET fax_shift=%f", &shift);
	if (n == 1) {
	    e->shift = shift;
		printf("FAX rx%d shift=%f\n", rx_chan, shift);
		return true;
	}
	
	if (strcmp(msg, "SET fax_stop") == 0) {
		printf("FAX rx%d stop\n", rx_chan);
		e->capture = false;
		ext_unregister_receive_real_samps_task(rx_chan);
		//ext_unregister_receive_real_samps(rx_chan);
		return true;
	}

	if (strcmp(msg, "SET fax_file_open") == 0) {
		printf("FAX fax_file_open\n");
		m_FaxDecoder[rx_chan].FileOpen();
		return true;
	}

	if (strcmp(msg, "SET fax_file_close") == 0) {
		printf("FAX fax_file_close\n");
		m_FaxDecoder[rx_chan].FileClose();
		char *cmd, *reply;
		
		asprintf(&cmd, "cd /root/kiwi.config; pnmtopng fax.ch%d.pgm > fax.ch%d.png; "
		    "pnmscale fax.ch%d.pgm -width=96 -height=32 > fax.ch%d.thumb.pgm; "
		    "pnmtopng fax.ch%d.thumb.pgm > fax.ch%d.thumb.png",
		    rx_chan, rx_chan, rx_chan, rx_chan, rx_chan, rx_chan);
		
		reply = non_blocking_cmd(cmd, NULL);
		free(cmd);
		kstr_free(reply);
        ext_send_msg(rx_chan, false, "EXT fax_download_avail");
		return true;
	}

	return false;
}

void fax_main();

ext_t fax_ext = {
	"fax",
	fax_main,
	fax_close,
	fax_msgs,
};

void fax_main()
{
	ext_register(&fax_ext);
}
