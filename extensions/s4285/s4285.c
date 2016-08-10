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
	int rx_chan, run;
	int rx_task;
};

static s4285_t s4285[RX_CHANS];

void s4285_rx(void *param)
{
	while (1) {
		int rx_chan = TaskSleep(0);
		s4285_t *e = &s4285[rx_chan];
		while (m_CSt4285[rx_chan].process_rx_block())
			NextTask("s4285_rx");
	}
}

//void s4285_data(int rx_chan, int nsamps, TYPECPX *samps)
void s4285_data(int rx_chan, int nsamps, TYPEMONO16 *samps)
{
	s4285_t *e = &s4285[rx_chan];
	
	#define TX_LOOPBACK
	#ifdef TX_LOOPBACK
		//m_CSt4285[rx_chan].getTxOutput((void *) samps, nsamps, TYPE_IQ_F32_DATA, K_AMPMAX);
		m_CSt4285[rx_chan].getTxOutput((void *) samps, nsamps, TYPE_REAL_S16_DATA, K_AMPMAX);
		if (e->rx_task) TaskWakeup(e->rx_task, TRUE, e->rx_chan);
	#else
		m_CSt4285[rx_chan].process_rx_block(samps, nsamps, K_AMPMAX);
	#endif
}

u1_t s4285_tx_callback()
{
	return random() & 0xff;
}

bool s4285_msgs(char *msg, int rx_chan)
{
	s4285_t *e = &s4285[rx_chan];
	int n;
	
	printf("### s4285_msgs RX%d <%s>\n", rx_chan, msg);
	
	if (strcmp(msg, "SET ext_server_init") == 0) {
		e->rx_chan = rx_chan;	// remember our receiver channel number
		ext_send_msg(e->rx_chan, S4285_DEBUG_MSG, "EXT ready");
		return true;
	}

	n = sscanf(msg, "SET run=%d", &e->run);
	if (n == 1) {
		if (e->run) {
			if (!e->rx_task) {
				e->rx_task = CreateTask(s4285_rx, 0, EXT_PRIORITY);
			}
			m_CSt4285[rx_chan].reset();
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
};

void s4285_main()
{
	ext_register(&s4285_ext);
}

#endif
