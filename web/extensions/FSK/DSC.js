// Copyright (c) 2022 John Seamons, ZL4VO/KF6VO

function DSC(init, output_cb) {
   var t = this;

   console.log('FSK encoder: DSC');

   if (init) {
      output_cb('\f'+ ansi.RED +'WARNING: Do not use the DSC decoder in any life-safety application. Hobby radio monitoring only.\n'+ ansi.NORM +
         'No distress messages are currently decoded. See the KiwiSDR forum for details.\n\n');
   }

   t.dbg = 0;
   t.test_msgs = 0;
   t.dump_eos = 0;
   
   t.start_bit = 0;
   t.seq = 0;
   t.MSG_LEN_MIN = 62;
   t.MSG_LEN_MAX = 80;
   t.FIRST_CALL = true;
   t.POS_REQUEST = true;
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
      '   ', '   ', 'GEO', '   ', 'Pr0', 'Pr1', 'Pr2', 'Pr3', 'Pr4', 'Pr5',   // 10x
      'Pr6', 'Pr7', '*D*', '   ', 'INT', '   ', 'ALL', 'ARQ', '   ', '   ',   // 11x
      'STA', 'RSV', 'ABQ', 'ATO', '   ', 'Pdx', '  *', 'EOS'                  // 12x
   ];
   
   t.DX  = 125;
   t.RX7 = 111;
   
   t.FMT_GEO_AREA = 102;
   t.FMT_DISTRESS = 112;
   t.FMT_COMMON_INTEREST = 114;
   t.FMT_ALL_SHIPS = 116;
   t.FMT_INDIV_STA = 120;
   t.FMT_RESV = 121;
   t.FMT_IS_SEMI_AUTO = 123;

   t.CAT_ROUTINE = 100;
   t.CAT_SAFETY = 108;
   t.CAT_URGENCY = 110;
   t.CAT_DISTRESS = 112;
   
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
   t.CMD1_POSITION = 121;
   t.CMD1_NOP = 126;

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
   
   // sync  fmt    to  cat  from  cmd1  cmd2  freq1  freq2  EOS  ECC    nominal format (no var fields shown)
   //         A  B1-5    C  D1-5    E1    E2   F1-3   G1-3    H    I
   //         4     2    2     2     2     2      2      2    4    2    FEC repeat count
   //   14    4    10    2    10     2     2      6      6    4    2    = 62 (MSG_LEN_MIN)
   t.ecc_sym = [
      'fmt', 'to1', 'to2', 'to3', 'to4', 'to5', 'cat', 'from1', 'from2', 'from3', 'from4', 'from5',
      'cmd1', 'cmd2', 'freq1.1', 'freq1.2', 'freq1.3', 'freq2.1', 'freq2.2', 'freq2.3', 'eos'
   ];
   
   t.pos_s = [
      ' dx', 'rx7', ' dx', 'rx6', ' dx', 'rx5', ' dx', 'rx4', ' dx', 'rx3', ' dx', 'rx2',
      'Afmt', 'rx1', 'Afmt', 'rx0',
      'Bto1', 'Afmt', 'Bto2', 'Afmt', 'Bto3', 'Bto1', 'Bto4', 'Bto2', 'Bto5', 'Bto3',
      'Cat', 'Bto4', 'Dfr1', 'Bto5', 'Dfr2', 'Cat',
      'Dfr3', 'Dfr1', 'Dfr4', 'Dfr2', 'Dfr5', 'Dfr3',
      'E1cmd', 'Dfr4', 'E2cmd', 'Dfr5',
      'Ffreq1', 'E1cmd', 'Ffreq2', 'E2cmd', 'Ffreq3', 'Ffreq1',
      'Gfreq1', 'Ffreq2', 'Gfreq2', 'Ffreq3', 'Gfreq3', 'Gfreq1',
      'eos', 'Gfreq2', 'ecc', 'Gfreq3', 'eos', 'eos', 'eos', 'ecc'
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
   var rx = t.RX7;
   for (var i = 0; i < 12; i++) {
      t.svec[i] = (i&1)? ((edc(rx) << 7) | rx) : t.DX_w;
      if (i&1) rx--;
   }
   //console.log(t.svec);

   t.format_s = {
      102: 'geo-area', 112: '*distress*', 114: 'com-int', 116: 'all-ships', 120: 'indiv-sta', 121: 'resv', 123: 'semi-auto'
   };
   
   t.category_s = {
      100: 'routine', 108: 'safety', 110: 'urgency', 112: '*distress*'
   };
   
   t.MID_area = [ 'Europe', 'N.AM/C.AM', 'Asia', 'Oceania', 'Africa', 'S.AM' ];
   
   // tests (ecc calc is suppressed)
   // FSK.DSC1.12k.au file is nominally: 4.7 SAFETY Test ack, 002191000[Coast Station: Denmark, Lyngby] => 244170080[Flag: Netherlands]
   t.tlist = null;
   if (t.test_msgs) {
      var GRP = { s:'Afmt', n:t.FMT_COMMON_INTEREST };
      var ALL = { s:'Afmt', n:t.FMT_ALL_SHIPS };
      var GEO = { s:'Afmt', n:t.FMT_GEO_AREA };
      var IS = { s:'Afmt', n:t.FMT_INDIV_STA };
      var F_DIS = { s:'Afmt', n:t.FMT_DISTRESS };

      var C_DIS = { s:'Cat', n:t.CAT_DISTRESS };
      var URG = { s:'Cat', n:t.CAT_URGENCY };
      var SAF = { s:'Cat', n:t.CAT_SAFETY };
      var ROU = { s:'Cat', n:t.CAT_ROUTINE };

      var FM_RT = { s:'E1cmd', n:t.CMD1_FM_CALL };
      var J3E_RT = { s:'E1cmd', n:t.CMD1_J3E_RT };
      var FSK_FEC = { s:'E1cmd', n:t.CMD1_F1B_FEC };
      var POS = { s:'E1cmd', n:t.CMD1_POSITION };
      var TST = { s:'E1cmd', n:t.CMD1_TEST };

      var MED = { s:'E2cmd', n:t.CMD2_MED_TRANSPORTS };

      var F1 = { s:'Ffreq1', n:02 };
      var F2 = { s:'Ffreq2', n:34 };
      var F3 = { s:'Ffreq3', n:56 };
      var G1 = { s:'Ffreq1', n:12 };
      var G2 = { s:'Ffreq2', n:78 };
      var G3 = { s:'Ffreq3', n:90 };

      var EOS = { s:'eos', n:t.EOS };
      var ARQ = { s:'eos', n:t.ARQ };
      var ABQ = { s:'eos', n:t.ABQ };
      t.tlist = [];

      // 4.1 distress alert
      //t.tlist.push([ F_DIS, C_DIS, EOS ]);
      // 4.7 self-test (EOS instead of ARQ/ABQ)
      t.tlist.push([ IS, SAF, TST, { s:'Bto1', n:00 }, { s:'Bto2', n:36 }, { s:'Bto3', n:69 }, { s:'Bto4', n:99 }, { s:'Bto5', n:30 }, EOS ]);
      // 4.7 FM call req
      t.tlist.push([ IS, SAF, FM_RT, { s:'Bto1', n:97 }, { s:'Bto2', n:41 }, { s:'Bto3', n:90 }, { s:'Bto4', n:12 }, { s:'Bto5', n:30 }, ARQ ]);
      // 4.9 FM call ack
      t.tlist.push([ IS, ROU, FM_RT, { s:'Bto1', n:97 }, { s:'Bto2', n:51 }, { s:'Bto3', n:90 }, { s:'Bto4', n:12 }, { s:'Bto5', n:30 }, ABQ ]);
      // 4.7 position request ("pos3")
      t.tlist.push([ IS, SAF, POS, { s:'Ffreq1', n:55 }, { s:'Ffreq2', n:21 }, { s:'Ffreq3', n:23 }, { s:'Gfreq1', n:41 }, { s:'Gfreq2', n:56 }, { s:'Gfreq3', n:78 }, ARQ ]);
      // 4.7 position request ack (special "pos4" position msg)
      t.tlist.push([ IS, SAF, POS, { s:'Ffreq1', n:21 }, { s:'Ffreq2', n:23 }, { s:'Ffreq3', n:41 }, { s:'Gfreq1', n:56 }, { s:'Gfreq2', n:78 }, { s:'Gfreq3', n:t.NOP }, ABQ ]);
      // 4.9 call ack w/ position ("pos2")
      t.tlist.push([ IS, ROU, J3E_RT, { s:'Ffreq1', n:55 }, { s:'Ffreq2', n:21 }, { s:'Ffreq3', n:23 }, { s:'Gfreq1', n:41 }, { s:'Gfreq2', n:56 }, { s:'Gfreq3', n:78 }, ABQ ]);
      // 4.5 from => to:all-ships
      t.tlist.push([ ALL, SAF, J3E_RT, F1, F2, F3, EOS ]);
      // 4.6 from => to:geo
      t.tlist.push([ GEO, URG, J3E_RT, MED, { s:'Bto1', n:21 }, { s:'Bto2', n:21 }, { s:'Bto3', n:34 }, { s:'Bto4', n:02 }, { s:'Bto5', n:03 }, F1, F2, F3, EOS ]);
      // 4.7 freq
      t.tlist.push([ IS, SAF, J3E_RT, F1, F2, F3, G1, G2, G3, ARQ ]);
      // 4.8 from => to:ship-group
      t.tlist.push([ GRP, ROU, FSK_FEC, { s:'Bto1', n:02 }, { s:'Bto2', n:19 }, { s:'Bto3', n:00 }, { s:'Bto4', n:00 }, { s:'Bto5', n:00 }, F1, F2, F3, EOS ]);
      // 4.9 unable
      t.tlist.push([ IS, ROU, { s:'E1cmd', n:t.CMD1_UNABLE_COMPLY }, { s:'E2cmd', n:t.CMD2_UNABLE_FIRST }, F1, F2, F3, ABQ ]);
   }
   
   t.init_MID_MMSI();
}

