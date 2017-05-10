// Copyright (c) 2017 John Seamons, ZL/KF6VO

var timecode_ext_name = 'timecode';		// NB: must match timecode.c:timecode_ext.name

var tc_first_time = true;

function timecode_main()
{
	ext_switch_to_client(timecode_ext_name, tc_first_time, tc_recv_msg);		// tell server to use us (again)
	if (!tc_first_time)
		tc_controls_setup();
	tc_first_time = false;
}

var tc_dbug = '';

function tc_dmsg(s)
{
	if (s == undefined)
		tc_dbug = '';
	else
		tc_dbug += s;
	w3_el_id('id-tc-dbug').innerHTML = tc_dbug;
}

var isFullMin, sec_per_sweep;

function tc_stat(color, s)
{
   w3_el_id('id-tc-status').style.color = color;
   w3_el_id('id-tc-status').innerHTML = s;
}

function tc_stat2(color, s)
{
   w3_el_id('id-tc-status2').style.color = color;
   w3_el_id('id-tc-status2').innerHTML = s;
}

var tc = {
	srate:		0,
	interval:	0,
	
	scope_ct:	null,
	scope_run:	1,
	scope_bg:	'#f2f2f2',
	
	ten_sec:		0,
	mkr_per:		0,
	mkr:			0,
};

function tc_scope_clr()
{
}

function tc_scope(t1, t2, t3, t4)
{
}

function tc_test(samp, ph, slice)
{
}

var bits=[];
var dbits='';

var tc_cur=0, tc_phdata=0, tc_phdata_last=0, tc_inverted;
var tc_cnt=0, tc_width=0, tc_trig=0, tc_sample_point=-1, tc_dcnt, tc_recv=0, tc_min;
var TC_NOISE_THRESHOLD=15;
var tc_data = [];
var tc_sync = [ 0,0,0,1,1,1,0,1,1,0,1,0,0,0 ];

var tc_mo = [ 'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec' ];
//             jan feb mar apr may jun jul aug sep oct nov dec
var tc_dim = [ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 ];
var tc_MpD = 24 * 60, tc_MpY = tc_MpD * 365;
var tc_time0, tc_time0_copy;

var tc_sixmin_mode=0;

var tc_state, tc_st = { ACQ_SYNC:0, ACQ_DATA:1, ACQ_SIXMIN:2 };

// example: 6578970(min)/1440(min/day) = 4568.730, 6578970-(1440*4568) = 1050(min)
// for 2012: 365*12 (2000..2011) + 3(LY: 2000, 2004, 2008) = 4383, 4568-4383=185(days)
// 2012: 1/31 + 2/29(LY) + 3/31 + 4/30 + 5/31 + 6/30 = 182 days (#181 base-0), so day #185 = July 4 2012
// 1050(min) = 60*17(hrs) + 30(min), so timecode is between 17:30..17:31 UTC

function wwvb_sync()
{
	var l = tc_data.length;
	var sync = 1;
	for (i = 0; i < 14; i++) {
		if (tc_data[l-(14-i)] != tc_sync[i]) {
			sync = 0;
			break;
		}
	}
	if (!sync) {
			sync = 2;
			for (i = 0; i < 14; i++) {
				if (tc_data[l-(14-i)] != (tc_sync[i] ^ 1)) {
					sync = 0;
					break;
				}
			}
	}
	
	return sync;
}

