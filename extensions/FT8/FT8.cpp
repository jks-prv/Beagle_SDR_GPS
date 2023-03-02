// Copyright (c) 2023 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "coroutines.h"
#include "conn.h"
#include "data_pump.h"
#include "mem.h"

#include "FT8.h"
#include "PSKReporter.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <sys/mman.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own ft8[] data structure.

typedef struct {
	int rx_chan;
	int run;
	int proto;
	int last_freq_kHz;
	
	bool task_created;
	tid_t tid;
	int rd_pos;
	bool seq_init;
	u4_t seq;

	bool test;
	u1_t start_test;
    s2_t *s2p;
} ft8_t;

static ft8_t ft8[MAX_RX_CHANS];

ft8_conf_t ft8_conf;

static void ft8_file_data(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps, int freqHz)
{
    ft8_t *e = &ft8[rx_chan];

    if (!e->test) {
        return;
    }
    if (e->s2p >= ft8_conf.s2p_end) {
        e->test = false;
        e->start_test = 0;
        return;
    }
    
    if (e->test && e->start_test) {
        for (int i = 0; i < nsamps; i++) {
            if (e->s2p < ft8_conf.s2p_end) {
                *samps++ = (s2_t) FLIP16(*e->s2p);
            }
            e->s2p++;
        }

        #if 0
        int pct = e->nsamps * 100 / ft8_conf.tsamps;
        e->nsamps += nsamps;
        pct += 3;
        if (pct > 100) pct = 100;
        ext_send_msg(rx_chan, false, "EXT bar_pct=%d", pct);
        #endif
    }

}

static void ft8_task(void *param)
{
    int rx_chan = (int) FROM_VOID_PARAM(param);
    ft8_t *e = &ft8[rx_chan];
    conn_t *conn = rx_channels[rx_chan].conn;
    rx_dpump_t *rx = &rx_dpump[rx_chan];
    decode_ft8_setup(rx_chan);
    
	while (1) {
		TaskSleepReason("wait for wakeup");
		
		int new_freq_kHz = (int) round(conn->freqHz/1e3);
		if (e->last_freq_kHz != new_freq_kHz) {
		    rcprintf(rx_chan, "FT8: freq changed %d => %d\n", e->last_freq_kHz, new_freq_kHz);
		    e->last_freq_kHz = new_freq_kHz;
		    decode_ft8_protocol(rx_chan, conn->freqHz, e->proto);
		}

		while (e->rd_pos != rx->real_wr_pos) {
		    if (rx->real_seqnum[e->rd_pos] != e->seq) {
                if (!e->seq_init) {
                    e->seq_init = true;
                } else {
                    u4_t got = rx->real_seqnum[e->rd_pos], expecting = e->seq;
                    rcprintf(rx_chan, "FT8 SEQ: @%d got %d expecting %d (%d)\n", e->rd_pos, got, expecting, got - expecting);
                }
                e->seq = rx->real_seqnum[e->rd_pos];
            }
            e->seq++;
		    
		    //real_printf("%d ", e->rd_pos); fflush(stdout);
		    decode_ft8_samples(rx_chan, &rx->real_samples[e->rd_pos][0], FASTFIR_OUTBUF_SIZE, conn->freqHz, &e->start_test);
			e->rd_pos = (e->rd_pos+1) & (N_DPBUF-1);
		}
    }
}

void ft8_close(int rx_chan)
{
	ft8_t *e = &ft8[rx_chan];
    rcprintf(rx_chan, "FT8: close task_created=%d\n", e->task_created);
    ext_unregister_receive_real_samps_task(rx_chan);

	if (e->task_created) {
        rcprintf(rx_chan, "FT8: TaskRemove\n");
		TaskRemove(e->tid);
		e->task_created = false;
	}
	
	decode_ft8_free(rx_chan);
}

bool ft8_msgs(char *msg, int rx_chan)
{
	ft8_t *e = &ft8[rx_chan];
	int n;
	
	//rcprintf(rx_chan, "### ft8_msgs <%s>\n", msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

    int proto;
	if (sscanf(msg, "SET ft8_start=%d", &proto) == 1) {
	    e->proto = proto;
        conn_t *conn = rx_channels[e->rx_chan].conn;
		e->last_freq_kHz = conn->freqHz/1e3;
		rcprintf(rx_chan, "FT8 start %s\n", proto? "FT4" : "FT8");
		decode_ft8_init(rx_chan, proto? 1:0);

		if (ft8_conf.tsamps != 0) {
            ext_register_receive_real_samps(ft8_file_data, rx_chan);
		}

        if (!e->task_created) {
            e->tid = CreateTaskF(ft8_task, TO_VOID_PARAM(rx_chan), EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL));
            e->task_created = true;
        }

        e->seq_init = false;
        ext_register_receive_real_samps_task(e->tid, rx_chan);
		return true;
	}
	
	if (sscanf(msg, "SET ft8_protocol=%d", &proto) == 1) {
	    e->proto = proto;
        conn_t *conn = rx_channels[e->rx_chan].conn;
		e->last_freq_kHz = conn->freqHz/1e3;
		rcprintf(rx_chan, "FT8 protocol %s freq %.2f\n", proto? "FT4" : "FT8", conn->freqHz/1e3);
		decode_ft8_protocol(rx_chan, conn->freqHz, proto? 1:0);
		return true;
	}

	if (strcmp(msg, "SET ft8_close") == 0) {
		rcprintf(rx_chan, "FT8 close\n");
		ft8_close(rx_chan);
		return true;
	}
	
	if (strcmp(msg, "SET ft8_test") == 0) {
		rcprintf(rx_chan, "FT8 test\n");
		e->start_test = 0;
        e->s2p = ft8_conf.s2p_start;
		e->test = true;
		return true;
	}
	
	return false;
}

void FT8_main();

ext_t ft8_ext = {
	"FT8",
	FT8_main,
	ft8_close,
	ft8_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void FT8_main()
{
	ext_register(&ft8_ext);
	PSKReporter_init();

    //const char *fn = cfg_string("FT8.test_file", NULL, CFG_OPTIONAL);
    const char *fn = "FT8.test.au";
    if (!fn || *fn == '\0') return;
    char *fn2;
    asprintf(&fn2, "%s/samples/%s", DIR_CFG, fn);
    //cfg_string_free(fn);
    printf("FT8: mmap %s\n", fn2);
    int fd = open(fn2, O_RDONLY);
    if (fd < 0) {
        printf("FT8: open failed\n");
        return;
    }
    off_t fsize = kiwi_file_size(fn2);
    kiwi_ifree(fn2);
    char *file = (char *) mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) {
        printf("FT8: mmap failed\n");
        return;
    }
    close(fd);
    int words = fsize/2;
    ft8_conf.s2p_start = (s2_t *) file;
    u4_t off = *(ft8_conf.s2p_start + 3);
    off = FLIP16(off);
    printf("FT8: off=%d size=%ld\n", off, fsize);
    off /= 2;
    ft8_conf.s2p_start += off;
    words -= off;
    ft8_conf.s2p_end = ft8_conf.s2p_start + words;
    ft8_conf.tsamps = words / NIQ;
}