DSC.prototype.sym_by_sym_name = function(sym_name) {
   var t = this;
   var pos = t.pos[sym_name];
   //console.log(sym_name +'=@'+ pos +'='+ t.syms[pos]);
   return { /*name:sym_name,*/ pos:pos, sym:t.syms[pos] };
}
   
DSC.prototype.output_msg = function(s) {
   var t = this;
   //toUTCString().substr(5,20)
   return ((new Date()).toUTCString().substr(17,8) +' '+ (ext_get_freq()/1e3).toFixed(2) +' '+ s +'\n');
}
   
DSC.prototype.process_msg = function(show_errs) {
   var t = this;
   var i, s;
   var ecc_ck = [];
   t.test = (t.tlist && t.tlist.length)? t.tlist[0] : null;
   
   var check = function(sym_name, repeats, id) {
      
      if (t.test) {
         var hit = -1;
         t.test.forEach(function(a, i) {
            //console.log(sym_name +'|'+ a.s);
            if (sym_name == a.s) {
               hit = i;
            }
         });
         if (hit != -1) {
            var sym = t.test[hit].n;
            console.log('TEST '+ sym_name +'='+ sym);
            return { n:sym, sym_s:sym.toString(), z:sym.leadingZeros(2), nop:(sym == t.NOP), err:0 };
         }
      }
      
      // only consider symbols that don't have bit count errors
      var i, ck = [];
      for (i = 0; i < repeats; i++) {
         var sym = t.sym_by_sym_name(sym_name +'-'+ i);
         if (!t.bc_err[sym.pos]) ck.push(sym);
      }
      var empty = (ck.length == 0), err = 0;

      if (!empty) {
         // FIXME: use voting logic here when ck.length >= 3?
         var first = ck[0];
         for (i = 1; i < ck.length; i++) {
            var subsq = ck[i];
            if (first.sym != subsq.sym) {
               //console.log('FEC FAIL: '+ sym_name + paren(id) +
               //   ' [0@'+ first.pos  +']='+ first.sym +'. ['+ i +'@'+ subsq.pos +']='+ subsq.sym +'.');
               err = 1;
            }
         }
      }
      
      if ((1 || t.dbg) && (empty || err)) {
         var s = '';
         if (empty) s += '(ALL Zerr) ';
         for (i = 0; i < repeats; i++) {
            var sn = sym_name +'-'+ i;
            var sym = t.sym_by_sym_name(sn);
            s += sn +':'+ (t.bc_err[sym.pos]? 'Zerr' : sym.sym) +' ';
         }
         t.FEC_errors++;
         if (t.dbg) console.log('FEC FAIL: '+ id +' '+ s);
      }

      if (empty) return { n:-1, err:1 };
      //console.log(sym_name +':'+ repeats);
      //console.log(ck);
      var sym = ck[0].sym;
      if (isUndefined(sym)) console.log(ck);
      return { n: err? -1 : sym, sym_s:sym.toString(), z:sym.leadingZeros(2), nop:(sym == t.NOP), err:err };
   };
   
   var format = function() {
      var fmt = check('Afmt', 4, 'format');
      ecc_ck.push(fmt.n);
      fmt.s = fmt.err? 'fmtFEC': t.format_s[fmt.sym_s];
      fmt.s = fmt.s || (fmt.n +'?');
      return fmt;
   };
   
   var category = function() {
      var cat = check('Cat', 2, 'category');
      ecc_ck.push(cat.n);
      cat.s = cat.err? 'catFEC' : t.category_s[cat.sym_s];
      cat.s = cat.s || (cat.n +'?');
      return cat;
   };
   
   var mid = function(mid) {     // mid(
      //console.log('MID: mid='+ mid +' => '+ t.MID[mid]);
      return t.MID[mid] || '(unknown)';
   }

   var mmsi_coast = function(mmsi_7) {    // mmsi_coast(
      //console.log('MMSI: mmsi_7='+ mmsi_7 +' => '+ t.MMSI_coast[mmsi_7]);
      return t.MMSI_coast[mmsi_7] || '(unknown)';
   }

   var call = function(sym_name, fmt, id) {     // call(
      var c12 = check(sym_name +'1', 2, id);
      var c34 = check(sym_name +'2', 2, id);
      var c56 = check(sym_name +'3', 2, id);
      var c78 = check(sym_name +'4', 2, id);
      var c90 = check(sym_name +'5', 2, id);    // tenth digit always supposed to be zero
      ecc_ck.push(c12.n);
      ecc_ck.push(c34.n);
      ecc_ck.push(c56.n);
      ecc_ck.push(c78.n);
      ecc_ck.push(c90.n);

      if (c12.err || c34.err || c56.err || c78.err || c90.err) return { s:'callFEC', err:1 };
      var _12 = c12.z;
      var _34 = c34.z;
      var _56 = c56.z;
      var _78 = c78.z;
      var _90 = c90.z;
      var _56789 = _56 + _78 + _90[0];
      var mmsi_s = _12 + _34 + _56789;

      var iden1 = +_12[0];
      var iden2 = +_12[1];
      var iden3 = +_34[0];
      var isMID1 = (iden1 >= 2 && iden1 <= 7);
      var isMID2 = (iden2 >= 2 && iden2 <= 7);
      var isMID3 = (iden3 >= 2 && iden3 <= 7);

      var s, format = fmt.n;
      
      // FMT_GEO_AREA only applies to "to" address
      if (format == t.FMT_GEO_AREA && sym_name == 'Dfr') format = t.FMT_INDIV_STA;

      // FMT_ALL_SHIPS only applies to "to" address
      if (format == t.FMT_ALL_SHIPS && sym_name == 'Dfr') format = t.FMT_INDIV_STA;

      // FMT_COMMON_INTEREST (group call) only applies to "to" address
      if (format == t.FMT_COMMON_INTEREST && sym_name == 'Dfr') format = t.FMT_INDIV_STA;

      switch (format) {
      
      case t.FMT_INDIV_STA:
         s = '';
         
         // ship: MID xx xx xx
         //       123 45 67 89
         if (isMID1) {
            var mid_s = _12 + _34[0];
            s = w3_link('w3-esc-html w3-link-darker-color', 'www.marinetraffic.com/en/ais/details/ships/mmsi:'+ mmsi_s, mmsi_s) +'[Flag: '+ mid(mid_s) +']';
         } else
      
         // coast station: 00 MID xx xx
         //                12 345 67 89
         if (_12 == '00' && isMID3) {
            var mid_s = _34 + _56[0];
            var mmsi_7 = _34 + _56789;
            s = mmsi_s +'[Coast Station: '+ mid(mid_s) +', '+ mmsi_coast(mmsi_7) +']';
         } else
         
         // ship group: 0 MID x xx xx
         //             1 234 5 67 89
         if (iden1 == 0 && isMID2) {
            var mid_s = _12[1] + _34;
            s = mmsi_s +'[Ship group: '+ mid(mid_s) +']';
         } else
         
         // NAVAID: 99 MID xx xx
         //         12 345 67 89
         if (_12 == '99' && isMID3) {
            var mid_s = _34 + _56[0];
            s = mmsi_s +'[NAVAID: '+ mid(mid_s) +']';
         } else
         
         if (_12 == '98' && isMID3) {
            var mid_s = _34 + _56[0];
            s = mmsi_s +'[Associated craft: '+ mid(mid_s) +']';
         } else
         
         if (_12 == '97') {
            var _3 = +_34[0];
            if (_3 == 0) s = '[SAR transponder]'; else
            if (_3 == 2) s = '[MOB device]'; else
            if (_3 == 4) s = '[EPIRB]';
            if (s != '') s = mmsi_s + color(ansi.YELLOW, s);
         } else
         
         if (iden1 == 1) {
            s = mmsi_s + color(ansi.YELLOW, '[SAR aircraft]');
         } else
         
         if (iden1 == 8) {
            s = mmsi_s + '[Handheld VHF]';
         }
         
         if (s == '') s = mmsi_s + color(ansi.YELLOW, '[Unknown]');
         break;
      
      // FIXME: enforce 2/4 min (vs 1/4) detection of fmt char against false alarms
      case t.FMT_DISTRESS:
      case t.FMT_ALL_SHIPS:
         s = '[All ships]';
         break;
      
      case t.FMT_COMMON_INTEREST:
         // ship group: 0 MID x xx xx
         //             1 234 5 67 89
         if (iden1 == 0 && isMID2) {
            var mid_s = _12[1] + _34;
            s = mmsi_s +'[Ship group: '+ mid(mid_s) +']';
         } else
            s = mmsi_s +'[Ship group]';
         break;
      
      case t.FMT_GEO_AREA:
         // 12 34 56 78 90
         // Qd dD DD vv hh
         // |  |   |  |  +- horiz lon deg
         // |  |   |  +---- vert lat deg
         // |  |   +------- lon deg
         // |  +----------- lat deg
         // +-------------- quadrant
         var quad = iden1;
         var lat_sgn = (quad == 0 || quad == 1)? '' : '-';
         var lon_sgn = (quad == 0 || quad == 2)? '' : '-';
         var s = lat_sgn + _12[1] + _34[0] +'\xb0/'+ lon_sgn +_34[1] + _56 +'\xb0 '+ _78+'\xb0V/'+ _90 +'\xb0H [Area]';
         break;
      
      default:
         s = color(ansi.YELLOW, mmsi_s) +'[Other]';
         break;
      }

      return { s:s, z: mmsi_s + _90[1], err:0 };
   }
   
   var command = function(sym_name, id) {
      var cmd = check(sym_name, 2, id);
      ecc_ck.push(cmd.n);
      cmd.s = cmd.err? 'cmdFEC' : ((cmd.nop)? '*' : cmd.sym_s);
      return cmd;
   };
   
   var frequency = function(sym_name, first_call, id, force_pos) {      // frequency(
      var f1 = check(sym_name +'1', 2, id);
      var f2 = check(sym_name +'2', 2, id);
      var f3 = check(sym_name +'3', 2, id);
      ecc_ck.push(f1.n);
      ecc_ck.push(f2.n);
      ecc_ck.push(f3.n);
      if (f1.err || f2.err || f3.err) return { s:'freqFEC', err:1 };

      var pre = '', type = 0;
      
      if (!f1.nop && !f2.nop) {     // can't check for !f3.nop because it's true for the CMD1_POSITION case
         var n = f1.z[0];

         // don't know eos yet, so this is just speculative for possible later eos=ABQ
         if (first_call && cmd1.n == t.CMD1_POSITION) {
            type = 5;
            //console.log('sym_name='+ sym_name +' CMD1_POSITION');
         }
         
         // position
         if (force_pos == true || n == '5' || type == 5) {
            return { s: f1.z + f2.z + f3.z, type:5, err:0 };
         }
      
         // 6-digit freq
         if (n == '0' || n == '1' || n == '2') {
            return { s: ((n == '0')? f1.z[1] : f1.z) + f2.z + f3.z[0] +'.'+ f3.z[1], type:6, err:0 };
         }
      
         // 7-digit freq
         // FIXME: account for +1 add'l packet length this causes
         if (n == '4') {
            pre = '7-DIGIT MODE|';
            type = 7;
         }
      }

      if (f1.nop) f1.z = '*';
      if (f2.nop) f2.z = '*';
      if (f3.nop) f3.z = '*';
      //console.log('sym_name='+ sym_name +' default return type='+ type);
      return { s: pre + f1.z +'|'+ f2.z +'|'+ f3.z, type:type, err:0 };
   }
   
   var end_of_sequence = function() {
      var eos = check('eos', 4, 'EOS');
      ecc_ck.push(eos.n);
      eos.s = eos.err? 'eosFEC' : ((eos.n == t.EOS)? 'EOS' : ((eos.n == t.ARQ)? 'ARQ' : ((eos.n == t.ABQ)? 'ABQ' : 'eos?')));
      return eos;
   };
   
   var error_check_char = function() {
      var ecc = check('ecc', 2, 'ECC');
      ecc.fail = false;
      
      var dump = function() {
         err = [];
         ecc_ck.forEach(function(a, i) { err.push( { n: t.ecc_sym[i], s: a } ); });
         console.log(err);
      };

      if (!ecc.err && !t.test) {
         var xor = 0;
         ecc_ck.forEach(function(a, i) { xor ^= a; });
         console.log('ECC len='+ ecc_ck.length +' got='+ xor +' want='+ ecc.n);
         ecc.fail = (xor != ecc.n);
         if (dbgUs && ecc.fail) dump();
      }
      if (dbgUs && t.test) dump();
      ecc.s = ecc.err? 'eccFEC' : ((ecc.fail)? 'eccFAIL' : '');
      return ecc;
   };
   
   var color = function(color, s) {
      return (color + s + ansi.NORM);
   };
   
   var fmt = format();
   var to = call('Bto', fmt, 'called to');
   var cat = category();
   var from = call('Dfr', fmt, 'called from');
   var cmd1 = command('E1cmd', 'telecommand 1');
   var cmd2 = command('E2cmd', 'telecommand 2');
   var f1 = frequency('Ffreq', t.FIRST_CALL, 'frequency 1');
   var f2 = frequency('Gfreq', false, 'frequency 2', (f1.type == 5));
   var eos = end_of_sequence();
   var ecc = error_check_char();

   var pe_s = t.parity_errors? (' '+ color(ansi.BLUE, t.parity_errors +' PE')) : '';
   var ecc_s = ecc.err? 'ECC FEC' : (ecc.fail? 'ECC' : '');
   if (ecc_s != '') ecc_s = ' '+ color(ansi.RED, ecc_s);
   
   var ack = function(s1, s2) {
      return (s1 + ((eos.n == t.ABQ)? ' ack' : '') + (s2? s2 : ''));
   };
   
   var type = function() {
      if (fmt.n == t.FMT_DISTRESS) return color(ansi.RED, 'DISTRESS');
      switch (cat.n) {
         case t.CAT_DISTRESS: return color(ansi.RED, 'DISTRESS');
         case t.CAT_URGENCY: return color(ansi.YELLOW, 'URGENT');
         case t.CAT_SAFETY: return color(ansi.CYAN, 'SAFETY');
         case t.CAT_ROUTINE: return color(ansi.GREEN, 'ROUTINE');
      }
      return color(ansi.GREY, 'UNKNOWN');
   };
   
   var from_to = function(s) {
      return (s +', '+ from.s +' => '+ to.s);
   };
   
   var freq = function(pos_request) {     // freq(
      var s = '';
      
      // frequency
      if (f1.type == 6) {
         s += ' '+ f1.s;
         if (f2.type == 6) s += '/'+ f2.s;
         s += ' kHz';
      } else
      
      // position
      if (f1.type == 5) {

         // "pos[23]" begin with "55" (offset 2)
         // "pos4" ends with NOP (offset 0)
         var i = (pos_request == true && eos.n == t.ABQ)? 0 : 2;
         var p = f1.s + f2.s;

         // spec position ack format
         // p[0-11]
         // 01 23 45 67 89
         // Qd dm mD DD MM
         // ||     +------ lon deg/min
         // |+------------ lat deg/min
         // +------------- quadrant
         var quad = +p[i+0];
         var lat_sgn = (quad == 0 || quad == 1)? '' : '-';
         var lon_sgn = (quad == 0 || quad == 2)? '' : '-';
         var s = ' '+ lat_sgn + p[i+1] + p[i+2] +'\xb0'+ p[i+3] + p[i+4] +"'/"+
            lon_sgn + p[i+5] + p[i+6] + p[i+7] +'\xb0'+ p[i+8] + p[i+9] +"'";
      }

      return s;
   };
   
   var ds;
   // don't include ecc.err here: for now, let decode occur even if ecc has fec error preventing ecc check
   var fec = (fmt.err || to.err || cat.err || from.err || cmd1.err || cmd2.err || f1.err || f2.err || eos.err);

   if (fec) {
      ;
   } else
      
   // A1-4.1: distress alerts
   // FMT_DISTRESS only used here, cat unused
   if (fmt.n == t.FMT_DISTRESS && eos.n == t.EOS) {
      s = ds = '4.1';
   } else

   // A1-4.2: distress acks
   if (cat.n == t.CAT_DISTRESS && fmt.n == t.FMT_ALL_SHIPS && cmd1.n == t.CMD1_DISTRESS_ACK && eos.n == t.EOS) {
      s = ds = '4.2';
   } else

   // A1-4.3: distress alert relays
   // differs from above in cmd1
   if (cat.n == t.CAT_DISTRESS && cmd1.n == t.CMD1_DISTRESS_ALERT_RELAY && (eos.n == t.EOS || eos.n == t.ARQ)) {
      // fmt: FMT_INDIV_STA, FMT_COMMON_INTEREST, FMT_GEO_AREA, FMT_ALL_SHIPS
      s = ds = '4.3';
   } else

   // A1-4.4: distress alert relay acks
   // differs from above in eos
   if (cat.n == t.CAT_DISTRESS && cmd1.n == t.CMD1_DISTRESS_ALERT_RELAY && eos.n == t.ABQ) {
      // fmt: FMT_INDIV_STA, FMT_COMMON_INTEREST, FMT_GEO_AREA, FMT_ALL_SHIPS
      s = ds = '4.4';
   } else

   // A1-4.5: urgency & safety calls, all ships
   if ((cat.n == t.CAT_SAFETY || cat.n == t.CAT_URGENCY) && fmt.n == t.FMT_ALL_SHIPS && cmd2.n == t.CMD2_NOP && eos.n == t.EOS) {
      s = ds = '4.5';
      
      switch (cmd1.n) {
         case t.CMD1_J3E_RT: s = from_to('SSB call'+ freq()); break;
         case t.CMD1_F1B_FEC: s = from_to('FSK-FEC call'+ freq()); break;
      }
   } else

   // A1-4.6: urgency & safety calls, geo areas
   // differs from above in fmt
   if ((cat.n == t.CAT_SAFETY || cat.n == t.CAT_URGENCY) && fmt.n == t.FMT_GEO_AREA && eos.n == t.EOS) {
      s = ds = '4.6';
      var s2 = (cmd2.n == t.CMD2_NOP)? '' : ((cmd2.n == t.CMD2_MED_TRANSPORTS)? ' medical' : ((cmd2.n == t.CMD2_NON_CONFLICT)? ' ships/aircraft' : ''));

      switch (cmd1.n) {
         case t.CMD1_J3E_RT: s = from_to('SSB call'+ s2 + freq()); break;
         case t.CMD1_F1B_FEC: s = from_to('FSK-FEC call'+ s2 + freq()); break;
      }
   } else

   // A1-4.7: urgency & safety calls, individual calls and acks
   // differs from above in fmt
   if ((cat.n == t.CAT_SAFETY || cat.n == t.CAT_URGENCY) && fmt.n == t.FMT_INDIV_STA) {
      s = ds = '4.7';
   
      if (eos.n == t.ARQ || eos.n == t.ABQ) {
         if (cmd2.n == t.CMD2_NOP) {
            switch (cmd1.n) {
               case t.CMD1_FM_CALL: s = from_to(ack('FM call', freq())); break;
               case t.CMD1_FM_DUPLEX_CALL: s = from_to(ack('FM duplex call', freq())); break;
               case t.CMD1_J3E_RT: s = from_to(ack('SSB call', freq())); break;
               case t.CMD1_F1B_FEC: s = from_to(ack('FSK-FEC call', freq())); break;
               case t.CMD1_F1B_ARQ: s = from_to(ack('FSK-ARQ call', freq())); break;
               case t.CMD1_POSITION: s = from_to(ack('Position request', freq(t.POS_REQUEST))); break;
               case t.CMD1_TEST: s = from_to(ack('Test')); break;
            }
         } else {
            if (cmd1.n == t.CMD1_UNABLE_COMPLY && cmd2.n >= t.CMD2_UNABLE_FIRST && cmd2.n <= t.CMD2_UNABLE_LAST && eos.n == t.ABQ) {
               s = from_to(color(ansi.YELLOW, 'Unable to comply ack') + freq());
            }
         }
      } else
      
      // out-of-spec, but seen. USGC MMSI 0036699930 sending to itself, hence EOS instead of expected ARQ/ABQ
      if (eos.n == t.EOS && cmd1.n == t.CMD1_TEST && cmd2.n == t.CMD2_NOP) {
         s = from_to('Test');
      }
   } else

   // A1-4.8: routine group calls
   if (cat.n == t.CAT_ROUTINE && fmt.n == t.FMT_COMMON_INTEREST && cmd2.n == t.CMD2_NOP && eos.n == t.EOS) {
      s = ds = '4.8';

      switch (cmd1.n) {
         case t.CMD1_J3E_RT: s = from_to('SSB call'+ freq()); break;
         case t.CMD1_F1B_FEC: s = from_to('FSK-FEC call'+ freq()); break;
      }
   } else

   // A1-4.9: routine individual calls and acks
   // differs from above in fmt
   if (cat.n == t.CAT_ROUTINE && fmt.n == t.FMT_INDIV_STA && (eos.n == t.ARQ || eos.n == t.ABQ)) {
      s = ds = '4.9';

      if (cmd2.n == t.CMD2_NOP) {
         switch (cmd1.n) {
            case t.CMD1_FM_CALL: s = from_to(ack('FM call', freq())); break;
            case t.CMD1_FM_DUPLEX_CALL: s = from_to(ack('FM duplex call', freq())); break;
            case t.CMD1_J3E_RT: s = from_to(ack('SSB call', freq())); break;
            case t.CMD1_F1B_FEC: s = from_to(ack('FSK-FEC call', freq())); break;
            case t.CMD1_F1B_ARQ: s = from_to(ack('FSK-ARQ call', freq())); break;
            case t.CMD1_F1B_DATA: s = from_to(ack('FSK-DATA call', freq())); break;
            case t.CMD1_POLLING: s = from_to(ack('Polling')); break;
         }
      } else {
         if (cmd1.n == t.CMD1_UNABLE_COMPLY && cmd2.n >= t.CMD2_UNABLE_FIRST && cmd2.n <= t.CMD2_UNABLE_LAST && eos.n == t.ABQ) {
            s = from_to(color(ansi.YELLOW, 'Unable to comply ack') + freq());
         }
      }
   } else
   
   // A1-4.10: semi/auto MF/HF
   // differs from above in fmt
   if (cat.n == t.CAT_ROUTINE && fmt.n == t.FMT_IS_SEMI_AUTO && (eos.n == t.ARQ || eos.n == t.ABQ)) {
      s = ds = '4.10';
   } else {
      s = ds = '4.x';
   }
   
   if (dbgUs) {
      var ok = '$$ '+ (fec? 'FEC FAIL' : 'OK');
      var to_s = kiwi_remove_escape_sequences(to.s);
      var from_s = kiwi_remove_escape_sequences(from.s);
      console.log(ok +' fmt='+ fmt.s +' to='+ to_s +' cat='+ cat.s +' from='+ from_s);
      console.log(ok +' cmd1='+ cmd1.s +' cmd2='+ cmd2.s +' freq1='+ f1.s +' freq2='+ f2.s +' eos='+ eos.s +' ecc='+ ecc.s + pe_s + ecc_s);
   }

   var msg_s = '', err_s = '';
   if (fec) {
      err_s = color(ansi.BLUE, t.FEC_errors +' FEC');
      ecc_s = '';    // if fec errors don't bother displaying ecc which will always have failed
   } else {
      if (s.startsWith('4.')) {
         msg_s = color(ansi.MAGENTA, 'DECODE '+ s) +' fmt='+ fmt.n +' cat='+ cat.n +' cmd1='+ cmd1.n+ ' cmd2='+ cmd2.n +
            ' from='+ from.z +' to='+ to.z +' freq1='+ f1.s + ' freq2='+ f2.s +' eos='+ eos.n +' ecc='+ ecc.n;
      } else {
         msg_s = (dbgUs? (ds + ' ') : '') + type() +' '+ s;
      }
      if (t.test) {
         msg_s = 'TEST '+ msg_s;
         t.tlist.shift();
      }
   }

   if (show_errs) {
      s = (err_s != '')? err_s : msg_s;
      return t.output_msg(s + pe_s + ecc_s);
   } else {
      s = (err_s != '')? '' : t.output_msg(msg_s + ecc_s);
      return s;
   }
}

