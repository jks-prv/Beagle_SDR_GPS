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
#ifndef _IIR_HPP_cm110629_
#define _IIR_HPP_cm110629_

#include <vector>
#include <utility>
#include <cmath>
#include <stdexcept>

namespace filter {
    
  // -----------------------------------------------------------------------------
  //  filter interface
  // -----------------------------------------------------------------------------
  // template<typename STATE>
  // struct filter_base {
  //   virtual ~filter_base() {}
  //   virtual STATE process(STATE s) = 0;
  //   virtual STATE get() const = 0;
  //   virtual void reset(STATE s) = 0;
  // } ;
  // -----------------------------------------------------------------------------
  
  template<typename COEFF,
           typename STATE>
  class iir {
  public:
    typedef STATE state_type;
    typedef std::vector<COEFF> vector_type_coeff;
    typedef std::vector<STATE> vector_type_state;
    
    iir(size_t N)
      : a_(N, 0)
      , b_(N, 0)
      , in_(N, 0)
      , out_(N, 0) {
      if (N == 0)
        throw std::runtime_error("iir: N=0 not allowed");
      a_[0] = COEFF(1);
    }
    
    iir(const vector_type_coeff& a,
        const vector_type_coeff& b)
      : a_(a)
      , b_(b)
      , in_ (a.size(), 0)
      , out_(a.size(), 0) {
      if (a_.size() != b_.size())
        throw std::runtime_error("iir: a.size() != b.size()");
    }
    
    iir<COEFF,STATE>& init(const vector_type_coeff& a,
                           const vector_type_coeff& b) {
      if (a_.size() != a.size() || b_.size() != b.size())
        throw std::runtime_error("iir: a.size() != b.size()");
      std::copy(a.begin(), a.end(), a_.begin());
      std::copy(b.begin(), b.end(), b_.begin());
//       reset();
      return *this;
    }
    
    void reset(STATE s=STATE(0)) {
      for (size_t i(0), n(in_.size()); i<n; ++i)
        in_[i] = out_[i] = STATE(0);
      in_.back() = s;
    }
    
    STATE process(STATE s) {
      in_.back()  = s;
      out_.back() = b_[0]*in_.back();
      for (size_t i(1), n(in_.size()); i<n; ++i) {
        out_.back() += b_[i]*in_[n-1-i] - a_[i]*out_[n-1-i];
      }
      if (a_[0] != COEFF(1))
        out_.back() /= a_[0];
      std::copy(in_.begin() +1, in_.end(),  in_.begin());
      std::copy(out_.begin()+1, out_.end(), out_.begin());
      return out_.back();
    }
    
    STATE get() const { return out_.back(); }

  protected:
    vector_type_coeff a_;
    vector_type_coeff b_;

  private:
    vector_type_state in_;
    vector_type_state out_;
  } ;
  
  template<typename STATE>
  class loop_filter_2nd {
  public:
    typedef iir<STATE, STATE> iir_type;
    
    loop_filter_2nd(double xi,
                    double dwl,
                    double fs)
      : iir_(2)
      , xi_(xi)
      , dwl_(dwl)
      , fs_(fs) {
      init(xi, dwl, fs);
      reset();
    }
    // virtual ~loop_filter_2nd() {}

    void update_ppb(double ppb) {
      init(xi_, dwl_, fs_*(1.-1e-9*ppb));
    }

    void init(double xi,
              double dwl,
              double fs) {
      const double wl = dwl*2*M_PI;
      const double wn = wl/2/xi;
      const double k0 = 1.;
      const double kd = 1.;
      const double tau1 = k0*kd/wn/wn;
      const double tau2 = xi*2/wn;
      const double ts2 = 0.5/fs; // ts/2
      // loop filter coefficients from s -> z transform with prewarping
      //   (1 + s*tau1) / (s*tau2)
      typename iir_type::vector_type_coeff a(2);
      typename iir_type::vector_type_coeff b(2);
      a[0] =  1.0;
      a[1] = -1.0;
      b[0] = ts2/tau1*(1.+1./std::tan(ts2/tau2));
      b[1] = ts2/tau1*(1.-1./std::tan(ts2/tau2));
      iir_.init(a, b);
    }
    void reset(STATE s=0) { iir_.reset(s); }
    STATE process(STATE s) { return iir_.process(s); }
    STATE get() const { return iir_.get(); }
  protected:
  private:
    iir_type iir_;
    double xi_;
    double dwl_;
    double fs_;
  } ;

  template<typename STATE,
           typename COEFF>
  class iir_lowpass_1pole {
  public:
    iir_lowpass_1pole(double tc,
                      double fs) 
      : s_(0)
      , alpha_(1) {
      init(tc, fs);
    }
    
    void init(double tc,   // time constant
              double fs) { // sampling rate
      alpha_ = std::max(COEFF(0), std::min(COEFF(1), COEFF(1./(tc*fs))));
    }
    void reset(STATE s=0) { s_ = s; }
    STATE process(STATE s) { 
      s_ = (1 - alpha_)*s_ + alpha_*s; 
      return s_;
    }
    STATE get() const { return s_; }
  protected:
  private:
    STATE s_;
    COEFF alpha_;
  } ;
  
  template<typename STATE>
  class integrator {
  public:
    typedef STATE state_type;
    
    integrator() : s_(0) {}
    
    STATE get() const { return s_; }
    STATE process(STATE s) { return (s_ += s); }
    void reset(STATE s=0) { s_=s; }
  protected:
  private:
    STATE s_;
  } ;

  template<typename STATE>
  class integrator_constraint {
  public:
    typedef STATE state_type;
    
    integrator_constraint(state_type min,
                          state_type max) 
      : s_((min+max)/2)
      , min_(min)
      , max_(max) {}
    
    STATE get() const { return s_; }
    STATE process(STATE s) { 
      s_ = bound(s_+s);
      return s_;
    }
    void reset(STATE s=0) { s_=(s==0) ? (min_+max_)/2 : bound(s); }
  protected:
    STATE bound(STATE s) const {
      return (s < min_) ? min_ : (s > max_ ? max_ : s);
    }
  private:
    STATE s_;
    const STATE min_;
    const STATE max_;
  } ;
  
  template<typename STATE>
  class integrator_modulo {
  public:
    typedef STATE state_type;
    
    integrator_modulo(STATE modulo) 
      : s_(0)
      , modulo_(modulo) {}
    STATE get() const { return s_; }
    STATE process(STATE s) {
      s_ = modulo(s_+s);
      return s_;
    }
    void reset(STATE s=0) { s_=modulo(s); }
  protected:
  private:
    STATE modulo(STATE s) const {
      return (s > modulo_/2) ? modulo(s-modulo_) : ((s <= -modulo_/2) ? modulo(s+modulo_) : s);
    }
    STATE s_;
    const STATE modulo_;
  } ;
  
  template<typename F1,
           typename F2>
  class filter_pair {
  public:
    typedef typename F2::state_type state_type;
    typedef typename std::pair<F1, F2> pair_type;
    
    filter_pair(const F1& f1,
                const F2& f2)
      : fp_(f1, f2) {}
    
    state_type get() const { return fp_.second.get(); }
    state_type process(state_type s) {
      return fp_.second.process(fp_.first.process(s));
    }
    void reset(state_type s=0) { 
      fp_.first.reset(s); 
      fp_.second.reset(fp_.first.get()); 
    }
  protected:
  private:
    pair_type fp_;
  } ;
} // namespace filter
#endif // _IIR_HPP_cm110629_
