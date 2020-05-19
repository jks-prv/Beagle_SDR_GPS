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

    f32_t a[3 * SAM_PLL_HILBERT_STAGES + 3];     // Filter a variables
    f32_t b[3 * SAM_PLL_HILBERT_STAGES + 3];     // Filter b variables
    f32_t c[3 * SAM_PLL_HILBERT_STAGES + 3];     // Filter c variables
    f32_t d[3 * SAM_PLL_HILBERT_STAGES + 3];     // Filter d variables
    f32_t dsI;             // delayed sample, I path
    f32_t dsQ;             // delayed sample, Q path

    f32_t SAM_carrier;
    f32_t SAM_lowpass;
};

static wdsp_SAM_t wdsp_SAM[MAX_RX_CHANS];

static const f32_t pll_fmax = +22000.0;
static const int zeta_help = 65;
static const f32_t zeta = (f32_t) zeta_help / 100.0; // PLL step response: smaller, slower response 1.0 - 0.1
static const f32_t omegaN = 200.0; // PLL bandwidth 50.0 - 1000.0

// pll
static f32_t omega_min;
static f32_t omega_max;
static f32_t g1;
static f32_t g2;

// fade leveler
#define FADE_LEVELER 1
static const f32_t tauR = 0.02; // original 0.02;
static const f32_t tauI = 1.4; // original 1.4;
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

void wdsp_SAM_demod_init()
{
    // DX adjustments: zeta = 0.15, omegaN = 100.0
    // very stable, but does not lock very fast
    // standard settings: zeta = 1.0, omegaN = 250.0
    // maybe user can choose between slow (DX), medium, fast SAM PLL
    // zeta / omegaN
    // DX = 0.2, 70
    // medium 0.6, 200
    // fast 1.2, 500
    //f32_t zeta = 0.8; // PLL step response: smaller, slower response 1.0 - 0.1
    //f32_t omegaN = 250.0; // PLL bandwidth 50.0 - 1000.0

    omega_min = K_2PI * (-pll_fmax) / snd_rate;
    omega_max = K_2PI * pll_fmax / snd_rate;
    g1 = 1.0 - expf(-2.0 * omegaN * zeta / snd_rate);
    g2 = - g1 + 2.0 * (1 - expf(- omegaN * zeta / snd_rate) * cosf(omegaN / snd_rate * sqrtf(1.0 - zeta * zeta)));

    mtauR = expf(-1 / (snd_rate * tauR));
    onem_mtauR = 1.0 - mtauR;
    mtauI = expf(-1 / (snd_rate * tauI));
    onem_mtauI = 1.0 - mtauI;
}

void wdsp_SAM_reset(int rx_chan)
{
    memset(&wdsp_SAM[rx_chan], 0, sizeof(wdsp_SAM_t));
}

f32_t wdsp_SAM_carrier(int rx_chan)
{
    return wdsp_SAM[rx_chan].SAM_carrier;
}

void wdsp_SAM_demod(int rx_chan, int mode, int ns_out, TYPECPX *in, TYPEMONO16 *out)
{
    TYPECPX *out_stereo = in;
    wdsp_SAM_t *w = &wdsp_SAM[rx_chan];
    f32_t ai_ps, bi_ps, bq_ps, aq_ps;
    
    for (u4_t i = 0; i < ns_out; i++) {
          
        f32_t err_sin = sinf(w->phzerror);
        f32_t err_cos = cosf(w->phzerror);
        f32_t ai = err_cos * in[i].re;
        f32_t bi = err_sin * in[i].re;
        f32_t aq = err_cos * in[i].im;
        f32_t bq = err_sin * in[i].im;

        if (mode != MODE_SAM) {
            w->a[0] = w->dsI;
            w->b[0] = bi;
            w->c[0] = w->dsQ;
            w->d[0] = aq;
            w->dsI = ai;
            w->dsQ = bq;

            for (int j = 0; j < SAM_PLL_HILBERT_STAGES; j++) {
              int k = 3 * j;
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
                w->a[j] = w->a[j - 1];
                w->b[j] = w->b[j - 1];
                w->c[j] = w->c[j - 1];
                w->d[j] = w->d[j - 1];
            }
        }

        f32_t corr[2];
        corr[0] = +ai + bq;
        corr[1] = -bi + aq;

        f32_t audio, audiou;
        switch (mode) {
            case MODE_SAM:
                audio = corr[0];
                break;

            case MODE_SAU:
                audio = (ai_ps - bi_ps) + (aq_ps + bq_ps);
                break;

            case MODE_SAL:
                audio = (ai_ps + bi_ps) - (aq_ps - bq_ps);
                break;

            case MODE_SAS:
                audio = (ai_ps + bi_ps) - (aq_ps - bq_ps);
                audiou = (ai_ps - bi_ps) + (aq_ps + bq_ps);
                break;
            
            default:
                panic("SAM");
                break;
        }
        
        if (FADE_LEVELER) {
            w->dc = mtauR * w->dc + onem_mtauR * audio;
            w->dc_insert = mtauI * w->dc_insert + onem_mtauI * corr[0];
            audio = audio + w->dc_insert - w->dc;
        }
        
        if (mode == MODE_SAS) {
            if (FADE_LEVELER) {
                w->dcu = mtauR * w->dcu + onem_mtauR * audiou;
                w->dc_insertu = mtauI * w->dc_insertu + onem_mtauI * corr[0];
                audiou = audiou + w->dc_insertu - w->dcu;
            }
            
            out_stereo[i].re = audio;
            out_stereo[i].im = audiou;
        } else {
            out[i] = audio;
        }
        
        f32_t det = atan2f(corr[1], corr[0]);
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
            printf("car %.1f Hz, err %.3f rad\n", w->SAM_carrier, w->phzerror);
            last = now;
        }
    #endif
}
