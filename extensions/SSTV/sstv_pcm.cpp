/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 * Copyright (c) 2007-2013, Oona Räisänen (OH2EIQ [at] sral.fi)
 */

#include "types.h"
#include "mem.h"
#include "sstv.h"

void sstv_pcm_once(sstv_chan_t *e)
{
    assert((PCM_BUFLEN % FASTFIR_OUTBUF_SIZE) == 0);
    e->pcm.Buffer = (s2_t *) kiwi_imalloc("sstv_pcm_once", PCM_BUFLEN * sizeof(s2_t));
}

void sstv_pcm_init(sstv_chan_t *e)
{
    e->pcm.WindowPtr = 0;
    e->rd_pos = e->rd_idx = 0;
    e->seq_init = false;
}

static void pcm_copy(sstv_chan_t *e, int idx, int nsamps)
{
    rx_dpump_t *rx = &rx_dpump[e->rx_chan];
    
    #if 1
    
    while (nsamps) {
        int cnt = MIN(nsamps, FASTFIR_OUTBUF_SIZE - e->rd_idx);
        assert(cnt > 0 && (idx+cnt) <= PCM_BUFLEN);
        //printf("idx=%d cnt=%d nsamps=%d\n", idx, cnt, nsamps);
        memcpy(&e->pcm.Buffer[idx], &rx->real_samples_s2[e->rd_pos][e->rd_idx], cnt * sizeof(s2_t));
        nsamps -= cnt;
        idx += cnt;
        e->rd_idx += cnt;
        if (e->rd_idx >= FASTFIR_OUTBUF_SIZE) {
            e->rd_idx = 0;
            e->rd_pos = (e->rd_pos+1) & (N_DPBUF-1);

            if (0 && e->rd_pos != rx->real_wr_pos) {
                int diff = (int) rx->real_wr_pos - (int) e->rd_pos;
                if (diff < 0) diff += 16;
                real_printf("%d ", diff); fflush(stdout);
            }

            while (e->rd_pos == rx->real_wr_pos) {
               // real_printf("."); fflush(stdout);
                TaskSleepReason("pcm_copy");
            }

		    if (rx->real_seqnum[e->rd_pos] != e->seq) {
                if (!e->seq_init) {
                    e->seq_init = true;
                } else {
                    u4_t got = rx->real_seqnum[e->rd_pos], expecting = e->seq;
                    //real_printf("SSTV rx%d SEQ: @%d got %d expecting %d (%d)\n", e->rx_chan, e->rd_pos, got, expecting, got - expecting);
                    printf("SSTV: seq diff %d ########\n", got - expecting);
                }
                e->seq = rx->real_seqnum[e->rd_pos];
            }
            e->seq++;
        } else {
            if (0 && e->rd_pos != rx->real_wr_pos) {
                int diff = (int) rx->real_wr_pos - (int) e->rd_pos;
                if (diff < 0) diff += 16;
                real_printf("%d ", diff); fflush(stdout);
            }
        }
    }
    
    #else
    
    memcpy(&e->pcm.Buffer[idx], e->s22p, nsamps * sizeof(s2_t));
    e->s22p += nsamps;
    
    #endif
}

void sstv_pcm_read(sstv_chan_t *e, s4_t numsamples)
{
    int i;

    if (e->pcm.WindowPtr == 0) {  
        // Fill buffer on first run
        pcm_copy(e, 0, PCM_BUFLEN);
        e->pcm.WindowPtr = PCM_BUFLEN/2;
    } else {
        // Shift buffer and push new samples
        for (i=0; i < PCM_BUFLEN-numsamples; i++) e->pcm.Buffer[i] = e->pcm.Buffer[i+numsamples];
        pcm_copy(e, PCM_BUFLEN-numsamples, numsamples);
        e->pcm.WindowPtr -= numsamples;
        if  (!(e->pcm.WindowPtr >= 0 && e->pcm.WindowPtr < PCM_BUFLEN)) {
            printf("sstv_pcm_read ASSERT %d: WindowPtr=%d numsamples=%d\n", e->rx_chan, e->pcm.WindowPtr, numsamples);
            panic("pcm");
        }
    }
}
