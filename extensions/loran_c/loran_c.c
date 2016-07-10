// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "loran_c.h"

#ifndef EXT_LORAN_C
	void loran_c_main() {}
#else

#include "kiwi.h"
#include "data_pump.h"
#include "ext.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define LORAN_C_DEBUG_MSG	true
#define LORAN_C_DEBUG_MSG	false

#define	SCOPE_DATA	0
#define	SCOPE_RESET	1

#define MAX_SRATE		15000
#define MAX_GRI			9999
#define	GRI_2_SEC(gri)	(double (gri) / 1e5)
#define	MAX_BUCKET		(MAX_SRATE * (MAX_GRI/100))

// rx_chan is the receiver channel number we've been assigned, 0..RX_CHAN
// We need this so the extension can support multiple users, each with their own loran_c[] data structure.

struct loran_c_ch_t {
	u4_t samp, nbucket, dsp_samps, avg_samps, navgs;
	double gri, samp_per_GRI;
	float cma[MAX_BUCKET];
	float gain, max;
	int offset;
	bool restart;
};

struct loran_c_t {
	int rx_chan;
	
	u4_t i_srate;
	double srate;
	
	#define	NCH 2
	struct loran_c_ch_t ch[NCH];

	u1_t scope[MAX_BUCKET];
	bool reset;
};

static loran_c_t loran_c[RX_CHANS];

//#define	CUTESDR_SCALE	15			// +/- 1.0 -> +/- 32.0K (s16 equivalent)
#define MAX_VAL ((float) ((1 << CUTESDR_SCALE) - 1))
#define MAX_PWR ((2 * MAX_VAL * MAX_VAL)-1)

#define USE_IQ

#ifdef USE_IQ
static void loran_c_data(int rx_chan, int nsamps, TYPECPX *samps)
#else
static void loran_c_data(int rx_chan, int nsamps, TYPEMONO16 *samps)
#endif
{
	loran_c_t *e = &loran_c[rx_chan];
	int i, j, ch;
	struct loran_c_ch_t *c;
	
    for (i=0; i < nsamps; i++) {
    	#ifdef USE_IQ
			float re = (float) samps[i].re;
			float im = (float) samps[i].im;
			float pwr = re*re + im*im;
		#else
			float pwr = abs(samps[i]);		// really amplitude, not power
		#endif
		
		for (ch=0; ch < NCH; ch++) {
			c = &(e->ch[ch]);
			int bn = round(fmod(c->samp + c->offset, c->samp_per_GRI));

			if (bn == 0 && c->dsp_samps > e->i_srate) {
				c->dsp_samps = 0;
				
				//printf("scope: ");
				c->max = 0;
				for (j=0; j < c->nbucket; j++) {
					if (c->cma[j] > c->max)
						c->max = c->cma[j];
				}
				
				if (c->gain != 0) {
					//printf("ch%d MAX_PWR 0x%x max %.3f gain %.3f %.3f %d\n",
					//	ch, (int) MAX_PWR, c->max, gain, MAX_VAL * gain, c->gain);
					c->max = MAX_VAL * c->gain;
				}
//if (ch == 1) printf("ch%d gain %f max %f navgs %d\n", ch, c->gain, c->max, c->navgs);

				for (j=0; j < c->nbucket; j++) {
					int scope;
					
					float avg = c->cma[j];
					if (avg > c->max) avg = c->max;
					if (avg < 0) avg = 0;
					scope = c->max? (255 * (avg / c->max)) : 0;
					c->cma[j] = 0;
	
					//if (j < 16) printf("%4d ", scope);
					e->scope[j+1] = scope;
				}
				//printf("\n");
		
				e->scope[0] = ch;
				ext_send_data_msg(rx_chan, LORAN_C_DEBUG_MSG,
					e->reset? SCOPE_RESET : SCOPE_DATA, e->scope, c->nbucket+1);
				e->reset = false;
			}
			c->dsp_samps++;

			if (c->restart || (bn == 0 && c->avg_samps > e->i_srate*60)) {
				if (c->restart) {
					printf("### ch%d restart\n", ch);
					c->restart = false;
					c->dsp_samps = 0;
				}

				printf("ch%d avg zero\n", ch);
				c->avg_samps = 0;
				for (j=0; j < c->nbucket; j++) {
					c->cma[j] = 0;
				}
				c->navgs = -1;
			}
			c->avg_samps++;

			// CMA
			if (bn == 0) c->navgs++;
			c->cma[bn] = (c->cma[bn] * c->navgs) + pwr;
			c->cma[bn] /= c->navgs + 1;
			c->samp++;
		}
    }
}

