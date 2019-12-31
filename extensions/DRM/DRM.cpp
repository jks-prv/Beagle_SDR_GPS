// Copyright (c) 2017-2019 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "DRM.h"
#include "DRM_main.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <sys/mman.h>

#ifdef DRM_SHMEM_DISABLE
    static drm_shmem_t drm_shmem;
    drm_shmem_t *drm_shmem_p = &drm_shmem;
#endif

static drm_info_t drm_info;

void DRM_close(int rx_chan)
{
    drm_t *d = &DRM_SHMEM->drm[rx_chan];
    assert(d->init);
    
    #ifdef DRM_SHMEM_DISABLE
        if (d->tid) {
            TaskRemove(d->tid);
            d->tid = 0;
        }
    #endif
}

#ifdef DRM_SHMEM_DISABLE

void drm_task(void *param)
{
    int rx_chan = (int) FROM_VOID_PARAM(param);
	rcprintf(rx_chan, "DRM drm_task calling DRL_loop() rx_chan=%d\n", rx_chan);
    DRM_loop(rx_chan);
    DRM_SHMEM->drm[rx_chan].tid = 0;
}

#endif

#ifdef DRM_TEST_FILE
static void drm_pushback_file_data(int rx_chan, int chan, int nsamps, TYPECPX *samps)
{
    drm_t *d = &DRM_SHMEM->drm[rx_chan];
    assert(d->init);
    if (!d->test) return;
    int pct = d->tsamp * 100 / ((d->test == 1)? d->info->tsamps1 : d->info->tsamps2);
    ext_send_msg(d->rx_chan, false, "EXT drm_bar_pct=%d", pct);

    for (int i = 0; i < nsamps; i++) {
        u2_t t;
        if (d->s2p >= ((d->test == 1)? d->info->s2p_end1 : d->info->s2p_end2)) {
            d->s2p = ((d->test == 1)? d->info->s2p_start1 : d->info->s2p_start2);
            d->tsamp = 0;
            ext_send_msg(d->rx_chan, false, "EXT annotate=0");
        }
        t = (u2_t) *d->s2p;
        d->s2p++;
        samps->re = (TYPEREAL) (s4_t) (s2_t) FLIP16(t);
        t = (u2_t) *d->s2p;
        d->s2p++;
        samps->im = (TYPEREAL) (s4_t) (s2_t) FLIP16(t);
        samps++;
        d->tsamp++;
    }
}
#endif

bool DRM_msgs(char *msg, int rx_chan)
{
    
    drm_t *d = &DRM_SHMEM->drm[rx_chan];
	rcprintf(rx_chan, "DRM <%s>\n", msg);

    if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, false, "EXT ready");
		
		// extension notified of rx_chan >= drm_info.drm_chan in response to lock_set below
		if (rx_chan < drm_info.drm_chan) {
            d->rx_chan = rx_chan;	// remember our receiver channel number
    
            if (!d->tid) {
                #ifdef DRM_SHMEM_DISABLE
                    d->tid = CreateTaskF(drm_task, TO_VOID_PARAM(rx_chan), EXT_PRIORITY, CTF_STACK_LARGE | CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL));
                #else
                    rcprintf(rx_chan, "DRM ext_server_init shmem_ipc_invoke rx_chan=%d\n", rx_chan);
                    shmem_ipc_invoke(SIG_IPC_DRM + rx_chan, rx_chan, NO_WAIT);
                    d->tid = 1;
                #endif
            }
        }
        
        return true;
    }

    if (strcmp(msg, "SET lock_set") == 0) {
        int rv;
        if (snd_rate != SND_RATE_4CH) {
            rv = -1;
        } else {
            if (is_BBAI) {
                is_locked = (rx_chan < drm_info.drm_chan)? 0 : drm_info.drm_chan;
            } else {
                int inuse = rx_chans - rx_chan_free_count(RX_COUNT_ALL, NULL);
                is_locked = (inuse == 1)? 1:0;
                printf("DRM lock_set inuse=%d locked=%d\n", inuse, is_locked);
            }
            rv = is_locked;
            //#define DRM_TEST_MULTI
            #ifdef DRM_TEST_MULTI
                //jks test BBG/B 2 DRM
                if (!is_BBAI) {
                    is_locked = 0;
                    rv = 1;
                }
            #endif
        }
		ext_send_msg(rx_chan, false, "EXT locked=%d", rv);
        return true;    
    }
    
    if (strcmp(msg, "SET lock_clear") == 0) {
        is_locked = 0;
        printf("DRM lock_clear\n");
        return true;    
    }
    
    int run = 0;
    if (sscanf(msg, "SET run=%d", &run) == 1) {
        printf("DRM run=%d rx_chan=%d\n", run, rx_chan);
        d->run = run;
        return true;
    }

    int test = 0;
    if (sscanf(msg, "SET test=%d", &test) == 1) {
        printf("DRM test=%d rx_chan=%d\n", test, rx_chan);
        d->test = test;

        if (d->test) {
            d->s2p = ((d->test == 1)? d->info->s2p_start1 : d->info->s2p_start2);
            d->tsamp = 0;
            
            // misuse ext_register_receive_real_samps() to pushback audio samples from the test file
            ext_register_receive_iq_samps(drm_pushback_file_data, rx_chan);
        } else {
            ext_unregister_receive_iq_samps(rx_chan);
        }
        return true;
    }
    
    int svc = 0;
    if (sscanf(msg, "SET svc=%d", &svc) == 1) {
        d->audio_service = svc - 1;
        return true;
    }
    
    int debug = 0;
    if (sscanf(msg, "SET debug=%d", &debug) == 1) {
        d->debug = debug;
        return true;
    }
    
    int send_iq;
    if (sscanf(msg, "SET send_iq=%d", &send_iq) == 1) {
        d->send_iq = send_iq? true:false;
        return true;
    }
    
    int monitor = 0;
    if (sscanf(msg, "SET monitor=%d", &monitor) == 1) {
        return true;
    }
    
    if (strcmp(msg, "SET reset") == 0) {
        d->reset = true;
        d->i_epoch = d->i_samples = d->i_tsamples = 0;
        d->no_input = d->sent_silence = 0;
        return true;    
    }
    
    return false;
}

