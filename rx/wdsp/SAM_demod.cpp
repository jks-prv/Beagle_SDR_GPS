//
// synchronous AM PLL & PHASE detector from Warren Pratt´s wdsp package
// github.com/TAPR/OpenHPSDR-PowerSDR/tree/master/Project%20Files/Source/wdsp [GNU GPL v2.0]
// github.com/g0orx/wdsp
// github.com/NR0V/wdsp
// (c) Warren Pratt wdsp library 2016
//
// via the Teensy-ConvolutionSDR project: github.com/DD4WH/Teensy-ConvolutionSDR [GNU GPL v3.0]
// (c) Frank DD4WH 2020_04_19
//

#include "types.h"
#include "kiwi.h"
#include "kiwi_assert.h"
#include "wdsp.h"
#include "fir.h"

#include <math.h>

#define SAM_PLL_HILBERT_STAGES 7

struct wdsp_SAM_t {
    // pll
    f32_t phzerror;
    f32_t fil_out;
    f32_t omega2;

    // fade leveler
    f32_t dc;
    f32_t dc_insert;
    f32_t dcu;
    f32_t dc_insertu;

    #define SAM_array_dim(d,l) assert_array_dim(d,l)
    #define ABCD_DIM (3 * SAM_PLL_HILBERT_STAGES + 3)
    f32_t a[ABCD_DIM];     // Filter a variables
    f32_t b[ABCD_DIM];     // Filter b variables
    f32_t c[ABCD_DIM];     // Filter c variables
    f32_t d[ABCD_DIM];     // Filter d variables
    f32_t dsI;             // delayed sample, I path
    f32_t dsQ;             // delayed sample, Q path

    f32_t SAM_carrier;
    f32_t SAM_lowpass;
};

static wdsp_SAM_t wdsp_SAM[MAX_RX_CHANS];

// pll
static const f32_t pll_fmax = +22000.0;
static f32_t omega_min;
static f32_t omega_max;
static f32_t zeta;          // PLL step response: smaller, slower response 1.0 - 0.1
static f32_t omegaN;        // PLL bandwidth 50.0 - 1000.0
static f32_t g1;
static f32_t g2;

// fade leveler
#define FADE_LEVELER 1
static const f32_t tauR = 0.02;
static const f32_t tauI = 1.4;
static f32_t mtauR;
static f32_t onem_mtauR;
static f32_t mtauI;
static f32_t onem_mtauI;

#define OUT_IDX   (3 * SAM_PLL_HILBERT_STAGES)

// Coefficients for SAM sideband selection Hilbert filters
static const f32_t c0[SAM_PLL_HILBERT_STAGES] = {
    -0.328201924180698,
    -0.744171491539427,
    -0.923022915444215,
    -0.978490468768238,
    -0.994128272402075,
    -0.998458978159551,
    -0.999790306259206,
};

static const f32_t c1[SAM_PLL_HILBERT_STAGES] = {
    -0.0991227952747244,
    -0.565619728761389,
    -0.857467122550052,
    -0.959123933111275,
    -0.988739372718090,
    -0.996959189310611,
    -0.999282492800792
};


// DX adjustments: zeta = 0.15, omegaN = 100.0
// very stable, but does not lock very fast
// standard settings: zeta = 1.0, omegaN = 250.0
// maybe user can choose between slow (DX), medium, fast SAM PLL
// zeta / omegaN
// DX = 0.2, 70
// medium 0.6, 200
// fast 1.2, 500

const int PLL_DX = 0, PLL_MED = 1, PLL_FAST = 2;

void wdsp_SAM_PLL(int type)
{
    switch (type) {
    
    case PLL_DX:
        zeta = 0.2;
        omegaN = 70;
        break;
    
    case PLL_MED:
        zeta = 0.65;
        omegaN = 200.0;
        break;
    
    case PLL_FAST:
        zeta = 1.0;
        omegaN = 500;
        break;
    }

    //printf("SAM_PLL zeta=%.2f omegaN=%.0f\n", zeta, omegaN);
    g1 = 1.0 - expf(-2.0 * omegaN * zeta / snd_rate);
    g2 = - g1 + 2.0 * (1 - expf(- omegaN * zeta / snd_rate) * cosf(omegaN / snd_rate * sqrtf(1.0 - zeta * zeta)));
}

