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
	w3_el('id-tc-dbug').innerHTML = tc_dbug;
}

var isFullMin, sec_per_sweep;

function tc_stat(color, s)
{
   w3_el('id-tc-status').style.color = color;
   w3_el('id-tc-status').innerHTML = s;
}

function tc_stat2(color, s)
{
   w3_el('id-tc-status2').style.color = color;
   w3_el('id-tc-status2').innerHTML = s;
}

var tc = {
	srate:		0,
	srate_2pi:  0,
	interval:	0,
	
	need_pll:   0,
	
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

var tc_pll = {
   phase:      0,
   freq:       0,
   lpf_coeff:  1e-2,
   lpf_re:     0,
   lpf_im:     0,
   lpf_ph_err: 0,
   alpha:      1e-12,
   beta:       1e-7,
};

function tc_audio_data_cb(samps, nsamps)
{
   for (var i = 0; i < nsamps; i++) {

      // if needed, run a phase-tracking PLL on the sample stream
      if (tc.need_pll) {
         var samp_re = samps[i];

         // real-to-complex mixer
         var mix_re = samp_re * Math.cos(tc_pll.phase);
         var mix_im = samp_re * Math.sin(tc_pll.phase);
         tc_pll.lpf_re += tc_pll.lpf_coeff * (mix_re - tc_pll.lpf_re);
         tc_pll.lpf_im += tc_pll.lpf_coeff * (mix_im - tc_pll.lpf_im);
   
         var ph_err = Math.atan2(tc_pll.lpf_re, tc_pll.lpf_im);		// +/- pi max
         tc_pll.lpf_ph_err += tc_pll.lpf_coeff * (ph_err - tc_pll.lpf_ph_err);
   
         var phase_err =
            tc_pll.lpf_re;
            //ph_err;
            //tc_pll.lpf_ph_err;
         tc_pll.freq += phase_err * tc_pll.alpha;
         tc_pll.phase += tc_pll.freq + (phase_err * tc_pll.beta);
      }
      
      // display variables
      var d_ampl = samp_re / 15000;
      var d_phase = -tc_pll.lpf_ph_err;		// +/- pi max
		var slice_data = (d_phase > 1.5)? 1 : ((d_phase < -1.5)? 1 : 0);
		var d_slice = slice_data? 3.1 : undefined;

		switch (tc_config.sig) {

		case tc_sig.test:
			tc_test(samp_real, d_phase, slice_data);
      	tc_scope(
      	   d_slice,
      	   (tc_trig == tc_sample_point)? 2.5 : ampl,
      	   tc_phdata? 1.8 : undefined,
      	   d_phase
      	);
			break;

		case tc_sig.WWVBp:
      	tc_scope(
      	   d_slice,
      	   (tc_trig == tc_sample_point)? 2.5 : ampl,
      	   tc_phdata? 1.8 : undefined,
      	   d_phase
      	);
			break;

		case tc_sig.MSF:
      	tc_scope(
      	   d_slice,
      	   tc.mkr? 2.5 : ampl,
      	   d_phase,
      	   undefined
      	);
			break;

		case tc_sig.TDF:
      	tc_scope(
      	   d_phase*2,
      	   tc.mkr? 2.5 : 0,
      	   undefined,
      	   undefined
      	);
			break;

		default:
      	tc_scope(
      	   d_phase*2,
      	   tc.mkr? 2.5 : 0,
      	   undefined,
      	   undefined
      	);
			break;
		}
		
   }
}


function tc_recv_msg(data)
{
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('tc_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('tc_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
            kiwi_load_js_dir('extensions/timecode/', ['wwvb.js', 'tdf.js'], 'tc_controls_setup');
				break;

			default:
				console.log('tc_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var tc_sig = { JJY40:0, WWVBa:1, WWVBp:2, JJY60:3, MSF:4, BPC:5, DCF77a:6, TDF:7, test:8 };
var tc_sig_s = [
	'40 kHz JJY',
	'60 kHz WWVB ampl',
	'60 kHz WWVB phase',
	'60 kHz JJY',
	'60 kHz MSF',
	'68.5 kHz BPC',
	'77.5 kHz DCF77 ampl',
	'162 kHz TDF',
   '1107 kHz test'
];
var tc_freq = { 0:40, 1:60, 2:60, 3:60, 4:60, 5:68.5, 6:77.5, 7:162, 8:1107 };
var tc_config = { sig: tc_sig.WWVBp };

function tc_controls_setup()
{
   var data_html =
      time_display_html('tc') +

		w3_div('id-tc-data|width:1024px; height:200px; background-color:black; position:relative;',
			'<canvas id="id-tc-scope" width="1024" height="200" style="position:absolute"></canvas>'
		);

	var controls_html =
		w3_div('id-tc-controls w3-text-white',
			w3_div('w3-medium w3-text-aqua', '<b>Time station timecode decoder</b>'),
			w3_divs('w3-margin-T-8/w3-show-inline w3-margin-right',
				w3_select('', '', '', 'tc_config.sig', tc_config.sig, tc_sig_s, 'tc_signal_menu_cb'),
				w3_div('id-tc-status w3-show-inline-block'),
				w3_div('id-tc-status2 w3-show-inline-block')
			),
			'<pre>'+
				'<span id="id-tc-dbug"></span>'+
			'</pre>'
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('tc');
	tc_environment_changed( {resize:1} );
	
	ext_set_controls_width_height(1024);
	
	var p = ext_param();
	if (p) {
	   var i;
      p = p.toLowerCase();
      for (i = 0; i < tc_sig_s.length; i++) {
         if (tc_sig_s[i].toLowerCase().includes(p))
            break;
      }
      if (i < tc_sig_s.length) {
         w3_select_value('tc_config.sig', i);
         tc_config.sig = i;
      }
   }

	tc.scope_ct = w3_el('id-tc-scope').getContext("2d");
	tc_scope_clr();

	tc.srate = ext_sample_rate();
	console.log('srate='+ tc.srate);
	
	// receive the network-rate, post-decompression, real-mode samples
	ext_register_audio_data_cb(tc_audio_data_cb);
	
	// FIXME: use a different call that isn't privileged and doesn't leak location
	//ext_send("SET gps_update");
	//tc.interval = setInterval(function() {ext_send("SET gps_update");}, 1000);
}

function tc_environment_changed(changed)
{
   if (!changed.resize) return;
	var el = w3_el('id-tc-data');
	var left = (window.innerWidth - 1024 - time_display_width()) / 2;
	el.style.left = px(left);
}

function tc_signal_menu_cb(path, val)
{
	val = +val;
	tc_config.sig = val;
	tc_dmsg();
	tc_state = tc_st.ACQ_SYNC;
	tc_stat('yellow', 'Looking for sync');
	tc_scope_clr();
	
	// defaults
	tc.need_pll = 1;
   isFullMin = 0;
   sec_per_sweep = 10;

	switch (val) {

	case tc_sig.test:
		ext_tune(tc_freq[val], 'cwn', ext_zoom.ABS, 12);
		tc.need_pll = 1;
		break;

	case tc_sig.WWVBp:
		ext_tune(tc_freq[val], 'cwn', ext_zoom.ABS, 12);
		break;

	case tc_sig.MSF:
		ext_tune(tc_freq[val], 'cwn', ext_zoom.ABS, 12);
		break;

	case tc_sig.TDF:
		ext_tune(tc_freq[val], 'cwn', ext_zoom.ABS, 12);
		sec_per_sweep = 1;
		break;
	
	default:
		ext_tune(tc_freq[val], 'cwn', ext_zoom.ABS, 12);
		sec_per_sweep = 1;
		break;

	}
	
	if (tc.need_pll) {
	   tc.srate = ext_sample_rate();
	   tc.srate_2pi = tc.srate * 2*Math.PI;
	   var pb = ext_get_passband();
	   var pb_cf = pb.low + (pb.high - pb.low)/2;
	   tc_pll.phase = 0;
	   tc_pll.freq = pb_cf / tc.srate_2pi;
	}
}

function timecode_blur()
{
	kiwi_clearInterval(tc.interval);
	ext_unregister_audio_data_cb();
}