static void init_gri(loran_c_t *e, int ch, int gri)
{
	struct loran_c_ch_t *c = &(e->ch[ch]);
	c->gri = gri;
	c->samp_per_GRI = e->srate * GRI_2_SEC(gri);
	c->nbucket = lround(c->samp_per_GRI) + 1;
}

bool loran_c_msgs(char *msg, int rx_chan)
{
	loran_c_t *e = &loran_c[rx_chan];
	int n;
	
	printf("loran_c_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		memset(e, 0, sizeof(loran_c_t));
		e->rx_chan = rx_chan;
		e->srate = ext_get_sample_rateHz();
		e->i_srate = lround(e->srate);
		ext_send_msg(rx_chan, LORAN_C_DEBUG_MSG, "EXT ms_per_bin=%.9f ready", 1.0/e->srate * 1e3);
		return true;
	}
	
	int i_gri0, i_gri1;
	n = sscanf(msg, "SET gri0=%d gri1=%d", &i_gri0, &i_gri1);
	if (n == 2) {
		init_gri(e, 0, i_gri0);
		init_gri(e, 1, i_gri1);
		printf("loran_c RX%d srate = %.1f, samp_per_GRI = %.1f/%.1f\n",
			rx_chan, e->srate, e->ch[0].samp_per_GRI, e->ch[1].samp_per_GRI);
		printf("loran_c RX%d i_srate = %d, nbucket = %d/%d\n",
			rx_chan, e->i_srate, e->ch[0].nbucket, e->ch[1].nbucket);
		e->reset = true;
		return true;
	}

	int off0, off1;
	n = sscanf(msg, "SET offset0=%d offset1=%d", &off0, &off1);
	if (n == 2) {
		struct loran_c_ch_t *c;
		c = &(e->ch[0]);
		c->offset = c->nbucket * off0 / 100;
		c->restart = true;
		//printf("loran_c ch%d nbucket %d off %d %d%%\n", 0, c->nbucket, c->offset, off0);
		c = &(e->ch[1]);
		c->offset = c->nbucket * off1 / 100;
		c->restart = true;
		//printf("loran_c ch%d nbucket %d off %d %d%%\n", 1, c->nbucket, c->offset, off1);
		return true;
	}
	
	int gain0, gain1;
	n = sscanf(msg, "SET gain0=%d gain1=%d", &gain0, &gain1);
	if (n == 2) {
		e->ch[0].gain = gain0? pow(10.0, (-gain0)/10) : 0;
		e->ch[1].gain = gain1? pow(10.0, (-gain1)/10) : 0;
		e->ch[0].restart = e->ch[1].restart = true;
		return true;
	}
	
	if (strcmp(msg, "SET start") == 0) {
		printf("### loran_c start\n");
		e->reset = true;
		#ifdef USE_IQ
			ext_register_receive_iq_samps(loran_c_data, rx_chan);
		#else
			ext_register_receive_real_samps(loran_c_data, rx_chan);
		#endif
		return true;
	}

	if (strcmp(msg, "SET stop") == 0) {
		printf("### loran_c stop\n");
		#ifdef USE_IQ
			ext_unregister_receive_iq_samps(e->rx_chan);
		#else
			ext_unregister_receive_real_samps(e->rx_chan);
		#endif
		return true;
	}

	return false;
}

void loran_c_main();

ext_t loran_c_ext = {
	"loran_c",
	loran_c_main,
	loran_c_msgs,
};

void loran_c_main()
{
	ext_register(&loran_c_ext);
}

#endif
