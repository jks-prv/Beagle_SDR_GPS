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

#include "gps.h"
#include "ephemeris.h"

#include <stdio.h>
#include <math.h>

EPHEM Ephemeris[MAX_SATS];

///////////////////////////////////////////////////////////////////////////////////////////////

static double TimeFromEpoch(double t, double t_ref) {
    t-= t_ref;
    if      (t> 302400) t -= 604800;
    else if (t<-302400) t += 604800;
    return t;
}

///////////////////////////////////////////////////////////////////////////////////////////////

union PACK {
    PACK(int a, int b=0, int c=0, int d=0) { byte[3]=a, byte[2]=b, byte[1]=c, byte[0]=d; }
    char     byte[4];
    unsigned u32;
    signed   s32;
    unsigned u(int n) { return u32>>(32-n); }
    signed   s(int n) { return s32>>(32-n); }
};

///////////////////////////////////////////////////////////////////////////////////////////////

void EPHEM::Subframe1(char *nav) {
    week      =               PACK(                  nav[ 6], nav[ 7]).u(10);
    t_gd      = pow(2, -31) * PACK(                           nav[20]).s( 8);
    IODC      =               PACK(                           nav[21]).u( 8);
    t_oc      =    (1 << 4) * PACK(                  nav[22], nav[23]).u(16);
    a_f[2]    = pow(2, -55) * PACK(                           nav[24]).s( 8);
    a_f[1]    = pow(2, -43) * PACK(                  nav[25], nav[26]).s(16);
    a_f[0]    = pow(2, -31) * PACK(         nav[27], nav[28], nav[29]).s(22);
    
    //if (!is_Navstar(sat)) printf("%s t_gd=0x%x\n", PRN(sat), PACK(nav[20]).u(8));
}

void EPHEM::Subframe2(char *nav) {
    IODE2     =               PACK(                           nav[ 6]).u( 8);
    C_rs      = pow(2,  -5) * PACK(                  nav[ 7], nav[ 8]).s(16);
    dn        = pow(2, -43) * PACK(                  nav[ 9], nav[10]).s(16) * PI;
    M_0       = pow(2, -31) * PACK(nav[11], nav[12], nav[13], nav[14]).s(32) * PI;
    C_uc      = pow(2, -29) * PACK(                  nav[15], nav[16]).s(16);
    e         = pow(2, -33) * PACK(nav[17], nav[18], nav[19], nav[20]).u(32);
    C_us      = pow(2, -29) * PACK(                  nav[21], nav[22]).s(16);
    sqrtA     = pow(2, -19) * PACK(nav[23], nav[24], nav[25], nav[26]).u(32);
    t_oe      =    (1 << 4) * PACK(                  nav[27], nav[28]).u(16);
}

void EPHEM::Subframe3(char *nav) {
    C_ic      = pow(2, -29) * PACK(                  nav[ 6], nav[ 7]).s(16);
    OMEGA_0   = pow(2, -31) * PACK(nav[ 8], nav[ 9], nav[10], nav[11]).s(32) * PI;
    C_is      = pow(2, -29) * PACK(                  nav[12], nav[13]).s(16);
    i_0       = pow(2, -31) * PACK(nav[14], nav[15], nav[16], nav[17]).s(32) * PI;
    C_rc      = pow(2,  -5) * PACK(                  nav[18], nav[19]).s(16);
    omega     = pow(2, -31) * PACK(nav[20], nav[21], nav[22], nav[23]).s(32) * PI;
    OMEGA_dot = pow(2, -43) * PACK(         nav[24], nav[25], nav[26]).s(24) * PI;
    IODE3     =               PACK(                           nav[27]).u( 8);
    IDOT      = pow(2, -43) * PACK(                  nav[28], nav[29]).s(14) * PI;
}

void EPHEM::LoadPage18(char *nav) {
	// Ionospheric delay
    alpha[0]  = pow(2, -30) * PACK(nav[ 7]).s(8);
    alpha[1]  = pow(2, -27) * PACK(nav[ 8]).s(8);
    alpha[2]  = pow(2, -24) * PACK(nav[ 9]).s(8);
    alpha[3]  = pow(2, -24) * PACK(nav[10]).s(8);
    beta [0]  = pow(2,  11) * PACK(nav[11]).s(8);
    beta [1]  = pow(2,  14) * PACK(nav[12]).s(8);
    beta [2]  = pow(2,  16) * PACK(nav[13]).s(8);
    beta [3]  = pow(2,  16) * PACK(nav[14]).s(8);
    
    // GPS/UTC delta time due to leap seconds (Navstar only)
    if (is_Navstar(sat)) {
        gps.delta_tLS  = PACK(nav[24]).s(8);
        gps.delta_tLSF = PACK(nav[27]).s(8);
        if (!gps.tLS_valid)
            printf("GPS/UTC +%d sec\n", gps.delta_tLS);
        gps.tLS_valid = true;
    }
}

