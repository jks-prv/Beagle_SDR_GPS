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

function JNX()
{
   var t = this;
   
   t.State_e = { NOSIGNAL:0, SYNC1:1, SYNC2:2, READ_DATA:3, FIXED_LENGTH:4 };
   t.states = [ 'NOSIGNAL', 'SYNC1', 'SYNC2', 'READ_DATA', 'FIXED_LENGTH' ];
   t.state = t.State_e.NOSIGNAL;
   
   // "front porch" mode -- look for run of "mark" bits followed by the start bit
   t.fp_OFF = 0;
   t.fp_WAIT = 1;
   t.fp_WAIT2 = 2;
   t.fp_START = 3;
   t.fp_PASS = 4;
   
   // constants
   t.invsqr2 = 1.0 / Math.sqrt(2);
   
   // persistent
   t.audio_average = 0;
   t.signal_accumulator = 0;
   t.bit_duration = 0;
   t.succeed_tally = t.fail_tally = 0;
   
   // no init
   t.bit_count = t.code_bits = t.code_word = t.idle_word = 0;
   t.error_count = t.valid_count = 0;
   
   // init
   t.inverted = 0;
   
   // objects
   t.biquad_mark = new BiQuadraticFilter();
   t.biquad_space = new BiQuadraticFilter();
   t.biquad_lowpass = new BiQuadraticFilter();
}

JNX.prototype.setup_values = function(sample_rate, center_frequency_f, shift_Hz, baud_rate, framing, inverted, encoding) {
   var t = this;
   t.sample_rate = sample_rate;
   t.center_frequency_f = (center_frequency_f == undefined)? 500.0 : center_frequency_f;

   t.baud_rate = (baud_rate == undefined)? 100 : baud_rate;
   t.shift_Hz = (shift_Hz == undefined)? 850 : shift_Hz;
   t.deviation_f = t.shift_Hz / 2.0;

   t.lowpass_filter_f = 140.0;

   t.audio_average_tc = 1000.0 / sample_rate;
   t.audio_minimum = 256;

   // this value must never be zero
   t.baud_rate = (t.baud_rate < 10) ? 10 : t.baud_rate;
   t.bit_duration_seconds = 1.0 / t.baud_rate;
   t.bit_sample_count = Math.floor(t.sample_rate * t.bit_duration_seconds + 0.5);
   //console.log('JNX bit_sample_count='+ t.bit_sample_count +' sr+bds='+ (t.sample_rate * t.bit_duration_seconds));
   t.half_bit_sample_count = t.bit_sample_count / 2;
   
   t.framing = framing;
   t.stop_variable = (framing.endsWith('V') || framing.includes('EFR'))? 1:0;
   
   if (framing == 'CHU') {
      t.fp = t.fp_WAIT;
      t.fp_mark_bits = 32;
      t.fp_pass_bits = 11 * 10;
   } else
      t.fp = t.fp_OFF;
   
   if (dbgUs && framing == 'CHU') {
      //t.dbg = 1;
      t.chu_dbg = 1;
   }
   
   t.inverted = inverted;

   switch(encoding) {
   
      case 'ITA2':
      case 'ASCII':
      default:
         t.encoding = new FSK_async(framing, encoding);
         break;
   
      case 'CCIR476':
         t.encoding = new CCIR476();
         break;
   
   }

   t.msb = t.encoding.get_msb();
   t.nbits = t.encoding.get_nbits();

   t.pulse_edge_event = false;
   t.error_count = 0;
   t.valid_count = 0;
   t.bbuffer = [];
   t.out_buffer = [];
   t.sbuffer = [];
   t.sample_count = 0;
   t.next_event_count = 0;
   t.averaged_mark_state = 0;

   t.zero_crossing_samples = 16;
   t.zero_crossings_divisor = 4;
   t.zero_crossing_count = 0;
   t.zero_crossings = [];

   t.sync_delta = 0;
   t.update_filters();
   t.state = t.State_e.NOSIGNAL;
}
   
JNX.prototype.set_baud_error_cb = function(func) {
   this.baud_error_cb = func;
}

JNX.prototype.set_scope_cb = function(func) {
   this.scope_cb = func;
}

JNX.prototype.set_output_char_cb = function(func) {
   this.output_char_cb = func;
}

JNX.prototype.set_output_bit_cb = function(func) {
   this.output_bit_cb = func;
}

