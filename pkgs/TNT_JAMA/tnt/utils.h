// -*- C++ -*-

#ifndef TNT_UTILS_H
#define TNT_UTILS_H

#include <array>
#include <cmath>
#include <vector>

#include <tnt/array1d.h>
#include <tnt/array2d.h>

namespace TNT {

template<typename T>
Array2D<T> make3x3(const std::array<T, 3>& v1,
                   const std::array<T, 3>& v2,
                   const std::array<T, 3>& v3) {
    Array2D<T> a(3,3);
    for (int i=0; i<3; ++i) {
        a[0][i] = v1[i];
        a[1][i] = v2[i];
        a[2][i] = v3[i];
    }
    return a;
}
template<typename T>
Array2D<T> makeDiag(const std::vector<T>& v) {
    return makeDiag(Array1D<T>(v.size(), &v[0]));
}
template<typename T>
Array2D<T> makeDiag(Array1D<T> v) {
    const int n = v.dim();
    Array2D<T> a(n,n, T(0));
    for (int i=0; i<n; ++i)
        a[i][i] = v[i];
    return a;
}

template<typename T>
Array1D<T> makeVector(const std::vector<T>& v) {
    Array1D<T> a(v.size());
    for (size_t i=0; i<v.size(); ++i)
        a[i] = v[i];
    return a;
}
template<typename T>
Array1D<T> operator+(Array1D<T> a, Array1D<T> b) {
    assert(a.dim1() == b.dim1());
    Array1D<T> c(a.dim1(), T(0));
    for (int i=0; i<a.dim1(); ++i)
        c[i] = a[i] + b[i];
    return c;
}
template<typename T>
Array1D<T> operator+(Array1D<T> a, Array2D<T> b) {
    assert(a.dim1() == b.dim1() && b.dim1() == 1);
    Array1D<T> c(a.dim1(), T(0));
    for (int i=0; i<a.dim1(); ++i)
        c[i] = a[i] + b[i][0];
    return c;
}
template<typename T>
Array1D<T> operator+(Array2D<T> a, Array1D<T> b) {
    return b+a;
}
template<typename T>
Array1D<T> operator-(Array1D<T> a, Array1D<T> b) {
    assert(a.dim1() == b.dim1());
    Array1D<T> c(a.dim1(), T(0));
    for (int i=0; i<a.dim1(); ++i)
        c[i] = a[i] - b[i];
    return c;
}
template<typename T>
Array1D<T> operator-(Array1D<T> a, Array2D<T> b) {
    assert((a.dim() == b.dim1() && b.dim2()==1) ||
           (a.dim() == b.dim2() && b.dim1()==1));
    Array1D<T> c(a.dim(), T(0));
    for (int i=0; i<a.dim(); ++i)
        c[i] = a[i] - (b.dim2()==1 ? b[i][0] : b[0][i]);
    return c;
}
template<typename T>
Array1D<T> operator*(Array2D<T> a, Array1D<T> b) {
    assert(a.dim2() == b.dim());
    Array1D<T> c(a.dim1(), T(0));
    for (int i=0; i<a.dim1(); ++i)
        for (int j=0; j<a.dim2(); ++j)
            c[i] += a[i][j]*b[j];
    return c;
}

template<typename T> T norm(const Array1D<T>& A) {
    return std::sqrt(norm2(A));
}
template<typename T> T norm2(const Array1D<T>& A) {
    T sum(0);
    for (int i = 0; i < A.dim(); ++i)
        sum += A[i] * A[i];
    return sum;
}
template<typename T> T mean(const Array1D<T>& A) {
    const int n = A.dim();
    T sum(0);
    for (int i = 0; i < n; ++i) {
        sum += A[i];
    }
    return n ? sum/T(n) : T(0);
}
template<typename T> T norm(const Array2D<T>& A) {
    return std::sqrt(norm2(A));
}
template<typename T> T norm2(const Array2D<T>& A) {
    T sum(0);
    for (int i = 0; i < A.dim1(); ++i)
        for (int j = 0; j < A.dim2(); ++j)
            sum += A[i][j] * A[i][j];
    return sum;
}
template<typename T> T mean(const Array2D<T>& A) {
    const int n = A.dim1()*A.dim2();
    T sum(0);
    for (int i = 0; i < A.dim1(); ++i) {
        for (int j = 0; j < A.dim2(); ++j) {
            sum += A[i][j];
        }
    }
    return n ? sum/T(n) : T(0);
}

} // namespace TNT
#endif