void EPHEM::Subframe4(char *nav) {
    if (PACK(nav[6]).u(8) == ((1 << 6) + 56)) LoadPage18(nav);
}

///////////////////////////////////////////////////////////////////////////////////////////////

double EPHEM::TimeOfEphemerisAge(double t) const {

     // Time from ephemeris reference epoch
    double t_k = TimeFromEpoch(t, t_oe);
    return t_k;
}

///////////////////////////////////////////////////////////////////////////////////////////////

double EPHEM::EccentricAnomaly(double t_k) const {
    // Computed mean motion (rad/sec)
    double n_0 = sqrt(MU/(A()*A()*A()));

    // Corrected mean motion
    double n = n_0 + dn;

    // Mean anomaly
    double M_k = M_0 + n*t_k;

    // Solve Kepler's Equation for Eccentric Anomaly
    double E_k = M_k;
    int i;
    for(i=0; i<10000; i++) {
        double temp = E_k;
        E_k = M_k + e*sin(E_k);
        if (fabs(E_k - temp) < 1e-10) break;
    }
    if (i == 10000) printf("EPHEM::EccentricAnomaly didn't converge?\n");

    return E_k;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void EPHEM::GetXYZ(double *x, double *y, double *z, double t) const { // Get satellite position at time t

     // Time from ephemeris reference epoch
    double t_k = TimeFromEpoch(t, t_oe);

    // Eccentric Anomaly
    double E_k = EccentricAnomaly(t_k);

    // True Anomaly
    double v_k = atan2(
        sqrt(1-e*e) * sin(E_k),
        cos(E_k) - e);

    // Argument of Latitude
    double AOL = v_k + omega;

    // Second Harmonic Perturbations
    double du_k = C_us*sin(2*AOL) + C_uc*cos(2*AOL);    // Argument of Latitude Correction
    double dr_k = C_rs*sin(2*AOL) + C_rc*cos(2*AOL);    // Radius Correction
    double di_k = C_is*sin(2*AOL) + C_ic*cos(2*AOL);    // Inclination Correction

    // Corrected Argument of Latitude; Radius & Inclination
    double u_k = AOL + du_k;
    double r_k = A()*(1-e*cos(E_k)) + dr_k;
    double i_k = i_0 + di_k + IDOT*t_k;

    // Positions in orbital plane
    double x_kp = r_k*cos(u_k);
    double y_kp = r_k*sin(u_k);

    // Corrected longitude of ascending node
    double OMEGA_k = OMEGA_0 + (OMEGA_dot-OMEGA_E)*t_k - OMEGA_E*t_oe;

    // Earth-fixed coordinates
    *x = x_kp*cos(OMEGA_k) - y_kp*cos(i_k)*sin(OMEGA_k);
    *y = x_kp*sin(OMEGA_k) + y_kp*cos(i_k)*cos(OMEGA_k);
    *z = y_kp*sin(i_k);
}

///////////////////////////////////////////////////////////////////////////////////////////////

double EPHEM::GetClockCorrection(double t) const {

     // Time from ephemeris reference epoch
    double t_k = TimeFromEpoch(t, t_oe);

    // Eccentric Anomaly
    double E_k = EccentricAnomaly(t_k);

    // Relativistic correction
    double t_R = F*e*sqrtA*sin(E_k);

    // Time from clock correction epoch
    t = TimeFromEpoch(t, t_oc);

    // 20.3.3.3.3.1 User Algorithm for sat Clock Correction
    return a_f[0]
         + a_f[1] * pow(t, 1)
         + a_f[2] * pow(t, 2) + t_R - t_gd;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void EPHEM::Init(int sat) {
    this->sat = sat;
    isE1B = is_E1B(sat);
}

///////////////////////////////////////////////////////////////////////////////////////////////

bool EPHEM::Valid() {
    return
        isE1B?
            (IODN[0] != 0 && IODN[0] == IODN[1] && IODN[0] == IODN[2] && IODN[0] == IODN[3])
        :
            (IODC!=0 && IODC==IODE2 && IODC==IODE3);
}

///////////////////////////////////////////////////////////////////////////////////////////////

void EPHEM::Subframe(char *buf) { // called from channel tasks
    char nav[30];

	sub = tow_pg = bin(buf+49,3);

    for (int i=0; i<30; buf+=6) {	// skip 6 parity bits
        for (int j=0; j<3; j++) {
			nav[i++] = bin(buf,8);
			buf += 8;
        }
    }

    tow = PACK(nav[3], nav[4], nav[5]).u(17) * 6;
    
    tow_time = timer_ms();      //jks2
    //printf("%s SET  TOW %d sf%d\n", PRN(sat), tow/6, sub);

    switch (sub) {
        case 1: Subframe1(nav); break;
        case 2: Subframe2(nav); break;
        case 3: Subframe3(nav); break;
        case 4: Subframe4(nav); break;
//      case 5: Subframe5(nav); break;
    }
}

// FIXME_E1B: Consider recasting GNSS-SDRLIB/sdrnav_gal.cpp code to reside here using PACK() etc.

void EPHEM::PageN(unsigned page) {
    sub = page;
}

//#define E1B_IODN_DEBUG
#ifdef E1B_IODN_DEBUG
    #define EDC(p,s,o,i) \
        if (this->o != i) printf("ECHK pg%d %s %s %g = %g\n", p, PRN(sat), s, this->o, i);
    
    #define EUC(p,s,o,i) \
        if (this->o != i) printf("ECHK pg%d %s %s %d = %d\n", p, PRN(sat), s, this->o, i);
#else
    #define EDC(p,s,o,i)
    #define EUC(p,s,o,i)
#endif

void EPHEM::Page1(unsigned IODC, double M0, double e, double sqrtA, unsigned toe) {
        //printf("%s W1 IOD %d\n", PRN(nav->sat), keph->IODN[0]);
        EUC(1, "IODN", IODN[0], IODC);
    IODN[0] = IODC;
        EDC(1, "M0", M_0, M0);
    this->M_0 = M0;
        EDC(1, "e", e, e);
    this->e = e;
        EDC(1, "sqrtA", sqrtA, sqrtA);
    this->sqrtA = sqrtA;
        EUC(1, "toe", t_oe, toe);
    if (toe != 0) t_oe = toe;
}

void EPHEM::Page2(unsigned IODC, double OMG0, double i0, double omg, double idot) {
        //printf("%s W2 IOD %d\n", PRN(nav->sat), keph->IODN[1]);
        EUC(2, "IODN", IODN[1], IODC);
    IODN[1] = IODC;
        EDC(2, "OMG0", OMEGA_0, OMG0);
    OMEGA_0 = OMG0;
        EDC(2, "i0", i_0, i0);
    i_0 = i0;
        EDC(2, "omg", omega, omg);
    omega = omg;
        EDC(2, "idot", IDOT, idot);
    IDOT = idot;
}

void EPHEM::Page3(unsigned IODC, double OMGd, double deln, double cuc, double cus, double crc, double crs) {
        //printf("%s W3 IOD %d\n", PRN(nav->sat), keph->IODN[2]);
        EUC(3, "IODC", IODN[2], IODC);
    IODN[2] = IODC;
        EDC(3, "OMG0", OMEGA_dot, OMGd);
    OMEGA_dot = OMGd;
        EDC(3, "deln", dn, deln);
    dn = deln;
        EDC(3, "cuc", C_uc, cuc);
    C_uc = cuc;
        EDC(3, "cus", C_us, cus);
    C_us = cus;
        EDC(3, "crc", C_rc, crc);
    C_rc = crc;
        EDC(3, "crs", C_rs, crs);
    C_rs = crs;
}

void EPHEM::Page4(unsigned IODC, double cic, double cis, double f0, double f1, double f2, unsigned toc) {
        //printf("%s W4 IOD %d\n", PRN(nav->sat), keph->IODN[3]);
        EUC(4, "IODN", IODN[3], IODC);
    IODN[3] = IODC;
        EDC(4, "cic", C_ic, cic);
    C_ic = cic;
        EDC(4, "cis", C_is, cis);
    C_is = cis;
        EDC(4, "f0", a_f[0], f0);
    // NB: the #bits and scaling are different from Navstar for these three
    // but it shouldn't matter since these are just coefficients applied to
    // the same polynomial
    a_f[0] = f0;
        EDC(4, "f1", a_f[1], f1);
    a_f[1] = f1;
        EDC(4, "f2", a_f[2], f2);
    a_f[2] = f2;
        EUC(4, "toc", t_oc, toc);
    if (toc != 0) t_oc = toc;
}

void EPHEM::Page5(unsigned tow, unsigned week, double tgd, double toc, double toe) {
    if (0) printf("%s W5 E1B-TOW %d\n", PRN(sat), tow);
    tow_pg = 5;
    tow_time = timer_ms();      //jks2

    this->tow = tow;
    this->week = week;
        EDC(5, "tgd", t_gd, tgd);
    t_gd = tgd;
        EUC(5, "toc", t_oc, toc);
    if (toc != 0) t_oc = toc;
        EUC(5, "toe", t_oe, toe);
    if (toe != 0) t_oe = toe;
}

void EPHEM::Page6(unsigned tow, unsigned week) {
    if (0) printf("%s W6 E1B-TOW %d\n", PRN(sat), tow);
    tow_pg = 6;
    tow_time = timer_ms();      //jks2

    this->tow = tow;
    this->week = week;
}

void EPHEM::Page0(unsigned tow, unsigned week) {
    if (0) printf("%s W0 E1B-TOW %d\n", PRN(sat), tow);
    tow_pg = 0;
    tow_time = timer_ms();      //jks2

    this->tow = tow;
    this->week = week;
}
