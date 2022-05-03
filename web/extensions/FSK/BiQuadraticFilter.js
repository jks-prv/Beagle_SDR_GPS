/***************************************************************************
 *   Copyright (C) 2011 by Paul Lutus                                      *
 *   lutusp@arachnoid.com                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/**
 *
 * @author lutusp
 */
// http://en.wikipedia.org/wiki/Digital_biquad_filter

function BiQuadraticFilter(type, center_freq, sample_rate, Q, gainDB) {
    var t = this;

    t.Type_e = { BANDPASS:0, LOWPASS:1, HIGHPASS:2, NOTCH:3, PEAK:4, LOWSHELF:5, HIGHSHELF:6 };
    t.type = t.Type_e.BANDPASS;

    t.a0 = t.a1 = t.a2 = t.b0 = t.b1 = t.b2 = 0;
    t.x1 = t.x2 = t.y = t.y1 = t.y2 = 0;
    t.gain_abs = 0;
    t.center_freq = t.sample_rate = t.Q = t.gainDB = 0;

    if (type != undefined)
        t.configure(type, center_freq, sample_rate, Q, gainDB);
}

    BiQuadraticFilter.prototype.reset = function() {
        var t = this;
        t.x1 = t.x2 = t.y1 = t.y2 = 0;
    }

    BiQuadraticFilter.prototype.frequency = function() {
        return this.center_freq;
    }

    BiQuadraticFilter.prototype.configure = function(type, center_freq, sample_rate, Q, gainDB) {
        var t = this;
        t.reset();
        Q = (Q == 0) ? 1e-9 : Q;
        t.type = type;
        t.sample_rate = sample_rate;
        t.Q = Q;
        t.gainDB = (gainDB != undefined)? gainDB:0;
        t.reconfigure(center_freq);
    }

    // allow parameter change while running
    BiQuadraticFilter.prototype.reconfigure = function(cf) {
        var t = this;
        t.center_freq = cf;
        // only used for peaking and shelving filters
        t.gain_abs = Math.pow(10, t.gainDB / 40);
        var omega = 2 * Math.PI * cf / t.sample_rate;
        var sn = Math.sin(omega);
        var cs = Math.cos(omega);
        var alpha = sn / (2 * t.Q);
        var beta = Math.sqrt(t.gain_abs + t.gain_abs);
        switch (t.type) {
            case t.Type_e.BANDPASS:
                t.b0 = alpha;
                t.b1 = 0;
                t.b2 = -alpha;
                t.a0 = 1 + alpha;
                t.a1 = -2 * cs;
                t.a2 = 1 - alpha;
                break;
            case t.Type_e.LOWPASS:
                t.b0 = (1 - cs) / 2;
                t.b1 = 1 - cs;
                t.b2 = (1 - cs) / 2;
                t.a0 = 1 + alpha;
                t.a1 = -2 * cs;
                t.a2 = 1 - alpha;
                break;
            case t.Type_e.HIGHPASS:
                t.b0 = (1 + cs) / 2;
                t.b1 = -(1 + cs);
                t.b2 = (1 + cs) / 2;
                t.a0 = 1 + alpha;
                t.a1 = -2 * cs;
                t.a2 = 1 - alpha;
                break;
            case t.Type_e.NOTCH:
                t.b0 = 1;
                t.b1 = -2 * cs;
                t.b2 = 1;
                t.a0 = 1 + alpha;
                t.a1 = -2 * cs;
                t.a2 = 1 - alpha;
                break;
            case t.Type_e.PEAK:
                t.b0 = 1 + (alpha * t.gain_abs);
                t.b1 = -2 * cs;
                t.b2 = 1 - (alpha * t.gain_abs);
                t.a0 = 1 + (alpha / t.gain_abs);
                t.a1 = -2 * cs;
                t.a2 = 1 - (alpha / t.gain_abs);
                break;
            case t.Type_e.LOWSHELF:
                t.b0 = t.gain_abs * ((t.gain_abs + 1) - (t.gain_abs - 1) * cs + beta * sn);
                t.b1 = 2 * t.gain_abs * ((t.gain_abs - 1) - (t.gain_abs + 1) * cs);
                t.b2 = t.gain_abs * ((t.gain_abs + 1) - (t.gain_abs - 1) * cs - beta * sn);
                t.a0 = (t.gain_abs + 1) + (t.gain_abs - 1) * cs + beta * sn;
                t.a1 = -2 * ((t.gain_abs - 1) + (t.gain_abs + 1) * cs);
                t.a2 = (t.gain_abs + 1) + (t.gain_abs - 1) * cs - beta * sn;
                break;
            case t.Type_e.HIGHSHELF:
                t.b0 = t.gain_abs * ((t.gain_abs + 1) + (t.gain_abs - 1) * cs + beta * sn);
                t.b1 = -2 * t.gain_abs * ((t.gain_abs - 1) + (t.gain_abs + 1) * cs);
                t.b2 = t.gain_abs * ((t.gain_abs + 1) + (t.gain_abs - 1) * cs - beta * sn);
                t.a0 = (t.gain_abs + 1) - (t.gain_abs - 1) * cs + beta * sn;
                t.a1 = 2 * ((t.gain_abs - 1) - (t.gain_abs + 1) * cs);
                t.a2 = (t.gain_abs + 1) - (t.gain_abs - 1) * cs - beta * sn;
                break;
        }
        // prescale filter constants
        t.b0 /= t.a0;
        t.b1 /= t.a0;
        t.b2 /= t.a0;
        t.a1 /= t.a0;
        t.a2 /= t.a0;
    }

    // provide a static amplitude result for testing
    BiQuadraticFilter.prototype.result = function(f) {
        var t = this;
        var phi = Math.pow((Math.sin(2.0 * Math.PI * f / (2.0 * t.sample_rate))), 2.0);
        return (Math.pow(t.b0 + t.b1 + t.b2, 2.0) - 4.0 * (t.b0 * t.b1 + 4.0 * t.b0 * t.b2 + t.b1 * t.b2) * phi + 16.0 * t.b0 * t.b2 * phi * phi) / (Math.pow(1.0 + t.a1 + t.a2, 2.0) - 4.0 * (t.a1 + 4.0 * t.a2 + t.a1 * t.a2) * phi + 16.0 * t.a2 * phi * phi);
    }

    // provide a static decibel result for testing
    BiQuadraticFilter.prototype.log_result = function(f) {
        var r;
        try {
            r = 10 * Math.log10(this.result(f));
        } catch (e) {
            r = -100;
        }
        if(!isFinite(r) || isNaN(r)) {
            r = -100;
        }
        return r;
    }

   // return the constant set for this filter
    BiQuadraticFilter.prototype.constants = function() {
      var t = this;
      return new [ t.b0, t.b1, t.b2, t.a1, t.a2 ];
   }

   // perform one filtering step
    BiQuadraticFilter.prototype.filter = function(x) {
      var t = this;
      var y = t.b0 * x + t.b1 * t.x1 + t.b2 * t.x2 - t.a1 * t.y1 - t.a2 * t.y2;
      t.x2 = t.x1;
      t.x1 = x;
      t.y2 = t.y1;
      t.y1 = y;
      return (y);
   }
