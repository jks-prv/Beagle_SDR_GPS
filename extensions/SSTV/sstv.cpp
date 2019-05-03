/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
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

#ifdef SSTV_FILE
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

    while (1) {
        printf("SSTV: sstv_task TOP\n");
        
        u1_t mode = sstv_get_vis(e);
        if (mode == 0) continue;
        printf("SSTV: VIS mode=%d\n", mode);
        
        SSTV_REAL initial_rate;
        #ifdef SSTV_FILE
            initial_rate = sstv.nom_rate;
        #else
            initial_rate = ext_update_get_sample_rateHz(rx_chan);
        #endif

        sstv_video_init(e, initial_rate, mode);
        bool Finished = sstv_video_get(e, 0, FALSE);
        
        if (Finished) {
            char fsk_id[20];
            fsk_id[0] = '\0';
            sstv_get_fsk(e, fsk_id);
            printf("SSTV: FSK ID \"%s\"\n", fsk_id);
            ext_send_msg_encoded(rx_chan, false, "EXT", "fsk_id", "%s", fsk_id);

        }

        if (Finished) {
            // Fix slant
            printf("SSTV: sync @ %.1f Hz\n", e->pic.Rate);
            ext_send_msg_encoded(rx_chan, false, "EXT", "status", "align image");
            e->pic.Rate = sstv_sync_find(e, &e->pic.Skip);

            // Final image  
            printf("SSTV: getvideo @ %.1f Hz, Skip %d, HedrShift %+d Hz\n", e->pic.Rate, e->pic.Skip, e->pic.HedrShift);
            sstv_video_get(e, e->pic.Skip, TRUE);
        }

        sstv_video_done(e);
        printf("SSTV: sstv_task DONE\n");
    }
}

void sstv_close(int rx_chan)
{
	sstv_chan_t *e = &sstv_chan[rx_chan];
    printf("SSTV: close task_created=%d\n", e->task_created);
	if (e->task_created) {
        printf("SSTV: TaskRemove\n");
		TaskRemove(e->tid);
		e->task_created = false;
	}
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

		ext_send_msg(e->rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}
	
	if (strcmp(msg, "SET start") == 0) {
		printf("SSTV: start\n");

        sstv_pcm_init(e);

        #ifdef SSTV_FILE
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

		ext_unregister_receive_real_samps_task(rx_chan);
		sstv_close(e->rx_chan);
        sstv_video_done(e);
        e->test = false;

        #ifdef SSTV_FILE
		    ext_unregister_receive_real_samps(rx_chan);
		#endif

		return true;
	}

	if (strcmp(msg, "SET test") == 0) {
		printf("SSTV: test\n");

        #ifdef SSTV_FILE
            e->s2p = e->s22p = sstv.s2p_start;
		#endif

		e->test = true;
        ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "test image");
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

#define SSTV_FILE_DIR "extensions/SSTV/"

#define SSTV_FN SSTV_FILE_DIR "s1.test.pattern.au"   // slanted, 25 ms
//#define SSTV_FN SSTV_FILE_DIR "m2.f5oql.FSK.au"   // bad pic
//#define SSTV_FN SSTV_FILE_DIR "s1.strange.au"     // 30 ms, slanted
//#define SSTV_FN SSTV_FILE_DIR "s2.au"   // stop bit 30 ms
//#define SSTV_FN SSTV_FILE_DIR "s2.f4cyh.FSK.au"
//#define SSTV_FN SSTV_FILE_DIR "m1.au"   // stop bit 25 ms

#ifdef SSTV_FILE
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
