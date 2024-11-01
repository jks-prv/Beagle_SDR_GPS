// Copyright (c) 2021 John Seamons, ZL4VO/KF6VO

#ifdef KIWI

#include "ALE_2G.h"

// don't use 12k => 8k resampler
//#define NO_RESAMPLING_8K

// run in do_modem() in a task, but don't use rx buffering
//#define DIRECT_NO_RX_BUF

// better resampler from DRM/Dream
#define NEW_RESAMPLER


//#define ALE_2G_DEBUG_MSG	true
#define ALE_2G_DEBUG_MSG	false

static ale_2g_t ale_2g;
static ale_2g_chan_t ale_2g_chan[MAX_RX_CHANS];

#ifdef ALE_2G_TEST_FILE

static void ale_2g_file_data(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps, int freqHz)
{
    ale_2g_chan_t *e = &ale_2g_chan[rx_chan];

    if (!e->test) return;
    if (e->s2p >= ale_2g.s2p_end) {
        //printf("ale_2g_file_data test_done\n");
        ext_send_msg(rx_chan, false, "EXT test_done");
        e->test = false;
        return;
    }
    
    // Pushback 12 kHz sample file so it sounds right.
    // Have ale_2g_task() do 12k => 8k resampling before calling decoder (which requires 8 kHz sampling)
    if (e->test) {
        double tuned_f = ext_get_displayed_freq_kHz(e->rx_chan);
        bool pushback = (e->test_f < 0 || fabs(fabs(e->test_f) - tuned_f) < 0.001);

        for (int i = 0; i < nsamps; i++) {
            if (e->s2p < ale_2g.s2p_end && pushback) {
                *samps++ = (s2_t) FLIP16(*e->s2p);
            }
            e->s2p++;
        }

        int pct = e->nsamps * 100 / ale_2g.tsamps;
        e->nsamps += nsamps;
        pct += 3;
        if (pct > 100) pct = 100;
        ext_send_msg(rx_chan, false, "EXT bar_pct=%d", pct);
    }

}
#endif

static int ale_2g_input(int rx_chan, s2_t **inp = NULL, int *freqHz = NULL)
{
    ale_2g_chan_t *e = &ale_2g_chan[rx_chan];
    rx_dpump_t *rx = &rx_dpump[rx_chan];
    
    #ifdef DIRECT_NO_RX_BUF
        while (1) {
            TaskSleepMsec(25);
            if (e->test && inp) {
                for (int i = 0; i < FASTFIR_OUTBUF_SIZE; i++) {
                    if (e->s2p < ale_2g.s2p_end) {
                        (*inp)[i] = (s2_t) FLIP16(*e->s2p);
                    }
                    e->s2p++;
                }

                if (e->s2p < ale_2g.s2p_end) {
                #if 1
                    int pct = e->nsamps * 100 / ale_2g.tsamps;
                    e->nsamps += FASTFIR_OUTBUF_SIZE;
                    pct += 3;
                    if (pct > 100) pct = 100;
                    ext_send_msg(rx_chan, false, "EXT bar_pct=%d", pct);
                #endif

                    return FASTFIR_OUTBUF_SIZE;
                } else {
                    ext_send_msg(rx_chan, false, "EXT test_done");
                    e->test = false;
                }
            }
        }
    #else

	while (1) {
        if (e->rd_pos != rx->real_wr_pos) {
            if (rx->real_seqnum[e->rd_pos] != e->seq) {
                if (!e->seq_init) {
                    e->seq_init = true;
                } else {
                    u4_t got = rx->real_seqnum[e->rd_pos], expecting = e->seq;
                    printf("ALE_2G rx%d SEQ: @%d got %d expecting %d (%d)\n", rx_chan, e->rd_pos, got, expecting, got - expecting);
                }
                e->seq = rx->real_seqnum[e->rd_pos];
            }
            e->seq++;
        
            if (e->reset) {
                e->reset = false;
            }

            if (inp) *inp = &rx->real_samples_s2[e->rd_pos][0];
            if (freqHz) *freqHz = rx->freqHz[e->rd_pos];
            e->rd_pos = (e->rd_pos+1) & (N_DPBUF-1);

            return FASTFIR_OUTBUF_SIZE;
        } else {
            TaskSleepReason("wait for wakeup");
        }
    }
    #endif
}

