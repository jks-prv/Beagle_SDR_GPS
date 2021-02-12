// Copyright (c) 2017-2021 John Seamons, ZL/KF6VO

//
// Just display the bits. The actual decode is currently unknown.
//

var bpc = {
   arm: 0,
   NOISE_THRESHOLD: 3,
   cur: 0,
   sec: 0,
   msec: 0,
   dcnt: 0,
   line: 0,
   prev: [],
   crnt: [],
   diff: [],
   raw: [],
   chr: 0,
   tod: 0,
   time: 0,
   dibit: [ '00', '01', '10', '11', '--' ],
   
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
      tc_dmsg('ss hhhhhh mmmmmm ????P? dddddd mmmm yyyyyy ?? <br>');
   }
}

function bpc_decode(b)
{
   var hour = b[1]*16 + b[2]*4 + b[3];
   var pm = b[9] & 2;
   if (pm) hour += 12;
   var min = b[4]*16 + b[5]*4 + b[6];
   var sec = b[0]*20;
   
   var day = b[10]*16 + b[11]*4 + b[12];
   var mo = b[13]*4 + b[14] - 1;
   var yr = b[15]*16 + b[16]*4 + b[17] + 2000;
   
   tc_dmsg('   '+ day +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +':'+ sec.leadingZeros(2) +' CST<br>');
   tc_stat('lime', 'Time decoded: '+ day +' '+ tc.mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ min.leadingZeros(2) +':'+ sec.leadingZeros(2) +' CST');
}

function bpc_clr()
{
   var m = bpc;
   
   m.cur = m.cnt = m.one_width = m.zero_width = 0;
   m.arm = m.no_modulation = m.dcnt = m.modct = m.line = m.sec = m.msec = 0;
   tc.trig = 0;
   tc.state = tc.ACQ_SYNC;

   if (server_time_local != null && server_time_local != '') {
      m.time = parseInt(server_time_local)*3600 + parseInt(server_time_local.substr(3))*60 + 60;
   }
   
   //m.time = 7*3600 + 59*60;
   m.tod = m.time;
}

// called at 100 Hz (10 msec)
function bpc_ampl(ampl)
{
	var i;
	var m = bpc;
	tc.trig++; if (tc.trig >= 100) tc.trig = 0;
	ampl = (ampl > 0.5)? 1:0;
	if (!tc.ref) { tc.data = ampl^1; tc.ref = 1; }
	
	// de-noise signal
   if (ampl == m.cur) {
   	m.cnt = 0;
   } else {
   	m.cnt++;
   	if (m.cnt > m.NOISE_THRESHOLD) {
   		m.cur = ampl;
   		m.cnt = 0;
   		//if (tc.state == tc.ACQ_SYNC)
   		//   tc_dmsg((tc.data? '1':'0') +':'+ (tc.data? m.one_width : m.zero_width) +' ');
   		//if (tc.state == tc.ACQ_SYNC && !tc.data)
   		//   tc_dmsg(m.zero_width +' ');
	      tc.data_last = tc.data;
   		tc.data ^= 1;
   		//if (tc.data) m.one_width = 0; else m.zero_width = 0;
   		if (tc.data) {
   		   //tc_dmsg('0-'+ m.zero_width +' ');
   		   //m.chr += 5;
   		   if (tc.state != tc.ACQ_SYNC) {
   		   /*
   		      if (m.dcnt == 0) {
   		         var secs = m.tod;
   		         var hh = Math.floor(secs/3600);
   		         secs -= hh * 3600;
   		         var mm = Math.floor(secs/60);
   		         secs -= mm * 60;
   		         tc_dmsg(hh.leadingZeros(2) +':'+ mm.leadingZeros(2) +':'+ secs.leadingZeros(2) +' ');
   		         m.tod += 20;
   		      }
   		   */
   		      
   		      var t = Math.round((m.zero_width - 9) / 10);
   		      if (t >= 4) t = 0;
   		      m.raw[m.dcnt] = t;
   		      var s = m.dibit[t];
               m.diff[m.dcnt] = 0;
               if (t != m.prev[m.dcnt] && m.line) {
                  s = '<span style="color:lime">'+ s +'</span>';
                  m.diff[m.dcnt] = 1;
               }
               m.prev[m.dcnt] = t;
               tc_dmsg(s);
               if ([0,3,6,9,12,14,17].includes(m.dcnt)) tc_dmsg(' ');
               //m.chr += s.length;
               //if (m.chr > 80) { tc_dmsg('<br>'); m.chr = 0; }
               m.dcnt++;
               if (m.dcnt >= 19) {
               /*
                  //tc_dmsg(' ');
                  for (i=0; i < 19; i++) {
                     var tt = m.prev[i];
                     m.raw[i] = (tt >= 4)? 0 : tt;
                     if (tt >= 4) tt = 4;
                     s = m.dibit[tt];
                     if (m.diff[i]) {
                        s = '<span style="color:lime">'+ s +'</span>';
                     }
                     tc_dmsg(s);
                     if ([0,3,6,9,12,14,17].includes(i)) tc_dmsg(' ');
                  }
               */
                  bpc_decode(m.raw);
                  m.dcnt = 0; m.line++;
                  bpc_legend();
               }
            }
   		   m.one_width = 0;
   		} else {
   		   //tc_dmsg('1-'+ m.one_width +' ');
   		   //m.chr += 5;
   		   //if (m.chr > 80) { tc_dmsg('<br>'); m.chr = 0; }
   		   m.zero_width = 0;
   		}
   	}
   }

   if (tc.data) m.one_width++; else m.zero_width++;
	
	// 1900 ms (2 sec - 10 ms) of carrier every 20 sec
	if (tc.state == tc.ACQ_SYNC && m.arm == 0 && m.one_width >= 170) { m.arm = 1; m.one_width = 0; }
	if (m.arm == 1 && tc.data_last == 1 && tc.data == 0) {
	   m.arm = 2;
	   tc.state = tc.MIN_MARK;
      tc.trig = 0;
      tc.sample_point = 40;
	   bpc_legend();
      tc_stat('cyan', 'Found sync');

	}

   m.msec += 10;

   if (m.msec == 1000) {
      m.sec++;
      m.msec = 0;
   }
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
