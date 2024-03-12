// Copyright (c) 2023 John Seamons, ZL4VO/KF6VO

#include "types.h"
#include "bits.h"
#include "printf.h"
#include "str.h"
#include "misc.h"
#include "timer.h"
#include "coroutines.h"
#include "config.h"
#include "rx.h"
#include "net.h"
#include "web.h"
#include "PSKReporter.h"
#include "FT8.h"

// can't use traditional hton[sl] when initializing structs with const values
#define hns(i)          FLIP16(i)
#define hnl(i)          FLIP32(i)

#define PR_INFO_ELEM(i) hns(0x8000 | (i))
#define PR_LEN_BYTE     hns(1)
#define PR_LEN_SHORT    hns(2)
#define PR_LEN_INT      hns(4)
#define PR_LEN_STRING   0xffff
#define PR_ENTERPRISE   hnl(30351)
#define PR_TIME_SECS    hns(150)

struct msg_hdr_t {
    u2_t ver = hns(10);
    u2_t total_len;
    u4_t upload_time;
    u4_t seq;
    u4_t uniq;
} __attribute__((packed)) msg_hdr;


// rx info desc
#define PR_RX_ID            hns(3)
#define PR_RX_LINK          hns(0x1138)
#define PR_RX_ANT_LINK      hns(0x1139)
#define PR_RX_CALL          PR_INFO_ELEM(2)
#define PR_RX_LOC           PR_INFO_ELEM(4)
#define PR_RX_CLIENT        PR_INFO_ELEM(8)
#define PR_RX_ANT           PR_INFO_ELEM(9)
#define PR_RX_NFIELDS       hns(3)
#define PR_RX_ANT_NFIELDS   hns(4)

struct {
    u2_t id = PR_RX_ID;
    u2_t len;
    u2_t link = PR_RX_LINK;
    u2_t nfields = PR_RX_NFIELDS;
    u2_t scope_field_count = 0;
    
    u2_t op1_el = PR_RX_CALL;
    u2_t op1_len = PR_LEN_STRING;
    u4_t op1_ent = PR_ENTERPRISE;
    
    u2_t op2_el = PR_RX_LOC;
    u2_t op2_len = PR_LEN_STRING;
    u4_t op2_ent = PR_ENTERPRISE;
    
    u2_t op3_el = PR_RX_CLIENT;
    u2_t op3_len = PR_LEN_STRING;
    u4_t op3_ent = PR_ENTERPRISE;
    
    u2_t pad[1];
} __attribute__((packed)) rx_info_desc;

struct {
    u2_t id = PR_RX_ID;
    u2_t len;
    u2_t link = PR_RX_ANT_LINK;
    u2_t nfields = PR_RX_ANT_NFIELDS;
    u2_t scope_field_count = 0;
    
    u2_t op1_el = PR_RX_CALL;
    u2_t op1_len = PR_LEN_STRING;
    u4_t op1_ent = PR_ENTERPRISE;
    
    u2_t op2_el = PR_RX_LOC;
    u2_t op2_len = PR_LEN_STRING;
    u4_t op2_ent = PR_ENTERPRISE;
    
    u2_t op3_el = PR_RX_ANT;
    u2_t op3_len = PR_LEN_STRING;
    u4_t op3_ent = PR_ENTERPRISE;
    
    u2_t op4_el = PR_RX_CLIENT;
    u2_t op4_len = PR_LEN_STRING;
    u4_t op4_ent = PR_ENTERPRISE;
    
    u2_t pad[1];
} __attribute__((packed)) rx_ant_info_desc;


// tx info desc
#define PR_TX_ID        hns(2)
#define PR_TX_LINK      hns(0x1140)
#define PR_TX_CALL      PR_INFO_ELEM(1)
#define PR_TX_FREQ      PR_INFO_ELEM(5)
#define PR_TX_SNR       PR_INFO_ELEM(6)
#define PR_TX_MODE      PR_INFO_ELEM(10)
#define PR_TX_LOC       PR_INFO_ELEM(3)
#define PR_TX_ISRC      PR_INFO_ELEM(11)
#define PR_TX_ISRC_AUTO 1
#define PR_TX_NFIELDS   hns(7)

struct {
    u2_t id = PR_TX_ID;
    u2_t len;
    u2_t link = PR_TX_LINK;
    u2_t nfields = PR_TX_NFIELDS;
    
    u2_t op1_el = PR_TX_CALL;
    u2_t op1_len = PR_LEN_STRING;
    u4_t op1_ent = PR_ENTERPRISE;
    
