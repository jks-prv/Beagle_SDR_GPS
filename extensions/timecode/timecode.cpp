// Copyright (c) 2017-2019 John Seamons, ZL4VO/KF6VO

#include "ext.h"	// all calls to the extension interface begin with "ext_", e.g. ext_register()

#include "types.h"
#include "kiwi.h"
#include "mode.h"
#include "printf.h"
#include "clk.h"
#include "cuteSDR.h"
#include "pll.h"
#include "misc.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

static s2_t *testStart, *testEnd;

class timecode {

public:
    typedef std::unique_ptr<timecode> sptr;
    ~timecode() {}

    static sptr make(int rx_chan) {
        return sptr(new timecode(rx_chan));
    }

    std::complex<float> process_sample(TYPECPX *s) {
        std::complex<float> sample(s->re, s->im);
        std::complex<float> recovered_carrier = 1;
        if (_exponent) {    // -> pll_mode != 0
            const std::complex<float> exp_sample = (std::complex<float>)(std::pow(sample, _exponent));
            const float phase = _pll.update(exp_sample);
            recovered_carrier = std::exp(std::complex<float>(0.0f, -phase/_exponent));

            if (_display_mode == 0) {
                sample *= recovered_carrier;
            } else {
                sample = recovered_carrier;
            }
        }

        _ama    = (float(_maN)*_ama + std::abs(sample))/float(_maN+1);
        _maN   += (_maN < int(0.5*_fs));
        return sample;
    }
    
    u1_t to_u1(std::complex<float> sample) const {
        const float scale_factor = 255*(_gain ? _gain / CUTESDR_MAX_VAL : 1.0f/(2.0*_ama));
        sample *= scale_factor;
        return u1_t(std::max(0.0f, std::min(255.0f, 127+sample.real())));
    }

    void display_data(int instance, int nsamps, TYPECPX *samps) {
        for (int i=0; i<nsamps; ++i) {
            const std::complex<float> new_sample = process_sample(samps+i);
            _buf[i] = to_u1(new_sample);
            if (0 && i == 22)
                printf("%f,%f %f,%f %d\n",
                    (samps+i)->re, (samps+i)->im, new_sample.real(), new_sample.imag(), _buf[i]);
        }
        ext_send_msg_data(_rx_chan, false, _draw, (u1_t*)(&(_buf[0])), nsamps);

        _nsamp += nsamps;
        if (_nsamp > _maNsend) {
            float df, phase;
            df    = _exponent ? _pll.df()/(2*M_PI*_exponent) : 0;
            phase = _pll.phase();
            ext_send_msg(_rx_chan, false, "EXT df=%e phase=%f", df, phase);
            _nsamp -= _maNsend;
        }
    }

    void set_sample_rate(float fs) {
        _fs       = fs;
        _maNsend = fs/4;
        pll_init();
    }

    void pll_init() {
        _pll.init(_pll_bandwidth, _pll_offset, _fs);
    }

    bool process_msg(const char* msg) {

        int gain = 0;
        if (sscanf(msg, "SET gain=%d", &gain) == 1) {
            set_gain(gain? std::pow(10.0, ((float) gain - 50) / 10.0) : 0); // 0 .. +100 dB of CUTESDR_MAX_VAL
            return true;
        }

        int points = 0;
        if (sscanf(msg, "SET points=%d", &points) == 1) {
            set_points(points);
            return true;
        }

        int pll_mode = 0, arg=0;
        // pll_mode = 0 -> no PLL
        // pll_mode = 1 -> single carrier tracking arg= exponent (allowed values are 1,2,4,8)
        if (sscanf(msg, "SET pll_mode=%d arg=%d", &pll_mode, &arg) == 2) {
            set_pll_mode(pll_mode, arg);
            return true;
        }

        float pll_bandwidth = 0;
        if (sscanf(msg, "SET pll_bandwidth=%f", &pll_bandwidth) == 1) {
            set_pll_bandwidth(pll_bandwidth);
            return true;
        }

        float pll_offset = 0;
        if (sscanf(msg, "SET pll_offset=%f", &pll_offset) == 1) {
            set_pll_offset(pll_offset);
            return true;
        }

        int draw = 0;
        if (sscanf(msg, "SET draw=%d", &draw) == 1) {
            set_draw(draw);
            return true;
        }

        int display_mode = 0;
        if (sscanf(msg, "SET display_mode=%d", &display_mode) == 1) {
            set_display_mode(display_mode);
            return true;
        }

        if (strcmp(msg, "SET clear") == 0) {
            clear();
            return true;
        }

        if (strcmp(msg, "SET test") == 0) {
            static bool test_init;
            if (!test_init) {
                char *file;
                int fd;
                #define TIMECODE_FNAME DIR_CFG "/samples/timecode.test.au"
                printf("timecode: mmap " TIMECODE_FNAME "\n");
                fd = open(TIMECODE_FNAME, O_RDONLY);
                if (fd >= 0) {
                    off_t fsize = kiwi_file_size(TIMECODE_FNAME);
                    printf("timecode: size=%ld\n", fsize);
                    file = (char *) mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
                    if (file == MAP_FAILED) sys_panic("timecode mmap");
                    close(fd);
                    int words = fsize / sizeof(s2_t);
                    testStart = (s2_t *) file;
                    testEnd = testStart + words;
                    test_init = true;
                }
            }

            if (test_init) {
                testP = testStart;
                test = true;
            }
            return true;
        }

        return false;
    }

