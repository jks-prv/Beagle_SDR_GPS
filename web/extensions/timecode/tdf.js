// Copyright (c) 2017-2024 John Seamons, ZL4VO/KF6VO

var tdf = {
   arm: 0,
   mod_threshold: 0.3,
   no_modulation: 0,
   slice_period: 150,
   slice_threshold: 5,
   modct: 0,
   sec: 0,
   msec: 0,
   line: 0,
   dcnt: 0,
   
   end: null
};

function tdf_dmsg(s)
{
   w3_innerHTML('id-tc-tdf', s);
}

// see: en.wikipedia.org/wiki/TDF_time_signal
function tdf_legend()
{
   if ((tdf.line++ & 3) == 0)
      tc_dmsg('0 LS hhhh 000000 HH 0 ASN 01 mmmmmmm p hhhhhh p dddddd www mmmmm yyyyyyyy p 0<br>');
}

function tdf_decode(bits)
{
   // bits are what the minute _will be_ at the approaching minute boundary
   
   var min  = tc_bcd(bits, 21, 7, 1);
   var hour = tc_bcd(bits, 29, 6, 1);
   var day  = tc_bcd(bits, 36, 6, 1);
   var wday = tc_bcd(bits, 42, 3, 1);
   var mo   = tc_bcd(bits, 45, 5, 1) - 1;
   var yr   = tc_bcd(bits, 50, 8, 1) + 2000;
   var tz   = bits[17]? 'CEST' : (bits[18]? 'CET' : 'TZ?');

   tc_dmsg('  '+ day +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +' '+ tz +'<br>');
   tc_stat('lime', 'Time: '+ day +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +' '+ tz);
}

function tdf_clr()
{
   var t = tdf;
   
   t.arm = t.no_modulation = t.dcnt = t.modct = t.line = t.sec = t.msec = 0;
}

// called at 100 Hz (10 msec)
function tdf_phase(ampl)
{
   var t = tdf;
   var abs_ampl = Math.abs(ampl);
   
   if (abs_ampl < t.mod_threshold) {
      // no modulation

      if (tc.no_modulation == 100) {      // 59th second -- period of no phase transitions
         t.arm = 1;
         t.modct = 0;
      }
      tc.no_modulation++;
   } else {
      // modulation
      
      // sync when armed and the first modulation occurs 
      if (t.arm && abs_ampl >= t.mod_threshold) {
         if (t.modct == 0) {
            scope_clr();
            tc.mkr = 1;
            t.sec = 0;
            t.msec = 0;
            if (tc.state == tc.ACQ_SYNC) {
               tc_stat('cyan', 'Found sync');
            } else {
               tdf_decode(tc.raw);   // for the minute just reached
            }
            tdf_legend();
            tc.raw = [];
            t.dcnt = 0;
            tc.state = tc.ACQ_DATA;
         }
         t.arm = 0;
      }

      tc.no_modulation = 0;
      t.modct++;
   }

   t.msec += 10;

   if (t.msec == 1000) {
      t.sec++;
      t.msec = 0;
      t.modct = 0;
   }

   // after slice_period msec the modct versus slice_threshold determines if one or zero received
   if (tc.state == tc.ACQ_DATA && t.msec == t.slice_period) {
      tc.mkr = 2;
      //tc_dmsg(t.modct +' ');
      var b = (t.modct >= t.slice_threshold)? 1:0;
      tc.trig = b+2;
      tc.raw[t.sec] = b;
      tc_dmsg(b);
      if ([0,2,6,12,14,15,18,20,27,28,34,35,41,44,49,57,58].includes(t.dcnt)) tc_dmsg(' ');
      t.dcnt++;
      if (t.dcnt == 60) t.dcnt = 0;
   } else
      tc.trig = 1;
}

function tdf_focus()
{
}

function tdf_blur()
{
   var el;
	el = w3_el('id-tc-tdf');
	if (el) el.innerHTML = '';
}

function tdf_init()
{
   w3_el('id-tc-addon').innerHTML += w3_div('id-tc-tdf');
}
