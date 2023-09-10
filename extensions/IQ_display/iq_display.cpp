// Copyright (c) 2016 John Seamons, ZL4VO/KF6VO

#include "ext.h"    // all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "types.h"
#include "clk.h"
#include "pll.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>

//#define IQ_DISPLAY_DEBUG_MSG  true
#define IQ_DISPLAY_DEBUG_MSG    false

// rx_chan is the receiver channel number we've been assigned, 0..rx_chans
// We need this so the extension can support multiple users, each with their own iq_display[] data structure.

#define IQ_POINTS       0
#define IQ_DENSITY      1
//#define  IQ_CLEAR        2

#define N_IQ_RING (16*1024)
#define N_CH        2
#define N_HISTORY   2

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
        std::complex<float> recovered_carrier = 1;
        if (_msk_mode) {
            const std::complex<float> s2 = sample*sample;
            const float phaseMinus = _pllMinus.update(s2);
            const float phasePlus  = _pllPlus.update(s2);
            recovered_carrier = std::exp(std::complex<float>(0.0f, -0.25*(phasePlus+phaseMinus)));
        } else if (_exponent) {
            const std::complex<float> exp_sample = (std::complex<float>)(std::pow(sample, _exponent));
            const float phase = _pll.update(exp_sample);
            recovered_carrier = std::exp(std::complex<float>(0.0f, -phase/_exponent));
        }

        if (_exponent) {        // -> pll_mode != 0
            if (_display_mode == 0) {
                sample *= recovered_carrier;
            } else {
                sample = recovered_carrier;
            }
        }

        _cma    = (float(_maN)*_cma +          sample )/float(_maN+1);
        _ama    = (float(_maN)*_ama + std::abs(sample))/float(_maN+1);
        _maN   += (_maN < int(0.5*_fs));
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
                    ext_send_msg_data(_rx_chan, IQ_DISPLAY_DEBUG_MSG, cmd, (u1_t*)(&(_plot[ch][0][0])), _points*N_CH*NIQ);
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
                    ext_send_msg_data(_rx_chan, IQ_DISPLAY_DEBUG_MSG, cmd, (u1_t*)(&(_map[0])), _points*NIQ);
                    _ring[ch] = 0;
                }
            }
        }
        _nsamp += nsamps;
        if (_nsamp > _maNsend) {
            float df, phase;
            if (_msk_mode) {
                df    = (_pllPlus.df()+_pllMinus.df())/(8*M_PI);
                phase = std::fmod(0.25f*(_pllPlus.phase()+_pllMinus.phase()), float(2*M_PI));
            } else {
                df    = _exponent ? _pll.df()/(2*M_PI*_exponent) : 0;
                phase = _pll.phase();
            }
            ext_send_msg(_rx_chan, IQ_DISPLAY_DEBUG_MSG, "EXT cmaI=%e cmaQ=%e df=%e phase=%f adc_clock=%.0f",
                         _cma.real(), _cma.imag(), df, phase, adc_clock_system());
            _nsamp -= _maNsend;
        }
    }

    void set_sample_rate(float fs) {
        _fs      = fs;
        _maNsend = fs/4;
        pll_init();
    }

    void pll_init() {
        _pll.init(_pll_bandwidth, 0.0, _fs);
        _pllMinus.init(_pll_bandwidth, -0.5*_msk_baud, _fs);
        _pllPlus.init (_pll_bandwidth,  0.5*_msk_baud, _fs);
    }

    bool process_msg(const char* msg) {
        int gain = 0;
        if (sscanf(msg, "SET gain=%d", &gain) == 1)
            set_gain(gain? std::pow(10.0, ((float) gain - 50) / 10.0) : 0); // 0 .. +120 dB of CUTESDR_MAX_VAL

        int points = 0;
        if (sscanf(msg, "SET points=%d", &points) == 1)
            set_points(points);

        int pll_mode = 0, arg=0;
        // pll_mode = 0 -> no PLL
        // pll_mode = 1 -> single carrier tracking arg= exponent (allowed values are 1,2,4,8)
        // pll_mode = 2 -> MSK carrier tracking using two PLLs, arg = MSK bps
        if (sscanf(msg, "SET pll_mode=%d arg=%d", &pll_mode, &arg) == 2)
            set_pll_mode(pll_mode, arg);

        float pll_bandwidth = 0;
        if (sscanf(msg, "SET pll_bandwidth=%f", &pll_bandwidth) == 1)
            set_pll_bandwidth(pll_bandwidth);

        int draw = 0;
        if (sscanf(msg, "SET draw=%d", &draw)== 1)
            set_draw(draw);

        int display_mode = 0;
        if (sscanf(msg, "SET display_mode=%d", &display_mode)== 1)
            set_display_mode(display_mode);

        if (strcmp(msg, "SET clear") == 0)
            clear();

        return true;
    }

