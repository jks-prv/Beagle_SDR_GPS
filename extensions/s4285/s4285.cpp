// Copyright (c) 2000-2016, C.H Brain, G4GUO

#include "ext.h"

#ifndef EXT_S4285
	void s4285_main() {}
#else

#include "types.h"
#include "kiwi.h"
#include "datatypes.h"
#include "st4285.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define S4285_DEBUG_MSG	true
#define S4285_DEBUG_MSG	false

struct s4285_t {
	int rx_chan, run, mode, draw;
	int rx_task;

	float gain;
	double cma;
	u4_t ncma;
	int ring, points;
	#define N_IQ_RING (16*1024)
	float iq[N_IQ_RING][NIQ];
	u1_t plot[N_IQ_RING][2][NIQ];
	u1_t map[N_IQ_RING][NIQ];
};

static s4285_t s4285[MAX_RX_CHANS];

#define	MODE_RX				0
#define	MODE_TX_LOOPBACK	1

#define	DRAW_POINTS		0
#define	DRAW_DENSITY	1

#define N_RXBLK 512
#define N_RXBLKS 256
TYPEMONO16 s4285_rx_blocks[N_RXBLKS][512];
int s4285_rx_ra, s4285_rx_wa, s4285_rx_count, s4285_rx_count_max;

static void send_status(int rx_chan)
{
	char status[256];
	bool upd;
	if ((upd = m_CSt4285[rx_chan].get_status_text(status))) {
		ext_send_msg_encoded(rx_chan, S4285_DEBUG_MSG, "EXT", "status", "%s", status);
	}
	//printf("upd %d\n", upd? 1:0);
}

void s4285_rx(void *param)
{
	while (1) {
		int rx_chan = TaskSleep();
		s4285_t *e = &s4285[rx_chan];

		if (e->mode == MODE_TX_LOOPBACK) {
			while (m_CSt4285[rx_chan].process_rx_block_tx_loopback()) {
				TaskFastIntr("s4285_rx");
			}
		} else {
			while (s4285_rx_count) {
				m_CSt4285[rx_chan].process_rx_block(&s4285_rx_blocks[s4285_rx_ra][0], N_RXBLK, K_AMPMAX);
				s4285_rx_ra++;
				if (s4285_rx_ra == N_RXBLKS) s4285_rx_ra = 0;
				s4285_rx_count--;
				TaskFastIntr("s4285_rx");
				send_status(rx_chan);
			}
		}
		send_status(rx_chan);
	}
}

//void s4285_data(int rx_chan, int instance, int nsamps, TYPECPX *samps)
void s4285_data(int rx_chan, int instance, int nsamps, TYPEMONO16 *samps)
{
	s4285_t *e = &s4285[rx_chan];
	
	if (e->mode == MODE_TX_LOOPBACK) {
		//m_CSt4285[rx_chan].getTxOutput((void *) samps, nsamps, TYPE_IQ_F32_DATA, K_AMPMAX);
		m_CSt4285[rx_chan].getTxOutput((void *) samps, nsamps, TYPE_REAL_S16_DATA, K_AMPMAX);
		if (e->rx_task) TaskWakeupP(e->rx_task, TWF_CHECK_WAKING, e->rx_chan);
	} else {
		#if 0
		static u4_t last_time;
		u4_t now = timer_us();
		printf("s4285 nsamps %d %7.3f msec\n", nsamps, (float) (now - last_time) / 1e3);
		last_time = now;
		#endif
		
		assert(nsamps == N_RXBLK);
		memcpy(&s4285_rx_blocks[s4285_rx_wa][0], samps, sizeof(TYPEMONO16)*N_RXBLK);
		s4285_rx_wa++;
		if (s4285_rx_wa == N_RXBLKS) s4285_rx_wa = 0;
		s4285_rx_count++;
		if (s4285_rx_count > s4285_rx_count_max) {
			printf("%d\n", s4285_rx_count);
			s4285_rx_count_max = s4285_rx_count;
		}
		assert(s4285_rx_count < N_RXBLKS);
		if (e->rx_task) TaskWakeupP(e->rx_task, TWF_CHECK_WAKING, e->rx_chan);
	}
}

#define S4285_MAX_VAL 2.0

#define	PLOT_XY		200
#define	PLOT_MAX	(PLOT_XY-1)
#define	PLOT_HMAX	(PLOT_XY/2-1)

#define PLOT_FIXED_SCALE(d, iq) { \
	t = iq * e->gain; \
	if (t > S4285_MAX_VAL) t = S4285_MAX_VAL; \
	if (t < -S4285_MAX_VAL) t = -S4285_MAX_VAL; \
	t = t*PLOT_MAX / S4285_MAX_VAL; \
	t += PLOT_HMAX; \
	if (t > PLOT_MAX) t = PLOT_MAX; \
	if (t < 0) t = 0; \
	d = (u1_t) t; \
}

