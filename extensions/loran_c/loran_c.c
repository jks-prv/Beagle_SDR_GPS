// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#ifndef EXT_LORAN_C
	void loran_c_main() {}
#else

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define LORAN_C_DEBUG_MSG	true
#define LORAN_C_DEBUG_MSG	false

#define	SCOPE_DATA	0
#define	SCOPE_RESET	1

#define	GRI_2_SEC(gri)	(double (gri) / 1e5)

#define MAX_SRATE		15000
#define MAX_GRI			9999
#define	MAX_GRI_RATE	10
#define	FRI_PER_GRI		2
#define	GRI_2_MSEC(gri)	((gri) / 100)
//#define	MAX_BUCKET		(MAX_SRATE / MAX_GRI_RATE * FRI_PER_GRI)
#define	MAX_BUCKET		(MAX_SRATE / MAX_GRI_RATE)

// rx_chan is the receiver channel number we've been assigned, 0..RX_CHAN
// We need this so the extension can support multiple users, each with their own loran_c[] data structure.

struct loran_c_ch_t {
	u4_t gri, samp, nbucket, dsp_samps, avg_samps, navgs;
	double samp_per_GRI;
	float avg[MAX_BUCKET];
	//float avgIQ[NIQ][MAX_BUCKET];
	float gain, max;
	int offset, avg_algo, avg_param;
	bool restart;
};

struct loran_c_t {
	int rx_chan;
	
	u4_t i_srate;
	double srate;
	
	#define	NCH 2
	loran_c_ch_t ch[NCH];

	u1_t scope[MAX_BUCKET];
	bool redraw_legend;
};

static loran_c_t loran_c[RX_CHANS];

#define LORAN_C_MAX_PWR ((2 * CUTESDR_MAX_VAL * CUTESDR_MAX_VAL)-1)

#define AVG_CMA		0
#define AVG_EMA		1
#define AVG_IIR		2

//#define AVG_FRI
#define USE_IQ

#ifdef USE_IQ
static void loran_c_data(int rx_chan, int chan, int nsamps, TYPECPX *samps)
#else
static void loran_c_data(int rx_chan, int chan, int nsamps, TYPEMONO16 *samps)
#endif
{
	loran_c_t *e = &loran_c[rx_chan];
	int i, j, ch;
	loran_c_ch_t *c;
	
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
			int bn = floor(fmod(c->samp - c->offset, c->samp_per_GRI));

			if (bn == 0 && c->dsp_samps > e->i_srate) {
				c->dsp_samps = 0;
				
				//printf("scope: ");
				if (c->gain == 0) {		// auto-scale
					c->max = 0;
					for (j=0; j < c->nbucket; j++) {
						if (c->avg[j] > c->max)
							c->max = c->avg[j];
					}
				
				} else {
					//printf("ch%d LORAN_C_MAX_PWR 0x%x max %.3f gain %.3f %.3f %d\n",
					//	ch, (int) LORAN_C_MAX_PWR, c->max, gain, CUTESDR_MAX_VAL * gain, c->gain);
					c->max =  c->gain * CUTESDR_MAX_VAL;
				}
				//if (ch == 0) printf("ch%d gain %f max %f navgs %d\n", ch, c->gain, c->max, c->navgs);

				for (j=0; j < c->nbucket; j++) {
					int scope;
					
					float avg = c->avg[j];
					if (avg > c->max) avg = c->max;
					if (avg < 0) avg = 0;
					scope = c->max? (255 * (avg / c->max)) : 0;
	
					//if (j < 16) printf("%4d ", scope);
					e->scope[j+1] = scope;
				}
				//printf("\n");
		
				e->scope[0] = ch;
				ext_send_msg_data(rx_chan, LORAN_C_DEBUG_MSG,
					e->redraw_legend? SCOPE_RESET : SCOPE_DATA, e->scope, c->nbucket+1);
				e->redraw_legend = false;
			}
			c->dsp_samps++;

			if (c->avg_algo == AVG_CMA) {
				if (bn == 0 && (c->restart || (c->avg_samps > (e->i_srate * c->avg_param)))) {
					if (c->restart) {
						//printf("### ch%d restart\n", ch);
						c->restart = false;
						c->dsp_samps = 0;
					}
	
					//printf("ch%d averages-%d reset\n", ch, c->avg_param);
	
					for (j=0; j < c->nbucket; j++) {
						c->avg[j] = 0;
					}
	
					c->avg_samps = 0;
					c->navgs = -1;
					//printf("AVG RESET samp %d bn %d navgs %d nbucket %d\n", c->samp, bn, c->navgs, c->nbucket);
				}
				c->avg_samps++;

				if (bn == 0)
					c->navgs++;
				if (bn < c->nbucket-1) {
					c->avg[bn] = (c->avg[bn] * c->navgs) + pwr;
					c->avg[bn] /= c->navgs + 1;
				}
			} else

			if (c->avg_algo == AVG_EMA) {
				if (bn == 0 && c->restart) {
					if (c->restart) {
						//printf("### ch%d restart\n", ch);
						c->restart = false;
						c->dsp_samps = 0;
					}
	
					for (j=0; j < c->nbucket; j++) {
						c->avg[j] = 0;
					}
				}
				
				if (bn < c->nbucket-1) {
					#define DECAY 256
					c->avg[bn] += (pwr - c->avg[bn]) / DECAY;
				}
			} else

			if (c->avg_algo == AVG_IIR) {
				if (bn == 0 && c->restart) {
					if (c->restart) {
						//printf("### ch%d restart\n", ch);
						c->restart = false;
						c->dsp_samps = 0;
					}
	
					for (j=0; j < c->nbucket; j++) {
						c->avg[j] = 0;
					}
				}
				
				if (bn < c->nbucket-1) {
					float iir_gain = 1.0 - expf(-0.2 * pwr/CUTESDR_MAX_VAL);
					c->avg[bn] += (pwr - c->avg[bn]) * iir_gain;
				}
			} else
			
				panic("bad avg_algo");

			c->samp++;
		}
    }
}

