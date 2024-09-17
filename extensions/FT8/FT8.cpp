// Copyright (c) 2024 John Seamons, ZL4VO/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "coroutines.h"
#include "conn.h"
#include "rx_util.h"
#include "data_pump.h"
#include "mem.h"
#include "misc.h"
#include "wspr.h"
#include "FT8.h"
#include "PSKReporter.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <sys/mman.h>

// order matches "cfg value order" FT8.js::ft8.menu_i_to_cfg_i (NOT "menu order" ft8.autorun_u)
// only add new entries to the end so as not to disturb existing values stored in config
#define N_FREQ 30
static double ft8_autorun_dial[N_FREQ] = {      // usb carrier/dial freq
    /* FT8 */ 1840, 3573, 5357, 7074,   10136, 14074, 18100, 21074, 24915, 28074,   // 1-10
    /* FT4 */       3575.5,     7047.5, 10140, 14080, 18104, 21140, 24919, 28180,   // 11-18
    /* FT8 */ 40680, 50313, 50323, 70154, 70190, 144174, 222065, 432174, 1296174,   // 19-27
    /* FT4 */ 50318, 144150,    // 28-29
    /* FT8 */ 10489540          // 30
};

// must map to above entries
static u1_t isFT4[N_FREQ] = { 0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0,0, 1,1, 0 };

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own ft8[] data structure.

typedef struct {
	int rx_chan;
	int run;
	int proto;
	bool debug;
	int last_freq_kHz;
	
	bool task_created;
	tid_t tid;
	int rd_pos;
	bool seq_init;
	u4_t seq;

	// autorun
	bool autorun;
	int instance;
	conn_t *arun_csnd, *arun_cext;
	double arun_dial_freq_kHz;
    int spot_count;

	bool test;
	u1_t start_test;
    s2_t *s2p;
} ft8_t;

static ft8_t ft8[MAX_RX_CHANS];

ft8_conf_t ft8_conf;

typedef struct {
    int num_autorun;
    char *rcall;
    latLon_t r_loc;
	bool syslog, spot_log;
} ft8_conf2_t;

static ft8_conf2_t ft8_conf2;

static int ft8_arun_band[MAX_ARUN_INST];
static int ft8_arun_preempt[MAX_ARUN_INST];
static bool ft8_arun_seen[MAX_ARUN_INST];
static internal_conn_t iconn[MAX_ARUN_INST];

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
    decode_ft8_setup(rx_chan, e->debug);
    
	while (1) {
		TaskSleepReason("wait for wakeup");
		
		// Only call decode_ft8_protocol() when rounded freq changes >= 1 kHz
		// Decoder can cope with smaller freq changes.
		int new_freq_kHz = (int) round(conn->freqHz/1e3);
		if (e->last_freq_kHz != new_freq_kHz) {
		    //rcprintf(rx_chan, "FT8: freq changed %d => %d\n", e->last_freq_kHz, new_freq_kHz);
		    e->last_freq_kHz = new_freq_kHz;
		    decode_ft8_protocol(rx_chan, ft8_conf.freq_offset_Hz + conn->freqHz, e->proto);
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
		    ft8_conf.test = e->test;
		    decode_ft8_samples(rx_chan, &rx->real_samples_s2[e->rd_pos][0], FASTFIR_OUTBUF_SIZE, conn->freqHz, &e->start_test);
			e->rd_pos = (e->rd_pos+1) & (N_DPBUF-1);
		}
    }
}

void ft8_reset(ft8_t *e)
{
    memset(e, 0, sizeof(*e));
}

void ft8_close(int rx_chan)
{
    rx_util_t *r = &rx_util;
	ft8_t *e = &ft8[rx_chan];
    //rcprintf(rx_chan, "FT8: close rx_chan=%d autorun=%d arun_which=%d task_created=%d\n",
    //    rx_chan, e->autorun, r->arun_which[rx_chan], e->task_created);
    ext_unregister_receive_real_samps_task(rx_chan);

	if (e->autorun) {
        internal_conn_shutdown(&iconn[e->instance]);
	}

	if (e->task_created) {
        //rcprintf(rx_chan, "FT8: TaskRemove\n");
		TaskRemove(e->tid);
		e->task_created = false;
	}
	
	decode_ft8_free(rx_chan);
    ft8_reset(e);
    r->arun_which[rx_chan] = ARUN_NONE;
}