void DRM_msg(drm_t *drm, kstr_t *ks)
{
    kiwi_strncpy(drm->msg_buf, kstr_sp(ks), N_MSGBUF);
    drm->msg_tx_seq++;
    DRM_YIELD();
}

void DRM_data(drm_t *drm, u1_t cmd, u1_t *data, u4_t nbuf)
{
    assert(nbuf <= N_DATABUF);
    drm->data_cmd = cmd;
    memcpy(drm->data_buf, data, nbuf);
    drm->data_nbuf = nbuf;
    drm->data_tx_seq++;
    DRM_YIELD();
}

void DRM_poll(int rx_chan)
{
    if (rx_chan >= drm_info.drm_chan) return;
    
    drm_t *d = &DRM_SHMEM->drm[rx_chan];
    if (d->msg_rx_seq != d->msg_tx_seq) {
        //printf("%d %s\n", rx_chan, d->msg_buf);
        ext_send_msg_encoded(rx_chan, false, "EXT", "drm_status_cb", "%s", d->msg_buf);
        d->msg_rx_seq = d->msg_tx_seq;
    }
    
    if (d->data_rx_seq != d->data_tx_seq) {
        //printf("%d %s\n", rx_chan, d->msg_buf);
        ext_send_msg_data(rx_chan, false, d->data_cmd, d->data_buf, d->data_nbuf);
        d->data_rx_seq = d->data_tx_seq;
    }
}

#ifdef DRM_TEST_FILE
static s2_t *drm_mmap(const char *fn, int *words)
{
    int n;
    char *file;
    int fd;
    struct stat st;

    printf("DRM: mmap %s\n", fn);
    scall("drm open", (fd = open(fn, O_RDONLY)));
    scall("drm fstat", fstat(fd, &st));
    printf("DRM: size=%d\n", st.st_size);
    file = (char *) mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) sys_panic("DRM mmap");
    close(fd);

    *words = st.st_size / sizeof(s2_t);
    return (s2_t *) file;
}
#endif

void DRM_main();

ext_t DRM_ext = {
	"DRM",
	DRM_main,
	DRM_close,
	DRM_msgs,
	DRM_poll
};

void DRM_main()
{
    drm_t *d;
    #ifdef DRM_TEST_MULTI
        drm_info.drm_chan = is_BBAI? MAX_DRM_RX : 2;        //jks test BBG/B 2 DRM
    #else
        drm_info.drm_chan = is_BBAI? MAX_DRM_RX : 1;
    #endif
    
	for (int i=0; i < MAX_DRM_RX; i++) {
        d = &DRM_SHMEM->drm[i];
		memset(d, 0, sizeof(drm_t));
		d->init = true;
		
		DRM_CHECK(d->magic1 = 0xcafe; d->magic2 = 0xbabe;)
		d->rx_chan = i;
		d->info = &drm_info;

        #ifdef DRM_SHMEM_DISABLE
        #else
            // Needs to be done as separate Linux process per channel because DRM_loop() is
            // long-running and there would be no time-slicing otherwise, i.e. no NextTask() equivalent.
            shmem_ipc_setup(stprintf("kiwi.drm-%02d", i), SIG_IPC_DRM + i, DRM_loop);
        #endif

        #ifdef DRM_TEST_FILE
            d->tsamp = 0;
        #endif
    }
    
	ext_register(&DRM_ext);

#ifdef DRM_TEST_FILE
    int words;
    
    drm_info.s2p_start1 = drm_mmap(DIR_CFG "/samples/drm.test1.be12", &words);
    drm_info.s2p_end1 = drm_info.s2p_start1 + words;
    drm_info.tsamps1 = words / NIQ;

    drm_info.s2p_start2 = drm_mmap(DIR_CFG "/samples/drm.test2.be12", &words);
    drm_info.s2p_end2 = drm_info.s2p_start2 + words;
    drm_info.tsamps2 = words / NIQ;
#endif
}
