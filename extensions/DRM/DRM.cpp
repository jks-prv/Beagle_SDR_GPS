// Copyright (c) 2017-2019 John Seamons, ZL4VO/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "DRM.h"
#include "mem.h"
#include "rx_util.h"

#ifdef DRM
    #include "DRM_main.h"
#else
    static void DRM_loop(int rx_chan) {}
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>

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
	rcprintf(rx_chan, "DRM drm_task calling DRM_loop() rx_chan=%d\n", rx_chan);
    DRM_loop(rx_chan);
    DRM_SHMEM->drm[rx_chan].tid = 0;
}

#endif

static void drm_pushback_file_data(int rx_chan, int instance, int nsamps, TYPECPX *samps)
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

bool DRM_msgs(char *msg, int rx_chan)
{
	rcprintf(rx_chan, "DRM enable=%d <%s>\n", DRM_enable, msg);
	if (rx_chan == -1) {
	    printf("DRM rx_chan == -1?\n");
        dump();
	    return true;
	}

    drm_t *d = &DRM_SHMEM->drm[rx_chan];
    if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, false, "EXT ready");
		
		// extension notified of rx_chan >= drm_info.drm_chan in response to lock_set below
		if (rx_chan < drm_info.drm_chan) {
            d->rx_chan = rx_chan;	// remember our receiver channel number
        }
        
        return true;
    }

    conn_t *conn = rx_channels[rx_chan].conn;
	if (conn == NULL) {
	    rcprintf(rx_chan, "DRM conn == NULL?\n");
        dump();
	    return true;
	}
	rcprintf(rx_chan, "DRM conn=%p isLocal=%d\n", conn, conn->isLocal);
	
	// stop any non-Kiwi API attempts to run DRM if it's disabled and connection is not local
	if (!DRM_enable && conn && !conn->isLocal) {
	    rcprintf(rx_chan, "DRM not enabled, kicked!\n");
	    ext_kick(rx_chan);
        return true;
	}
	
	#define DRM_HACKED              -2
	#define DRM_MODE_NOT_SUPPORTED  -1
	#define DRM_CONFLICT            0
	#define DRM_OK_LOCKED           1

    if (strcmp(msg, "SET lock_set") == 0) {
        int rv, heavy = 0;
        int inuse = rx_chans - rx_chan_free_count(RX_COUNT_ALL, NULL, &heavy);

    //#define DRM_12KHZ_ONLY
    #ifdef DRM_12KHZ_ONLY
        if (snd_rate != SND_RATE_4CH) {
            printf("DRM only works 12 kHz mode configurations currently\n");
            rv = DRM_MODE_NOT_SUPPORTED;
        } else
    #endif
        if (!DRM_enable && !conn->isLocal) {
            rv = DRM_HACKED;    // prevent attempt to bypass the javascript kiwi.is_local check
        } else {
            int prev = is_locked;
            if (is_multi_core) {
                is_locked = (rx_chan < drm_info.drm_chan)? 1:0;
                printf("DRM multi-core lock_set: inuse=%d heavy=%d rx_chan=%d drm_chan=%d prev=%d locked=%d\n",
                    inuse, heavy, rx_chan, drm_info.drm_chan, prev, is_locked);
            } else {
                // inuse-1 to not count DRM channel
                is_locked = ((inuse-1) <= drm_nreg_chans && heavy == 0 && rx_chan < drm_info.drm_chan)? 1:0;
                if (conn->is_locked)
                    printf("DRM conn->is_locked was already set?\n");
                printf("DRM single-core lock_set: inuse=%d(%d) heavy=%d prev=%d locked=%d\n", inuse, inuse-1, heavy, prev, is_locked);
            }
            if (is_locked) conn->is_locked = true;
            rv = is_locked? DRM_OK_LOCKED : DRM_CONFLICT;
        }
        
		if (rv == DRM_OK_LOCKED) {
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
		
        printf("DRM FINAL: inuse=%d heavy=%d locked=%d\n", inuse, heavy, rv);
		ext_send_msg(rx_chan, false, "EXT inuse=%d heavy=%d locked=%d", inuse, heavy, rv);
		
        return true;    
    }
    
    if (strcmp(msg, "SET lock_clear") == 0) {
        is_locked = 0;
        DRM_close(rx_chan);
        printf("DRM lock_clear\n");
        return true;    
    }
    
    int run = 0;
    if (sscanf(msg, "SET run=%d", &run) == 1) {
        d->run = run;
        d->dbgUs = kiwi.dbgUs;
        for (int i = 0; i < 4; i++) d->p_i[i] = p_i[i];
        printf("DRM run=%d rx_chan=%d dbgUs=%d p_i[0]=%d\n", run, rx_chan, d->dbgUs, d->p_i[0]);
        return true;
    }

    int test = 0;
    if (sscanf(msg, "SET test=%d", &test) == 1) {
        printf("DRM test=%d rx_chan=%d\n", test, rx_chan);
        d->test = test;
        if (drm_info.s2p_start1 == NULL || drm_info.s2p_start2 == NULL) {
            d->test = 0;
            return true;
        }

        if (d->test) {
            d->s2p = ((d->test == 1)? d->info->s2p_start1 : d->info->s2p_start2);
            d->tsamp = 0;
            
            // misuse ext_register_receive_iq_samps() to pushback audio samples from the test file
            ext_register_receive_iq_samps(drm_pushback_file_data, rx_chan);
        } else {
            ext_unregister_receive_iq_samps(rx_chan);
        }
        return true;
    }
    
    int lpf = 0;
    if (sscanf(msg, "SET lpf=%d", &lpf) == 1) {
        d->use_LPF = lpf;
        rcprintf(rx_chan, "DRM lpf=%d rx_chan=%d\n", lpf);
        return true;
    }

    int svc = 0;
    if (sscanf(msg, "SET svc=%d", &svc) == 1) {
        d->audio_service = svc - 1;
        return true;
    }
    
    int objID = 0;
    if (sscanf(msg, "SET journaline_objID=%d", &objID) == 1) {
        printf("DRM journaline_objID=0x%x\n", objID);
        d->journaline_objID = objID;
        d->journaline_objSet = true;
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
        d->monitor = monitor;
        return true;
    }
    
    if (strcmp(msg, "SET reset") == 0) {
        d->reset = true;
        d->i_epoch = d->i_samples = d->i_tsamples = 0;
        d->no_input = d->sent_silence = 0;
        d->journaline_objID = 0;
        d->journaline_objSet = true;
        return true;    
    }
    
    return false;
}

