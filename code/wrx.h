#ifndef _WRX_H_
#define _WRX_H_

#include "types.h"
#include "wrx.gen.h"
#include "web.h"
#include "coroutines.h"

#define	I	0
#define	Q	1
#define	IQ	2

// ADC clk generated from FPGA via multiplied GPS TCXO
	#define	GPS_CLOCK			(16.368*MHz)		// 61.095 ns
	#define ADC_CLOCK_65M		(GPS_CLOCK*4)		// 65.472 MHz 15.274 ns
	#define ADC_CLOCK_49M		(GPS_CLOCK*3)		// 49.104 MHz
	#define ADC_CLOCK_82M		(GPS_CLOCK*5)		// 81.840 MHz

// use 80M ADC clk on HF1, generate sync 20 MHz CPU clock in FPGA (GPS won't work)
	#define	ADC_CLOCK_80M_SYNC	(80.0*MHz)			// 12.500 ns

// use 80M ADC clk on HF1, generate async CPU/GPS clock from GPS TCXO
	#define	ADC_CLOCK_80M_ASYNC	(80.0*MHz)			// 12.500 ns

#define	ADC_MAX		0x1fff		// +8K
#define	ADC_MIN		0x2000		// -8K

// S-Meter calibration offset added to make reading absolute dBm
// Measured against 8642A as source on 11-Oct-14
#define SMETER_CALIBRATION -12	

#define	WF_SPEED_SLOW		10
#define	WF_SPEED_MED		15
#define	WF_SPEED_FAST		0		// fixme: unlimited for testing

#ifdef SPI_32
	#define	WF_SPEED_MAX		30
#else
	#define	WF_SPEED_MAX		30
#endif

#define	WF_SPEED_OTHERS		1

#define	WEB_SERVER_POLL_MS	(1000 / WF_SPEED_MAX / 2)

extern conn_t conns[RX_CHANS*2];
extern bool background_mode;
extern int debug, p0, p1, p2, wf_sim, wf_real, wf_time, wf_dump, wf_flip, wf_exit, wf_start, tone, down, navg,
	rx_cordic, rx_cic, rx_cic2, rx_dump, wf_cordic, wf_cic, wf_mult, wf_mult_gen, show_adc, wf_pipe, wspr, meas,
	rx_yield, gps_chans, spi_clkg, spi_speed, wf_max, rx_num, wf_num, do_slice, do_gps, do_wrx, wf_olap,
	spi_delay, do_fft, noisePwr, unwrap, rev_iq, ineg, qneg, fft_file, fftsize, fftuse, bg, do_owrx, owrx,
	color_map, port_override;
extern float g_genfreq, g_genampl, g_mixfreq;
extern double adc_clock;

extern int rx0_wakeup;
extern u4_t rx0_twoke;
extern volatile int audio_bytes, waterfall_bytes, waterfall_frames, http_bytes;

#define	ROM_WIDTH	16
#define	ROM_DEPTH	13
extern u4_t rom_sin[1<<ROM_DEPTH];


// sound
extern const char *mode_s[];
// = { "am", "ssb", NULL };
enum mode_e { MODE_AM, MODE_SSB };

// waterfall
#define	WF_SPLIT			0
#define	WF_SPECTRUM			1
#define	WF_NORM				2
#define	WF_WEAK				3
#define	WF_STRONG			4

#define META_WSPR_INIT		8
#define META_WSPR_SYNC		9
#define META_WSPR_DATA		10
#define	META_WSPR_PEAKS		11
#define META_WSPR_DECODING	12
#define META_WSPR_DECODED	13

#define	KEEPALIVE_MS		10000

#define GPS_TASKS			(GPS_CHANS + 3)			// chan*n + search + solve + user
#define	RX_TASKS			(RX_CHANS*2 + 1)		// chan*n + web
#define	MAX_TASKS			(GPS_TASKS + RX_TASKS + 4)

void rx_server_init();
void rx_server_remove(conn_t *c);
conn_t *rx_server_websocket(struct mg_connection *mc);

void w2a_sound_init();
void w2a_sound(void *param);

void w2a_waterfall_init();
void w2a_waterfall(void *param);

void loguser(conn_t *c, const char *type);
void webserver_print_stats();

#endif
