// Copyright (c) 2018 John Seamons, ZL4VO/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "coroutines.h"
#include "data_pump.h"
#include "mem.h"
#include "uhsdr_cw_decoder.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <sys/mman.h>

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

	bool test;
    int nsamps;
    s2_t *s2p;
} cw_decoder_t;

static cw_decoder_t cw_decoder[MAX_RX_CHANS];

typedef struct {
    s2_t *s2p_start, *s2p_end;
    int tsamps;
} cw_conf_t;

static cw_conf_t cw_conf;

static void cw_file_data(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps, int freqHz)
{
    cw_decoder_t *e = &cw_decoder[rx_chan];

    if (!e->test) return;
    /*
    if (e->s2p >= cw_conf.s2p_end) {
        ext_send_msg(rx_chan, false, "EXT test_done");
        e->test = false;
        return;
    }
    */
    
    if (e->test) {
        //real_printf("#"); fflush(stdout);
        for (int i = 0; i < nsamps; i++) {
            if (e->s2p > cw_conf.s2p_end) {
                e->s2p = cw_conf.s2p_start;
                e->nsamps = 0;
            }
            *samps++ = (s2_t) FLIP16(*e->s2p);
            e->s2p++;
        }

        int pct = e->nsamps * 100 / cw_conf.tsamps;
        e->nsamps += nsamps;
        pct = CLAMP(pct, 3, 100);
        ext_send_msg(rx_chan, false, "EXT bar_pct=%d", pct);
    }

}

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
		    
		    //real_printf("%d ", e->rd_pos); fflush(stdout);
		    CwDecode_RxProcessor(rx_chan, 0, FASTFIR_OUTBUF_SIZE, &rx->real_samples_s2[e->rd_pos][0]);
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
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	//printf("### cw_decoder_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	int training_interval;
	if (sscanf(msg, "SET cw_start=%d", &training_interval) == 1) {
		printf("CW rx%d start training_interval=%d\n", rx_chan, training_interval);
		CwDecode_Init(rx_chan, 0, training_interval);
		
		if (cw_conf.tsamps != 0) {
            ext_register_receive_real_samps(cw_file_data, rx_chan);
		}

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
		e->test = false;
		return true;
	}
	
	if (sscanf(msg, "SET cw_train=%d", &training_interval) == 1) {
		printf("CW rx%d train %d\n", rx_chan, training_interval);
		CwDecode_Init(rx_chan, 0, training_interval);
		//e->test = false;
		return true;
	}
	
	int test;
	if (sscanf(msg, "SET cw_test=%d", &test) == 1) {
		printf("CW rx%d test=%d\n", rx_chan, test);
        e->s2p = cw_conf.s2p_start;
        e->nsamps = 0;
        e->test = test? true:false;
		return true;
	}
	
	u4_t pboff;
	if (sscanf(msg, "SET cw_pboff=%d", &pboff) == 1) {
		//printf("CW rx%d pboff %d\n", rx_chan, pboff);
		CwDecode_pboff(rx_chan, pboff);
		return true;
	}
	
	int wpm;
	if (sscanf(msg, "SET cw_wpm=%d,%d", &wpm, &training_interval) == 2) {
		printf("CW rx%d wpm %d\n", rx_chan, wpm);
		CwDecode_wpm(rx_chan, wpm, training_interval);
		return true;
	}
	
	int wsc;
	if (sscanf(msg, "SET cw_wsc=%d", &wsc) == 1) {
		//printf("CW rx%d wsc %d\n", rx_chan, wsc);
		CwDecode_wsc(rx_chan, wsc);
		return true;
	}
	
	int isAutoThreshold;
	if (sscanf(msg, "SET cw_auto_thresh=%d", &isAutoThreshold) == 1) {
		printf("CW rx%d isAutoThreshold=%d\n", rx_chan, isAutoThreshold);
		CwDecode_threshold(rx_chan, 0, isAutoThreshold);
		return true;
	}
	
	int threshold_lin;
	if (sscanf(msg, "SET cw_threshold=%d", &threshold_lin) == 1) {
		printf("CW rx%d threshold_lin=%d\n", rx_chan, threshold_lin);
		CwDecode_threshold(rx_chan, 1, threshold_lin);
		return true;
	}
	
	return false;
}

void CW_decoder_main();

ext_t cw_decoder_ext = {
	"CW_decoder",
	CW_decoder_main,
	cw_close,
	cw_decoder_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void CW_decoder_main()
{
	ext_register(&cw_decoder_ext);

    //const char *fn = cfg_string("CW.test_file", NULL, CFG_OPTIONAL);
    const char *fn = "CW.test.au";
    if (!fn || *fn == '\0') return;
    char *fn2;
    asprintf(&fn2, "%s/samples/%s", DIR_CFG, fn);
    //cfg_string_free(fn);
    printf("CW: mmap %s\n", fn2);
    int fd = open(fn2, O_RDONLY);
    if (fd < 0) {
        printf("CW: open failed\n");
        return;
    }
    off_t fsize = kiwi_file_size(fn2);
    kiwi_asfree(fn2);
    char *file = (char *) mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) {
        printf("CW: mmap failed\n");
        return;
    }
    close(fd);
    int words = fsize/2;
    cw_conf.s2p_start = (s2_t *) file;
    u4_t off = *(cw_conf.s2p_start + 3);
    off = FLIP16(off);
    printf("CW: off=%d size=%ld\n", off, fsize);
    off /= 2;
    cw_conf.s2p_start += off;
    words -= off;
    cw_conf.s2p_end = cw_conf.s2p_start + words;
    cw_conf.tsamps = words;
}
