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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "gps.h"
#include "spi.h"
#include "ephemeris.h"

const int PWR_LEN = 8;
const int MAX_BITS = 64;

struct UPLOAD { // Embedded CPU CHANNEL structure
    uint16_t nav_ms;                // Milliseconds 0 ... 19
    uint16_t nav_bits;              // Bit count
    uint16_t nav_glitch;            // Glitch count
    uint16_t nav_prev;              // Last data bit
    uint16_t nav_buf[MAX_BITS/16];  // NAV data buffer
    uint16_t ca_freq[4];            // Loop filter integrator
    uint16_t lo_freq[4];            // Loop filter integrator
     int16_t iq[2];                 // Last I, Q samples
    uint16_t ca_gain[2];            // Code loop ki, kp
    uint16_t lo_gain[2];            // Carrier loop ki, kp
    uint16_t ca_unlocked;
    //uint16_t pp[2], pe[2], pl[2];
};

struct CHANNEL { // Locally-held channel data
    UPLOAD ul;                      // Copy of embedded CPU channel state
    float pwr_tot, pwr[PWR_LEN];    // Running average of signal power
    int pwr_pos;                    // Circular counter
    int gain_adj;                   // AGC
    int ch, sv;                     // Association
    int probation;                  // Temporarily disables use if channel noisy
    int holding, rd_pos;            // NAV data bit counters

    void  Reset();
    void  Start(int sv, int t_sample, int taps, int lo_shift, int ca_shift);
    void  SetGainAdj(int);
    int   GetGainAdj();
    void  CheckPower();
    float GetPower();
    void  Service();
    void  Acquisition();
    void  Tracking();
    void  SignalLost();
    void  UploadEmbeddedState();
    int   ParityCheck(char *buf, int *nbits);
    void  Subframe(char *buf);
    void  Status();
    int   RemoteBits(uint16_t wr_pos);
    bool  GetSnapshot(uint16_t wr_pos, int *p_sv, int *p_bits, float *p_pwr);
};

static CHANNEL Chans[GPS_CHANS];

static unsigned BusyFlags;

///////////////////////////////////////////////////////////////////////////////////////////////

static double GetFreq(uint16_t *u) { // Convert NCO command to Hertz
	return ( u[0] * pow(2, -64)
		   + u[1] * pow(2, -48)
		   + u[2] * pow(2, -32)
		   + u[3] * pow(2, -16)) * FS;
}

///////////////////////////////////////////////////////////////////////////////////////////////

const char preambleUpright [] = {1,0,0,0,1,0,1,1};
const char preambleInverse [] = {0,1,1,1,0,1,0,0};

static int parity(char *p, char *word, char D29, char D30) {
    char *d = word-1;
    for (int i=1; i<25; i++) d[i] ^= D30;
    p[0] = D29 ^ d[1] ^ d[2] ^ d[3] ^ d[5] ^ d[6] ^ d[10] ^ d[11] ^ d[12] ^ d[13] ^ d[14] ^ d[17] ^ d[18] ^ d[20] ^ d[23];
    p[1] = D30 ^ d[2] ^ d[3] ^ d[4] ^ d[6] ^ d[7] ^ d[11] ^ d[12] ^ d[13] ^ d[14] ^ d[15] ^ d[18] ^ d[19] ^ d[21] ^ d[24];
    p[2] = D29 ^ d[1] ^ d[3] ^ d[4] ^ d[5] ^ d[7] ^ d[8] ^ d[12] ^ d[13] ^ d[14] ^ d[15] ^ d[16] ^ d[19] ^ d[20] ^ d[22];
    p[3] = D30 ^ d[2] ^ d[4] ^ d[5] ^ d[6] ^ d[8] ^ d[9] ^ d[13] ^ d[14] ^ d[15] ^ d[16] ^ d[17] ^ d[20] ^ d[21] ^ d[23];
    p[4] = D30 ^ d[1] ^ d[3] ^ d[5] ^ d[6] ^ d[7] ^ d[9] ^ d[10] ^ d[14] ^ d[15] ^ d[16] ^ d[17] ^ d[18] ^ d[21] ^ d[22] ^ d[24];
    p[5] = D29 ^ d[3] ^ d[5] ^ d[6] ^ d[8] ^ d[9] ^ d[10] ^ d[11] ^ d[13] ^ d[15] ^ d[19] ^ d[22] ^ d[23] ^ d[24];
    return memcmp(d+25, p, 6);
}

