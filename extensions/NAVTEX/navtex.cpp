// Copyright (c) 2017 John Seamons, ZL4VO/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "kiwi.h"
#include "misc.h"
#include "mem.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <sys/mman.h>

//#define DEBUG_MSG	true
#define DEBUG_MSG	false

typedef struct {
    s2_t *s2p_start, *s2p_end;
    int tsamps;
} navtex_t;

static navtex_t navtex;


// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own navtex_chan[] data structure.

typedef struct {
	int rx_chan;
	bool test;

    s2_t *s2p, *s22p, *s2px;
    int nsamps;
} navtex_chan_t;

static navtex_chan_t navtex_chan[MAX_RX_CHANS];

static void navtex_file_data(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps, int freqHz)
{
    navtex_chan_t *e = &navtex_chan[rx_chan];

    if (!e->test) return;
    if (e->s2p >= navtex.s2p_end) {
        //printf("navtex_file_data test_done\n");
        ext_send_msg(rx_chan, false, "EXT test_done");
        e->test = false;
        return;
    }
    
    // Pushback 12 kHz sample file so it sounds right.
    if (e->test) {
        for (int i = 0; i < nsamps; i++) {
            if (e->s2p < navtex.s2p_end) {
                *samps++ = (s2_t) FLIP16(*e->s2p);
            }
            e->s2p++;
        }

        int pct = e->nsamps * 100 / navtex.tsamps;
        e->nsamps += nsamps;
        pct += 3;
        if (pct > 100) pct = 100;
        //ext_send_msg(rx_chan, false, "EXT bar_pct=%d", pct);
    }

}

bool navtex_msgs(char *msg, int rx_chan)
{
	navtex_chan_t *e = &navtex_chan[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	//printf("### navtex_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, DEBUG_MSG, "EXT ready");
		return true;
	}

	if (strcmp(msg, "SET test") == 0) {
        e->s2p = e->s2px = e->s22p = navtex.s2p_start;
        e->test = true;
        
        // misuse ext_register_receive_real_samps() to pushback audio samples from the test file
        ext_register_receive_real_samps(navtex_file_data, rx_chan);
		return true;
	}
	
	return false;
}

void navtex_close(int rx_chan)
{
	navtex_chan_t *e = &navtex_chan[rx_chan];
    ext_unregister_receive_real_samps(e->rx_chan);
}

void NAVTEX_main();

ext_t navtex_ext = {
	"NAVTEX",
	NAVTEX_main,
	navtex_close,
	navtex_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void NAVTEX_main()
{
	ext_register(&navtex_ext);

    const char *fn = cfg_string("navtex.test_file", NULL, CFG_OPTIONAL);
    if (!fn || *fn == '\0') return;
    char *fn2;
    asprintf(&fn2, "%s/samples/%s", DIR_CFG, fn);
    cfg_string_free(fn);
    printf("NAVTEX: mmap %s\n", fn2);
    int fd = open(fn2, O_RDONLY);
    if (fd < 0) {
        printf("NAVTEX: open failed\n");
        return;
    }
    off_t fsize = kiwi_file_size(fn2);
    kiwi_asfree(fn2);
    char *file = (char *) mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) {
        printf("NAVTEX: mmap failed\n");
        return;
    }
    close(fd);
    int words = fsize/2;
    navtex.s2p_start = (s2_t *) file;
    u4_t off = *(navtex.s2p_start + 3);
    off = FLIP16(off);
    printf("NAVTEX: size=%ld\n", fsize);
    off /= 2;
    navtex.s2p_start += off;
    words -= off;
    navtex.s2p_end = navtex.s2p_start + words;
    navtex.tsamps = words;
}