static int ale_2g_slot(double freq)
{
    if (fabs(freq - 23982.60) < 0.001) return 0;
    if (fabs(freq - 24000.60) < 0.001) return 1;
    if (fabs(freq - 24013.00) < 0.001) return 2;
    return 999;
}

static void ale_2g_task(void *param)
{
    int i, n;
    int rx_chan = (int) FROM_VOID_PARAM(param);
    ale_2g_chan_t *e = &ale_2g_chan[rx_chan];
    e->ni = 0;

    while (1) {
        #ifdef NO_RESAMPLING_8K
            // don't do any resampling
            e->ni = ale_2g_input(rx_chan, &e->inp);
            for (i = 0; i < e->ni; i++) {
                e->fout[i] = (float) e->inp[i];
            }
            e->decode.do_modem(e->fout, e->ni);
        #else
            if (e->use_new_resampler) {
                int freqHz;
                n = ale_2g_input(rx_chan, &e->inp, &freqHz);
                double tuned_f1 = (double) freqHz / kHz + freq.offset_kHz;
                
                #if 0
                    double tuned_f2 = ext_get_displayed_freq_kHz(e->rx_chan);
                    bool diff_f = (fabs(tuned_f2 - tuned_f1) >= 0.001);
                    static bool different;
                    if (diff_f) {
                        real_printf("%d|%d ", ale_2g_slot(tuned_f2), ale_2g_slot(tuned_f1));
                        different = true;
                    } else {
                        if (different)
                            real_printf("%d.", ale_2g_slot(tuned_f1));
                        else
                            real_printf(".");
                        different = false;
                    }
                    fflush(stdout);
                #endif

                e->decode.set_freq(tuned_f1);
                e->decode.do_modem_resampled(e->inp, n);
            } else {
            
                // 12k/8k = 1.5 = 375/250                                                                                                                                           

                int v4 = 0, v5 = 0;

                for (int v2 = 0; v2 < 250; v2++) {
                    v5 += 15000;
                    int v6 = v5 / 10000;
                    v5 -= v6 * 10000;
                    float v0 = 0;
                    int v3 = v4;
                    v4 += v6;
                    while (v3 < v4) {
                        if (e->ni == 0) { e->ni = ale_2g_input(rx_chan, &e->inp); i = 0; }
                        v0 += (float) e->inp[i]; i++; e->ni--;
                        v3++;
                    }

                    v0 /= (float) v6;
                    if (v0 > 32767) v0 = 32767;
                    else
                    if (v0 < -32768) v0 = -32768;
                    e->fout[v2] = v0;
                }
            
                //printf("O"); fflush(stdout);
                e->decode.do_modem(e->fout, 250);
            }
        #endif
    
        NextTask("ale_2g_task");
        //NextTaskFast("ale_2g_task");
    }
}

void ale_2g_close(int rx_chan)
{
	ale_2g_chan_t *e = &ale_2g_chan[rx_chan];
    //printf("ALE_2G: close task_created=%d\n", e->task_created);

    ext_unregister_receive_real_samps_task(e->rx_chan);
    
    #ifdef ALE_2G_TEST_FILE
        ext_unregister_receive_real_samps(e->rx_chan);
    #endif

	if (e->task_created) {
        //printf("ALE_2G: TaskRemove\n");
		TaskRemove(e->tid);
		e->task_created = false;
	}

    ext_unregister_receive_cmds(e->rx_chan);
}

bool ale_2g_receive_cmds(u2_t key, char *cmd, int rx_chan)
{
    int i, n;
    
    if (key == CMD_TUNE) {
        char *mode_m;
        double locut, hicut, freq;
        int mparam;
        n = sscanf(cmd, "SET mod=%16ms low_cut=%lf high_cut=%lf freq=%lf param=%d", &mode_m, &locut, &hicut, &freq, &mparam);
        if (n == 4 || n == 5) {
	        ale_2g_chan_t *e = &ale_2g_chan[rx_chan];
	        e->tuned_f = freq;
            //printf("ALE_2G: CMD_TUNE freq=%.2f mode=%s\n", freq, mode_m);
            kiwi_asfree(mode_m);
            return true;
        }
    }
    
    return false;
}

