// -*- C++ -*-

#ifndef _GPS_EKF_POSITION_SOLVER_H_
#define _GPS_EKF_POSITION_SOLVER_H_

#include "PositionSolverBase.h"

// Extended Kalman filter position solution

class EKFPositionSolver : public PositionSolverBase {
public:
    // state x = (x,y,z,ct,ctdot) with units [m,m,m,m,m/s]
    EKFPositionSolver(kiwi_yield::wptr yield=kiwi_yield::wptr())
        : PositionSolverBase(5, yield)
        , _q{{5e-5, 5e-5, 5e-5}}
        , _h{{1.8e-21, 6.5e-22, 1.4e-24}}
        , _valid(false) {}

    virtual ~EKFPositionSolver() {}

    void Reset(vec_type const& state, mat_type const& state_cov) {
        _state.inject(state);
        _cov.inject(state_cov);
    }
    mat_type const& P() const { return _cov; }
    mat_type&       P()       { return _cov; }

    bool update(mat_type sv,     // (4,nsv)
                vec_type weight, // (nsv,1)
                double dt)       // [sec]
    {
        int nsv = sv.dim2();
        if (nsv < 1)
            return (_valid = false);

        assert(sv.dim1() == 4 && weight.dim() == nsv);

        // (1) pred state
        mat_type const Phi = MakePhi(dt);
        vec_type        xp = Phi * state();
        xp(3) = mod_gpsweek_abs(xp(3));

        // (2) pred. measurements+Jacobian at xp
        mat_type h(nsv, 5, 0.0);
        vec_type dz(nsv, 0.0);
        vec_type w(nsv, 0.0);
        nsv = 0;
        Iter(xp(3), sv, [&h,&dz,&w,&weight,&nsv,this](int i_sv, vec_type const& dp, double cdt) {
                yield();
                double const z  = cdt;           // measured pseudorange
                double const zp = TNT::norm(dp); // predicted pseudorange
#ifdef DEBUG_POS_SOLVER
                printf("EKF %3d %10.3f %e\n", i_sv, z-zp, weight(nsv));
#endif
                if (std::abs(z-zp) < 1e3) {      // filter outliers
                    dz(nsv)  = z - zp;
                    h.subarray(nsv,nsv,0,2).inject(dp/zp);
                    h(nsv,3) = -1;
                    h(nsv,4) =  0;
                    w(nsv)   = weight(nsv);
                    ++nsv;
                }
            });

        if (nsv < 1)
          return (_valid = false);

        h  = h.subarray (0,nsv-1, 0,4).copy();
        dz = dz.subarray(0,nsv-1).copy();
        w  = w.subarray (0,nsv-1).copy();

        // make up measurement and process noise  covariance matrices
        mat_type const R = TNT::makeDiag(w);
        mat_type const Q = MakeQ(dt);

        // (3)
        mat_type const  Pp = Phi * P() * TNT::transpose(Phi) + Q; // (5,5)
        mat_type const tmp = h * Pp * TNT::transpose(h) + R;      // (nsv,nsv)
        yield();
        double det = 0;
        mat_type const cov = TNT::invert_lu(tmp, det);            // (nsv,nsv)
        if (det < 1e-30)
          return (_valid = false);
        mat_type   const G = Pp*TNT::transpose(h) * cov;          // (5,nsv)

        // (4) update state and P
        vec_type dx = G*dz;
#ifdef DEBUG_POS_SOLVER
        printf("EKF dx %10.3f %10.3f %10.3f %15.9f %15.9f %10.3f\n",
               dx(0), dx(1), dx(2), dx(3), dx(4), TNT::norm(dx.subarray(0,2)));
#endif
        if (TNT::norm(dx.subarray(0,2)) > 1e3)
          return (_valid = false);

        _state.inject(xp);
        _state    += dx;
        _state(3)  = mod_gpsweek_abs(_state(3));
        P().inject((TNT::eye<double>(5) - G * h) * Pp);
        return (_valid = true);
    }

protected:
    static mat_type MakePhi(double dt) {
        mat_type phi(TNT::eye<double>(5));
        phi(3,4) = dt;
        return phi;
    }
    mat_type MakeQ(double dt) {
        dt = std::max(0.1, dt);
        double const dt2 =  dt*dt;
        double const dt3 = dt2*dt;
        mat_type q(5,5, 0.0);
        q(0,0) = std::pow(_q[0], 2); // [m^2]
        q(1,1) = std::pow(_q[1], 2); // [m^2]
        q(2,2) = std::pow(_q[2], 2); // [m^2]
        q(3,3)          = c2()*(0.5*_h[0]*dt + 2*_h[1]*dt2 + 2.0/3.0*M_PI*M_PI*_h[2]*dt3);  // [m^2]
        q(3,4) = q(4,3) = c2()*(               2*_h[1]*dt  +         M_PI*M_PI*_h[2]*dt2);  // [m^2/sec]
        q(4,4)          = c2()*(0.5*_h[0]/dt + 2*_h[1]     + 8.0/3.0*M_PI*M_PI*_h[2]*dt);   // [m^2/sec^2]
        if (_valid) {
          mat_type const h = wgs84().dXYZdENU(LLH());
          q.subarray(0,2, 0,2).inject(h * q.subarray(0,2,0,2) * TNT::transpose(h));
        }
        return q;
    }
private:
    std::array<double, 3> _q;  // XYZ process noise sigma [m]
    std::array<double, 3> _h;  // Allan variance parameters [sec, 1, 1/sec]
    bool _valid;
} ;

#endif // _GPS_EKF_POSITION_SOLVER_H_
