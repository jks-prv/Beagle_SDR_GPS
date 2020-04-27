
// Christoph's PLL from the IQ display extension

#include <array>
#include <complex>
#include <memory>

class pll {
public:
	pll(float fs=12e3, float dwl=10, float fc=0)
		: _fs(fs)
		, _fc(0)
		, _df(0)
		, _phase(0)
		, _b()
		, _ud(0) {
		init(dwl, fc, _fs);
	}

	float phase() const { return _phase; }
	float df() const { return _df; }
	
	void init(float dwl,                // PLL bandwidth in Hz
			float fc,                   // frequency offset in Hz
            float fs,                   // sampling frequency
			float xi = 1.0f/sqrt(2.0f)) // PLL damping (default is critical damping)
    {
        const float wn  = M_PI*dwl/xi;
        const float tau[2] = { 1/(wn*wn), 2*xi/wn };
		_fs = fs;
        const float ts2 = 0.5/_fs;
        _b[0] = ts2/tau[0]*(1 + 1/std::tan(ts2/tau[1]));
        _b[1] = ts2/tau[0]*(1 - 1/std::tan(ts2/tau[1]));
        _phase = _ud = _df = 0;
        _fc = fc;
    }

    float update(std::complex<float> sample) {    
        _phase = std::fmod(_phase+float(2*M_PI*_fc+_df)/_fs, float(8*2*M_PI));
        const float ud_old = _ud;
        _ud  = std::arg(sample * std::exp(std::complex<float>(0.0f, -_phase)));
        _df += _b[0]*_ud + _b[1]*ud_old;
        return _phase;
    }
protected:
    
private:
    float _fs;         // sampling frequency (Hz)
    float _fc;         // frequency offset (Hz)
    float _df;         // frequency error in radians per sample
    float _phase;      // accumulated phase for frequency correction
    float _b[2];       // loop filter coefficients
    float _ud;         //
} ;
