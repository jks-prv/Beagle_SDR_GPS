// Copyright (c) 2022 John Seamons, ZL4VO/KF6VO

function Selcall(init, output_cb) {
   var t = this;

   console.log('FSK encoder: Selcall');

   t.dbg = 0;
   t.dump_always = 1;
   t.dump_non_std_len = 1;
   t.dump_no_eos = 0;
   t.zoom = 10;
   
   t.start_bit = 0;
   t.seq = 0;
   t.pkt = 0;
   t.MSG_LEN_MIN = 30;
   t.MSG_LEN_MAX = 80;
   t.synced = 0;

   t.sym_s = [    // sync & format
   //   xx0    xx1    xx2    xx3    xx4    xx5    xx6    xx7    xx8    xx9
      '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ',   // 00x
      '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ',   // 01x
      '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ',   // 02x
      '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ',   // 03x
      '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ',   // 04x
      '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ',   // 05x
      '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ',   // 06x
      '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ',   // 07x
      '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ',   // 08x
      '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ', '   ',   // 09x
      'RTN', '   ', '   ', '   ', 'Pr0', 'Pr1', 'Pr2', 'Pr3', 'Pr4', 'Pr5',   // 10x
      '   ', '   ', '   ', '   ', '   ', '   ', '   ', 'ARQ', '   ', '   ',   // 11x
      'SEL', '   ', 'ABQ', 'SMA', '   ', 'Pdx', '  *', 'EOS'                  // 12x
   ];
   
   t.DX  = 125;
   t.RX5 = 109;
   
   // fmt/cat seen w/ Selcall
   // 120 100  indiv
   // 123 100  semi-auto

   t.FMT_GEO_CALL = 102;
   t.FMT_DISTRESS = 112;
   t.FMT_GROUP_CALL = 114;
   t.FMT_ALL_STATIONS = 116;
   t.FMT_INDIV_CALL = 120;
   t.FMT_RESV = 121;
   t.FMT_INDIV_SEMI_AUTO = 123;

   t.CAT_ROUTINE = 100;
   t.CAT_SHIPS_BUSINESS = 106;      // per Rivet CCIR493.java
   t.CAT_SAFETY = 108;
   t.CAT_URGENCY = 110;
   t.CAT_DISTRESS = 112;
   
   // seen w/ Selcall
   // 123 100 121 110   sym-7 117-3                South China Sea Beacons (SCS-B) unencrypted position
   // 123 100 121 110   sym-22 117-3               Austravelnet
   // 123 100 102 100   sym-21 117 sym-2 117-2     Austravelnet

   t.CMD1_102 = 102;
   t.CMD1_POSITION = 121;
   
   // these are from DSC
   t.CMD1_FM_CALL = 100;
   t.CMD1_FM_DUPLEX_CALL = 101;
   t.CMD1_POLLING = 103;
   t.CMD1_UNABLE_COMPLY = 104;
   t.CMD1_END_OF_CALL = 105;
   t.CMD1_F1B_DATA = 106;
   t.CMD1_J3E_RT = 109;
   t.CMD1_DISTRESS_ACK = 110;
   t.CMD1_DISTRESS_ALERT_RELAY = 112;
   t.CMD1_F1B_FEC = 113;
   t.CMD1_F1B_ARQ = 115;
   t.CMD1_TEST = 118;
   t.CMD1_NOP = 126;

   t.CMD2_100 = 100;
   t.CMD2_110 = 110;

   // these are from DSC
   t.CMD2_UNABLE_FIRST = 100;
   t.CMD2_BUSY = 102;
   t.CMD2_UNABLE_LAST = 109;
   t.CMD2_NON_CONFLICT = 110;
   t.CMD2_MED_TRANSPORTS = 111;
   t.CMD2_NOP = 126;
   
   t.ARQ = 117;
   t.ABQ = 122;
   t.EOS = 127;
   
   t.NOP = 126;
   
   t.pos_s = [
   /*
      // 6-digit A1|A2|A3 => I1|I2|I3
      'dx', 'rx5', 'dx', 'rx4', 'dx', 'rx3', 'dx', 'rx2', 'dx', 'rx1', 'dx', 'rx0',
      'fmt', 'fmt', 'A1', 'fmt', 'A2', 'fmt', 'A3', 'A1', 'cat', 'A2',
      'I1', 'A3', 'I2', 'cat', 'I3', 'I1',
      'eos', 'I2', 'eos', 'I3', 'eos', 'eos'
   */

      // 4-digit B1|B2 => A1|A2
      'dx', 'rx5', 'dx', 'rx4', 'dx', 'rx3', 'dx', 'rx2', 'dx', 'rx1', 'dx', 'rx0',
      'fmt', 'fmt', 'B1', 'fmt', 'B2', 'fmt', 'cat', 'B1',
      'A1', 'B2', 'A2', 'cat',
      'eos', 'A1', 'eos', 'A2', 'eos', 'eos'
   ];
   
   // symbolic position lookup
   t.pos = {};
   t.pos_n = {};
   t.pos_s.forEach(function(pos_s, i) {
      pos_s = pos_s.trim();
      if (!isDefined(t.pos_n[pos_s])) t.pos_n[pos_s] = 0;
      t.pos[pos_s +'-'+ t.pos_n[pos_s].toString()] = i;
      t.pos_n[pos_s]++;
   });
   if (t.dbg) console.log(t.pos);
   
   var edc = function(v) {
      var bc = kiwi_bitCount(v ^ 0x7f);   // count # of zeros
      return kiwi_bitReverse(bc, 3);
   };

   t.DX_w = (edc(t.DX) << 7) | t.DX;
   t.svec = [];
   var rx = t.RX5;
   for (var i = 0; i < 12; i++) {
      t.svec[i] = (i&1)? ((edc(rx) << 7) | rx) : t.DX_w;
      if (i&1) rx--;
   }
   if (t.dbg) console.log(t.svec);
}

