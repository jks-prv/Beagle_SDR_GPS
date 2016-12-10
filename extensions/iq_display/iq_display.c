// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#ifndef EXT_IQ_DISPLAY
	void iq_display_main() {}
#else

#include "gps.h"
#include "st4285.h"
#include "fmdemod.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define IQ_DISPLAY_DEBUG_MSG	true
#define IQ_DISPLAY_DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..RX_CHAN
// We need this so the extension can support multiple users, each with their own iq_display[] data structure.

#define N_CH		2
#define N_HISTORY	2

struct iq_display_t {
	int rx_chan;
	int run, draw, s4285;
	float send_cmaIQ_nsamp, nsamp, gain;
	double cma;
	u4_t ncma;
	int ring[N_CH], points;
	#define N_IQ_RING (16*1024)
	float iq[N_CH][N_IQ_RING][IQ];
	u1_t plot[N_CH][N_IQ_RING][N_HISTORY][IQ];
	u1_t map[N_IQ_RING][IQ];
	
	int cmaN;
	double cmaI, cmaQ;
};

static iq_display_t iq_display[RX_CHANS];

#define	IQ_POINTS		0
#define	IQ_DENSITY		1
#define	IQ_S4285_P		2
#define	IQ_S4285_D		3
#define	IQ_CLEAR		4

#define PLOT_FIXED_SCALE(d, iq) { \
	t = iq * (ch? (e->gain*20) : e->gain); \
	if (t > CUTESDR_MAX_VAL) t = CUTESDR_MAX_VAL; \
	if (t < -CUTESDR_MAX_VAL) t = -CUTESDR_MAX_VAL; \
	t = t*255 / CUTESDR_MAX_VAL; \
	t += 127; \
	if (t > 255) t = 255; \
	if (t < 0) t = 0; \
	d = (u1_t) t; \
}

// FIXME: needs improvement
#define PLOT_AUTO_SCALE(d, iq) { \
	abs_iq = fabs(iq); \
	e->cma = (e->cma * e->ncma) + abs_iq; \
	e->ncma++; \
	e->cma /= e->ncma; \
	t = iq / (4 * e->cma) * 255; \
	t += 127; \
	if (t > 255) t = 255; \
	if (t < 0) t = 0; \
	d = (u1_t) t; \
}

void iq_display_data(int rx_chan, int ch, int nsamps, TYPECPX *samps)
{
	iq_display_t *e = &iq_display[rx_chan];
	int i;
	int ring;
	int cmd = (e->draw << 1) + (ch & 1);
	
	if (e->s4285)
		m_CSt4285[rx_chan].getTxOutput((void *) samps, nsamps, TYPE_IQ_F32_DATA, K_AMPMAX/4);

	ring = e->ring[ch];
	
	if (e->draw == IQ_POINTS || e->draw == IQ_S4285_P) {
		for (i=0; i<nsamps; i++) {
			float oI = e->iq[ch][ring][I];
			float oQ = e->iq[ch][ring][Q];
			float nI = (float) samps[i].re;
			float nQ = (float) samps[i].im;
			
			float t; double abs_iq;
			
			if (e->gain) {
				PLOT_FIXED_SCALE(e->plot[ch][ring][0][I], oI);
				PLOT_FIXED_SCALE(e->plot[ch][ring][0][Q], oQ);
				PLOT_FIXED_SCALE(e->plot[ch][ring][1][I], nI);
				PLOT_FIXED_SCALE(e->plot[ch][ring][1][Q], nQ);
			} else {
				PLOT_AUTO_SCALE(e->plot[ch][ring][0][I], oI);
				PLOT_AUTO_SCALE(e->plot[ch][ring][0][Q], oQ);
				PLOT_AUTO_SCALE(e->plot[ch][ring][1][I], nI);
				PLOT_AUTO_SCALE(e->plot[ch][ring][1][Q], nQ);
			}
			
			e->iq[ch][ring][I] = nI;
			e->iq[ch][ring][Q] = nQ;
			ring++;
			if (ring >= e->points) {
				ext_send_msg_data(rx_chan, IQ_DISPLAY_DEBUG_MSG, cmd, &(e->plot[ch][0][0][0]), e->points*4 +1);
				ring = 0;
			}
		}
	} else

	if (e->draw == IQ_DENSITY || e->draw == IQ_S4285_D) {
		if (ch == 0) for (i=0; i<nsamps; i++) {
			float nI = (float) samps[i].re;
			float nQ = (float) samps[i].im;
			
			float t; double abs_iq;
			
			if (e->gain) {
				PLOT_FIXED_SCALE(e->map[ring][I], nI);
				PLOT_FIXED_SCALE(e->map[ring][Q], nQ);
			} else {
				PLOT_AUTO_SCALE(e->map[ring][I], nI);
				PLOT_AUTO_SCALE(e->map[ring][Q], nQ);
			}
			
			ring++;
			if (ring >= e->points) {
				ext_send_msg_data(rx_chan, IQ_DISPLAY_DEBUG_MSG, cmd, &(e->map[0][0]), e->points*2 +1);
				ring = 0;
			}
		}
	}

	e->ring[ch] = ring;
	
	for (i=0; i<nsamps; i++) {
		e->cmaI = (e->cmaI * e->cmaN) + (double) samps[i].re;
		e->cmaI /= e->cmaN+1;
		e->cmaQ = (e->cmaQ * e->cmaN) + (double) samps[i].im;
		e->cmaQ /= e->cmaN+1;
		e->cmaN++;
	}

	e->nsamp += nsamps;
	if (e->nsamp > e->send_cmaIQ_nsamp) {
		ext_send_msg(e->rx_chan, IQ_DISPLAY_DEBUG_MSG, "EXT cmaI=%e cmaQ=%e", e->cmaI, e->cmaQ);
		e->nsamp -= e->send_cmaIQ_nsamp;
	}
}