bool ft8_msgs(char *msg, int rx_chan)
{
	ft8_t *e = &ft8[rx_chan];
    e->rx_chan = rx_chan;   // remember our receiver channel number
	int n;
	
	//rcprintf(rx_chan, "### ft8_msgs <%s>\n", msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

    int proto;
	if (sscanf(msg, "SET ft8_start=%d", &proto) == 1) {
	    e->debug = kiwi.dbgUs;
	    e->proto = proto;
        conn_t *conn = rx_channels[rx_chan].conn;
		e->last_freq_kHz = conn->freqHz/1e3;
        ft8_conf.freq_offset_Hz = freq.offset_Hz;
		//rcprintf(rx_chan, "FT8 start %s\n", proto? "FT4" : "FT8");
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
        conn_t *conn = rx_channels[rx_chan].conn;
		e->last_freq_kHz = conn->freqHz/1e3;
		//rcprintf(rx_chan, "FT8 protocol %s freq %.2f\n", proto? "FT4" : "FT8", conn->freqHz/1e3);
		decode_ft8_protocol(rx_chan, ft8_conf.freq_offset_Hz + conn->freqHz, proto? 1:0);
		return true;
	}

	float df;
	n = sscanf(msg, "SET dialfreq=%f", &df);
	if (n == 1) {
		e->arun_dial_freq_kHz = df;
		//rcprintf(rx_chan, "FT8 autorun: dial_freq_kHz=%.6f\n", e->arun_dial_freq_kHz);
		return true;
	}

	if (strcmp(msg, "SET autorun") == 0) {
	    e->autorun = true;
	    return true;
	}

	if (strcmp(msg, "SET ft8_close") == 0) {
		//rcprintf(rx_chan, "FT8 close\n");
		ft8_close(rx_chan);
		return true;
	}
	
	if (strcmp(msg, "SET ft8_test") == 0) {
		//rcprintf(rx_chan, "FT8 test\n");
		e->start_test = 0;
        e->s2p = ft8_conf.s2p_start;
		e->test = true;
		return true;
	}

	return false;
}

void ft8_update_rgrid(char *rgrid)
{
    kiwi_strncpy(ft8_conf.rgrid, rgrid, LEN_GRID);
    //printf("ft8_conf.rgrid %s\n", ft8_conf.rgrid);
    
    // update grid shown in user lists when there haven't been any new spots recently
    for (int ch = 0; ch < MAX_RX_CHANS; ch++) {
        if (ft8[ch].autorun)
            ft8_update_spot_count(ch, 0);
    }
}

