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

function FSK_async(framing, encoding) {
   var t = this;
   t.framing = framing;
   t.start_bit = 1;
   t.data_bits = +framing.substr(0,1);
   t.stop_variable = 0;

   if (framing.endsWith('0V')) {
      t.stop_bits = 0;
      t.stop_variable = 1;
   } else
   if (framing.endsWith('1V')) {
      t.stop_bits = 1;
      t.stop_variable = 1;
   } else
   if (framing.endsWith('1.5')) {
      t.stop_bits = 1.5;
   } else
   if (framing.endsWith('2')) {
      t.stop_bits = 2;
   } else {
      t.stop_bits = 1;
   }

   if (framing.includes('E')) {
      t.parity_bits = 1;
      t.parity_even = 1;
   } else
   if (framing.includes('O')) {
      t.parity_bits = 1;
      t.parity_odd = 1;
   } else
   if (framing.includes('P')) {
      t.parity_bits = 1;
      t.parity_ignore = 1;
   } else {
      t.parity_bits = 0;
   }
   
   if (framing.includes('EFR')) {
      t.EFR = 1;
      if (framing == 'EFR2') t.EFR2 = 1;     // EFR2 prints only "uncommon" telegrams
      if (t.EFR2)
         fsk_output_char('EFR2 mode displays only uncommon telegrams\n');
      t.EFR_hdr_found = 0;
      t.EFR_ch = [];
      t.data_bits = 8;
      t.parity_bits = 1;
      t.parity_ignore = 1;
      t.stop_bits = 1;
      t.stop_variable = 1;    // to handle idle mark carrier
   } else {
      t.EFR = 0;
      t.EFR2 = 0;
   }

   if (framing == 'CHU') {
      t.CHU = 1;
      t.data_bits = 8;
      t.parity_bits = 0;
      t.stop_bits = 2;
      fsk_output_char('CHU mode not yet working\n');
   }

   t.nbits = t.start_bit + t.data_bits + t.parity_bits + t.stop_bits;
   if (t.stop_bits == 1.5) t.nbits *= 2;
   t.msb = 1 << (t.nbits - 1);
   t.data_msb = 1 << (t.data_bits - 1);
   console.log('FSK_async data_bits='+ t.data_bits +' parity_bits='+ t.parity_bits +' stop_bits='+ t.stop_bits +' nbits='+ t.nbits +' data_msb=0x'+ t.data_msb.toString(16) +' msb=0x'+ t.msb.toString(16) +' enc='+ encoding);
   t.ITA2 = t.ASCII = 0;
   
   switch (encoding) {
   
   case 'ITA2':
   default:
      t.ITA2 = 1;
      t.LETTERS = 0x1f;
      t.FIGURES = 0x1b;
      break;

   case 'ASCII':
      t.ASCII = 1;
      t.LETTERS = -1;
      t.FIGURES = -1;
      break;
   }

   t.last_code = 0;
   t.code_ltrs = []; t.ltrs_code = [];
   t.code_figs = []; t.figs_code = [];
   t.shift = false;

   var NUL = '\00';
   var QUO = '\'';
   var LF = '\n';
   var CR = '\r';
   var BEL = '\07';
   var FGS = LTR = '_';   // documentation only

   // see https://en.wikipedia.org/wiki/Baudot_code and http://www.quadibloc.com/crypto/tele03.htm
   // this is the US-TTY version: BEL $ # ' " and ; differ from standard ITA2
   t.ltrs = [
    // x0   x1   x2   x3   x4   x5   x6   x7   x8   x9   xa   xb   xc   xd   xe   xf
      NUL, 'E',  LF, 'A', ' ', 'S', 'I', 'U',  CR, 'D', 'R', 'J', 'N', 'F', 'C', 'K',   // 0x
      'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q', 'O', 'B', 'G', FGS, 'M', 'X', 'V', LTR    // 1x
   ];
   
   t.figs = [
    // x0   x1   x2   x3   x4   x5   x6   x7   x8   x9   xa   xb   xc   xd   xe   xf
      NUL, '3',  LF, '-', ' ', BEL, '8', '7',  CR, '$', '4', QUO, ',', '!', ':', '(',   // 0x
      '5', '"', ')', '2', '#', '6', '0', '1', '9', '?', '&', FGS, '.', '/', ';', LTR    // 1x
   ];

   // tables are small enough we don't bother with a hash map/table
   for (var code = 0; code < 32; code++) {
      var ltrv = t.ltrs[code];
      if (ltrv != '_') {
         t.code_ltrs[code] = ltrv;
         t.ltrs_code[ltrv] = code;
      }
      var figv = t.figs[code];
      if (figv != '_') {
         t.code_figs[code] = figv;
         t.figs_code[figv] = code;
      }
   }
}