u1_t iq_display_s4285_tx_callback()
{
	return random() & 0xff;
	//return 0;
}

bool iq_display_msgs(char *msg, int rx_chan)
{
	iq_display_t *e = &iq_display[rx_chan];
	int n;
	
	//printf("### iq_display_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, IQ_DISPLAY_DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
		if (e->run) {
			e->send_cmaIQ_nsamp = ext_get_sample_rateHz() / 4;
			ext_register_receive_iq_samps(iq_display_data, rx_chan);
		} else {
			ext_unregister_receive_iq_samps(rx_chan);
		}
		if (e->points == 0)
			e->points = 1024;
		return true;
	}
	
	int gain;
	n = sscanf(msg, "SET gain=%d", &gain);
	if (n == 1) {
		// 0 .. +100 dB of CUTESDR_MAX_VAL
		#ifdef NBFM_PLL_DEBUG
			static int initGain;
			if (!initGain) {
				printf("initGain\n");
				gain = 76;
				initGain = 1;
			}
		#endif
		e->gain = gain? pow(10.0, ((float) gain - 50) / 10.0) : 0;
		printf("e->gain %d dB %.6f\n", gain-50, e->gain);
		return true;
	}
	
	int points;
	n = sscanf(msg, "SET points=%d", &points);
	if (n == 1) {
		e->points = points;
		printf("points %d\n", points);
		return true;
	}
	
	int draw;
	n = sscanf(msg, "SET draw=%d", &draw);
	if (n == 1) {
		e->draw = draw;
		printf("draw %d\n", draw);
		e->s4285 = 0;
		if (draw == IQ_S4285_P || draw == IQ_S4285_D) {
			e->s4285 = 1;
			e->gain = 1;
			m_CSt4285[rx_chan].reset();
			m_CSt4285[rx_chan].registerTxCallback(iq_display_s4285_tx_callback);
			//m_CSt4285[rx_chan].control((void *) "SET MODE 600L", NULL, 0);
			//m_CSt4285[rx_chan].setSampleRate(ext_get_sample_rateHz());
		}
		return true;
	}
	
	float offset;
	n = sscanf(msg, "SET offset=%f", &offset);
	if (n == 1) {
		adc_clock -= adc_clock_offset;
		adc_clock_offset = offset;
		adc_clock += adc_clock_offset;
		gps.adc_clk_corr++;
		printf("adc_clock %.6f offset %.2f\n", adc_clock/1e6, offset);
		return true;
	}
	
	// FIXME: need to check admin credential on server side before allowing this
	double dc_off_I, dc_off_Q;
	n = sscanf(msg, "SET DC_offset I=%lf Q=%lf", &dc_off_I, &dc_off_Q);
	if (n == 2) {
		DC_offset_I += dc_off_I;
		DC_offset_Q += dc_off_Q;
		printf("DC_offset: I %.4lg/%.4lg Q %.4lg/%.4lg\n", dc_off_I, DC_offset_I, dc_off_Q, DC_offset_Q);

		// FIXME: this will always fail until we add some check allowing credentialed set from non-admin connection
		//cfg_set_float("DC_offset_I", DC_offset_I);
		//cfg_set_float("DC_offset_Q", DC_offset_Q);
		return true;
	}
	
	n = strcmp(msg, "SET clear");
	if (n == 0) {
		printf("cmaN %d cmaI %e cmaQ %e\n", e->cmaN, e->cmaI, e->cmaQ);
		e->cma = e->ncma = e->cmaI = e->cmaQ = e->cmaN = e->nsamp = 0;
		return true;
	}
	
	return false;
}

void iq_display_close(int rx_chan)
{

}

void iq_display_main();

ext_t iq_display_ext = {
	"iq_display",
	iq_display_main,
	iq_display_close,
	iq_display_msgs,
};

void iq_display_main()
{
	ext_register(&iq_display_ext);
}

#endif
