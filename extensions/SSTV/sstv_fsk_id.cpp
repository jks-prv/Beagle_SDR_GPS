/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 * Copyright (c) 2007-2013, Oona Räisänen (OH2EIQ [at] sral.fi)
 */

#include "sstv.h"

/* 
 *
 * Decode FSK ID
 *
 * * The FSK IDs are 6-bit bytes, LSB first, 45.45 baud (22 ms/bit), 1900 Hz = 1, 2100 Hz = 0
 * * Text data starts with 20 2A and ends in 01
 * * Add 0x20 and the data becomes ASCII
 *
 */

void sstv_get_fsk(sstv_chan_t *e, char *fsk_id)
{

    const int   FFTLen = 2048;
    int         i=0, LoBin, HiBin, MidBin, TestNum=0;
    int         TestPtr = 24;   // NB: not zero because of strange reverse bit addressing code below
    u1_t        Bit = 0, AsciiByte = 0, BytePtr = 0, TestBits[24] = {0}, BitPtr=0;
    SSTV_REAL   HiPow,LoPow,Hann[SSTV_MS_2_MAX_SAMPS(22)];
    bool        InSync = false;

    // Bit-reversion lookup table
    static const u1_t BitRev[] = {
        0x00, 0x20, 0x10, 0x30,   0x08, 0x28, 0x18, 0x38,
        0x04, 0x24, 0x14, 0x34,   0x0c, 0x2c, 0x1c, 0x3c,
        0x02, 0x22, 0x12, 0x32,   0x0a, 0x2a, 0x1a, 0x3a,
        0x06, 0x26, 0x16, 0x36,   0x0e, 0x2e, 0x1e, 0x3e,
        0x01, 0x21, 0x11, 0x31,   0x09, 0x29, 0x19, 0x39,
        0x05, 0x25, 0x15, 0x35,   0x0d, 0x2d, 0x1d, 0x3d,
        0x03, 0x23, 0x13, 0x33,   0x0b, 0x2b, 0x1b, 0x3b,
        0x07, 0x27, 0x17, 0x37,   0x0f, 0x2f, 0x1f, 0x3f };

    memset(e->fft.in2k, 0, sizeof(SSTV_REAL) * FFTLen);

    // Create 22ms Hann window
    int samps_22ms = SSTV_MS_2_SAMPS(22);
    int half_samps_22ms = samps_22ms/2;
    for (i = 0; i < samps_22ms; i++)
        Hann[i] = 0.5 * (1 - SSTV_MCOS( 2 * M_PI * i / (SSTV_REAL) (samps_22ms-1) ));

    while (true) {
        int samps_half_full = InSync? samps_22ms : (samps_22ms/2);
    
        // Read data from DSP
        sstv_pcm_read(e, samps_half_full);
    
        if (e->pcm.WindowPtr < half_samps_22ms) {
            e->pcm.WindowPtr += samps_half_full;
            continue;
        }
    
        // Apply Hann window
        for (i = 0; i < samps_22ms; i++)
            e->fft.in2k[i] = e->pcm.Buffer[e->pcm.WindowPtr+i- half_samps_22ms] * Hann[i];
        
        e->pcm.WindowPtr += samps_half_full;
    
        // FFT of last 22 ms
        SSTV_FFTW_EXECUTE(e->fft.Plan2048);
        NextTask("sstv FFT FSK");
    
        LoBin  = GET_BIN(1900+e->pic.HeaderShift, FFTLen)-1;
        MidBin = GET_BIN(2000+e->pic.HeaderShift, FFTLen);
        HiBin  = GET_BIN(2100+e->pic.HeaderShift, FFTLen)+1;
    
        LoPow = 0;
        HiPow = 0;
    
        for (i = LoBin; i <= HiBin; i++) {
            if (i < MidBin) LoPow += POWER(e->fft.out2k[i]);
            else            HiPow += POWER(e->fft.out2k[i]);
        }
    
        Bit = (LoPow > HiPow);
    
        if (!InSync) {
    
            // Wait for 20 2A
            TestBits[TestPtr % 24] = Bit;
    
            TestNum = 0;
            for (i=0; i < 12; i++) {
                // NB: TestPtr cannot be initialized to zero due to this code!
                int tp = (TestPtr - (23-i*2)) % 24;
                //assert(tp > 0);
                //assert_lt(tp, 24);
                TestNum |= TestBits[tp] << (11-i);
            }
    
            if (BitRev[(TestNum >>  6) & 0x3f] == 0x20 && BitRev[TestNum & 0x3f] == 0x2a) {
                InSync    = true;
                AsciiByte = 0;
                BitPtr    = 0;
                BytePtr   = 0;
            }
    
            if (++TestPtr > 200) break;
    
        } else {
    
            AsciiByte |= Bit << BitPtr;
    
            if (++BitPtr == 6) {
                if (AsciiByte < 0x0d || BytePtr > 9) break;
                fsk_id[BytePtr] = AsciiByte + 0x20;
                BitPtr        = 0;
                AsciiByte     = 0;
                BytePtr++;
            }
        }
    }
    
    fsk_id[BytePtr] = '\0';
    ext_send_msg_encoded(e->rx_chan, false, "EXT", "fsk_id", "%s", fsk_id);
}
