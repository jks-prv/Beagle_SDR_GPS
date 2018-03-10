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

#include <memory.h>
#include <stdio.h>
#include <math.h>

#include "PosSolver.h"

#include "types.h"
#include "gps.h"
#include "clk.h"
#include "ephemeris.h"
#include "spi.h"

#define MAX_ITER 20

#define UERE (6.0)

///////////////////////////////////////////////////////////////////////////////////////////////

struct SNAPSHOT {
    EPHEM eph;
    float power;
    int ch, sv, ms, bits, g1, ca_phase;
    bool LoadAtomic(int ch, uint16_t *up, uint16_t *dn);
    double GetClock() const;
};

static SNAPSHOT Replicas[GPS_CHANS];
static u64_t ticks;

///////////////////////////////////////////////////////////////////////////////////////////////
// Gather channel data and consistent ephemerides

bool SNAPSHOT::LoadAtomic(int ch_, uint16_t *up, uint16_t *dn) {

    /* Called inside atomic section - yielding not allowed */

    if (ChanSnapshot(
        ch_,     // in: channel id
        up[1],  // in: FPGA circular buffer pointer
        &sv,    // out: satellite id
        &bits,  // out: total bits held locally (CHANNEL struct) + remotely (FPGA)
        &power) // out: received signal strength ^ 2
    && Ephemeris[sv].Valid()) {

        ms = up[0];
        g1 = dn[0] & 0x3FF;
        ca_phase = dn[0] >> 10;

        memcpy(&eph, Ephemeris+sv, sizeof eph);
        return true;
    }
    else
        return false; // channel not ready
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int LoadAtomic() {

	// i.e. { ticks[47:0], srq[GPS_CHANS-1:0], { GPS_CHANS { clock_replica } } }
	// clock_replica = { ch_NAV_MS[15:0], ch_NAV_BITS[15:0], ca_phase_code[15:0] }
	// NB: ca_phase_code is in reverse GPS_CHANS order, hence use of "up" and "dn" logic below

    const int WPT=3;	// words per ticks field
    const int WPS=1;	// words per SRQ field
    const int WPC=3;	// words per clock replica field

    SPI_MISO clocks;
    int chans=0;

    // Yielding to other tasks not allowed after spi_get_noduplex returns
	spi_get_noduplex(CmdGetClocks, &clocks, S2B(WPT) + S2B(WPS) + S2B(GPS_CHANS*WPC));

    uint16_t srq = clocks.word[WPT+0];              // un-serviced epochs
    uint16_t *up = clocks.word+WPT+1;               // Embedded CPU memory containing ch_NAV_MS and ch_NAV_BITS
    uint16_t *dn = clocks.word+WPT+WPC*GPS_CHANS;   // FPGA clocks (in reverse order)

    // NB: see tools/ext64.c for why the (u64_t) casting is very important
    ticks = ((u64_t) clocks.word[0]<<32) | ((u64_t) clocks.word[1]<<16) | clocks.word[2];

    for (int ch=0; ch<gps_chans; ch++, srq>>=1, up+=WPC, dn-=WPC) {

        up[0] += (srq&1); // add 1ms for un-serviced epochs

        if (Replicas[chans].LoadAtomic(ch,up,dn))
            Replicas[chans++].ch = ch;
    }

    // Safe to yield again ...
    return chans;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int LoadReplicas() {
    const int GLITCH_GUARD=500;
    SPI_MISO glitches[2];

    // Get glitch counters "before"
    spi_get(CmdGetGlitches, glitches+0, GPS_CHANS*2);
    TaskSleepMsec(GLITCH_GUARD);

    // Gather consistent snapshot of all channels
    int pass1 = LoadAtomic();
    int pass2 = 0;

    // Get glitch counters "after"
    TaskSleepMsec(GLITCH_GUARD);
    spi_get(CmdGetGlitches, glitches+1, GPS_CHANS*2);

    // Strip noisy channels
    for (int i=0; i<pass1; i++) {
        int ch = Replicas[i].ch;
        if (glitches[0].word[ch] != glitches[1].word[ch]) continue;
        if (i>pass2) memcpy(Replicas+pass2, Replicas+i, sizeof(SNAPSHOT));
        pass2++;
    }

    return pass2;
}

///////////////////////////////////////////////////////////////////////////////////////////////

double SNAPSHOT::GetClock() const {

    // Find 10-bit shift register in 1023 state sequence
    int chips = SearchCode(sv, g1);
    if (chips == -1) return NAN;

    // TOW refers to leading edge of next (un-processed) subframe.
    // Channel.cpp processes NAV data up to the subframe boundary.
    // Un-processed bits remain in holding buffers.
    // 15 nsec resolution due to inclusion of ca_phase.

    return // Un-corrected satellite clock
                                        //                                      min    max         step (secs)
        eph.tow * 6 +                   // Time of week in seconds (0...100799) 0      604794      6.000
        bits / BPS  +                   // NAV data bits buffered (1...300)     0.020  6.000       0.020 (50 Hz)
        ms * 1e-3   +                   // Milliseconds since last bit (0...20) 0.000  0.020       0.001
        chips / CPS +                   // Code chips (0...1022)                0.000  0.000999    0.000000999 (1 usec)
        ca_phase * pow(2, -6) / CPS;    // Code NCO phase (0...63)              0.000  0.00000096  0.000000015 (15 nsec)
}


// GNSSDataForEpoch holds all data for a position solution in a given epoch:
//  * satellite (X,Y,Z)             ...     sv(:,0:2)  [m]
//  * clock corrected time t*c      ...     sv(:,3)    [m]
//  * weight=signal power           ... weight(:)
//  * 48 bit ADC clock tick counter ... ticks          [ADC clock ticks]
class GNSSDataForEpoch {
public:
    typedef PosSolver::vec_type vec_type;
    typedef PosSolver::mat_type mat_type;
    typedef TNT::Array1D<int>  ivec_type;

    GNSSDataForEpoch(int max_channels)
        : _chans(0)
        , _sv(4, max_channels)
        , _weight(max_channels)
        , _sv_num(max_channels)
        , _prn(max_channels)
        , _adc_ticks(0ULL) {}

    int       chans() const { return _chans; }
    mat_type     sv() const { return _chans ? _sv.subarray(0,3,0,_chans-1).copy() : PosSolver::mat_type(); }
    vec_type weight() const { return _chans ? _weight.subarray(0,_chans-1).copy() : PosSolver::vec_type(); }
    u64_t adc_ticks() const { return _adc_ticks; }
    int    prn(int i) const { return _prn[i]; }
    int sv_num(int i) const { return _sv_num[i]; }

    bool LoadFromReplicas(int chans, const SNAPSHOT* replicas, u64_t adc_ticks) {
        _adc_ticks = adc_ticks;
        _chans  = 0;
        _weight = 0;
        _sv     = 0;
        _sv_num = 0;
        _prn    = 0;
        for (int i=0; i<chans; ++i) {
            NextTask("solve1");

            // power of received signal
            _weight[i] = replicas[i].power;

            // un-corrected time of transmission
            double t_tx = replicas[i].GetClock();
            if (t_tx == NAN) return false;

            // apply clock correction
            t_tx -= replicas[i].eph.GetClockCorrection(t_tx);
            _sv[3][i] = C*t_tx; // [s] -> [m]

            // get SV position in ECEF coords
            replicas[i].eph.GetXYZ(&_sv[0][i],
                                   &_sv[1][i],
                                   &_sv[2][i],
                                   t_tx);

            _sv_num[i] = Replicas[i].sv;
            _prn[i]    = Sats_L1[_sv_num[i]].prn;

            _chans += 1;
        }
        return (_chans > 0);
    }

private:
    int       _chans;     // number of good channels
    mat_type  _sv;        // sat. x,y,z,ct [m,m,m,m]
    vec_type  _weight;    // weights = sat. signal power/mean(sat. signal power)
    ivec_type _sv_num;    // svn number
    ivec_type _prn;       // prn
    u64_t     _adc_ticks; // ADC clock ticks
} ;

///////////////////////////////////////////////////////////////////////////////////////////////

void update_gps_info_before() {
    time_t t; time(&t);
    struct tm tm; gmtime_r(&t, &tm);
    //int samp = (tm.tm_hour & 3)*60 + tm.tm_min;
    const int samp = tm.tm_min;

    if (gps.last_samp != samp) {
        gps.last_samp = samp;
        for (int sv = 0; sv < NUM_L1_SATS; sv++) {
            gps.az[gps.last_samp][sv] = 0;
            gps.el[gps.last_samp][sv] = 0;
        }
    }
}
void update_gps_info_after(const PosSolver::sptr&  posSolver,
                           const GNSSDataForEpoch& gnssDataForEpoch) {
    assert(posSolver);

    const std::vector<PosSolver::ElevAzim> elaz = posSolver->elev_azim(gnssDataForEpoch.sv());

    for (int i=0, n=elaz.size(); i<n; ++i) {
        if (gps.el[gps.last_samp][gnssDataForEpoch.sv_num(i)])
            continue;

        printf("el,az= %12.6f %12.6f\n", elaz[i].elev_deg, elaz[i].azim_deg);
        const int az = std::round(elaz[i].azim_deg);
        const int el = std::round(elaz[i].elev_deg);

        if (az < 0 || az >= 360 || el <= 0 || el > 90)
            continue;

        gps.az[gps.last_samp][gnssDataForEpoch.sv_num(i)] = az;
        gps.el[gps.last_samp][gnssDataForEpoch.sv_num(i)] = el;
        
        gps.shadow_map[az] |= (1 << int(std::round(el/90.0*31.0)));

        // add az/el to channel data
        for (int ch = 0; ch < GPS_CHANS; ch++) {
            gps_stats_t::gps_chan_t *chp = &gps.ch[ch];
            if (SAT_L1(chp->sat) == gnssDataForEpoch.prn(i)) {
                chp->az = az;
                chp->el = el;
            }
        }
        if (gnssDataForEpoch.prn(i) == 195) { // QZS-3
            gps.qzs_3.az = az;
            gps.qzs_3.el = el;
            printf("QZS-3 az=%d el=%d\n", az, el);
        }
    }

    if (posSolver->state() & PosSolver::state::HAS_TIME) {
        gps.fixes++;
        GPSstat(STAT_TIME, posSolver->t_rx());
        clock_correction(posSolver->t_rx(), gnssDataForEpoch.adc_ticks());
    }

    if (posSolver->state() & PosSolver::state::HAS_POS) {
        GPSstat(STAT_LAT, posSolver->llh().lat());
        GPSstat(STAT_LON, posSolver->llh().lon());
        GPSstat(STAT_ALT, posSolver->llh().alt());
    }
}

void SolveTask(void *param) {
    GNSSDataForEpoch gnssDataForEpoch(GPS_CHANS);

    PosSolver::mat_type ekfCov(5,5, 0.0);
    ekfCov(0,0) = std::pow(0.001, 2);
    ekfCov(1,1) = std::pow(0.001, 2);
    ekfCov(2,2) = std::pow(0.001, 2);
    ekfCov(3,3)          = std::pow(1e-9*C, 2);
    ekfCov(3,4) = ekfCov(4,3) = 1e-9*C*1;
    ekfCov(4,4)          = std::pow(1.0, 2);

    PosSolver::sptr posSolver = PosSolver::make(ekfCov, UERE, ADC_CLOCK_TYP);

    double lat=0, lon=0, alt;
    int good = -1;

    for (;;) {

        // while we're waiting send IQ values if requested
        u4_t now = timer_ms();
            int ch = gps.IQ_data_ch - 1;
            if (ch != -1) {
                spi_set(CmdIQLogReset, ch);
                //printf("SOLVE CmdIQLogReset ch=%d\n", ch);
                TaskSleepMsec(1024 + 100);
                static SPI_MISO rx;
                spi_get(CmdIQLogGet, &rx, S2B(GPS_IQ_SAMPS_W));
                memcpy(gps.IQ_data, rx.word, S2B(GPS_IQ_SAMPS_W));
                // printf("gps.IQ_data %d rx.word %d S2B(GPS_IQ_SAMPS_W) %d\n", sizeof(gps.IQ_data), sizeof(rx.word), S2B(GPS_IQ_SAMPS_W));
                gps.IQ_seq_w++;

                #if 0
                    int i;
                    #if 0
                        printf("I ");
                        for (i=0; i < 16; i++)
                            printf("%6d ", (s2_t) rx.word[i*2]);
                        printf("\n");
                        printf("Q ");
                        for (i=0; i < 16; i++)
                            printf("%6d ", (s2_t) rx.word[i*2+1]);
                        printf("\n");
                    #else
                        printf("I ");
                        for (i=0; i < GPS_IQ_SAMPS; i++) {
                            printf("%d|%d ", i, (s2_t) rx.word[i*2]);
                            if ((i % 8) == 7)
                                printf("\n");
                        }
                        printf("\n");
                    #endif
                #endif
            }
        u4_t elapsed = timer_ms() - now;
        u4_t remaining = SEC_TO_MSEC(4) - elapsed;
        if (elapsed < SEC_TO_MSEC(4)) {
            //printf("ch=%d%s remaining=%d\n", ch+1, (ch+1 == 0)? "(off)":"", remaining);
		    TaskSleepMsec(remaining);
		}

        good = LoadReplicas();

        update_gps_info_before();

        gps.good = good;
        const bool enable = SearchTaskRun();
        if (!enable || good == 0) continue;

        if (gnssDataForEpoch.LoadFromReplicas(good, Replicas, ticks)) {
            posSolver->solve(gnssDataForEpoch.sv(),
                             gnssDataForEpoch.weight(),
                             gnssDataForEpoch.adc_ticks());
            printf("POS(%d): %13.3f %13.3f %13.3f %.9f %.9f %10.6f %10.6f %4.0f\n",
                   posSolver->state(),
                   posSolver->pos()[0],
                   posSolver->pos()[1],
                   posSolver->pos()[2],
                   posSolver->t_rx(),
                   posSolver->osc_corr(),
                   posSolver->llh().lat(),
                   posSolver->llh().lon(),
                   posSolver->llh().alt());
        }

        TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, 0, 0);
        update_gps_info_after(posSolver, gnssDataForEpoch);
    }
}
