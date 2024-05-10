// Copyright (c) 2017-2024 John Seamons, ZL4VO/KF6VO

//
// TODO
//    phase decoding
//    The protocol is unpublished, so some of the data bit values are still unknown.
//

var bpc = {
   // ampl
   AMPL_NOISE_THRESHOLD: 3,
   arm: 0,
   data: 0,
   data_last: 0,

   // phase
   // ...

   cur: 0,
   sec: 0,
   msec: 0,
   dcnt: 0,
   line: 0,
   prev: [],
   raw: [],
   chr: 0,
   tod: 0,
   time: 0,
   dibit: [ '00', '01', '10', '11' ],
   
   end: null
};

function bpc_dmsg(s)
{
   w3_innerHTML('id-tc-bpc', s);
}

// see: en.wikipedia.org/wiki/BPC_(time_signal)
function bpc_legend()
{
   if ((bpc.line & 3) == 0) {
      //tc_dmsg('         ');
      tc_dmsg('ss hhhhhh mmmmmm ?????? dddddd mmmm yyyyyy ?? <br>');
   }
}

function bpc_decode(b)
{
   // bits are what time the 20 second frame _was_ at the previous sync boundary
   
   var hour = b[1]*16 + b[2]*4 + b[3];
   if (hour <= 11) {
      var pm = b[9] & 2;
      if (pm) hour += 12;
      hour = hour.leadingZeros(2);
   } else hour = '??';

   var min = b[4]*16 + b[5]*4 + b[6];
   min = (min > 59)? '??' : min.leadingZeros(2);

   var sec = b[0];   // 0..2
   sec = (sec > 2)? '??' : (sec*20).leadingZeros(2);
   
   var day = b[10]*16 + b[11]*4 + b[12];
   day = (day == 0 || day > 31)? '??' : day.fieldWidth(2);

   var mo = b[13]*4 + b[14];  // 1..12
   mo = (mo == 0 || mo > 12)? '???' : tc.mo[mo-1];

   var yr = b[15]*16 + b[16]*4 + b[17] + 2000;
   
   var s = day +' '+ mo +' '+ yr +' '+ hour +':'+ min +':'+ sec +' CST';

/*
   var parity_check = (b[8] & 1) ^ 1;
   var parity_sum = 0;
   for (var i = 0; i < 19; i++) {
      parity_sum += [0,1,1,2][b[i]];
   }
   var parity_error = ((parity_sum & 1) != parity_check)? false : true;

   var p = parity_error? 'PARITY ERROR' : '';
   var p2 = parity_error? '<span style="color:orange">PARITY ERROR</span>' : '';
   tc_dmsg('   '+ s +'  '+ p2 +'<br>');
   tc_stat('lime', 'Time: '+ (parity_error? p : s));
*/

   tc_dmsg('   '+ s +'<br>');
   tc_stat('lime', 'Time: '+ s);
}

function bpc_clr()
{
   var b = bpc;
   
   b.data = b.data_last = 0;
   b.cur = b.cnt = b.one_width = b.zero_width = 0;
   b.arm = b.no_modulation = b.dcnt = b.modct = b.line = b.sec = b.msec = 0;
   tc.trig = 0;

   if (server_time_local != null && server_time_local != '') {
      b.time = parseInt(server_time_local)*3600 + parseInt(server_time_local.substr(3))*60 + 60;
   }
   
   //b.time = 7*3600 + 59*60;
   b.tod = b.time;
}