protected:
    int rx_chan() const { return _rx_chan; }

    void set_gain(float arg) { _gain = arg; }
    void set_points(int arg) { _points = arg; }
    void set_pll_mode(int pll_mode, int arg) {
        if (pll_mode <= 1) {
            _exponent = arg;
            _msk_mode = false;
        }
        if (pll_mode == 2) {
            _exponent = 2;
            _msk_mode = true;
            _msk_baud = arg;
        }
        pll_init();
    }
    void set_pll_bandwidth(float arg) {
        _pll_bandwidth = arg;
        pll_init();
    }
    void set_draw(int arg) { _draw = arg; }
    void set_display_mode(int arg) { _display_mode = arg; }

    void clear() {
        _ama   = 0;
        _cma   = 0;
        _maN   = 0;
        _nsamp = 0;
        _pll.init(_pll_bandwidth, 0.0f, _fs);
        _pllMinus.init(_pll_bandwidth, -0.5*_msk_baud, _fs);
        _pllPlus.init (_pll_bandwidth,  0.5*_msk_baud, _fs);
    }

private:
    iq_display(int rx_chan)
        : _rx_chan(rx_chan)
        , _points(1024)
        , _nsamp(0)
        , _exponent(1)
        , _msk_mode(false)
        , _msk_baud(200)
        , _draw(0)
        , _display_mode(0)
        , _gain(0.0f)
        , _fs(12000.0f)
        , _maN(0)
        , _cma(0,0)
        , _ama(0)
        , _maNsend(0)
        , _pll_bandwidth(10)
        , _pll(_fs, _pll_bandwidth, 0.0)
        , _pllMinus(_fs, _pll_bandwidth, -0.5*_msk_baud)
        , _pllPlus (_fs, _pll_bandwidth,  0.5*_msk_baud)
    {
        _ring[0] = _ring[1] = 0;
    }
    iq_display(const iq_display&);
    iq_display& operator=(const iq_display&);

    int   _rx_chan;
    int   _points;
    int   _nsamp;
    int   _exponent;
    bool  _msk_mode;
    int   _msk_baud;
    int   _draw;
    int   _display_mode;
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
    float _pll_bandwidth;
    pll   _pll;        // PLL
    pll   _pllMinus;   // PLL used for MSK carrier recovery
    pll   _pllPlus;    // PLL used for MSK carrier recovery
} ;

std::array<iq_display::sptr, MAX_RX_CHANS> iqs;

void iq_display_data(int rx_chan, int instance, int nsamps, TYPECPX *samps)
{
    if (iqs[rx_chan])
        iqs[rx_chan]->display_data(instance, nsamps, samps);
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

    if (iqs[rx_chan])
        return iqs[rx_chan]->process_msg(msg);

    return false;
}


void IQ_display_main();

ext_t iq_display_ext = {
    "IQ_display",
    IQ_display_main,
    iq_display_close,
    iq_display_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void IQ_display_main()
{
    ext_register(&iq_display_ext);
}