JNX.prototype.get_encoding_obj = function(func) {
   return this.encoding;
}

JNX.prototype.update_filters = function() {
   this.set_filter_values();
   this.configure_filters();
}

JNX.prototype.set_filter_values = function() {
   var t = this;
   // carefully manage the parameters WRT the center frequency

   // Q must change with frequency
   t.mark_space_filter_q = 6 * t.center_frequency_f / 1000.0;

   // try to maintain a zero mixer output
   // at the carrier frequency
   var _qv = t.center_frequency_f + (4.0 * 1000 / t.center_frequency_f);
   t.mark_f = _qv + t.deviation_f;
   t.space_f = _qv - t.deviation_f;
}

JNX.prototype.configure_filters = function() {
   var t = this;
   t.biquad_mark.configure(t.biquad_mark.Type_e.BANDPASS, t.mark_f, t.sample_rate, t.mark_space_filter_q);
   t.biquad_space.configure(t.biquad_space.Type_e.BANDPASS, t.space_f, t.sample_rate, t.mark_space_filter_q);
   t.biquad_lowpass.configure(t.biquad_lowpass.Type_e.LOWPASS, t.lowpass_filter_f, t.sample_rate, t.invsqr2);
}

JNX.prototype.set_state = function(s) {
   var t = this;
   if (s != t.state) {
      t.state = s;
      //console.log("New state: " + t.states[t.state]);
   }
}

