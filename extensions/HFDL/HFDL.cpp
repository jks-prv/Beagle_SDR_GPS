// Copyright (c) 2021 John Seamons, ZL4VO/KF6VO

#ifdef KIWI

#include "HFDL.h"

static hfdl_t hfdl;
static hfdl_chan_t hfdl_chan[MAX_RX_CHANS];

static void hfdl_pushback_file_data(int rx_chan, int instance, int nsamps, TYPECPX *samps)
{
    hfdl_chan_t *e = &hfdl_chan[rx_chan];

    if (!e->test || !e->input_tid) return;
    if (e->s2p >= hfdl.s2p_end) {
        //printf("HFDL: pushback test file DONE ch=%d tid=%d\n", rx_chan, e->input_tid);

        ext_send_msg(rx_chan, false, "EXT test_done");
        e->test = false;
        ext_unregister_receive_iq_samps(rx_chan);
        return;
    }
    
    // Pushback 12 kHz sample file so it sounds right.
    // Have hfdl_task() do 12k => 36k resampling before calling decoder (which requires 36 kHz sampling)
    for (int i = 0; i < nsamps; i++) {
        if (e->s2p < hfdl.s2p_end) {
            samps->re = (TYPEREAL) (s4_t) *e->s2p;
            e->s2p++;
            samps->im = (TYPEREAL) (s4_t) *e->s2p;
            e->s2p++;
            samps++;
        }
    }

    int pct = e->nsamps * 100 / hfdl.tsamps;
    e->nsamps += nsamps;
    pct += 3;
    if (pct > 100) pct = 100;
    ext_send_msg(rx_chan, false, "EXT bar_pct=%d", pct);
}

static int hfdl_input(int rx_chan, TYPECPX **samps_c = NULL)
{
    hfdl_chan_t *e = &hfdl_chan[rx_chan];
    iq_buf_t *rx = &RX_SHMEM->iq_buf[rx_chan];
    
    while (1) {
        // blocks via TaskSleep() / wakeup due to ext_register_receive_iq_samps_task()
        if (e->rd_pos != rx->iq_wr_pos) {
            if (rx->iq_seqnum[e->rd_pos] != e->seq) {
                if (!e->seq_init) {
                    e->seq_init = true;
                } else {
                    u4_t got = rx->iq_seqnum[e->rd_pos], expecting = e->seq;
                    rcprintf(rx_chan, "HFDL SEQ: @%d got %d expecting %d (%d)\n", e->rd_pos, got, expecting, got - expecting);
                }
                e->seq = rx->iq_seqnum[e->rd_pos];
            }
            e->seq++;
    
            if (e->reset) {
                e->reset = false;
            }

            if (samps_c) *samps_c = &rx->iq_samples[e->rd_pos][0];
            e->rd_pos = (e->rd_pos+1) & (N_DPBUF-1);

            return HFDL_OUTBUF_SIZE;
        } else {
            TaskSleepReason("wait for wakeup");
        }
    }
}

static void hfdl_task(void *param)
{
    int i, n;
    int rx_chan = (int) FROM_VOID_PARAM(param);
    hfdl_chan_t *e = &hfdl_chan[rx_chan];

    while (1) {
        n = hfdl_input(rx_chan, &e->samps_c);

        //if (e->test && e->input_tid) {
        if (e->input_tid) {
            e->HFDLResample.run((float *) e->samps_c, n, &e->resamp);

            #define SAMPLES_CF32
            #ifdef SAMPLES_CF32
                // needed so hfdl_channel_create() "c->noise_floor = 1.0f" works
                // i.e. float samples need to be restricted to (+1,-1)
                const float f_scale = 32768.0f;      
                static float out[HFDL_N_SAMPS];
                for (int i=0; i < e->outputBlockSize; i++) {
                    out[i*2]   = e->resamp.out_I[i] / f_scale;
                    out[i*2+1] = e->resamp.out_Q[i] / f_scale;
                }
            #endif

            // FIXME: doesn't seem to work?
            //#define SAMPLES_CS16
            #ifdef SAMPLES_CS16
                const s2_t s2_scale = 2;
                static s2_t out[HFDL_N_SAMPS];
                for (int i=0; i < e->outputBlockSize; i++) {
                    out[i*2]   = (s2_t) (e->resamp.out_I[i] / s2_scale);
                    out[i*2+1] = (s2_t) (e->resamp.out_Q[i] / s2_scale);
                }
            #endif
            
            //static int wakes;
            //real_printf("%d-", wakes++); fflush(stdout);
            TaskWakeupFP(e->input_tid, TWF_NONE, TO_VOID_PARAM(out));
        }
    }
}

