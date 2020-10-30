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

#pragma once

class EPHEM {
    int sat;
    bool isE1B;

    // E1B
    unsigned IODN[4];
    
    // Subframe 1
    unsigned IODC, t_oc;
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

    double EccentricAnomaly(double t_k) const;

public:
    double A() const { return sqrtA*sqrtA; }     // Semi-major axis
    unsigned week, tow;
    
    // debug
    u4_t sub, tow_pg, tow_time;
    double t_tx_prev, L_lat, L_d_lat, L_lon, L_d_lon, L_alt;

    // GST-GPS
    double A_0G, A_1G;
    unsigned t_0G, WN_0G;

    // E1B
    void PageN(unsigned page);
    void Page1(unsigned IODC, double M0, double e, double sqrtA, unsigned toe=0);
    void Page2(unsigned IODC, double OMG0, double i0, double omg, double idot);
    void Page3(unsigned IODC, double OMGd, double deln, double cuc, double cus, double crc, double crs);
    void Page4(unsigned IODC, double cic, double cis, double f0, double f1, double f2, unsigned toc=0);
    void Page5(unsigned tow, unsigned week, double tgd, double toc, double toe);
    void Page6(unsigned tow, unsigned week);
    void Page0(unsigned tow, unsigned week);

    void   Init(int sat);
    void   Subframe(char *buf);
    bool   Valid();
    double GetClockCorrection(double t) const;
    void   GetXYZ(double *x, double *y, double *z, double t) const;
    double TimeOfEphemerisAge(double t) const;
};

extern EPHEM Ephemeris[];
