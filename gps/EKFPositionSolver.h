// -*- C++ -*-

#ifndef _GPS_EKF_POSITION_SOLVER_H_
#define _GPS_EKF_POSITION_SOLVER_H_

#include "PositionSolverBase.h"

class EKFPositionSolver : public PositionSolverBase {
public:

    // state x = (x,y,z,ct,ctdot) with units [m,m,m,m,m/s]
    EKFPositionSolver()
        : PositionSolverBase(5)
        , _process_noise_cov(5,5) {}

    virtual ~EKFPositionSolver() {}

    void Reset(const vec_type& state, const mat_type& state_cov) {
        _state.inject(state);
        _cov.inject(state_cov);
    }
    void SetProcessNoiseCov(mat_type q) {
        _process_noise_cov.inject(q);
    }
    void SetProcessNoiseCov(mat_type q, const LonLatAlt& llh) { // q: ENU,ct,ctdot
        _process_noise_cov.inject(q);
        const mat_type h = wgs84().dXYZdENU(llh);
        _process_noise_cov.subarray(0,2, 0,2).inject(h * q.subarray(0,2,0,2) * TNT::transpose(h));
    }

    const mat_type& Q() const { return _process_noise_cov; }
    const mat_type& P() const { return _cov; }
    mat_type& P()  { return _cov; }

    bool update(mat_type& sv,    // (4,nsv)
                vec_type weight, // (nsv,1)
                double dt,       // [sec]
                int  nsv)        // number of satellites used
    {
        nsv = sv.dim2();
        if (nsv < 1)
            return false;

        assert(sv.dim1() == 4 && weight.dim() == nsv);

        // (1) pred state
        const mat_type Phi = MakePhi(dt);
        vec_type xp = Phi * state();
        const double cws = c()*7*24*3600; // C*(GPS week seconds)
        xp(3) -= (xp(3) > cws)*cws;

        // (2) pred. measurements+Jacobian at xp
        mat_type h(nsv, 5, 0.0);
        vec_type dz(nsv, 0.0);
        vec_type w(nsv, 0.0);
        nsv = 0;
        Iter(xp(3), sv, [&h,&dz,&w,&weight,&nsv](int i_sv, const vec_type& dp, double cdt) {
                const double z  = cdt;           // measured pseudorange
                const double zp = TNT::norm(dp); // predicted pseudorange
                printf("EKF_Iter(%2d): dz=%f (%f %f)\n", i_sv, z-zp, z, zp);
                if (std::abs(z-zp) < 10e3) {     // filter outliers
                    dz(nsv)  = z - zp;
                    h.subarray(nsv,nsv,0,2).inject(dp/zp);
                    h(nsv,3) = -1;
                    h(nsv,4) =  0;
                    w(nsv)   = weight(nsv);
                    ++nsv;
                }
            });

        if (nsv < 1)
            return false;

        h  = h.subarray (0,nsv-1, 0,4).copy();
        dz = dz.subarray(0,nsv-1).copy();
        w  = w.subarray (0,nsv-1).copy();

        for (int i=0; i<nsv; ++i)
            printf("dz %2d %.3f\n", i, dz(i));

        // make up meas. covariance
        const mat_type R = TNT::makeDiag(w);

        // (3)
        const mat_type  Pp = Phi * P() * TNT::transpose(Phi) + Q(); // (5,5)
        const mat_type tmp = h * Pp * TNT::transpose(h) + R;        // (nsv,nsv)
        double det=0;
        const mat_type cov = TNT::invert_lu(tmp, det);              // (nsv,nsv)
        // TODO check determinant
        const mat_type   G = Pp*TNT::transpose(h) * cov;            // (5,nsv)

        // (4) update state and P
        vec_type dx = G*dz;
        printf("dx %10.3f %10.3f %10.3f %15.9f %15.9f (%10.3f)\n", dx(0), dx(1), dx(2), dx(3), dx(4), TNT::norm(dx.subarray(0,2)));
        if (TNT::norm(dx.subarray(0,2)) > 100)
            return false;
        state().inject(xp);
        state()    += dx;
        state()(3) -= (state(3) > cws)*cws;
        P().inject((TNT::eye<double>(5) - G * h) * Pp);
        return true;
    }

protected:
    static mat_type MakePhi(double dt) {
        mat_type phi = TNT::eye<double>(5);
        phi(3,4) = dt;
        return phi;
    }
private:
    mat_type _process_noise_cov;
} ;

#endif // _GPS_EKF_POSITION_SOLVER_H_
