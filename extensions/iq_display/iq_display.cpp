// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "ext.h"    // all calls to the extension interface begin with "ext_", e.g. ext_register()

#ifndef EXT_IQ_DISPLAY
    void iq_display_main() {}
#else

#include "clk.h"
#include "gps.h"
#include "st4285.h"
#include "fmdemod.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define IQ_DISPLAY_DEBUG_MSG  true
#define IQ_DISPLAY_DEBUG_MSG    false

// rx_chan is the receiver channel number we've been assigned, 0..RX_CHAN
// We need this so the extension can support multiple users, each with their own iq_display[] data structure.

#define IQ_POINTS       0
#define IQ_DENSITY      1
// #define  IQ_S4285_P      2
// #define  IQ_S4285_D      3
// #define  IQ_CLEAR        4

#define N_IQ_RING (16*1024)
#define N_CH        2
#define N_HISTORY   2

#include <array>
#include <complex>
#include <memory>

class iq_display {
public:
    typedef std::unique_ptr<iq_display> sptr;
    ~iq_display() {}

    static sptr make(int rx_chan) {
        return sptr(new iq_display(rx_chan));
    }
    std::complex<u1_t> to_u1(int ch, std::complex<float> sample) const {
        const float scale_factor = 255*(_gain ? _gain * (ch ? 20 : 1) / CUTESDR_MAX_VAL : 1.0f/(4.0*_ama));
        sample *= scale_factor;
        return std::complex<u1_t>(u1_t(std::max(0.0f, std::min(255.0f, 127+sample.real()))),
                                  u1_t(std::max(0.0f, std::min(255.0f, 127+sample.imag()))));
    }
    std::complex<float> process_sample(TYPECPX *s) {
        std::complex<float> sample(s->re, s->im);
        
        if (_exponent) {
            std::complex<float> exp_sample = (std::complex<float>) std::pow(sample, _exponent);
    
            // update pll
            _phase = std::fmod(_phase+_df/_fs, float(8*2*M_PI));
            const float ud_old = _ud;
            _ud  = std::arg(exp_sample * std::exp(std::complex<float>(0.0f, -_phase)));
            _df +=_b[0]*_ud + _b[1]*ud_old;
    
            std::complex<float> recovered_carrier = std::exp(std::complex<float>(0.0f, -_phase/_exponent));
            if (_mode == 0) {
                /*
                float sI = real(sample);
                float sQ = imag(sample);
                float Csin = imag(recovered_carrier);
                float Ccos = real(recovered_carrier);
                sample = std::complex<float>(Ccos * sI - Csin * sQ, Csin * sI + Ccos * sQ);
                */
                sample *= recovered_carrier;
            } else {
                sample = recovered_carrier;
            }
        }

        _cma    = (float(_maN)*_cma +          sample )/float(_maN+1);
        _ama    = (float(_maN)*_ama + std::abs(sample))/float(_maN+1);
        _maN  += (_maN < int(0.5*_fs));
        return sample;
    }
    
    void display_data(int ch, int nsamps, TYPECPX *samps) {
        const int cmd = (_draw<<1) + (ch&1);
        if (_draw == IQ_POINTS) {
            for (int i=0; i<nsamps; ++i) {
                const std::complex<float> old_sample(_iq[ch][_ring[ch]]);
                const std::complex<float> new_sample = process_sample(samps+i);
                _plot[ch][_ring[ch]][0] = to_u1(ch, old_sample);
                _plot[ch][_ring[ch]][1] = to_u1(ch, new_sample);
                _iq[ch][_ring[ch]] = new_sample;
                _ring[ch] += 1;
                if (_ring[ch] >= _points) {
                    ext_send_msg_data(_rx_chan, IQ_DISPLAY_DEBUG_MSG, cmd, (u1_t*)(&(_plot[ch][0][0])), 1 + _points*4);
                    _ring[ch] = 0;
                }
            }
        }
        if (_draw == IQ_DENSITY) {
            for (int i=0; i<nsamps; ++i) {
                const std::complex<float> new_sample = process_sample(samps+i);
                _map[_ring[ch]] = to_u1(ch, new_sample);
                _ring[ch] += 1;
                if (_ring[ch] >= _points) {
                    ext_send_msg_data(_rx_chan, IQ_DISPLAY_DEBUG_MSG, cmd, (u1_t*)(&(_map[0])), 1 + _points*2);
                    _ring[ch] = 0;
                }
            }
        }
        _nsamp += nsamps;
        if (_nsamp > _maNsend) {
            float df = _exponent? _df/(2*M_PI*_exponent) : 0;
            ext_send_msg(_rx_chan, IQ_DISPLAY_DEBUG_MSG, "EXT cmaI=%e cmaQ=%e df=%e", _cma.real(), _cma.imag(), df);
            _nsamp -= _maNsend;
        }
    }

    void set_sample_rate(float fs) {
        _fs       = fs;
        _maNsend = fs/4;
        pll_init();
    }

