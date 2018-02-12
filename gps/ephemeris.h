//////////////////////////////////////////////////////////////////////////
// Homemade GPS Receiver
// Copyright (C) 2013 Andrew Holme
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
// http://www.holmea.demon.co.uk/GPS/Main.htm
//////////////////////////////////////////////////////////////////////////

class EPHEM {

    // Subframe 1
    unsigned week, IODC, t_oc;
    double t_gd, a_f[3];

    // Subframe 2
    unsigned IODE2, t_oe;
    double C_rs, dn, M_0, C_uc, e, C_us, sqrtA;

    // Subframe 3
    unsigned IODE3;
    double C_ic, OMEGA_0, C_is, i_0, C_rc, omega, OMEGA_dot, IDOT;

    // Subframe 4, page 18 - Ionospheric delay
    double alpha[4], beta[4];
    void LoadPage18(char *nav);

    void Subframe1(char *nav);
    void Subframe2(char *nav);
    void Subframe3(char *nav);
    void Subframe4(char *nav);
//  void Subframe5(char *nav);

    double A() const { return sqrtA*sqrtA; }
    double EccentricAnomaly(double t_k) const;

public:
    int prn;
    unsigned tow;

    void   Subframe(char *buf);
    bool   Valid() const;
    double GetClockCorrection(double t) const;
    void   GetXYZ(double *x, double *y, double *z, double t) const;
};

extern EPHEM Ephemeris[];