// catch changes to reporter call/grid from admin page FT8 config (also called during initialization)
bool ft8_update_vars_from_config(bool called_at_init_or_restart)
{
    int i, n;
    rx_util_t *r = &rx_util;
    bool update_cfg = false;
    char *s;
    
    cfg_default_object("ft8", "{}", &update_cfg);
    
    // Changing reporter call on admin page requires restart. This is because of
    // conditional behavior at startup, e.g. uploads enabled because valid call is now present
    // or autorun tasks starting for the same reason.
    // ft8_conf2.rcall is still updated here to handle the initial assignment and
    // manual changes from FT8 admin page.
    //
    // Also, first-time init of FT8 reporter call/grid from WSPR values
    
    s = (char *) cfg_string("WSPR.callsign", NULL, CFG_REQUIRED);
    cfg_default_string("ft8.callsign", s, &update_cfg);
	cfg_string_free(s);
    s = (char *) cfg_string("ft8.callsign", NULL, CFG_REQUIRED);
    kiwi_ifree(ft8_conf2.rcall, "ft8 rcall");
	ft8_conf2.rcall = kiwi_str_encode(s);
	cfg_string_free(s);

    s = (char *) cfg_string("WSPR.grid", NULL, CFG_REQUIRED);
    cfg_default_string("ft8.grid", s, &update_cfg);
	cfg_string_free(s);
    s = (char *) cfg_string("ft8.grid", NULL, CFG_REQUIRED);
	kiwi_strncpy(ft8_conf.rgrid, s, LEN_GRID);
    wspr_set_latlon_from_grid(s);
	cfg_string_free(s);
	grid_to_latLon(ft8_conf.rgrid, &ft8_conf2.r_loc);
	if (ft8_conf2.r_loc.lat != 999.0)
		latLon_deg_to_rad(ft8_conf2.r_loc);
    
    // Make sure ft8.autorun holds *correct* count of non-preemptable autorun processes.
    // For the benefit of refusing enable of public listing if there are no non-preemptable autoruns.
    // If Kiwi was previously configured for a larger rx_chans, and more than rx_chans worth
    // of autoruns were enabled, then with a reduced rx_chans it is essential not to count
    // the ones beyond the rx_chans limit. That's why "i < rx_chans" appears below and
    // not MAX_RX_CHANS.
    if (called_at_init_or_restart) {
        int num_autorun = 0, num_non_preempt = 0;
        for (int instance = 0; instance < rx_chans; instance++) {
            int autorun = cfg_default_int(stprintf("ft8.autorun%d", instance), 0, &update_cfg);
            int preempt = cfg_default_int(stprintf("ft8.preempt%d", instance), 0, &update_cfg);
            //cfg_default_int(stprintf("ft8.start%d", instance), 0, &update_cfg);
            //cfg_default_int(stprintf("ft8.stop%d", instance), 0, &update_cfg);
            //printf("ft8.autorun%d=%d(band=%d) ft8.preempt%d=%d\n", instance, autorun, autorun-1, instance, preempt);
            if (autorun) num_autorun++;
            if (autorun && (preempt == 0)) num_non_preempt++;
            ft8_arun_band[instance] = autorun;
            ft8_arun_preempt[instance] = preempt;
        }
        if (snd_rate != MIN_SND_RATE) {
            printf("FT8 autorun: Only works on Kiwis configured for 12 kHz wide channels\n");
            num_autorun = num_non_preempt = 0;
        }
        if (ft8_conf2.rcall == NULL || *ft8_conf2.rcall == '\0' || ft8_conf.rgrid[0] == '\0') {
            printf("FT8 autorun: reporter callsign and grid square fields must be entered on FT8 section of admin page\n");
            num_autorun = num_non_preempt = 0;
        }
        ft8_conf2.num_autorun = num_autorun;
        cfg_update_int("ft8.autorun", num_non_preempt, &update_cfg);
        //printf("FT8 autorun: num_autorun=%d ft8.autorun=%d(non-preempt) rx_chans=%d\n", num_autorun, num_non_preempt, rx_chans);
    }

    ft8_conf.SNR_adj = cfg_default_int("ft8.SNR_adj", -22, &update_cfg);
    if (ft8_conf.SNR_adj == 0) {    // update to new default
        cfg_set_int("ft8.SNR_adj", -22);
        update_cfg = true;
    }
    ft8_conf.dT_adj = cfg_default_int("ft8.dT_adj", -1, &update_cfg);
    if (ft8_conf.dT_adj == 0) {     // update to new default
        cfg_set_int("ft8.dT_adj", -1);
        update_cfg = true;
    }

    ft8_conf.GPS_update_grid = cfg_default_bool("ft8.GPS_update_grid", false, &update_cfg);
    ft8_conf2.syslog = ft8_conf.syslog = cfg_default_bool("ft8.syslog", false, &update_cfg);
    ft8_conf2.spot_log = cfg_default_bool("ft8.spot_log", false, &update_cfg);

	//printf("ft8_update_vars_from_config: rcall <%s> ft8_conf.rgrid=<%s> ft8_conf.GPS_update_grid=%d\n", ft8_conf2.rcall, ft8_conf.rgrid, ft8_conf.GPS_update_grid);
    return update_cfg;
}

