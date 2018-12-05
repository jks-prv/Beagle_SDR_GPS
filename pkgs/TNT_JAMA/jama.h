#ifndef JAMA_H
#define JAMA_H

#include <tnt.h>
#include <jama/lu.h>
#include <jama/qr.h>
#include <jama/cholesky.h>

// matrix inversion

namespace TNT {
  namespace {
    template<class T, class DECOMP>
    Array2D<T> invert(const DECOMP& decomp, int n, T& det) {
      det = decomp.det();
      return decomp.solve(eye<T>(n));
    }
  }
  template<class T>
  Array2D<T> invert_lu(const Array2D<T> &M, T& determinant)
  {
#ifdef TNT_BOUNDS_CHECK
    assert(M.dim1() == M.dim2());
#endif
    const JAMA::LU<T> lu(M);
    return invert(lu, M.dim1(), determinant);
  }
  template<class T>
  Array2D<T> invert_qr(const Array2D<T> &M, T& determinant)
  {
#ifdef TNT_BOUNDS_CHECK
    assert(M.dim1() == M.dim2());
#endif
    const JAMA::QR<T> qr(M);
    return invert(qr, M.dim1(), determinant);
  }
  template<class T>
  Array2D<T> invert_chol(const Array2D<T> &M, T& determinant)
  {
#ifdef TNT_BOUNDS_CHECK
    assert(M.dim1() == M.dim2());
#endif
    const JAMA::Cholesky<T> chol(M);
    return invert(chol, M.dim1(), determinant);
  }
} // namespace TNT

#endif // JAMA_H