void hfdl_close(int rx_chan)
{
	hfdl_chan_t *e = &hfdl_chan[rx_chan];
    //printf("HFDL: close tid=%d dumphfdl_tid=%d\n", e->tid, e->dumphfdl_tid);

    ext_unregister_receive_iq_samps_task(e->rx_chan);
    
	if (e->tid) {
        //printf("HFDL: TaskRemove dumphfdl_task\n");
		TaskRemove(e->tid);
		e->tid = 0;
	}

#if 0
	if (e->dumphfdl_tid) {
        //printf("HFDL: TaskRemove dumphfdl_tid\n");
		TaskRemove(e->dumphfdl_tid);
		e->dumphfdl_tid = 0;
	}
#else
    // everything should shutdown when the input routine receives an EOF
    if (e->input_tid)
        TaskWakeupFP(e->input_tid, TWF_NONE, TO_VOID_PARAM(0));
#endif

    ext_unregister_receive_cmds(e->rx_chan);
}

bool hfdl_receive_cmds(u2_t key, char *cmd, int rx_chan)
{
    int i, n;
    
    if (key == CMD_TUNE) {
        char *mode_m;
        double locut, hicut, freq;
        int mparam;
        n = sscanf(cmd, "SET mod=%16ms low_cut=%lf high_cut=%lf freq=%lf param=%d", &mode_m, &locut, &hicut, &freq, &mparam);
        if (n == 4 || n == 5) {
	        hfdl_chan_t *e = &hfdl_chan[rx_chan];
	        e->tuned_f = freq;
            //printf("HFDL: CMD_TUNE freq=%.2f mode=%s\n", freq, mode_m);
            kiwi_asfree(mode_m);
            return true;
        }
    }
    
    return false;
}

static const char *hfdl_argv[] = {
    "dumphfdl", "--system-table", DIR_CFG "/samples/HFDL_systable.conf",
    "--in-buf",

    #ifdef SAMPLES_CF32
        "--sample-format", "CF32",
    #endif

    #ifdef SAMPLES_CS16
        "--sample-format", "CS16",
    #endif

    "--sample-rate", "36000", "--centerfreq", "0", "0"
};

static void dumphfdl_task(void *param)
{
    int rx_chan = (int) FROM_VOID_PARAM(param);
    hfdl_chan_t *e = &hfdl_chan[rx_chan];

    #ifdef HFDL
        dumphfdl_main(ARRAY_LEN(hfdl_argv), (char **) hfdl_argv, rx_chan, e->outputBlockSize * NIQ);
    #endif
    e->dumphfdl_tid = 0;
}
    
