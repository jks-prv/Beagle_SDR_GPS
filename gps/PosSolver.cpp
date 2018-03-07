// -*- C++ -*-

#include "PosSolver.h"
#include "SinglePointPositionSolver.h"
#include "EKFPositionSolver.h"

class PosSolverImpl : public PosSolver {
public:
    PosSolverImpl(const mat_type& Q,
                  double uere,
                  double fOsc)
        : PosSolver()
        , _Q(Q)
        , _uere(uere)
        , _fOsc(fOsc)
        , _spp(20)
        , _ekf()
        , _state(state::NO_POS)
        , _pos(3, 0.0)
        , _t_rx(0)
        , _oscCorr(-1)
        , _adc_ticks(0)    // 48 bit tick counter
        , _llh() {}

    bool solve(mat_type sv, vec_type weight, uint64_t adc_ticks) final {
        assert(sv.dim1() == 4 && sv.dim2() == weight.dim());

        const int nsv = sv.dim2();
        if (!nsv)
            return false;

        // normalize weights and apply UERE scaling
        weight /= (TNT::mean(weight) * _uere*_uere);

        bool success  = false;
        if ((nsv > 0) &&
            ((_state & state::HAS_POS_AND_TIME) == state::HAS_POS_AND_TIME &&
             (_state & state::DO_EKF_TRACKING)  == state::DO_EKF_TRACKING)) {
            const double dt = dadc_ticks(adc_ticks)/_fOsc;
            if (dt > 10*60.0) { // if last update is too old fall back to single point position solutions
                _state &= ~(state::DO_EKF_TRACKING | state::HAS_TIME);
            }
            success = _ekf.update(sv, weight, dt, nsv);
            if (success) {
                _state    |=  state::HAS_POS_AND_TIME;
                _pos.inject(_ekf.pos());
                _t_rx      = _ekf.ct_rx()/_ekf.c();
                _adc_ticks =  adc_ticks;
                _oscCorr   = _ekf.state(4)/_ekf.c();
                _llh       = _ekf.LLH();
            } else {
                // NOP
            }
        }
        if (nsv >= 4) {
            success = _spp.Solve(sv, TNT::makeDiag(weight));
            if (success) {
                // basic quality check
                printf("quality check:  alt=%f\n", _spp.LLH().alt());
                const char* spaces= "                                             ";
                printf("%sSPP___: %13.3f %13.3f %13.3f %.9f %.9f %10.6f %10.6f %4.0f\n", spaces,
                       _spp.state()[0],
                       _spp.state()[1],
                       _spp.state()[2],
                       _spp.ct_rx()/_spp.c(),
                       0.0,
                       _spp.LLH().lat(),
                       _spp.LLH().lon(),
                       _spp.LLH().alt());
                if (_spp.LLH().alt() >    0 &&
                    _spp.LLH().alt() < 9000) {
                    if ((_state & state::DO_EKF_TRACKING) == 0) {
                        if (_state & state::HAS_TIME) {
                            // if there is already a past time measurement compute oscillator correction
                            const double dt_adc = dadc_ticks(adc_ticks)/_fOsc;
                            if (dt_adc < 10*60.) {
                                const double dt     = _spp.mod_gpsweek(_spp.ct_rx() - _spp.c()*_t_rx)/_spp.c(); // ## mod GPS week
                                _oscCorr = dt/dt_adc;
                                printf("oscCorr = %f,%f,%f\n", dt_adc, dt, _oscCorr);
                            }
                        }
                        _pos.inject(_spp.pos());
                        _t_rx      = _spp.ct_rx()/_spp.c();
                        _llh       = _spp.LLH();
                        _adc_ticks =  adc_ticks;
                        if (_state & state::HAS_TIME) { // start EKF tracking
                            reset_EKF();
                            _state |= state::DO_EKF_TRACKING;
                        }
                        _state |=  state::HAS_POS_AND_TIME;
                    }
                }
            }
        }

        for (const auto& x : elev_azim(sv))
            printf("elev,azim= %12.6f %12.6f\n", x.elev_deg, x.azim_deg);

        return success;
    }
    int               state() const final { return _state; }
    const vec_type      pos() const final { return _pos; }
    double             t_rx() const final { return _t_rx; }
    double         osc_corr() const final { return _oscCorr; }
    const LonLatAlt&    llh() const final { return _llh; }

    std::vector<ElevAzim> elev_azim(mat_type sv) final {
        if (!(_state & state::HAS_POS))
            return std::vector<ElevAzim>();

        std::vector<ElevAzim> elevAzim(sv.dim2());
        const auto g = [&elevAzim](int i_sv, double elev_rad, double azim_rad) {
            elevAzim[i_sv].elev_deg = elev_rad/M_PI*180;
            elevAzim[i_sv].azim_deg = azim_rad/M_PI*180;
        };
        if (state() & state::DO_EKF_TRACKING)
            _ekf.IterElevAzim(sv, g);
        else
            _spp.IterElevAzim(sv, g);
        return elevAzim;
    }

protected:
    uint64_t dadc_ticks(uint64_t adc_ticks) const {
        // 48-bit overflow protection
        return adc_ticks + (adc_ticks < _adc_ticks)*(1ULL<<48) - _adc_ticks;
    }
    void reset_EKF() {
        printf("reset_EKF\n");
        _ekf.SetProcessNoiseCov(_Q, llh());

        mat_type ekf_cov(5,5, 0.0);
        ekf_cov.subarray(0,3,0,3).inject(_spp.cov());
        ekf_cov(4,4) = 2*_Q(4,4);
        EKFPositionSolver::vec_type ekf_state(5, 0.0);
        ekf_state.subarray(0,3).inject(_spp.state());
        ekf_state(4) = _spp.c()*_oscCorr;
        _ekf.Reset(ekf_state, ekf_cov);
    }
private:
    mat_type _Q;
    double   _uere;
    double   _fOsc;
    SinglePointPositionSolver _spp;
    EKFPositionSolver         _ekf;
    int      _state;
    vec_type _pos;
    double   _t_rx;
    double   _oscCorr;
    uint64_t _adc_ticks;
    LonLatAlt _llh;
} ;

PosSolver::sptr PosSolver::make(const mat_type& Q,
                                double uere,
                                double fOsc)
{
    return PosSolver::sptr(new PosSolverImpl(Q, uere, fOsc));
}
