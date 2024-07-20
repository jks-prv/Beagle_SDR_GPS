/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2024 John Seamons, ZL4VO/KF6VO

#include "sdrpp_server.h"
#include "spyserver_protocol.h"
#include "data_pump.h"
#include "printf.h"
#include "rx/rx.h"
#include "rx/rx_sound_cmd.h"


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#ifdef MG_API_NEW

//hf+ sdr://143.178.161.182:5555
//rtl sdr://94.208.173.143:5555

#define SDRPP_PRINTF
#ifdef SDRPP_PRINTF
	#define sdrpp_prf(fmt, ...) \
		lprintf(fmt, ## __VA_ARGS__)
#else
	#define sdrpp_prf(fmt, ...)
#endif

//#define SDRPP_DPRINTF
#ifdef SDRPP_DPRINTF
	#define sdrpp_dprf(fmt, ...) \
		printf(fmt, ## __VA_ARGS__)
#else
	#define sdrpp_dprf(fmt, ...)
#endif

typedef struct {
    u2_t i, q;
} iq16_t;

typedef struct {
    u1_t i, q;
} iq8_t;

/* iq8 bytes: 75k 1472, 150k 2944, 300k 5888 (2944 * iq8_t)
    75/1472 = 96/x, 1472*96/75 = 1884.16
*/

#define FNAME DIR_CFG "/samples/CQWW_CW_2005.iq16b.fs96k.cf7040.full.iq"

// options
//#define FILE_IN
#ifdef FILE_IN
    #define PACER
    #define SWAP_IQ         // CQWW file is IQ swapped
#else
    //#define SOURCE_MMAP
    #ifdef SOURCE_MMAP
        #define PACER
        #define SWAP_IQ     // CQWW file is IQ swapped
    #else
        #define DIRECT
        //#define CAMP
    #endif
#endif

#define IQ16
#define FLIP_IQ

#define N_SAMPS (960 * 10)

#ifdef IQ16
    #define SIZEOF_SAMP sizeof(iq16_t)
#else
    #define SIZEOF_SAMP sizeof(iq8_t)
#endif

static u4_t bw;
static int fd = -1, streaming_enabled;
static tid_t sdrpp_WR_tid = -1;
static iq16_t *in;

#ifdef FILE_IN
    static iq16_t file[N_SAMPS];
#else
    #ifdef SOURCE_MMAP
        static off_t fsize;
        static iq16_t *fp;
    #else
        #ifdef CAMP
            static u4_t our_rd_pos;
            #define RD_POS our_rd_pos
        #else
            #define RD_POS rx->rd_pos
        #endif
    #endif
#endif

static void sdrpp_WR(void *param)
{
    struct mg_connection *mc = (struct mg_connection *) FROM_VOID_PARAM(param);
    
    while (1) {
        if (streaming_enabled) {
            void sdrpp_out(struct mg_connection *mc, void *ev_data);
            sdrpp_out(mc, NULL);
        } else {
            #ifdef CAMP
                //real_printf("W"); fflush(stdout);
                TaskSleepMsec(250);
            #else
                //real_printf("W"); fflush(stdout);
                TaskSleepMsec(250);
            #endif
        }
        
        #ifdef PACER
            TaskSleepMsec(1);
        #else
            NextTask("sdrpp_WR");
        #endif
    }
}

// server => client (init)
void sdrpp_accept(struct mg_connection *mc, void *ev_data)
{
    u4_t srate, freq, f_min, f_max;

    #ifdef PACER
        bw = 96000;
        srate = bw * 2;
        freq = 7040000;
        f_min = freq - bw/2;
        f_max = freq + bw/2;
    
        fd = open(FNAME, O_RDONLY);
        if (fd < 0) panic("sdrpp: open");
        
        #ifdef SOURCE_MMAP
            fsize = kiwi_file_size(FNAME);
            fp = (iq16_t *) mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
            if (fp == MAP_FAILED) {
                panic("sdrpp: mmap failed\n");
            }
            close(fd);
            fd = -1;
            in = fp;
            sdrpp_dprf("sdrpp: %d|%d %p|%p\n", N_SAMPS, fsize / sizeof(iq16_t), in + N_SAMPS, fp + (fsize / sizeof(iq16_t)));
        #endif

        sdrpp_WR_tid = CreateTask(sdrpp_WR, TO_VOID_PARAM(mc), SND_PRIORITY);
    #endif

    #ifdef DIRECT
        rx_chan_t *rxc = &rx_channels[RX_CHAN0];
        bw = wb_rate;
        srate = bw * 2;
        freq = 14040000;
        f_min = freq - bw/2;
        f_max = freq + bw/2;
        if (nrx_samps_wb > N_SAMPS) panic("nrx_samps_wb > N_SAMPS");
    
        #ifdef CAMP
        #else
            if (rxc->busy) {
                mg_http_reply(mc, 503, NULL, "");       // service unavailable
                mg_connection_close(mc);
                return;
            }
        #endif
        
        if (sdrpp_WR_tid != -1) {
            TaskRemove(sdrpp_WR_tid);
            sdrpp_WR_tid = -1;
        }
        
        #ifdef CAMP
        #else
            c2s_sound_init();
            data_pump_startup();
        #endif
        
        sdrpp_WR_tid = CreateTask(sdrpp_WR, TO_VOID_PARAM(mc), SND_PRIORITY);

        #ifdef CAMP
        #else
            rxc->busy = true;
            rx_enable(RX_CHAN0, RX_CHAN_ENABLE);
            rx_enable(RX_CHAN0, RX_DATA_ENABLE);
        #endif
    #endif

// hf+
//0000: a406 0002 0000 0000 0000 0000 0000 0000 3000 0000>0200 0000 3230 3131 00b8 0b00  ................0.......2011....
//0020: 2012 0a00 0800 0000 0000 0000 0000 0000 0000 0000 00f1 5365 1000 0000 0200 0000  .....................Se........
//0040: 0100 0000                                                                        ....

// rtl
//0000: 1e07 0002 0000 0000 0000 0000 0000 0000 3000 0000>0300 0000 0100 0000 009f 2400  ................0.............$.
//0020: 8084 1e00 0900 0000 0000 0000 1d00 0000 0036 6e01 00d2 496b 0800 0000 0000 0000  .................6n...Ik........
//0040: 0000 0000                                                                        ....
    struct SpyServerDeviceInfo di = {
        .mh.ProtocolID = SPYSERVER_PROTOCOL_VERSION,
        .mh.MessageType = SPYSERVER_MSG_TYPE_DEVICE_INFO,
        .mh.StreamType = SPYSERVER_STREAM_TYPE_STATUS,
        .mh.SequenceNumber = 0,
        .mh.BodySize = DI_SIZE,     // 0x30 12*4=48
                                                // rtl      hf+
        .DeviceType = SPYSERVER_DEVICE_RTLSDR,  // 3        2
        .DeviceSerial = 0,                      // 1        0x31313032
        .MaximumSampleRate = srate,             // 2.4M     768k
        .MaximumBandwidth = bw,                 // 2M       660k
        .DecimationStageCount = 1,              // 9        8
        .GainStageCount = 0,                    // 0        0
        .MaximumGainIndex = 1,                  // 29       0
        .MinimumFrequency = f_min,              // 24M      0
        .MaximumFrequency = f_max,              // 1800M    1700M
        .Resolution = 1,                        // 8        16
        .MinimumIQDecimation = 1,               // 0        2
        .ForcedIQFormat = 1                     // 0        1
    };
    sdrpp_prf("sdrpp_accept send SpyServerDeviceInfo=%d\n", sizeof(di));
    mg_send(mc, &di, sizeof(di));

// hf+
//0000: a406 0002 0100 0000 0000 0000 0000 0000 2400 0000 0100 0000 0000 0000 1009 0500  ................$...............
//0020: 1009 0500 1009 0500 4442 0100 bcae 5265 1009 0500 f0e7 4e65                      ........DB....Re......Ne

// rtl 75k
//0000: a406 0002 0100 0000 0000 0000 0000 0000 2400 0000 0100 0000 0000 0000 00e1 f505  ................$...............
//0020: 00e1 f505 00e1 f505 12b0 6e01 ee57 496b 4078 7d01 c08f 3a6b                      ..........n..WIk@x}...:k

// rtl 150k
//0000: a406 0002 0100 0000 0000 0000 0000 0000 2400 0000 0100 0000 0000 0000 00e1 f505  ................$...............
//0020: 00e1 f505 00e1 f505 242a 6f01 dcdd 486b 4078 7d01 c08f 3a6b                      ........$*o...Hk@x}...:k
    struct SpyServerClientSync cs = {
        .mh.ProtocolID = SPYSERVER_PROTOCOL_VERSION,
        .mh.MessageType = SPYSERVER_MSG_TYPE_CLIENT_SYNC,
        .mh.StreamType = SPYSERVER_STREAM_TYPE_STATUS,
        .mh.SequenceNumber = 0,
        .mh.BodySize = CS_SIZE,     
                                    // 0x24 9*4=36  // rtl      hf+
        .CanControl = 1,                            // 1        1
        .Gain = 0,                                  // 0        0
        .DeviceCenterFrequency = freq,              // 100M     330k
        .IQCenterFrequency = freq,                  // 100M     330k
        .FFTCenterFrequency = freq,                 // 100M     330k
        .MinimumIQCenterFrequency = f_min,          // 25M      82.5k
                                                    // 24.031250(75k)
                                                    // 24.062500(150k)
        .MaximumIQCenterFrequency = f_max,          // 1799M    1700M-82.5k
                                                    // 1799.968750(75k)
                                                    // 1799.937500(150k)
        .MinimumFFTCenterFrequency = f_min,         // 25M      330k
        .MaximumFFTCenterFrequency = f_max          // 1799M    1700M-82.5k
    };
    sdrpp_prf("sdrpp_accept send SpyServerClientSync=%d\n", sizeof(cs));
    mg_send(mc, &cs, sizeof(cs));
    mc->is_resp = 0;    // response is complete
}

// client => server
// called from MG_EV_READ event when input data is available
void sdrpp_in(struct mg_connection *mc, void *ev_data)
{
    int bytes_in = (int) *((long *) ev_data);
    //sdrpp_prf("sdrpp_in bytes_in=%d\n", bytes_in);
    int bytes_rem = bytes_in;
    u1_t *buf = mc->recv.buf;
    
    while (bytes_rem) {
        struct SpyServerCommandHeader *ch = (struct SpyServerCommandHeader *) buf;
        
        switch (ch->CommandType) {
        
//0000: 0000 0000 0900 0000 a406 0002 5344 522b 2b ............SDR++
        case SPYSERVER_CMD_HELLO: {
            struct SpyServerClientHandshake *hs = (struct SpyServerClientHandshake *) buf;
            int len = bytes_in - HS_SIZE;
            sdrpp_prf("SPYSERVER_CMD_HELLO ProtocolVersion=%08x(%08x) id=%.*s\n", hs->ProtocolVersion, SPYSERVER_PROTOCOL_VERSION, len, hs->id);
            bytes_rem = 0;
            break;
        }

// hf+ ???
//0000: 0200 0000 0800 0000 // 6400 0000 0200 0000  ........d.......
    
//0000: 0200 0000 0800 0000 // 6600 0000 0200 0000 // 0200 0000 0800 0000 // 6500 0000 3c37 0000  ........f...............e...<7..
//0020: 0200 0000 0800 0000 // 0000 0000 0100 0000 // 0200 0000 0800 0000 // 0200 0000 0000 0000  ................................
//0040: 0200 0000 0800 0000 // 6700 0000 0600 0000 // 0200 0000 0800 0000 // 0100 0000 0100 0000  ........g.......................
//0060: 0200 0000 0800 0000 // 6500 0000 3c37 0000                                          ........e...<7..
        case SPYSERVER_CMD_SET_SETTING: {
            struct SpyServerSettingTarget *st = (struct SpyServerSettingTarget *) buf;
            //sdrpp_prf("SPYSERVER_CMD_SET_SETTING Setting=%d Value=%d\n", st->Setting, st->Value);
    
            switch (st->Setting) {
                case SPYSERVER_SETTING_STREAMING_MODE: sdrpp_prf("SPYSERVER_SETTING_STREAMING_MODE = %d\n", st->Value); break;
                case SPYSERVER_SETTING_STREAMING_ENABLED: sdrpp_prf("SPYSERVER_SETTING_STREAMING_ENABLED = %d\n", st->Value);
                    streaming_enabled = st->Value;
                    if (sdrpp_WR_tid != -1) {
                        #ifdef CAMP
                        #else
                            rx_chan_t *rxc = &rx_channels[RX_CHAN0];
                            rxc->wb_task = sdrpp_WR_tid;
                        #endif
                        TaskWakeup(sdrpp_WR_tid);
                    }
                    break;
                case SPYSERVER_SETTING_GAIN: sdrpp_prf("SPYSERVER_SETTING_GAIN = %d\n", st->Value); break;
    
                case SPYSERVER_SETTING_IQ_FORMAT: sdrpp_prf("SPYSERVER_SETTING_IQ_FORMAT = %d\n", st->Value); break;
                case SPYSERVER_SETTING_IQ_FREQUENCY:
                    //sdrpp_prf("SPYSERVER_SETTING_IQ_FREQUENCY = %d\n", st->Value);
                    rx_sound_set_freq(NULL, (double) st->Value / 1e3, /* jksxmg FIXME */ false);
                    break;
                case SPYSERVER_SETTING_IQ_DECIMATION: sdrpp_prf("SPYSERVER_SETTING_IQ_DECIMATION = %d\n", st->Value); break;
                case SPYSERVER_SETTING_IQ_DIGITAL_GAIN: sdrpp_prf("SPYSERVER_SETTING_IQ_DIGITAL_GAIN = %d\n", st->Value); break;
    
                case SPYSERVER_SETTING_FFT_FORMAT: sdrpp_prf("SPYSERVER_SETTING_FFT_FORMAT = %d\n", st->Value); break;
                case SPYSERVER_SETTING_FFT_FREQUENCY: sdrpp_prf("SPYSERVER_SETTING_FFT_FREQUENCY = %d\n", st->Value); break;
                case SPYSERVER_SETTING_FFT_DECIMATION: sdrpp_prf("SPYSERVER_SETTING_FFT_DECIMATION = %d\n", st->Value); break;
                case SPYSERVER_SETTING_FFT_DB_OFFSET: sdrpp_prf("SPYSERVER_SETTING_FFT_DB_OFFSET = %d\n", st->Value); break;
                case SPYSERVER_SETTING_FFT_DB_RANGE: sdrpp_prf("SPYSERVER_SETTING_FFT_DB_RANGE = %d\n", st->Value); break;
                case SPYSERVER_SETTING_FFT_DISPLAY_PIXELS: sdrpp_prf("SPYSERVER_SETTING_FFT_DISPLAY_PIXELS = %d\n", st->Value); break;
    
                default: sdrpp_prf("SPYSERVER_CMD_SET_SETTING UNKNOWN Setting=%d Value=%d\n", st->Setting, st->Value); break;
            }
            buf += ST_SIZE;
            bytes_rem -= ST_SIZE;
            break;
        }
        
        case SPYSERVER_CMD_PING: {
            sdrpp_prf("SPYSERVER_CMD_PING\n");
            bytes_rem = 0;
            break;
        }
        
        default:
            // sdr# ?
            // sdrpp_in: CommandType=1314410051 UNKNOWN
            // sdrpp_in: CommandType=1313165391 UNKNOWN
            sdrpp_prf("sdrpp_in: CommandType=%d UNKNOWN\n", ch->CommandType);
            bytes_rem = 0;
            break;
        }
    }
    mc->recv.len = 0;   // indicate consumed
}

// server => client
// called from MG_EV_WRITE event when a prior mg_send() completes

// hf+
//0000: a406 0002 6400 0600 0100 0000 0000 0000 000f 0000 // 8080 8080 8080 8080 8080 8080  ....d...........................

// rtl 64|0f 75k 0x5c0 1472 736*2
//0020:                                                             a406 0002 6400 0f00  $}........n..WIk@x}...:k....d...
//0040: 0100 0000 0000 0000 c005 0000 8080 8080 8080 8080 8080 8080 8080 8080 8080 8080  ................................

// rtl 64|0c 150k 0xb80 2944 
//                                                                  a406 0002 6400 0c00  $}......$*o...Hk@x}...:k....d...
//0040: 0100 0000 0000 0000 800b 0000 8080 8080 8080 8080 8080 8080 8080 8080 8080 8080  ................................
//      strm ty iq      seq len

typedef struct {
    struct SpyServerMessageHeader mh;
    iq16_t buf16[N_SAMPS];
} out16_t;
static out16_t out16;

typedef struct {
    struct SpyServerMessageHeader mh;
    iq8_t buf8[N_SAMPS];
} out8_t;
static out8_t out8;

void sdrpp_out(struct mg_connection *mc, void *ev_data)
{
    long *bytes_out = (long *) ev_data;
    
    #ifdef PACER
        static bool init;
        static u4_t start, tick;
        static double interval_us, elapsed_us;
        static int perHz;
        if (!init) {
            start = timer_us();
            init = true;
            elapsed_us = 0;
            interval_us = 1e6 / (float) bw * N_SAMPS;
            perHz = (int) round((float) bw / N_SAMPS);
            printf("sdrpp_out bw=%d interval(msec)=%.3f\n", bw, interval_us/1e3);
        }
        
        u4_t now = timer_us();
        u4_t diff = now - start;
        //real_printf("%d %.0f\n", diff, round(elapsed_us));
        if ((now - start) < round(elapsed_us)) {
            return;
        }
        if (tick > 1) elapsed_us += interval_us;
        tick++;
        //if ((tick % perHz) == 0) { real_printf("#"); fflush(stdout); }
        
        #ifdef FILE_IN
            if ((tick % perHz) == 0) { printf("%d\n", tick); }
        #endif
        #ifdef SOURCE_MMAP
            if ((tick % perHz) == 0) { printf("%d %p|%p\n", tick, in, fp + (fsize / (sizeof(iq16_t)))); }
        #endif
    #else
        #ifdef DIRECT
            rx_chan_t *rxc = &rx_channels[RX_CHAN0];
            #ifdef CAMP
                if (!rxc->busy) {
                    //real_printf("."); fflush(stdout);
                    TaskSleepMsec(250);
                    return;
                }
            #endif
            
            rx_dpump_t *rx = &rx_dpump[RX_CHAN0];
            while (rx->wr_pos == RD_POS) {
                TaskSleepReason("check pointers");
            }
            //real_printf("#"); fflush(stdout);
        #endif
    #endif
    
    #ifdef IQ16
        out16_t *out = &out16;
    #else
        out8_t *out = &out8;
    #endif

    static u4_t seq;
    #define GAIN 0xf
    out->mh.ProtocolID = SPYSERVER_PROTOCOL_VERSION;
    #ifdef IQ16
        out->mh.MessageType = (GAIN << 16) | SPYSERVER_MSG_TYPE_INT16_IQ;
    #else
        out->mh.MessageType = (GAIN << 16) | SPYSERVER_MSG_TYPE_UINT8_IQ;
    #endif
    out->mh.StreamType = SPYSERVER_STREAM_TYPE_IQ;
    out->mh.SequenceNumber = seq++;

    u4_t num_samps, out_bytes;
    int n;
    
    #ifdef FILE_IN
        num_samps = N_SAMPS;
        do {
            n = read(fd, &file, sizeof(file));
            if (n < sizeof(file)) lseek(fd, 0, SEEK_SET);
        } while (n < sizeof(file));
        in = file;
    #endif
    
    #ifdef SOURCE_MMAP
        num_samps = N_SAMPS;
        if ((in + N_SAMPS) >= (fp + (fsize / (sizeof(iq16_t))))) {
            in = fp;
        }
    #endif
    
    #ifdef DIRECT
        num_samps = nrx_samps_wb;
        TYPECPX24 *in_wb_samps = rx->wb_samps[RD_POS];
        //real_printf("<%d=%p> ", RD_POS, in_wb_samps); fflush(stdout);
        if (pos_wrap_diff(rx->wr_pos, RD_POS, N_WB_DPBUF) >= N_WB_DPBUF) {
            real_printf("X"); fflush(stdout);
        }
        RD_POS = (RD_POS+1) & (N_WB_DPBUF-1);
        rxc->rd++;
        //real_printf("."); fflush(stdout);
        //real_printf("%d ", num_samps); fflush(stdout);
    #endif
    
    out_bytes = num_samps * SIZEOF_SAMP;
    out->mh.BodySize = out_bytes;
    out_bytes += sizeof(struct SpyServerMessageHeader);

    #ifdef IQ16
        for (int j = 0; j < num_samps; j++, in++) {
            #ifdef DIRECT

                // zoom in fully to see the picket
                //#define PICKET
                #ifdef PICKET
                    u2_t i, q;
                    static u4_t ctr;
                    if ((ctr % 1024) == 0) i = 0xfff; else i = 0;
                    ctr++;
                    q = 0;
                #else
                    #if 1
                        u2_t i = (in_wb_samps[j].re >> 2) & 0xffff;
                        u2_t q = (in_wb_samps[j].im >> 2) & 0xffff;
                    #else
                        u2_t i = in_wb_samps[j].re & 0xffff;
                        u2_t q = in_wb_samps[j].im & 0xffff;
                    #endif
                #endif
            #else
                u2_t i = in->i;
                u2_t q = in->q;
            #endif
            
            #ifdef FLIP_IQ
                i = FLIP16(i);
                q = FLIP16(q);
            #endif
            
            #ifdef SWAP_IQ
                out->buf16[j].i = q;
                out->buf16[j].q = i;
            #else
                out->buf16[j].i = i;
                out->buf16[j].q = q;
            #endif
        }
        mg_send(mc, &out16, out_bytes);
    #else
        for (int j = 0; j < num_samps; j++, in++) {
            u1_t i = in->i & 0xff;
            u1_t q = in->q & 0xff;
            
            #ifdef SWAP_IQ
                out->buf8[j].i = q;
                out->buf8[j].q = i;
            #else
                out->buf8[j].i = i;
                out->buf8[j].q = q;
            #endif
        }
        mg_send(mc, &out8, out_bytes);
    #endif

    mc->is_resp = 0;    // response is complete
    mg_send_flush();
}

static void sdrpp_handler(struct mg_connection *mc, int ev, void *ev_data)
{
    switch (ev) {
        case MG_EV_OPEN:
            sdrpp_dprf("sdrpp_handler %s\n", mg_ev_names[ev]);

            if (sdrpp_WR_tid != -1) {
                TaskRemove(sdrpp_WR_tid);
                sdrpp_WR_tid = -1;
            }

            #ifdef FILE_IN
                if (fd != -1)
                    close(fd);
            #endif
            return;
            
        case MG_EV_ACCEPT:
            sdrpp_dprf("sdrpp_handler %s\n", mg_ev_names[ev]);
            mg_ev_setup_api_compat(mc);
            sdrpp_accept(mc, ev_data);
            return;
            
        case MG_EV_CLOSE: {
            sdrpp_dprf("sdrpp_handler %s %s:%d\n", mg_ev_names[ev], mc->remote_ip, mc->remote_port);
            //sdrpp_dprf("sdrpp_handler %s init=%d\n", mg_ev_names[ev]);

            #ifdef DIRECT
                #ifdef CAMP
                #else
                    rx_chan_t *rxc = &rx_channels[RX_CHAN0];
                    rxc->wb_task = 0;
                    rx_enable(RX_CHAN0, RX_CHAN_FREE);
                #endif
            #endif

            if (sdrpp_WR_tid != -1)
                TaskRemove(sdrpp_WR_tid);
            sdrpp_WR_tid = -1;
            
            #ifdef FILE_IN
                close(fd);
            #endif
            return;
        }

        case MG_EV_READ:
            //sdrpp_dprf("sdrpp_handler %s %s:%d\n", mg_ev_names[ev], mc->remote_ip, mc->remote_port);
            sdrpp_in(mc, ev_data);
            return;

        case MG_EV_WRITE:
            //sdrpp_dprf("sdrpp_handler %s %s:%d\n", mg_ev_names[ev], mc->remote_ip, mc->remote_port);
            return;

        case MG_EV_POLL:
            return;
            
        default:
            //printf("sdrpp_handler %s %s:%d => %d\n", mg_ev_names[ev], mc->remote_ip, mc->remote_port, mc->loc.port);
            sdrpp_dprf("sdrpp_handler %s\n", mg_ev_names[ev]);
            return;
    }
}

void sdrpp_server_init(struct mg_mgr *server)
{
    //if (!kiwi.isWB) return;
    if (mg_listen(server, "tcp://0.0.0.0:5555", sdrpp_handler, NULL) == NULL) {
        lprintf("network port 5555 in use?\n");
    }
}

#endif // MG_API_NEW
