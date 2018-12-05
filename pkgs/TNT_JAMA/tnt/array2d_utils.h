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

#ifndef TNT_ARRAY2D_UTILS_H
#define TNT_ARRAY2D_UTILS_H

#include <cstdlib>
#include <cassert>

namespace TNT {

template <class T>
std::ostream &operator<<(std::ostream &s, const Array2D<T> &A) {
  const int M = A.dim1();
  const int N = A.dim2();

  s << M << " " << N << "\n";

  for (int i = 0; i < M; i++) {
    for (int j = 0; j < N; j++) {
      s << A[i][j] << " ";
    }
    s << "\n";
  }

  return s;
}

template <class T> std::istream &operator>>(std::istream &s, Array2D<T> &A) {
  int M, N;
  s >> M >> N;

  Array2D<T> B(M, N);
  for (int i = 0; i < M; i++) {
    for (int j = 0; j < N; j++) {
      s >> B[i][j];
    }
  }
  A = B;
  return s;
}

template <class T>
Array2D<T> operator+(const Array2D<T> &A, const Array2D<T> &B) {
  const int m = A.dim1();
  const int n = A.dim2();
#ifdef TNT_BOUNDS_CHECK
  assert(B.dim1() == m && B.dim2() == n);
#endif
  Array2D<T> C(m, n);
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < n; j++)
      C[i][j] = A[i][j] + B[i][j];
  }
  return C;
}

template <class T>
Array2D<T> operator-(const Array2D<T> &A, const Array2D<T> &B) {
  const int m = A.dim1();
  const int n = A.dim2();
#ifdef TNT_BOUNDS_CHECK
  assert(B.dim1() == m && B.dim2() == n);
#endif
  Array2D<T> C(m, n);
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < n; j++)
      C[i][j] = A[i][j] - B[i][j];
  }
  return C;
}

#if 0
    template <class T>
    Array2D<T> operator*(const Array2D<T> &A, const Array2D<T> &B) {
        int m = A.dim1();
        int n = A.dim2();

        if (B.dim1() != m ||  B.dim2() != n )
            return Array2D<T>();

        Array2D<T> C(m,n);

        for (int i=0; i<m; i++) {
            for (int j=0; j<n; j++)
                C[i][j] = A[i][j] * B[i][j];
        }
    }
}
#else
template <class T>
Array2D<T> operator*(const Array2D<T> &A, const Array2D<T> &B) {
  return matmult(A, B);
}
#endif
template <class T> Array2D<T> operator*(const Array2D<T> &A, T scale) {
  Array2D<T> B = A.copy();
  return B *= scale;
}
template <class T> Array2D<T> operator*(T scale, const Array2D<T> &A) {
  return A * scale;
}
template <class T> Array2D<T> operator/(const Array2D<T> &A, T scale) {
  Array2D<T> B = A.copy();
  return B * (T(1)/scale);
}

template <class T> Array2D<T> eye(int n) {
  Array2D<T> id(n, n, T(0));
  for (int i = 0; i < n; i++)
    id[i][i] = 1;
  return id;
}

template <class T> Array2D<T> transpose(const Array2D<T> &A) {
  const int m = A.dim1();
  const int n = A.dim2();
  Array2D<T> At(n, m);
  for (int i = 0; i < m; ++i)
    for (int j = 0; j < n; ++j)
      At[j][i] = A[i][j];
  return At;
}
template <class T>
Array2D<T> operator/(const Array2D<T> &A, const Array2D<T> &B) {
  const int m = A.dim1();
  const int n = A.dim2();
#ifdef TNT_BOUNDS_CHECK
  assert(B.dim1() == m && B.dim2() != n);
#endif
  Array2D<T> C(m, n);
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < n; j++)
      C[i][j] = A[i][j] / B[i][j];
  }
  return C;
}

template <class T> Array2D<T> &operator+=(Array2D<T> &A, const Array2D<T> &B) {
  const int m = A.dim1();
  const int n = A.dim2();
#ifdef TNT_BOUNDS_CHECK
  assert(B.dim1() == m && B.dim2() == n);
#endif
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < n; j++)
      A[i][j] += B[i][j];
  }
  return A;
}

template <class T> Array2D<T> &operator-=(Array2D<T> &A, const Array2D<T> &B) {
  const int m = A.dim1();
  const int n = A.dim2();
#ifdef TNT_BOUNDS_CHECK
  assert(B.dim1() == m && B.dim2() == n);
#endif
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < n; j++)
      A[i][j] -= B[i][j];
  }
  return A;
}

#if 0
    template <class T>
    Array2D<T>&  operator*=(Array2D<T> &A, const Array2D<T> &B) {
        int m = A.dim1();
        int n = A.dim2();

        if (B.dim1() == m ||  B.dim2() == n ) {
            for (int i=0; i<m; i++) {
                for (int j=0; j<n; j++)
                    A[i][j] *= B[i][j];
            }
        }
        return A;
    }
#endif

template <class T> Array2D<T> &operator*=(Array2D<T> &A, T scale) {
  for (int i = 0; i < A.dim1(); i++) {
    for (int j = 0; j < A.dim2(); j++)
      A[i][j] *= scale;
  }
  return A;
}

template <class T> Array2D<T> &operator/=(Array2D<T> &A, const Array2D<T> &B) {
  const int m = A.dim1();
  const int n = A.dim2();
#ifdef TNT_BOUNDS_CHECK
  assert(B.dim1() == m && B.dim2() == n);
#endif
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < n; j++)
      A[i][j] /= B[i][j];
  }
  return A;
}

/**
   Matrix Multiply:  compute C = A*B, where C[i][j]
   is the dot-product of row i of A and column j of B.


   @param A an (m x n) array
   @param B an (n x k) array
   @return the (m x k) array A*B, or a null array (0x0)
   if the matrices are non-conformant (i.e. the number
   of columns of A are different than the number of rows of B.)


*/
template <class T>
Array2D<T> matmult(const Array2D<T> &A, const Array2D<T> &B) {
#ifdef TNT_BOUNDS_CHECK
  assert(A.dim2() == B.dim1());
#endif
  const int M = A.dim1();
  const int N = A.dim2();
  const int K = B.dim2();

  Array2D<T> C(M, K);
  for (int i = 0; i < M; i++) {
    for (int j = 0; j < K; j++) {
      T sum = 0;
      for (int k = 0; k < N; k++)
        sum += A[i][k] * B[k][j];

      C[i][j] = sum;
    }
  }
  return C;
}

} // namespace TNT

#endif