    u2_t op2_el = PR_TX_FREQ;
    u2_t op2_len = PR_LEN_INT;
    u4_t op2_ent = PR_ENTERPRISE;
    
    u2_t op3_el = PR_TX_SNR;
    u2_t op3_len = PR_LEN_BYTE;
    u4_t op3_ent = PR_ENTERPRISE;
    
    u2_t op4_el = PR_TX_MODE;
    u2_t op4_len = PR_LEN_STRING;
    u4_t op4_ent = PR_ENTERPRISE;
    
    u2_t op5_el = PR_TX_LOC;
    u2_t op5_len = PR_LEN_STRING;
    u4_t op5_ent = PR_ENTERPRISE;
    
    u2_t op6_el = PR_TX_ISRC;
    u2_t op6_len = PR_LEN_BYTE;
    u4_t op6_ent = PR_ENTERPRISE;
    
    u2_t op7_el = PR_TIME_SECS;
    u2_t op7_len = PR_LEN_INT;
    
    //u2_t pad[1];
} __attribute__((packed)) tx_info_desc;

// PR_TX_LINK, len, call, freq, snr, mode, grid, isrc, slot_time, pad (max), slop
#define PR_TX_MAX_LEN   (2 + 2 + (1+14) + 4 + 1 + (1+3) + (1+6) + 1 + 4 + 3 + 16)
#define PR_BUF_LEN 2048

typedef struct {
	bool task_created;
	tid_t tid;

    char *upload_url;
    u4_t pending_uploads[MAX_RX_CHANS];
    u4_t num_uploads[MAX_RX_CHANS];     // running total number of uploads created per channel
	char rcall[16], rgrid[8];
    latLon_t r_loc;
    bool grid_ok;
    bool have_ant;
    char *ant;

	int send_info_desc_repeat, send_info_desc_interval;
	bool sent_info_desc[2];
	u4_t spots[2];
	bool packet_full;
	int ping_pong;
    u1_t buf[2][PR_BUF_LEN], *bp, *bbp, *xbp, *xbbp;
    struct msg_hdr_t *hdr;
	u4_t seq, uniq;
} pr_conf_t;

static pr_conf_t pr_conf;

static void pr_dump(const char *which, int ping_pong, u1_t *bbp, u1_t *bp, int so)
{
    pr_conf_t *pr = &pr_conf;
    u1_t *p = bp;
    int off = bp - bbp;
    printf("PSKReporter dump %s pp=%d pkt_o=%d|%d s/o=%d(0x%x)(%d/4)\n", which, ping_pong, off, off/4, so, so, so/4);
    bool upload = (strcmp(which, "upload") == 0);
    u4_t i, flip = 0, flip_off = 16;
    u2_t *u2_p = (u2_t *) bp;
    if (upload) real_printf(CYAN);
    for (i = 0; i < so; i++) {
        if (upload && i == flip_off) {
            flip ^= 1;
            real_printf(NONL "%s", flip? YELLOW : CYAN);
            u2_t len2 = FLIP16(u2_p[i/2+1]);
            flip_off = i + len2;
            //real_printf("{%d %d %d} ", flip_off, i, len2);
        }
        real_printf("%02x ", *p);
        if ((i & 3) == 3) real_printf("| ");
        if (!upload && (i % (14*4)) == (14*4-1)) real_printf("\n");
        p++;
    }
    real_printf(NONL);

    p = bp;
    flip = 0, flip_off = 16;
    u2_p = (u2_t *) bp;
    if (upload) real_printf(CYAN);
    for (i = 0; i < so; i++) {
        if (upload && i == flip_off) {
            flip ^= 1;
            real_printf(NONL "%s", flip? YELLOW : CYAN);
            u2_t len2 = FLIP16(u2_p[i/2+1]);
            flip_off = i + len2;
        }
        real_printf("%2s ", ASCII[*p]);
        if ((i & 3) == 3) real_printf("| ");
        if (!upload && (i % (14*4)) == (14*4-1)) real_printf("\n");
        p++;
    }
    real_printf(NORM "\n\n");
    check((so % 4) == 0);
}

static u1_t *pr_emit_string(u1_t *bp, const char *s)
{
    int so = strlen(s);
    *bp = so;
    bp++;
    strncpy((char *) bp, s, so);
    bp += so;
    return bp;
}