JNX.prototype.process_data = function(samps, nsamps) {
   var t = this;
   var _mark_level, _space_level, _mark_abs, _space_abs;
   
   for (var i = 0; i < nsamps; i++) {
      var dv = samps[i];

      // separate mark and space by narrow filtering
      _mark_level = t.biquad_mark.filter(dv);
      _space_level = t.biquad_space.filter(dv);
      
      _mark_abs = Math.abs(_mark_level);
      _space_abs = Math.abs(_space_level);
      
      t.audio_average += (Math.max(_mark_abs, _space_abs) - t.audio_average) * t.audio_average_tc;
      
      t.audio_average = Math.max(.1, t.audio_average);
      
      // produce difference of absolutes of mark and space
      var _diffabs = (_mark_abs - _space_abs);
      
      _diffabs /= t.audio_average;
      
      dv /= t.audio_average;
      
      _mark_level /= t.audio_average;
      _space_level /= t.audio_average;
      
      // now low-pass the resulting difference
      var _logic_level = t.biquad_lowpass.filter(_diffabs);

      ////////
      
      var _mark_state = (_logic_level > 0);
      t.signal_accumulator += (_mark_state) ? 1 : -1;
      t.bit_duration++;

      // adjust signal synchronization over time
      // by detecting zero crossings

      if (t.zero_crossings != null) {
         if (_mark_state != t.old_mark_state) {
            // a valid bit duration must be longer than bit duration / 2
            if ((t.bit_duration % t.bit_sample_count) > t.half_bit_sample_count) {
               // create a relative index for this zero crossing
               var _index = Math.floor((t.sample_count - t.next_event_count + t.bit_sample_count * 8) % t.bit_sample_count);
               t.zero_crossings[_index / t.zero_crossings_divisor]++;
            }
            t.bit_duration = 0;
         }
         t.old_mark_state = _mark_state;
          
         if (t.sample_count % t.bit_sample_count == 0) {
            t.zero_crossing_count++;
            if (t.zero_crossing_count >= t.zero_crossing_samples) {
               var _best = 0;
               var _index = 0;
               var _q;
               // locate max zero crossing
               for (var j = 0; j < t.zero_crossings.length; j++) {
                  _q = t.zero_crossings[j];
                  t.zero_crossings[j] = 0;
                  if (_q > _best) {
                     _best = _q;
                     _index = j;
                  }
               }
               if (_best > 0) { // if there is a basis for choosing
                      // create a signed correction value
                      _index *= t.zero_crossings_divisor;
                      _index = ((_index + t.half_bit_sample_count) % t.bit_sample_count) - t.half_bit_sample_count;
                      //parent.p("error index: " + _index);
                      // limit loop gain
                      _index /= 8.0;
                      // sync_delta is a temporary value that is
                      // used once, then reset to zero
                      t.sync_delta = _index;
                      //t.sync_delta = 0;
                      // baud_error is persistent -- used by baud error label
                      t.baud_error = _index;
                      t.baud_error_cb(t.baud_error);
               }
               t.zero_crossing_count = 0;
            }
         }
      }

      // flag the center of signal pulses
      t.pulse_edge_event = (t.sample_count >= t.next_event_count);
      if (t.pulse_edge_event) {
         t.averaged_mark_state = (t.signal_accumulator > 0) ^ t.inverted;
         //console.log('sa='+ t.signal_accumulator +' ams='+ t.averaged_mark_state);
         t.signal_accumulator = 0;
         // set new timeout value, include zero crossing correction
         t.next_event_count = t.sample_count + t.bit_sample_count + Math.floor(t.sync_delta + 0.5);
         t.sync_delta = 0;
      }
      
      if (t.scope_cb) t.scope_cb(_logic_level, t.pulse_edge_event? 1:0, t.averaged_mark_state? 1:0);

      if (t.output_bit_cb && t.pulse_edge_event)
         t.output_bit_cb(t.averaged_mark_state? 1:0);

      if (t.audio_average < t.audio_minimum && t.state != t.State_e.NOSIGNAL) {
         t.set_state(t.State_e.NOSIGNAL);
      } else
      if (t.state == t.State_e.NOSIGNAL) {
         t.sync_setup = 1;
      }
      
      if (!t.pulse_edge_event) {
         t.sample_count++;
         continue;
      }

      var bit = t.averaged_mark_state? 1:0;

      if (t.fp == t.fp_START || t.fp == t.fp_PASS) {
         if (t.fp == t.fp_START) {
            t.fp = t.fp_PASS;
         }
         t.fp_count++;
         if (t.fp_count > t.fp_pass_bits) {
            t.fp = t.fp_WAIT;
         }
      }

      if (t.fp == t.fp_WAIT || t.fp == t.fp_WAIT2) {
         if (t.fp == t.fp_WAIT) {
            t.fp_count = 0;
            t.fp = t.fp_WAIT2;
            //console.log('fp_WAIT');
         }
         if (t.fp_count >= t.fp_mark_bits && bit == 0) {
            t.fp_count = 0;
            t.sync_setup = 1;
            t.fp = t.fp_START;      // one-time event for others to observe
            //console.log('fp_START');
         } else {
            if (bit == 1) t.fp_count++; else t.fp_count = 0;
            t.sample_count++; continue;   // don't forward any bits while waiting
         }
      }
   
      if (t.chu_dbg) {
         if (t.fp == t.fp_START) {
            t.chu_bn = 0;
            t.chu_cc = 0;
            t.chu_s1 = '';
         }
         
         if (t.chu_bn == 0) {
            t.chu_s = bit +' ';
            t.chu_v = 0;
         }
         if (t.chu_bn >= 1 && t.chu_bn <= 8) { t.chu_s += bit; t.chu_v >>= 1; t.chu_v |= bit? 0x80:0; }
         if (t.chu_bn == 8) t.chu_s += ' ';
         if (t.chu_bn >= 9 && t.chu_bn <= 10) { t.chu_s += bit; t.chu_s1 += bit; }
         if (t.chu_bn == 10) {
            var hv = t.chu_v;
            hv = (((hv & 0xf) << 4) & 0xf0) | (((hv & 0xf0) >>> 4) & 0xf);
            t.chu_s = t.chu_cc.leadingZeros(2) +': '+ t.chu_s +' '+ hv.toHex(-2) +' ^'+ (hv ^ 0xff).toHex(-2) +' ';
            //console.log(t.chu_s);
            t.chu_s1 += '_'+ hv.toHex(-2) +' ';
            t.chu_bn = 0;

            t.chu_cc++;
            if (t.chu_cc == 5) t.chu_s1 += ' |  ';
            if (t.chu_cc == 10) {
                  t.chu_bn = 11;    // idle the loop until next fp_START
                  t.chu_cc = 0;
                  console.log(t.chu_s1);
                  //console.log('CHU DONE');
            }
         } else
            t.chu_bn++;

         //t.sample_count++; continue;
      }
      
      if (t.sync_setup) {
         t.bit_count = 0;
         t.code_bits = 0;
         t.error_count = 0;
         t.valid_count = 0;
         t.encoding.reset();
         t.sync_chars = [];
         
         // If we've already waited for the start bit via front porch search
         // use a fixed-length transfer scheme.
         if (t.fp == t.fp_START)
            t.set_state(t.State_e.FIXED_LENGTH);
         else
            t.set_state(t.State_e.SYNC1);

         t.sync_setup = 0;
      }
            
      switch (t.state) {
         // scan indefinitely for valid bit pattern
         case t.State_e.SYNC1:
            t.code_bits = (t.code_bits >> 1) | (bit? t.msb : 0);
            if (t.dbg) console.log('SYNC1 '+ (bit? 1:0) +' 0x'+ t.code_bits.toString(16));
            if (t.encoding.check_bits(t.code_bits)) {
               t.sync_chars.push(t.code_bits);
               t.valid_count++;
               if (t.dbg) console.log('SYNC1 OK: first valid sync_chars. len='+ t.sync_chars.length);
               t.bit_count = 0;
               t.code_bits = 0;
               t.set_state(t.State_e.SYNC2);
               t.waiting = true;
            }
            break;

         // sample and validate bits in groups of t.nbits
         case t.State_e.SYNC2:
            // find any bit alignment that produces a valid character
            // then test that synchronization in subsequent groups of t.nbits bits
            // wait for start bit if there are a variable number of stop bits
            if (t.stop_variable && t.waiting && bit == 1) {
               if (t.dbg) console.log('SYNC2 waiting');
               break;
            }
            t.waiting = false;

            t.code_bits = (t.code_bits >> 1) | (bit? t.msb : 0);
            if (t.dbg) console.log('SYNC2-'+ t.bit_count +' '+ (bit? 1:0) +' 0x'+ t.code_bits.toString(16));
            t.bit_count++;
            if (t.bit_count == t.nbits) {
               if (t.dbg) console.log("SYNC2 0x"+ t.code_bits.toString(16));
               if (t.encoding.check_bits(t.code_bits)) {
                  t.sync_chars.push(t.code_bits);
                  t.code_bits = 0;
                  t.bit_count = 0;
                  t.valid_count++;
                  if (t.dbg) console.log("SYNC2 OK: valid_count="+ t.valid_count +' sync_chars.len='+ t.sync_chars.length);

                  // successfully read 4 characters?
                  if (t.valid_count == 4) {
                     for (var k=0; k < t.sync_chars.length; k++) {
                        var rv = t.encoding.process_char(t.sync_chars[k], 0, t.output_char_cb);
                        if (rv.tally == 1) t.succeed_tally++; else if (rv.tally == -1) t.fail_tally++;
                     }
                     t.set_state(t.State_e.READ_DATA);
                     if (t.dbg) console.log('READ_DATA sync_chars.len='+ t.sync_chars.length);
                  }
               } else { // failed subsequent bit test
                  t.code_bits = 0;
                  t.bit_count = 0;
                  if (t.dbg) console.log("SYNC2 BAD: restarting sync");
                  t.sync_setup = 1;
               }
               t.waiting = true;
            }
            break;

         case t.State_e.READ_DATA:
            // wait for start bit if there are a variable number of stop bits
            if (t.stop_variable && t.waiting && bit == 1) {
               if (t.dbg) console.log('READ_DATA waiting');
               break;
            }
            t.waiting = false;

            t.code_bits = (t.code_bits >> 1) | (bit? t.msb : 0);
            t.bit_count++;
            if (t.bit_count == t.nbits) {
               if (t.error_count > 0) {
                  if (t.dbg) console.log("READ_DATA: error count=" + t.error_count);
               }
               var rv = t.encoding.process_char(t.code_bits, 0, t.output_char_cb);
               if (rv.tally == 1) t.succeed_tally++; else if (rv.tally == -1) t.fail_tally++;
               if (rv.success) {
                  if (t.error_count > 0) {
                     t.error_count--;
                  }
               } else {
                  t.error_count++;
                  if (t.error_count > 2) {
                     if (t.dbg) console.log("READ_DATA: returning to sync");
                     t.sync_setup = 1;
                  }
               }
               t.bit_count = 0;
               t.code_bits = 0;
               t.waiting = true;
            }
            break;

         case t.State_e.FIXED_LENGTH:
            if (t.fp == t.fp_START) t.fixed_start = 1;
            t.code_bits = (t.code_bits >> 1) | (bit? t.msb : 0);
            t.bit_count++;
            if (t.bit_count == t.nbits) {
               t.encoding.process_char(t.code_bits, t.fixed_start, t.output_char_cb);
               t.bit_count = 0;
               t.code_bits = 0;
               t.fixed_start = 0;
            }
            break;
      }

      t.sample_count++;
   }
}
