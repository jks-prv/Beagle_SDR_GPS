// Copyright (c) 2023 John Seamons, ZL4VO/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "rx_util.h"
#include "data_pump.h"
#include "mem.h"
#include "printf.h"
#include "rsid.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <sys/mman.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own digi[] data structure.

typedef struct {
	int rx_chan;
	int last_freq_kHz;
	time_t last_rsid_time;
	cRsId *rsid;
	
	bool task_created;
	tid_t tid;
	int rd_pos;
	bool seq_init;
	u4_t seq;

	bool test;
    s2_t *s2p;
} digi_t;

static digi_t digi[MAX_RX_CHANS];

typedef struct {
    u64_t freq_offset_Hz;
    
    int test;
    s2_t *s2p_start, *s2p_end;
    int tsamps;
} digi_conf_t;

static digi_conf_t digi_conf;

static void digi_file_data(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps, int freqHz)
{
    digi_t *e = &digi[rx_chan];

    if (!e->test) {
        return;
    }
    if (e->s2p >= digi_conf.s2p_end) {
        e->test = false;
        return;
    }
    
    if (e->test) {
        for (int i = 0; i < nsamps; i++) {
            if (e->s2p < digi_conf.s2p_end) {
                *samps++ = (s2_t) FLIP16(*e->s2p);
            }
            e->s2p++;
        }

        #if 0
        int pct = e->nsamps * 100 / digi_conf.tsamps;
        e->nsamps += nsamps;
        pct += 3;
        if (pct > 100) pct = 100;
        ext_send_msg(rx_chan, false, "EXT bar_pct=%d", pct);
        #endif
    }

}

static void digi_task(void *param)
{
    int rx_chan = (int) FROM_VOID_PARAM(param);
    digi_t *e = &digi[rx_chan];
    conn_t *conn = rx_channels[rx_chan].conn;
    rx_dpump_t *rx = &rx_dpump[rx_chan];
    
	while (1) {
		TaskSleepReason("wait for wakeup");
		
		int new_freq_kHz = (int) round(conn->freqHz/1e3);

		while (e->rd_pos != rx->real_wr_pos) {
		    if (rx->real_seqnum[e->rd_pos] != e->seq) {
                if (!e->seq_init) {
                    e->seq_init = true;
                } else {
                    u4_t got = rx->real_seqnum[e->rd_pos], expecting = e->seq;
                    rcprintf(rx_chan, "digi SEQ: @%d got %d expecting %d (%d)\n", e->rd_pos, got, expecting, got - expecting);
                }
                e->seq = rx->real_seqnum[e->rd_pos];
            }
            e->seq++;
		    
		    //real_printf("%d ", e->rd_pos); fflush(stdout);
		    digi_conf.test = e->test;
		    //decode_ft8_samples(rx_chan, &rx->real_samples_s2[e->rd_pos][0], FASTFIR_OUTBUF_SIZE, conn->freqHz);
			e->rd_pos = (e->rd_pos+1) & (N_DPBUF-1);
		}
		
		if (e->rsid && e->rsid->rsidTime != e->last_rsid_time) {
		    //rcprintf(rx_chan, "digi CHARS %d|%d RSID: %s @ %0.1f Hz\n", e->rsid->rsidTime, e->last_rsid_time, e->rsid->rsidName, e->rsid->rsidFreq);
		    ext_send_msg_encoded(e->rx_chan, false, "EXT", "chars", "RSID: %s @ %0.1f Hz\n", e->rsid->rsidName, e->rsid->rsidFreq);
            e->last_rsid_time = e->rsid->rsidTime;
		}
    }
}

void digi_reset(digi_t *e)
{
    if (e->rsid) e->rsid->rsidTime = 0;     // otherwise last RSID msg is seen when ext reopened
    memset(e, 0, sizeof(*e));
}

void digi_close(int rx_chan)
{
    rx_util_t *r = &rx_util;
	digi_t *e = &digi[rx_chan];
    //rcprintf(rx_chan, "digi: close rx_chan=%d task_created=%d\n",
    //    rx_chan, e->task_created);
    ext_unregister_receive_real_samps_task(rx_chan);

	if (e->task_created) {
        //rcprintf(rx_chan, "digi: TaskRemove\n");
		TaskRemove(e->tid);
		e->task_created = false;
	}
	
    digi_reset(e);
}

bool digi_msgs(char *msg, int rx_chan)
{
	digi_t *e = &digi[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	//rcprintf(rx_chan, "### digi_msgs <%s>\n", msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		//rcprintf(rx_chan, "digi ext_server_init\n");
		ext_send_msg(rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	if (strcmp(msg, "SET digi_start") == 0) {
        conn_t *conn = rx_channels[rx_chan].conn;
		e->rsid = &m_RsId[rx_chan];
		e->last_freq_kHz = conn->freqHz/1e3;
        digi_conf.freq_offset_Hz = freq.offset_Hz;
		//rcprintf(rx_chan, "digi start\n");

		if (digi_conf.tsamps != 0) {
            ext_register_receive_real_samps(digi_file_data, rx_chan);
		}

        if (!e->task_created) {
            e->tid = CreateTaskF(digi_task, TO_VOID_PARAM(rx_chan), EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL));
            e->task_created = true;
        }

        e->seq_init = false;
        ext_register_receive_real_samps_task(e->tid, rx_chan);
		return true;
	}
	
	if (strcmp(msg, "SET digi_close") == 0) {
		//rcprintf(rx_chan, "digi close\n");
		digi_close(rx_chan);
		return true;
	}
	
	if (strcmp(msg, "SET digi_test") == 0) {
		//rcprintf(rx_chan, "digi test\n");
        e->s2p = digi_conf.s2p_start;
		e->test = true;
		return true;
	}
	
	return false;
}

void digi_modes_main();

ext_t digi_ext = {
	"digi_modes",
	digi_modes_main,
	digi_close,
	digi_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void digi_modes_main()
{
	ext_register(&digi_ext);

    //const char *fn = cfg_string("digi.test_file", NULL, CFG_OPTIONAL);
    const char *fn = "digi.test.au";
    if (!fn || *fn == '\0') return;
    char *fn2;
    asprintf(&fn2, "%s/samples/%s", DIR_CFG, fn);
    //cfg_string_free(fn);
    printf("digi: mmap %s\n", fn2);
    int fd = open(fn2, O_RDONLY);
    if (fd < 0) {
        printf("digi: open failed\n");
        return;
    }
    off_t fsize = kiwi_file_size(fn2);
    kiwi_asfree(fn2);
    char *file = (char *) mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) {
        printf("digi: mmap failed\n");
        return;
    }
    close(fd);
    int words = fsize/2;
    digi_conf.s2p_start = (s2_t *) file;
    u4_t off = *(digi_conf.s2p_start + 3);
    off = FLIP16(off);
    printf("digi: off=%d size=%ld\n", off, fsize);
    off /= 2;
    digi_conf.s2p_start += off;
    words -= off;
    digi_conf.s2p_end = digi_conf.s2p_start + words;
    digi_conf.tsamps = words / NIQ;
}
