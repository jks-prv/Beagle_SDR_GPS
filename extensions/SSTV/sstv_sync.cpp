#include "sstv.h"

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
    u2_t        xAcc[700] = {0};
    u2_t        cy, cx, Retries = 0;
    SSTV_REAL   t=0, slantAngle, s;
    SSTV_REAL   ConvoFilter[8] = { 1,1,1,1,-1,-1,-1,-1 };
    SSTV_REAL   convd, maxconvd=0;
    int         xmax=0;

    //u2_t  lines[600][(MAXSLANT-MINSLANT)*2];
    #define LINES_XDIM 600
    #define LINES_YDIM ((MAXSLANT-MINSLANT)*2)
    #define lines_o(x,y) ((x)*LINES_YDIM + (y))
    #define lines_size() lines_o(LINES_XDIM, LINES_YDIM)
    #define lines(x,y) lines_p[lines_o(x,y)]
    u2_t*   lines_p = (u2_t *) calloc(lines_size(), sizeof(u2_t));

    //bool  SyncImg[700][630] = {{FALSE}};
    #define SYNCIMG_XDIM 700
    #define SYNCIMG_YDIM 630
    #define SyncImg_o(x,y) ((x)*SYNCIMG_YDIM + (y))
    #define SyncImg_size() SyncImg_o(SYNCIMG_XDIM, SYNCIMG_YDIM)
    #define SyncImg(x,y) SyncImg_p[SyncImg_o(x,y)]
    bool*   SyncImg_p = (bool *) calloc(SyncImg_size(), sizeof(u2_t));

  // Repeat until slant < 0.5° or until we give up
  while (TRUE) {

    // Draw the 2D sync signal at current rate
    
    for (y=0; y<m->NumLines; y++) {
      for (x=0; x<LineWidth; x++) {
        t = (y + 1.0*x/LineWidth) * m->LineTime;
        SyncImg(x,y) = e->HasSync[ (int)( t * Rate / 13.0) ];
      }
        NextTask("sstv sync 1");
    }

    /** Linear Hough transform **/

    dMost = qMost = 0;
    memset(lines_p, 0, lines_size() * sizeof(u2_t));

    // Find white pixels
    for (cy = 0; cy < m->NumLines; cy++) {
      for (cx = 0; cx < LineWidth; cx++) {
        if (SyncImg(cx,cy)) {

          // Slant angles to consider
          for (q = MINSLANT*2; q < MAXSLANT*2; q ++) {

            // Line accumulator
            d = LineWidth + SSTV_MROUND( -cx * SSTV_MSIN(deg2rad(q/2.0)) + cy * SSTV_MCOS(deg2rad(q/2.0)) );
            if (d > 0 && d < LineWidth) {
              lines(d, q-MINSLANT*2) = lines(d, q-MINSLANT*2) + 1;
              if (lines(d, q-MINSLANT*2) > lines(dMost, qMost-MINSLANT*2)) {
                dMost = d;
                qMost = q;
              }
            }
          }
        }
      }
    NextTask("sstv sync 2");
    }

    if ( qMost == 0) {
      printf("SSTV: no sync signal; giving up\n");
      result = "no sync";
      break;
    }

    slantAngle = qMost / 2.0;

    printf("SSTV: %.1f° (d=%d) @ %.1f Hz\n", slantAngle, dMost, Rate);

    // Adjust sample rate
    Rate += SSTV_MTAN(deg2rad(90 - slantAngle)) / LineWidth * Rate;

    if (slantAngle > 89 && slantAngle < 91) {
      printf("SSTV: slant OK\n");
      result = "slant ok";
      break;
    } else if (Retries == 3) {
      printf("SSTV: still slanted; giving up\n");
      Rate = sstv.nom_rate;
      printf("SSTV: -> %d\n", sstv.nom_rate);
      result = "no slant fix";
      break;
    }
    printf("SSTV: -> %.1f recalculating\n", Rate);
    Retries ++;
  }
  
  // accumulate a 1-dim array of the position of the sync pulse
  memset(xAcc, 0, sizeof(xAcc[0]) * 700);
  for (y=0; y<m->NumLines; y++) {
    for (x=0; x<700; x++) { 
      t = y * m->LineTime + x/700.0 * m->LineTime;
      xAcc[x] += e->HasSync[ (int)(t / (13.0/sstv.nom_rate) * Rate/sstv.nom_rate) ];
    }
    NextTask("sstv sync 3");
  }

  // find falling edge of the sync pulse by 8-point convolution
  for (x=0; x<700-8; x++) {
    convd = 0;
    for (int i=0; i<8; i++) convd += xAcc[x+i] * ConvoFilter[i];
    if (convd > maxconvd) {
      maxconvd = convd;
      xmax = x+4;
    }
  }
    NextTask("sstv sync 4");

  // If pulse is near the right edge of the image, it just probably slipped
  // out the left edge
  if (xmax > 350) xmax -= 350;

  // Skip until the start of the line
  s = xmax / 700.0 * m->LineTime - m->SyncTime;
  
  // (Scottie modes don't start lines with sync)
  if (Mode == S1 || Mode == S2 || Mode == SDX)
    s = s - m->PixelTime * m->ImgWidth / 2.0
          + m->PorchTime * 2;

  *Skip = s * Rate;

  printf("SSTV: sync will return %.1f\n", Rate);
  
    ext_send_msg_encoded(e->rx_chan, false, "EXT", "status", "%s, Fs %.1f, header %+d Hz, skip %d pixels",
        result, Rate, e->pic.HedrShift, *Skip);
  
  free(lines_p);
  free(SyncImg_p);
  
  return (Rate);

}
