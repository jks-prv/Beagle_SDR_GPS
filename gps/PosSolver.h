// -*- C++ -*-

#ifndef GPS_POS_SOLVER_H
#define GPS_POS_SOLVER_H

#include <cmath>
#include <stdint.h>
#include <memory>
#include <vector>

#ifdef KIWI
#include "kiwi_assert.h"
#endif
//#define DEBUG_POS_SOLVER

#define TNT_BOUNDS_CHECK
#include <tnt/array1d.h>
#include <tnt/array2d.h>

#include "kiwi_yield.h"

class PosSolver {
public:
    typedef TNT::Array1D<double> vec_type;
    typedef TNT::Array2D<double> mat_type;

    struct ElevAzim {
        double elev_deg, azim_deg;
    } ;

    class LonLatAlt {
    public:
        LonLatAlt(double lambda=0, double phi=0, double alt=6371e3)
            : _lambda(lambda)
            , _phi(phi)
            , _alt(alt) {}

        double lambda() const { return _lambda; }
        double phi()    const { return _phi; }
        double alt()    const { return _alt; }

        double lon()    const { return _lambda/M_PI*180; }
        double lat()    const { return _phi/M_PI*180; }
    protected:
    private:
        double _lambda; // [rad]
        double _phi;    // [rad]
        double _alt;    // [m]
    } ;

    typedef std::unique_ptr<PosSolver> sptr;
    static sptr make(double uere,
                     double fOsc,
                     kiwi_yield::wptr yield=kiwi_yield::wptr());

    // input: GNSS observables + oscillator tick counter
    virtual bool solve(mat_type sv, vec_type weight, uint64_t ticks) = 0;

    virtual void set_use_kalman(bool ) = 0;

    // need to propagate information to kiwisdr global variables
    virtual std::vector<ElevAzim> elev_azim(mat_type sv) = 0;
#ifdef KIWI_DEBIAN_7
    virtual bool spp_valid() = 0;
    virtual bool ekf_valid() = 0;
    virtual vec_type const& pos() = 0;
    virtual double pos(int i) = 0;
    virtual double t_rx() = 0;
    virtual double osc_corr() = 0;
    virtual LonLatAlt const& llh() = 0;
#else
    virtual bool spp_valid() const = 0;
    virtual bool ekf_valid() const = 0;
    virtual vec_type const& pos() const = 0;
    virtual double pos(int i) const = 0;
    virtual double t_rx() const = 0;
    virtual double osc_corr() const = 0;
    virtual LonLatAlt const& llh() const = 0;

    virtual ~PosSolver() = default;
#endif

protected:
  PosSolver() = default;

private:
    PosSolver(PosSolver const&) = delete;
    PosSolver& operator=(PosSolver const&) = delete;
} ;

#endif // GPS_POS_SOLVER_H