Selcall.prototype.sym_by_sym_name = function(sym_name) {
   var t = this;
   var pos = t.pos[sym_name];
   //console.log(sym_name +'=@'+ pos +'='+ t.syms[pos]);
   return { /*name:sym_name,*/ pos:pos, sym:t.syms[pos] };
}
   
Selcall.prototype.output_msg = function(s) {
   var t = this;
   //toUTCString().substr(5,20)
   return ((new Date()).toUTCString().substr(17,8) +' '+ (ext_get_freq()/1e3).toFixed(2) +' '+ s +'\n');
}
   
Selcall.prototype.code_to_char = function(code) {
   
   var t = this;
   if (code < 0 || code > 127) return '???';
   var sym = t.sym_s[code];
   //if (sym == '   ') return '['+ code +']';
   return sym;
}

Selcall.prototype.reset = function() {
   var t = this;
   t.syncA = t.syncB = t.syncC = t.syncD = t.syncE = 0;
}

Selcall.prototype.get_nbits = function() {
   return 10;
}

Selcall.prototype.get_msb = function() {
   return 0x200;
}

Selcall.prototype.check_bits = function() {
   return false;
}

Selcall.prototype.search_sync = function(bit) {
   var t = this;
   var cin, cout;

   cin = bit;
   cout = t.syncA & 1;
   t.syncA = (t.syncA >> 1) & 0x3ff;
   t.syncA |= cin? 0x200:0;

   cin = cout;
   cout = t.syncB & 1;
   t.syncB = (t.syncB >> 1) & 0x3ff;
   t.syncB |= cin? 0x200:0;

   cin = cout;
   cout = t.syncC & 1;
   t.syncC = (t.syncC >> 1) & 0x3ff;
   t.syncC |= cin? 0x200:0;

   cin = cout;
   cout = t.syncD & 1;
   t.syncD = (t.syncD >> 1) & 0x3ff;
   t.syncD |= cin? 0x200:0;

   cin = cout;
   cout = t.syncE & 1;
   t.syncE = (t.syncE >> 1) & 0x3ff;
   t.syncE |= cin? 0x200:0;
   
   if (0) {
      console.log(bit +' '+
         t.syncA.toString(2).leadingZeros(10) + paren(t.syncA.toHex(-3)) +'|'+
         t.syncB.toString(2).leadingZeros(10) + paren(t.syncB.toHex(-3)) +'|'+
         t.syncC.toString(2).leadingZeros(10) + paren(t.syncC.toHex(-3)) +'|'+
         t.syncD.toString(2).leadingZeros(10) + paren(t.syncD.toHex(-3)) +'|'+
         t.syncE.toString(2).leadingZeros(10) + paren(t.syncE.toHex(-3)));
      console.log('> '+
         t.svec[1].toString(2).leadingZeros(10) + paren(t.svec[1].toHex(-3)) +'|'+
         (0+0).toString(2).leadingZeros(10) + paren((0+0).toHex(-3)) +'|'+
         t.svec[3].toString(2).leadingZeros(10) + paren(t.svec[3].toHex(-3)) +'|'+
         (0+0).toString(2).leadingZeros(10) + paren((0+0).toHex(-3)) +'|'+
         t.svec[5].toString(2).leadingZeros(10) + paren(t.svec[5].toHex(-3)));
   }

   for (var i = 0; i <= (12-3); i++) {
      if (t.syncC == t.svec[i] && t.syncB == t.svec[i+1] && t.syncA == t.svec[i+2]) {
         if (1 || t.dbg) console.log('SYNC-DRD-'+ i);
         t.seq = i+3;
         t.full_syms = [];
         t.syms = [];
         t.bc_err = [];
         t.parity_errors = 0;
         t.FEC_errors = 0;
         t.synced = 1;
         return true;
      }
   }
   for (var i = 1; i <= 7; i += 2) {
      if (t.syncE == t.svec[i] && t.syncC == t.svec[i+2] && t.syncA == t.svec[i+4]) {
         if (1 || t.dbg) console.log('SYNC-RRR-'+ i);
         t.seq = i+5;
         t.full_syms = [];
         t.syms = [];
         t.bc_err = [];
         t.parity_errors = 0;
         t.FEC_errors = 0;
         t.synced = 1;
         return true;
      }
   }
   
   t.synced = 0;
   return false;
}

