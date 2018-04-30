// -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil  -*-
// $Id$
//
// Copyright 2010-2014 Christoph Mayer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef _PLL_HPP_cm110527_
#define _PLL_HPP_cm110527_

#include <vector>
#include <complex>

namespace filter {
  // phase-locked loop
  template <typename T,
            class LOOP_FILTER,
            class INTEGRATOR>
  class pll {
  public:
    typedef T float_type;
    typedef typename std::complex<float_type> complex_type;
    typedef typename std::vector<complex_type> complex_vector_type;

    pll(double fc,
        double fs,
        double dwl, // band width
        const LOOP_FILTER& loop_filter,
        const INTEGRATOR& integrator)
      : loop_filter_(loop_filter)
      , integrator_(integrator)
      , f1_(0)
      , fc_(fc)
      , ts_(1./fs)
      , dwl_(dwl)
      , ppb_(0)
      , exp_i_phase_(1) {
      reset();
    }

    void update_ppb(double ppb, double fc) {
      ppb_ = ppb;
      fc_  = fc;
      loop_filter_.update_ppb(ppb);
    }

    // filter reset
    void reset() {
      f1_= 2*M_PI*fc();
      loop_filter_.reset();
      integrator_.reset();
      exp_i_phase_ = 1;
    }

    // filter update
    double process(complex_type s) {
      const complex_type i_phase(float_type(0), integrator_.process(f1_*ts()));
      exp_i_phase_ = std::exp(i_phase);
      const double two_pi(2*M_PI);
      f1_ = two_pi*fc() + loop_filter_.process(std::arg(s * std::conj(exp_i_phase_)));
      if (f1_ > two_pi*f1_upper_limit() || f1_ < two_pi*f1_lower_limit())
        reset();
      return theta();
    }

    double fc() const { return fc_; }
    double ts() const { return ts_*(1+ppb_*1e-9); }

    float_type theta() const { return integrator_.get(); }
    float_type uf() const { return loop_filter_.get(); }
    float_type f1() const { return f1_; }

    complex_type exp_i_phase() const { return exp_i_phase_; }

  protected:
    const LOOP_FILTER& loop_filter() const { return loop_filter_; }
    const INTEGRATOR& integrator() const { return integrator_; }

    double f1_upper_limit() const { return fc()+10*dwl_; }
    double f1_lower_limit() const { return fc()-10*dwl_; }

  private:
    LOOP_FILTER loop_filter_;
    INTEGRATOR  integrator_;
    double f1_;               // current nco frequency
    double fc_;
    double ts_;
    double dwl_;
    double ppb_;
    complex_type exp_i_phase_;
  } ;
} // namespace filter
#endif // _PLL_HPP_cm110527_
