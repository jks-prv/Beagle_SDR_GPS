/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 * Copyright (c) 2007-2013, Oona Räisänen (OH2EIQ [at] sral.fi)
 */

#include "sstv.h"

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()
#include "kiwi.h"

#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own sstv_chan[] data structure.

sstv_t sstv;
sstv_chan_t sstv_chan[MAX_RX_CHANS];

void sstv_process(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps)
{
	sstv_chan_t *e = &sstv_chan[rx_chan];
}

#ifdef SSTV_TEST_FILE
static void sstv_file_data(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps)
{
    sstv_chan_t *e = &sstv_chan[rx_chan];
    if (!e->test || e->s2p >= sstv.s2p_end) return;
    
    for (int i = 0; i < nsamps; i++) {
        u2_t t = (u2_t) *e->s2p;
        if (e->s2p < sstv.s2p_end) *samps++ = (s2_t) FLIP16(t);
        e->s2p++;
    }
}
#endif

static void sstv_task(void *param)
{
    int rx_chan = (int) FROM_VOID_PARAM(param);
    sstv_chan_t *e = &sstv_chan[rx_chan];
    e->state = INIT;
    
    while (1) {
        printf("SSTV: sstv_task TOP\n");
        if (e->reset) {
            ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "reset");
            e->reset = false;
        }
        
        u1_t mode = sstv_get_vis(e);
        if (mode == UNKNOWN) continue;
        printf("SSTV: VIS mode=%d\n", mode);
        e->state = BUSY;
        
        SSTV_REAL initial_rate;
        if (e->test)
            initial_rate = SSTV_TEST_FILE_RATE;
        else
            initial_rate = ext_update_get_sample_rateHz(rx_chan);

        // delay release of buffers from previous image in case of manual shift adjustment
        sstv_video_done(e);
        sstv_video_init(e, initial_rate, mode);
        sstv_video_get(e, 0, false);
        
        char fsk_id[20];
        fsk_id[0] = '\0';
        if (!e->reset) {
            sstv_get_fsk(e, fsk_id);
            printf("SSTV: FSK ID \"%s\"\n", fsk_id);
        }

        // Fix slant
        if (!e->noadj && !e->reset) {
            printf("SSTV: INITIAL sync @ %.1f Hz, Skip %d\n", e->pic.Rate, e->pic.Skip);
            e->pic.Rate = sstv_sync_find(e, &e->pic.Skip);
    
            // Final image  
            printf("SSTV: getvideo @ %.1f Hz, Skip %d, HeaderShift %+d Hz\n", e->pic.Rate, e->pic.Skip, e->pic.HeaderShift);
            sstv_video_get(e, e->pic.Skip, true);
        } else {
            e->pic.Skip = 0;
        }
        
        printf("SSTV: sstv_task DONE\n");
        e->state = DONE;
    }
}

void sstv_close(int rx_chan)
{
	sstv_chan_t *e = &sstv_chan[rx_chan];
    printf("SSTV: close task_created=%d\n", e->task_created);

    ext_unregister_receive_real_samps_task(e->rx_chan);
    
    #ifdef SSTV_TEST_FILE
        ext_unregister_receive_real_samps(e->rx_chan);
    #endif

	if (e->task_created) {
        printf("SSTV: TaskRemove\n");
		TaskRemove(e->tid);
		e->task_created = false;
	}

    sstv_video_done(e);
}