DSC.prototype.code_to_char = function(code) {
   
   var t = this;
   if (code < 0 || code > 127) return '???';
   var sym = t.sym_s[code];
   //if (sym == '   ') return '['+ code +']';
   return sym;
}

DSC.prototype.reset = function() {
   var t = this;
   t.syncA = t.syncB = t.syncC = t.syncD = t.syncE = 0;
}

DSC.prototype.get_nbits = function() {
   return 10;
}

DSC.prototype.get_msb = function() {
   return 0x200;
}

DSC.prototype.check_bits = function() {
   return false;
}

DSC.prototype.search_sync = function(bit) {
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

DSC.prototype.process_char = function(_code, fixed_start, output_cb, show_raw, show_errs) {
   var t = this;
   if (!t.synced) return { resync:0 };

   // test EOS detection
   //if (t.seq == 54) _code = 99;
   //if (t.seq == 58) _code = 99;

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
   var zc = (bc_ck != bc_data)? (' Zck='+ bc_ck +' Zdata='+ bc_data) : '';
   t.bc_err[t.seq] = (bc_ck != bc_data);
   if (t.bc_err[t.seq]) t.parity_errors++;

   if (t.dbg) console.log(t.seq.leadingZeros(2) +' '+ pos_s.fieldWidth(6) +': '+ chr +' '+ code.fieldWidth(3) +'.'+
      ' '+ bc_rev.toString(2).leadingZeros(3) +' '+ code.toString(2).leadingZeros(7) + zc);
   t.seq++;

   // detect variable length EOS
   var eos = false;

   if (t.seq >= t.MSG_LEN_MIN) {
      var eos_n = 0, s;
      
      var eos_ck = function(eos_sym) {
         var eos_n = 0;
         if (!t.bc_err[t.seq-2] && t.syms[t.seq-2] == eos_sym) eos_n++;
         if (!t.bc_err[t.seq-3] && t.syms[t.seq-3] == eos_sym) eos_n++;
         if (!t.bc_err[t.seq-4] && t.syms[t.seq-4] == eos_sym) eos_n++;
         if (!t.bc_err[t.seq-8] && t.syms[t.seq-8] == eos_sym) eos_n++;
         if (eos_n >= 3) console.log('eos_n('+ eos_sym +')='+ eos_n);
         return (eos_n >= 3);
      }
      
      var color = function(color, s) {
         return (color + s + ansi.NORM);
      };

      if (eos_ck(t.EOS) || eos_ck(t.ARQ) || eos_ck(t.ABQ)) eos = true;
      if (eos || t.seq > t.MSG_LEN_MAX) {
         var dump = 0;
         if (eos) {
            if (t.seq != t.MSG_LEN_MIN) {
               if (show_errs) output_cb(t.output_msg(color(ansi.BLUE, 'non-std len='+ t.seq)));
               console.log('$$ non-std len='+ t.seq);
               dump = 1;
            }
            output_cb(t.process_msg(show_errs));
         } else {
            var pe_s = t.parity_errors? (' '+ color(ansi.BLUE, (t.parity_errors +' PE'))) : '';
            if (show_errs) output_cb(t.output_msg(color(ansi.BLUE, 'no EOS') + pe_s));
            console.log('$$ no EOS pe='+ t.parity_errors);
            dump = t.dump_eos;
         }

         if (dump && dbgUs) {
            for (var i = 0; i < t.seq; i++) {
               var _code = t.full_syms[i];
               if (isUndefined(_code)) _code = 119;   // sync
               var code = _code & 0x7f;
               var pos_s = t.pos_s[i];
               pos_s = pos_s || 'unk';
               var chr = t.code_to_char(code);
               var bc_rev = (_code >> 7) & 7;
               var bc_ck = kiwi_bitReverse(bc_rev, 3);
               var bc_data = kiwi_bitCount(code ^ 0x7f);
               var zc = (bc_ck != bc_data)? (' Zck='+ bc_ck +' Zdata='+ bc_data) : '';
               console.log(i.leadingZeros(2) +' '+ pos_s.fieldWidth(6) +': '+ chr +' '+ code.fieldWidth(3) +'.'+
                  ' '+ bc_rev.toString(2).leadingZeros(3) +' '+ code.toString(2).leadingZeros(7) + zc);
            }
         }

         t.synced = 0;
         return { resync:1 };
      }
   }
   
   return { resync:0 };
}

DSC.prototype.init_MID_MMSI = function() {

this.MID =
{
    "201": "Albania",
    "202": "Andorra",
    "203": "Austria",
    "204": "Azores",
    "205": "Belgium",
    "206": "Belarus",
    "207": "Bulgaria",
    "208": "Vatican City",
    "209": "Cyprus",
    "210": "Cyprus",
    "211": "Germany",
    "212": "Cyprus",
    "213": "Georgia",
    "214": "Moldova",
    "215": "Malta",
    "216": "Armenia",
    "218": "Germany",
    "219": "Denmark",
    "220": "Denmark",
    "224": "Spain",
    "225": "Spain",
    "226": "France",
    "227": "France",
    "228": "France",
    "229": "Malta",
    "230": "Finland",
    "231": "Faroe Is",
    "232": "United Kingdom",
    "233": "United Kingdom",
    "234": "United Kingdom",
    "235": "United Kingdom",
    "236": "Gibraltar",
    "237": "Greece",
    "238": "Croatia",
    "239": "Greece",
    "240": "Greece",
    "241": "Greece",
    "242": "Morocco",
    "243": "Hungary",
    "244": "Netherlands",
    "245": "Netherlands",
    "246": "Netherlands",
    "247": "Italy",
    "248": "Malta",
    "249": "Malta",
    "250": "Ireland",
    "251": "Iceland",
    "252": "Liechtenstein",
    "253": "Luxembourg",
    "254": "Monaco",
    "255": "Madeira",
    "256": "Malta",
    "257": "Norway",
    "258": "Norway",
    "259": "Norway",
    "261": "Poland",
    "262": "Montenegro",
    "263": "Portugal",
    "264": "Romania",
    "265": "Sweden",
    "266": "Sweden",
    "267": "Slovak Republic",
    "268": "San Marino",
    "269": "Switzerland",
    "270": "Czech Republic",
    "271": "Turkey",
    "272": "Ukraine",
    "273": "Russian Federation",
    "274": "Macedonia",
    "275": "Latvia",
    "276": "Estonia",
    "277": "Lithuania",
    "278": "Slovenia",
    "279": "Serbia",
    "301": "Anguilla",
    "303": "Alaska (State of, USA)",
    "304": "Antigua and Barbuda",
    "305": "Antigua and Barbuda",
    "306": "Sint Maarten/Bonaire/Cura§ao",
    "307": "Aruba",
    "308": "Bahamas",
    "309": "Bahamas",
    "310": "Bermuda",
    "311": "Bahamas",
    "312": "Belize",
    "314": "Barbados",
    "316": "Canada",
    "319": "Cayman Is",
    "321": "Costa Rica",
    "323": "Cuba",
    "325": "Dominica",
    "327": "Dominican Republic",
    "329": "Guadeloupe",
    "330": "Grenada",
    "331": "Greenland",
    "332": "Guatemala",
    "334": "Honduras",
    "336": "Haiti",
    "338": "USA",
    "339": "Jamaica",
    "341": "Saint Kitts/Nevis",
    "343": "Saint Lucia",
    "345": "Mexico",
    "347": "Martinique",
    "348": "Montserrat",
    "350": "Nicaragua",
    "351": "Panama",
    "352": "Panama",
    "353": "Panama",
    "354": "Panama",
    "355": "-",
    "356": "-",
    "357": "-",
    "358": "Puerto Rico",
    "359": "El Salvador",
    "361": "Saint Pierre and Miquelon",
    "362": "Trinidad and Tobago",
    "364": "Turks and Caicos Is",
    "366": "USA",
    "367": "USA",
    "368": "USA",
    "369": "USA",
    "370": "Panama",
    "371": "Panama",
    "372": "Panama",
    "373": "Panama",
    "374": "-",
    "375": "Saint Vincent/Grenadines",
    "376": "Saint Vincent/Grenadines",
    "377": "Saint Vincent/Grenadines",
    "378": "British Virgin Is",
    "379": "US Virgin Is",
    "401": "Afghanistan",
    "403": "Saudi Arabia",
    "405": "Bangladesh",
    "408": "Bahrain",
    "410": "Bhutan",
    "412": "China",
    "413": "China",
    "414": "China",
    "416": "Taiwan",
    "417": "Sri Lanka",
    "419": "India",
    "422": "Iran",
    "423": "Azerbaijan",
    "425": "Iraq",
    "428": "Israel",
    "431": "Japan",
    "432": "Japan",
    "434": "Turkmenistan",
    "436": "Kazakhstan",
    "437": "Uzbekistan",
    "438": "Jordan",
    "440": "South Korea",
    "441": "South Korea",
    "443": "State of Palestine",
    "445": "DPRK",
    "447": "Kuwait",
    "450": "Lebanon",
    "451": "Kyrgyz Republic",
    "453": "Macao",
    "455": "Maldives",
    "457": "Mongolia",
    "459": "Nepal",
    "461": "Oman",
    "463": "Pakistan",
    "466": "Qatar",
    "468": "Syrian Arab Republic",
    "470": "United Arab Emirates",
    "472": "Tajikistan",
    "473": "Yemen",
    "475": "Yemen",
    "477": "Hong Kong",
    "478": "Bosnia and Herzegovina",
    "501": "Adelie Land",
    "503": "Australia",
    "506": "Myanmar",
    "508": "Brunei Darussalam",
    "510": "Micronesia",
    "511": "Palau",
    "512": "New Zealand",
    "514": "Cambodia",
    "515": "Cambodia",
    "516": "Christmas Is (Indian Ocean)",
    "518": "Cook Is",
    "520": "Fiji",
    "523": "Cocos (Keeling) Is",
    "525": "Indonesia",
    "529": "Kiribati",
    "531": "Lao People's D.Rep",
    "533": "Malaysia",
    "536": "Northern Mariana Is",
    "538": "Marshall Is",
    "540": "New Caledonia",
    "542": "Niue",
    "544": "Nauru",
    "546": "French Polynesia",
    "548": "Philippines",
    "553": "Papua New Guinea",
    "555": "Pitcairn Is",
    "557": "Solomon Is",
    "559": "American Samoa",
    "561": "Samoa",
    "563": "Singapore",
    "564": "Singapore",
    "565": "Singapore",
    "566": "Singapore",
    "567": "Thailand",
    "570": "Tonga",
    "572": "Tuvalu",
    "574": "Viet Nam",
    "576": "Vanuatu",
    "577": "Vanuatu",
    "578": "Wallis and Futuna Is",
    "601": "South Africa",
    "603": "Angola",
    "605": "Algeria",
    "607": "Saint Paul and Amsterdam Is",
    "608": "Ascension Is",
    "609": "Burundi",
    "610": "Benin",
    "611": "Botswana",
    "612": "Central African Republic",
    "613": "Cameroon",
    "615": "Congo",
    "616": "Comoros",
    "617": "Cabo Verde",
    "618": "Crozet Archipelago",
    "619": "Cote d'Ivoire",
    "620": "Comoros",
    "621": "Djibouti",
    "622": "Egypt",
    "624": "Ethiopia",
    "625": "Eritrea",
    "626": "Gabonese Republic",
    "627": "Ghana",
    "629": "Gambia",
    "630": "Guinea-Bissau",
    "631": "Equatorial Guinea",
    "632": "Guinea",
    "633": "Burkina Faso",
    "634": "Kenya",
    "635": "Kerguelen Is",
    "636": "Liberia",
    "637": "Liberia",
    "638": "South Sudan",
    "642": "Libya",
    "644": "Lesotho",
    "645": "Mauritius",
    "647": "Madagascar",
    "649": "Mali",
    "650": "Mozambique",
    "654": "Mauritania",
    "655": "Malawi",
    "656": "Niger",
    "657": "Nigeria",
    "659": "Namibia",
    "660": "Reunion",
    "661": "Rwanda",
    "662": "Sudan",
    "663": "Senegal",
    "664": "Seychelles",
    "665": "Saint Helena",
    "666": "Somalia",
    "667": "Sierra Leone",
    "668": "Sao Tome and Principe",
    "669": "Swaziland",
    "670": "Chad",
    "671": "Togolese Republic",
    "672": "Tunisia",
    "674": "Tanzania",
    "675": "Uganda",
    "676": "D.Rep Congo",
    "677": "Tanzania",
    "678": "Zambia",
    "679": "Zimbabwe",
    "701": "Argentine",
    "710": "Brazil",
    "720": "Bolivia",
    "725": "Chile",
    "730": "Colombia",
    "735": "Ecuador",
    "740": "Falkland Is",
    "745": "Guiana",
    "750": "Guyana",
    "755": "Paraguay",
    "760": "Peru",
    "765": "Suriname",
    "770": "Uruguay",
    "775": "Venezuela"
};

this.MMSI_coast =
{
    "2040100": "Delgada",
    "2040200": "Horta",
    "2050480": "Oostende",
    "2050485": "Antwerpen",
    "2070810": "Varna",
    "2091000": "Cyprus",
    "2111240": "Bremen Rescue Radio",
    "2130100": "Batumi",
    "2130300": "Poti",
    "2191000": "Lyngby",
    "2240991": "Barcelona",
    "2240992": "Coruna",
    "2240993": "Finisterre",
    "2240994": "Tarifa",
    "2240995": "Las Palmas",
    "2240996": "Bilbao",
    "2240997": "Gijon",
    "2240998": "Vigo",
    "2241001": "Algeciras",
    "2241002": "Almeria",
    "2241003": "Cartegena",
    "2241004": "Valencia",
    "2241005": "Palma",
    "2241006": "Tarragona",
    "2241007": "Tenerife",
    "2241008": "Madrid",
    "2241009": "Santander",
    "2241011": "Cadiz",
    "2241012": "Huelva",
    "2241016": "Castellon",
    "2241021": "Bilbao",
    "2241022": "Coruna",
    "2241023": "Malaga",
    "2241024": "Valencia",
    "2241025": "Tenerife",
    "2241026": "Las Palmas",
    "2241078": "Madrid Radio",
    "2275000": "Etel",
    "2275100": "Gris Nez",
    "2275200": "Jobourg",
    "2275300": "Corsen",
    "2275400": "La Garde",
    "2275420": "Aspretto",
    "2300230": "Turku",
    "2301000": "Turku",
    "2302000": "Helsinki",
    "2303000": "Vaasa",
    "2311000": "Torshavn",
    "2320001": "MRSC Shetland",
    "2320004": "MRCC Aberdeen",
    "2320005": "MRSC Forth",
    "2320007": "MRSC Humber",
    "2320008": "MRCC Yarmouth",
    "2320009": "MRSC Thames",
    "2320010": "MRCC Dover",
    "2320011": "MRSC Solent",
    "2320012": "MRSC Portland",
    "2320013": "MRSC Brixham",
    "2320014": "MRCC Falmouth",
    "2320016": "MRCC Swansea",
    "2320017": "MRSC Milford Haven",
    "2320018": "MRSC Holyhead",
    "2320019": "MRSC Liverpool",
    "2320021": "MRSC Belfast",
    "2320022": "MRCC Clyde",
    "2320024": "MRSC Stornoway",
    "2320060": "Jersey Radio",
    "2320064": "S.Peter Port Radio",
    "2371000": "Olympia",
    "237673000": "Piraeus JRCC",
    "237673140": "Patrai Port Auth",
    "237673150": "Rodos Port Auth",
    "237673180": "Iraklion Port Auth",
    "237673190": "Kerkyra Port Auth",
    "237673210": "Thessaloniki Port Auth",
    "237673220": "Mytilini Port Auth",
    "237673230": "Pylos Port Auth",
    "2380100": "Split",
    "2380200": "Rijeka",
    "2380300": "Dubrovnik",
    "2387010": "Rijeka",
    "2387020": "Rijeka",
    "2387030": "Split",
    "2387040": "Split",
    "2387400": "Zadar",
    "2387401": "Zadar",
    "2387500": "Sibenik",
    "2387501": "Sibenik",
    "2387800": "Dubrovnik",
    "2387801": "Dubrovnik",
    "2391000": "Aspropirgos Attikis",
    "2442000": "Netherland CG Radio",
    "2470001": "Roma",
    "2470002": "Palermo",
    "2500100": "Malin Head",
    "2500200": "Valentia",
    "2500300": "Dublin",
    "2510100": "Reykjavik",
    "2510200": "Reykjavik",
    "2550100": "Funchal",
    "2550200": "Porto Santo",
    "2570000": "All Norwegian Stations",
    "2570100": "Tjoeme",
    "2570300": "Rogaland",
    "2570500": "Floroe",
    "2570700": "Bodoe",
    "2570800": "Vardoe",
    "2570900": "Svalbard/Bjoernoeya/Jan Mayen",
    "2610210": "Witowo",
    "2630100": "Lisboa",
    "2630200": "Apulia",
    "2630400": "Sagres",
    "2640570": "Constanta",
    "2653000": "Goteborg",
    "2711000": "Istanbul",
    "2712000": "Samsun",
    "2713000": "Antalya",
    "2715000": "Izmir",
    "2723650": "Mariupol",
    "2723655": "Yuzhnyy",
    "2723659": "Kerch",
    "2723660": "Odessa",
    "2723663": "Theodosia",
    "2723672": "Berdiansk",
    "2733728": "Magadan",
    "2733700": "Sankt Peterburg",
    "2733733": "Yuzhno-Sakhalinsk",
    "2734411": "Novorossiysk",
    "2734412": "Vladivostok",
    "2734413": "Tuapse",
    "2734414": "Arkhangelsk",
    "2734415": "Vyborg",
    "2734416": "Magadan",
    "2734417": "Kaliningrad",
    "2734418": "Petropavlovsk-Kamchatskiy",
    "2734419": "Astrakhan",
    "2734420": "Murmansk",
    "2734421": "Vanino",
    "2734422": "Eisk/Rostov-na-Donu",
    "2734423": "Makhachkala",
    "2750100": "Riga Rescue Radio",
    "2761000": "Tallinn",
    "2760120": "Kuressaare",
    "2770330": "Klaipeda Rescue Centre",
    "2780200": "Koper",
    "2790001": "Bar",
    "2790002": "Obosnik",
    "3061000": "Curacao",
    "3100001": "Bermuda Harbour",
    "3160010": "Vancouver",
    "3160011": "Victoria",
    "3160012": "Tofino",
    "3160013": "Prince Rupert",
    "3160014": "Comox",
    "3160015": "St John",
    "3160016": "Halifax",
    "3160017": "Sydney",
    "3160018": "Port Aux Basques",
    "3160019": "Placentia",
    "3160020": "St Johns",
    "3160021": "St Anthony",
    "3160022": "Labrador",
    "3160023": "Iqaluit",
    "3160025": "Riviere au Renard",
    "3311000": "Qaqortoq",
    "3313000": "Aasiaat",
    "3314000": "Ammassalik",
    "3450010": "Cerrillo",
    "34500475": "Isla Socorro",
    "3450110": "Tampico",
    "3450120": "La Pesca",
    "3450172": "Matamoros",
    "3450173": "Mezquital",
    "3450174": "San Blas",
    "3450210": "Ensenada",
    "3450272": "Isla de Cedros",
    "3450273": "Isla Guadalupe",
    "3450274": "San Felipe",
    "3450310": "Veracruz",
    "3450320": "Coatzacoalcos",
    "3450330": "Tuxpan",
    "3450372": "Tuxpan",
    "3450410": "La Paz",
    "3450420": "Los Cabos",
    "3450430": "San Carlos",
    "3450440": "Guerrero Negro",
    "3450471": "La Paz",
    "3450472": "Isla Clarion/Puerto Cortez",
    "3450473": "Los Cabos",
    "3450474": "Santa Rosalia",
    "3450610": "Guaymas",
    "3450620": "Puerto Penasco",
    "3450671": "Guaymas",
    "3450672": "Puerto Penasco",
    "3450710": "Ciudad de Carmen",
    "3450720": "Campeche",
    "3450772": "Lerma",
    "3450810": "Mazatlan",
    "3450820": "Topolobampo",
    "3450872": "Topolobampo",
    "3450910": "Progreso",
    "3450974": "Cayo Arcas",
    "3451110": "Cozumel",
    "3451120": "Chetumal",
    "3451130": "Puerto Juarez",
    "3451171": "Isla Mujeres",
    "3451174": "Isla Holbox",
    "3451175": "Isla Contoy",
    "3451176": "Playa Linda",
    "3451210": "Puerto Vallarta",
    "3451410": "Manzanillo",
    "3451610": "Lazaro Cardenas",
    "3451810": "Acapulco",
    "3452010": "Salina Cruz",
    "3452030": "Puerto Angel",
    "3452071": "Salina Cruz",
    "3452072": "Huatulco",
    "3452210": "Puerto Madero",
    "3452271": "Puerto Madero",
    "3452272": "Paredon",
    "3660030": "Mobile Radio/WLO",
    "3669899": "USCG Kodiak",
    "3669902": "Woods Hole",
    "3669903": "Atlantic City",
    "3669904": "Port Angeles",
    "3669905": "Honolulu",
    "3669906": "Cape Hatteras",
    "3669907": "Charleston",
    "3669908": "New Orleans",
    "3669909": "Humboldt Bay",
    "3669910": "Astoria WA",
    "3669911": "North Bend",
    "3669912": "Long Beach CA",
    "3669913": "San Diego CA",
    "3669914": "Mobile AL",
    "3669915": "Galveston TX",
    "3669916": "Corpus Christi",
    "3669917": "St.Petersburg",
    "3669918": "Key West FL",
    "3669919": "Miami FL",
    "3669920": "Fort Macon",
    "3669921": "Southwest",
    "3669922": "Juneau",
    "3669924": "Valdez",
    "3669925": "Mayport",
    "3669926": "San Francisco",
    "3669928": "South Portland",
    "3669929": "New York NY",
    "3669931": "New Haven",
    "3669932": "Chincoteague",
    "3669936": "Moriches",
    "3669990": "USCG Pt Reyes CAMSPAC",
    "3669991": "USCG Boston MA",
    "3669992": "San Juan",
    "3669993": "USCG Honolulu",
    "3669994": "USCG Guam",
    "3669995": "USCG Portsmouth CAMSLANT",
    "3669997": "USCG Miami FL",
    "3669998": "USCG New Orleans LA",
    "3669999": "Any USCG",
    "4030000": "Jeddah",
    "4121100": "Tianjin",
    "4121200": "Qinhuangdao",
    "4121300": "Dalian",
    "4121400": "Yantai",
    "4122100": "Shanghai",
    "4122200": "Qingdao",
    "4122300": "Lianyungang",
    "4122400": "Ningbo",
    "4122500": "Wenzhou",
    "4122600": "Fuzhou",
    "4122700": "Xiamen",
    "4123100": "Guangzhou",
    "4123200": "Shantou",
    "4123300": "Zhangjiang",
    "4123400": "Beihai",
    "4123500": "Haikou",
    "4123600": "Basuo",
    "4123700": "Sanya",
    "4162019": "Chilung",
    "4192201": "Daman",
    "4192202": "Porbandar",
    "4192203": "Mumbai",
    "4192204": "New Mangalore",
    "4192205": "Kochi",
    "4192206": "Goa",
    "4192207": "Okha",
    "4194401": "Chennai",
    "4194402": "Visakhapatnam",
    "4194403": "Paradip",
    "4194404": "Haldia",
    "4194405": "Tuticorin",
    "4194406": "Mandapam",
    "4194407": "Diglipur",
    "4194408": "Campbell Bay",
    "4194409": "Port Blair",
    "4225300": "Bandar Khomeyni",
    "4225301": "Chabahar",
    "4225302": "Bushehr",
    "4225303": "Now-shahr",
    "4225304": "Bander-e Abbas",
    "4225305": "Anzali",
    "4225306": "Kharg",
    "4225307": "Lengeh",
    "4225308": "Bahonar",
    "4225309": "Khorramshahr",
    "4225311": "Neka",
    "4280001": "Haifa",
    "4310001": "Tokyo Sea Patrol Radio",
    "4310101": "Otaru",
    "4310102": "Kushiro",
    "4310201": "Shiogama",
    "4310301": "Yokohama",
    "4310401": "Nagoya",
    "4310501": "Kobe",
    "4310502": "Tanabe",
    "4310503": "Kochi",
    "4310601": "Hiroshima",
    "4310701": "Moji",
    "4310702": "Sasebo",
    "4310801": "Maizuru",
    "4310901": "Niigata",
    "4311001": "Kagoshima",
    "4311101": "Naha",
    "4311102": "Ishigaki",
    "4381234": "Aqaba Radio",
    "4400001": "Inchon Radio",
    "4400002": "Seoul Radio",
    "4400101": "Pusan Radio",
    "4400301": "Mokpo Radio",
    "4400601": "Kangung Radio",
    "4401001": "Inchon Police Radio",
    "4401002": "Tonghae Police Radio",
    "4401003": "Mokpo Police Radio",
    "4401004": "Pusan Police Radio",
    "4401005": "Cheju Police Radio",
    "4472188": "Kuwait",
    "4634052": "Gawadar",
    "4634056": "Ormara",
    "4634060": "Karachi",
    "4661001": "Mesaieed Port",
    "4700000": "Emirates",
    "4773500": "Hong Kong Marine Rescue",
    "5030001": "Charleville/Wiluna",
    "5060100": "Yangon",
    "5060200": "Myeik",
    "5120010": "Taupo",
    "5201100": "Suva",
    "5250000": "Jakarta",
    "5250001": "Surabaya",
    "5250002": "Makassar",
    "5250003": "Belawan",
    "5250004": "Dumai",
    "5250005": "Bitung",
    "5250006": "Amboina",
    "5250007": "Jayapura",
    "5250008": "Semarang",
    "5250009": "Balikpapan",
    "5250010": "Kupang",
    "5250011": "Sorong",
    "5250012": "Batu Ampar",
    "5250013": "Panjang",
    "5250014": "Benoa",
    "5250015": "Dili",
    "5250016": "Pontianak",
    "5250017": "Tarakan",
    "5250018": "Pantoloan",
    "5250019": "Kendari",
    "5250020": "Ternate",
    "5250021": "Merauke",
    "5250022": "Lembar",
    "5250023": "Manokwari",
    "5250024": "Tahuna",
    "5250025": "Sanana",
    "5250026": "Fak-Fak",
    "5250028": "Sibolga",
    "5250029": "Sei Kolak Kiang/Tanjung Pinang",
    "5250030": "Cilacap",
    "5250031": "Biak",
    "5330001": "Gunung Jerai",
    "5330002": "Permatang Pauh",
    "5330003": "Gunung Berinchang",
    "5330004": "Ulu Kali",
    "5330005": "Gunung Ledang",
    "5330006": "Tioman",
    "5330007": "Kuala Rompin",
    "5330008": "Kuantan",
    "5330009": "Kuala Terengganu",
    "5330010": "Machang",
    "5330011": "Kuching",
    "5330012": "Bintulu",
    "5330013": "Kota Kinabalu",
    "5330014": "Labuan",
    "5401000": "Noumea",
    "5480001": "Aparri",
    "5480002": "San Jose",
    "5480003": "San Fernando",
    "5480004": "Cagayan de Oro",
    "5480005": "Real",
    "5480006": "Roxas",
    "5480007": "Tacloban",
    "5630002": "Singapore",
    "5671000": "Bangkok",
    "5741993": "Ho Chi Minh-Ville",
    "5741996": "Hai Phong",
    "5741999": "Da Nang",
    "5742001": "Nha Trang",
    "5742004": "Vung Tau",
    "5742006": "Hon Gai/Ha Long",
    "5742007": "Mong Cai",
    "5742008": "Cua Ong",
    "5742009": "Ben Thuy",
    "5742010": "Hue",
    "5742011": "Quy Nhon",
    "5742012": "Kien Giang",
    "5742013": "Can Tho",
    "5742014": "Thanh Hoa",
    "5742015": "Bach Long",
    "5742016": "Hon La",
    "5742017": "Cua Viet",
    "5742018": "Ly Son",
    "5742019": "Quang Ngai",
    "5742020": "Cam Ranh",
    "5742021": "Phan Thiet",
    "5742022": "Phu Quy",
    "5742023": "Con Dao",
    "5742024": "Tho Chu",
    "5742025": "Nam Can",
    "5742026": "Bac Lieu",
    "5742027": "Ha Tien",
    "5742028": "Phu Quoc",
    "5742029": "Dung Quat",
    "6010001": "Capetown",
    "6100001": "Cotonou",
    "6221111": "Alexandria",
    "6221112": "Quseir",
    "6221113": "Port Said",
    "6270000": "Tema",
    "6452700": "Mauritius",
    "6501000": "Maputo",
    "6502000": "Inhambane",
    "6503000": "Beira",
    "6504000": "Quelimane",
    "6505000": "Angoche",
    "6506000": "Nacala",
    "6507000": "Pema",
    "7010001": "Buenos Aires",
    "7010002": "Recalada Rio de la Plata",
    "7010003": "Mar del Plata",
    "7010005": "Bahia Blanca",
    "7010006": "San Blas",
    "7010007": "Rawson",
    "7010008": "Comodoro Rivadavia",
    "7010009": "Puerto Deseado",
    "7010010": "Rio Gallegos",
    "7010011": "Ushuaia",
    "7010111": "Argentina Radio",
    "7010221": "Mar del Plata",
    "7100001": "Rio",
    "7100002": "Recife",
    "7100003": "Manaus",
    "7250010": "Arica",
    "7250020": "Iquique",
    "7250030": "Tocopilla",
    "7250040": "Mejillones",
    "7250050": "Antofagasta",
    "7250060": "Taltal",
    "7250070": "Chanaral",
    "7250080": "Caldera",
    "7250090": "Huasco",
    "7250100": "Isla de Pascua",
    "7250110": "Coquimbo",
    "7250120": "Los Vilos",
    "7250125": "Quintero",
    "7250130": "Juan Fernandez",
    "7250140": "San Antonio",
    "7250150": "Constitucion",
    "7250170": "Talcahuano",
    "7250210": "Corral",
    "7250220": "Valdivia",
    "7250230": "Puerto Montt",
    "7250235": "Faro Punta Corona",
    "7250240": "Ancud",
    "7250250": "Castro",
    "7250260": "Chaiten",
    "7250270": "Quellon",
    "7250280": "Melinka",
    "7250290": "Faro Isla Guafo",
    "7250294": "Puerto Aguirre",
    "7250298": "Puerto Chacabuco",
    "7250300": "Puerto Aysen",
    "7250310": "Faro Raper",
    "7250320": "San Pedro",
    "7250330": "Puerto Eden",
    "7250340": "Puerto Natales",
    "7250350": "Faro Islote Evangelistas",
    "7250360": "Faro Fairway",
    "7250370": "Bahia Felix",
    "7250380": "Punta Arenas",
    "7250390": "Faro Delgada",
    "7250400": "Faro Dungeness",
    "7250410": "Faro Espiritu Santo",
    "7250420": "Puerto Williams",
    "7250430": "Wollaston",
    "7250440": "Diego Ramirez",
    "7250450": "Bahia Fildes/Antarctica",
    "7250460": "Base A",
    "7250470": "Bahia Paraiso/Antarctica",
    "7251860": "Valparaiso Playa Ancha",
    "7354750": "Guayaquil",
    "7354752": "Esmeraldas",
    "7354753": "Bahia",
    "7354754": "Manta",
    "7354755": "Salinas",
    "7354756": "Puerto Bolivar",
    "7354757": "Ayora",
    "7354758": "Baquerizo Moreno",
    "7600120": "Zorritos",
    "7600121": "Paita",
    "7600122": "Talara",
    "7600123": "Pimentel",
    "7600124": "Salaverry",
    "7600125": "Callao",
    "7600126": "Chimbote",
    "7600127": "Supe",
    "7600128": "Huacho",
    "7600129": "Mollendo",
    "7600130": "Pisco",
    "7600131": "S Juan/S Nicolas",
    "7600132": "Ilo",
    "7600133": "Iquitos",
    "7703870": "Montevideo"
};

}