FSK_async.prototype.reset = function() {
   this.shift = false;
}

FSK_async.prototype.get_shift = function() {
   return this.shift;
}

FSK_async.prototype.get_nbits = function() {
   return this.nbits;
}

FSK_async.prototype.get_msb = function() {
   return this.msb;
}

FSK_async.prototype.check_valid = function(code) {
   if (this.ITA2)
      return (code >= 0 && code < 32);
   else
      return (code >= 0 && code <= 0x7f);
}

FSK_async.prototype.code_to_char = function(code, shift) {
   var t = this;
   var ch;
   
   if (t.ITA2) {
      ch = shift? t.code_figs[code] : t.code_ltrs[code];
      if (ch == undefined)
         ch = -code;   // default: return negated code
      //console.log('ITA2 code=0x'+ code.toString(16) +' shift='+ (shift? 'T':'F') +' char=['+ ch +']');
   } else {
      if (
         (code >= 0x00 && code <= 0x09) ||
         (code >= 0x0b && code <= 0x0c) ||
         (code >= 0x0e && code <= 0x1f) ||
         (code >= 0x7f)) {
            ch = -code;    // non-printing
      } else {
         ch = String.fromCharCode(code);
      }
      if (t.EFR) {
         ch = '';
         var c;
         if (t.EFR_hdr_found == 0) {
            t.EFR_ch.push(code);
            if (t.EFR_ch.length > 4) t.EFR_ch.shift();
            c = t.EFR_ch;
            if (c[0] == 0x68 && c[1] == c[2] && c[3] == 0x68) t.EFR_hdr_found = 1;
         } else {
            t.EFR_ch.push(code);
            c = t.EFR_ch;
            var ulen = c[1];
            var len = ulen + (4+2);
            if (c.length == len) {
               //console.log('DECISION len='+ len +' eom='+ c[len-1].toHex(2));
               if (c[len-1] == 0x16) {
                  if (ulen == 0xa && (c[4] & 0xf) == 0x7 && c[5] == 0 && c[6] == 0 && (c[13] & 0x7f) == 18) {
                     if (!t.EFR2) {
                        // time telegram
                        //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
                        // 68 0a 0a 68 n7 00 00 -- ss .. .. .. .. .. ck 16
                        var ss = (c[8] >> 2).leadingZeros(2);
                        var mm = (c[9] & 0x3f).leadingZeros(2);
                        var hh = (c[10] & 0x1f).leadingZeros(2);
                        var dst = c[10] >> 5;
                        var dd = c[11] & 0x1f;
                        var dy = c[11] >> 5;       // sunday = 7, monday = 1
                        var mo = c[12] & 0x0f;
                        var yy = c[13] & 0x7f;
                        ch = 'EFR time '+ hh +':'+ mm +':'+ ss +' ('+ dst +') '+ ['mon','tue','wed','thu','fri','sat','sun'] [dy-1] +' '+
                           dd +' '+ ['jan','feb','mar','apr','may','jun','jul','aug','sep','oct','nov','dec'] [mo-1] +' 20'+ yy;
                        if (0) {
                           ch += ' | ';
                           for (var i = 4; i < c.length-2; i++) {
                              ch += c[i].toHex(-2) +' ';
                           }
                        }
                        ch += '\n';
                     }
                  } else {
                     if (!t.EFR2 || !(ulen == 0x13 && (c[4] & 0xf) == 0 && c[5] == 0x20)) {
                        ch = t.EFR2? 'EFR uncommon ' : 'EFR data ';
                        for (var i = 0; i < 4; i++) {
                           ch += c[i].toHex(-2) +' ';
                        }
                        ch += 'C='+ c[4].toHex(-2) +' ';
                        ch += 'A='+ c[5].toHex(-2) +' ';
                        ch += 'CI='+ c[6].toHex(-2) +' ';
                        for (var i = 7; i < c.length-2; i++) {
                           ch += c[i].toHex(-2) +' ';
                        }
                        ch += 'ck='+ c[c.length-2].toHex(-2) +' ';
                        ch += c[c.length-1].toHex(-2);
                        ch += ' |';
                        for (var i = 7; i < c.length-2; i++) {
                           ch = ch + ((c[i] >= 0x20 && c[i] < 0x7f)? String.fromCharCode(c[i]) : '.');
                        }
                        ch += '|\n';
                     }
                  }
               }
               t.EFR_ch = [];
               t.EFR_hdr_found = 0;
            }
         }
      }
      if (t.CHU) {
         ch = '';
      }
      //console.log('ASCII code=0x'+ code.toString(16) +' char=['+ ch +']');
   }

   return ch;
}