static u1_t * pr_info_desc(u1_t *bp)
{
    pr_printf("PSKReporter send info_desc\n");
    pr_conf_t *pr = &pr_conf;
    int so;
    u2_t *lenp;
    
    // tx info desc
    so = sizeof(tx_info_desc);
    tx_info_desc.len = hns(so);
    memcpy(bp, (char *) &tx_info_desc, so);
    //pr_dump("tx_info_desc", pr->ping_pong, pr->bbp, bp, so);
    bp += so;

    // rx info desc
    if (pr->have_ant) {
        so = sizeof(rx_ant_info_desc);
        rx_ant_info_desc.len = hns(so);
        memcpy(bp, (char *) &rx_ant_info_desc, so);
        //pr_dump("rx_ant_info_desc", pr->ping_pong, pr->bbp, bp, so);
    } else {
        so = sizeof(rx_info_desc);
        rx_info_desc.len = hns(so);
        memcpy(bp, (char *) &rx_info_desc, so);
        //pr_dump("rx_info_desc", pr->ping_pong, pr->bbp, bp, so);
    }
    bp += so;
    
    pr->sent_info_desc[pr->ping_pong] = true;
    return bp;
}

static u1_t *pr_rx_info(u1_t *bp)
{
    pr_conf_t *pr = &pr_conf;
    u1_t *bbp = bp;
    int so;
    u2_t *lenp;
    
    *(u2_t *) bp = pr->have_ant? PR_RX_ANT_LINK : PR_RX_LINK;
    bp += 2;
    lenp = (u2_t *) bp;
    bp += 2;
    
    bp = pr_emit_string(bp, pr->rcall);
    bp = pr_emit_string(bp, pr->rgrid);
    if (pr->have_ant) bp = pr_emit_string(bp, pr->ant);
    
    const char *client = "KiwiSDR 1.1";
    bp = pr_emit_string(bp, client);
    
    while (((bp - bbp) % 4) != 0) *bp++ = 0;
    *lenp = hns(bp - bbp);
    //pr_dump("rx_info", pr->ping_pong, pr->bbp, bbp, bp - bbp);
    return bp;
}

static u1_t *pr_check_need_hdr(u1_t *bp)
{
    pr_conf_t *pr = &pr_conf;
    if (pr->hdr != NULL) return bp;
    
    pr->hdr = (struct msg_hdr_t *) bp;
    int so = sizeof(msg_hdr);
    memcpy(bp, (char *) &msg_hdr, so);
    //pr_dump("msg_hdr", pr->ping_pong, pr->bbp, bp, so);
    bp += so;

    if (pr->send_info_desc_repeat > 0) {
        bp = pr_info_desc(bp);
        pr->send_info_desc_repeat--;
    }
	
    bp = pr_rx_info(bp);    // always send rx info before spots
    return bp;
}

int PSKReporter_distance(const char *grid)
{
    pr_conf_t *pr = &pr_conf;
    return pr->grid_ok? grid_to_distance_km(&pr->r_loc, (char *) grid) : 0;
}

int PSKReporter_spot(int rx_chan, const char *call, u4_t passband_freq, s1_t snr, ftx_protocol_t protocol, const char *grid, u4_t slot_time, u4_t slot)
{
    pr_conf_t *pr = &pr_conf;
    int km = PSKReporter_distance(grid);
    
    if (pr->task_created) {
        conn_t *conn = rx_channels[rx_chan].conn;
        u4_t freq = conn->freqHz + ft8_conf.freq_offset_Hz + passband_freq;
        const char *mode = (protocol == FTX_PROTOCOL_FT8)? "FT8" : "FT4";
        time_t time = (time_t) slot_time;
        rcfprintf(rx_chan, ft8_conf.syslog? (PRINTF_REG | PRINTF_LOG) : PRINTF_REG,
            "PSKReporter spot %s %9.3f %8s %s %+3d %5dkm %s\n", mode, (double) freq / 1e3, call, grid, snr, km, var_ctime_static(&time));
        ext_send_msg_encoded(rx_chan, false, "EXT", "debug", "%s %.3f %s %s %+d %dkm %s", mode, (double) freq / 1e3, call, grid, snr, km, var_ctime_static(&time));
    
        u1_t *bp = pr->bp;
        int so;
        u2_t *lenp;
        bp = pr_check_need_hdr(bp);
        u1_t *bbp = bp;

        *(u2_t *) bp = PR_TX_LINK;
        bp += 2;
        lenp = (u2_t *) bp;
        bp += 2;
        
        bp = pr_emit_string(bp, call);
        *(u4_t *) bp = hnl(freq); bp += 4;
        *bp++ = (u1_t) snr;
        bp = pr_emit_string(bp, mode);
        bp = pr_emit_string(bp, grid);
        *bp++ = PR_TX_ISRC_AUTO;
        *(u4_t *) bp = hnl(slot_time); bp += 4;

        // pad out
        while (((bp - bbp) % 4) != 0) *bp++ = 0;
        *lenp = hns(bp - bbp);
        //pr_dump("spot", pr->ping_pong, pr->bbp, bbp, bp - bbp);
        pr->bp = bp;
    
        size_t offset = pr->bp - pr->bbp;
        bool pkt_full = offset > (PR_BUF_LEN - PR_TX_MAX_LEN);
        //ext_send_msg_encoded(rx_chan, false, "EXT", "debug", "pp %d off %d full %d", pr->ping_pong, offset, pkt_full);
        pr->spots[pr->ping_pong]++;
        pr->pending_uploads[rx_chan]++;
        pr_printf("pending_uploads[%d]=%d\n", rx_chan, pr->pending_uploads[rx_chan]);

        if (pkt_full) {
            pr->xbp = pr->bp; pr->xbbp = pr->bbp;
            pr->ping_pong ^= 1; pr->bp = pr->bbp = pr->buf[pr->ping_pong]; pr->hdr = NULL;
            pr->packet_full = true;
            pr_printf("PSKReporter spot pkt FULL ping_pong %d => %d\n", pr->ping_pong ^ 1, pr->ping_pong);
            TaskWakeupF(pr->tid, TWF_CANCEL_DEADLINE);
        }
        
        static u4_t last_slot;
        if (slot != last_slot) {
            ft8_update_spot_count(rx_chan, pr->pending_uploads[rx_chan]);
            last_slot = slot;
        }
    }
    
    return km;
}

