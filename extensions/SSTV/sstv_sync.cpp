/*
 * slowrx - an SSTV decoder
 * * * * * * * * * * * * * *
 * 
 * Copyright (c) 2007-2013, Oona Räisänen (OH2EIQ [at] sral.fi)
 */

#include "types.h"
#include "mem.h"
#include "sstv.h"

void sstv_sync_once(sstv_chan_t *e)
{
    e->lines = (lines_t *) kiwi_imalloc("sstv_sync_once", sizeof(lines_t));
    assert(e->lines != NULL);

    e->sync_img = (sync_img_t *) kiwi_imalloc("sstv_sync_once", sizeof(sync_img_t));
    assert(e->sync_img != NULL);
}

/* Find the slant angle of the sync signal and adjust sample rate to cancel it out
 *   Length:  number of PCM samples to process
 *   Mode:    one of M1, M2, S1, S2, R72, R36 ...
 *   Rate:    approximate sampling rate used
 *   Skip:    pointer to variable where the skip amount will be returned
 *   returns  adjusted sample rate
 *
 */
SSTV_REAL sstv_sync_find(sstv_chan_t *e, int *Skip)
{
    const char *result;
    u1_t Mode = e->pic.Mode;
    ModeSpec_t *m = &ModeSpec[Mode];
    SSTV_REAL Rate = e->pic.Rate;

    int         LineWidth = m->LineTime / m->SyncTime * 4;
    int         x,y;
    int         q, d, qMost, dMost;
    u2_t        cy, cx, Retries = 0;
    SSTV_REAL   t=0, slantAngle, s;
    SSTV_REAL   ConvoFilter[8] = { 1,1,1,1,-1,-1,-1,-1 };
    SSTV_REAL   convd, maxconvd;
    int         xmax=0;

    ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "align image");

    // Repeat until slant < 0.5° or until we give up
    while (true) {

        // Draw the 2D sync signal at current rate
        
        // sync fails to be found in test image without this,
        // but shouldn't be necessary looking at how sync_img is accessed by the code!
        memset(e->sync_img, false, sizeof(sync_img_t));

        for (y=0; y < m->NumLines; y++) {
            for (x=0; x < LineWidth; x++) {
                t = (y + 1.0*x/LineWidth) * m->LineTime;
                int SyncSampleNum = (int)( t * Rate / 13.0);
                //assert_array_dim(SyncSampleNum, e->HasSync_len);
                assert_array_dim(x, SYNC_IMG_XDIM);
                assert_array_dim(y, SYNC_IMG_YDIM);
                if (SyncSampleNum >= 0 && SyncSampleNum < e->HasSync_len) {
                    (*e->sync_img)[x][y] = e->HasSync[SyncSampleNum];
                } else {
                    lprintf("SSTV: sync_img mode=%d t=%.6f x=%d y=%d LineWidth=%d Rate=%.6f SyncSampleNum=%d HasSync_len=%d\n",
                        Mode, t, x, y, LineWidth, Rate, SyncSampleNum, e->HasSync_len);
                }
            }
            NextTask("sstv sync 1");
        }
    
        /** Linear Hough transform **/
    
        dMost = 0;
        qMost = MINSLANT*2;     // NB: bias up so qMost_min_slant is zero initially
        memset(e->lines, 0, sizeof(lines_t));

        // Find white pixels
        for (cy = 0; cy < m->NumLines; cy++) {
            for (cx = 0; cx < LineWidth; cx++) {
                assert_array_dim(cx, SYNC_IMG_XDIM);
                assert_array_dim(cy, SYNC_IMG_YDIM);
                if ((*e->sync_img)[cx][cy]) {
    
                    // Slant angles to consider
                    for (q = MINSLANT*2; q < MAXSLANT*2; q++) {
    
                        // Line accumulator
                        d = LineWidth + SSTV_MROUND(-cx * SSTV_MSIN(deg2rad(q/2.0)) + cy * SSTV_MCOS(deg2rad(q/2.0)));
                        if (d > 0 && d < LineWidth) {
                        
                            // zero biased
                            int q_min_slant = q-MINSLANT*2;
                            int qMost_min_slant = qMost-MINSLANT*2;
                            
                            assert_array_dim(d, LINES_XDIM);
                            assert_array_dim(dMost, LINES_XDIM);
                            assert_array_dim(q_min_slant, LINES_YDIM);
                            assert_array_dim(qMost_min_slant, LINES_YDIM);

                            (*e->lines)[d][q_min_slant] = (*e->lines)[d][q_min_slant] + 1;
                            if ((*e->lines)[d][q_min_slant] > (*e->lines)[dMost][qMost_min_slant]) {
                                dMost = d;
                                qMost = q;
                            }
                        }
                    }
                }
            }
            NextTask("sstv sync 2");
        }

        if (qMost == 0) {
            printf("SSTV: no sync signal; giving up\n");
            result = "no sync";
            break;
        }
    
        slantAngle = qMost / 2.0;
    
        printf("SSTV: try #%d, slant %.1f deg (d=%d) @ %.1f Hz\n", Retries+1, slantAngle, dMost, Rate);
    
        // Adjust sample rate
        Rate += SSTV_MTAN(deg2rad(90 - slantAngle)) / LineWidth * Rate;
    
        if (slantAngle > 89 && slantAngle < 91) {
            printf("SSTV: slant OK\n");
            result = "slant ok";
            break;
        } else
        if (Retries == 3) {
            printf("SSTV: still slanted; giving up\n");
            Rate = sstv.nom_rate;
            printf("SSTV: -> %d\n", sstv.nom_rate);
            result = "no fix";
            break;
        }

        printf("SSTV: -> %.1f recalculating\n", Rate);
        Retries++;
        TaskSleepMsec(1000);    // don't lock-out lower priority tasks (e.g. waterfalls)
    } // while (true)
  
    // accumulate a 1-dim array of the position of the sync pulse
    memset(e->xAcc, 0, sizeof(e->xAcc));
    for (y=0; y < m->NumLines; y++) {
        for (x=0; x < 700; x++) { 
            t = y * m->LineTime + x/700.0 * m->LineTime;
            int SyncSampleNum = (int) (t / (13.0/sstv.nom_rate) * Rate/sstv.nom_rate);
            //assert_array_dim(SyncSampleNum, e->HasSync_len);
            if (SyncSampleNum >= 0 && SyncSampleNum < e->HasSync_len) {
                e->xAcc[x] += e->HasSync[SyncSampleNum];
            } else {
                lprintf("SSTV: xAcc mode=%d t=%.6f x=%d y=%d Rate=%.6f SyncSampleNum=%d HasSync_len=%d\n",
                    Mode, t, x, y, Rate, SyncSampleNum, e->HasSync_len);
            }
        }
        NextTask("sstv sync 3");
    }
    
    // find falling edge of the sync pulse by 8-point convolution
    maxconvd = 0;
    for (x=0; x < 700-8; x++) {
        convd = 0;
        for (int i=0; i<8; i++) {
            assert_array_dim(x+i, XACC_DIM);
            convd += e->xAcc[x+i] * ConvoFilter[i];
        }
        if (convd > maxconvd) {
            maxconvd = convd;
            xmax = x+4;
        }
    }
    NextTask("sstv sync 4");

    // If pulse is near the right edge of the image, it just probably slipped off the left edge
    printf("SSTV: sync xmax %d/700 %.3f\n", xmax, xmax/700.0);
    if (xmax > 350) {
        xmax -= 350;
        printf("SSTV: sync xmax RIGHT EDGE %d/700 %.3f\n", xmax, xmax/700.0);
    }

    // Skip until the start of the line
    s = xmax / 700.0 * m->LineTime - m->SyncTime;
    printf("SSTV: sync s %.3f %d samp\n", s, (int) (s * Rate));

    // Scottie modes don't start lines with sync, i.e. pGpBSpR (p = porch, S = sync)
    if (Mode == S1 || Mode == S2 || Mode == SDX) {
        SSTV_REAL sync_offset = m->PorchTime * 2 - m->PixelTime * m->ImgWidth / 2.0;
        s = s + sync_offset;
        printf("SSTV: sync s Scottie %.3f %d samp (%.3f %.1f samp)\n",
            s, (int) (s * Rate), sync_offset, sync_offset * Rate);
    }

    *Skip = s * Rate;

    printf("SSTV: sync returns %.1f\n", Rate);
  
    ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "%s, %s, %.1f Hz (hdr %+d), skip %d smp (%.1f ms)",
        e->pic.modespec->ShortName, result, Rate, e->pic.HeaderShift, *Skip, *Skip * (1e3 / Rate));
  
    return Rate;
}
