// -*- C++ -*-

#ifndef _GPS_KIWI_YIELD_H_
#define _GPS_KIWI_YIELD_H_

#include <memory>

//
// This class is used to isolate the position solution algorithms from the KiwiSDR framework
//
// The idea is that the position solution classes get a weak pointer of kiwi_yield
// and by this are able to call the yield method without referring to NextTask.
//
class kiwi_yield {
public:
    typedef std::shared_ptr<kiwi_yield> sptr;
    typedef std::weak_ptr<kiwi_yield> wptr;

    virtual void yield() const = 0;

protected:
    kiwi_yield() = default;
private:
    kiwi_yield(kiwi_yield const& ) = delete;
    kiwi_yield& operator=(kiwi_yield const& ) = delete;
} ;

#endif // _GPS_KIWI_YIELD_H_