bool hfdl_msgs(char *msg, int rx_chan)
{
	hfdl_chan_t *e = &hfdl_chan[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	//printf("### hfdl_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {

		ext_send_msg(rx_chan, false, "EXT ready");
		return true;
	}

	if (strcmp(msg, "SET start") == 0) {
		//printf("HFDL: start\n");
        //c2s_waterfall_no_sync(rx_chan, true);

        e->s2p = e->s2px = e->s22p = hfdl.s2p_start;

        if (!e->tid) {
            e->seq_init = false;
			e->tid = CreateTaskF(hfdl_task, TO_VOID_PARAM(rx_chan), EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL));
        }
		
		ext_register_receive_iq_samps_task(e->tid, rx_chan, POST_AGC);

        e->outputBlockSize = e->HFDLResample.setup(snd_rate, HFDL_OUTBUF_SIZE);
        if (!e->dumphfdl_tid)
            e->dumphfdl_tid = CreateTaskF(dumphfdl_task, TO_VOID_PARAM(rx_chan), EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL));

		ext_register_receive_cmds(hfdl_receive_cmds, rx_chan);
		return true;
	}

	if (strcmp(msg, "SET stop") == 0) {
		//printf("HFDL: stop\n");
        //c2s_waterfall_no_sync(rx_chan, false);
		hfdl_close(rx_chan);
		e->test = false;
		return true;
	}
	
	if (strcmp(msg, "SET reset") == 0) {
		//printf("HFDL: RESET\n");
		e->reset = true;
		e->test = false;
		return true;
	}
	
	if (strcmp(msg, "SET send_lowres_latlon") == 0) {
	    if (kiwi.lowres_lat != 0 || kiwi.lowres_lon != 0)
            ext_send_msg_encoded(rx_chan, false, "EXT", "lowres_latlon", "[%d,%d]", kiwi.lowres_lat, kiwi.lowres_lon);
		return true;
	}
	
	tid_t tid;
	if (sscanf(msg, "SET input_tid=%d", &tid) == 1) {
	    e->input_tid = tid;
		//printf("HFDL: input_tid=%d\n", tid);
		return true;
	}
	
	double freq;
	if (sscanf(msg, "SET display_freq=%lf", &freq) == 1) {
		//rcprintf(rx_chan, "HFDL: display_freq=%.2lf\n", freq);
        #ifdef HFDL
		    dumphfdl_set_freq(rx_chan, freq);
		#endif
		return true;
	}

	if (sscanf(msg, "SET tune=%lf", &freq) == 1) {
		conn_t *conn = rx_channels[rx_chan].conn;
        input_msg_internal(conn, "SET mod=x low_cut=0 high_cut=0 freq=%.2f", freq);
		e->freq = freq;
		return true;
	}
	
	double test_f;
	if (sscanf(msg, "SET test=%lf", &test_f) == 1) {
        e->s2p = e->s2px = e->s22p = hfdl.s2p_start;
        e->test_f = test_f;
        e->nsamps = 0;

        //bool test = (test_f != 0) && (snd_rate != SND_RATE_3CH);
        bool test = (test_f != 0);
        //printf("HFDL: test=%d test_f=%.2f\n", test, e->test_f);
        if (test) {
            // misuse ext_register_receive_iq_samps() to pushback audio samples from the test file
            ext_register_receive_iq_samps(hfdl_pushback_file_data, rx_chan, PRE_AGC);
            e->test = true;
        } else {
            e->test = false;
            ext_unregister_receive_iq_samps(rx_chan);
        }
		return true;
	}
	
	return false;
}

void HFDL_main();

ext_t hfdl_ext = {
	"HFDL",
	HFDL_main,
	hfdl_close,
	hfdl_msgs,
};

void HFDL_main()
{
    hfdl.nom_rate = snd_rate;

    #ifdef HFDL
	    ext_register(&hfdl_ext);
        dumphfdl_init();
	#else
        printf("ext_register: \"HFDL\" not configured\n");
	    return;
	#endif

    int n;
    char *file;
    int fd;
    const char *fn;
    
    fn = DIR_CFG "/samples/HFDL_test_12k_cf13270_df13270.wav";
    printf("HFDL: mmap %s\n", fn);
    scall("hfdl open", (fd = open(fn, O_RDONLY)));
    off_t fsize = kiwi_file_size(fn);
    file = (char *) mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) sys_panic("HFDL mmap");
    close(fd);
    printf("HFDL: size=%ld\n", fsize);
    int words = fsize/2;
    hfdl.s2p_start = (s2_t *) file;
    hfdl.s2p_end = hfdl.s2p_start + words;
    hfdl.tsamps = words / NIQ;
}

#endif
