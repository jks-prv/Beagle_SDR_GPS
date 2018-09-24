// -*- C++ -*-

#include <iostream>
#include <sstream>

//#include <cmath>

#define TNT_BOUNDS_CHECK
#include <tnt/array1d.h>
#include <tnt/array2d.h>
#include <tnt/utils.h>
#include <tnt/array1d_utils.h>
#include <tnt/array2d_utils.h>

#include "PosSolver.h"
#include "SinglePointPositionSolver.h"
#include "EKFPositionSolver.h"

typedef TNT::Array1D<double> vec_type;
typedef TNT::Array2D<double> mat_type;
typedef PosSolver::LonLatAlt LonLatAlt;

double const C = 2.99792458e8;    // Speed of light (m/s)
double const F0 = 66.66e6;        // nominal ADC clock speed (Hz)
double const UERE = 6.0; // [m]

int const max_chan = 12;

#ifndef KIWI
int main()
{
  int nsv = 0;
  vec_type weight(max_chan, 0.0);
  mat_type sv(4, max_chan, 0.0);
  uint64_t ticks = 0;

  PosSolver::sptr p = PosSolver::make(UERE, F0);

  std::string line, prn[max_chan];
  while (std::getline(std::cin, line)) {
    std::size_t const found = line.find("GNSS");
    if (found != std::string::npos) {
      line = line.substr(found, std::string::npos);
      std::istringstream iss(line);
      std::string tok;
      iss >> tok;

      if (tok == "GNSS_start") {
        iss >> ticks;
        nsv = 0;
      } else if (tok == "GNSS_sat") {
        iss >> prn[nsv]
            >> sv(0,nsv)
            >> sv(1,nsv)
            >> sv(2,nsv)
            >> sv(3,nsv)
            >> weight(nsv);
        sv(3,nsv++) *= C; // sec -> m
      } else if (tok == "GNSS_end") {
        vec_type _weight = weight.subarray(0,nsv-1).copy();
        mat_type _sv     = sv.subarray(0,3,0,nsv-1).copy();

        for (int i=0; i<nsv; ++i) {
          printf("SAT %2d %s %13.3f %13.3f %13.3f %.9f\n", i, prn[i].c_str(),
                 sv(0,i), sv(1,i), sv(2,i), sv(3,i)/C);
        }
        p->solve(_sv, _weight, ticks);

        printf("%d %d\n", p->spp_valid(), p->ekf_valid());
      }
    }
  }
}
#endif
#if 0
      // ----------------------------------------------------------------------
      // (1) SPP position
      // ----------------------------------------------------------------------
      if (nsv >= 4) {
        double t0 = posSolverSPP.state(3)/C;
        bool success = posSolverSPP.Solve(sv.leftCols(nsv), weight.head(nsv));
        if (success) {
          const LonLatAlt llh = posSolverSPP.LLH();
          if (llh.alt() < 0 | llh.alt() > 9000)
            success = false;
          if (success) {
            const double dt = (ticks - ticks_p_old)/F0;
            ticks_p_old = ticks;
            Eigen::VectorXi idx_bad;
            success = success && posSolverSPP.quality_check(sv.leftCols(nsv), weight.head(nsv), idx_bad);
            double hdop,vdop,pdop,tdop,gdop;
            posSolverSPP.compute_dop(uere, hdop,vdop,pdop,tdop,gdop);
            printf("POS(%d): %13.3f %8.3f %13.3f %8.3f %13.3f %8.3f %.9f %.9f %.9f %10.6f %10.6f %4.0f  %7.1f %7.1f %7.1f %7.1f %7.1f\n",
                   success,
                   posSolverSPP.state(0),
                   std::sqrt(posSolverSPP.cov(0,0)),
                   posSolverSPP.state(1),
                   std::sqrt(posSolverSPP.cov(1,1)),
                   posSolverSPP.state(2),
                   std::sqrt(posSolverSPP.cov(2,2)),
                   posSolverSPP.state(3)/C,
                   std::sqrt(posSolverSPP.cov(3,3))/C,
                   (posSolverSPP.state(3)/C-t0)/dt,
                   llh.lat(),
                   llh.lon(),
                   llh.alt(),
                   hdop,vdop,pdop,tdop,gdop);
          }
          if (state != 2)
            state = success;
        }
      }
      // ----------------------------------------------------------------------
      // (2) EKF position solver
      // ----------------------------------------------------------------------
      if (state == 1) {
        posSolverEKF.SetProcessNoiseCov(Q, posSolverSPP.LLH());
        EKFPositionSolver::cov_type ekf_cov = EKFPositionSolver::cov_type::Zero();
        ekf_cov.topLeftCorner(4,4) = posSolverSPP.cov();
        ekf_cov(4,4)               = 2*Q(4,4);
        EKFPositionSolver::state_type ekf_state = EKFPositionSolver::state_type::Zero();
        ekf_state.head(4) = posSolverSPP.state();
        ekf_state(4)      = C*0.999903;
        posSolverEKF.Reset(ekf_state, ekf_cov);
        state = 2;
        if (ticks_old == 0)
          ticks_old = ticks;
      }
      if (state == 2) {
        double dt = (ticks - ticks_old)/F0; // TODO: modulo correction
        const bool success = posSolverEKF.update(sv.leftCols(nsv), weight.head(nsv), dt, nsv);
        printf("EKF: dt= %.9f success=%d\n", dt, success);
        if (!success) {
          state = 2;
        } else {
          ticks_old = ticks;
          const LonLatAlt llh = posSolverEKF.LLH();
          printf("EKF(%d): %13.3f %8.3f %13.3f %8.3f %13.3f %8.3f %.9f %.3e %.9f %.3e %2d  %10.6f %10.6f %4.0f\n",
                 success,
                 posSolverEKF.state(0),
                 std::sqrt(posSolverEKF.cov(0,0)),
                 posSolverEKF.state(1),
                 std::sqrt(posSolverEKF.cov(1,1)),
                 posSolverEKF.state(2),
                 std::sqrt(posSolverEKF.cov(2,2)),
                 posSolverEKF.state(3)/C,
                 std::sqrt(posSolverEKF.cov(3,3))/C,
                 posSolverEKF.state(4)/C,
                 std::sqrt(posSolverEKF.cov(4,4))/C,
                 nsv,
                 llh.lat(),
                 llh.lon(),
                 llh.alt());
        }
      }
    }
    double dummy=0;
    iss >> dummy >> ticks;
    sv     = Eigen::MatrixXd::Zero(4,max_chan);
    weight = Eigen::VectorXd::Zero(max_chan);
  } else if (tok == "GPS") {
    iss >> nsv;
    iss >> sv(0,nsv)
        >> sv(1,nsv)
        >> sv(2,nsv)
        >> sv(3,nsv)
        >> weight(nsv);
    //        sv(3,nsv) += (sv(3,nsv) < 7*24*3600/2)*7*24*3600;
    sv(3,nsv) *= C; // sec -> m
  }
}
#endif