bool ale_2g_msgs(char *msg, int rx_chan)
{
	ale_2g_chan_t *e = &ale_2g_chan[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	//printf("### ale_2g_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {

		ext_send_msg(rx_chan, ALE_2G_DEBUG_MSG, "EXT ready");
		return true;
	}

	if (strcmp(msg, "SET start") == 0) {
		//printf("ALE_2G: start\n");
        //c2s_waterfall_no_sync(rx_chan, true);

        #ifdef ALE_2G_TEST_FILE
            e->s2p = e->s2px = e->s22p = ale_2g.s2p_start;
            
            // misuse ext_register_receive_real_samps() to pushback audio samples from the test file
            #ifdef DIRECT_NO_RX_BUF
                e->inp = e->s2in;
            #else
		        ext_register_receive_real_samps(ale_2g_file_data, rx_chan);
		    #endif
		#endif

        if (!e->task_created) {
            e->seq_init = false;
			e->tid = CreateTaskF(ale_2g_task, TO_VOID_PARAM(rx_chan), EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL));
            e->task_created = true;
        }
		
		ext_register_receive_real_samps_task(e->tid, rx_chan);

		ext_register_receive_cmds(ale_2g_receive_cmds, rx_chan);
		return true;
	}

	if (strcmp(msg, "SET stop") == 0) {
		//printf("ALE_2G: stop\n");
        //c2s_waterfall_no_sync(rx_chan, false);
		ale_2g_close(rx_chan);
		e->test = false;
		return true;
	}
	
	if (strcmp(msg, "SET reset") == 0) {
		//printf("ALE_2G: RESET\n");
		e->reset = true;
		e->test = false;
		e->decode.modem_init(rx_chan, e->use_new_resampler, ext_update_get_sample_rateHz(rx_chan), FASTFIR_OUTBUF_SIZE);
		return true;
	}
	
	int use_new_resampler;
	if (sscanf(msg, "SET use_new_resampler=%d", &use_new_resampler) == 1) {
		//printf("ALE_2G: use_new_resampler %d\n", use_new_resampler);
		e->use_new_resampler = use_new_resampler? true:false;
		e->decode.modem_init(rx_chan, e->use_new_resampler, ext_update_get_sample_rateHz(rx_chan), FASTFIR_OUTBUF_SIZE);
		return true;
	}
	
	int dsp;
	if (sscanf(msg, "SET display=%d", &dsp) == 1) {
		//printf("ALE_2G: display %d\n", dsp);
		e->dsp = dsp;
		e->decode.set_display(dsp);
		return true;
	}
	
	double freq;
	int scan;
	if (sscanf(msg, "SET tune=%lf,%d", &freq, &scan) == 2) {
		//printf("ALE_2G: rx tune %.2f\n", freq);
		//e->decode.set_freq(freq);
        ext_send_msg(rx_chan, false, "EXT tune_ack=%.2f", freq);
        //snd_send_msg_encoded(rx_chan, true, "MSG", "audio_flags2", "tune_ack:%.2f", frequency);

		conn_t *conn = rx_channels[rx_chan].conn;
        input_msg_internal(conn, "SET mod=x low_cut=0 high_cut=0 freq=%.2f param=%d", freq, scan? MODE_FLAGS_SCAN : 0);
		e->freq = freq;
		
		//#define TEST_SCAN_STOP
		#ifdef TEST_SCAN_STOP
		    if (!e->test && freq == 7102) ext_send_msg(rx_chan, false, "EXT test_start");
		#endif
		
		return true;
	}
	
	double test_f;
	if (sscanf(msg, "SET test=%lf", &test_f) == 1) {
        #ifdef ALE_2G_TEST_FILE
            e->s2p = e->s2px = e->s22p = ale_2g.s2p_start;
            e->test_f = test_f;
            e->nsamps = 0;
		#endif

        //e->test = (test_f != 0) && (snd_rate != SND_RATE_3CH);
        e->test = (test_f != 0);
        //printf("ALE_2G: test=%d test_f=%.2f\n", e->test, e->test_f);
        if (e->test) {
            e->decode.modem_init(rx_chan, e->use_new_resampler, ext_update_get_sample_rateHz(rx_chan), FASTFIR_OUTBUF_SIZE);
		    e->decode.set_freq(fabs(e->test_f));
        } else {
            e->decode.modem_reset();    // in case need to reset active state due to a force cleared test mode
        }
		return true;
	}
	
	return false;
}

