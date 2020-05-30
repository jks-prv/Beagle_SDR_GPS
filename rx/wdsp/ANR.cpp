//
// Automatic noise reduction
// Variable-leak LMS algorithm from Warren Pratt's wdsp package
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
#include "rx_noise.h"
#include "noise_filter.h"
#include <math.h>

#define ANR_DLINE_SIZE  512
#define ANR_MASK        (ANR_DLINE_SIZE - 1)

struct wdsp_ANR_t {
    int in_idx;
    f32_t d[ANR_DLINE_SIZE];
    f32_t w[ANR_DLINE_SIZE];

    int taps;           // 16..128, >= delay
    int delay;          // 2..128, <= taps
    int position;
    f32_t two_mu;       // gain, 1e-7..8.192e-2, step */= 2 (20 steps)
    f32_t gamma;        // leakage, 1e-3..8192.0, step */= 2 (23 steps)
    f32_t lidx;
    f32_t lidx_min;
    f32_t lidx_max;
    f32_t ngamma;
    f32_t den_mult;
    f32_t lincr;
    f32_t ldecr;
};

static wdsp_ANR_t wdsp_ANR[NOISE_TYPES][MAX_RX_CHANS];

void wdsp_ANR_init(int rx_chan, nr_type_e nr_type, TYPEREAL nr_param[NOISE_PARAMS])
{
    wdsp_ANR_t *w = &wdsp_ANR[nr_type][rx_chan];
    memset(w, 0, sizeof(wdsp_ANR_t));

    w->taps =     (int) nr_param[NR_TAPS];
    w->delay =    (int) nr_param[NR_DLY];
    w->position = 0;
    w->two_mu =   nr_param[NR_GAIN];
    w->gamma =    nr_param[NR_LEAKAGE];
    w->lidx =     120.0;
    w->lidx_min = 120.0;
    w->lidx_max = 200.0;
    w->ngamma =   0.001;
    w->den_mult = 6.25e-10;
    w->lincr =    1.0;
    w->ldecr =    3.0;
    //printf("wdsp_ANR_init type=%d taps=%d delay=%d gain=%.9f leakage=%.9f\n",
    //    nr_type, w->taps, w->delay, w->two_mu, w->gamma);
}

void wdsp_ANR_filter(int rx_chan, nr_type_e nr_type, int ns_out, TYPEMONO16 *in, TYPEMONO16 *out)
{
    wdsp_ANR_t *w = &wdsp_ANR[nr_type][rx_chan];
    int idx;
    f32_t c0, c1;
    f32_t y, error, sigma, inv_sigp;
    f32_t nel, nev;
    f32_t out_f;

    for (int i = 0; i < ns_out; i++) {
        w->d[w->in_idx] = ((TYPEREAL) in[i]) / K_AMPMAX;
        y = 0;
        sigma = 0;

        for (int j = 0; j < w->taps; j++) {
            idx = (w->in_idx + j + w->delay) & ANR_MASK;
            y += w->w[j] * w->d[idx];
            sigma += w->d[idx] * w->d[idx];
        }

        inv_sigp = 1.0 / (sigma + 1e-10);
        error = w->d[w->in_idx] - y;

        if (nr_type == NR_AUTONOTCH)
            out_f = error;      // notch filter
        else
            out_f = y * 4.0;    // noise reduction

        out[i] = (TYPEMONO16) MROUND(out_f * K_AMPMAX);

        if ((nel = error * (1.0 - w->two_mu * sigma * inv_sigp)) < 0.0) nel = -nel;

        if ((nev = w->d[w->in_idx] - (1.0 - w->two_mu * w->ngamma) * y - w->two_mu * error * sigma * inv_sigp) < 0.0)
            nev = -nev;

        if (nev < nel) {
            if ((w->lidx += w->lincr) > w->lidx_max) w->lidx = w->lidx_max;
            else
            if ((w->lidx -= w->ldecr) < w->lidx_min) w->lidx = w->lidx_min;
        }

        w->ngamma = w->gamma * (w->lidx * w->lidx) * (w->lidx * w->lidx) * w->den_mult;

        c0 = 1.0 - w->two_mu * w->ngamma;
        c1 = w->two_mu * error * inv_sigp;

        for (int j = 0; j < w->taps; j++) {
            idx = (w->in_idx + j + w->delay) & ANR_MASK;
            w->w[j] = c0 * w->w[j] + c1 * w->d[idx];
        }

        w->in_idx = (w->in_idx + ANR_MASK) & ANR_MASK;
    }
}
