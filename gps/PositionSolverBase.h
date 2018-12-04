// -*- C++ -*-

#ifndef _GPS_POS_SOLVER_BASE_H_
#define _GPS_POS_SOLVER_BASE_H_

#include "PosSolver.h"

#define TNT_BOUNDS_CHECK
#include <tnt.h>
#include <jama.h>

#include "kiwi_yield.h"
#include "Ellipsoid.h"

class PositionSolverBase {
public:
    typedef TNT::Array1D<double> vec_type;
    typedef TNT::Array2D<double> mat_type;
    typedef PosSolver::LonLatAlt LonLatAlt;

    PositionSolverBase(int dim, kiwi_yield::wptr yield)
        : _state(dim)
        , _cov(dim,dim)
        , _kiwi_yield(yield)
        , _wgs84()
        , _omega_e(7.2921151467e-5)
        , _c(2.99792458e8) {}

    void yield() const {
        kiwi_yield::sptr p = _kiwi_yield.lock();
        if (p)
          p->yield();
    }
    double omega_e() const { return _omega_e; }
    double c()       const { return _c; }
    double c2()      const { return c()*c(); }

    double state(int idx) const { return _state(idx); }
    const vec_type& state() const { return _state; }

    double cov(int i, int j) const {return _cov[i][j]; }
    const mat_type& cov() const { return _cov; }

    vec_type pos() { return _state.subarray(0,2).copy(); }
    double ct_rx() const { return _state(3); }

    LonLatAlt LLH() { return _wgs84.XYZ2LLH(pos()); }
    vec_type ENU(vec_type v) { return _wgs84.dENUdXYZ(LLH())*v; }

    void compute_dop(double uere, double& hdop, double& vdop, double &pdop, double& tdop, double& gdop)  {
        mat_type h = _wgs84.dENUdXYZ(LLH());
        mat_type c = h*cov().subarray(0,2,0,2)*TNT::transpose(h);
        hdop = std::sqrt(c(0,0)+c(1,1))/uere;
        vdop = std::sqrt(c(2,2))/uere;
        pdop = std::sqrt(c(0,0)+c(1,1)+c(2,2))/uere;
        tdop = std::sqrt(cov(3,3))/uere;
        gdop = std::sqrt(pdop*pdop + tdop*tdop);
    }

    template<typename F>
    void IterElevAzim(mat_type sv, F const& f) {
        auto g = [=](int i_sv, vec_type dp, double cdt) {
            vec_type enu = ENU(-1.0*dp);
            enu /= TNT::norm(enu);
            double const elev_rad = std::asin(enu(2));          // asin(U)
            double const azim_rad = std::atan2(enu(0), enu(1)); // atan(E/N)
            f(i_sv, elev_rad, azim_rad + (azim_rad < 0)*2*M_PI);
        };
        Iter(pos(), sv, g);
    }

    // -cws/2 .. +cws/2
    double mod_gpsweek_rel(double cdt) const { // cdt [m]
        // correction for roll-over of GPS week seconds
        double const cws = c()*7*24*3600; // C*(GPS week seconds)
        cdt -= (cdt > +0.5*cws)*cws;
        cdt += (cdt < -0.5*cws)*cws;
        return cdt;
    }
    // 0 .. +cws
    double mod_gpsweek_abs(double cdt) const { // cdt [m]
        // correction for roll-over of GPS week seconds
        double const cws = c()*7*24*3600; // C*(GPS week seconds)
        cdt -= (cdt >= cws)*cws;
        cdt += (cdt <    0)*cws;
        return cdt;
    }

protected:
    // used for building up the Jacobian matrix
    template<typename T>
    void Iter(double ct_rx, mat_type sv, T const& f) {
        for (int i_sv=0, nsv=sv.dim2(); i_sv<nsv; ++i_sv) {
            double const ct_tx = sv(3,i_sv);
            double const cdt   = mod_gpsweek_rel(ct_rx - ct_tx);
            double const theta = -cdt/c()*omega_e();
            mat_type satpos = MakeRotZ(theta) * sv.subarray(0,2,i_sv,i_sv);
            vec_type dp     = pos() - satpos;
            f(i_sv, dp, cdt);
        }
    }
    static mat_type MakeRotZ(double theta) {
        double const ct=std::cos(theta), st=std::sin(theta);
        return TNT::make3x3<double>({{ct,-st,0}}, {{st,ct,0}}, {{0,0,1}});
    }
    // used for consistency checks; t_rx is not used and not needed
    template<typename T>
    void Iter(vec_type user_pos, mat_type sv, T const& f) {
        for (int i_sv=0, nsv=sv.dim2(); i_sv<nsv; ++i_sv) {
            mat_type satpos  = sv.subarray(0,2,i_sv,i_sv);
            vec_type dp      = user_pos - satpos;
            double cdt       = TNT::norm(dp);
            double theta_old = 0;
            // "light-time equation"-like fixed point iteration
            for (int i=0; i<5; ++i) {
                double const theta = -cdt/c()*omega_e();
                dp.inject(user_pos - MakeRotZ(theta) * satpos);
                cdt = TNT::norm(dp);
                if (std::abs(theta-theta_old) < 1e-9)
                    break;
                theta_old = theta;
            }
            f(i_sv, dp, cdt);
        }
    }

    Ellipsoid const& wgs84() const { return _wgs84; }

protected:
    vec_type _state;
    mat_type _cov;

private:
    kiwi_yield::wptr _kiwi_yield;
    Ellipsoid const  _wgs84;    // WGS84 ellipsoid
    double    const  _omega_e;  // WGS84 Earth's rotation rate (rad/s)
    double    const  _c;        // Speed of light (m/s)
} ;

#endif // _GPS_POS_SOLVER_BASE_H_