    bool test;
    s2_t *testP;

protected:
    int rx_chan() const { return _rx_chan; }

    void set_gain(float arg) { _gain = arg; }
    void set_points(int arg) { _points = arg; }
    void set_pll_mode(int pll_mode, int arg) {
        if (pll_mode <= 1) {
            _exponent = arg;
        }
        pll_init();
    }
    void set_pll_bandwidth(float arg) {
        _pll_bandwidth = arg;
        pll_init();
    }
    void set_pll_offset(float arg) {
        _pll_offset = arg;
        pll_init();
    }
    void set_draw(int arg) { _draw = arg; }
    void set_display_mode(int arg) { _display_mode = arg; }

    void clear() {
        _ama   = 0;
        _maN  = 0;
        _nsamp = 0;
        _pll.init(_pll_bandwidth, _pll_offset, _fs);
    }

private:
    timecode(int rx_chan)
        : _rx_chan(rx_chan)
        , _points(1024)
        , _nsamp(0)
        , _exponent(1)
        , _draw(0)
        , _display_mode(0)
        , _gain(0.0f)
        , _fs(12000.0f)
        , _maN(0)
        , _ama(0)
        , _maNsend(0)
        , _pll_bandwidth(10)
        , _pll_offset(0)
        , _pll(_fs, _pll_bandwidth, _pll_offset)
    { }
    timecode(const timecode&);
    timecode& operator=(const timecode&);

    int   _rx_chan;
    int   _points;
    int   _nsamp;
    int   _exponent;
    int   _draw;
    int   _display_mode;
    float _gain;
    float _fs; // sample rate
    u1_t  _buf[FASTFIR_OUTBUF_SIZE];
    int                 _maN; // for moving average
    float               _ama;  // moving average of abs(z)
    int                 _maNsend; // for cma
    float _pll_bandwidth;
    float _pll_offset;
    pll   _pll;        // PLL
};

std::array<timecode::sptr, MAX_RX_CHANS> tc;

void timecode_data(int rx_chan, int instance, int nsamps, TYPECPX *samps)
{
    if (!tc[rx_chan]) return;
    
    if (tc[rx_chan]->test) {
    
        // pushback audio samples from the test file
        TYPECPX *s = samps;
        for (int i = 0; i < nsamps; i++) {
            if (tc[rx_chan]->testP >= testEnd) {
                tc[rx_chan]->testP = testStart;
            }

            u2_t t;            
            t = (u2_t) *tc[rx_chan]->testP;
            tc[rx_chan]->testP++;
            s->re = (TYPEREAL) (s4_t) (s2_t) FLIP16(t);
            t = (u2_t) *tc[rx_chan]->testP;
            tc[rx_chan]->testP++;
            s->im = (TYPEREAL) (s4_t) (s2_t) FLIP16(t);
            s++;
        }
    }
    
    tc[rx_chan]->display_data(instance, nsamps, samps);
        
}

void timecode_close(int rx_chan) {
    tc[rx_chan].reset();
}

bool timecode_msgs(char *msg, int rx_chan)
{
	//rcprintf(rx_chan, "TIMECODE <%s>\n", msg);

    if (strcmp(msg, "SET ext_server_init") == 0) {
        tc[rx_chan] = timecode::make(rx_chan);
        ext_send_msg(rx_chan, false, "EXT ready");
        return true;
    }

    int do_run = 0;
    if (sscanf(msg, "SET run=%d", &do_run)) {
        if (do_run) {
            tc[rx_chan]->test = false;
            tc[rx_chan]->set_sample_rate(ext_update_get_sample_rateHz(rx_chan));
            ext_register_receive_iq_samps(timecode_data, rx_chan);
        } else {
            ext_unregister_receive_iq_samps(rx_chan);
        }
        return true;
    }

    if (tc[rx_chan])
        return tc[rx_chan]->process_msg(msg);

    return false;
}

void timecode_main();

ext_t timecode_ext = {
	"timecode",
	timecode_main,
	timecode_close,
	timecode_msgs,
	EXT_NEW_VERSION,
	EXT_FLAGS_HEAVY
};

void timecode_main()
{
	ext_register(&timecode_ext);
}
