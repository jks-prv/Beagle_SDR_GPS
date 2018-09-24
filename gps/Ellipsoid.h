// -*- C++ -*-
#ifndef _GPS_ELLIPSOID_H
#define _GPS_ELLIPSOID_H

#include <cmath>

#define TNT_BOUNDS_CHECK
#include <tnt.h>
#include <jama.h>

#include "PosSolver.h"

class Ellipsoid {
public:
  typedef PosSolver::LonLatAlt LonLatAlt;
  typedef PosSolver::vec_type vec_type;
  typedef PosSolver::mat_type mat_type;

  // default is WGS84
  Ellipsoid(double a=6378137.0,
            double b=6356752.31424518,
            int max_iter=10,
            double dh=1e-3)
    : _a(a)
    , _b(b)
    , _max_iter(max_iter)
    , _dh(dh) {}

  // LLH -> XYZ  (LLH = LonLatAlt)
  vec_type LLH2XYZ(LonLatAlt const& llh) const {
    double const cp = std::cos(llh.phi()),    sp = std::sin(llh.phi());
    double const cl = std::cos(llh.lambda()), sl = std::sin(llh.lambda());
    double const nu = rc_normal(llh);
    return TNT::makeVector<double>({{(nu + llh.alt())*cp*cl,
            (nu + llh.alt())*cp*sl,
            ((1-e2())*nu + llh.alt())*sp}});
  }
  // XYZ -> LLH
  LonLatAlt XYZ2LLH(vec_type x) const {
    double const rho      = TNT::norm(x.subarray(0,1));
    double const z_by_rho = x(2)/rho;
    double const lambda   = 2.0*std::atan2(x(1), x(0)+rho);
    double alt[2] = { 0,0 };
    double phi = phi_iter(z_by_rho, 1.0);
    for (int i=0; i<max_iter(); ++i) {
      const double nu = rc_normal(phi);
      alt[1] = rho/std::cos(phi) - nu;
      phi    = phi_iter(z_by_rho, nu/(nu+alt[1]));
      if (std::abs(alt[1]-alt[0]) < dh())
        break;
      alt[0] = alt[1];
    }
    return LonLatAlt(lambda, phi, alt[1]);
  }

  // Jacobian dXYZ/dENU (ENU = EastNorthUp)
  mat_type dXYZdENU(LonLatAlt const& llh) const {
    double const cp = std::cos(llh.phi()),    sp = std::sin(llh.phi());
    double const cl = std::cos(llh.lambda()), sl = std::sin(llh.lambda());
    return TNT::make3x3<double>({{-sl, -sp*cl, cp*cl}},
                                {{+cl, -sp*sl, cp*sl}},
                                {{0.0, +cp,    sp}});
  }
  // Jacobian dENU/dXYZ
  mat_type dENUdXYZ(LonLatAlt const& llh) const {
    return TNT::transpose(dXYZdENU(llh)); // transpose = inverse
  }

  // Jacobian dENU/dLLH
  mat_type dENUdLLH(LonLatAlt const& llh) const {
    return TNT::makeDiag<double>({{(rc_normal(llh) + llh.alt())*std::cos(llh.phi()),
                                  rc_meridional(llh) + llh.alt(),
            1.0}});
  }
  // Jacobian dLLH/dENU
  mat_type dLLHdENU(LonLatAlt const& llh) const {
    double determinant = 0.0;
    return TNT::invert_lu(dENUdLLH(llh), determinant);
  }

protected:
  double phi_iter(double z_by_rho, double nu_ratio) const {
    return std::atan(z_by_rho / (1.0 - e2()*nu_ratio));
  }
  // normal radius of curvature
  double rc_normal(LonLatAlt const& llh) const {
    return rc_normal(llh.phi());
  }
  double rc_normal(double phi) const {
    double const sp(std::sin(phi));
    return a()/std::sqrt(1.0 - e2()*sp*sp);
  }
  // meridional radius of curvature
  double rc_meridional(LonLatAlt const& llh) const {
    double const sp(std::sin(llh.phi()));
    return a()*(1.0 - e2())*std::pow(1.0 - e2()*sp*sp, -1.5);
  }
  double a()  const { return _a; }
  double b()  const { return _b; }
  double a2() const { return _a*_a; }
  double b2() const { return _b*_b; }
  double e2() const { return 1.0 - b2()/a2(); }

  int max_iter() const { return _max_iter; }
  double dh() const { return _dh; }

 private:
  const double _a;
  const double _b;
  const int    _max_iter;
  const double _dh;
} ;

#endif // _GPS_ELLIPSOID_H