///////////////////////////////////////////////////////////////////////////////////////////////

void CHANNEL::UploadEmbeddedState() {
    SPI_MISO miso;
    spi_get(CmdGetChan, &miso, sizeof(ul), ch);
    memcpy(&ul, miso.byte, sizeof(ul));
}

///////////////////////////////////////////////////////////////////////////////////////////////

int CHANNEL::GetGainAdj() {
    return gain_adj;
}

void CHANNEL::SetGainAdj(int adj) {
    gain_adj = adj;

    int lo_ki = 20 + gain_adj;
    int lo_kp = 27 + gain_adj;

    spi_set(CmdSetGainLO, ch, lo_ki + ((lo_kp-lo_ki)<<16));
}

///////////////////////////////////////////////////////////////////////////////////////////////

void CHANNEL::Reset() {
    uint32_t ca_rate = CPS/FS*powf(2,32);
    spi_set(CmdSetRateCA, ch, ca_rate);

    int ca_ki = 20-9;	// 11
    int ca_kp = 27-4;	// 23, kp-ki = 12

    spi_set(CmdSetGainCA, ch, ca_ki + ((ca_kp-ca_ki)<<16));

    SetGainAdj(0);

    memset(pwr, 0, sizeof pwr);
    pwr_tot=0;
    pwr_pos=0;
    probation=2;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void CHANNEL::Start( // called on search thread to initiate acquisition
    int sv_,
    int t_sample,
    int taps,
    int lo_shift,
    int ca_shift) {

    this->sv = sv_;

    // Estimate Doppler from FFT bin shift
    double lo_dop = lo_shift*BIN_SIZE;
    double ca_dop = (lo_dop/L1)*CPS;

    // NCO rates
    uint32_t lo_rate = (FC  + lo_dop)/FS*pow(2,32);
    uint32_t ca_rate = (CPS + ca_dop)/FS*pow(2,32);

    // Initialise code and carrier NCOs
    spi_set(CmdSetRateLO, ch, lo_rate);
    spi_set(CmdSetRateCA, ch, ca_rate);

    // Seconds elapsed since sample taken
    double secs = (Microseconds()-t_sample) / 1e6;

    // Code creep due to code rate Doppler
    int code_creep = nearbyint((ca_dop*secs/CPS)*FS);

    // Align code generator by pausing NCO
    uint32_t ca_pause = ((FS_I*2/1000)-(ca_shift+code_creep)) % (FS_I/1000);

	if (ca_pause) spi_set(CmdPause, ch, ca_pause-1);
    spi_set(CmdSetSV, ch, taps); // Gold Code taps

    // Wait 3 epochs to be sure phase errors are valid before ...
    TimerWait(3);
    // ... enabling embedded PI controllers
    spi_set(CmdSetMask, BusyFlags |= 1<<ch);

	UserStat(STAT_DEBUG, secs, ch, ca_shift, code_creep, ca_pause);

#ifndef QUIET
    printf("Channel %d SV-%d PRN-%d lo_dop %f lo_rate 0x%x ca_dop %f ca_rate 0x%x pause %d\n\n",
    	ch, sv, sv+1, lo_dop, lo_rate, ca_dop, ca_rate, ca_pause-1);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////

void CHANNEL::Service() {

    /* entered on channel thread after search thread has called Start() */

    Acquisition();
    Tracking();
    SignalLost();
}

///////////////////////////////////////////////////////////////////////////////////////////////

void CHANNEL::Acquisition() {

    // Code loop always locks, but carrier loop sometimes needs help!

    // Carrier might fall outside Costas loop capture range because error in
    // initial Doppler estimate (FFT bin size) is larger than loop bandwidth.

    // Give them 5 seconds ...
	TimerWait(5000);

    // Get accurate Doppler measurement from locked code NCO
    UploadEmbeddedState();

	double ca_freq = GetFreq(ul.ca_freq);
	double ca_dop = ca_freq - CPS;

    // Put carrier NCO precisely on-frequency.  Now it will lock.
	double ca2lo_dop = ca_dop*L1/CPS;
	double f_lo_rate = (FC + ca2lo_dop)/FS;
	uint32_t lo_rate = f_lo_rate*pow(2,32);

#ifndef QUIET
	printf("Channel %d SV-%d PRN-%d new lo_rate 0x%x\n", ch, sv, sv+1, lo_rate);
#endif

    spi_set(CmdSetRateLO, ch, lo_rate);
}

///////////////////////////////////////////////////////////////////////////////////////////////

void CHANNEL::Tracking() {
    char buf[300+MAX_BITS-1];

    const int POLLING=250;  // Poll 4 times per second

    const int TIMEOUT=60*4;   // Bail after 60 seconds on LOS

	const int LOW_BITS_TIMEOUT=30*4;
	const int LOW_BITS=96;

	const int LOW_RSSI_TIMEOUT=15*4;
    const float LOW_RSSI = 400*400;

	float pwr, maxpwr=0;

    holding=0;

    for (int watchdog=0; watchdog<TIMEOUT; watchdog++) {
        TimerWait(POLLING);
        UploadEmbeddedState();

        // Process NAV data
        for(int avail = RemoteBits(ul.nav_bits) & ~0xF; avail; avail-=16) {
            int word = ul.nav_buf[rd_pos/16];
            for (int i=0; i<16; i++) {
                word<<=1;
                buf[holding++] = (word>>16) & 1;
            }
            rd_pos+=16;
            rd_pos&=MAX_BITS-1;
        }

        while (holding>=300) { // Enough for a subframe?
            int nbits;
            if (0==ParityCheck(buf, &nbits)) watchdog=0;
            memmove(buf, buf+nbits, holding-=nbits);
        }

		Status();
#ifndef QUIET
		printf(" wdog %d\n", watchdog);
#endif
    	UserStat(STAT_WDOG, 0, ch, watchdog/4, holding, ul.ca_unlocked);
    	//UserStat(STAT_WDOG, 0, ch, watchdog/4, holding, 0);
    	//UserStat(STAT_EPL, 0, ch, (ul.pe[1]<<16) | ul.pe[0], (ul.pp[1]<<16) | ul.pp[0], (ul.pl[1]<<16) | ul.pl[0]);
        CheckPower();
        
        pwr = GetPower();
        if (pwr > maxpwr) maxpwr=pwr;
        if ((watchdog >= LOW_RSSI_TIMEOUT) && (maxpwr <= LOW_RSSI))
        	break;
        if ((watchdog >= LOW_BITS_TIMEOUT) && (holding <= LOW_BITS))
        	break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

void CHANNEL::SignalLost() {
    // Disable embedded PI controllers
    BusyFlags &= ~(1<<ch);
    spi_set(CmdSetMask, BusyFlags);

    // Re-enable search for this SV
    SearchEnable(sv);

    UserStat(STAT_POWER, 0, ch); //Flatten bar graph
}

///////////////////////////////////////////////////////////////////////////////////////////////

int CHANNEL::RemoteBits(uint16_t wr_pos) { // NAV bits held in FPGA circular buffer
    return (MAX_BITS-1) & (wr_pos-rd_pos);
}

///////////////////////////////////////////////////////////////////////////////////////////////

void CHANNEL::CheckPower() {
    // Running average of received signal power
    pwr_tot -= pwr[pwr_pos];
    pwr_tot += pwr[pwr_pos++] = powf(ul.iq[0],2) + powf(ul.iq[1],2);
    pwr_pos %= PWR_LEN;

    float mean = GetPower();

    // Carrier loop gain proportional to signal power (k^2).
    // Loop unstable if gain not reduced for strong signals

    // AGC hysteresis thresholds
    const float HYST_LO = 1200*1200;
    const float HYST_HI = 1400*1400;

    if (GetGainAdj()) {
        if (mean<HYST_LO) SetGainAdj(0); // default
    }
    else {
        if (mean>HYST_HI) SetGainAdj(-1); // half loop gain
    }

    UserStat(STAT_POWER, mean, ch, GetGainAdj());
}

float CHANNEL::GetPower() {
    return pwr_tot / PWR_LEN;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Debug

static unsigned bin(char *s, int n) {
	unsigned u=*s;
	while(--n) u+=u+*++s;
	return u;
}

void CHANNEL::Subframe(char *buf) {
    char *sub = buf+49;
    char *tow = buf+30;
    int id;

#ifndef	QUIET
    printf("sub %d tow %d  ", id=bin(sub,3), bin(tow,17));

    if (id>3) {
        int j, pg;
        for (pg=0, j=0; j<6; j++) pg += pg + buf[62+j];
        printf("pg %2d", pg);
    }

    putchar('\n');
#endif

	UserStat(STAT_SUB, 0, ch, bin(sub,3));
}

void CHANNEL::Status() {
    double rssi = sqrt(GetPower());
    double lo_f = GetFreq(ul.lo_freq) - FC;
    double ca_f = GetFreq(ul.ca_freq) - CPS;

	UserStat(STAT_LO, lo_f, ch);
	UserStat(STAT_CA, ca_f, ch);
#ifndef QUIET
    printf("chan %d PRN %2d rssi %4.0f adj %2d freq %5.6f %6.6f ", ch, sv+1, rssi, gain_adj, lo_f, ca_f);
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////

int CHANNEL::ParityCheck(char *buf, int *nbits) {
    char p[6];

    // Upright or inverted preamble?  Setting of parity bits resolves phase ambiguity.
    if      (0==memcmp(buf, preambleUpright, 8)) p[4]=p[5]=0;
    else if (0==memcmp(buf, preambleInverse, 8)) p[4]=p[5]=1;
    else return *nbits=1;

    // Parity check up to ten 30-bit words ...
    for (int i=0; i<300; i+=30) {
        if (parity(p, buf+i, p[4], p[5])) {
            Status();
#ifndef QUIET
            puts("parity");
#endif
			UserStat(STAT_SUB, 0, ch, 6);
            probation=2;
            return *nbits=i+30;
        }
    }

    Status();
    Subframe(buf);
    Ephemeris[sv].Subframe(buf);
    if (probation) probation--;
    *nbits=300;
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////

bool CHANNEL::GetSnapshot(
    uint16_t wr_pos,    // in: circular nav_buf pointer
    int *p_sv,          // out: Ephemeris[] index
    int *p_bits,        // out: total NAV bits held locally + remotely
    float *p_pwr) {     // out: signal power for least-squares weighting

    if (probation) return false; // temporarily too noisy

    *p_sv   = sv;
    *p_bits = holding + RemoteBits(wr_pos);
    *p_pwr  = GetPower();

    return true; // ok to use
}

bool ChanSnapshot( // called on solver thread
    int ch, uint16_t wr_pos, int *p_sv, int *p_bits, float *p_pwr) {

    if (BusyFlags&(1<<ch))
        return Chans[ch].GetSnapshot(wr_pos, p_sv, p_bits, p_pwr);
    else
        return false; // channel not enabled
}

///////////////////////////////////////////////////////////////////////////////////////////////

void ChanTask() { // one thread per channel
    static int inst;
    int ch = inst++; // which channel am I?
    Chans[ch].ch=ch;
    unsigned bit = 1<<ch;
    for (;;) {
        if (BusyFlags & bit) Chans[ch].Service(); // returns after loss of signal
        NextTask();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

int ChanReset() { // called on search thread before sampling
    for (int ch=0; ch<gps_chans; ch++) {
        if (BusyFlags&(1<<ch)) continue;
        Chans[ch].Reset();
        return ch;
    }

    return -1; // all channels busy
}

///////////////////////////////////////////////////////////////////////////////////////////////

void ChanStart( // called on search thread to initiate acquisition of detected SV
    int ch,
    int sv,
    int t_sample,
    int taps,
    int lo_shift,
    int ca_shift) {

    Chans[ch].Start(sv, t_sample, taps, lo_shift, ca_shift);
}