void wdsp_SAM_demod_init()
{
    wdsp_SAM_PLL(PLL_MED);

    omega_min = K_2PI * (-pll_fmax) / snd_rate;
    omega_max = K_2PI * pll_fmax / snd_rate;

    mtauR = expf(-1 / (snd_rate * tauR));
    onem_mtauR = 1.0 - mtauR;
    mtauI = expf(-1 / (snd_rate * tauI));
    onem_mtauI = 1.0 - mtauI;
}

void wdsp_SAM_reset(int rx_chan)
{
    wdsp_SAM_t *w = &wdsp_SAM[rx_chan];
    memset(w, 0, sizeof(wdsp_SAM_t));
}

f32_t wdsp_SAM_carrier(int rx_chan)
{
    f32_t carrier = wdsp_SAM[rx_chan].SAM_carrier;
    if (isnan(carrier)) carrier = 0;
    return carrier;
}

void wdsp_SAM_demod(int rx_chan, int mode, int chan_null, int ns_out, TYPECPX *in, TYPEMONO16 *out)
{
    TYPECPX *out_stereo = in;   // stereo output is pushed back onto IQ input buffer
    wdsp_SAM_t *w = &wdsp_SAM[rx_chan];
    f32_t ai_ps, bi_ps, bq_ps, aq_ps;
    bool isChanNull = (mode == MODE_SAM && chan_null != CHAN_NULL_NONE);
    bool need_ps = ((mode != MODE_SAM && mode != MODE_QAM) || isChanNull);
    bool stereoMode_or_nulling = (mode == MODE_SAS || mode == MODE_QAM || isChanNull);
    
    for (u4_t i = 0; i < ns_out; i++) {
          
        f32_t err_sin = sinf(w->phzerror);
        f32_t err_cos = cosf(w->phzerror);
        f32_t ai = err_cos * in[i].re;
        f32_t bi = err_sin * in[i].re;
        f32_t aq = err_cos * in[i].im;
        f32_t bq = err_sin * in[i].im;

        if (need_ps) {
            w->a[0] = w->dsI;
            w->b[0] = bi;
            w->c[0] = w->dsQ;
            w->d[0] = aq;
            w->dsI = ai;
            w->dsQ = bq;

            for (int j = 0; j < SAM_PLL_HILBERT_STAGES; j++) {
              int k = 3 * j;
              SAM_array_dim(k+5, ABCD_DIM);
              w->a[k + 3] = c0[j] * (w->a[k] - w->a[k + 5]) + w->a[k + 2];
              w->b[k + 3] = c1[j] * (w->b[k] - w->b[k + 5]) + w->b[k + 2];
              w->c[k + 3] = c0[j] * (w->c[k] - w->c[k + 5]) + w->c[k + 2];
              w->d[k + 3] = c1[j] * (w->d[k] - w->d[k + 5]) + w->d[k + 2];
            }
            
            ai_ps = w->a[OUT_IDX];
            bi_ps = w->b[OUT_IDX];
            bq_ps = w->c[OUT_IDX];
            aq_ps = w->d[OUT_IDX];

            for (int j = OUT_IDX + 2; j > 0; j--) {
                SAM_array_dim(j, ABCD_DIM);
                SAM_array_dim(j-1, ABCD_DIM);
                w->a[j] = w->a[j - 1];
                w->b[j] = w->b[j - 1];
                w->c[j] = w->c[j - 1];
                w->d[j] = w->d[j - 1];
            }
        }

        f32_t corr[NIQ];
        corr[I] = +ai + bq;
        corr[Q] = -bi + aq;

        f32_t audio, audiou, audion;

        switch (mode) {
            case MODE_SAM:
                if (isChanNull) {
                    audio  = (ai_ps + bi_ps) - (aq_ps - bq_ps); // lsb
                    audiou = (ai_ps - bi_ps) + (aq_ps + bq_ps); // usb

                    if (chan_null == CHAN_NULL_LSB) {
                        audion = audio - audiou;
                    } else {    // CHAN_NULL_USB
                        audion = audiou - audio;
                    }
                } else {
                    audio = corr[I];
                    if (p5_f) audio *= p5_f;
                }
                break;

            case MODE_SAU:
                audio = (ai_ps - bi_ps) + (aq_ps + bq_ps);  // usb
                if (p6_f) audio *= p6_f;
                break;

            case MODE_SAL:
                audio = (ai_ps + bi_ps) - (aq_ps - bq_ps);  // lsb
                if (p6_f) audio *= p6_f;
                break;

            case MODE_SAS:
                audio  = (ai_ps + bi_ps) - (aq_ps - bq_ps); // lsb
                if (p6_f) audio *= p6_f;
                audiou = (ai_ps - bi_ps) + (aq_ps + bq_ps); // usb
                if (p6_f) audiou *= p6_f;
                break;

            case MODE_QAM:  // C-QUAM
                audio  = corr[I]/2 + corr[Q]/2; // L
                audio *= 2;
                if (p7_f) audio *= p7_f;
                audiou = corr[I]/2 - corr[Q]/2; // R
                audiou *= 2;
                if (p7_f) audiou *= p7_f;
                break;
            
            default:
                panic("SAM");
                break;
        }
        
        if (FADE_LEVELER) {
            w->dc = mtauR * w->dc + onem_mtauR * audio;
            w->dc_insert = mtauI * w->dc_insert + onem_mtauI * corr[I];
            audio = audio + w->dc_insert - w->dc;
        }
        
        if (stereoMode_or_nulling) {
            if (FADE_LEVELER) {
                w->dcu = mtauR * w->dcu + onem_mtauR * audiou;
                w->dc_insertu = mtauI * w->dc_insertu + onem_mtauI * corr[I];
                audiou = audiou + w->dc_insertu - w->dcu;
            }
            
            if (isChanNull) {   // MODE_SAM
                out[i] = audion;    // lsb or usb nulled

                // also save lsb/usb for display purposes
                if (chan_null == CHAN_NULL_LSB) {
                    out_stereo[i].re = audion;  // lsb nulled
                    //out_stereo[i].im = audiou;  // usb
                    out_stereo[i].im = 0;  // usb
                } else {
                    //out_stereo[i].re = audio;   // lsb
                    out_stereo[i].re = 0;   // lsb
                    out_stereo[i].im = audion;  // usb nulled
                }
            } else {
                out_stereo[i].re = audio;
                out_stereo[i].im = audiou;
            }
        } else {
            out[i] = audio;
        }
        
        f32_t det = atan2f(corr[Q], corr[I]);
        f32_t del_out = w->fil_out;
        w->omega2 = w->omega2 + g2 * det;
        w->omega2 = CLAMP(w->omega2, omega_min, omega_max);
        w->fil_out = g1 * det + w->omega2;
        w->phzerror = w->phzerror + del_out;

        // wrap round 2PI, modulus
        while (w->phzerror >= K_2PI) w->phzerror -= K_2PI;
        while (w->phzerror < 0.0) w->phzerror += K_2PI;
    }

    // a simple lowpass/exponential averager here . . .
    w->SAM_carrier = 0.08 * (w->omega2 * snd_rate) / K_2PI;
    w->SAM_carrier = w->SAM_carrier + 0.92 * w->SAM_lowpass;
    w->SAM_lowpass = w->SAM_carrier;
    
    #if 0
        static u4_t last;
        u4_t now = timer_sec();
        if (last != now) {
            printf("car %.1f Hz, err %.3f rad, omega2 %f\n", w->SAM_carrier, w->phzerror, w->omega2);
            last = now;
        }
    #endif
}