bool sstv_msgs(char *msg, int rx_chan)
{
	sstv_chan_t *e = &sstv_chan[rx_chan];
	int i, n;
    char *cmd_p;
	
	//printf("### sstv_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
	    memset(e, 0, sizeof(*e));
		e->rx_chan = rx_chan;	// remember our receiver channel number

        e->fft.in = SSTV_FFTW_ALLOC_REAL(2048);
        assert(e->fft.in != NULL);
        e->fft.out = SSTV_FFTW_ALLOC_COMPLEX(2048);
        assert(e->fft.out != NULL);
        memset(e->fft.in, 0, sizeof(SSTV_REAL) * 2048);
        e->fft.Plan1024 = SSTV_FFTW_PLAN_DFT_R2C_1D(1024, e->fft.in, e->fft.out, FFTW_ESTIMATE);
        e->fft.Plan2048 = SSTV_FFTW_PLAN_DFT_R2C_1D(2048, e->fft.in, e->fft.out, FFTW_ESTIMATE);

        sstv_pcm_once(e);
        sstv_video_once(e);
        sstv_sync_once(e);
        
		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}
	
	if (strcmp(msg, "SET start") == 0) {
		printf("SSTV: start\n");

        sstv_pcm_init(e);

        #ifdef SSTV_TEST_FILE
            e->s2p = e->s22p = sstv.s2p_start;
		    ext_register_receive_real_samps(sstv_file_data, rx_chan);
		#endif

        if (!e->task_created) {
			e->tid = CreateTaskF(sstv_task, TO_VOID_PARAM(rx_chan), EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL), 0);
            e->task_created = true;
        }
		
		ext_register_receive_real_samps_task(e->tid, rx_chan);
		return true;
	}

	if (strcmp(msg, "SET stop") == 0) {
		printf("SSTV: stop\n");

		sstv_close(e->rx_chan);
        e->test = false;
		return true;
	}
	
	int x0, y0, x1, y1;
	if (sscanf(msg, "SET shift x0=%d y0=%d x1=%d y1=%d", &x0, &y0, &x1, &y1) == 4) {
	    printf("SSTV: xy %d,%d -> %d,%d\n", x0, y0, x1, y1);
	    if (e->state != DONE) return true;

        ModeSpec_t  *m = e->pic.modespec;
        SSTV_REAL x = x1;
        SSTV_REAL y = y1 / m->LineHeight;
        SSTV_REAL dx = x - x0;
        SSTV_REAL dy = y - (y0 / m->LineHeight);

        // Adjust sample rate, if in sensible limits
        SSTV_REAL newrate = e->pic.Rate + e->pic.Rate * (dx * m->PixelTime) / (dy * m->LineHeight * m->LineTime);
        if (newrate < sstv.nom_rate - (sstv.nom_rate/4) || newrate > sstv.nom_rate + (sstv.nom_rate/4)) return true;
        e->pic.Rate = newrate;

        // Find x-intercept and adjust skip
        SSTV_REAL xic = SSTV_MFMOD((x - (y / (dy/dx))), m->ImgWidth);
        printf("SSTV: manual adjust xic %.3f\n", xic);

        if (xic < 0) {
            xic = m->ImgWidth - xic;
            printf("SSTV: manual adjust xic < 0 %.3f\n", xic);
        }

        SSTV_REAL line_samps = m->LineTime * e->pic.Rate;
        SSTV_REAL pixel_samps = m->PixelTime * e->pic.Rate;
        SSTV_REAL xic_samps = xic * pixel_samps;
        int skip_new = SSTV_MFMOD(e->pic.Skip + xic_samps, line_samps);
        printf("SSTV: manual adjust Skip %d = old %d + xic_samps %.1f\n", skip_new, e->pic.Skip, xic_samps);
        e->pic.Skip = skip_new;

        SSTV_REAL img_width_samps = m->ImgWidth * pixel_samps;
        if (e->pic.Skip > img_width_samps / 2.0) {
            e->pic.Skip -= img_width_samps;
            printf("SSTV: manual adjust (Skip > %d) => %d\n", (int) (img_width_samps / 2.0), e->pic.Skip);
        }

        printf("SSTV: manual adjust @ %.1f Hz, Skip %d\n", e->pic.Rate, e->pic.Skip);
        ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "%s, manual adjust, Fs %.1f, header %+d Hz, skip %d pixels",
            m->ShortName, e->pic.Rate, e->pic.HeaderShift, e->pic.Skip);
        sstv_video_get(e, e->pic.Skip, true);

	    return true;
	}

	if (strcmp(msg, "SET reset") == 0) {
		printf("SSTV: reset\n");
		e->reset = true;
		e->test = false;
		return true;
	}
	
	if (strcmp(msg, "SET test") == 0) {
		printf("SSTV: test\n");

        #ifdef SSTV_TEST_FILE
            e->s2p = e->s22p = sstv.s2p_start;
		#endif

		e->test = true;
        ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "test image");
		return true;
	}
	
	if (strcmp(msg, "SET noadj") == 0) {
		printf("SSTV: noadj\n");
		e->noadj = true;
		return true;
	}
	
	return false;
}

void SSTV_main();

ext_t sstv_ext = {
	"SSTV",
	SSTV_main,
	sstv_close,
	sstv_msgs
};

void SSTV_main() {

    ext_register(&sstv_ext);
    sstv.nom_rate = snd_rate;

#define SSTV_TEST_FILE_DIR "extensions/SSTV/"

#define SSTV_FN SSTV_TEST_FILE_DIR "s1.test.pattern.au"   // slanted, 25 ms
//#define SSTV_FN SSTV_TEST_FILE_DIR "m2.f5oql.FSK.au"   // bad pic
//#define SSTV_FN SSTV_TEST_FILE_DIR "s1.strange.au"     // 30 ms, slanted
//#define SSTV_FN SSTV_TEST_FILE_DIR "s2.test.pattern.au"    // stop bit 30 ms
//#define SSTV_FN SSTV_TEST_FILE_DIR "s2.f4cyh.FSK.au"
//#define SSTV_FN SSTV_TEST_FILE_DIR "m1.au"   // stop bit 25 ms
//#define SSTV_FN SSTV_TEST_FILE_DIR "r24.test.pattern.au"
//#define SSTV_FN SSTV_TEST_FILE_DIR "r36.test.pattern.au"
//#define SSTV_FN SSTV_TEST_FILE_DIR "r36.color.bars.au"
//#define SSTV_FN SSTV_TEST_FILE_DIR "r72.test.pattern.au"

#ifdef SSTV_TEST_FILE
    int n, words;
    char *file;
    
    printf("SSTV: load " SSTV_FN "\n");

    #if 0
        int fd;
        scall("sstv open", (fd = open(SSTV_FN, O_RDONLY)));
        struct stat st;
        scall("sstv fstat", fstat(fd, &st));
        printf("SSTV: size=%d\n", st.st_size);
        file = (char *) malloc(st.st_size);
        assert(file != NULL);
        scall("sstv read", (n = read(fd, file, st.st_size)));
        words = st.st_size/2;
    #else
        size_t size;
        extern const char *edata_always2(const char *, size_t *);
        file = (char *) edata_always2(SSTV_FN, &size);
        assert(file != NULL);
        words = size/2;
    #endif

    sstv.s2p_start = (s2_t *) file;
    sstv.s2p_end = sstv.s2p_start + words;
#endif
}
