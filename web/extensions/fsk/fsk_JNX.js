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

function fsk_JNX()
{
   var t = this;
   
   t.State_e = { NOSIGNAL:0, SYNC_SETUP:1, SYNC1:2, SYNC2:3, READ_DATA:4 };
   t.states = [ 'NOSIGNAL', 'SYNC_SETUP', 'SYNC1', 'SYNC2', 'READ_DATA' ];
   t.state = t.State_e.NOSIGNAL;
   
   // constants
   t.invsqr2 = 1.0 / Math.sqrt(2);
   t.code_character32 = 0x6a;
   t.code_ltrs = 0x1f;
   t.code_figs = 0x1b;
   t.code_alpha = 0x0f;
   t.code_beta = 0x33;
   t.code_char32 = 0x6a;
   t.code_rep = 0x66;
   t.char_bell = 0x07;
   
   // persistent
   t.audio_average = 0;
   t.signal_accumulator = 0;
   t.bit_duration = 0;
   t.succeed_tally = t.fail_tally = 0;
   
   // no init
   t.bit_count = t.code_bits = t.code_word = t.idle_word = 0;
   t.error_count = t.valid_count = 0;
   
   // init
   t.shift = false;
   t.alpha_phase = false;
   t.invert = 0;
   
   // config
   t.strict_mode = false;

   // objects
   t.coding = new ITA2();
   t.nbit = t.coding.nbit();
   t.nbits = t.coding.nbits();
   t.biquad_mark = new BiQuadraticFilter();
   t.biquad_space = new BiQuadraticFilter();
   t.biquad_lowpass = new BiQuadraticFilter();
}

    fsk_JNX.prototype.setup_values = function(sample_rate, baud_rate, shift_Hz, center_frequency_f) {
        var t = this;
        t.sample_rate = sample_rate;
        t.baud_rate = (baud_rate == undefined)? 100 : baud_rate;
        t.shift_Hz = (shift_Hz == undefined)? 850 : shift_Hz;
        t.deviation_f = t.shift_Hz / 2.0;
        t.center_frequency_f = (center_frequency_f == undefined)? 500.0 : center_frequency_f;

        t.lowpass_filter_f = 140.0;

        t.audio_average_tc = 1000.0 / sample_rate;
        t.audio_minimum = 256;

        // this value must never be zero
        t.baud_rate = (t.baud_rate < 10) ? 10 : t.baud_rate;
        t.bit_duration_seconds = 1.0 / t.baud_rate;
        t.bit_sample_count = Math.floor(t.sample_rate * t.bit_duration_seconds + 0.5);
        console.log('FSK bit_sample_count='+ t.bit_sample_count);
        t.half_bit_sample_count = t.bit_sample_count / 2;
        t.pulse_edge_event = false;
        t.shift = false;
        t.error_count = 0;
        t.valid_count = 0;
        t.sample_interval = 1.0 / t.sample_rate;
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
    }

    fsk_JNX.prototype.update_filters = function() {
        this.set_filter_values();
        this.configure_filters();
    }

    fsk_JNX.prototype.set_filter_values = function() {
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

    fsk_JNX.prototype.configure_filters = function() {
        var t = this;
        t.biquad_mark.configure(t.biquad_mark.Type_e.BANDPASS, t.mark_f, t.sample_rate, t.mark_space_filter_q);
        t.biquad_space.configure(t.biquad_space.Type_e.BANDPASS, t.space_f, t.sample_rate, t.mark_space_filter_q);
        t.biquad_lowpass.configure(t.biquad_lowpass.Type_e.LOWPASS, t.lowpass_filter_f, t.sample_rate, t.invsqr2);
    }

    fsk_JNX.prototype.set_state = function(s) {
        var t = this;
        if (s != t.state) {
            t.state = s;
            //console.log("New state: " + t.states[t.state]);
        }
    }

    fsk_JNX.prototype.debug_print = function(code) {
        var t = this;
        var ch = t.coding.code_to_char(code, false);
        ch = (ch < 0) ? '_' : ch;
        
        if (0) {
        if (code == code_rep) {
        alpha_channel = true;
        }
        if (code == code_alpha) {
        alpha_channel = true;
        }
        }
        
        var qs = t.alpha_phase ? "alpha                                " : "rep";
        //System.out.println(String.format("%s|%s: count = %d, code = %x, ch = %c, shift = %s",qs, s, char_count, code, ch, shift ? "True" : "False"));
        console.log(qs +'|0x'+ code.toString(16) +':'+ ch);
        //alpha_channel = !alpha_channel;
    }

    // two phases: alpha and rep
    // marked during sync by code_alpha and code_rep
    // then for data: rep phase character is sent first,
    // then, three chars later, same char is sent in alpha phase
    fsk_JNX.prototype.process_char = function(code) {
        var t = this;
            if (0) {
               var rtn = t.coding.check_bits(code);
               //console.log('process_char 0x'+ code.toString(16) +' '+ (rtn? 'OK':'BAD'));
               return rtn;
            }
        //t.debug_print(code);
        var success = t.coding.check_bits(code);
         if (!success) {
             t.fail_tally++;
             //console.log('fail all options: 0x'+ code.toString(16) +' -> 0x'+ t.c1.toString(16));
         } else {
             t.succeed_tally++;

             code = t.coding.code();
             switch (code) {
                 case t.code_ltrs:
                     t.shift = false;
                     break;
                 case t.code_figs:
                     t.shift = true;
                     break;
                 default:
                     var chr = t.coding.code_to_char(code, t.shift);
                     //console.log('code=0x'+ code.toString(16) +' chr=0x'+ Math.abs(chr).toString(16));
                     if (chr < 0) {
                         console.log('missed this code: 0x'+ Math.abs(chr).toString(16));
                     } else {
                         fsk_output_char(chr);
                     }
                     break;
             } // switch

         }

        return success;
    }

fsk_JNX.prototype.process_data = function(samps, nsamps) {
   var t = this;
   var _mark_level, _space_level, _mark_abs, _space_abs;
   
   for (var i = 0; i < nsamps; i++) {
      var dv = samps[i];
      t.time_sec = t.sample_count * t.sample_interval;

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
                      // baud_error is persistent -- used by baud error label
                      t.baud_error = _index;
                      fsk_baud_error(t.baud_error);
                  }
                  t.zero_crossing_count = 0;
              }
          }
      }

      // flag the center of signal pulses
      t.pulse_edge_event = (t.sample_count >= t.next_event_count);
      if (t.pulse_edge_event) {
          t.averaged_mark_state = (t.signal_accumulator > 0) ^ t.invert;
          //console.log('sa='+ t.signal_accumulator +' ams='+ t.averaged_mark_state);
          t.signal_accumulator = 0;
          // set new timeout value, include zero crossing correction
          t.next_event_count = t.sample_count + t.bit_sample_count + Math.floor(t.sync_delta + 0.5);
          t.sync_delta = 0;
      }
      
      fsk_scope(_logic_level, t.pulse_edge_event? 1:0);

      if (t.audio_average < t.audio_minimum && t.state != t.State_e.NOSIGNAL) {
          t.set_state(t.State_e.NOSIGNAL);
      } else if (t.state == t.State_e.NOSIGNAL) {
          t.set_state(t.State_e.SYNC_SETUP);
      }


   if (0) {
      if (t.pulse_edge_event) {
         console.log((t.averaged_mark_state? 1:0) +' '+ (t.averaged_mark_state? 0:1) +' '+ t.sample_count);
      }
   } else {

      switch (t.state) {
          case t.State_e.SYNC_SETUP:
              t.bit_count = -1;
              t.code_bits = 0;
              t.error_count = 0;
              t.valid_count = 0;
              t.shift = false;
              t.sync_chars = [];
              t.set_state(t.State_e.SYNC1);
              break;
          // scan indefinitely for valid bit pattern
          case t.State_e.SYNC1:
              if (t.pulse_edge_event) {
                  t.code_bits = (t.code_bits >> 1) | ((t.averaged_mark_state) ? t.nbit : 0);
                  //console.log('SYNC1 '+ (t.averaged_mark_state? 1:0) +' 0x'+ t.code_bits.toString(16));
                  if (t.coding.check_bits(t.code_bits)) {
                      t.sync_chars.push(t.code_bits);
                      t.valid_count++;
                      //console.log('SYNC1 OK: first valid sync_chars.len='+ t.sync_chars.length);
                      t.bit_count = 0;
                      t.code_bits = 0;
                      t.set_state(t.State_e.SYNC2);
                  }
              }
              break;
          //  sample and validate bits in groups of t.nbits
          case t.State_e.SYNC2:
              // find any bit alignment that produces a valid character
              // then test that synchronization in subsequent groups of t.nbits bits
              if (t.pulse_edge_event) {
                  t.code_bits = (t.code_bits >> 1) | ((t.averaged_mark_state) ? t.nbit : 0);
                  //console.log('SYNC2-'+ t.bit_count +' '+ (t.averaged_mark_state? 1:0) +' 0x'+ t.code_bits.toString(16));
                  t.bit_count++;
                  if (t.bit_count == t.nbits) {
                      //console.log("SYNC2 0x"+ t.code_bits.toString(16));
                      if (t.coding.check_bits(t.code_bits)) {
                          //console.log("SYNC2 OK: valid_count="+ t.valid_count);
                          t.sync_chars.push(t.code_bits);
                          t.code_bits = 0;
                          t.bit_count = 0;
                          t.valid_count++;
                          // successfully read 4 characters?
                          if (t.valid_count == 8) {
                              for (var k=0; k < t.sync_chars.length; k++) {
                                  t.process_char(t.sync_chars[k]);
                              }
                              t.set_state(t.State_e.READ_DATA);
                              //console.log('READ_DATA sync_chars.len='+ t.sync_chars.length);
                          }
                      } else { // failed subsequent bit test
                          t.code_bits = 0;
                          t.bit_count = 0;
                          //console.log("SYNC2 BAD: restarting sync");
                          t.set_state(t.State_e.SYNC_SETUP);
                      }
                  }
              }
              break;
          case t.State_e.READ_DATA:
              if (t.pulse_edge_event) {
                  t.code_bits = (t.code_bits >> 1) | ((t.averaged_mark_state) ? t.nbit : 0);
                  t.bit_count++;
                  if (t.bit_count == t.nbits) {
                      if (t.error_count > 0) {
                          console.log("READ_DATA: error count=" + t.error_count);
                      }
                      if (t.process_char(t.code_bits)) {
                          if (t.error_count > 0) {
                              t.error_count--;
                          }
                      } else {
                          t.error_count++;
                          if (t.error_count > 2) {
                              console.log("READ_DATA: returning to sync");
                              t.set_state(t.State_e.SYNC_SETUP);
                          }
                      }
                      t.bit_count = 0;
                      t.code_bits = 0;
                  }
              }
              break;
      }

}
      t.sample_count++;
  }
}
