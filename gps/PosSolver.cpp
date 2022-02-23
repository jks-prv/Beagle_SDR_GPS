// -*- C++ -*-

#include <array>

#include "PosSolver.h"
#include "SinglePointPositionSolver.h"
#include "EKFPositionSolver.h"

class PosSolverImpl : public PosSolver {
public:

#ifdef KIWI_DEBIAN_7
    virtual bool pos_valid()  { return _pos_valid; }
    virtual bool spp_valid()  { return _state_spp[0]; }
    virtual bool ekf_valid()  { return _ekf_running >= 1; }
    virtual vec_type const& pos()  { return _pos; }
    virtual double pos(int i)  { return _pos[i]; }
    virtual double t_rx()  { return _t_rx; }
    virtual double osc_corr()  { return _osc_corr; }
    virtual LonLatAlt const& llh()  { return _llh; }
#else
    virtual bool pos_valid() const final { return _pos_valid; }
    virtual bool spp_valid() const final { return _state_spp[0]; }
    virtual bool ekf_valid() const final { return _ekf_running >= 1; }
    virtual vec_type const& pos() const final { return _pos; }
    virtual double pos(int i) const final { return _pos[i]; }
    virtual double t_rx() const final { return _t_rx; }
    virtual double osc_corr() const final { return _osc_corr; }
    virtual LonLatAlt const& llh() const final { return _llh; }
#endif

    virtual void set_use_kalman(bool b) {
        _use_kalman = b;
        if (!_use_kalman) {
            _ekf_running = -1;
            _state_spp[0] = _state_spp[1] = false;
        }
    }

#ifdef KIWI_DEBIAN_7
    virtual std::vector<ElevAzim> elev_azim(mat_type sv) {
#else
    virtual std::vector<ElevAzim> elev_azim(mat_type sv) final {
#endif
        if (!spp_valid() && !ekf_valid())
            return std::vector<ElevAzim>();

        std::vector<ElevAzim> elevAzim(sv.dim2());
        const auto g = [&elevAzim](int i_sv, double elev_rad, double azim_rad) {
            elevAzim[i_sv].elev_deg = elev_rad/M_PI*180;
            elevAzim[i_sv].azim_deg = azim_rad/M_PI*180;
        };
        if (ekf_valid()) {
            _ekf.IterElevAzim(sv, g);
        } else {
            _spp.IterElevAzim(sv, g);
        }
        return elevAzim;
    }

#ifdef KIWI_DEBIAN_7
    virtual bool solve(mat_type sv, vec_type weight, uint64_t adc_ticks) {
#else
    virtual bool solve(mat_type sv, vec_type weight, uint64_t adc_ticks) final {
#endif
        assert(sv.dim2() == weight.dim());

        int nsv = sv.dim2();
        if (!nsv)
            return false;

        assert(sv.dim1() == 4);

        // normalize weights and apply UERE scaling
        weight /= (TNT::mean(weight) * _uere*_uere);

        // save adc_ticks history
        _ticks_spp[1] = _ticks_spp[0];
        _ticks_ekf[0] = _ticks_spp[0] = adc_ticks;

        // SPP solution
        bool const status = _spp.Solve(sv, TNT::makeDiag(weight));

        // update SPP status
        _state_spp[1] = _state_spp[0];
        double const altitude = _spp.LLH().alt();
        _state_spp[0] = (status && altitude > -100 && altitude < 9000);

        // save ct_rx history
        _ct_rx[1] = _ct_rx[0];
        _ct_rx[0] = _spp.ct_rx();

        // update state
        if (_state_spp[0]) {
          _llh  = _spp.LLH();
          _t_rx = _spp.ct_rx()/_spp.c();
          _pos.inject(_spp.pos());
          _pos_valid = true;
        }

        // when the last two SPP solutions are valid, compute osc_corr and start EFK tracking
        if (_state_spp[0] && _state_spp[1]) {
          double const dt_adc_sec = dadc_ticks_sec(_ticks_spp);
          _osc_corr = _spp.mod_gpsweek_rel(_ct_rx[0] - _ct_rx[1]) / _spp.c() / dt_adc_sec;
#ifdef DEBUG_POS_SOLVER
          printf("POS_SPP: %13.3f %13.3f %13.3f %.9f %.9f %2d\n",
                 _pos(0), _pos(1), _pos(2), _t_rx, _osc_corr, nsv);
#endif
          if (_use_kalman && _ekf_running == -1) {
            mat_type ekf_cov(5,5, 0.0);
            ekf_cov.subarray(0,3,0,3).inject(_spp.cov());
            ekf_cov(4,4) = 1.0;

            vec_type ekf_state(5, 0.0);
            ekf_state.subarray(0,3).inject(_spp.state());
            ekf_state(4) = _osc_corr * _ekf.c();

            _ekf.Reset(ekf_state, ekf_cov);
            _ticks_ekf[1] = _ticks_ekf[0];
            _ekf_running = 0;
          }
        }

        if (_use_kalman && _ekf_running >= 0) {
          double const dt_adc_sec = dadc_ticks_sec(_ticks_ekf);
          if (_ekf.update(sv, weight, dt_adc_sec)) {
            _ticks_ekf[1]  = _ticks_ekf[0];
            _ekf_running  += (_ekf_running < 4);
            _llh           = _ekf.LLH();
            _t_rx          = _ekf.ct_rx()/_ekf.c();
            _osc_corr      = _ekf.state(4)/_ekf.c();
            _pos_valid     = true;
            _pos.inject(_ekf.pos());
#ifdef DEBUG_POS_SOLVER
            printf("POS_EKF: %13.3f %13.3f %13.3f %.9f %.9f %f\n",
                   _pos(0), _pos(1), _pos(2), _t_rx, _osc_corr, dt_adc_sec);

#endif
            } else {
                _ekf_running = -1;
            }
        }
        return true;
    }

    PosSolverImpl(double uere,
                  double fOsc,
                  kiwi_yield::wptr yield)
        : PosSolver()
        , _uere(uere)
        , _fOsc(fOsc)
        , _use_kalman(true)
        , _spp(20, yield)
        , _ekf(yield)
        , _pos(3, 0.0)
        , _t_rx(0)
        , _osc_corr(-1)
        , _llh()
        , _pos_valid(false)
        , _ekf_running(-1)
        , _state_spp{{false, false}}
        , _ticks_spp{{0,0}}
        , _ticks_ekf{{0,0}}
        , _ct_rx{{0,0}} {}

    virtual ~PosSolverImpl() {}

protected:
    double dadc_ticks_sec(std::array<uint64_t,2> const& adc_ticks) const {
        // 48-bit overflow protection (assuming that clock ticks increase monotonously)
        return (adc_ticks[0] + (adc_ticks[0] < adc_ticks[1])*(1ULL<<48) - adc_ticks[1])/_fOsc;
    }
private:
    mat_type  _Q;
    double    _uere;
    double    _fOsc;
    bool      _use_kalman;
    SinglePointPositionSolver _spp;
    EKFPositionSolver _ekf;
    vec_type  _pos;
    double    _t_rx;
    double    _osc_corr;
    LonLatAlt _llh;
    bool      _pos_valid;
    int       _ekf_running;
    std::array<bool,2>     _state_spp;
    std::array<uint64_t,2> _ticks_spp;
    std::array<uint64_t,2> _ticks_ekf;
    std::array<double,2>   _ct_rx;
} ;

PosSolver::sptr PosSolver::make(double uere, double fOsc, kiwi_yield::wptr yield)
{
    return PosSolver::sptr(new PosSolverImpl(uere, fOsc, yield));
}