u4_t PSKReporter_num_uploads(int rx_chan)
{
    return pr_conf.num_uploads[rx_chan];
}

// reset counters on band/frequency change so PSKReporter_num_uploads() always returns the
// spot count for the current settings
void PSKReporter_reset(int rx_chan)
{
    pr_conf_t *pr = &pr_conf;
    pr->pending_uploads[rx_chan] = pr->num_uploads[rx_chan] = 0;
    //printf("PSKReporter RESET\n");
}

static void PSKreport(void *param)      // task
{
    pr_conf_t *pr = &pr_conf;
    pr->ping_pong = 0; pr->bp = pr->bbp = pr->buf[pr->ping_pong]; pr->hdr = NULL;
    pr->seq = 1;
    pr->uniq = timer_us();
    pr->send_info_desc_repeat = PR_INFO_DESC_RPT;
    pr->send_info_desc_interval = 60;
    s4_t delta_msec = 0;
    
    struct mg_connection *mg = web_connect(pr->upload_url);

	while (1) {
        pr->send_info_desc_interval--;
	    if (pr->send_info_desc_interval == 0) {
            pr->send_info_desc_repeat = 1;
            pr->send_info_desc_interval = 60;
	    }
	    
		u64_t sleep_msec = SEC_TO_MSEC(MINUTES_TO_SEC(PR_UPLOAD_MINUTES)) + delta_msec;
		TaskSleepMsec(sleep_msec);
		
		// Randomize next upload time +/- 8192 msec
		delta_msec = timer_us() & 0x1fff;
		if (delta_msec & 0x1000) delta_msec = -delta_msec;
		//printf("PSKReporter delta %d\n", delta_msec);
		
		int ping_pong;
		if (!pr->packet_full) {
		    // flip_flop
            ping_pong = pr->ping_pong;
            pr->xbp = pr->bp; pr->xbbp = pr->bbp;
            pr->ping_pong ^= 1; pr->bp = pr->bbp = pr->buf[pr->ping_pong]; pr->hdr = NULL;
		    pr_printf("PSKReporter task wakeup: TIMEOUT ping_pong %d => %d\n", pr->ping_pong ^ 1, pr->ping_pong);
            ext_send_msg_encoded(/* rx_chan */ 0, false, "EXT", "debug",
                "PSKReporter task wakeup: TIMEOUT ping_pong %d => %d\n", pr->ping_pong ^ 1, pr->ping_pong);
		} else {
		    // PSKReporter_spot() did flip_flop and set xbp/xbbp
		    ping_pong = pr->ping_pong ^ 1;
		    pr->packet_full = false;
		    pr_printf("PSKReporter task wakeup: EARLY pkt FULL\n");
            ext_send_msg_encoded(/* rx_chan */ 0, false, "EXT", "debug",
		        "PSKReporter task wakeup: EARLY pkt FULL\n");
		}
		
		// upload
        u1_t *bbp = pr->xbbp, *bp = pr->xbp;
        int total_len = bp - bbp;
        if (total_len == 0) {
		    pr_printf("PSKReporter task wakeup: nothing to do, send_info_desc_interval=%d\n", pr->send_info_desc_interval);
            ext_send_msg_encoded(/* rx_chan */ 0, false, "EXT", "debug",
		        "PSKReporter task wakeup: nothing to do, send_info_desc_interval=%d\n", pr->send_info_desc_interval);
            continue;
        }
        
        struct msg_hdr_t *hdr = (struct msg_hdr_t *) bbp;
        hdr->total_len = hns(total_len);
        hdr->upload_time = hnl(utc_time());
        hdr->seq = hnl(pr_conf.seq); pr_conf.seq++;
        hdr->uniq = hnl(pr_conf.uniq);
        #ifdef PR_TESTING
            pr_dump("upload", ping_pong, bbp, bbp, total_len);
        #endif

        lfprintf(ft8_conf.syslog? (PRINTF_REG | PRINTF_LOG) : PRINTF_REG, "PSKReporter upload %d spots %sto %s\n",
            pr->spots[ping_pong], pr->sent_info_desc[ping_pong]? "(and info desc) " : "", pr->upload_url);
        ext_send_msg_encoded(/* rx_chan */ 0, false, "EXT", "debug",
            "PSKReporter upload %d spots %sto %s\n",
            pr->spots[ping_pong], pr->sent_info_desc[ping_pong]? "(and info desc) " : "", pr->upload_url);
        pr->sent_info_desc[ping_pong] = false;
        pr->spots[ping_pong] = 0;
        size_t n = mg_write(mg, bbp, total_len);
        //pr_printf("PSK UDP mg_write n=%d|%d\n", n, total_len);
        
        for (int rxc = 0; rxc < MAX_RX_CHANS; rxc++) {
            pr->num_uploads[rxc] = pr->pending_uploads[rxc];
            pr_printf("num_uploads[%d] = pending_uploads %d\n", rxc, pr->pending_uploads[rxc]);
        }
	}
}