void ft8_update_spot_count(int rx_chan, u4_t spot_count)
{
    ft8_t *e = &ft8[rx_chan];
    if (e->autorun) {
        if (spot_count == 0) spot_count = e->spot_count;
        const char *pre = e->arun_csnd->arun_preempt? (ft8_conf.GPS_update_grid? ",%20pre" : ",%20preemptable") : "";
        const char *rgrid = ft8_conf.GPS_update_grid? stprintf(",%%20%s", ft8_conf.rgrid) : "";
        input_msg_internal(e->arun_csnd, (char *) "SET geoloc=%d%%20decoded%s%s", spot_count, pre, rgrid);
        e->spot_count = spot_count;
    }
}

static void ft8_autorun(int instance, bool initial)
{
    rx_util_t *r = &rx_util;
    int band = ft8_arun_band[instance]-1;
    double dial_freq_kHz = ft8_autorun_dial[band];
    double if_freq_kHz = dial_freq_kHz - freq.offset_kHz;
    bool ft4 = (isFT4[band] != 0);

	if (!rx_freq_inRange(dial_freq_kHz)) {
	    if (!ft8_arun_seen[instance]) {
            printf("FT%d autorun: ERROR band=%d dial_freq_kHz %.2f is outside rx range %.2f - %.2f\n",
                ft4? 4:8, band, dial_freq_kHz, freq.offset_kHz, freq.offmax_kHz);
	        ft8_arun_seen[instance] = true;
	    }
	    return;
	}

    bool preempt = (ft8_arun_preempt[instance] != ARUN_PREEMPT_NO);
    char *ident_user;
    asprintf(&ident_user, "FT%d-autorun", ft4? 4:8);
    char *geoloc;
    const char *pre = preempt? (ft8_conf.GPS_update_grid? ",%20pre" : ",%20preemptable") : "";
    const char *rgrid = ft8_conf.GPS_update_grid? stprintf(",%%20%s", ft8_conf.rgrid) : "";
    printf("ft8_autorun preempt=%d rgrid=%s GPS_update_grid=%d\n", preempt, ft8_conf.rgrid, ft8_conf.GPS_update_grid);
    asprintf(&geoloc, "0%%20decoded%s%s", pre, rgrid);

	bool ok = internal_conn_setup(ICONN_WS_SND | ICONN_WS_EXT, &iconn[instance], instance, PORT_BASE_INTERNAL_FT8,
	    WS_FL_IS_AUTORUN | (initial? WS_FL_INITIAL : 0),
        "usb", FT8_PASSBAND_LO, FT8_PASSBAND_HI, if_freq_kHz, ident_user, geoloc, "FT8");
    free(ident_user); free(geoloc);
    if (!ok) {
        //printf("FT8 autorun: internal_conn_setup() FAILED instance=%d band=%d %s %.2f\n",
	    //    instance, band, ft4? "FT4" : "FT8", dial_freq_kHz);
        return;
    }

    conn_t *csnd = iconn[instance].csnd;
    csnd->arun_preempt = preempt;
    int rx_chan = csnd->rx_channel;
    r->arun_which[rx_chan] = ARUN_FT8;
    r->arun_band[rx_chan] = band;
    ft8_t *e = &ft8[rx_chan];
    ft8_reset(e);
    e->instance = instance;
    e->rx_chan = rx_chan;
    e->arun_csnd = csnd;
    e->arun_dial_freq_kHz = dial_freq_kHz;

	clprintf(csnd, "FT8 autorun: START instance=%d rx_chan=%d band=%d %s %.2f preempt=%d\n",
	    instance, rx_chan, band, ft4? "FT4" : "FT8", dial_freq_kHz, preempt);
	
    conn_t *cext = iconn[instance].cext;
    e->arun_cext = cext;
    input_msg_internal(cext, (char *) "SET autorun");
    input_msg_internal(cext, (char *) "SET dialfreq=%.2f", dial_freq_kHz);
    input_msg_internal(cext, (char *) "SET ft8_start=%d", ft4? 1:0);    // ext task created here
}