function wwvb(d)
{
	var i;
	tc_trig++; if (tc_trig >= 100) tc_trig = 0;
	
   if (d == tc_cur) {
   	tc_cnt = 0;
   } else {
   	tc_cnt++;
   	if (tc_cnt > TC_NOISE_THRESHOLD) {
   		tc_cur = d;
   		tc_cnt = 0;
   		if (tc_cur) {
   			tc_phdata ^= 1;		// flip phase data on 0 -> 1 transition of phase reversal
   		}
   	}
   }

	if (tc_phdata) {
		tc_width++;
	}
	
	// if we need it, measure width on falling edge tc_phdata
	if (tc_sample_point == -1 && tc_phdata_last == 1 && tc_phdata == 0) {		
		tc_dmsg(tc_width.toString() +' ');
		if (tc_width > 50 && tc_width < 110) {
			tc_sample_point = tc_trig - Math.round(tc_width/2);
			if (tc_sample_point < 0) tc_sample_point += 100;
			tc_dmsg('T='+ tc_sample_point.toString() +' ');
		}
		tc_width = 0;
	}
	
	if (tc_sample_point == tc_trig) {
		if (tc_state == tc_st.ACQ_SYNC) {
			tc_dmsg(tc_phdata.toString());
			tc_data.push(tc_phdata);
			
			if (tc_data.length >= 14) {
				tc_inverted = 0;
				var sync = wwvb_sync();
				if (sync == 2) tc_inverted = 1;
				if (sync) {
					tc_stat('magenta', 'Found sync');
					tc_dmsg(' SYNC'+ (tc_inverted? '-I':'') +'<br>00011101101000 ');
					tc_dcnt = 13;
					tc_state = tc_st.ACQ_DATA;
					tc_data = [];
					tc_min = 0;
				}
			}

			tc_recv++;
			if (tc_recv > (65 + Math.round(Math.random()*20))) {		// no sync, pick another sample point
				tc_dmsg(' RESTART<br>');
				tc_sample_point = -1;
				tc_width = 0;
				tc_recv = 0;
			}
		} else
		
		if (tc_state == tc_st.ACQ_DATA) {
			var data = tc_phdata ^ tc_inverted;
			tc_data.push(data);
			tc_dmsg(data.toString());
			if (tc_dcnt == 12 ||
					tc_dcnt == 17 ||
					tc_dcnt == 28 ||
					tc_dcnt == 29 ||
					tc_dcnt == 38 ||
					tc_dcnt == 39 ||
					tc_dcnt == 46 ||
					tc_dcnt == 52)
				tc_dmsg(' ');
			
			if (tc_dcnt == 18 || (tc_dcnt >= 20 && tc_dcnt <= 46 && tc_dcnt != 29 && tc_dcnt != 39)) {
				tc_min = (tc_min << 1) + data;
				//console.log(tc_dcnt +': '+ data +' '+ tc_min);
			}

			if (tc_dcnt == 19) tc_time0_copy = data;
			if (tc_dcnt == 46) tc_time0 = data;
			
			if (tc_data.length >= 14 && tc_dcnt == 12) {
				var sync = wwvb_sync();
				if (!sync) {
					tc_dmsg('RESYNC<br>');
					tc_state = tc_st.ACQ_SYNC;
					tc_sample_point = -1;
					tc_width = 0;
					tc_recv = 0;
				}
				if ((sync == 1 && tc_inverted) || (sync == 2 && !tc_inverted)) {
					tc_dmsg(' PHASE INVERSION<br>00011101101000 ');
					tc_inverted ^= 1;
				}
			}
			
			// NB: This only works for the 21st century.
			if (tc_dcnt == 58) {
				if (tc_min && tc_time0_copy != tc_time0) {
					tc_dmsg(' TIME BIT-0 MISMATCH');
				} else
				if (tc_min) {
					tc_dmsg(' '+ tc_min.toString());
					console.log('start '+ tc_min);
					var m = tc_min - (tc_MpY + tc_MpD);	// 2000 is LY
					m++;		// minute increments just as we print decode
					console.log('2000: '+ m);
					var yr = 2001;
					var LY;
					while (true) {
						LY = ((yr & 3) == 0);
						var MpY = tc_MpY + (LY? tc_MpD : 0);
						if (m > MpY) {
							m -= MpY;
							console.log(yr +': '+ MpY +' '+ m);
							yr++;
						} else {
							break;
						}
					}
					console.log('yr done: '+ m);
					
					var mo = 0;
					while (true) {
						var days = tc_dim[mo];
						if (mo == 1 && LY) days++;
						var MpM = tc_MpD * days;
						console.log(tc_mo[mo] +': LY'+ LY +' '+ days);
						if (m > MpM) {
							m -= MpM;
							console.log(tc_mo[mo] +': '+ MpM +' '+ m);
							mo++;
						} else {
							break;
						}
					}
					console.log('mo done: '+ m +' '+ (m/24));
					var day = Math.floor(m / tc_MpD);
					m -= day * tc_MpD;
					var hour = Math.floor(m / 60);
					m -= hour * 60;
					tc_dmsg(' '+ (day+1) +' '+ tc_mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ m.leadingZeros(2) +' UTC');
					tc_stat('lime', 'Time decoded: '+ (day+1) +' '+ tc_mo[mo] +' '+ yr +' '+ hour.leadingZeros(2) +':'+ m.leadingZeros(2) +' UTC');
					
					if (m == 10 || m == 40) tc_sixmin_mode = 1;
				}
				
				tc_dmsg('<br>');
				tc_data = [];
				tc_min = 0;
			}
			
			tc_dcnt++;
			if (tc_dcnt == 60) tc_dcnt = 0;

			if (tc_dcnt == 0 && tc_sixmin_mode) {
				tc_sixmin_mode = 0;
				tc_state = tc_st.ACQ_SIXMIN;
			}
		} else

		if (tc_state == tc_st.ACQ_SIXMIN) {
			if (tc_dcnt == 0) tc_dmsg('127b: ');
			if (tc_dcnt == 127) tc_dmsg('<br>106b: ');
			if (tc_dcnt == 233) tc_dmsg('<br>127b: ');

			var data = tc_phdata ^ tc_inverted;
			tc_dmsg(data.toString());

			tc_dcnt++;
			if (tc_dcnt == 360) {
				tc_dmsg('<br>');
				tc_dcnt = 0;
				tc_state = tc_st.ACQ_DATA;
			}
		}
	}

	tc_phdata_last = tc_phdata;
}