int DRM_rx_chan()
{
    drm_t *drm = &DRM_SHMEM->drm[(int) FROM_VOID_PARAM(TaskGetUserParam())];
    return drm->rx_chan;
}

// pass msg & data via shared mem buf since DRM_SHMEM might be enabled
void DRM_msg_encoded(drm_msg_e msg_type, const char *cmd, kstr_t *ks)
{
    drm_t *drm = &DRM_SHMEM->drm[(int) FROM_VOID_PARAM(TaskGetUserParam())];
    int slen = strlen(kstr_sp(ks));
    if (slen >= L_MSGBUF)
        printf("WARNING DRM_msg_encoded: msg_type=%d len=%d/%d\n", msg_type, slen, L_MSGBUF);
    kiwi_strncpy(drm->msg_cmd[msg_type], cmd, L_MSGCMD);
    kiwi_strncpy(drm->msg_buf[msg_type], kstr_sp(ks), L_MSGBUF);
    drm->msg_tx_seq[msg_type]++;
    DRM_YIELD();
}

void DRM_data(u1_t cmd, u1_t *data, u4_t nbuf)
{
    drm_t *drm = &DRM_SHMEM->drm[(int) FROM_VOID_PARAM(TaskGetUserParam())];
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
    
    for (int i = 0; i < N_MSGBUF; i++) {
        if (d->msg_rx_seq[i] != d->msg_tx_seq[i]) {
            //printf("%d %s=%s\n", rx_chan, d->msg_cmd[i], d->msg_buf[i]);
            ext_send_msg_encoded(rx_chan, false, "EXT", d->msg_cmd[i], "%s", d->msg_buf[i]);
            d->msg_rx_seq[i] = d->msg_tx_seq[i];
        }
    }
    
    if (d->data_rx_seq != d->data_tx_seq) {
        ext_send_msg_data(rx_chan, false, d->data_cmd, d->data_buf, d->data_nbuf);
        d->data_rx_seq = d->data_tx_seq;
    }
    
    #if 0
        conn_t *conn = rx_channels[rx_chan].conn;
        if (conn->ident_user) {
            static u4_t last_journaline_objID;
            int objID;
            if (sscanf(conn->ident_user, "%x", &objID) == 1 && objID != last_journaline_objID) {
                d->journaline_objID = objID;
                d->journaline_objSet = true;
                last_journaline_objID = objID;
                printf("DRM journaline_objID=0x%x\n", objID);
            }
        }
    #endif
}

static s2_t *drm_mmap(char *fn, int *words)
{
    int n;
    char *file;
    int fd;
    struct stat st;

    printf("DRM: mmap %s\n", fn);
    if ((fd = open(fn, O_RDONLY)) < 0) {
        printf("DRM: open failed\n");
        return NULL;
    }
    scall("drm fstat", fstat(fd, &st));
    printf("DRM: size=%d\n", st.st_size);
    file = (char *) mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (file == MAP_FAILED) {
        printf("DRM: mmap failed\n");
        return NULL;
    }
    *words = st.st_size / sizeof(s2_t);
    return (s2_t *) file;
}

void DRM_main();

ext_t DRM_ext = {
	"DRM",
	DRM_main,
	DRM_close,
	DRM_msgs,
	EXT_NEW_VERSION,
	EXT_NO_FLAGS,       // don't set EXT_FLAGS_HEAVY for ourselves
	DRM_poll
};

void DRM_main()
{
    drm_t *d;
    drm_info.drm_chan = DRM_MAX_RX;
    
    #ifdef DRM
        for (int i=0; i < DRM_MAX_RX; i++) {
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

            d->tsamp = 0;
        }
    
        ext_register(&DRM_ext);

        int words;
        const char *fn;
        char *fn2;
    
        fn = cfg_string("DRM.test_file1", NULL, CFG_OPTIONAL);
        if (!fn || *fn == '\0') return;
        asprintf(&fn2, "%s/samples/%s", DIR_CFG, fn);
        cfg_string_free(fn);
        drm_info.s2p_start1 = drm_mmap(fn2, &words);
        kiwi_asfree(fn2);
        if (drm_info.s2p_start1 == NULL) return;
        drm_info.s2p_end1 = drm_info.s2p_start1 + words;
        drm_info.tsamps1 = words / NIQ;

        fn = cfg_string("DRM.test_file2", NULL, CFG_OPTIONAL);
        if (!fn || *fn == '\0') return;
        asprintf(&fn2, "%s/samples/%s", DIR_CFG, fn);
        cfg_string_free(fn);
        drm_info.s2p_start2 = drm_mmap(fn2, &words);
        kiwi_asfree(fn2);
        if (drm_info.s2p_start2 == NULL) return;
        drm_info.s2p_end2 = drm_info.s2p_start2 + words;
        drm_info.tsamps2 = words / NIQ;
    #else
        printf("ext_register: \"DRM\" not configured\n");
        return;
    #endif
}