Selcall.prototype.process_char = function(_code, fixed_start, output_cb, show_raw, show_errs) {
   var t = this;
   if (!t.synced) return { resync:0 };
   if (t.seq == 12) t.pkt++;
   
   // test
   var test = false;
   if (test && t.pkt == 1) {
      //if (t.seq == 18) _code = 0;
      //if (t.seq == 18+5) _code = 0;
      //if (t.seq == 24) _code = 0;
      //if (t.seq == 24+5) _code = 0;
      if (t.seq == 30) _code = 0;
      if (t.seq == 30+5) _code = 0;
   }
   
   t.full_syms[t.seq] = _code;
   var bc_rev = (_code >> 7) & 7;
   var code = _code & 0x7f;
   t.syms[t.seq] = code;
   var pos_s = t.pos_s[t.seq];
   pos_s = pos_s || 'unk';
   var chr = t.code_to_char(code);
   
   // verify #zeros count
   var bc_ck = kiwi_bitReverse(bc_rev, 3);
   var bc_data = kiwi_bitCount(code ^ 0x7f);
   var pe = (bc_ck != bc_data);
   var zc = pe? (' Zck='+ bc_ck +' Zdata='+ bc_data) : '';
   t.bc_err[t.seq] = pe;
   if (pe) t.parity_errors++;

   if (t.dbg) console.log(t.seq.leadingZeros(2) +' '+ pos_s.fieldWidth(6) +': '+ chr +' '+ code.fieldWidth(3) +'.'+
      ' '+ bc_rev.toString(2).leadingZeros(3) +' '+ code.toString(2).leadingZeros(7) + zc);
   t.seq++;

   // detect variable length EOS
   var eos = false;

   if (t.seq >= t.MSG_LEN_MIN) {
      var eos_n = 0, s;
      
      var eos_ck = function(eos_sym) {
         var eos_n = 0;
         if (!t.bc_err[t.seq-1] && t.syms[t.seq-1] == eos_sym) eos_n++;
         if (!t.bc_err[t.seq-2] && t.syms[t.seq-2] == eos_sym) eos_n++;
         if (!t.bc_err[t.seq-4] && t.syms[t.seq-4] == eos_sym) eos_n++;
         if (!t.bc_err[t.seq-6] && t.syms[t.seq-6] == eos_sym) eos_n++;
         if (eos_n >= 3) console.log('eos_n('+ eos_sym +')='+ eos_n);
         return (eos_n >= 3);
      }
      
      var color = function(_color, s) {
         return (_color + s + ansi.NORM);
      };

      var url_lat_lon = function(lat_dd, lon_dd) {
         return 'https://www.marinetraffic.com/en/ais/home/centerx:'+ lon_dd +'/centery:'+ lat_dd +'/zoom:'+ t.zoom;
      }

      var link_lat_lon = function(lat_lon_s, lat_dd, lon_dd) {
         return w3_link('w3-esc-html w3-link-darker-color', url_lat_lon(lat_dd, lon_dd), lat_lon_s);
      }

      var fec = function(i) {
         var rv = { code: t.full_syms[i] & 0x7f, fec_err: false };

         if (t.bc_err[i]) {
            if (i+5 < t.seq) {   // apply FEC
               if (!t.bc_err[i+5]) {
                  rv.code = t.full_syms[i+5] & 0x7f;
                  if (test) console.log('FEC: i='+ i +' alt code='+ rv.code);
                  return rv;
               } 
            }
            rv.code = -1;
            rv.fec_err = true;
            if (test) console.log('FEC: i='+ i +' FAIL');
         }
         
         return rv;
      };
      
      if (eos_ck(t.EOS) || eos_ck(t.ARQ) || eos_ck(t.ABQ)) eos = true;

      if (eos || t.seq > t.MSG_LEN_MAX) {
         var dump = t.dump_always;

         if (eos) {
            if (t.seq != t.MSG_LEN_MIN) {
               //if (show_errs) output_cb(t.output_msg(color(ansi.BLUE, 'non-std len='+ t.seq)));
               console.log('$$ non-std len='+ t.seq);
               dump |= t.dump_non_std_len;
            }
            //output_cb(t.process_msg(show_errs));
         } else {
            var pe = t.parity_errors? (' '+ color(ansi.BLUE, (t.parity_errors +' PE'))) : '';
            //if (show_errs) output_cb(t.output_msg(color(ansi.BLUE, 'no EOS') + pe));
            console.log('$$ no EOS pe='+ t.parity_errors);
            dump |= t.dump_no_eos;
         }

         if (dump) {
            var dump_line = 1;
            var fec_errors = false;
            var color_n = -1, color_s;
            var prev_len = 0;
            var sym = [];
            var freq = ext_get_freq() / 1e3;
            var raw = (new Date()).toUTCString().substr(17,8) +' '+ freq.toFixed(2) +' ';
            var fmt = fec(12).code;
            var cat, cmd1, cmd2;
            
            var Austravel_net_match = false;
            if (ext_get_name() == 'NAVTEX') {
               var Austravel_net = getVarFromString('nt.freqs.Selcall.Austravel_net');
               if (isArray(Austravel_net)) {
                  Austravel_net.forEach(function(ae, i) {
                     if (Austravel_net_match) return;
                     if (Math.abs(freq - parseInt(ae)) < 1)
                        Austravel_net_match = true;
                  });
               }
            }

            var call_decode = function(call, call_s) {
               //console.log('Austravel_net_match='+ Austravel_net_match +' call='+ call);
               if (!Austravel_net_match) return call_s;
               var base = '';
               switch (call) {
                  case '2199': base = '(Austravel Casino NSW)'; break;
                  case '3199': base = '(Austravel Shepparton VIC)'; break;
                  case '4199': base = '(Austravel Mareeba North QLD)'; break;
                  case '5199': base = '(Austravel Base SA)'; break;
                  case '6199': base = '(Austravel Perth WA)'; break;
                  case '6299': base = '(Austravel Kununnura WA)'; break;
                  case '8199': base = '(Austravel Alice Springs NT)'; break;
                  /*
                  case '1234': base = '(Test1)'; break;
                  case '5678': base = '(Test2)'; break;
                  */
               }
               return w3_sb(call_s, base);
            };

            for (var i = 0; i < t.seq; i++) {
               var fec_err = false;
               var _code = t.full_syms[i];
               if (isUndefined(_code)) _code = 119;   // sync
               var code = _code & 0x7f;
               var pos_s = t.pos_s[i];
               pos_s = pos_s || 'unk';
               
               if (dump_line) {
                  if (i & 1 || i < 12) continue;      // skip FEC and phasing

                  var rv = fec(i);
                  code = rv.code;
                  if (rv.fec_err) fec_errors = true;

                  // 4-digit calls:
                  //   1  1  1   1  2  2   2   2  2  3
                  //   2  4  6   8  0  2   4   6  8  0
                  // 123 68 68 100 99 80 121 110 01 60 ...
                  //console.log(i +':'+ code);
                  
                  // format 4-digit calls to look like 6-digit
                  if (i == 14) {
                     cat = fec(18).code;
                     if (cat > 99) {
                        sym.push('  ');
                        raw += '__ ';
                     } else {
                        cat = fec(20).code;
                     }
                  }
                  if (i == 20) {
                     cmd1 = fec(24).code;
                     cmd2 = fec(26).code;
                     if (cmd1 > 99) {
                        sym.push('  ');
                        raw += '__ ';
                     } else {
                        cmd1 = fec(28).code;
                        cmd2 = fec(30).code;
                     }
                  }

                  var code_s;
                  if (rv.fec_err) {
                     raw += 'X ';
                     code_s = '0';
                  } else {
                     code_s = (code <= 99)? code.leadingZeros(2) : code.toString();
                     if (prev_len != code_s.length) {
                        color_n = (color_n + 1) % ansi.rolling_n;
                        color_s = ansi[ansi.rolling[color_n]];
                        prev_len = code_s.length;
                     }
                     raw += color_s + code_s + ansi.NORM +' ';
                  }

                  sym.push(code_s);
               } else {
                  var chr = t.code_to_char(code);
                  var bc_rev = (_code >> 7) & 7;
                  var zc = '';
                  if (t.bc_err[i]) {
                     var bc_ck = kiwi_bitReverse(bc_rev, 3);
                     var bc_data = kiwi_bitCount(code ^ 0x7f);
                     var pe = (bc_ck != bc_data);
                     zc = ' Zck='+ bc_ck +' Zdata='+ bc_data;
                  }
                  console.log(i.leadingZeros(2) +' '+ pos_s.fieldWidth(6) +': '+ chr +' '+ code.fieldWidth(3) +'.'+
                     ' '+ bc_rev.toString(2).leadingZeros(3) +' '+ code.toString(2).leadingZeros(7) + zc);
               }
            }
            
            if (dump_line) {
               var deco = null;
               if (fec_errors) {
                  raw += color(ansi.RED, 'FEC');
               } else {
                  var isCall = (fmt == t.FMT_INDIV_CALL || fmt == t.FMT_INDIV_SEMI_AUTO);
                  
                  // 6-digit calls:
                  //                                    1  1
                  //   0  1  2  3   4  5  6  7   8   9  0  1
                  // 123 00 68 68 100 01 99 80 121 110 01 60 ...

                  if (isCall) {
                     //  0  1  2  3  4  5  6    sym[i+10]
                     // 01 23 45 67 89 01 23    s3.slice()
                     // 01 61 41 10 31 20 38    16.14 110.31 20:38
                     var call_to, call_to_s, call_from, call_from_s;
                     if (+sym[1]) {
                        call_to = sym[1] + sym[2] + sym[3];
                        call_to_s = color(ansi.CYAN, call_to);
                     } else {
                        call_to = sym[2] + sym[3];
                        call_to_s = color(ansi.CYAN, '__'+ call_to);
                     }
                     if (+sym[5]) {
                        call_from = sym[5] + sym[6] + sym[7];
                        call_from_s = color(ansi.YELLOW, call_from);
                     } else {
                        call_from = sym[6] + sym[7];
                        call_from_s = color(ansi.YELLOW, '__'+ call_from);
                     }
                     deco = call_decode(call_from, call_from_s) +' calling '+ call_decode(call_to, call_to_s);
                  }
                  
                  // SCS-B unencrypted position format, 4 and 6-digit calls
                  if (isCall && cmd1 == t.CMD1_POSITION && cmd2 == t.CMD2_110 &&
                     t.seq >= 48 && t.seq <= 56) {

                     var s = '';
                     for (i = 0; i <= 6; i++) s += sym[i+10];
                     
                     // [30.81,329.40] 01:63
                     //  0  1  2  3  4  5  6
                     // 01 23 45 67 89 01 23    s3.slice()
                     // 03 08 13 29 40 01 63   -30.81, 129.40   81/
                     
                     // 56 42 55 59 46 40 42 05 58 51 38 59 38 46 49 38 39 49 42 117 35 22 117 117 117
                     //  8  *  7  ;  .  (  *     :  3  &  ;  &  .  1  &  '  1  *      # syn
                     // 

                     var lat_d = s.slice(1,3);
                     var lon_d = s.slice(5,8);
                     var lat_m = s.slice(3,5);
                     var lon_m = s.slice(8,10);

                     var lat_dd = lat_d +'.'+ lat_m;
                     var lon_dd = lon_d +'.'+ lon_m;
                     deco += ', position '+ link_lat_lon('['+ lat_dd +','+ lon_dd +']', +lat_dd, +lon_dd);
                     //deco += link_lat_lon('*', -lat_dd, +lon_dd) +' ';
                     
                     var bad_min = false;
                     var lat_min = +lat_m;
                     var lon_min = +lon_m;
                     if (lat_min >= 60) { lat_min = 59; bad_min = true; }
                     if (lon_min >= 60) { lon_min = 59; bad_min = true; }
                     var lat_dm = lat_d +'\u00b0'+ lat_m +"'";
                     var lon_dm = lon_d +'\u00b0'+ lon_m +"'";
                     var lat = lat_d + (lat_min/60).toFixed(2).slice(1);
                     var lon = lon_d + (lon_min/60).toFixed(2).slice(1);
                     deco += ' '+ link_lat_lon('['+ lat_dm +','+ lon_dm +']', +lat, +lon);
                     //deco += link_lat_lon('[-lat]', -lat, +lon) +' ';

                     deco += ' '+ s.slice(10,12) +':'+ s.slice(12,14);     // hh:mm
                     
                     var dupe = false;
                     if (ext_get_name() == 'NAVTEX')
                        dupe = navtex_location_update(call_from, +lat, +lon, url_lat_lon(+lat, +lon),
                           bad_min? [ 'white', 'red' ] : [ 'white', (freq < 7500)? 'magenta' : 'blue' ]);
                     if (dupe)
                        deco += ' '+ color(ansi.YELLOW, '(dupe)');
                  }
                  
                  if (deco) deco = (new Date()).toUTCString().substr(17,8) +' '+ freq.toFixed(2) +' '+ deco;
               }
               console.log('fec_errors='+ fec_errors +' show_errs='+ show_errs +' seq='+ t.seq);
               if (!fec_errors && deco) {
                  output_cb(deco +'\n');
               }
               if ((!fec_errors && (!deco || show_raw)) || (fec_errors && show_errs)) {
                  output_cb(raw +'\n');
               }
            }
         }

         t.synced = 0;
         return { resync:1 };
      }
   }
   
   return { resync:0 };
}
