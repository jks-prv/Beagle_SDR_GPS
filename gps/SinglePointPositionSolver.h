// -*- C++ -*-

#ifndef _GPS_SINGLE_POINT_POSITION_SOLVER_H_
#define _GPS_SINGLE_POINT_POSITION_SOLVER_H_

#include "PositionSolverBase.h"
#include <jama.h>

class SinglePointPositionSolver : public PositionSolverBase {
public:
    // state = (x,y,z,ct) [m]
    SinglePointPositionSolver(int max_iter,
                              kiwi_yield::wptr yield=kiwi_yield::wptr())
        : PositionSolverBase(4, yield)
        , _max_iter(max_iter) {}
    virtual ~SinglePointPositionSolver() {}

    int max_iter() const { return _max_iter; }

    bool Solve(mat_type sv,
               mat_type weight) {
        int const nsv = sv.dim2();
        if (nsv < 4)
            return false;

        assert(sv.dim1() == 4 && weight.dim1() == nsv && weight.dim2() == nsv);

        // start point for iteration
        double const ct0 = TNT::mean(sv.subarray(3,3, 0,sv.dim2()-1)) + c()*75e-3;
        _state.inject(TNT::makeVector<double>({0,0,0,ct0}));

        int i=0;
        for (; i<max_iter(); ++i) {
            mat_type h(nsv,4, 0.0);
            vec_type drho(nsv, 0.0);
            Iter(ct_rx(), sv, [&h,&drho,this](int i_sv, const vec_type& dp, double cdt) {
                    yield();
                    double const dpn = TNT::norm(dp);
                    h.subarray(i_sv,i_sv,0,2).inject(dp/dpn);
                    h(i_sv,3)  = -1;
                    drho(i_sv) = dpn - cdt;
                });
            if (!ComputeCov(h, weight))
                return false;
            vec_type dxyzt = cov() * TNT::transpose(h) * weight * drho;
            _state   -= dxyzt;
            _state(3) = mod_gpsweek_abs(_state(3));
            if (TNT::norm(dxyzt.subarray(0,2)) < 0.001)
                break;
        }
        return (i < max_iter());
    }

protected:
    bool ComputeCov(mat_type const& h, mat_type const& weight) {
        double det = 0;
        mat_type const tmp     = TNT::transpose(h) * weight * h;
        mat_type const cov_tmp = TNT::invert_lu(tmp, det);
        if (det > 1e-20 && cov_tmp.dim1() == 4 && cov_tmp.dim2() == 4) {
            _cov.inject(cov_tmp);
            return true;
        }
        return false;
    }

private:
    int _max_iter;
} ;

#endif // _GPS_SINGLE_POINT_POSITION_SOLVER_H_
