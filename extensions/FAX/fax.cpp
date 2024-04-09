// Copyright (c) 2017 John Seamons, ZL4VO/KF6VO

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

static fax_t fax[MAX_RX_CHANS];
static u4_t serno[MAX_RX_CHANS];

//static void fax_data(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps)
void fax_task(void *param)
{
	//printf("fax_task running..\n");

	while (1) {
		
		//printf("fax_task sleep..\n");
		int rx_chan = (int) FROM_VOID_PARAM(TaskSleepReason("fax_task wakeup"));

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
            u4_t dt = time - e->timeL;
            if (1 || nbuf > 1)
                real_printf("%d %.3f ", nbuf, (float) dt / 1e3); fflush(stdout);
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
                    rcprintf(rx_chan, "FAX SEQ: @%d got %d expecting %d (%d)\n", e->rd_pos, got, expecting, got - expecting);
                    if (ev_dump) evFAX(EC_DUMP, EV_EXT, ev_dump, "FAX", evprintf("DUMP in %.3f sec", ev_dump/1000.0));
                }
                e->seq = rx->real_seqnum[e->rd_pos];
            }
            e->seq++;
		    
		    m_FaxDecoder[rx_chan].ProcessSamples(&rx->real_samples_s2[e->rd_pos][0], FASTFIR_OUTBUF_SIZE, e->shift);
            evFAX(EC_EVENT, EV_EXT, ev_dump, "FAX", evprintf("ProcessSamples "));
            NextTaskFast("fax_task");
		    e->shift = 0;
			e->rd_pos = (e->rd_pos+1) & (N_DPBUF-1);
		}
	}
}

void fax_close(int rx_chan)
{
	fax_t *e = &fax[rx_chan];
    e->capture = false;
    ext_unregister_receive_real_samps_task(rx_chan);
    //ext_unregister_receive_real_samps(rx_chan);

	if (e->task_created) {
		TaskRemove(e->tid);
		e->task_created = false;
	}
	memset(e, 0, sizeof(*e));
	rcprintf(rx_chan, "FAX close\n");
}

bool fax_msgs(char *msg, int rx_chan)
{
	fax_t *e = &fax[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	rcprintf(rx_chan, "FAX msg <%s>\n", msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		
		// remove old results for this channel on each start of the extension
        non_blocking_cmd_system_child("kiwi.fax", 
            stprintf("cd " DIR_DATA "; rm -f fax.ch%d_*", rx_chan), POLL_MSEC(500));

		ext_send_msg(rx_chan, false, "EXT ready=%d", rx_chan);
		return true;
	}

    int lpm, phasing, autostop, debug;
	if (sscanf(msg, "SET fax_start lpm=%d phasing=%d autostop=%d debug=%d", &lpm, &phasing, &autostop, &debug) == 4) {
		rcprintf(rx_chan, "FAX configure/start lpm=%d phasing=%d autostop=%d debug=%d\n", lpm, phasing, autostop, debug);

        m_FaxDecoder[rx_chan].Configure(
            rx_chan,
            lpm,
            1024,       // int imagewidth
            8,          // int BitsPerPixel
            1900,       // int carrier
            400,        // int deviation, black/white freq +/- deviation from carrier
            FaxDecoder::firfilter::MIDDLE,     // bandwidth
            15.0,       // double minus_saturation_threshold
            true,       // bool bIncludeHeadersInImages
            phasing,
            autostop,
            debug,
            true        // bool reset
        );
        
        if (!e->task_created) {
			e->tid = CreateTaskF(fax_task, TO_VOID_PARAM(rx_chan), EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL));
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
		rcprintf(rx_chan, "FAX shift=%f\n", shift);
		return true;
	}
	
	if (strcmp(msg, "SET fax_stop") == 0) {
		rcprintf(rx_chan, "FAX stop\n");
		fax_close(rx_chan);
		return true;
	}

	if (strcmp(msg, "SET fax_file_open") == 0) {
		rcprintf(rx_chan, "FAX fax_file_open\n");
		m_FaxDecoder[rx_chan].FileOpen();
		return true;
	}

	if (strcmp(msg, "SET fax_file_close") == 0) {
		rcprintf(rx_chan, "FAX fax_file_close\n");
		m_FaxDecoder[rx_chan].FileClose();
		
		u4_t sn = serno[rx_chan];
		#if 1
		    // Debian 11 doesn't have pnmtopng anymore, use .ppm intermediate format
		    // Also, have to use .gif now instead of .png (there is no p*topng)
            non_blocking_cmd_system_child("kiwi.fax", 
                stprintf("cd " DIR_DATA "; "
                    "pgmtoppm rgbi:1/1/1 fax.ch%d.pgm > fax.ch%d.ppm; "
                    "ppmtogif fax.ch%d.ppm > fax.ch%d_%d.gif; "
                    "pnmscale fax.ch%d.pgm -width=96 -height=32 > fax.ch%d.thumb.pgm; "
                    "pgmtoppm rgbi:1/1/1 fax.ch%d.thumb.pgm > fax.ch%d.thumb.ppm; "
                    "ppmtogif fax.ch%d.thumb.ppm > fax.ch%d_%d.thumb.gif",
                    rx_chan, rx_chan,
                    rx_chan, rx_chan, sn,
                    rx_chan, rx_chan,
                    rx_chan, rx_chan,
                    rx_chan, rx_chan, sn),
                POLL_MSEC(500));
		#else
            non_blocking_cmd_system_child("kiwi.fax", 
                stprintf("cd " DIR_DATA ";
                    "pnmtopng fax.ch%d.pgm > fax.ch%d_%d.png; "
                    "pnmscale fax.ch%d.pgm -width=96 -height=32 > fax.ch%d.thumb.pgm; "
                    "pnmtopng fax.ch%d.thumb.pgm > fax.ch%d_%d.thumb.png",
                    rx_chan, rx_chan, sn,
                    rx_chan, rx_chan,
                    rx_chan, rx_chan, sn),
                POLL_MSEC(500));
        #endif
        
        ext_send_msg(rx_chan, false, "EXT fax_download_avail=%d", sn);
        serno[rx_chan]++;
		return true;
	}

	return false;
}

void FAX_main();

ext_t fax_ext = {
	"FAX",
	FAX_main,
	fax_close,
	fax_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void FAX_main()
{
	ext_register(&fax_ext);
}
