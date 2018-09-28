// -*- C++ -*-

#ifndef KIWI

#include <iostream>
#include <string>
#include <sstream>

#define TNT_BOUNDS_CHECK
#include <tnt/array1d.h>
#include <tnt/array2d.h>
#include <tnt/utils.h>
#include <tnt/array1d_utils.h>
#include <tnt/array2d_utils.h>

#define DEBUG_POS_SOLVER
#include "PosSolver.h"

typedef TNT::Array1D<double> vec_type;
typedef TNT::Array2D<double> mat_type;
typedef PosSolver::LonLatAlt LonLatAlt;

const double C = 2.99792458e8;    // Speed of light (m/s)
const double F0 = 66.66e6;        // nominal ADC clock speed (Hz)

const double uere = 6.0; // [m]

int main()
{
  const int max_chan=12;

  int nsv = 0;
  vec_type weight(max_chan, 0.0);
  mat_type sv(4, max_chan, 0.0);
  uint64_t ticks;
  PosSolver::sptr p = PosSolver::make(uere, F0);

  std::string line, prn[max_chan];
  while (std::getline(std::cin, line)) {
    std::size_t found = line.find("GNSS");
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
        sv(3,nsv) *= C; // sec -> m
        nsv += (weight(nsv) > 1e5 && weight(nsv) < 5e6);
      } else if (tok == "GNSS_end") {
        vec_type _weight = weight.subarray(0,nsv-1).copy();
        mat_type _sv     = sv.subarray(0,3,0,nsv-1).copy();

        for (int i=0; i<nsv; ++i) {
          printf("SAT %2d %s %13.3f %13.3f %13.3f %.9f %10.0f\n", i, prn[i].c_str(),
                 sv(0,i), sv(1,i), sv(2,i), sv(3,i)/C, _weight(i));
        }
        p->solve(_sv, _weight, ticks);
      }
    }
  }
}
#endif