void ft8_autorun_start(bool initial)
{
    rx_util_t *r = &rx_util;
    if (ft8_conf2.num_autorun == 0) {
        //printf("FT8 autorun_start: none configured\n");
        return;
    }

    for (int instance = 0; instance < rx_chans; instance++) {
        int band = ft8_arun_band[instance];
        if (band == ARUN_REG_USE) continue;     // "regular use" menu entry
        band--;     // make array index
        
        // Is this instance already running on any channel?
        // This loop should never exclude ft8_autorun() when called from ft8_autorun_restart()
        // because all instances were just stopped.
        // When called from rx_autorun_restart_victims() it functions normally.
        int rx_chan;
        for (rx_chan = 0; rx_chan < rx_chans; rx_chan++) {
            if (r->arun_which[rx_chan] == ARUN_FT8 && r->arun_band[rx_chan] == band) {
                //printf("FT8 autorun: instance=%d band=%d %.2f already running on rx%d\n",
                //    instance, band, ft8_autorun_dial[band], rx_chan);
                break;
            }
        }
        if (rx_chan == rx_chans) {
            // arun_{which,band} set only after ft8_autorun():internal_conn_setup() succeeds
            //printf("FT8 autorun: instance=%d band=%d %.2f START rx%d\n",
            //    instance, band, ft8_autorun_dial[band], rx_chan);
            ft8_autorun(instance, initial);
        }
    }

    ft8_conf.arun_restart_offset = false;
}

void ft8_autorun_restart()
{
    int rx_chan;
    rx_util_t *r = &rx_util;
    ft8_t *ft8_p[MAX_RX_CHANS];

    printf("FT8 autorun: RESTART\n");
    ft8_conf.arun_suspend_restart_victims = true;
        // shutdown all
        for (rx_chan = 0; rx_chan < rx_chans; rx_chan++) {
            ft8_p[rx_chan] = NULL;
            if (r->arun_which[rx_chan] == ARUN_FT8) {
                ft8_t *e = &ft8[rx_chan];
                ft8_p[rx_chan] = e;
                internal_conn_shutdown(&iconn[e->instance]);
                //printf("FT8 autorun STOP1 rx_chan=%d ARUN_FT8 => ARUN_NONE\n", rx_chan);
                r->arun_which[rx_chan] = ARUN_NONE;
            }
        }
        rx_autorun_clear();
        TaskSleepReasonSec("ft8_autorun_stop", 3);      // give time to disconnect
    
        // reset only autorun instances identified above (there may be non-autorun FT8 extensions running)
        for (rx_chan = 0; rx_chan < rx_chans; rx_chan++) {
            if (ft8_p[rx_chan] != NULL) {
                //printf("FT8 autorun STOP2 ch=%d\n", rx_chan);
                ft8_reset(ft8_p[rx_chan]);
            }
        }
        memset(iconn, 0, sizeof(iconn));
        memset(ft8_arun_seen, 0, sizeof(ft8_arun_seen));
        
        // bring ft8_arun_band[] and ft8_arun_preempt[] up-to-date
        ft8_update_vars_from_config(true);
        
        // XXX Don't start autorun here. Let rx_autorun_restart_victims() do it
        // because there might be other extension autorun restarting at the same time.
        // And starting too soon while the others are in the middle of kicking their EXT tasks
        // causes conflicts. rx_autorun_restart_victims() correctly waits for all extension
        // *.arun_suspend_restart_victims to become false.
        
        // restart all enabled
        //ft8_autorun_start(true);
    ft8_conf.arun_suspend_restart_victims = false;
}

void FT8_poll(int rx_chan)
{
    ft8_t *e = &ft8[rx_chan];
    
    // detect when freq offset changed so autorun can be restarted
    if (e->autorun && !ft8_conf.arun_restart_offset && ft8_conf.freq_offset_Hz != freq.offset_Hz) {
        ft8_conf.arun_restart_offset = true;
        ft8_autorun_restart();
    }
}

void FT8_main();

ext_t ft8_ext = {
	"FT8",
	FT8_main,
	ft8_close,
	ft8_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY,
	FT8_poll
};

void FT8_main()
{
	ext_register(&ft8_ext);
    ft8_update_vars_from_config(false);
	PSKReporter_init();
    ft8_autorun_start(true);

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
    kiwi_asfree(fn2);
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
