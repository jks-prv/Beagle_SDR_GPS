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

#include <array>
#include <memory.h>
#include <stdio.h>
#include <math.h>

#include "types.h"
#include "gps.h"
#include "clk.h"
#include "ephemeris.h"
#include "spi.h"
#include "spi_dev.h"
#include "timer.h"
#include "PosSolver.h"

#include <time.h>

// user-equivalent range error (m)
#define UERE 6.0

///////////////////////////////////////////////////////////////////////////////////////////////

struct SNAPSHOT {
    EPHEM eph;
    float power;
    int ch, sat, srq, ms;
    mutable int bits;
    int bits_tow, chips, cg_phase;
    bool isE1B;
    mutable bool tow_delayed;
    bool LoadAtomic(int ch, uint16_t *up, uint16_t *dn, int srq);
    double GetClock() const;
};

static SNAPSHOT Replicas[GPS_MAX_CHANS];
static u64_t ticks;

///////////////////////////////////////////////////////////////////////////////////////////////
// Gather channel data and consistent ephemerides

bool SNAPSHOT::LoadAtomic(int ch_, uint16_t *up, uint16_t *dn, int srq_) {

    /* Called inside atomic section - yielding not allowed */

    if (ChanSnapshot(
        ch_,        // in: channel id
        up[1],      // in: FPGA circular buffer pointer
        &sat,       // out: satellite id
        &bits,      // out: total bits held locally (CHANNEL struct) + remotely (FPGA)
        &bits_tow,  // out: bits since last valid TOW
        &power)     // out: received signal strength ^ 2
    && Ephemeris[sat].Valid()) {

        isE1B = is_E1B(sat);
        srq = srq_;
        ms = up[0];
        if (srq) ms += isE1B? E1B_CODE_PERIOD : L1_CODE_PERIOD;     // add one code period for un-serviced epochs
        chips = ((dn[0] & 0x3) << 10) | (dn[-1] & 0x3FF);
        //if (isE1B) printf("%s %d 0x%x = [0x%x,0x%x]\n", PRN(sat), chips, chips, dn[0], dn[-1]&0x3ff);
        cg_phase = dn[-1] >> 10;

        memcpy(&eph, Ephemeris+sat, sizeof eph);
        if (0) printf("%s copy TOW %d(%d) %s%d|%d bits %d bits_tow %d %.3f\n",
            PRN(sat), eph.tow/6, eph.tow, isE1B? "pg":"sf", eph.sub, eph.tow_pg, bits, bits_tow, (float) bits_tow/50);  //jks2
        return true;
    }
    else {
        if (0 && is_E1B(sat))
        printf("%s copy TOW %d(%d) %s%d|%d bits %d bits_tow %d %.3f NOT READY\n",
            PRN(sat), Ephemeris[sat].tow/6, Ephemeris[sat].tow, isE1B? "pg":"sf", Ephemeris[sat].tow_pg, Ephemeris[sat].sub,
            bits, bits_tow, (float) bits_tow/50);  //jks2
        return false; // channel not ready
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int LoadAtomic() {

	// i.e. { ticks[47:0], srq[gps_chans-1:0], { gps_chans { clock_replica } } }
	// clock_replica = { ch_NAV_MS[15:0], ch_NAV_BITS[15:0], cg_phase_code[15:0] }
	// NB: cg_phase_code is in reverse gps_chans order, hence use of "up" and "dn" logic below

    const int WPT = 3;      // words per ticks field
    const int WPS = 1;      // words per SRQ field
    const int WPC = 2+2;    // words per clock replica field

    SPI_MISO *clocks = &SPI_SHMEM->gps_clocks_miso;
    int chans=0;

    // Yielding to other tasks not allowed after spi_get_noduplex returns.
    // Why? Because below we need to snapshot the ephemerides state that match the just loaded clock replicas.
	spi_get_noduplex(CmdGetClocks, clocks, S2B(WPT) + S2B(WPS) + S2B(gps_chans*WPC));

    uint16_t srq = clocks->word[WPT+0];              // un-serviced epochs
    uint16_t *up = clocks->word+WPT+WPS;             // Embedded CPU memory containing ch_NAV_MS and ch_NAV_BITS
    uint16_t *dn = clocks->word+WPT+WPC*gps_chans;   // FPGA clocks (in reverse order)

    // NB: see tools/ext64.c for why the (u64_t) casting is very important
    ticks = ((u64_t) clocks->word[0]<<32) | ((u64_t) clocks->word[1]<<16) | clocks->word[2];

    //int any = 0;
    for (int ch=0; ch < gps_chans; ch++, srq >>= 1, up += WPC, dn -= WPC) {
        
        if (Replicas[chans].LoadAtomic(ch,up,dn,srq&1)) {
            Replicas[chans].ch = ch;
            //real_printf("%s%s ", (Replicas[chans].isE1B && !gps.include_E1B)? "x-":"", PRN(Replicas[chans].sat)); any = 1;
            chans++;
        }
    }
    //if (any) real_printf("\n");

    // Safe to yield again ...
    return chans;
}

///////////////////////////////////////////////////////////////////////////////////////////////

static int LoadReplicas() {
    const int GLITCH_GUARD=500;
    SPI_MISO *glitches = SPI_SHMEM->gps_glitches_miso;

    // Get glitch counters "before"
    spi_get(CmdGetGlitches, glitches+0, gps_chans*2);
    TaskSleepMsec(GLITCH_GUARD);

    // Gather consistent snapshot of all channels
    int pass1 = LoadAtomic();
    int pass2 = 0;

    // Get glitch counters "after"
    TaskSleepMsec(GLITCH_GUARD);
    spi_get(CmdGetGlitches, glitches+1, gps_chans*2);

    // Strip noisy channels
    for (int i=0; i<pass1; i++) {
        int ch = Replicas[i].ch;
        if (glitches[0].word[ch] != glitches[1].word[ch]) continue;
        if (Replicas[i].isE1B && !gps.include_E1B) continue;
        if (i>pass2) memcpy(Replicas+pass2, Replicas+i, sizeof(SNAPSHOT));
        pass2++;
    }

    return pass2;
}

///////////////////////////////////////////////////////////////////////////////////////////////

double SNAPSHOT::GetClock() const {

    // TOW refers to leading edge of next (un-processed) subframe.
    // Channel.cpp processes NAV data up to the subframe boundary.
    // Un-processed bits remain in holding buffers.
    // 15 nsec resolution due to inclusion of cg_phase.

    bool bad = false;
    if (isE1B) {
        if (bits < 0 && bits > 500+MAX_NAV_BITS) {
            lprintf("GPS E1B BAD bits\n");
            //panic("E1B bits");
            bad = true;
        }
        
        //assert(ms == 0 || ms == E1B_CODE_PERIOD);   // because ms == E1B_CODE_PERIOD for un-serviced epochs (see code above)
        if (ms != 0 && ms != E1B_CODE_PERIOD) {
            lprintf("GPS E1B BAD ms E1B_CODE_PERIOD=%d\n", E1B_CODE_PERIOD);
            bad = true;
        }
        
        //assert(chips >= 0 && chips <= (E1B_CODELEN-1));
        if (chips < 0 || chips > (E1B_CODELEN-1)) {
            lprintf("GPS E1B BAD chips\n");
            bad = true;
        }
        
        //assert(cg_phase >= 0 && cg_phase <= 63);
        if (cg_phase < 0 && cg_phase > 63) {
            lprintf("GPS E1B BAD cg_phase\n");
            bad = true;
        }
    } else {
        if (ms < 0 && ms > (19+1)) {
            lprintf("GPS C/A BAD ms\n");
            bad = true;
        }
    }
    if (bad) {
        lprintf("GPS GetClock %s tow=%d bits=%d ms=%d chips=0x%x cg_phase=%d\n",
            PRN(sat), eph.tow, bits, ms, chips, cg_phase);
        return NAN;
    }
    
    //jks2
    if (0) {
        u4_t diff = timer_ms() - eph.tow_time;
        printf("%s clk  TOW %d(%d) %s%d|%d bits %d bits_tow %d diff %.3f %s\n",
            PRN(sat), eph.tow/6, eph.tow, isE1B? "pg":"sf", eph.sub, eph.tow_pg, bits, bits_tow, (float) diff/1e3,
            (bits != bits_tow)? "************************************************":"");
    }

    tow_delayed = false;
    #define MAX_TOW_DELAY   (5*500)
    if (bits != bits_tow && bits_tow < MAX_TOW_DELAY) {
        bits = bits_tow;
        tow_delayed = true;
    }

    // Un-corrected satellite clock
    double clock = isE1B?
                                        //                                      min    max         step (secs)
        (eph.tow +                      // Time of week in seconds (0...604794) 0      604794      2.000
        bits / E1B_BPS +                // NAV data bits buffered (0...500+)    0.000  2.000+      0.004 (250 Hz, ~1200km)
        ms * 1e-3   +                   // Un-serviced epoch adj (0 or 4)       0.000  0.004       0.004
        chips / CPS + 0.25/CPS +        // Code chips (0...4091)                0.000  0.003999    0.000000976 (~1 usec, ~300m)
        cg_phase * pow(2, -6) / CPS)    // Code NCO phase (0...63)              0.000  0.00000096  0.000000015 (15 nsec, ~4.5m)
    :
                                        //                                      min    max         step (secs)
        (eph.tow +                      // Time of week in seconds (0...604794) 0      604794      6.000
        bits / L1_BPS +                 // NAV data bits buffered (0...300+)    0.000  6.000+      0.020 (50 Hz)
        ms * 1e-3   +                   // Milliseconds since last bit (0...19) 0.000  0.019       0.001
        chips / CPS +                   // Code chips (0...1022)                0.000  0.000999    0.000000976 (~1 usec)
        cg_phase * pow(2, -6) / CPS);   // Code NCO phase (0...63)              0.000  0.00000096  0.000000015 (15 nsec)
    
    return clock;
}

// GNSSDataForEpoch holds all data for a position solution in a given epoch:
//  * satellite (X,Y,Z)             ...     sv(:,0:2)  [m]
//  * clock corrected time t*C      ...     sv(:,3)    [m]
//  * weight=signal power           ... weight(:)
//  * 48-bit ADC clock tick counter ... ticks          [ADC clock ticks]
class GNSSDataForEpoch {
public:
    typedef PosSolver::vec_type vec_type;
    typedef PosSolver::mat_type mat_type;
    typedef TNT::Array1D<int>  ivec_type;

    GNSSDataForEpoch(int max_channels)
        : _chans(0)
        , _sv(4, max_channels, 0.0)
        , _weight(max_channels, 0.0)
        , _sat(max_channels, 0)
        , _ch(max_channels, 0)
        , _prn(max_channels, 0)
        , _type(max_channels, 0)
        , _week(max_channels, 0)
        , _adc_ticks(0ULL) {}

    int       chans() const { return _chans; }
    mat_type     sv() const { return _chans ? _sv.subarray(0,3,0,_chans-1).copy() : PosSolver::mat_type(); }
    vec_type weight() const { return _chans ? _weight.subarray(0,_chans-1).copy() : PosSolver::vec_type(); }
    u64_t adc_ticks() const { return _adc_ticks; }
    int    prn(int i) const { return _prn[i]; }
    int    sat(int i) const { return _sat[i]; }
    int     ch(int i) const { return _ch[i]; }
    int   type(int i) const { return _type[i]; }
    int   week(int i) const { return _week[i]; }

    u4_t ch_has_soln() const {
        u4_t bitmap = 0;
        for (int i=0; i<_chans; ++i)
            bitmap |= (1 << ch(i));
        return bitmap;
    }

    template<typename PRED>
    vec_type weight(PRED const& pred) const {
        if (!_chans)
            return PosSolver::vec_type();
        vec_type weight_filtered = weight().copy();
        int i_filtered=0;
        for (int i=0; i<_chans; ++i) {
            if (!pred(type(i)))
                continue;
            weight_filtered[i_filtered] = _weight[i];
            ++i_filtered;
        }
        if (!i_filtered)
            return PosSolver::vec_type();
        return weight_filtered.subarray(0,i_filtered-1).copy();
    }
    template<typename PRED>
    mat_type sv(PRED const& pred) const {
        if (!_chans)
            return PosSolver::mat_type();
        mat_type sv_filtered = sv().copy();
        int i_filtered=0;
        for (int i=0; i<_chans; ++i) {
            if (!pred(type(i)))
                continue;
            for (int j=0; j<4; ++j)
                sv_filtered[j][i_filtered] = _sv[j][i];
            ++i_filtered;
        }
        if (!i_filtered)
            return PosSolver::mat_type();
        return sv_filtered.subarray(0,3,0,i_filtered-1).copy();
    }

    bool LoadFromReplicas(int chans, const SNAPSHOT* replicas, u64_t adc_ticks) {
        clear();
        _adc_ticks = adc_ticks;
        _chans     = 0;
        for (int i=0; i<chans; ++i) {
            NextTask("solve1");

            // power of received signal
            _weight[_chans] = replicas[i].power;

            // remove satellites with unreasonable signal power
            if (_weight[_chans] < 1e5 || _weight[_chans] > 5e6)
                continue;

            // un-corrected time of transmission
            double t_tx = replicas[i].GetClock();
            if (t_tx == NAN)
                continue;

            // apply clock correction
            t_tx -= replicas[i].eph.GetClockCorrection(t_tx);
            _sv[3][_chans] = C*t_tx; // [s] -> [m]
            
            double t_k = replicas[i].eph.TimeOfEphemerisAge(t_tx);
            UMS hms(fabs(t_k)/60/60);
            if (hms.u > 9) hms.u = 9;
            gps.ch[i].too_old = (hms.u >= 4);
            snprintf(gps.ch[i].age, GPS_N_AGE, "%c%01d:%02d:%02.0f",
                (t_k < 0)? '-':' ', hms.u, hms.m, hms.s);
            //printf("ch%02d %s t_k %s\n", i, PRN(Replicas[i].sat), gps.ch[i].age);

            // get SV position in ECEF coords
            replicas[i].eph.GetXYZ(&_sv[0][_chans],
                                   &_sv[1][_chans],
                                   &_sv[2][_chans],
                                   t_tx);

            _sat[_chans]  = Replicas[i].sat;
            _ch[_chans]   = Replicas[i].ch;
            _prn[_chans]  = Sats[_sat[i]].prn;
            _type[_chans] = Sats[_sat[i]].type;
            _week[_chans] = replicas[i].eph.week;
            _chans       += 1;
        }
        return (_chans > 0);
    }
protected:
    void clear() {
        _adc_ticks = 0;
        _chans     = 0;
        _weight    = 0;
        _sv        = 0;
        _weight    = 0;
        _sat       = 0;
        _ch        = 0;
        _prn       = 0;
        _type      = 0;
        _week      = 0;
    }

private:
    int       _chans;     // number of good channels
    mat_type  _sv;        // sat. x,y,z,ct [m,m,m,m]
    vec_type  _weight;    // weights = sat. signal power
    ivec_type _sat;       // sat  number
    ivec_type _ch;        // channel
    ivec_type _prn;       // prn
    ivec_type _type;      // type
    ivec_type _week;      // week
    u64_t     _adc_ticks; // ADC clock ticks
} ;

void update_gps_info_before()
{
    int samp_hour=0, samp_min=0;
    utc_hour_min_sec(&samp_hour, &samp_min);

    if (gps.last_samp_hour != samp_hour) {
        gps.fixes_hour = gps.fixes_hour_incr;
        gps.fixes_hour_incr = 0;
        gps.fixes_hour_samples++;
        gps.last_samp_hour = samp_hour;
    }
        
    //printf("GPS last_samp=%d samp_min=%d fixes_min=%d\n", gps.last_samp, samp_min, gps.fixes_min);
    if (gps.last_samp != samp_min) {
        //printf("GPS fixes_min=%d fixes_min_incr=%d\n", gps.fixes_min, gps.fixes_min_incr);
        gps.fixes_min = gps.fixes_min_incr;
        gps.fixes_min_incr = 0;
        for (int sat = 0; sat < MAX_SATS; sat++) {
            gps.az[gps.last_samp][sat] = 0;
            gps.el[gps.last_samp][sat] = 0;
        }
        gps.last_samp = samp_min;
    }
}

void update_gps_info_after(GNSSDataForEpoch const& gnssDataForEpoch,
                           std::array<PosSolver::sptr, 3> const& pos_solvers,
                           bool plot_E1B)
{
    gps_pos_t *pos = &gps.POS_data[MAP_WITH_E1B][gps.POS_next];
    pos->x = pos->y = pos->lat = pos->lon = 0;

    gps_map_t *map = &gps.MAP_data[MAP_WITH_E1B][gps.MAP_next];
    map->lat = map->lon = 0;
    map = &gps.MAP_data[MAP_ONLY_E1B][gps.MAP_next];
    map->lat = map->lon = 0;

    if (pos_solvers[0]->ekf_valid() || pos_solvers[0]->spp_valid()) { // solution using all satellites
        
        GPSstat(STAT_TIME, pos_solvers[0]->t_rx());
        clock_correction(pos_solvers[0]->t_rx(), gnssDataForEpoch.adc_ticks()); // TODO
        
        if (gps.tod_chan >= gnssDataForEpoch.chans()) gps.tod_chan = 0;
        tod_correction(gnssDataForEpoch.week(gps.tod_chan), gnssDataForEpoch.sat(gps.tod_chan));
        gps.tod_chan++;

        const PosSolver::LonLatAlt llh = pos_solvers[0]->llh();

        if (gps.have_ref_lla) {
            gps.E1B_plot_separately = plot_E1B;
            const int which_map = (plot_E1B ? MAP_WITH_E1B : MAP_ALL);

            pos = &gps.POS_data[which_map][gps.POS_next];
            pos->x = pos_solvers[0]->pos()(1);       // NB: swapped
            pos->y = pos_solvers[0]->pos()(0);
            pos->lat = llh.lat();
            pos->lon = llh.lon();

            map = &gps.MAP_data[which_map][gps.MAP_next];
            map->lat = llh.lat();
            map->lon = llh.lon();
            gps.MAP_seq_w++;
            gps.MAP_data_seq[gps.MAP_next] = gps.MAP_seq_w;

            if (gps.E1B_plot_separately) {
                if (pos_solvers[1]->ekf_valid() || pos_solvers[1]->spp_valid()) { // not Galileo
                    const PosSolver::LonLatAlt llh1 = pos_solvers[1]->llh();
                    pos = &gps.POS_data[MAP_WITHOUT_E1B][gps.POS_next];
                    pos->x = pos_solvers[1]->pos()(1);       // NB: swapped
                    pos->y = pos_solvers[1]->pos()(0);
                    pos->lat = llh1.lat();
                    pos->lon = llh1.lon();
                    
                    map = &gps.MAP_data[MAP_WITHOUT_E1B][gps.MAP_next];
                    map->lat = llh1.lat();
                    map->lon = llh1.lon();
                }
                if (pos_solvers[2]->ekf_valid() || pos_solvers[2]->spp_valid()) { // only Galileo
                    const PosSolver::LonLatAlt llh2 = pos_solvers[2]->llh();
                    map = &gps.MAP_data[MAP_ONLY_E1B][gps.MAP_next];
                    map->lat = llh2.lat();
                    map->lon = llh2.lon();
                }
            } // gps.E1B_plot_separately

            gps.POS_next++;
            if (gps.POS_next >= GPS_POS_SAMPS) gps.POS_next = 0;
            if (gps.POS_len < GPS_POS_SAMPS) gps.POS_len++;
            gps.POS_seq_w++;
            
            gps.MAP_next++;
            if (gps.MAP_next >= GPS_MAP_SAMPS) gps.MAP_next = 0;
            if (gps.MAP_len < GPS_MAP_SAMPS) gps.MAP_len++;
        } // gps.have_ref_lla

        if (!gps.have_ref_lla && gps.fixes >= 3) {
            gps.ref_lat = llh.lat();
            gps.ref_lon = llh.lon();
            gps.have_ref_lla = true;
        }

    } // solution with all satellites found

    // green  -> EKF
    // yellow -> SPP
    // red    -> no position solution
    const int grn_yel_red = (pos_solvers[0]->ekf_valid() ? 0 : (pos_solvers[0]->spp_valid() ? 1 : 2));
    GPSstat(STAT_SOLN, 0, grn_yel_red, gnssDataForEpoch.ch_has_soln());

    // update az/el
    const auto elev_azim = pos_solvers[0]->elev_azim(gnssDataForEpoch.sv());
    if (pos_solvers[0]->ekf_valid() || pos_solvers[0]->spp_valid()) {
        for (int i=0; i<gnssDataForEpoch.chans(); ++i) {
            NextTask("solve3");
            const int sat = gnssDataForEpoch.sat(i);

            // already have az/el for this sat in this sample period? 
            if (gps.el[gps.last_samp][sat])
                continue;

            const int el = std::round(elev_azim[i].elev_deg);
            const int az = std::round(elev_azim[i].azim_deg);

            // printf("%s NEW EL/AZ=%2d %3d\n", PRN(sat), el, az);
            if (az < 0 || az >= 360 || el <= 0 || el > 90)
                continue;

            gps.az[gps.last_samp][sat] = az;
            gps.el[gps.last_samp][sat] = el;

            gps.shadow_map[az] |= (1 << int(std::round(el / 90.0 * 31.0)));

            // add az/el to channel data
            for (int ch=0; ch < gps_chans; ++ch) {
                gps_chan_t *chp = &gps.ch[ch];
                if (chp->sat == sat) {
                    chp->az = az;
                    chp->el = el;
                }
            }
            // special treatment for QZS_3 
            if (gnssDataForEpoch.prn(i)  == 199 && gnssDataForEpoch.type(i) == QZSS) {
                gps.qzs_3.az = az;
                gps.qzs_3.el = el;
                //printf("QZS-3 az=%d el=%d\n", az, el);
            }
            
        } // next satellite

        gps.fixes++; gps.fixes_min_incr++; gps.fixes_hour_incr++;
        
        // at startup immediately indicate first solution
        if (gps.fixes_min == 0) gps.fixes_min++;

        // at startup incrementally update until first hour sample period has ended
        if (gps.fixes_hour_samples <= 1) gps.fixes_hour++;
        
        const PosSolver::LonLatAlt llh = pos_solvers[0]->llh();
        GPSstat(STAT_LAT, llh.lat());
        GPSstat(STAT_LON, llh.lon());
        GPSstat(STAT_ALT, llh.alt());  
    }
}


// Used by the position solver classes, see kiwi_yield.h
class kiwi_next_task : public kiwi_yield {
public:
    kiwi_next_task() {}
    virtual ~kiwi_next_task() {}

    virtual void yield() const {
        NextTask("PosSolver yield");
    }
} ;

void SolveTask(void *param) {
    GNSSDataForEpoch gnssDataForEpoch(gps_chans);
    auto yield = std::make_shared<kiwi_next_task>();

    std::array<PosSolver::sptr, 3> posSolvers = {
        PosSolver::make(UERE, ADC_CLOCK_TYP, yield), // all satellites
        PosSolver::make(UERE, ADC_CLOCK_TYP, yield), // not Galileo
        PosSolver::make(UERE, ADC_CLOCK_TYP, yield)  // only Galileo
    };

    auto const predNotGalileo  = [](int type) { return (type != E1B); };
    auto const predOnlyGalileo = [](int type) { return (type == E1B); };

    double lat=0, lon=0, alt;
    int good = -1;

    for (;;) {
    
        // while we're waiting send IQ values if requested
        u4_t now = timer_ms();
            int ch = gps.IQ_data_ch - 1;
            if (ch != -1) {
                spi_set(CmdIQLogReset, ch);
                //printf("SOLVE CmdIQLogReset ch=%d\n", ch);
                //TaskSleepMsec(1024 + 100);
                TaskSleepMsec(900);     //jks2
                SPI_MISO *rx = &SPI_SHMEM->gps_iqdata_miso;
                spi_get(CmdIQLogGet, rx, S2B(GPS_IQ_SAMPS_W));
                memcpy(gps.IQ_data, rx->word, S2B(GPS_IQ_SAMPS_W));
               // printf("gps.IQ_data %d rx->word %d S2B(GPS_IQ_SAMPS_W) %d\n", \
                    sizeof(gps.IQ_data), sizeof(rx->word), S2B(GPS_IQ_SAMPS_W));
                gps.IQ_seq_w++;
            }
        //#define SOLVE_RATE  (4-1)   // 1 is GLITCH_GUARD*2
        #define SOLVE_RATE  (2-1)   // 1 is GLITCH_GUARD*2      jks2
        u4_t elapsed = timer_ms() - now;
        u4_t remaining = SEC_TO_MSEC(SOLVE_RATE) - elapsed;
        if (elapsed < SEC_TO_MSEC(SOLVE_RATE)) {
            //printf("ch=%d%s remaining=%d\n", ch+1, (ch+1 == 0)? "(off)":"", remaining);
		    TaskSleepMsec(remaining);
		}

        good = LoadReplicas();

        update_gps_info_before();

        // this needs to replaced, see (*)
        gps.good = good;
        SearchTaskRun();
        if (good == 0) continue;

        const bool plot_E1B   = admcfg_bool("plot_E1B", NULL, CFG_REQUIRED);
        const bool use_kalman = admcfg_default_bool("use_kalman_position_solver", true, NULL);
        for (auto& p : posSolvers)
            p->set_use_kalman(use_kalman);

        if (gnssDataForEpoch.LoadFromReplicas(good, Replicas, ticks)) {
            // new code (*)
            posSolvers[0]->solve(gnssDataForEpoch.sv(),
                                 gnssDataForEpoch.weight(),
                                 gnssDataForEpoch.adc_ticks());

            if (plot_E1B) {
                // make separate position solutions for Galileo and ~Galileo stats
                posSolvers[1]->solve(gnssDataForEpoch.sv(predNotGalileo),
                                     gnssDataForEpoch.weight(predNotGalileo),
                                     gnssDataForEpoch.adc_ticks());
                
                posSolvers[2]->solve(gnssDataForEpoch.sv(predOnlyGalileo),
                                     gnssDataForEpoch.weight(predOnlyGalileo),
                                     gnssDataForEpoch.adc_ticks());
            }
        }

        update_gps_info_after(gnssDataForEpoch, posSolvers, plot_E1B);

        // result_t result = Solve(good, &lat, &lon, &alt);
        TaskStat(TSTAT_INCR|TSTAT_ZERO, 0, "sol");
    }
}