    bool process_msg(const char* msg) {
        int gain = 0;
        if (sscanf(msg, "SET gain=%d", &gain) == 1)
            set_gain(gain? std::pow(10.0, ((float) gain - 50) / 10.0) : 0); // 0 .. +100 dB of CUTESDR_MAX_VAL

        int points = 0;
        if (sscanf(msg, "SET points=%d", &points) == 1)
            set_points(points);

        int exponent = 0;
        if (sscanf(msg, "SET exponent=%d", &exponent) == 1) // allowed values are 1,2,4,8
            set_exponent(exponent);

        float pll_bandwidth = 0;
        if (sscanf(msg, "SET pll_bandwidth=%f", &pll_bandwidth) == 1)
            set_pll_bandwidth(pll_bandwidth);

        int draw = 0;
        if (sscanf(msg, "SET draw=%d", &draw)== 1)
            set_draw(draw);

        int mode = 0;
        if (sscanf(msg, "SET mode=%d", &mode)== 1)
            set_mode(mode);

        if (strcmp(msg, "SET clear") == 0)
            clear();

        return true;
    }

protected:
    int rx_chan() const { return _rx_chan; }

    void set_gain(float arg) { _gain = arg; }
    void set_points(int arg) { _points = arg; }
    void set_exponent(int arg) { _exponent = arg; }
    void set_pll_bandwidth(float arg) { _pll_bandwidth = arg; }
    void set_draw(int arg) { _draw = arg; }
    void set_mode(int arg) { _mode = arg; }

    void clear() {
        //printf("maN %d cmaI %e cmaQ %e\n", e->maN, e->cmaI, e->cmaQ);
        //      e->cma = e->ncma = e->cmaI = e->cmaQ = e->maN = e->nsamp = 0;
        _ama   = 0;
        _cma   = 0;
        _maN  = 0;
        _nsamp = 0;
        pll_init();
    }

private:
    void pll_init() {
        const float xi  = 1/sqrt(2.0f);         // PLL damping
        const float dwl = _exponent*_pll_bandwidth; // PLL bandwidth in Hz
        const float wn  = M_PI*dwl/xi;
        const float tau[2] = { 1/(wn*wn), 2*xi/wn };
        const float ts2 = 0.5/_fs;
        _b[0] = ts2/tau[0]*(1 + 1/std::tan(ts2/tau[1]));
        _b[1] = ts2/tau[0]*(1 - 1/std::tan(ts2/tau[1]));
        _phase = _ud = _df = 0;
    }
    iq_display(int rx_chan)
        : _rx_chan(rx_chan)
        , _points(1024)
        , _nsamp(0)
        , _exponent(1)
        , _draw(0)
        , _mode(0)
        , _gain(0.0f)
        , _fs(12000.0f)
        , _maN(0)
        , _cma(0,0)
        , _ama(0)
        , _maNsend(0)
        , _pll_bandwidth(10) {
        _ring[0] = _ring[1] = 0;
        pll_init();
    }
    iq_display(const iq_display&);
    iq_display& operator=(const iq_display&);

    int   _rx_chan;
    int   _points;
    int   _nsamp;
    int   _exponent;
    int   _draw;
    int   _mode;
    float _gain;
    float _fs; // sample rate
    int   _ring[N_CH];
    std::complex<float>  _iq[N_CH][N_IQ_RING];
    std::complex<u1_t> _plot[N_CH][N_IQ_RING][N_HISTORY];
    std::complex<u1_t>  _map[N_IQ_RING];
    int                 _maN; // for moving average
    std::complex<float> _cma;  // complex moving average
    float               _ama;  // moving average of abs(z)
    int                 _maNsend; // for cma
    // PLL
    float _pll_bandwidth;  // PLL bandwidth in Hz
    float _df;         // frequency offset in radians per sample
    float _phase;      // accumulated phase for frequency correction
    float _b[2];       // loop filter coefficients
    float _ud;         //
} ;

std::array<iq_display::sptr, RX_CHANS> iqs;

void iq_display_data(int rx_chan, int ch, int nsamps, TYPECPX *samps)
{
    if (iqs[rx_chan])
        iqs[rx_chan]->display_data(ch, nsamps, samps);
}

void iq_display_close(int rx_chan) {
    iqs[rx_chan].reset();
}

bool iq_display_msgs(char *msg, int rx_chan)
{
    if (strcmp(msg, "SET ext_server_init") == 0) {
        iqs[rx_chan] = iq_display::make(rx_chan);
        ext_send_msg(rx_chan, IQ_DISPLAY_DEBUG_MSG, "EXT ready");
        return true;
    }

    int do_run = 0;
    if (sscanf(msg, "SET run=%d", &do_run)) {
        if (do_run) {
            iqs[rx_chan]->set_sample_rate(ext_update_get_sample_rateHz(rx_chan));
            ext_register_receive_iq_samps(iq_display_data, rx_chan);
        } else {
            ext_unregister_receive_iq_samps(rx_chan);
        }
        return true;
    }

    // SECURITY
    // FIXME: need a per-user PLL instead of just changing the clock offset
    float offset = 0.0f;
    if (sscanf(msg, "SET offset=%f", &offset) == 1) {
        ext_adjust_clock_offset(rx_chan, offset);
        return true;
    }

    if (iqs[rx_chan])
        return iqs[rx_chan]->process_msg(msg);

    return false;
}


void iq_display_main();

ext_t iq_display_ext = {
    "iq_display",
    iq_display_main,
    iq_display_close,
    iq_display_msgs,
};

void iq_display_main()
{
    ext_register(&iq_display_ext);
}

#endif