// FIXME: needs improvement
#define PLOT_AUTO_SCALE(d, iq) { \
	abs_iq = fabs(iq); \
	e->cma = (e->cma * e->ncma) + abs_iq; \
	e->ncma++; \
	e->cma /= e->ncma; \
	t = iq / (4 * e->cma) * PLOT_MAX; \
	t += PLOT_HMAX; \
	if (t > PLOT_MAX) t = PLOT_MAX; \
	if (t < 0) t = 0; \
	d = (u1_t) t; \
}

void s4285_rx_callback(int rx_chan, FComplex *samps, int nsamps, int incr)
{
	s4285_t *e = &s4285[rx_chan];
	int i;
	int ring = e->ring;

#if 0
	float maxx=0;
	for (i=0; i<nsamps; i++) {
		float nI = (float) samps[i].re;
		float nQ = (float) samps[i].im;
		if (nI > maxx) maxx = nI;
		if (nQ > maxx) maxx = nQ;
	}
	printf("maxx %f\n", maxx);
#endif

	if (e->draw == DRAW_POINTS) {
		for (i=0; i<nsamps; i += incr) {
			float oI = e->iq[ring][I];
			float oQ = e->iq[ring][Q];
			float nI = (float) samps[i].re;
			float nQ = (float) samps[i].im;
			
			float t; double abs_iq;
			
			if (e->gain) {
				PLOT_FIXED_SCALE(e->plot[ring][0][I], oI);
				PLOT_FIXED_SCALE(e->plot[ring][0][Q], oQ);
				PLOT_FIXED_SCALE(e->plot[ring][1][I], nI);
				PLOT_FIXED_SCALE(e->plot[ring][1][Q], nQ);
			} else {
				PLOT_AUTO_SCALE(e->plot[ring][0][I], oI);
				PLOT_AUTO_SCALE(e->plot[ring][0][Q], oQ);
				PLOT_AUTO_SCALE(e->plot[ring][1][I], nI);
				PLOT_AUTO_SCALE(e->plot[ring][1][Q], nQ);
			}
			
			e->iq[ring][I] = nI;
			e->iq[ring][Q] = nQ;
			ring++;
			if (ring >= e->points) {
				ext_send_msg_data(rx_chan, S4285_DEBUG_MSG, e->draw, &(e->plot[0][0][0]), e->points*4 +1);
				ring = 0;
			}
		}
	} else

	if (e->draw == DRAW_DENSITY) {
		for (i=0; i<nsamps; i += incr) {
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
				ext_send_msg_data(rx_chan, S4285_DEBUG_MSG, e->draw, &(e->map[0][0]), e->points*2 +1);
				ring = 0;
			}
		}
	}

	e->ring = ring;
}

u1_t s4285_tx_callback()
{
	return random() & 0xff;
	//return 0;
}

bool s4285_msgs(char *msg, int rx_chan)
{
	s4285_t *e = &s4285[rx_chan];
    e->rx_chan = rx_chan;	// remember our receiver channel number
	int n;
	
	printf("### s4285_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		ext_send_msg(rx_chan, S4285_DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
		if (e->run) {
			if (!e->rx_task) {
				e->rx_task = CreateTaskF(s4285_rx, 0, EXT_PRIORITY, CTF_RX_CHANNEL | (rx_chan & CTF_CHANNEL));
			}
			m_CSt4285[rx_chan].reset();
			//m_CSt4285[rx_chan].setSampleRate(ext_update_get_sample_rateHz());
			m_CSt4285[rx_chan].registerRxCallback(s4285_rx_callback, rx_chan);
			m_CSt4285[rx_chan].registerTxCallback(s4285_tx_callback);
			//m_CSt4285[rx_chan].control((void *) "SET MODE 600L", NULL, 0);
			//ext_register_receive_iq_samps(s4285_data, rx_chan);
			ext_register_receive_real_samps(s4285_data, rx_chan);
		} else {
			ext_unregister_receive_iq_samps(rx_chan);
			if (e->rx_task) {
				TaskRemove(e->rx_task);
				e->rx_task = 0;
			}
		}
		if (e->points == 0)
			e->points = 128;
		return true;
	}
	
	n = sscanf(msg, "SET mode=%d", &e->mode);
	if (n == 1) {
		return true;
	}
	
	int gain;
	n = sscanf(msg, "SET gain=%d", &gain);
	if (n == 1) {
		// 0 .. +100 dB of S4285_MAX_VAL
		e->gain = gain? pow(10.0, ((float) -gain) / 10.0) : 0;
		printf("e->gain %.6f\n", e->gain);
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
		return true;
	}
	
	n = strcmp(msg, "SET clear");
	if (n == 0) {
		e->cma = e->ncma = 0;
		return true;
	}
	
	return false;
}

void s4285_close(int rx_chan)
{

}

void s4285_main();

ext_t s4285_ext = {
	"s4285",
	s4285_main,
	s4285_close,
	s4285_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void s4285_main()
{
	ext_register(&s4285_ext);
}

#endif
