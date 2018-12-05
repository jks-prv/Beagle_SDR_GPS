// -*- C++ -*-
/*
 *
 * Template Numerical Toolkit (TNT)
 *
 * Mathematical and Computational Sciences Division
 * National Institute of Technology,
 * Gaithersburg, MD USA
 *
 *
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 *
 */

#ifndef TNT_ARRAY1D_UTILS_H
#define TNT_ARRAY1D_UTILS_H

#include <cstdlib>
#include <cassert>

namespace TNT {

template <class T>
std::ostream &operator<<(std::ostream &s, const Array1D<T> &A) {
  const int N = A.dim1();
#ifdef TNT_DEBUG
  s << "addr: " << (void *)&A[0] << "\n";
#endif
  s << N << "\n";
  for (int j = 0; j < N; j++)
    s << A[j] << "\n";
  return s << "\n";
}

template <class T> std::istream &operator>>(std::istream &s, Array1D<T> &A) {
  int N;
  s >> N;

  Array1D<T> B(N);
  for (int i = 0; i < N; i++)
    s >> B[i];
  A = B;
  return s;
}

template <class T>
Array1D<T> operator+(const Array1D<T> &A, const Array1D<T> &B) {
  const int n = A.dim1();
#ifdef TNT_BOUNDS_CHECK
  assert(B.dim1() == n);
#endif
  Array1D<T> C(n);
  for (int i = 0; i < n; i++)
    C[i] = A[i] + B[i];
  return C;
}

template <class T>
Array1D<T> operator-(const Array1D<T> &A, const Array1D<T> &B) {
  const int n = A.dim1();
#ifdef TNT_BOUNDS_CHECK
  assert(B.dim1() == n);
#endif
  Array1D<T> C(n);
  for (int i = 0; i < n; i++)
    C[i] = A[i] - B[i];
  return C;
}

template <class T>
Array1D<T> operator*(const Array1D<T> &A, const Array1D<T> &B) {
  Array1D<T> C = A.copy();
  C *= B;
  return C;
}

template <class T>
Array1D<T> operator/(const Array1D<T> &A, const Array1D<T> &B) {
  Array1D<T> C = A.copy();
  C /= B;
  return C;
}

template <class T>
Array1D<T> operator*(const Array1D<T> &A, T scale) {
  Array1D<T> C(A.copy());
  for (int i=0; i<C.dim(); ++i)
    C[i] *= scale;
  return C;
}
template <class T>
Array1D<T> operator*(T scale, const Array1D<T> &A) {
  return A*scale;
}
template <class T>
Array1D<T> operator/(const Array1D<T> &A, T scale) {
  Array1D<T> C(A.copy());
  C /= scale;
  return C;
}

template <class T> Array1D<T> &operator+=(Array1D<T> &A, const Array1D<T> &B) {
  const int n = A.dim1();
  if (B.dim1() == n) {
    for (int i = 0; i < n; i++)
      A[i] += B[i];
  }
  return A;
}

template <class T> Array1D<T> &operator-=(Array1D<T> &A, const Array1D<T> &B) {
  const int n = A.dim1();
#ifdef TNT_BOUNDS_CHECK
  assert(B.dim1() == n);
#endif
  for (int i = 0; i < n; i++)
    A[i] -= B[i];
  return A;
}

template <class T> Array1D<T> &operator*=(Array1D<T> &A, const Array1D<T> &B) {
  const int n = A.dim1();
#ifdef TNT_BOUNDS_CHECK
  assert(B.dim1() == n);
#endif
    for (int i = 0; i < n; i++)
      A[i] *= B[i];
  return A;
}

template <class T> Array1D<T> &operator/=(Array1D<T> &A, const Array1D<T> &B) {
  const int n = A.dim1();
#ifdef TNT_BOUNDS_CHECK
  assert(B.dim1() == n);
#endif
  for (int i = 0; i < n; i++)
    A[i] /= B[i];
  return A;
}

} // namespace TNT

#endif
