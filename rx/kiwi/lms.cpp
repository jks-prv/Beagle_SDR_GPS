#include "rx_noise.h"
#include "lms.h"
#include "noise_filter.h"

#define DL_LEN_MAX              300     // 25 msec @ 12 kHz
#define DL_AUTONOTCH_LEN_DEF    48      //  4 msec @ 12 kHz
#define DL_NOISE_LEN_DEF        1

#define BETA_AUTONOTCH_DEF      0.125
#define BETA_NOISE_DEF          0.0058

#define DECAY_AUTONOTCH_DEF     0.99915
#define DECAY_NOISE_DEF         0.98

CLMS m_LMS[MAX_RX_CHANS][2];

CLMS::CLMS()
{
}

int CLMS::Initialize(nr_type_e nr_type, TYPEREAL nr_param[NOISE_PARAMS])
{
    TYPEREAL delayLineLen = nr_param[NR_DELAY], beta = nr_param[NR_BETA], decay = nr_param[NR_DECAY];
    m_nr_type = nr_type;
    
    if (m_nr_type == NR_AUTONOTCH) {
        if (delayLineLen <= 0) delayLineLen = DL_AUTONOTCH_LEN_DEF;
        if (beta <= 0) beta = BETA_AUTONOTCH_DEF;
        m_beta = beta;
        if (decay <= 0) decay = DECAY_AUTONOTCH_DEF;
        m_decay = decay;
    } else
    if (m_nr_type == NR_DENOISE) {
        if (delayLineLen <= 0) delayLineLen = DL_NOISE_LEN_DEF;
        if (beta <= 0) beta = BETA_NOISE_DEF;
        m_beta = beta;
        if (decay <= 0) decay = DECAY_NOISE_DEF;
        m_decay = decay;
    } else
        return -1;
    
    if (delayLineLen > DL_LEN_MAX) delayLineLen = DL_LEN_MAX;
    m_dlen = delayLineLen;
    m_dlp = 0;

    memset(m_dline, 0, sizeof(m_dline));
    memset(m_lmscoef, 0, sizeof(m_lmscoef));

    //printf("LMS %s dlen=%d FIR=%d beta=%.6f decay=%.6f\n", (m_nr_type == NR_AUTONOTCH)? "autonotch" : "denoise",
    //    m_dlen, LMSLEN, m_beta, m_decay);
    return 0;
}

/*

    For the traditional shifting delay-line (d) and FIR filter (f):

                  newest ---------> TIME -------> oldest
    new sample -> d0>d1>d2>d3 -> f0>f1>f2>f3>f4>f5>f6>f7
                                  |  |  |  |  |  |  |  |
                                  +--+--+--+--+--+--+--+--> FIR

    We instead use a moving pointer that is modulo a combined delay-line + FIR size buffer.
    So no buffer data has to me moved to implement a shift.

                          new sample ---+
                                        v
                  f3_f2_f1_f0_d3_d2_d1_d0_f7_f6_f5_f4
                  older <---- TIME <--- ^ oldest <---
                                        | pointer increments
                                          and wraps ->

    I.e. the interpretation of the data in the buffer changes as the pointer is incremented:
    
                  f3_f2_f1_f0_d3_d2_d1_d0_f7_f6_f5_f4
                                        ^
                  f4_f3_f2_f1_f0_d3_d2_d1_d0_f7_f6_f5
                                           ^
                  f5_f4_f3_f2_f1_f0_d3_d2_d1_d0_f7_f6   etc.
                                              ^
*/

void CLMS::ProcessFilter(int ilen, TYPEMONO16* ibuf, TYPEMONO16* obuf)
{
    //printf("LMS run %s dlen=%d beta=%.6f decay=%.6f\n", (m_nr_type == NR_AUTONOTCH)? "autonotch" : "denoise", \
        m_dlen, m_beta, m_decay);

    int i;
    
    for (int bp = 0; bp < ilen; bp++) {
	    TYPEREAL samp = ((TYPEREAL) ibuf[bp]) / K_AMPMAX;
	    m_dline[m_dlp] = samp; INC(m_dlp);

        // Wiener filter convolution
        TYPEREAL fir = 0;
        for (i=0; i < LMSLEN; i++) {
            fir += m_dline[m_dlp] * m_lmscoef[i];
            INC(m_dlp);
        }
        DEC(m_dlp);     // backup to last
	
        if (m_nr_type == NR_DENOISE) {
            obuf[bp] = (TYPEMONO16) MROUND(fir * 2 * K_AMPMAX);
        }
    
        TYPEREAL err = samp - fir;
    
        if (m_nr_type == NR_AUTONOTCH) {
            obuf[bp] = (TYPEMONO16) MROUND(err * K_AMPMAX);
        }

        // Wiener filter adaptation
        TYPEREAL err2 = err * m_beta;

        TYPEREAL new_co;
        for (i = LMSLEN_M1; i >= 0; i--) {
            new_co = m_dline[m_dlp] * err2 + m_lmscoef[i] * m_decay;
            m_lmscoef[i] = new_co;
            DEC(m_dlp);
        }
        INC(m_dlp);
    }
}