FSK_async.prototype.get_code = function() {
   return this.last_code;
}

FSK_async.prototype.check_bits = function(v) {
   var t = this;
   
   switch (t.stop_bits) {
   
   // N1.5 
   // ttt d4 d3 d2 d1 d0 ss (1.5 stop bits, 15 bits total, 0x7fff)
   case 1.5:
      //console.log('FSK_async nstop=1.5 check_bits=0x'+ v.toString(16));
      if ((v & 3) != 0) return false;  // 1 start bit = 00
      v >>= 2;
      t.last_code = 0;
      for (var i = 0; i < t.data_bits; i++) {
         var d = v & 3;
         if (d != 0 && d != 3) return false;    // data half-bits not the same
         t.last_code = (t.last_code >> 1) | (d? t.data_msb : 0);
         v >>= 2;
      }
      if ((v & 7) != 7) return false;  // 1.5 stop bits = 111
      v >>= 3;
      if (v != 0) return false;  // too many bits given
      break;
   
   // N0, N1, N2
   default:
      //console.log('FSK_async nstop='+ t.stop_bits +' check_bits=0x'+ v.toString(16));
      if ((v & 1) != 0) return false;  // 1 start bit = 0
      v >>= 1;
      t.last_code = 0;
      for (var i = 0; i < t.data_bits; i++) {
         t.last_code = (t.last_code >> 1) | ((v & 1)? t.data_msb : 0);
         v >>= 1;
      }
      if (t.parity_bits == 1) {
         v >>= 1;
      }
      if (t.stop_bits == 2) {
         if ((v & 3) != 3) return false;  // 2 stop bits = 11
         v >>= 2;
      } else
      if (t.stop_bits == 1) {
         if ((v & 1) != 1) return false;  // 1 stop bit = 1
         v >>= 1;
      }
      if (v != 0) return false;  // too many bits given
      break;
   }
   
   return true;
}

FSK_async.prototype.process_char = function(code, cb) {
   var t = this;
   var success = t.check_bits(code);
   var tally = 0;

   if (!success) {
      tally = -1;
      //console.log('fail all options: 0x'+ code.toString(16) +' -> 0x'+ t.c1.toString(16));
   } else {
      tally = 1;

      switch (t.last_code) {
         case t.LETTERS:
            t.shift = false;
            break;
            
         case t.FIGURES:
            t.shift = true;
            break;
            
         default:
            var chr = t.code_to_char(t.last_code, t.shift);
            //console.log('FSK_async code=0x'+ t.last_code.toString(16) +' sft='+ t.shift +' chr=0x'+ Math.abs(chr).toString(16) +' ['+ chr +']');
            if (chr < 0) {
               if (t.ITA2)
                  console.log('FSK_async missed this code: 0x'+ Math.abs(chr).toString(16));
               else
                  chr = '\u2612';   // ASCII non-printing
            } else {
               cb(chr);
            }
            break;
      }
   }

   return { success:success, tally:tally };
}