static void init_gri(loran_c_t *e, int ch, int gri)
{
	loran_c_ch_t *c = &(e->ch[ch]);
	c->gri = gri;
	c->samp_per_GRI = e->srate * GRI_2_SEC(gri);
	c->nbucket = floor(c->samp_per_GRI) + 1;
}

bool loran_c_msgs(char *msg, int rx_chan)
{
	loran_c_t *e = &loran_c[rx_chan];
	loran_c_ch_t *c;
	int n, ch;
	
	//printf("loran_c_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		memset(e, 0, sizeof(loran_c_t));
		e->rx_chan = rx_chan;
		e->srate = ext_update_get_sample_rateHz(rx_chan);
		e->i_srate = SND_RATE;
		ext_send_msg(rx_chan, LORAN_C_DEBUG_MSG, "EXT ms_per_bin=%.9f ready", 1.0/e->srate * 1e3);
		return true;
	}
	
	int i_gri;
	n = sscanf(msg, "SET gri%d=%d", &ch, &i_gri);
	if (n == 2) {
		c = &(e->ch[ch]);
		init_gri(e, ch, i_gri);
		printf("loran_c RX%d srate = %.1f, samp_per_GRI = %.1f\n",
			rx_chan, e->srate, c->samp_per_GRI);
		printf("loran_c RX%d i_srate = %d, nbucket = %d\n",
			rx_chan, e->i_srate, c->nbucket);
		e->redraw_legend = true;
		c->restart = true;
		return true;
	}

	int offset;
	n = sscanf(msg, "SET offset%d=%d", &ch, &offset);
	if (n == 2) {
		c = &(e->ch[ch]);
		c->offset = (c->offset + offset) % c->nbucket;
		c->restart = true;
		//printf("loran_c ch%d offset %d nbucket %d offset %d\n", ch, offset, c->nbucket, c->offset);
		return true;
	}
	
	int gain;
	n = sscanf(msg, "SET gain%d=%d", &ch, &gain);
	if (n == 2) {
		// 0 .. -100 dB of CUTESDR_MAX_VAL
		c = &(e->ch[ch]);
		c->gain = gain? pow(10.0, ((float) -gain) / 10.0) : 0;
		c->restart = true;
		return true;
	}
	
	int avg_algo;
	n = sscanf(msg, "SET avg_algo%d=%d", &ch, &avg_algo);
	if (n == 2) {
		c = &(e->ch[ch]);
		c->avg_algo = avg_algo;
		c->restart = true;
		return true;
	}
	
	int avg_param;
	n = sscanf(msg, "SET avg_param%d=%d", &ch, &avg_param);
	if (n == 2) {
		c = &(e->ch[ch]);
		c->avg_param = avg_param;
		c->restart = true;
		return true;
	}
	
	if (strcmp(msg, "SET start") == 0) {
		printf("### loran_c start\n");
		e->redraw_legend = true;
		e->ch[0].restart = e->ch[1].restart = true;
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
	NULL,
	loran_c_msgs,
};

void loran_c_main()
{
	ext_register(&loran_c_ext);
}

#endif
