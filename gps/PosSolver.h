// -*- C++ -*-

#ifndef GPS_POS_SOLVER_H
#define GPS_POS_SOLVER_H

#include <cmath>
#include <stdint.h>
#include <memory>
#include <vector>

#define TNT_BOUNDS_CHECK
#include <tnt/array1d.h>
#include <tnt/array2d.h>

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

    virtual ~PosSolver() = default;

    struct state {
        enum {
            NO_POS   = 0,
            HAS_POS  = (1<<0),
            HAS_TIME = (1<<1),
            HAS_POS_AND_TIME = HAS_POS | HAS_TIME,
            DO_EKF_TRACKING = (1<<2)
        };
    } ;

    typedef std::unique_ptr<PosSolver> sptr;
    static sptr make(const mat_type& Q,
                     double uere,
                     double fOsc);

    // input: GNSS observables + oscillator tick counter
    virtual bool solve(mat_type sv, vec_type weight, uint64_t ticks) = 0;

    // need to propagate information to kiwisdr global variables
    virtual int state() const = 0;
    virtual const vec_type pos() const = 0;
    virtual double t_rx() const = 0;
    virtual double osc_corr() const = 0;
    virtual const LonLatAlt& llh() const = 0;
    virtual std::vector<ElevAzim> elev_azim(mat_type sv) = 0;

protected:
    PosSolver() = default;

private:
    PosSolver(const PosSolver&) = delete;
    PosSolver& operator=(const PosSolver&) = delete;
} ;

#endif // GPS_POS_SOLVER_H