void ALE_2G_main();

ext_t ale_2g_ext = {
	"ALE_2G",
	ALE_2G_main,
	ale_2g_close,
	ale_2g_msgs,
};

void ALE_2G_main()
{
	ext_register(&ale_2g_ext);

#ifdef ALE_2G_TEST_FILE
    int n;
    char *file;
    int fd;
    const char *fn;
    
    #ifdef NO_RESAMPLING_8K
        ale_2g.test_fn_sel = FILE_8k;
    #endif

    if (ale_2g.test_fn_sel == FILE_8k)
        fn = DIR_CFG "/samples/ALE.test.8k.au";
    else
        fn = DIR_CFG "/samples/ALE.test.12k.au";
    printf("ALE_2G: mmap %s\n", fn);
    scall("ale_2g open", (fd = open(fn, O_RDONLY)));
    off_t fsize = kiwi_file_size(fn);
    file = (char *) mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) sys_panic("ALE_2G mmap");
    close(fd);
    int words = fsize/2;
    ale_2g.s2p_start = (s2_t *) file;
    u4_t off = *(ale_2g.s2p_start + 3);

    #ifdef DEVSYS
        ale_2g.file_off = p0;
    #else
        if (ale_2g.file_off == 0) ale_2g.file_off = 16;
    #endif

    off = FLIP16(off) + ale_2g.file_off;
    printf("ALE_2G: size=%ld file_off=%d off=%d(0x%x) first_word=0x%04x\n",
        fsize, ale_2g.file_off, off, off, FLIP16(*(ale_2g.s2p_start + off/2)));
    off /= 2;
    ale_2g.s2p_start += off;
    words -= off;
    ale_2g.s2p_end = ale_2g.s2p_start + words;
    ale_2g.tsamps = words;
#endif
}

// direct test called from Kiwi main()
#if (defined(DEVSYS) && 1) || (defined(HOST) && 0)

void ale_2g_test()
{
    int i, j;
    
    printf("ale_2g_test: p0 = file_off, p2 = 1[dsp=DBG]; or compile NO_RESAMPLING_8K\n");
    int rx_chan = 0;
    ale_2g_chan_t *e = &ale_2g_chan[rx_chan];
    e->rx_chan = rx_chan;
    ale_2g.file_off = p0;
    
    static float fout[FASTFIR_OUTBUF_SIZE];
    
#if 0
    if (p1)
        e->use_new_resampler = false;
    else
#endif
        e->use_new_resampler = true;

    #ifdef NO_RESAMPLING_8K
        float f_srate = 8000;
        ale_2g.test_fn_sel = FILE_8k;
    #else
        float f_srate = 12000;
        ale_2g.test_fn_sel = FILE_12k;
        
        if (e->use_new_resampler == false)
            panic("ale_2g_test() can't do OLD resampler currently, only NEW");
    #endif

    ALE_2G_main();
    e->dsp = p2? DBG : CMDS;
    e->decode.set_display(e->dsp);
    e->decode.modem_init(e->rx_chan, e->use_new_resampler, f_srate, FASTFIR_OUTBUF_SIZE, false);

    for (j = 0; j < 8; j++) {
        printf("----------------------------------------------------- iter %d\n", j);
        e->s2px = ale_2g.s2p_start;
        
        while (1) {
            for (i = 0; i < FASTFIR_OUTBUF_SIZE; i++) {
                if (e->s2px < ale_2g.s2p_end) {
                    e->s2in[i] = (s2_t) FLIP16(*e->s2px);
                    fout[i] = (float) e->s2in[i];
                }
                e->s2px++;
            }
        
            if (e->s2px >= ale_2g.s2p_end)
                break;
        
            if (e->use_new_resampler)
                e->decode.do_modem_resampled(e->s2in, FASTFIR_OUTBUF_SIZE);
            else
                e->decode.do_modem(fout, FASTFIR_OUTBUF_SIZE);
        }

        e->decode.modem_reset();
        if (p1 && j == 1) break;
    }
}

#endif

#endif