int PSKReporter_setup(int rx_chan)
{
    pr_conf_t *pr = &pr_conf;
    
    // only launch once
    #ifdef PR_USE_CALLSIGN_HASHTABLE
        if (!pr->task_created) {
            bool no_call_or_grid = true;
            const char *rcall = cfg_string("ft8.callsign", NULL, CFG_OPTIONAL);
            const char *rgrid = cfg_string("ft8.grid", NULL, CFG_OPTIONAL);
            const char *ant = cfg_string("rx_antenna", NULL, CFG_OPTIONAL);
            pr->have_ant = (ant != NULL && ant[0] != '\0');
            //pr_printf("rcall <%s> rgrid <%s> ant <%s>\n", rcall, rgrid, ant);
            
            pr->grid_ok = false;
            if (rgrid) {
                int glen = strlen(rgrid);
                if (glen >= 4 && glen <= 6) {
                    kiwi_strncpy(pr->rgrid, rgrid, 8);
	                grid_to_latLon(rgrid, &pr->r_loc);
	                //pr_printf("PSKReporter_setup grid=%s lat=%f lon=%f\n", rgrid, pr->r_loc.lat, pr->r_loc.lon);
	                latLon_deg_to_rad(pr->r_loc);
                    pr->grid_ok = true;
                }
            }
            
            if (pr->have_ant) {
                pr->ant = strndup(ant, 255);
                kiwi_str_decode_inplace(pr->ant);
            }
            
            if (rcall && strlen(rcall) >= 3 && pr->grid_ok) {
                kiwi_strncpy(pr->rcall, rcall, 16);
                kiwi_str_decode_inplace(pr->rcall);
                pr->tid = CreateTask(PSKreport, NULL, SERVICES_PRIORITY);
                pr->task_created = true;
                no_call_or_grid = false;
            }
            cfg_string_free(rcall); cfg_string_free(rgrid); cfg_string_free(ant);
            if (no_call_or_grid) {
                rcprintf(rx_chan, "PSKReporter_setup: no call or grid configured\n");
                return -1;
            } else {
                return 1;
            }
        }
        return 0;
    #else
        #warning FT8: PSKReporter upload code not enabled
        return -1;
    #endif
}

void PSKReporter_init()
{
    #define BACKUP_PSKREPORTER_PUBLIC_IP "74.116.41.13"
    static ip_lookup_t ips_pskreporter;
    DNS_lookup("report.pskreporter.info", &ips_pskreporter, N_IPS, BACKUP_PSKREPORTER_PUBLIC_IP);
    u1_t a,b,c,d;
    inet4_h2d(ips_pskreporter.ip[0], &a,&b,&c,&d);
    asprintf(&pr_conf.upload_url, "udp://%d.%d.%d.%d:%d", a,b,c,d, PR_UPLOAD_PORT);
}