// called at 100 Hz (10 msec)
function bpc_ampl(ampl)
{
	var i;
	var b = bpc;
	tc.trig++; if (tc.trig >= 100) tc.trig = 0;
	ampl = (ampl > 0.5)? 1:0;
	ampl = ampl ^ tc.force_rev ^ tc.data_ainv;
	
	// de-noise signal
   if (ampl == b.cur) {
   	b.cnt = 0;
   } else {
   	b.cnt++;
   	if (b.cnt > b.AMPL_NOISE_THRESHOLD) {
   		b.cur = ampl;
   		b.cnt = 0;
   		//if (tc.state == tc.ACQ_SYNC)
   		//   tc_dmsg((b.data? '1':'0') +':'+ (b.data? b.one_width : b.zero_width) +' ');
   		//if (tc.state == tc.ACQ_SYNC && !b.data)
   		//   tc_dmsg(b.zero_width +' ');
	      b.data_last = b.data;
	      if (!tc.ref) { b.data = ampl; tc.ref = 1; } else { b.data ^= 1; }
   		//if (b.data) b.one_width = 0; else b.zero_width = 0;

   		if (b.data) {
   		   // zero-to-one transition
   		   //tc_dmsg('0-'+ b.zero_width +' ');
   		   //b.chr += 5;
   		   if (tc.state != tc.ACQ_PHASE && tc.state != tc.ACQ_SYNC) {
   		   /*
   		      if (b.dcnt == 0) {
   		         var secs = b.tod;
   		         var hh = Math.floor(secs/3600);
   		         secs -= hh * 3600;
   		         var mm = Math.floor(secs/60);
   		         secs -= mm * 60;
   		         tc_dmsg(hh.leadingZeros(2) +':'+ mm.leadingZeros(2) +':'+ secs.leadingZeros(2) +' ');
   		         b.tod += 20;
   		      }
   		   */
   		      
   		      // each pulse is 1, 2, 3 or 4 ten msec periods of no carrier
   		      // representing the dibit values 00, 01, 10 and 11 respectively
   		      var t = Math.round((b.zero_width - 9) / 10);
   		      if (t >= 4) t = 0;
   		      b.raw[b.dcnt] = t;
   		      var s = b.dibit[t];
               if (t != b.prev[b.dcnt] && b.line) {
                  s = '<span style="color:lime">'+ s +'</span>';
               }
               b.prev[b.dcnt] = t;
               tc_dmsg(s);
               if ([0,3,6,9,12,14,17].includes(b.dcnt)) tc_dmsg(' ');
               //b.chr += s.length;
               //if (b.chr > 80) { tc_dmsg('<br>'); b.chr = 0; }
               b.dcnt++;
               
               // each 20 sec frame is sync followed by 19 data dibits
               if (b.dcnt >= 19) {
                  bpc_decode(b.raw);
                  b.dcnt = 0; b.line++;
                  bpc_legend();
               }
            }
   		   b.one_width = 0;
   		} else {
   		   // one-to-zero transition
   		   //tc_dmsg('1-'+ b.one_width +' ');
   		   //b.chr += 5;
   		   //if (b.chr > 80) { tc_dmsg('<br>'); b.chr = 0; }
   		   b.zero_width = 0;
   		}
   	}
   }

   if (b.data) b.one_width++; else b.zero_width++;
	
	// sync is 1900 ms (2 sec - 10 ms) of carrier every 20 sec
	if (tc.state == tc.ACQ_SYNC && b.arm == 0 && b.one_width >= 170) { b.arm = 1; b.one_width = 0; }
	if (b.arm == 1 && b.data_last == 1 && b.data == 0) {
	   b.arm = 2;
	   tc.state = tc.SYNCED;
      tc.trig = 0;
      tc.sample_point = 40;
	   bpc_legend();
      tc_stat('cyan', 'Found sync');

	}

   b.msec += 10;

   if (b.msec == 1000) {
      b.sec++;
      b.msec = 0;
   }

   tc.data = b.data;
}

function bpc_focus()
{
}


function bpc_blur()
{
   var el;
	el = w3_el('id-tc-bits');
	if (el) el.innerHTML = '';
	el = w3_el('id-tc-bpc');
	if (el) el.innerHTML = '';
}


function bpc_init()
{
   w3_el('id-tc-addon').innerHTML += w3_div('id-tc-bits') + w3_div('id-tc-bpc');
}