function msf(d)
{
}

function tdf(d)
{
}

function tc_audio_data_cb(samps, nsamps)
{
}


function tc_recv_msg(data)
{
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (1 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('tc_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('tc_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				tc_controls_setup();
				break;

			default:
				console.log('tc_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var tc_sig = { JJY40:0, WWVBa:1, WWVBp:2, JJY60:3, MSF:4, BPC:5, DCF77a:6, TDF:7, test:8 };
var tc_sig_s = {
	0:	'40 kHz JJY',
	1:	'60 kHz WWVB ampl',
	2:	'60 kHz WWVB phase',
	3:	'60 kHz JJY',
	4:	'60 kHz MSF',
	5:	'68.5 kHz BPC',
	6:	'77.5 kHz DCF77 ampl',
	7:	'162 kHz TDF',
	8:	'610 kHz test',
};
var tc_freq = { 0:40, 1:60, 2:60, 3:60, 4:60, 5:68.5, 6:77.5, 7:162, 8:610 };
//jksd var tc_config = { sig: tc_sig.WWVBp };
var tc_config = { sig: tc_sig.test };

function tc_controls_setup()
{
   var data_html =
      '<div id="id-tc-time-display" style="top:50px; background-color:black; position:relative;"></div>' +

		'<div id="id-tc-data" class="scale" style="width:1024px; height:200px; background-color:black; position:relative; display:none" title="tc">' +
			'<canvas id="id-tc-scope" width="1024" height="200" style="position:absolute"></canvas>' +
		'</div>';

	var controls_html =
		w3_divs('id-tc-controls w3-text-white', '',
			w3_divs('', 'w3-medium w3-text-aqua', '<b>Time station timecode decoder</b>'),
			w3_divs('w3-margin-T-8', 'w3-show-inline w3-margin-right',
				w3_select('', '', 'tc_config.sig', tc_config.sig, tc_sig_s, 'tc_signal_cb'),
				w3_inline('id-tc-status', ''),
				w3_inline('id-tc-status2', '')
			),
			'<pre>'+
				'<span id="id-tc-dbug"></span>'+
			'</pre>'
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('id-tc-time-display');
	ext_set_controls_width(1024);
	
	tc.scope_ct = w3_el_id('id-tc-scope').getContext("2d");
	tc_scope_clr();

	visible_block('id-tc-data', 1);
	
	tc.srate = ext_sample_rate();
	console.log('srate='+ tc.srate);
	ext_register_audio_data_cb(tc_audio_data_cb);
	
	ext_send("SET gps_update");
	tc.interval = setInterval(function() {ext_send("SET gps_update");}, 1000);
}

function tc_signal_cb(path, val)
{
	val = +val;
	tc_config.sig = val;
	tc_dmsg();
	tc_state = tc_st.ACQ_SYNC;
	tc_stat('yellow', 'Looking for sync');
	tc_scope_clr();
	
	switch (val) {

	case tc_sig.test:
		ext_tune(tc_freq[val], 'cwn', ext_zoom.ABS, 12);
		isFullMin = 0;
		sec_per_sweep = 10;
		break;

	case tc_sig.WWVBp:
		ext_tune(tc_freq[val], 'cwn', ext_zoom.ABS, 12);
		isFullMin = 0;
		sec_per_sweep = 10;
		break;

	case tc_sig.MSF:
		ext_tune(tc_freq[val], 'cwn', ext_zoom.ABS, 12);
		isFullMin = 0;
		sec_per_sweep = 10;
		break;

	case tc_sig.TDF:
		ext_tune(tc_freq[val], 'cwn', ext_zoom.ABS, 12);
		isFullMin = 0;
		sec_per_sweep = 1;
		break;
	
	default:
		ext_tune(tc_freq[val], 'cwn', ext_zoom.ABS, 12);
		isFullMin = 0;
		sec_per_sweep = 1;
		break;

	}
}

function timecode_blur()
{
	kiwi_clearInterval(tc.interval);
	ext_unregister_audio_data_cb();
	visible_block('id-tc-data', 0);
}
