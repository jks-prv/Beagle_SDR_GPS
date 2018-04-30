// Copyright (c) 2016 John Seamons, ZL/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

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

//#define IQ_DISPLAY_DEBUG_MSG	true
#define IQ_DISPLAY_DEBUG_MSG	false

// rx_chan is the receiver channel number we've been assigned, 0..RX_CHAN
// We need this so the extension can support multiple users, each with their own iq_display[] data structure.

#define	IQ_POINTS		0
#define	IQ_DENSITY		1
// #define	IQ_S4285_P		2
// #define	IQ_S4285_D		3
// #define	IQ_CLEAR		4

#define N_IQ_RING (16*1024)
#define N_CH		2
#define N_HISTORY	2

#include <array>
#include <complex>
#include <memory>

#include "iir.hpp"
#include "pll.hpp"

class iq_display {
public:
    typedef std::unique_ptr<iq_display> sptr;
    ~iq_display() {}

    typedef filter::loop_filter_2nd<float > loop_filter_type;
    typedef filter::integrator_modulo<float > integrator_type;
    typedef filter::pll<float, loop_filter_type, integrator_type> pll_type;

    static sptr make(int rx_chan) {
        return sptr(new iq_display(rx_chan));
    }
    std::complex<u1_t> to_fixed_scale(int ch, std::complex<float> sample) const {
        sample *= _gain * (ch ? 20 : 1) * 255 / CUTESDR_MAX_VAL;
        return std::complex<u1_t>(u1_t(std::max(0.0f, std::min(255.0f, 127+sample.real()))),
                                  u1_t(std::max(0.0f, std::min(255.0f, 127+sample.imag()))));
    }
    std::complex<u1_t> to_auto_scale(int ch, std::complex<float> sample) const {
        sample *= 255 / (4.0*std::abs(_cma));
        return std::complex<u1_t>(u1_t(std::max(0.0f, std::min(255.0f, 127+sample.real()))),
                                  u1_t(std::max(0.0f, std::min(255.0f, 127+sample.imag()))));
    }
    void display_data(int ch, int nsamps, TYPECPX *samps) {
        const int cmd = (_draw<<1) + (ch&1);
        if (_draw == IQ_POINTS) {
            for (int i=0; i<nsamps; ++i) {
                const std::complex<float> old_sample(_iq[ch][_ring[ch]]);
                std::complex<float> new_sample(samps[i].re, samps[i].im);
                _pll.process(new_sample);
                new_sample *= std::conj(_pll.exp_i_phase());
                _cma = (float(_cmaN)*_cma + new_sample)/float(_cmaN+1);
                _cmaN += (_cmaN < int(0.5*_fs));
                if (_gain) { // fixed scale
                    _plot[ch][_ring[ch]][0] = to_fixed_scale(ch, old_sample);
                    _plot[ch][_ring[ch]][1] = to_fixed_scale(ch, new_sample);
                } else { // auto scale
                    _plot[ch][_ring[ch]][0] = to_auto_scale(ch, old_sample);
                    _plot[ch][_ring[ch]][1] = to_auto_scale(ch, new_sample);
                }
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
                std::complex<float> new_sample(samps[i].re, samps[i].im);
                _pll.process(new_sample);
                new_sample *= std::conj(_pll.exp_i_phase());
                _cma = (float(_cmaN)*_cma + new_sample)/float(_cmaN+1);
                _cmaN += (_cmaN < int(0.5*_fs));
                if (_gain) {
                    _map[_ring[ch]] = to_fixed_scale(ch, new_sample);
                } else {
                    _map[_ring[ch]] = to_auto_scale(ch, new_sample);
                }
                _ring[ch] += 1;
                if (_ring[ch] >= _points) {
                    ext_send_msg_data(_rx_chan, IQ_DISPLAY_DEBUG_MSG, cmd, (u1_t*)(&(_map[0])), 1 + _points*2);
                    _ring[ch] = 0;
                }
            }
        }
        _nsamp += nsamps;
        if (_nsamp > _cmaNsend) {
            ext_send_msg(_rx_chan, IQ_DISPLAY_DEBUG_MSG, "EXT cmaI=%e cmaQ=%e df=%e", _cma.real(), _cma.imag(), -_pll.uf()/(2*M_PI));
            _nsamp -= _cmaNsend;
        }
}

    void set_sample_rate(float fs) {
        _fs       = fs;
        _cmaNsend = fs/4;
        _pll.update_ppb(0.0, fs);
    }

    bool process_msg(const char* msg) {
        int gain = 0;
        if (sscanf(msg, "SET gain=%d", &gain) == 1)
            set_gain(gain? pow(10.0, ((float) gain - 50) / 10.0) : 0); // 0 .. +100 dB of CUTESDR_MAX_VAL

        int points = 0;
        if (sscanf(msg, "SET points=%d", &points) == 1)
            set_points(points);

	
        int draw = 0;
        if (sscanf(msg, "SET draw=%d", &draw)== 1)
            set_draw(draw);

        if (strcmp(msg, "SET clear") == 0)
            clear();

        return true;
    }

protected:
    int rx_chan() const { return _rx_chan; }

    void set_gain(float g) { _gain = g; }
    void set_points(int p) { _points = p; }
    void set_draw(int d) { _draw = d; }

    void clear() {
        //printf("cmaN %d cmaI %e cmaQ %e\n", e->cmaN, e->cmaI, e->cmaQ);
        //		e->cma = e->ncma = e->cmaI = e->cmaQ = e->cmaN = e->nsamp = 0;
        _cma   = 0;
        _cmaN  = 0;
        _nsamp = 0;
        _pll.reset();
    }
    
private:
    iq_display(int rx_chan)
        : _rx_chan(rx_chan)
        , _points(1024)
        , _nsamp(0)
        , _draw(0)
        , _gain(0.0f)
        , _fs(12000.0f)
        , _cmaN(0)
        , _cma(0,0)
        , _cmaNsend(0)
        , _loop_filter(1.0f/sqrt(2.0f), 20.0f, 12001.0f)
        , _integrator(2*M_PI)
        , _pll(0.0f, 12001.0f, 20.0f, _loop_filter, _integrator) { _ring[0] = _ring[1] = 0; }
    iq_display(const iq_display&);
    iq_display& operator=(const iq_display&);

    int   _rx_chan;
    int   _points;
    int   _nsamp;
    int   _draw;
    float _gain;
    float _fs; // sample rate
    int   _ring[N_CH];
    std::complex<float>  _iq[N_CH][N_IQ_RING];
    std::complex<u1_t> _plot[N_CH][N_IQ_RING][N_HISTORY];
    std::complex<u1_t>  _map[N_IQ_RING];
    int                 _cmaN; // for moving average
    std::complex<float> _cma;  // moving average
    int                 _cmaNsend; // for cma
    loop_filter_type _loop_filter;
    integrator_type  _integrator;
    pll_type         _pll;
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
