/*

OpenWebRX (c) Copyright 2013-2014 Andras Retzler <randras@sdr.hu>

This file is part of OpenWebRX.

    OpenWebRX is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenWebRX is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenWebRX. If not, see <http://www.gnu.org/licenses/>.

*/

// Copyright (c) 2015-2024 John Seamons, ZL4VO/KF6VO

/*
   searchable contents:
   
   #admin pwd panel
   #AGC
   #audio
   #audio FFT
   #band bars
   #band scale
   #canvas
   #confirmation panel
   #confirmation panel: cal ADC clock
   #confirmation panel: set freq offset
   #control panel
   #conversions
   #database lookup
   #demodulator
   #dx labels
   #extensions
   #frequency entry
   #frequency memory
   #misc
   #mobile
   #option bar
   #panel setup
   #panel support
   #panels
   #rf controls
   #right click menu
   #s-meter
   #scale
   #shortcuts
   #spectrum
   #topbar
   #ui
   #user ident
   #users
   #waterfall
   #waterfall export
   #waterfall / spectrum controls
   #websocket
   #wheel
   #zoom
*/

var owrx = {
   debug_drag: false,
   activeElementID: null,
   trace_freq_update: 0,
   
   EV_START: 0,
   EV_DRAG: 1,
   EV_END: 2,
   EV_OUTSIDE: 3,
   EV_MOUSEUP: 4,
   
   top_bar_nom_height: 67,
   hide_bars: 0,
   HIDE_TOPBAR: 1,
   HIDE_BANDBAR: 2,
   HIDE_ALLBARS: 3,
   
   SETUP_DEFAULT: 0,
   SETUP_RF_SPEC_WF: 1,
   SETUP_WF: 2,
   SETUP_DX: 3,
   SETUP_TOPBAR: 4,
   
   COMP_LAST: 0,
   COMP_ON: 1,
   COMP_OFF: 2,
   
   PB_NO_EDGE: 0,
   PB_LO_EDGE: 1,
   PB_HI_EDGE: 2,
   allow_pb_adj: false,
   allow_pb_adj_at_start: false,
   spec_pb_mkr_dragging: 0,
   
   PB_LO: 0,
   PB_HI: 1,
   PB_CENTER: 2,
   PB_WIDTH: 3,
   
   wheel_tunes: 0,
   wheel_dev: 1,
   WHEEL_MOUSE: 0,
   WHEEL_TRACKPAD: 1,
   WHEEL_CUSTOM: 2,
   wheel_dev_s: [ 'wheel mouse', 'trackpad', 'custom' ],
   wheel_poll_i: [   25,   50,   25 ],
   wheel_fast_i: [   8,    30,   8 ],
   wheel_unlock_i: [ 1000, 1250, 1000 ],
   wheel_poll: 0,
   wheel_fast: 0,
   wheel_unlock: 0,
   wheel_dir: 0,
   wheel_dir_s: [ 'normal', 'reverse' ],

   cfg_loaded: false,
   mobile: null,
   button: 0,
   buttons: 0,
   wf_snap: 0,
   wf_cursor: 'crosshair',
   last_shiftKey: false,
   last_pageX: 0,
   last_pageY: 0,
   mouse_freq: {},
   override_fmem: null,
   no_fmem_update: false,
   vfo_ab: 0,
   vfo_first: true,
   optbar_last_scrollpos: [],
   rec_init: false,
   resize_seq: 0,
   
   last_freq: -1,
   last_mode: '',
   last_locut: -1,
   last_hicut: -1,
   last_lo: [],
   last_hi: [],
   last_selected_band: 0,
   
   last_mode_el: null,
   dseq: 0,
   
   sMeter_dBm_biased: 0,
   sMeter_dBm: 0,
   smeter_interval: null,
   force_no_agc_thresh_smeter: false,
   overload_mute: 90,
   squelched_overload: false,
   prev_squelched_overload: false,
   
   touch_hold_pressed: false,
   show_cursor_freq: 0,
   tuning_locked: 0,
   freqstep_interval: null,
   lock_fast: false,
   lock_fast_timeo: null,
   ignore_freq_tune_onmouseup: false,
   
   dx_click_gid_last_stored: undefined,
   dx_click_gid_last_until_tune: undefined,
   
   sam_pll: 1,
   sam_pll_s: [ 'DX', 'med', 'fast' ],
   
   chan_null: 0,
   chan_null_s: [ 'normal', 'null LSB', 'null USB' ],
   
   SAM_opts: 3,
   SAM_opts_s: [ 'none', 'fade leveler', 'DC block', 'fade+block' ],
   SAM_opts_sft: 2,
   
   ovld_mute: 0,
   ovld_mute_s: [ 'off', 'on' ],
   
   FSET_NOP:            0,
   FSET_ADD:            1,
   FSET_SET_FREQ:       2,
   FSET_EXT_SET_PB:     3,
   FSET_PB_CHANGE:      4,
   FSET_DRAG_START:     5,
   FSET_DRAG:           6,
   FSET_DRAG_END:       7,
   FSET_MOUSE_CLICK:    8,
   FSET_TOUCH_START:    9,
   FSET_TOUCH_DRAG:     10,
   FSET_DTOUCH_DRAG:    11,
   FSET_TOUCH_DRAG_END: 12,
   FSET_SCALE_DRAG:     13,
   FSET_SCALE_END:      14,
   fset_s:     [ 'nop', 'add', 'fset', 'extSetPB', 'pbChg', 'dragStart', 'drag', 'dragEnd', 'mouseClick', 'touchStart', 'touchDrag', 'DtouchDrag', 'touchClick', 'scaleDrag', 'scaleEnd' ],
   fmem_upd:   [    0,     1,      1,          1,       0,           0,      0,         1,            1,            0,           0,            0,            1,           0,          1  ],
   LEFT_MARGIN: -1,
   NOT_MARGIN: 0,
   RIGHT_MARGIN: 1,
   
   WF_POS_AT_FREQ: 0,
   WF_POS_CENTER: 1,
   WF_POS_RECENTER_IF_OUTSIDE: 2,
   waterfall_tuned: 0,
   
   scale_canvas: {
	   mouse_out: false,
      target: null,
      restore_visibility: [],
      restore_visibility_delay: 500
   },
   
   canvas: {
      target: null,
      restore_visibility: [],
      restore_visibility_delay: 500
   },
   
   zoom_titles: [
      // zout                   zin
      [ 'zoom out',            'zoom in' ],              // NO_MODIFIER
      [ 'passband narrow',     'passband widen' ],       // SHIFT
      [ 'passband right edge', 'passband right edge' ],  // CTL_ALT
      [ 'passband left edge',  'passband left edge' ]    // SHIFT_PLUS_CTL_ALT
   ],
   
   __last: 0
};

// key freq concepts:
//		all freqs in Hz
//		center_freq is center of entire sampled bandwidth (e.g. 15 MHz)
//		offset_frequency (-bandwidth/2 .. bandwidth/2) = center_freq - freq
//		freq = center_freq + offset_frequency

// passed from server (bw/cf vary due to 30/32 MHz max freq range)
var bandwidth;
var center_freq;
var wf_fft_size;

var cur_mode;
var wf_fps, wf_fps_max;

var ws_snd, ws_wf;

var spectrum_show = 0, spectrum_param = -1;
var gen_freq = 0, gen_attn = 0;
var squelch_threshold = 0;
var wf_rate = '';
var wf_mm = '';
var wf_auto = null;
var wf_compression = 1;
var wf_interp = 13;  // WF_DROP + WF_CIC_COMP by default
var wf_winf = 2;     // WIN_BLACKMAN_HARRIS
var snd_winf = 0;    // WIN_BLACKMAN_NUTTALL
var debug_v = 0;		// a general value settable from the URI to be used during debugging
var sb_trace = 0;
var kiwi_gc = 1;
var kiwi_gc_snd = 0;
var kiwi_gc_wf = -1;
var kiwi_gc_recv = -1;
var kiwi_gc_wspr = -1;
var override_ext = null;
var muted_initially = 0;
var peak_initially1 = null, peak_initially2 = null;
var param_nocache = false;
var nocache = false;
var param_ctrace = false;
var ctrace = false;
var no_clk_corr = false;
var override_pbw = '';
var override_pbc = '';
var nb_click = false;
var no_geoloc = false;
var force_mobile = false;
var mobile_laptop_test = false;
var show_activeElement = false;
var force_need_autoscale = false;
var user_url = null;
var override_1Hz = null;

var wf_rates = { '0':0, 'off':0, '1':1, '1hz':1, 's':2, 'slow':2, 'm':3, 'med':3, 'f':4, 'fast':4 };

var okay_wf_init = false;

function kiwi_main()
{
   w3_do_when_cond(
      function() {
         //console.log('### '+ (owrx.cfg_loaded? 'GO' : 'WAIT') +' kiwi_main(cfg_loaded)');
         return owrx.cfg_loaded;
      },
      function() {
         kiwi_main_ready();
      }, null,
      200
   );
   // REMINDER: w3_do_when_cond() returns immediately
}

// capture button state for when needed inside e.g. non-mouse event handler that can't otherwise
// determine button state
function mouse_button_state(evt)
{
   if (!evt || (evt.type != 'mousedown' && evt.type != 'mouseup')) return;
   owrx.button = evt.button;
   owrx.buttons = evt.buttons;
   //event_dump(evt, 'mouse_button_state', true);
}

function kiwi_main_ready()
{
   /*
      // keyboard modifier event tester
      var el = w3_el('id-kiwi-container');
      w3_innerHTML(el, 'Keyboard modifier test');
	   if (1) window.addEventListener("keydown",
	      function(ev) {
	         event_dump(ev, 'KD', true);
	      }
	   );
	   if (1) window.addEventListener("keypress",
	      function(ev) {
	         event_dump(ev, 'KP', true);
	      }
	   );
	   if (1) window.addEventListener("keyup",
	      function(ev) {
	         event_dump(ev, 'KU', true);
	      }
	   );
	   if (1) window.addEventListener("click",
	      function(ev) {
	         event_dump(ev, 'click', true);
	      }
	   );
      return;
   */
   
   window.addEventListener("mousedown", function(evt) { mouse_button_state(evt); });
   window.addEventListener("mouseup", function(evt) { mouse_button_state(evt); });

	override_freq = parseFloat(kiwi_storeRead('last_freq'));
	override_mode = kiwi_storeRead('last_mode');
	override_zoom = parseFloat(kiwi_storeRead('last_zoom'));
	override_9_10 = parseFloat(kiwi_storeRead('last_9_10'));
	override_max_dB = parseFloat(kiwi_storeRead('last_max_dB'));
	override_min_dB = parseFloat(kiwi_storeRead('last_min_dB'));
	
	var last_vol = kiwi_storeRead('last_volume', 50);
   //console.log('last_vol='+ last_vol);
	setvolume(true, last_vol);
	
	console.log('LAST f='+ override_freq +' m='+ override_mode +' z='+ override_zoom +
		' 9_10='+ override_9_10 +' min='+ override_min_dB +' max='+ override_max_dB);

	// process URL query string parameters
	var num_win = 16;     // FIXME: should really be rx_chans, but that's not available yet 
	console.log('URL: '+ kiwi_url());
	
	var qs_parse = function(s) {
		var qd = {};
		if (s) s.split("&").forEach(function(item) {
			var a = item.split("=");
			qd[a[0]] = a[1]? a[1] : 1;		// &foo& shorthand for &foo=1&
		});
		return qd;
	};
	
	var q = window.location.search && window.location.search.substr(1);		// skip initial '?'
	var qs = [], qd = [];
	for (w = 1; w <= num_win; w++) {
		var win = '&win'+ (w+1) +'&';
		qs[w] = q && q.split(win)[0];
		q     = q && q.split(win)[1];
		qd[w] = qs_parse(qs[w]);
		qd[w].WID = w;
		//console.log(qd[w]);
	}

	var host = kiwi_url().split('?')[0];
	g_qs_idx = 2;     // NB: global var
	for (w = 2; w <= num_win; w++) {
      if (qs[w]) {
         setTimeout(function() {
               var url = host +'?'+ qs[g_qs_idx];
               g_qs_idx++;
               //console.log('OPEN '+ url);
               var win = kiwi_open_or_reload_page({ url:url, tab:1 });
               //console.log(win);
               if (win == null)
                  alert('Do you have popups blocked for this site? '+ url);
            }, (w-1) * 2000
         );
      }
	}
	
	q = qd[1];
	//console.log('this window/tab:');
	//console.log(q);
	
	var s = 'f'; if (q[s]) {
		var p = parse_freq_pb_mode_zoom(q[s]);
      // 'k' suffix is simply ignored since default frequencies are in kHz
      if (p[1]) override_freq = p[1].replace(',', '.').parseFloatWithUnits('M', 1e-3);
      if (p[1]) console.log('p='+ p[1] +' override_freq='+ override_freq);
      if (p[2]) console.log('override_pbw/pbc='+ p[2]);
		if (p[2] && p[2].charAt(0) == '/') override_pbw = p[2].substr(1);     // remove leading '/'
		if (p[2] && p[2].charAt(0) == ':') override_pbc = p[2].substr(1);     // remove leading ':'
		if (p[3]) override_mode = p[3].toLowerCase();
		if (p[4]) override_zoom = +p[4];
		//console.log('### f=['+ q[s] +'] len='+ p.length +' f=['+ p[1] +'] p=['+ p[2] +'] m=['+ p[3] +'] z=['+ p[4] +']');
	}

	s = 'ext'; if (q[s] && isString(q[s])) {
	   var ext = q[s].split(',');
		override_ext = ext[0];
		extint.param = ext.slice(1).join(',');
		console.log('URL: ext='+ override_ext +' ext_p='+ extint.param);
	}

	s = 'pbw'; if (q[s]) override_pbw = q[s];
	s = 'pb'; if (q[s]) override_pbw = q[s];
	s = 'pbc'; if (q[s]) override_pbc = q[s];
	s = 'z'; if (q[s]) override_zoom = +q[s];
	s = 'sp'; if (q[s]) spectrum_show = q[s];
	s = 'spec'; if (q[s]) spectrum_show = q[s];
	s = 'spp'; if (q[s]) spectrum_param = parseFloat(q[s]);
	s = 'vol'; if (q[s]) { setvolume(true, parseInt(q[s])); }
	s = 'mute'; if (q[s]) muted_initially = parseInt(q[s]);
	s = 'wf'; if (q[s]) wf_rate = q[s];
	s = 'wfm'; if (q[s]) wf_mm = q[s];
	s = 'wfa'; if (q[s]) wf_auto = parseInt(q[s]);
	s = 'cmap'; if (q[s]) wf.cmap_override = w3_clamp(parseInt(q[s]), 0, kiwi.cmap_s.length - 1, 0);
	s = 'sqrt'; if (q[s]) wf.sqrt = w3_clamp(parseInt(q[s]), 0, 4, 0);
	s = 'wfts'; if (q[s]) wf.url_tstamp = w3_clamp(parseInt(q[s]), 2, 60*60, 2);
	s = 'peak'; if (q[s]) peak_initially1 = w3_clamp(parseInt(q[s], 0, 2));
	s = 'peak2'; if (q[s]) peak_initially2 = w3_clamp(parseInt(q[s], 0, 2));
	s = 'no_geo'; if (q[s]) no_geoloc = true;
	s = 'keys'; if (q[s]) shortcut.keys = isNonEmptyString(q[s])? q[s].split('') : null;
	s = 'user'; if (q[s]) user_url = q[s];
	s = 'u'; if (q[s]) user_url = q[s];
	s = 'm'; if (q[s]) force_mobile = true;
	s = 'mobile'; if (q[s]) force_mobile = true;
	s = 'mem'; if (q[s]) owrx.override_fmem = q[s];
	s = '1hz'; if (q[s] || q['1Hz']) override_1Hz = true;
	// 'no_wf' is handled in kiwi_util.js
	// 'ant' is handled in ant_switch.js
	// 'foff' is handled in {rx_server,rx_cmd}.cpp

   // development
	s = 'sqth'; if (q[s]) squelch_threshold = parseFloat(q[s]);
	s = 'click'; if (q[s]) nb_click = true;
	s = 'nocache'; if (q[s]) { param_nocache = true; nocache = parseInt(q[s]); }
	s = 'ncc'; if (q[s]) no_clk_corr = parseInt(q[s]);
	s = 'wfdly'; if (q[s]) waterfall_delay = parseFloat(q[s]);
	s = 'wf_comp'; if (q[s]) wf_compression = parseInt(q[s]);
	s = 'wfi'; if (q[s]) wf_interp = parseInt(q[s]);
	s = 'wfw'; if (q[s]) wf_winf = parseInt(q[s]);
	s = 'sndw'; if (q[s]) snd_winf = parseInt(q[s]);
	s = 'gen'; if (q[s]) gen_freq = q[s].parseFloatWithUnits('kM', 1e-3);
	s = 'attn'; if (q[s]) gen_attn = Math.abs(parseInt(q[s]));
	s = 'blen'; if (q[s]) audio_buffer_min_length_sec = parseFloat(q[s])/1000;
	s = 'audio'; if (q[s]) audio_meas_dly_ena = parseFloat(q[s]);
	s = 'gc'; if (q[s]) kiwi_gc = parseInt(q[s]);
	s = 'gc_snd'; if (q[s]) kiwi_gc_snd = parseInt(q[s]);
	s = 'gc_wf'; if (q[s]) kiwi_gc_wf = parseInt(q[s]);
	s = 'gc_recv'; if (q[s]) kiwi_gc_recv = parseInt(q[s]);
	s = 'gc_wspr'; if (q[s]) kiwi_gc_wspr = parseInt(q[s]);
	s = 'ctrace'; if (q[s]) { param_ctrace = true; ctrace = parseInt(q[s]); }
	s = 'tmobile'; if (q[s]) mobile_laptop_test = true;
	s = 'ae'; if (q[s]) show_activeElement = true;
	s = 'fnas'; if (q[s]) force_need_autoscale = true;
	s = 'v'; if (q[s]) console.log('URL: debug_v = '+ (debug_v = q[s]));

	if (kiwi_gc_snd == -1) kiwi_gc_snd = kiwi_gc;
	if (kiwi_gc_wf == -1) kiwi_gc_wf = kiwi_gc;
	if (kiwi_gc_recv == -1) kiwi_gc_recv = kiwi_gc;
	if (kiwi_gc_wspr == -1) kiwi_gc_wspr = kiwi_gc;
	console.log('GC: snd='+ kiwi_gc_snd +' wf='+ kiwi_gc_wf +' recv='+ kiwi_gc_recv +' wspr='+ kiwi_gc_wspr);
	
	if (wf_mm != '') {
	   wf_mm = wf_mm.split(',');
	   var m = parseInt(wf_mm[0]);
	   if (!isNaN(m) && m >= -190 && m <= -30) override_min_dB = m;
	   if (wf_mm.length >= 2) {
	      m = parseInt(wf_mm[1]);
	      if (!isNaN(m) && m >= -100 && m <= 20) override_max_dB = m;
	   }
	}

   // shows activeElement in owner field
   if (show_activeElement) {
      setInterval(
         function() {
            var id = w3_id(document.activeElement);   // returns 'id-NULL' if ae is null
            if (id != owrx.activeElementID && id != 'id-freq-input') {
               w3_innerHTML('id-owner-info', id);
               owrx.activeElementID = id;
            }
         }, 250
      );
   }
   
   // Forces a periodic auto aperture autoscale.
   if (force_need_autoscale) {
      setInterval(
         function() {
            if (wf.aper == kiwi.APER_AUTO) {
               wf.need_autoscale = wf.FNAS = 1;
               colormap_update();
               console.log('FNAS');
            }
         }, 3000
      );
   }
      
	//kiwi_xdLocalStorage_init();
	kiwi_get_init_settings();
	if (!no_geoloc) kiwi_geolocate();

	freq_memory_init();
	init_top_bar();
	init_rx_photo();
	right_click_menu_init();
	freq_memory_menu_init();
	keyboard_shortcut_init();
	confirmation_panel_init();
	ext_panel_init();
	place_panels();
	init_panels();
   okay_wf_init = true;
	confirmation_panel_init2();
	smeter_init();
	time_display_setup('id-topbar-R-container');
	
	window.setInterval(send_keepalive, 5000);
	window.addEventListener('resize', openwebrx_resize);
        
   if (param_nocache) {
      //msg_send("SET nocache="+ (nocache? 1:0));
      console.log('### nocache='+ (nocache? 1:0));
   }
   if (param_ctrace) {
      //msg_send("SET ctrace="+ (ctrace? 1:0));
      console.log('### ctrace='+ (ctrace? 1:0));
   }

	// FIXME: eliminate most of these
	snd_send("SERVER DE CLIENT openwebrx.js SND");
	snd_send("SET dbug_v="+ debug_v +','+ (dbgUs? 1:0));

   // setup gen when configuration loaded
   w3_do_when_cond(
      function() { return (isArg(rx_chan) && owrx.cfg_loaded); },
      function() {
         if (rx_chan != 0) return;

         var attn_ampl = 0;
         if (gen_attn != 0) {
            var dB = gen_attn + wf.cal;
            var ampl_gain = Math.pow(10, -dB/20);		// use the amplitude form since we are multipling a signal
            attn_ampl = 0x01ffff * ampl_gain;
            //console.log('### GEN dB='+ dB +' ampl_gain='+ ampl_gain +' attn_ampl='+ attn_ampl +' / '+ attn_ampl.toHex() +' offset='+ wf.cal);
         }
   
         // always setup gen so it will get disabled (attn=0) if an rx0 page reloads using a URL where no gen is
         // specified, but it was previously enabled by the URL (i.e. so the gen doesn't continue to run).
         set_gen(gen_freq, attn_ampl);
      },
      null, 500
   );
   // REMINDER: w3_do_when_cond() returns immediately

   var _lo = kiwi_passbands(init_mode).lo;
   if (!isNumber(_lo)) _lo = -4000;
   var _hi = kiwi_passbands(init_mode).hi;
   if (!isNumber(_hi)) _hi = 4000;
	snd_send('SET mod='+ init_mode +' low_cut='+ _lo +' high_cut='+ _hi +' freq='+ init_frequency);
	//console.log('INIT: SET mod='+ init_mode +' low_cut='+ _lo +' high_cut='+ _hi +' freq='+ init_frequency);

	set_agc();
	snd_send("SET browser="+ navigator.userAgent);
	if (snd_winf != 0) snd_send('SET window_func='+ snd_winf);
	
	wf_send("SERVER DE CLIENT openwebrx.js W/F");
	wf_send("SET send_dB=1");
	// fixme: okay to remove this now?
	wf_send("SET zoom=0 start=0");
	wf_send("SET maxdb=0 mindb=-100");

	if (wf_compression == 0) wf_send('SET wf_comp=0');
	if (wf_interp != -1) wf_send('SET interp='+ wf_interp);
	if (wf_winf != 0) wf_send('SET window_func='+ wf_winf);
	wf_speed = wf_rates[wf_rate];
	//console.log('wf_rate="'+ wf_rate +'" wf_speed='+ wf_speed);
	if (wf_speed == undefined) wf_speed = WF_SPEED_FAST;
	wf_send('SET wf_speed='+ wf_speed);
}


////////////////////////////////
// #panel support
////////////////////////////////

var ptype = { HIDE:0, POPUP:1, TOGGLE:2 };
var popt = { CLOSE:-1, PERSIST:0 };
var visBorder = 10;
var visIcon = 24;

var readme_color = 'blueViolet';

var show_news = false;
//var news_color = '#bf00ff';
//var news_color = '#40ff00';
var news_color = '#ff00bf';

function init_panels()
{
	init_panel_toggle(ptype.TOGGLE, 'id-control', false, popt.PERSIST);

   var v = kiwi_storeRead('readme');
	var readme_firsttime = (v == null || v != 'seen2');
	if (readme_firsttime) kiwi_storeWrite('readme', 'seen2');
	
	if (kiwi_isMobile()) readme_firsttime = false;     // don't show readme panel at all on mobile devices
	init_panel_toggle(ptype.TOGGLE, 'id-readme', false, readme_firsttime? popt.PERSIST : popt.CLOSE, readme_color);

	//init_panel_toggle(ptype.TOGGLE, 'id-msgs', true, kiwi_isMobile()? popt.CLOSE : popt.PERSIST);
	//init_panel_toggle(ptype.POPUP, 'id-msgs', true, popt.CLOSE);

	var news_firsttime = (kiwi_storeRead('news', 'seen') == null);
	init_panel_toggle(ptype.POPUP, 'id-news', false, show_news? (news_firsttime? popt.PERSIST : popt.CLOSE) : popt.CLOSE);

	init_panel_toggle(ptype.POPUP, 'id-ext-controls', false, popt.CLOSE);

	init_panel_toggle(ptype.POPUP, 'id-confirmation', false, popt.CLOSE);
}

function init_panel_toggle(type, panel, scrollable, timeo, color)
{
	var divPanel = w3_el(panel);
	divPanel.ptype = type;
	//console.log('init_panel_toggle '+ panel +' ptype='+ divPanel.ptype +' '+ type);
	var divVis = w3_el(panel +'-vis');
	divPanel.scrollable = (scrollable == true)? true:false;
	var visHoffset = (divPanel.scrollable)? -kiwi_scrollbar_width() : visBorder;
	var rightSide = (divPanel.getAttribute('data-panel-pos') == "right");
   divPanel.panelShown = 1;
	
	if (type == ptype.TOGGLE) {
		var hide = rightSide? 'right':'left';
		var show = rightSide? 'left':'right';
		divVis.innerHTML =
			'<a id="'+panel+'-hide" onclick="toggle_panel('+ sq(panel) +');"><img src="icons/hide'+ hide +'.24.png" width="24" height="24" /></a>' +
			'<a id="'+panel+'-show" class="class-vis-show" onclick="toggle_panel('+ sq(panel) +');"><img src="icons/hide'+ show +'.24.png" width="24" height="24" /></a>';
	} else {		// ptype.POPUP or ptype:HIDE
		divVis.innerHTML =
			'<a id="'+panel+'-close" onclick="toggle_panel('+ sq(panel) +');">' +
			   w3_img(panel +'-close-img', 'icons/close.24.png', 24, 24) +
			'</a>';
	}

	var visOffset = divPanel.activeWidth - visIcon;
	//console.log("init_panel_toggle "+panel+" right="+rightSide+" off="+visOffset);
	if (rightSide) {
		divVis.style.right = px(0);
		//console.log("RS2");
	} else {
		divVis.style.left = px(visOffset + visHoffset);
	}
	divVis.style.top = px(visBorder);
	//console.log("ARROW l="+divVis.style.left+" r="+divVis.style.right+' t='+divVis.style.top);

	if (timeo != undefined) {
      //console.log(panel +' timeo='+ timeo);
		if (timeo > 0) {
			setTimeout(function() {toggle_panel(panel);}, timeo);
		} else
		if (timeo == popt.CLOSE) {
			toggle_panel(panel);		// make it go away immediately
		} else
		if (type == ptype.POPUP) {
			divPanel.style.visibility = "visible";
		}
	}
	
	if (color != undefined) {
		var divShow = w3_el(panel+'-show');
		if (divShow != undefined) divShow.style.backgroundColor = divShow.style.borderColor = color;
	}
	
	divPanel.init = true;
}

function toggle_panel(panel, set)
{
	var divPanel = w3_el(panel);
	var divVis = w3_el(panel +'-vis');
	//console.log('toggle_panel '+ panel +' ptype='+ divPanel.ptype +' panelShown='+ divPanel.panelShown);
	
	if (isDefined(set)) divPanel.panelShown = set ^ 1;    // ^1 because of inverted logic below

	if (divPanel.ptype == ptype.POPUP) {
		divPanel.style.visibility = divPanel.panelShown? 'hidden' : 'visible';
		divPanel.panelShown ^= 1;
		kiwi_storeWrite(panel, 'seen');
	   freqset_select();
		return;
	}
	
	var arrow_width = 12, hideWidth = divPanel.activeWidth + 2*15;
	var rightSide = (divPanel.getAttribute('data-panel-pos') == "right");
	var from, to;

	hideWidth = rightSide? -hideWidth : -hideWidth;
	if (divPanel.panelShown) {
		from = 0; to = hideWidth;
	} else {
		from = hideWidth; to = kiwi_isMobile()? 0 : kiwi_scrollbar_width();
	}
	//if (panel == 'id-control') canvas_log('f='+ from.toFixed(0) +' t='+ to.toFixed(0));
	
	// undo scaling before hide
	if (panel == 'id-control' && divPanel.panelShown) {
	   mobile_scale_control_panel(null, true);
	   //canvas_log('TogScale=>F'+ divPanel.style.left +':'+ divPanel.style.right);
	   //canvas_log(divPanel.style.marginLeft +':'+ divPanel.style.marginRight);
	}

	animate(divPanel, rightSide? 'right':'left', "px", from, to, 0.93, kiwi_isMobile()? 1:1000, 60, null);
	
	w3_hide(panel +'-'+ (divPanel.panelShown? 'hide':'show'));
	w3_show_block(panel +'-'+ (divPanel.panelShown? 'show':'hide'));
	divPanel.panelShown ^= 1;

	var visOffset = divPanel.activeWidth - visIcon;
	var visHoffset = (divPanel.scrollable)? -kiwi_scrollbar_width() : visBorder;
	console.log("toggle_panel "+panel+" right="+rightSide+" shown="+divPanel.panelShown);
	if (rightSide)
		divVis.style.right = px(divPanel.panelShown? 0 : (visOffset + visIcon + visBorder*2));
	else
		divVis.style.left = px(visOffset + (divPanel.panelShown? visHoffset : (visIcon + visBorder*2)));
	//if (panel == 'id-control') canvas_log('TogPan'+ visOffset +':'+ divPanel.style.right);

	// redo scaling after show
	// (NB: divPanel.panelShown instead of !divPanel.panelShown due to ^= 1 above
	if (panel == 'id-control' && divPanel.panelShown) {
	   mobile_scale_control_panel(null);
	   //canvas_log('TogScale=>T'+ divPanel.style.left +':'+ divPanel.style.right);
	   //canvas_log(divPanel.style.marginLeft +':'+ divPanel.style.marginRight);
	}

	freqset_select();
}

function mdev_log_resize()
{
	mdev_log('w='+ window.innerWidth +' sX='+ TF(w3_isScrollingX('id-topbar')) +' tbH='+ owrx.top_bar_cur_height +' mOpt='+ TF(mobileOpt()) +' mOptN='+ TF(mobileOptNew()));
}

function openwebrx_resize(from, delay)
{
   if (isNumber(delay)) {
      setTimeout(function() { openwebrx_resize(from); }, delay);
   }
   
   from = (isString(from) && from.startsWith('orient'))? from : 'event';
	resize_wf_canvases();
	resize_waterfall_container(true);
   extint_environment_changed( { resize:1, passband_screen_location:1 } );
	resize_scale(from);
	position_top_bar();
	owrx.resize_seq++;
	mdev_log_resize();
   //console.log('resize_seq='+ owrx.resize_seq);
}

/*
var orient = { cnt:0 };
function orientation_change() {
   //openwebrx_resize('orient '+ orientation);
   orient.AW = window.innerWidth;
   orient.AH = window.innerHeight;
   if (orient.cnt == 0) {
      setTimeout(function() {    // timing matters on iphone-5S but not iPad-2!
         alert('orient #'+ orient.cnt +' '+
            orient.AW +','+ orient.AH +' '+ window.innerWidth +','+ window.innerHeight);
         orient.cnt = 0;
      }, 500);
   } else orient.cnt++;
}

try {
   window.onorientationchange = orientation_change;
} catch(ex) {}
*/


////////////////////////////////
// #topbar
////////////////////////////////

function position_top_bar()
{
   var w = window.innerWidth;
   var w_min = kiwi.WIN_WIDTH_MIN;
   var narrow = (w < w_min);
   
	var info = w3_boundingBox_children('id-topbar-L-container');
	//console.log(info);
	var owner = w3_boundingBox_children('id-topbar-ML-container');
	//var ident = w3_boundingBox_children('id-topbar-MR-container');
	var time = w3_boundingBox_children('id-topbar-R-container');
	//console.log(time);
	
	
	// NB: When a div contains only absolutely positioned elements it has zero width.
	// So flex space-between doesn't work.
	// But we can compute correct width using w3_boundingBox_children() and set min-width directly.
	w3_el('id-topbar-L-container').style.minWidth = px(info.x2);
	if (owner.w)   // will be non-zero if absolutely positioned elements exist
	   w3_el('id-topbar-ML-container').style.minWidth = px(owner.w);
	w3_el('id-topbar-R-container').style.minWidth = px(time.w);
	
	// place photo open arrow right after info bbox
	w3_el('id-topbar-arrow').style.left = px(info.x2 + 16);

   scrollW = w3_isScrollingX('id-topbar')? kiwi_scrollbar_width() : 0;
   owrx.top_bar_cur_height = owrx.top_bar_nom_height + scrollW;
	w3_el("id-topbar").style.height = px(owrx.top_bar_cur_height);
	w3_el("id-main-container").style.top = px(scrollW);
   rx_photo_set_height();
}

function init_top_bar()
{
   owrx.top_bar_cur_height = owrx.top_bar_nom_height;
	w3_el("id-topbar").style.height = px(owrx.top_bar_cur_height);
	w3_el("id-main-container").style.top = px(0);
	rx_photo_set_height(true);

   w3_el('id-topbar').addEventListener('click', freqset_select, false);
   var rx_loc = w3_json_to_string('RX_LOC', cfg.index_html_params.RX_LOC);
   w3_innerHTML('id-rx-desc',
      rx_loc +' | Grid '+
      w3_div('id-web-grid w3-show-inline') +
      ', ASL '+ cfg.index_html_params.RX_ASL +', '+
      w3_link('id-rx-gps', '', '[map]', '', 'dont_toggle_rx_photo') +
      w3_div('id-rx-snr w3-show-inline')
   );
}

function update_web_grid(init)
{
   var grid = isNonEmptyString(kiwi.GPS_auto_grid)? kiwi.GPS_auto_grid : cfg.index_html_params.RX_QRA;
   //console_nv('update_web_grid', {init}, {grid}, 'cfg.GPS_update_web_grid');
   if (init != true && !cfg.GPS_update_web_grid) return;
   w3_innerHTML('id-web-grid',
      w3_link('', 'https://www.levinecentral.com/ham/grid_square.php?Grid='+ grid, grid, '', 'dont_toggle_rx_photo')
   );
   position_top_bar();
}

function update_web_map(init)
{
   var rx_gps = w3_json_to_string('rx_gps', cfg.rx_gps);
   rx_gps = isNonEmptyString(kiwi.GPS_auto_latlon)? kiwi.GPS_auto_latlon : rx_gps;
   //console_nv('update_web_map', {init}, {rx_gps}, 'cfg.GPS_update_web_lores', 'cfg.GPS_update_web_hires');
   if (init != true && !cfg.GPS_update_web_lores && !cfg.GPS_update_web_hires) return;
   w3_link_set('id-rx-gps', 'https://www.google.com/maps/place/'+ rx_gps);
   position_top_bar();
}

function rx_photo_set_height(setMaxHeight)
{
	owrx.rx_photo_total_height = +(cfg.index_html_params.RX_PHOTO_HEIGHT) + owrx.top_bar_cur_height;
	w3_el("id-top-photo-spacer").style.height = px(owrx.top_bar_cur_height);
	if (setMaxHeight)
	   w3_el("id-top-photo-clip").style.maxHeight = px(owrx.rx_photo_total_height);
}

function init_rx_photo()
{
   var el = w3_el('id-top-photo');
   var img = w3_el('id-top-photo-img');
   img.src = w3_json_to_string('RX_PHOTO_FILE', cfg.index_html_params.RX_PHOTO_FILE);
   img.alt = img.title = w3_html_to_nl(w3_json_to_html('RX_PHOTO_DESC', cfg.index_html_params.RX_PHOTO_DESC));
   var centered = cfg.index_html_params.RX_PHOTO_CENTERED;
   var left = cfg.index_html_params.RX_PHOTO_LEFT_MARGIN;

   if (img.src.endsWith('kiwi/pcb.png')) {
      centered = true;
      left = false;
   }
   
   if (centered) w3_add(el, "w3-center");
   img.style.paddingLeft = left? '50px' : 0;
   rx_photo_set_height();
   
	if (dbgUs || kiwi_isMobile()) {
		close_rx_photo();
	} else {
		window.setTimeout(function() { close_rx_photo(); }, 3000);
	}
}

var rx_photo_state=1;

function open_rx_photo()
{
   //console.log('$open_rx_photo '+ w3_el('id-top-photo-clip').style.maxHeight +' => '+ owrx.rx_photo_total_height);
	rx_photo_state=1;
	w3_opacity("id-rx-photo-desc", 1);
	w3_opacity("id-rx-photo-title", 1);
	animate_to("id-top-photo-clip", "maxHeight", "px", owrx.rx_photo_total_height, 0.93, kiwi_isMobile()? 1:1000, 60, function(){resize_waterfall_container(true);});
	w3_hide('id-topbar-arrow-down');
	w3_show_block('id-topbar-arrow-up');
}

function close_rx_photo()
{
   //console.log('$close_rx_photo '+ w3_el('id-top-photo-clip').style.maxHeight +' => '+ owrx.top_bar_cur_height);
	rx_photo_state=0;
	animate_to("id-top-photo-clip", "maxHeight", "px", owrx.top_bar_cur_height, 0.93, kiwi_isMobile()? 1:1000, 60, function(){resize_waterfall_container(true);});
	w3_show_block('id-topbar-arrow-down');
	w3_hide('id-topbar-arrow-up');
}

dont_toggle_rx_photo_flag=0;

function dont_toggle_rx_photo()
{
	dont_toggle_rx_photo_flag=1;
}

function toggle_rx_photo()
{
	if (dont_toggle_rx_photo_flag) { dont_toggle_rx_photo_flag=0; return; }
	if (rx_photo_state)
		close_rx_photo();
	else
		open_rx_photo();
	freqset_select();
}

function animate(object, style_name, unit, from, to, accel, time_ms, fps, func)
{
	//console.log(object.className);
	object.style[style_name] = from.toString()+unit;
	object.anim_i = 0;
	n_of_iters = time_ms/(1000/fps);
	if (n_of_iters < 1) n_of_iters = 1;
	change = (to-from)/(n_of_iters);
	if (isDefined(object.anim_timer)) { window.clearInterval(object.anim_timer);  }

	object.anim_timer = window.setInterval(
		function() {
			if (object.anim_i++<n_of_iters) {
				if (accel==1) object.style[style_name] = (parseFloat(object.style[style_name]) + change).toString() + unit;
				else { 
					remain = parseFloat(object.style[style_name])-to;
					if (Math.abs(remain)>9||unit!="px") new_val = (to+accel*remain);
					else {if (Math.abs(remain)<2) new_val = to;
					else new_val = to+remain-(remain/Math.abs(remain));}
					object.style[style_name] = new_val.toString()+unit;
				}
			} else {
			   object.style[style_name] = to.toString()+unit;
			   window.clearInterval(object.anim_timer);
			   delete object.anim_timer;
			}
			w3_call(func);
		},1000/fps);
}

function animate_to(id, style_name, unit, to, accel, time_ms, fps, func)
{
   var el = w3_el(id);
   if (!el) return;
	from = parseFloat(css_style(el, style_name));
	//console.log("animate_to: FROM "+ style_name +'='+ from);
	animate(el, style_name, unit, from, to, accel, time_ms, fps, func);
}


////////////////////////////////
// #demodulator
////////////////////////////////

demodulators = [];

function demod_envelope_draw(range, from, to, line)
{
   //                                               ____
	// Draws a standard filter envelope like this: _/    \_
   // Parameters are given in offset frequency (Hz).
   // Envelope is drawn on the scale canvas.
	// A "drag range" object is returned, containing information about the draggable areas of the envelope
	// (beginning, ending and the line showing the offset frequency).
	
	var env_bounding_line_w = 5;   //     |from  |to
	var env_att_w = 5;             //     v______v  ___env_h2 in px   ___|_____
	var env_h1 = 17;               //   _/|      \_ ___env_h1 in px _/   |_    \_
	var env_h2 = 5;                //   |||env_att_w                     |_env_lineplus
	var env_lineplus = 1;          //   ||env_bounding_line_w
	var env_line_click_area = 8;
	var env_slop = 15;
	var env_adj = 20;
	var env_slope = env_bounding_line_w + env_att_w;
	
	//range = get_visible_freq_range();
	var from_px = scale_px_from_freq(from, range);
	var to_px = scale_px_from_freq(to, range);
	if (to_px < from_px) /* swap'em */ { var temp_px = to_px; to_px = from_px; from_px = temp_px; }
	
	pb_adj_cf.style.left = px(from_px + env_adj);
	pb_adj_cf.style.width = px(Math.max(0, to_px - from_px - 2*env_adj));

	pb_adj_lo.style.left = px(from_px - env_slope - env_adj);
	pb_adj_lo.style.width = px(env_slope + 2*env_adj);

	pb_adj_hi.style.left = px(to_px - env_adj);
	pb_adj_hi.style.width = px(env_slope + 2*env_adj);
	
	//     v_____v
	//   _/       \_
	//  ^           ^
	//  |from       |to
	from_px -= (env_att_w + env_bounding_line_w);
	to_px += (env_att_w + env_bounding_line_w);
	
	// do drawing:
	scale_ctx.lineWidth = 3;
	scale_ctx.strokeStyle = scale_ctx.fillStyle = owrx.lock_fast? 'orange' : 'lime';
	var drag_ranges = { envelope_on_screen: false, line_on_screen: false, allow_pb_adj: false };
	owrx.allow_pb_adj = false;
	
	if (!(to_px < 0 || from_px > window.innerWidth)) {
	   // at least one on screen
		drag_ranges.beginning = {x1: from_px - env_adj, x2: from_px + env_slope + env_adj};
		drag_ranges.ending = {x1: to_px - env_slope - env_adj, x2: to_px + env_adj };
		drag_ranges.whole_envelope = {x1: from_px, x2: to_px};
		
      // for spectrum passband marker
		owrx.pbx1 = from_px;
		owrx.pbx2 = to_px;
		
		drag_ranges.envelope_on_screen = true;
		//drag_ranges.allow_pb_adj = ((to_px - from_px) >= (dbgUs? 100:30));
		drag_ranges.allow_pb_adj = owrx.allow_pb_adj = ((to_px - from_px) >= 50);
		
		if (!drag_ranges.allow_pb_adj)
		   scale_ctx.strokeStyle = scale_ctx.fillStyle = owrx.lock_fast? 'orange' : 'yellow';
		
      scale_ctx.beginPath();
      scale_ctx.moveTo(from_px, env_h1);
      scale_ctx.lineTo(from_px + env_bounding_line_w, env_h1);
      scale_ctx.lineTo(from_px + env_slope, env_h2);
      scale_ctx.lineTo(to_px - env_bounding_line_w - env_att_w, env_h2);
      scale_ctx.lineTo(to_px - env_bounding_line_w, env_h1);
      scale_ctx.lineTo(to_px, env_h1);
      scale_ctx.globalAlpha = 0.3;
      scale_ctx.fill();
      scale_ctx.globalAlpha = 1;
      scale_ctx.stroke();
	} else {
	   owrx.pbx1 = owrx.pbx2 = 0;
	}
	
	if (isDefined(line)) {     // out of screen? 
		line_px = scale_px_from_freq(line, range);
		if (!(line_px < 0 || line_px > window.innerWidth)) {
			drag_ranges.line = {x1: line_px - env_line_click_area/2, x2: line_px + env_line_click_area/2};
			drag_ranges.line_on_screen = true;
			scale_ctx.moveTo(line_px, env_h1 + env_lineplus);
			scale_ctx.lineTo(line_px, env_h2 - env_lineplus);
			scale_ctx.stroke();

			pb_adj_car.style.left = px(drag_ranges.line.x1);
			pb_adj_car.style.width = px(env_line_click_area);

		}
	}

	return drag_ranges;
}

function demod_envelope_where_clicked(x, y, drag_ranges, key_modifiers)
{
	// Check exactly what the user has clicked based on ranges returned by demod_envelope_draw().
	var pbh = Math.round(scale_canvas_h/2);
	if (kiwi_isMobile()) pbh += 6;      // tooltip seems to respond to a larger touch point
	in_range = function(x, g_range) { return g_range.x1 <= x && g_range.x2 >= x && y <= pbh; };
	dr = demodulator.draggable_ranges;
	//console.log('demod_envelope_where_clicked x='+ x +' y='+ y +' in_pb='+ TF(y <= pbh) +' allow_pb_adj='+ drag_ranges.allow_pb_adj);
	//console.log(drag_ranges);
	//console.log(key_modifiers);

	if (key_modifiers.shiftKey) {
		// Check first: shift + center drag emulates BFO knob
		if (drag_ranges.line_on_screen && in_range(x, drag_ranges.line)) return dr.bfo;
		
		// Check second: shift + envelope drag emulates PBS/PBT knob
		if (drag_ranges.envelope_on_screen && in_range(x, drag_ranges.whole_envelope)) return dr.pbs;
	}
	
	if (key_modifiers.altKey) {
		if (drag_ranges.envelope_on_screen && in_range(x, drag_ranges.beginning)) return dr.bwlo;
		if (drag_ranges.envelope_on_screen && in_range(x, drag_ranges.ending)) return dr.bwhi;
	}
	
	if (drag_ranges.envelope_on_screen) { 
		// For low and high cut:
		if (drag_ranges.allow_pb_adj) {
		   if (in_range(x, drag_ranges.beginning)) return dr.beginning;
		   if (in_range(x, drag_ranges.ending)) return dr.ending;
		}

		// Last priority: having clicked anything else on the envelope, without holding the shift key
		if (in_range(x, drag_ranges.whole_envelope)) return dr.anything_else; 
	}

   //canvas_log(x +'|'+ y +' '+ JSON.stringify(drag_ranges.ending) +' '+ pbh);
   //canvas_log(y);
	return dr.none;   // User doesn't drag the envelope for this demodulator
}

//******* class demodulator *******
// this can be used as a base class for ANY demodulator
demodulator = function(offset_frequency)
{
	//console.log("this too");
	this.offset_frequency = offset_frequency;
	this.has_audio_output = true;
	this.has_text_output = false;
	this.envelope = {};
	this.stop = function(){};
};

// ranges on filter envelope that can be dragged:
demodulator.draggable_ranges = {
	none: 0,
	beginning: 1,	// from
	ending: 2,		// to
	anything_else: 3,
	bfo: 4,			// line (while holding shift)
	pbs: 5,			// passband (while holding shift)
	bwlo: 6,			// bandwidth (while holding alt in envelope beginning)
	bwhi: 7,			// bandwidth (while holding alt in envelope ending)
}; // to which parameter these correspond in demod_envelope_draw()

//******* class demodulator_default_analog *******
// This can be used as a base for basic audio demodulators.
// It already supports most basic modulations used for ham radio and commercial services: AM/FM/LSB/USB

demodulator_response_time = 100; 
//in ms; if we don't limit the number of SETs sent to the server, audio will underrun (possibly output buffer is cleared on SETs in GNU Radio

var passbands_fallback = {
	am:		{ lo: -4900,	hi:  4900 },            // 9.8 kHz instead of 10 to avoid adjacent channel heterodynes in SW BCBs
	amn:		{ lo: -2500,	hi:  2500 },
	amw:		{ lo: -6000,	hi:  6000 },            // FIXME: set based on current srate?
	sam:		{ lo: -4900,	hi:  4900 },
	sal:		{ lo: -4900,	hi:     0 },
	sau:		{ lo:     0,	hi:  4900 },
	sas:		{ lo: -4900,	hi:  4900 },
	qam:		{ lo: -4900,	hi:  4900 },
	drm:		{ lo: -5000,	hi:  5000 },
	lsb:		{ lo: -2700,	hi:  -300 },	         // cf = 1500 Hz, bw = 2400 Hz
	lsn:		{ lo: -2400,	hi:  -300 },	         // cf = 1350 Hz, bw = 2100 Hz
	usb:		{ lo:   300,	hi:  2700 },	         // cf = 1500 Hz, bw = 2400 Hz
	usn:		{ lo:   300,	hi:  2400 },	         // cf = 1350 Hz, bw = 2100 Hz
	cw:		{ lo:   300,	hi:   700,  pbw: 400 },	// cf = 500 Hz, bw = 400 Hz
	cwn:		{ lo:   470,	hi:   530,  pbw:  60 },	// cf = 500 Hz, bw = 60 Hz
	nbfm:		{ lo: -6000,	hi:  6000 },	         // FIXME: set based on current srate?
	nnfm:		{ lo: -3000,	hi:  3000 },
	iq:		{ lo: -5000,	hi:  5000 },
};

// When a new mode is defined, and doesn't yet exist in cfg.passbands because
// admin page hasn't been loaded, then use fallback values.
function kiwi_passbands(mode)
{
   var pb = cfg.passbands[mode];
   if (isDefined(pb)) {
      return pb;
   } else {
      console.log('kiwi_passbands('+ mode +') fallback:');
      if (isUndefined(mode)) {
         kiwi_trace();
         return passbands_fallback['am'];
      } else {
         console.log(passbands_fallback[mode]);
         return passbands_fallback[mode];
      }
   }
}

function demodulator_default_analog(offset_frequency, subtype, locut, hicut)
{
   if (kiwi_passbands(subtype) == null) subtype = 'am';
   //console.log('demodulator_default_analog '+ subtype +' locut='+ locut +' hicut='+ hicut);
   
	//http://stackoverflow.com/questions/4152931/javascript-inheritance-call-super-constructor-or-use-prototype-chain
	demodulator.call(this, offset_frequency);

	this.subtype = subtype;
	this.envelope.dragged_range = demodulator.draggable_ranges.none;
	var sampleRateDiv2 = audio_input_rate? audio_input_rate/2 : 5000;
	this.filter = {
		min_passband: 4,
		high_cut_limit: sampleRateDiv2,
		low_cut_limit: -sampleRateDiv2
	};

	//Subtypes only define some filter parameters and the mod string sent to server, 
	//so you may set these parameters in your custom child class.
	//Why? As the demodulation is done on the server, difference is mainly on the server side.
	this.server_mode = subtype;

   var lo = locut;
   if (isNaN(lo)) {
      lo = owrx.last_lo[subtype];
      //console.log('$demodulator_default_analog owrx.last_lo['+ subtype +']='+ lo);
      if (isNaN(lo)) {
         lo = kiwi_passbands(subtype).lo;
         //console.log('$demodulator_default_analog kiwi_passbands('+ subtype +')='+ lo);
      }
   }
   owrx.last_lo[subtype] = lo;

   var hi = hicut;
   if (isNaN(hi)) {
      hi = owrx.last_hi[subtype];
      if (isNaN(hi)) {
         hi = kiwi_passbands(subtype).hi;
      }
   }
   owrx.last_hi[subtype] = hi;

   var min, p;
	if (override_pbw != '') {
	   var center = lo + (hi-lo)/2;
	   min = this.filter.min_passband;
	   //console.log('### override_pbw cur_lo='+ lo +' cur_hi='+ hi +' cur_center='+ center +' min='+ min);
	   override_pbw = decodeURIComponent(override_pbw);
	   p = override_pbw.split(',');
	   console.log('p.len='+ p.length);
	   var nlo = p[0].parseFloatWithUnits('k');
      console.log('nlo="'+ p[0] +'" '+ nlo);
	   var nhi = NaN;
	   if (p.length > 1) {
	      nhi = p[1].parseFloatWithUnits('k');
         console.log('nhi="'+ p[1] +'" '+ nlo);
      }
	   
	   // adjust passband width about current pb center
	   if (p.length == 1 && !isNaN(nlo) && nlo >= min) {
	      // /pbw
         lo = center - nlo/2;
         hi = center + nlo/2;
	   } else
	   if (p.length == 2 && !isNaN(nlo) && !isNaN(nhi) && nlo < nhi && (nhi - nlo) >= min) {
	      // /pbl,pbh
	      lo = nlo;
	      hi = nhi;
	   }
	   //console.log('### override_pbw=['+ override_pbw +'] len='+ p.length +' nlo='+ nlo +' nhi='+ nhi +' lo='+ lo +' hi='+ hi);

	   override_pbw = '';
	   extint.override_pb = true;
	}
	
	if (override_pbc != '') {
	   var cpbhw = (hi - lo)/2;
	   var cpbc = lo + cpbhw;
	   min = this.filter.min_passband;
	   //console.log('### override_pbc cur_lo='+ lo +' cur_hi='+ hi +' cpbc='+ cpbc +' cpbhw='+ cpbhw +' min='+ min);
	   override_pbc = decodeURIComponent(override_pbc);
	   p = override_pbc.split(',');
	   var pbc = p[0].parseFloatWithUnits('k');
      console.log('pbc='+ dq(p[0]) +' '+ pbc);
	   var pbw = NaN;
	   if (p.length > 1) {
	      pbw = p[1].parseFloatWithUnits('k');
         console.log('pbw='+ dq(p[1]) +' '+ pbw);
      }
	   
      // adjust passband center using current or specified pb width
	   if (p.length == 1 && !isNaN(pbc)) {
	      // :pbc
         lo = pbc - cpbhw;
         hi = pbc + cpbhw;
	   } else
	   if (p.length == 2 && !isNaN(pbc) && !isNaN(pbw) && pbw >= min) {
	      // :pbc,pbw
         lo = pbc - pbw/2;
         hi = pbc + pbw/2;
	   }
	   //console.log('### override_pbc=['+ override_pbc +'] len='+ p.length +' pbc='+ pbc +' pbw='+ pbw +' lo='+ lo +' hi='+ hi);

	   override_pbc = '';
	   extint.override_pb = true;
	}
	
	this.low_cut = Math.max(lo, this.filter.low_cut_limit);
	this.high_cut = Math.min(hi, this.filter.high_cut_limit);
	//console.log('DEMOD set subtype='+ subtype +' lo='+ this.low_cut, ' hi='+ this.high_cut);
	
	this.usePBCenter = false;
	this.isCW = false;
	
	switch (subtype) {
	
	   case 'am':
	   case 'amn':
	   case 'amw':
	   case 'sam':
	   case 'sal':
	   case 'sau':
	   case 'sas':
	   case 'qam':
	   case 'drm':
	   case 'nbfm':
	   case 'nnfm':
	   case 'iq':
		   break;
		   
	   case 'lsb':
	   case 'lsn':
	   case 'usb':
	   case 'usn':
		   this.usePBCenter = true;
		   break;

	   case 'cw':
	   case 'cwn':
		   this.usePBCenter = true;
		   this.isCW = true;
		   break;
		   
		default:
		   console.log('DEMOD-new: unknown subtype='+ subtype);
		   break;
	}

	this.wait_for_timer = false;
	this.set_after = false;

	// set() is a wrapper to call doset(), but it ensures that doset won't execute more frequently than demodulator_response_time.
	this.set = function() {

		if (!this.wait_for_timer) {
			this.doset();
			this.set_after = false;
			this.wait_for_timer = true;
			timeout_this = this; //http://stackoverflow.com/a/2130411
			window.setTimeout(function() {
				timeout_this.wait_for_timer = false;
				if (timeout_this.set_after) timeout_this.set();
			}, demodulator_response_time);
		} else {
			this.set_after = true;
		}
	};

	// this function sends demodulator parameters to the server
	this.doset = function() {
		//console.log('DOSET fcar='+freq_car_Hz);
		//if (dbgUs && dbgUsFirst) { dbgUsFirst = false; console.trace(); }
		
		var freq_Hz = freq_car_Hz;
		var freq_kHz = freq_Hz/1000;
      var freq_kHz_s = freq_kHz.toFixed(3);     // always allow manual entry of 3 significant digits

		var mode = this.server_mode;
		var locut = this.low_cut.toString();
		var hicut = this.high_cut.toString();
		var mparam = (ext_mode(mode).SAx)? (' param='+ (owrx.chan_null | (owrx.SAM_opts << owrx.SAM_opts_sft))) : '';
		//if (mparam != '') console.log('$mode='+ mode +' mparam='+ mparam); else console.log('$mode='+ mode);
		var s = 'SET mod='+ mode +' low_cut='+ locut +' high_cut='+ hicut +' freq='+ freq_kHz_s + mparam;
		snd_send(s);
      //kiwi_trace('doSet');
		//console.log('$'+ s);

      var changed = null;
		freq_Hz = freq_displayed_Hz;
      if (!owrx.freq_dsp_1Hz) {
         // in 10 Hz mode store rounded freq so comparison below against last freq works correctly
         //var _freq_Hz = _10Hz(freq_Hz);
         var _freq_Hz = _10Hz_round(freq_Hz);
         //console.log('prev_freq_kHz PUSH '+ freq_Hz +'=>'+ _freq_Hz);
         freq_Hz = _freq_Hz;
      }
      if (freq_Hz != owrx.last_freq_Hz) {
         changed = changed || {};
         changed.freq = 1;
         if (freq_Hz > 0) {
            //console.log('prev_freq_kHz PUSH '+ freq_Hz +'|'+ owrx.last_freq_Hz);
            owrx.prev_freq_kHz.unshift(freq_Hz);
            owrx.prev_freq_kHz.length = 2;
            //console.log(owrx.prev_freq_kHz);
         }
         owrx.last_freq_Hz = freq_Hz;
      }
      if (mode != owrx.last_mode) {
         changed = changed || {};
         changed.mode = 1;
         owrx.last_mode = mode;
      }
      if (locut != owrx.last_locut || hicut != owrx.last_hicut) {
         changed = changed || {};
         changed.passband = 1;
         owrx.last_locut = locut; owrx.last_hicut = hicut;
      }
      if (changed != null) {
         changed.passband_screen_location = 1;
         extint_environment_changed(changed);
      }

		if (muted_until_freq_set) {
         //console.log('muted_initially='+ muted_initially);
		   toggle_or_set_mute(muted_initially);
		   muted_until_freq_set = false;
		}
		
		if (audio_meas_dly_ena) {
		   //console.log('audio_meas_dly_start');
		   audio_meas_dly_start = (new Date()).getTime();
		}
	};
	// this.set(); //we set parameters on object creation

	//******* envelope object *******
   // for drawing the filter envelope above scale
	this.envelope.parent = this;

	this.envelope.draw = function(visible_range) 
	{
      var lo = this.parent.low_cut, hi = this.parent.high_cut;
		this.visible_range = visible_range;
		this.drag_ranges = demod_envelope_draw(g_range,
				center_freq + this.parent.offset_frequency + lo,
				center_freq + this.parent.offset_frequency + hi,
				center_freq + this.parent.offset_frequency);

      var center = _10Hz(lo + (hi - lo) / 2);
      var width = hi - lo;
      setpb_cb('', lo, 0, 0, (-owrx.PB_LO)-1);
      setpb_cb('', hi, 0, 0, (-owrx.PB_HI)-1);
      setpb_cb('', center, 0, 0, (-owrx.PB_CENTER)-1);
      setpb_cb('', width, 0, 0, (-owrx.PB_WIDTH)-1);

      // replace bw in lo/hi/cf tooltips with "PBT" when shift key changed before dragging starts or while it is ongoing
      var isAdjPBT = (owrx.last_shiftKey || this.dragged_range == demodulator.draggable_ranges.pbs);
		var bw = Math.abs(hi - lo);
		var bw_pbt;
		if (isAdjPBT) {
         bw_pbt = ', PBT';
		} else {
		   bw_pbt = ', bw '+ bw.toString();
      }
      pb_adj_lo_ttip.innerHTML = 'lo '+ lo.toString() + bw_pbt;
      pb_adj_hi_ttip.innerHTML = 'hi '+ hi.toString() + bw_pbt;
		pb_adj_cf_ttip.innerHTML = 'cf '+ (lo + Math.abs(hi - lo)/2).toString() + bw_pbt;

      // show "BFO" in tooltip when shift key changed before BFO dragging starts or while it is ongoing
      var isAdjBFO = (owrx.last_shiftKey || this.dragged_range == demodulator.draggable_ranges.bfo);
      var f_kHz = (center_freq + this.parent.offset_frequency)/1000 + kiwi.freq_offset_kHz;
		pb_adj_car_ttip.innerHTML = (isAdjBFO? 'BFO ':'') + freq_field_prec_s(f_kHz) +' kHz';
	};

	// event handlers
	this.envelope.env_drag_start = function(x, y, key_modifiers)
	{
		this.key_modifiers = key_modifiers;
		this.dragged_range = demod_envelope_where_clicked(x, y, this.drag_ranges, key_modifiers);
		//console.log("DRAG-START dr="+ this.dragged_range.toString());
		this.drag_origin = {
			x: x,
			low_cut: this.parent.low_cut,
			high_cut: this.parent.high_cut,
			offset_frequency: this.parent.offset_frequency
		};
		return this.dragged_range != demodulator.draggable_ranges.none;
	};

	this.envelope.drag_move = function(x)
	{
	   var rv = { event_handled: false, fset: owrx.FSET_PB_CHANGE, d: 0 };
		var dr = demodulator.draggable_ranges;
		if (this.dragged_range == dr.none) {
		   rv.d = 1;
		   rv.fset = owrx.FSET_NOP;
			return rv; // we return if user is not dragging (us) at all
		}
		rv.event_handled = true;

		var freq_change = _10Hz(Math.round(this.visible_range.hpp * (x-this.drag_origin.x)));
		//console.log('DRAG fch='+ freq_change +' dr='+ this.dragged_range);

		var is_adj_BFO = this.dragged_range == dr.bfo;
		var is_adj_locut = this.dragged_range == dr.beginning;
		var is_adj_hicut = this.dragged_range == dr.ending;
		var is_adj_bwlo = this.dragged_range == dr.bwlo;
		var is_adj_bwhi = this.dragged_range == dr.bwhi;
		var is_BFO_PBS_BW = is_adj_BFO || this.dragged_range == dr.pbs || is_adj_bwlo || is_adj_bwhi;

		//dragging the line in the middle of the filter envelope while holding Shift does emulate
		//the BFO knob on radio equipment: moving offset frequency, while passband remains unchanged
		//Filter passband moves in the opposite direction than dragged, hence the minus below.
		var minus_lo = (is_adj_BFO || is_adj_bwhi)? -1:1;
		var minus_hi = (is_adj_BFO || is_adj_bwlo)? -1:1;

		//dragging any other parts of the filter envelope while holding Shift does emulate the PBS knob
		//(PassBand Shift) on radio equipment: PBS does move the whole passband without moving the offset
		//frequency.
		
		// calculate the proposed changes: both lo and hi can change if shifting passband
		var new_lo = this.drag_origin.low_cut;
		var do_lo = is_adj_locut || is_BFO_PBS_BW;
		if (do_lo) new_lo += minus_lo*freq_change;

		var new_hi = this.drag_origin.high_cut;
		var do_hi = is_adj_hicut || is_BFO_PBS_BW;
		if (do_hi) new_hi += minus_hi*freq_change;

		// validate the proposed changes
		if (do_lo) {
			//we don't let low_cut go beyond its limits
			if (new_lo < this.parent.filter.low_cut_limit) {
				//console.log('lo limit');
		      rv.d = 2;
				return rv;
			}
			//nor the filter passband be too small
			if (new_hi - new_lo < this.parent.filter.min_passband) {
				//console.log('lo min');
		      rv.d = 3;
				return rv;
			}
			//sanity check to prevent GNU Radio "firdes check failed: fa <= fb"
			if (new_lo >= new_hi) {
				//console.log('lo wrap');
		      rv.d = 4;
				return rv;
			}
		}
		
		if (do_hi) {
			//we don't let high_cut go beyond its limits
			if (new_hi > this.parent.filter.high_cut_limit) {
				//console.log('hi limit');
		      rv.d = 5;
				return rv;
			}
			//nor the filter passband be too small
			if (new_hi - new_lo < this.parent.filter.min_passband) {
				//console.log('hi min');
		      rv.d = 6;
				return rv;
			}
			//sanity check to prevent GNU Radio "firdes check failed: fa <= fb"
			if (new_hi <= new_lo) {
				//console.log('hi wrap');
		      rv.d = 7;
				return rv;
			}
		}
		
		// make the proposed changes
		if (do_lo) {
			owrx.last_lo[this.parent.server_mode] = this.parent.low_cut = new_lo;
			//console.log('DRAG-MOVE lo=', new_lo.toFixed(0));
		}
		
		if (do_hi) {
			owrx.last_hi[this.parent.server_mode] = this.parent.high_cut = new_hi;
			//console.log('DRAG-MOVE hi=', new_hi.toFixed(0));
		}
		
		if (this.dragged_range == dr.anything_else || is_adj_BFO) {
			//when any other part of the envelope is dragged, the offset frequency is changed (whole passband also moves with it)
			new_value = this.drag_origin.offset_frequency + freq_change;
			if (new_value > bandwidth/2 || new_value < -bandwidth/2) {
				//console.log('bfo range');
		      rv.d = 8;
				return rv; //we don't allow tuning above Nyquist frequency :-)
			}
			this.parent.offset_frequency = new_value;
			//console.log('DRAG-MOVE off=', new_value.toFixed(0));
		}
		
		// now do the actual modifications:
		mkenvelopes(this.visible_range);
		freqset_car_Hz(this.parent.offset_frequency + center_freq);
		this.parent.set();
		//console_nv('drag_move', {do_lo}, {do_hi});
		var fset = (do_lo || do_hi)? owrx.FSET_PB_CHANGE : owrx.FSET_SCALE_DRAG;
		freqset_update_ui(fset);
		//kiwi_trace();
		rv.fset = fset;
		return rv;
	};
	
	this.envelope.drag_end = function(x)
	{ //in this demodulator we've already changed values in the drag_move() function so we shouldn't do too much here.
		to_return = (this.dragged_range != demodulator.draggable_ranges.none); //this part is required for clicking anywhere on the scale to set offset
		this.dragged_range = demodulator.draggable_ranges.none;
		//console.log('this.envelope.drag_end x='+ x);
		//if (x) kiwi_trace();
		return to_return;
	};
	
}

function add_shift_listener(el)
{
   // couldn't get these global keyup/down to fire properly
   // so just call shift_event() from existing global keyup/down event handlers (hack)
	//el.addEventListener("keydown", shift_event, true);     // called via keyboard_shortcut_event()
	//el.addEventListener("keyup", shift_event, true);       // called via mouse_freq_remove()
	el.addEventListener("mouseover", shift_event, true);
}

function shift_event(evt, from)
{
   from = from || 'mouseover';
   var ok = (evt && (evt.type == 'mouseover' || (evt.key && evt.key == 'Shift')));
   //console.log('from='+ from +' evt='+ evt +' evt.key='+ evt.key +' ok='+ ok);
   if (!ok) return;
	//event_dump(evt, 'shift_event', 1);
	if (evt.key && evt.key == 'Shift') {
      owrx.last_shiftKey = evt.shiftKey;
      //console.log('shift_event: *** last_shiftKey='+ owrx.last_shiftKey);
   }
   
   // shift key has no effect on tooltip while dragging in progress
   if (demodulators[0] && demodulators[0].envelope.dragged_range == demodulator.draggable_ranges.none)
      mkenvelopes();
}

demodulator_default_analog.prototype = new demodulator();

function mkenvelopes(visible_range)    // called from mkscale etc
{
   if (isUndefined(visible_range)) visible_range = get_visible_freq_range();
	scale_ctx.clearRect(0,0,scale_ctx.canvas.width,22); //clear the upper part of the canvas (where filter envelopes reside)

   // show active label database on left side on scale
	scale_ctx.font = '13px sans-serif';
	scale_ctx.textBaseline = 'top';
	scale_ctx.fillStyle = 'white';
   scale_ctx.textAlign = "left";
   var s = dx.db_s[dx.db].split(' ')[0];
   if (kiwi.mdev) s += '  '+ kiwi.mdev_s;
   scale_ctx.fillText('database: '+ s, 20, 7);
   if (demodulators.length)
	   demodulators[0].envelope.draw(visible_range);
}

function demodulator_remove(which)
{
	demodulators[which].stop();
	demodulators.splice(which,1);
}

function demodulator_add(what)
{
	demodulators.push(what);
	if (waterfall_setup_done) mkenvelopes();
}

function demodulator_analog_replace(subtype, freq)
{ //this function should only exist until the multi-demodulator capability is added
   //console.log('demodulator_analog_replace subtype='+ subtype);
   if (kiwi_passbands(subtype) == null) subtype = 'am';

	var offset = 0, prev_pbo = 0;
	var wasCW = false, toCW = false, fromCW = false;

	if (demodulators.length) {
		wasCW = demodulators[0].isCW;
		offset = demodulators[0].offset_frequency;
		prev_pbo = passband_offset();
		demodulator_remove(0);
	} else {
	   var i_freq_kHz = init_frequency - kiwi.freq_offset_kHz;
		var i_freq_Hz = Math.round(i_freq_kHz * 1000);
      offset = (i_freq_Hz <= 0 || i_freq_Hz > bandwidth)? 0 : (i_freq_Hz - center_freq);
		//console.log('### init_freq='+ init_frequency +' freq_offset_kHz='+ kiwi.freq_offset_kHz +' i_freq_Hz='+ i_freq_Hz +' offset='+ offset +' init_mode='+ init_mode);
      owrx.prev_freq_kHz = [ i_freq_Hz, i_freq_Hz ];
      //console.log(owrx.prev_freq_kHz);
		subtype = isArg(init_mode)? init_mode : 'am';
	}
	
	// initial offset, but doesn't consider demod.isCW since it isn't valid yet
	if (isArg(freq)) {
		offset = freq - center_freq;
	}
	
	//console.log("DEMOD-replace calling add: INITIAL offset="+(offset+center_freq));
	demodulator_add(new demodulator_default_analog(offset, subtype, NaN, NaN));
	
	if (!wasCW && demodulators[0].isCW)
		toCW = true;
	if (wasCW && !demodulators[0].isCW)
		fromCW = true;
	
	// determine actual offset now that demod.isCW is valid
	if (isArg(freq)) {
		freq_car_Hz = freq_dsp_to_car(freq);
		//console.log('DEMOD-replace SPECIFIED freq='+ freq +' car='+ freq_car_Hz);
		offset = freq_car_Hz - center_freq;
		wf.audioFFT_clear_wf = true;
	} else {
		// Freq not changing, just mode. Do correct thing for switch to/from cw modes: keep display freq constant
		var pbo = 0;
		if (toCW) pbo = -passband_offset();
		if (fromCW) pbo = prev_pbo;	// passband offset calculated _before_ demod was changed
		offset += pbo;
		
		// clear switching to/from cw mode because of frequency offset
		if (toCW || fromCW) {
		   wf.audioFFT_clear_wf = true;
      }
		//console.log('DEMOD-replace SAME freq='+ (offset + center_freq) +' PBO='+ pbo +' prev='+ prev_pbo +' toCW='+ toCW +' fromCW='+ fromCW);
	}

   if (cur_mode != 'drm')
      extint.prev_mode = cur_mode;
	cur_mode = subtype;
   if (toCW || fromCW)
      set_agc();
	//console.log("demodulator_analog_replace: cur_mode="+ cur_mode);

	// must be done here after demod is added, so demod.isCW is available after demodulator_add()
	// done even if freq unchanged in case mode is changing
	//console.log("DEMOD-replace calling set: FINAL freq="+ (offset + center_freq));
	demodulator_set_offset_frequency(owrx.FSET_ADD, offset);
	
	try_modeset_update_ui(subtype);
}

function demodulator_set_frequency(which, freq_car_Hz)
{
	//console.log('demodulator_set_frequency: freq_car_Hz='+ freq_car_Hz);
   demodulator_set_offset_frequency(which, freq_car_Hz - center_freq);
}

function demodulator_set_offset_frequency(which, offset)
{
	if (offset > bandwidth/2 || offset < -bandwidth/2) return;
	offset = Math.round(offset);
	//console.log('demodulator_set_offset_frequency: offset='+ (offset + center_freq));
	
	// set carrier freq before demodulators[0].set() below
	// requires demodulators[0].isCW to be valid
	freqset_car_Hz(offset + center_freq);
	
	var demod = demodulators[0];
	demod.offset_frequency = offset;
	demod.set();
	try_freqset_update_ui(which);
}

function owrx_init_cfg()
{
   w3_do_when_rendered('id-rx-gps',
      function() {
         update_web_grid(true);
         update_web_map(true);
      }
   );
   // REMINDER: w3_do_when_rendered() returns immediately
   
   owrx.cfg_loaded = true;
}


////////////////////////////////
// #scale
////////////////////////////////

var scale_ctx, band_ctx, dx_ctx;
var pb_adj_cf, pb_adj_cf_ttip, pb_adj_lo, pb_adj_lo_ttip, pb_adj_hi, pb_adj_hi_ttip, pb_adj_car, pb_adj_car_ttip;
var scale_canvas, band_canvas, dx_div, dx_canvas;

function scale_setup()
{
	w3_el('id-scale-container').addEventListener("mouseover", scale_container_mouseover, false);
	w3_el('id-scale-container').addEventListener("mouseout", scale_container_mouseout, false);
   
	scale_canvas = w3_el("id-scale-canvas");
	scale_ctx = scale_canvas.getContext("2d");
	add_scale_listener(scale_canvas);

	pb_adj_car = w3_el("id-pb-adj-car");
	pb_adj_car.innerHTML = '<span id="id-pb-adj-car-ttip" class="class-passband-adjust-car-tooltip class-tooltip-text"></span>';
	pb_adj_car_ttip = w3_el("id-pb-adj-car-ttip");
	add_scale_listener(pb_adj_car);
	add_shift_listener(pb_adj_car);

	pb_adj_lo = w3_el("id-pb-adj-lo");
	pb_adj_lo.innerHTML = '<span id="id-pb-adj-lo-ttip" class="class-passband-adjust-cut-tooltip class-tooltip-text"></span>';
	pb_adj_lo_ttip = w3_el("id-pb-adj-lo-ttip");
	add_scale_listener(pb_adj_lo);
	add_shift_listener(pb_adj_lo);

	pb_adj_hi = w3_el("id-pb-adj-hi");
	pb_adj_hi.innerHTML = '<span id="id-pb-adj-hi-ttip" class="class-passband-adjust-cut-tooltip class-tooltip-text"></span>';
	pb_adj_hi_ttip = w3_el("id-pb-adj-hi-ttip");
	add_scale_listener(pb_adj_hi);
	add_shift_listener(pb_adj_hi);

	pb_adj_cf = w3_el("id-pb-adj-cf");
	pb_adj_cf.innerHTML = '<span id="id-pb-adj-cf-ttip" class="class-passband-adjust-cf-tooltip class-tooltip-text"></span>';
	pb_adj_cf_ttip = w3_el("id-pb-adj-cf-ttip");
	add_scale_listener(pb_adj_cf);
	add_shift_listener(pb_adj_cf);

	band_canvas = w3_el("id-band-canvas");
	band_ctx = band_canvas.getContext("2d");
	add_canvas_listener(band_canvas);
	
	dx_div = w3_el('id-dx-container');
	add_canvas_listener(dx_div);

	dx_canvas = w3_el("id-dx-canvas");
	dx_ctx = dx_canvas.getContext("2d");
	add_canvas_listener(dx_canvas);
	
	resize_scale('setup');
}

function add_scale_listener(obj)
{
	obj.addEventListener("mousedown", scale_canvas_mousedown, false);
	obj.addEventListener("mousemove", scale_canvas_mousemove, false);
	obj.addEventListener("mouseup", scale_canvas_mouseup, false);
	obj.addEventListener("contextmenu", scale_canvas_contextmenu, false);
	obj.addEventListener("wheel", canvas_mousewheel_ev, false);    // yes, canvas_*, not scale_canvas_*

	if (kiwi_isMobile()) {
		obj.addEventListener('touchstart', scale_canvas_touchStart, false);
		obj.addEventListener('touchmove', scale_canvas_touchMove, false);
		obj.addEventListener('touchend', scale_canvas_touchEnd, false);
	}
}

function scale_canvas_contextmenu(evt)
{
	//console.log('## SCMENU tgt='+ evt.target.id +' Ctgt='+ evt.currentTarget.id);
	return cancelEvent(evt);
}

var scale_canvas_params = {
	mouse_down: false,
	drag: false,
	start_x: 0,
	last_x: 0,
	start_y: 0,
	last_y: 0,
	key_modifiers: { shiftKey:false, altKey: false, ctrlKey: false }
};

function scale_canvas_mousedown(evt)
{
	//event_dump(evt, 'SC-MDN', 1);
	mouse_button_state(evt);
   scale_canvas_params.drag = false;
   scale_canvas_params.start_x = evt.pageX;
   scale_canvas_params.start_y = evt.pageY;
   scale_canvas_params.key_modifiers.shiftKey = evt.shiftKey;
   scale_canvas_params.key_modifiers.altKey = evt.altKey;
   scale_canvas_params.key_modifiers.ctrlKey = evt.ctrlKey;

   if (!w3_menu_active())
	   scale_canvas_start_drag(evt, 1);
	evt.preventDefault();
}

function scale_canvas_touchStart(evt)
{
   scale_canvas_params.drag = false;
   scale_canvas_params.last_x = start_x = evt.targetTouches[0].pageX;
   scale_canvas_params.last_y = start_y = evt.targetTouches[0].pageY;
   scale_canvas_params.key_modifiers.shiftKey = false;
   scale_canvas_params.key_modifiers.altKey = false;
   scale_canvas_params.key_modifiers.ctrlKey = false;
   scale_canvas_start_drag(evt, 0);
	
	touch_restore_tooltip_visibility(owrx.scale_canvas);
	evt.preventDefault();
}

var scale_canvas_ignore_mouse_event = false;

function scale_canvas_start_drag(evt, isMouse)
{
	// Distinguish ctrl-click right-button meta event from actual right-button on mouse (or touchpad two-finger tap).
	if (evt.button == mouse.RIGHT && !evt.ctrlKey) {
      if (w3_menu_active())
         w3_menu_close('1');     // close menu if another DTS while menu open
      else
		   right_click_menu(scale_canvas_params.start_x, scale_canvas_params.start_y, 1);
      scale_canvas_params.mouse_down = owrx.scale_canvas.mouse_out = false;
		scale_canvas_ignore_mouse_event = true;
		return;
	}

   scale_canvas_params.mouse_down = true;
   //console.log('SCSD MD=T');
   owrx.scale_canvas.target = evt.target;
   if (isMouse) scale_canvas_mousemove(evt);
}

function scale_carfreq_from_px(x, visible_range)
{
	if (isUndefined(visible_range)) visible_range = get_visible_freq_range();
	var offset = passband_offset();
	var f = visible_range.start + visible_range.bw * (x / wf_container.clientWidth);
	//console.log("SOCFFPX f="+f+" off="+offset+" f-o="+(f-offset)+" rtn="+(f - center_freq - offset));
	return f - offset;
}

function canvas_freq_upd(from, x)
{
   var area;
   var fnew = Math.round(scale_carfreq_from_px(x));
   var pb = freq_passband(fnew);
   //console.log('>'+ pb.lo +'|'+ pb.pbc +'|'+ pb.hi);
   var margin = Math.round(g_range.bw * 0.05);
   var lo_lim = g_range.start + margin;
   var hi_lim = g_range.end - margin;
   //console.log(lo_lim +'|'+ pb.lo +'_'+ pb.hi +'|'+ hi_lim);
   if (pb.lo <= lo_lim) {
      area = owrx.LEFT_MARGIN;
   } else
   if (pb.hi >= hi_lim) {
      area = owrx.RIGHT_MARGIN;
   } else {
      area = owrx.NOT_MARGIN;
   }
   
   // If called from FSET_SCALE_DRAG update freq even if in margin since waterfall will also scroll.
   //
   // If called from FSET_SCALE_END and cursor (x value) positioned in margins don't let freq update
   // so passband doesn't jump into margin
   if (from != owrx.FSET_SCALE_END || (from == owrx.FSET_SCALE_END && area == 0))
      demodulator_set_frequency(from, fnew);
   return area;
}

function scale_canvas_drag(evt, x, y)
{
   var i;
   if (scale_canvas_ignore_mouse_event) return;
   
	//event_dump(evt, 'SC-MDRAG', 1);
	var rv, event_handled = 0;
	var relX = Math.abs(x - scale_canvas_params.start_x);

	if (scale_canvas_params.mouse_down && !scale_canvas_params.drag /* && relX > canvas_drag_min_delta */ ) {
		// we can use the main drag_min_delta thing of the main canvas
		scale_canvas_params.drag = true;
		// call env_drag_start() for all demodulators (and they will decide if they're dragged, based on X coordinate)
      event_handled |= demodulators[0].envelope.env_drag_start(x, y, scale_canvas_params.key_modifiers);
		//console.log("MOV1 evh? "+event_handled);
		evt.target.style.cursor = "move";
	   //console.log('SCD drag START');
		//console.log('sc cursor');
	}     // scale is different from waterfall: mousedown alone immediately changes frequency

	if (scale_canvas_params.drag) {
		// call drag_move for demodulator (will decide if it's dragged)
		rv = demodulators[0].envelope.drag_move(x);
      event_handled |= rv.event_handled;
	   //console.log('SCD drag fset='+ rv.fset +' event_handled='+ event_handled);
		//console.log("MOV2 evh? "+event_handled);

      // #mobile-ui Fscale: scroll tuning in Fscale margins instead of stopping at scale end
      //var eh = TF(event_handled);
      //var pbc = TF(rv.fset == owrx.FSET_PB_CHANGE);
		if (!event_handled || rv.fset != owrx.FSET_PB_CHANGE) {
		   //jksx FIXME "backwards" fScale scroll for small movements
         var deltaX = scale_canvas_params.last_x - x;
		   var area = canvas_freq_upd(owrx.FSET_SCALE_DRAG, x);
         //console_log_dbgUs('SC area='+ area +' x='+ x +'|'+ scale_canvas_params.last_x +'|'+ deltaX);

		   if (area != owrx.NOT_MARGIN && mobileOpt()) {
		      //canvas_log(eh + pbc + area +'A'); // FF1A
		      // PB is in L/R margin -- also scroll waterfall
            var dbins = norm_to_bins(deltaX / waterfall_width);
            //console_log_dbgUs('dX='+ deltaX +' dbins='+ dbins);
            waterfall_pan_canvases(-dbins);
		   } else {
		      //canvas_log(eh + pbc + area +'p'); // FF0Tp
		      //canvas_log(eh + pbc + rv.fset + rv.d +'p'); // FF01p
            //console_log_dbgUs('SC x='+ x +'|'+ scale_canvas_params.last_x);
            scale_canvas_params.last_x = x;     // accept x movement
		   }
		}
		//else canvas_log(eh + pbc +'D');
	}
}

function scale_canvas_mousemove(evt)
{
	//event_dump(evt, 'SC-MM');
	//canvas_log('$'+ evt.pageY +' '+ evt.offsetY +' '+ (evt.pageY - owrx.scale_offsetY));
	scale_canvas_drag(evt, evt.pageX, evt.offsetY);
}

function scale_canvas_touchMove(evt)
{
   //canvas_log('TM');
	for (var i=0; i < evt.touches.length; i++) {
	   // no evt.touches[].offsetY in touch API, so do it ourselves
	   var offsetY = evt.touches[i].pageY - owrx.scale_offsetY;
		scale_canvas_drag(evt, evt.touches[i].pageX, offsetY);
	   //canvas_log('$'+ evt.touches[i].pageY +'-'+ owrx.scale_offsetY +'='+ offsetY);
	}
	evt.preventDefault();
}

function scale_canvas_end_drag(evt, x, canvas_mouse_up)
{
   //if (canvas_mouse_up == true)
   //   console.log('IME='+ scale_canvas_ignore_mouse_event +' SCMD='+ scale_canvas_params.mouse_down +' ET='+ evt.type);
   
   if (scale_canvas_ignore_mouse_event) {
      scale_canvas_ignore_mouse_event = scale_canvas_params.mouse_down = false;
      return;
   }


   // mouse was down when it went out -- let dragging continue in adjacent canvas
   // which will call us again when canvas_mouse_up occurs
   if (scale_canvas_params.mouse_down == true && evt.type == "mouseout") {
      //console.log('@mouse_out');
      owrx.scale_canvas.mouse_out = true;
      return;
   }

	scale_canvas_params.drag = false;
	var event_handled = false;
	
	if (scale_canvas_params.mouse_down == true) {
      event_handled |= demodulators[0].envelope.drag_end(x);
      //console.log("MED evh? "+event_handled);
      if (!event_handled) {
         canvas_freq_upd(owrx.FSET_SCALE_END, x);
      }
   }

	scale_canvas_params.mouse_down = false;
	//console.log('SCED MD=F');
	var target = owrx.scale_canvas.target;
	if (target && target.style) {
	   target.style.cursor = null;      // re-enable default mouseover cursor in .css (if any)
      //console.log('SCED cursor restore');
      //console.log(target);
	}
}

function scale_canvas_mouseup(evt)
{
	//event_dump(evt, 'SC-MUP', 1);
	mouse_button_state(evt);
	scale_canvas_end_drag(evt, evt.pageX);
}


// Solves this problem: Using a mouse, the tooltip will eventually disappear when the mouse wanders
// outside of the tooltip, because mouse-move events still occur even though the mouse button is up.
//
// But for touch events a touch-end *may* occur while still positioned over the tooltip or its
// container. When there are no more touches the tooltip remains displayed until the next touch-start.
//
// This is not so desirable. What we do is force-hide tooltip on touch-end after a delay and
// then re-enable them on the next touch-start.

function touch_hide_tooltip(evt, state)
{
	var unHover = '';
   var pe = state.target;     // parent is the pb adjust elem detected at the touchStart

   if (w3_contains(pe, 'class-tooltip')) {
      var ce = pe.childNodes[0];    // child is the tooltip itself
      if (w3_contains(ce, 'class-tooltip-text')) {
         unHover += '-T';
         state.restore_visibility_timeout = setTimeout(function() {
            ce.style.visibility = 'hidden';
            state.restore_visibility.push(ce);
         }, state.restore_visibility_delay);
      }
   }

	//canvas_log('TTE'+ unHover);
}

function touch_restore_tooltip_visibility(state)
{
   kiwi_clearTimeout(state.restore_visibility_timeout);
	state.restore_visibility.forEach(function(el) {
	   el.style.visibility = '';
	});
	state.restore_visibility = [];
   //canvas_log('TTS');
}

function scale_canvas_touchEnd(evt)
{
	scale_canvas_end_drag(evt, scale_canvas_params.last_x);
	touch_hide_tooltip(evt, owrx.scale_canvas);
	evt.preventDefault();
}

function scale_container_mouseover(evt)
{
   if (owrx.debug_drag) canvas_log('SMO Cdrag='+ TF(canvas_dragging) +' Sdrag='+ TF(owrx.spec_pb_mkr_dragging));

   // if mouse wanders outside spectrum cancel the tooltip update process and spectrum dragging
   if (canvas_dragging || owrx.spec_pb_mkr_dragging) {
      //console.log('canvas_dragging='+ canvas_dragging +' spec_pb_mkr_dragging='+ owrx.spec_pb_mkr_dragging);
      spectrum_tooltip_cancel(owrx.EV_DRAG);
      canvas_end_drag2();
   }
}

// When mouseup occurs outside our original canvas scale_canvas_mouseup() doesn't occur terminating the drag.
// So have to detect canvas crossing and terminate that way.
function scale_container_mouseout(evt)
{
   // Prevent mouseout generated from mouseover of passband elements from prematurely ending drag.
   // Can't use "pb_el.style.pointerEvents = 'none'" trick because that disables pb tooltip.
   var trel = evt.relatedTarget;
   trel = (isObject(trel) && trel != null && isDefined(trel.id))? trel.id : null;
   if (trel && (trel.startsWith('id-pb-adj') || trel.startsWith('id-scale'))) return;
   scale_canvas_mouseup(evt);
}

function scale_px_from_freq(f, range) { return Math.round(((f - range.start) / range.bw) * wf_container.clientWidth); }
function scale_px_from_freq_offset(f, range) { return Math.round(((f - range.start_offset) / range.bw) * wf_container.clientWidth); }

function get_visible_freq_range()
{
	out={};
	
	if (wf.audioFFT_active && cur_mode && demodulators.length != 0) {
	   var off, span;
      var srate = Math.round(audio_input_rate || 12000);
	   if (ext_is_IQ_or_stereo_curmode()) {
	      off = 0;
	      span = srate;
	   } else
	   if (ext_mode(cur_mode).LSB_SAL) {
	      off = 0;
	      span = srate/2;
	   } else {
	      off = srate/4;
	      span = srate/2;
	   }
      out.center = center_freq + demodulators[0].offset_frequency + off;
      out.start = out.center - span;
      out.end = out.center + span;
      x_bin = freq_to_bin(out.start);
      out.bw = out.end-out.start;
      out.hpp = out.bw / scale_canvas.clientWidth;
	   //console.log('GVFR'+ (wf.audioFFT_active? '(audioFFT)':'') +" mode="+cur_mode+" xb="+x_bin+" s="+out.start+" c="+out.center+" e="+out.end+" bw="+out.bw+" hpp="+out.hpp+" cw="+scale_canvas.clientWidth);
	} else {
	   var bins = bins_at_cur_zoom();
      out.start = bin_to_freq(x_bin);
      out.center = bin_to_freq(x_bin + bins/2);
      out.end = bin_to_freq(x_bin + bins);
      out.bw = out.end-out.start;
      out.hpp = out.bw / scale_canvas.clientWidth;
	   //console.log("GVFR z="+zoom_level+" xb="+x_bin+" BACZ="+bins+" s="+out.start+" c="+out.center+" e="+out.end+" bw="+out.bw+" hpp="+out.hpp+" cw="+scale_canvas.clientWidth);
	}
	
	out.start_offset = out.start + kiwi.offset_frac;
	return out;
}

var scale_markers_levels = [
	{
		"hz_per_large_marker":10000000, //large
		"estimated_text_width":70,
		"format":"{x} ",
		"pre_divide":1000000,
		"decimals":0
	},
	{
		"hz_per_large_marker":5000000,
		"estimated_text_width":70,
		"format":"{x} ",
		"pre_divide":1000000,
		"decimals":0
	},
	{
		"hz_per_large_marker":1000000,
		"estimated_text_width":70,
		"format":"{x} ",
		"pre_divide":1000000,
		"decimals":0
	},
	{
		"hz_per_large_marker":500000,
		"estimated_text_width":70,
		"format":"{x} ",
		"pre_divide":1000000,
		"decimals":1
	},
	{
		"hz_per_large_marker":100000,
		"estimated_text_width":70,
		"format":"{x} ",
		"pre_divide":1000000,
		"decimals":1
	},
	{
		"hz_per_large_marker":50000,
		"estimated_text_width":70,
		"format":"{x} ",
		"pre_divide":1000000,
		"decimals":2
	},
	{
		"hz_per_large_marker":10000,
		"estimated_text_width":70,
		"format":"{x} ",
		"pre_divide":1000000,
		"decimals":2
	},
	{
		"hz_per_large_marker":5000,
		"estimated_text_width":70,
		"format":"{x} ",
		"pre_divide":1000000,
		"decimals":3
	},
	{
		"hz_per_large_marker":1000,
		"estimated_text_width":70,
		"format":"{x} ",
		"pre_divide":1000000,
		"decimals":3
	}
];

var scale_min_space_btwn_texts = 50;
var scale_min_space_btwn_small_markers = 7;

function get_scale_mark_spacing(range)
{
	out = {};
	fcalc = function(mkr_spacing) { 
		out.numlarge = (range.bw/mkr_spacing);
		out.pxlarge = wf_container.clientWidth/out.numlarge; 	//distance between large markers (these have text)
		out.ratio = 5; 															//(ratio-1) small markers exist per large marker
		out.pxsmall = out.pxlarge/out.ratio; 								//distance between small markers
		if (out.pxsmall < scale_min_space_btwn_small_markers) return false; 
		if (out.pxsmall/2 >= scale_min_space_btwn_small_markers && mkr_spacing.toString()[0] != "5") { out.pxsmall/=2; out.ratio*=2; }
      //w3_innerHTML('id-owner-info', 'mkr_spacing='+ kHz(mkr_spacing));
		out.smallbw = mkr_spacing/out.ratio;
		return true;
	};

	for (var i=scale_markers_levels.length-1; i>=0; i--) {
		mp = scale_markers_levels[i];
		if (!fcalc(mp.hz_per_large_marker)) continue;
		//console.log(mp.hz_per_large_marker);
		//console.log(out);
		if (out.pxlarge-mp.estimated_text_width > scale_min_space_btwn_texts) break;
	}
	
	//console.log("using");
	//console.log(wf_container);
	//console.log(range);
	//console.log(mp);
	//console.log(out);
	out.params = mp;
	return out;
}

var g_range;

function mk_freq_scale()
{
	// clear the lower part of the canvas (where frequency scale resides; the upper part is used by filter envelopes):
	g_range = get_visible_freq_range();
	mkenvelopes(g_range);   // when scale changes we will always have to redraw filter envelopes, too

	scale_ctx.clearRect(0,22,scale_ctx.canvas.width,scale_ctx.canvas.height-22);
	scale_ctx.strokeStyle = "#fff";
	scale_ctx.font = "bold 12px sans-serif";
	scale_ctx.textBaseline = "top";
	scale_ctx.fillStyle = "#fff";
	
	var spacing = get_scale_mark_spacing(g_range);
	//console.log(spacing);
	var marker_hz = Math.ceil(g_range.start_offset / spacing.smallbw) * spacing.smallbw;
	var marker_hz_offset = marker_hz + kiwi.freq_offset_Hz - kiwi.offset_frac;
   //console.log('mkfs z'+ zoom_level +' freq_offset_kHz='+ kiwi.freq_offset_kHz +' offset_frac='+ kHz(kiwi.offset_frac) +' marker_hz='+ kHz(marker_hz) +'|'+ kHz(marker_hz_offset));
	text_y_pos = 22+10 + (kiwi_isFirefox()? 3:0);
	var text_to_draw;
	
	var ftext = function(fo) {
	   var f = fo;
		var pre_divide = spacing.params.pre_divide;
		var decimals = spacing.params.decimals;
		f += kiwi.freq_offset_Hz - kiwi.offset_frac;
		if (f < 1e6) {
			pre_divide /= 1000;
			decimals = 0;
		}
		text_to_draw = format_frequency(spacing.params.format+((f < 1e6)? 'kHz':'MHz'), f, pre_divide, decimals);
      //console.log('ftext-mkfs z'+ zoom_level +' '+ spacing.params.hz_per_large_marker +' marker_hz='+ kHz(marker_hz) +'|'+ kHz(marker_hz_offset) +' fo|f='+ kHz(fo) +'|'+ kHz(f) +' "'+ text_to_draw +'"');
	};
	
	var last_large;
   var conv_ct=0;
   var x, first_large;

	for (;;) {
      conv_ct++;
      if (conv_ct > 1000) break;
		x = scale_px_from_freq_offset(marker_hz, g_range);
      //console.log('mkfs marker_hz|HPLM|0?='+ kHz(marker_hz) +'|'+ kHz(marker_hz_offset) +' '+ kHz(spacing.params.hz_per_large_marker) +' '+ (marker_hz_offset % spacing.params.hz_per_large_marker) +' x='+ x);
		if (x > window.innerWidth) break;
		scale_ctx.beginPath();		
		scale_ctx.moveTo(x, 22);

		if (marker_hz_offset % spacing.params.hz_per_large_marker == 0) {

			//large marker
			if (isUndefined(first_large)) first_large = marker_hz; 
			last_large = marker_hz;
			scale_ctx.lineWidth = 3.5;
			scale_ctx.lineTo(x,22+11);
			ftext(marker_hz);
			var text_measured = scale_ctx.measureText(text_to_draw);
			scale_ctx.textAlign = "center";

			//advanced text drawing begins
			//console.log('text_to_draw='+ text_to_draw);
			//console.log('adv draw: '+ (g_range.start_offset + spacing.smallbw * spacing.ratio) +' >? marker_hz='+ marker_hz);
			if (zoom_level == 0 && g_range.start + spacing.smallbw * spacing.ratio > marker_hz) {

				//if this is the first overall marker when zoomed all the way out
				//console.log('case 1 x='+ x +' twh='+ text_measured.width/2);
				if (x <= text_measured.width/2) {
				   //and if it would be clipped off the screen
				   //console.log('case 1.1');
					if (scale_px_from_freq_offset(marker_hz + spacing.smallbw * spacing.ratio, g_range) - text_measured.width >= scale_min_space_btwn_texts) {
					   //and if we have enough space to draw it correctly without clipping
				      //console.log('case 1.2');
						scale_ctx.textAlign = "left";
						scale_ctx.fillText(text_to_draw, 0, text_y_pos); 
					}
				} else {
			      //draw text normally
               //console.log('case 1.3');
               scale_ctx.fillText(text_to_draw, x, text_y_pos);
				}
			} else
			
			if (zoom_level == 0 && g_range.end - spacing.smallbw * spacing.ratio < marker_hz) {

			   //if this is the last overall marker when zoomed all the way out
				//console.log('case 2');
				if (x > window.innerWidth - text_measured.width/2) {
				   //and if it would be clipped off the screen
					if (window.innerWidth-text_measured.width-scale_px_from_freq_offset(marker_hz-spacing.smallbw*spacing.ratio, g_range) >= scale_min_space_btwn_texts) {
					   //and if we have enough space to draw it correctly without clipping
				      //console.log('case 2.1');
						scale_ctx.textAlign = "right";
						scale_ctx.fillText(text_to_draw, window.innerWidth, text_y_pos); 
					}	
				} else {
					// last large marker is not the last marker, so draw normally
				   //console.log('case 2.2');
					scale_ctx.fillText(text_to_draw, x, text_y_pos);
				}
			} else {
			   //draw text normally
				//console.log('case 3');
				scale_ctx.fillText(text_to_draw, x, text_y_pos);
			}
		} else {
		
			//small marker
			scale_ctx.lineWidth = 2;
			scale_ctx.lineTo(x,22+8);
		}
		
		marker_hz += spacing.smallbw;
		marker_hz_offset += spacing.smallbw;
		scale_ctx.stroke();
	}

   if (conv_ct > 1000) { console.log("CONV_CT > 1000!!!"); kiwi_trace(); }

	if (zoom_level != 0) {	// if zoomed, we don't want the texts to disappear because their markers can't be seen
		// on the left side
		scale_ctx.textAlign = "center";
		var f = first_large - spacing.smallbw * spacing.ratio;
		x = scale_px_from_freq_offset(f, g_range);
		ftext(f);
		var w = scale_ctx.measureText(text_to_draw).width;
		if (x+w/2 > 0) scale_ctx.fillText(text_to_draw, x, 22+10);

		// on the right side
		f = last_large + spacing.smallbw * spacing.ratio;
		x = scale_px_from_freq_offset(f, g_range);
		ftext(f);
		w = scale_ctx.measureText(text_to_draw).width;
		if (x-w/2 < window.innerWidth) scale_ctx.fillText(text_to_draw, x, 22+10);
	}
}

// carrier marker symbol dimensions
var dx_car_size = 8;
var dx_car_border = 3;
var dx_car_w = dx_car_border*2 + dx_car_size;
var dx_car_h = dx_car_border*2 + dx_car_size;

function resize_scale(from)
{
	band_ctx.canvas.width  = window.innerWidth;
	band_ctx.canvas.height = band_canvas_h;

	dx_div.style.width = px(window.innerWidth);

	dx_ctx.canvas.width = dx_car_w;
	dx_ctx.canvas.height = dx_car_h;
	dx_canvas.style.top = px(scale_canvas_top - dx_car_h + 2);
	dx_canvas.style.left = 0;
   dx_canvas.style.zIndex = 100;
	dx_show(false);
	
	// the dx canvas is used to form the "carrier" marker symbol (yellow triangle) seen when
	// an NDB dx label is entered by the mouse (and used by labels with non-zero sig bw)
   dx_ctx.miterLimit = 2;
   dx_ctx.lineJoin = "round";
	dx_ctx.beginPath();
	dx_ctx.lineWidth = dx_car_border-1;
	dx_ctx.strokeStyle='black';
	var o = dx_car_border;
	var x = dx_car_size;
	dx_ctx.moveTo(o-1,o);
	dx_ctx.lineTo(o+x,o);
	dx_ctx.lineTo(o+x/2,o+x);
	dx_ctx.lineTo(o,o);
	dx_ctx.stroke();
	dx_ctx.fillStyle = "yellow";
	dx_ctx.fill();

	scale_ctx.canvas.width  = window.innerWidth;
	scale_ctx.canvas.height = scale_canvas_h;
	mkscale();	
   dx_schedule_update();
}

function format_frequency(format, freq_hz, pre_divide, decimals)
{
	out = format.replace("{x}",(freq_hz/pre_divide).toFixed(decimals));
	if (0) {
		at=out.indexOf(".")+4;
		while(decimals>3)
		{
			out=out.substr(0,at)+","+out.substr(at);
			at+=4;
			decimals-=3;
		}
	}
	return out;
}

function mkscale()
{
	mk_freq_scale();
	mk_bands_scale();

   if (dbgUs && kiwi.mnew) {
      scale_ctx.lineWidth = 1;
      scale_ctx.strokeStyle = 'red';
      scale_ctx.beginPath();
      scale_ctx.moveTo(waterfall_width/2, 0);
      scale_ctx.lineTo(waterfall_width/2, 46);
      scale_ctx.stroke();
   }
}


////////////////////////////////
// #conversions
////////////////////////////////

// A "bin" is the wf_fft_size multiplied by the maximum zoom factor.
// So there are approx 1024 * 2^14 = 16M bins.
// The left edge of the waterfall is specified with a bin number.
// The higher precision of having a large number of bins makes the code simpler.
// Remember that the actual displayed waterfall_width is typically larger than the
// wf_fft_size data in the canvas due to stretching of the canvas to fit the screen.

function bins_at_zoom(zoom)
{
	var bins = wf_fft_size << (zoom_levels_max - zoom);
	return bins;
}

function bins_at_cur_zoom()
{
	return bins_at_zoom(zoom_level);
}

// norm: normalized position, e.g. 0..1 cursor position on window
function norm_to_bins(norm)
{
	return Math.round(norm * bins_at_cur_zoom());
}

function bin_to_freq(bin) {
	var max_bins = wf_fft_size << zoom_levels_max;
	return Math.round((bin / max_bins) * bandwidth);
}

function freq_to_bin(freq) {
	var max_bins = wf_fft_size << zoom_levels_max;
	return Math.round(freq/bandwidth * max_bins);
}

function bins_to_pixels_frac(cf, bins, zoom) {
	var bin_ratio = bins / bins_at_zoom(zoom);
	if (sb_trace) console.log('bins_to_pixels_frac bins='+ bins +' z='+ zoom +' ratio='+ bin_ratio);
	if (bin_ratio > 1) bin_ratio = 1;
	if (bin_ratio < -1) bin_ratio = -1;
	var f_pixels = wf_fft_size * bin_ratio;
	return f_pixels;
}

function bins_to_pixels(cf, bins, zoom) {
	var f_pixels = bins_to_pixels_frac(cf, bins, zoom);
	var i_pixels = Math.round(f_pixels);
	return i_pixels;
}

function freq_to_pixel(freq) {
	var bins = freq_to_bin(freq) - x_bin;
	if (!(bins >= 0 && bins < bins_at_cur_zoom())) {
	   console.log("freq_to_pixel: bins="+bins+" bins_at_cur_zoom="+bins_at_cur_zoom());
		console.assert("assert fail");
	}
	var pixels = bins_to_pixels(0, bins, zoom_level);
	return pixels;
}

// clamp xbin (left edge of waterfall) to bin number available at current zoom level
function clamp_xbin(xbin)
{
	if (xbin < 0) {
	   //console.log('$clamp_xbin '+ xbin +' < 0');
	   //kiwi_trace('clamp_xbin');
	   xbin = 0;
	}
	var max_bins = wf_fft_size << zoom_levels_max;
	var max_xbin_at_cur_zoom = max_bins - bins_at_cur_zoom();		// because right edge would be > max_bins
	if (xbin > max_xbin_at_cur_zoom) xbin = max_xbin_at_cur_zoom;
	return xbin;
}

// if the center freq of the passband is visible in the waterfall:
//		returns the bin of the passband center freq
//		else returns the (negative bin - 1) of passband outside the waterfall

function passband_visible()
{
	if (isUndefined(demodulators[0])) return x_bin;    // punt if no demod yet
	var f = freq_passband_center();
	var pb_bin = freq_to_bin(f);
	var x_max = x_bin + bins_at_cur_zoom();
	var inside = (pb_bin >= x_bin && pb_bin < x_max);
	//console.log('PBV f='+ f +' '+ x_bin +'|'+ pb_bin +'|'+ x_max +' inside='+ TF(inside));
	return { bin:pb_bin, inside:inside };
}


////////////////////////////////
// #canvas
////////////////////////////////

var window_width;
var waterfall_width;

var main_container;
var wf_container;
var canvas_annotation;
var canvas_phantom;

var annotation_div;

// NB: canvas data width is wf_fft_size, but displayed style width is waterfall_width (likely different),
// so image is stretched to fit when rendered by browser.

function create_canvas(id, w, h, style_w, style_h)
{	
	var new_canvas = document.createElement('canvas');
	new_canvas.id = id;
	new_canvas.width = w;
	new_canvas.height = h;
	if (style_w) new_canvas.style.width = px(style_w);	
	if (style_h) new_canvas.style.height = px(style_h);	
	new_canvas.ctx = new_canvas.getContext("2d");
	return new_canvas;
}

function init_wf_container()
{
	window_width = window.innerWidth;		// window width minus any scrollbar

	wf_container = w3_el("id-waterfall-container");
	waterfall_width = wf_container.clientWidth;
	//console.log("init_wf_container ww="+waterfall_width);

	// annotation canvas for FSK shift markers etc.
   canvas_annotation = create_canvas('id-annotation-canvas', wf_fft_size, wf_canvas_default_height, waterfall_width, wf_canvas_default_height);
   canvas_annotation.style.zIndex = 1;    // make on top of waterfall
	wf_container.appendChild(canvas_annotation);
	add_canvas_listener(canvas_annotation);
	w3_el('id-kiwi-body').addEventListener('keyup', mouse_freq_remove, false);
	
	// annotation div for text containing links etc.
	annotation_div = w3_el('id-annotation-div');
	add_canvas_listener(annotation_div);
	annotation_div.style.pointerEvents = 'none';    // prevent premature end of canvas dragging

	// a phantom one at the end
	// not an actual canvas but a <div> spacer
	canvas_phantom = w3_el('id-phantom-canvas');
	add_canvas_listener(canvas_phantom);
	canvas_phantom.style.width = px(wf_container.clientWidth);

	// the first one to get started
	add_wf_canvas();

   spec.canvas = create_canvas('id-spectrum-canvas', wf_fft_size, spec.height_spectrum_canvas, waterfall_width, spec.height_spectrum_canvas);
	w3_el('id-spectrum-container').appendChild(spec.canvas);
	spec.ctx = spec.canvas.ctx;
	add_canvas_listener(spec.canvas);
	spec.ctx.font = "10px sans-serif";
	spec.ctx.textBaseline = "middle";
	spec.ctx.textAlign = "left";

   spec.pb_canvas = create_canvas('id-spectrum-pb-canvas', wf_fft_size, spec.height_spectrum_canvas, waterfall_width, spec.height_spectrum_canvas);
	w3_el('id-spectrum-container').appendChild(spec.pb_canvas);
	spec.pb_canvas.style.position = "absolute";
	spec.pb_ctx = spec.pb_canvas.ctx;
	add_canvas_listener(spec.pb_canvas);
	spec.pb_ctx.font = "10px sans-serif";
	spec.pb_ctx.textBaseline = "middle";
	spec.pb_ctx.textAlign = "left";
	spec.pb_ctx.x1 = spec.pb_ctx.x2 = null;
	spec.pb_ctx.seq = 0;

   spec.af_left = 50;
   spec.af_margins = spec.af_left * 2;
   spec.af_canvas = create_canvas('id-spectrum-af-canvas', wf_fft_size, spec.height_spectrum_canvas, waterfall_width - spec.af_margins, spec.height_spectrum_canvas);
	w3_el('id-spectrum-container').appendChild(spec.af_canvas);
	spec.af_canvas.style.left = px(spec.af_left);
	spec.af_canvas.style.position = 'absolute';
	spec.af_ctx = spec.af_canvas.ctx;
	add_canvas_listener(spec.af_canvas);

	spec.dB = w3_el('id-spectrum-dB');
	spec.dB.style.height = px(spec.height_spectrum_canvas);
	spec.dB.style.width = px(waterfall_width);
	spec.dB.innerHTML = '<span id="id-spectrum-dB-ttip" class="class-spectrum-dB-tooltip class-tooltip-text"></span>';
	add_canvas_listener(spec.dB);

	w3_do_when_rendered('id-spectrum-dB-ttip',
	   function(el) {
	      spec.dB_ttip = el;
	   }
	);
   // REMINDER: w3_do_when_rendered() returns immediately
	
	// done at document level because mouse_freq_add() updates owrx.last_page[XY] as a side-effect
	document.addEventListener("mousemove", mouse_freq_add, false);
}

function add_canvas_listener(obj)
{
	obj.addEventListener("mouseover", canvas_mouseover, false);
	obj.addEventListener("mouseout", canvas_mouseout, false);
	obj.addEventListener("mousedown", canvas_mousedown, false);
	obj.addEventListener("mousemove", canvas_mousemove, false);
	obj.addEventListener("mouseup", canvas_mouseup, false);
	obj.addEventListener("contextmenu", canvas_contextmenu, false);
	obj.addEventListener("wheel", canvas_mousewheel_ev, false);

	if (kiwi_isMobile()) {
		obj.addEventListener('touchstart', canvas_touchStart, false);
		obj.addEventListener('touchmove', canvas_touchMove, false);
		obj.addEventListener('touchend', canvas_touchEnd, false);
	}
}

function remove_canvas_listener(obj)
{
	obj.removeEventListener("mouseover", canvas_mouseover, false);
	obj.removeEventListener("mouseout", canvas_mouseout, false);
	obj.removeEventListener("mousedown", canvas_mousedown, false);
	obj.removeEventListener("mousemove", canvas_mousemove, false);
	obj.removeEventListener("mouseup", canvas_mouseup, false);
	obj.removeEventListener("contextmenu", canvas_contextmenu, false);
	obj.removeEventListener("wheel", canvas_mousewheel_ev, false);

	if (kiwi_isMobile()) {
		obj.removeEventListener('touchstart', canvas_touchStart, false);
		obj.removeEventListener('touchmove', canvas_touchMove, false);
		obj.removeEventListener('touchend', canvas_touchEnd, false);
	}
}

function canvas_contextmenu(evt)
{
   //event_dump(evt, "contextmenu", 1);
	//console.log('## CMENU tgt='+ evt.target.id +' Ctgt='+ evt.currentTarget.id);
	
	if (evt.target.id == 'id-wf-canvas') {
		// TBD: popup menu with database lookup, etc.
	}
	
	// let ctrl-dx_click thru
	if (evt && evt.currentTarget && evt.currentTarget.id == 'id-dx-container') {
	   //console.log('canvas_contextmenu: synthetic ctrl-dx_click');
	   dx.ctrl_click = true;
	   if (evt.target.id != '')
	      w3_el(evt.target.id).click();
	}
	
	// must always cancel even so system context menu doesn't appear
	return cancelEvent(evt);
}

function canvas_mouseover(evt)
{
	if (!waterfall_setup_done) return;
}

function canvas_mouseout(evt)
{
	if (!waterfall_setup_done) return;
	//if (owrx.debug_drag) event_dump(evt, 'canvas_mouseout', 1);
	

   // must also end dragging when mouse leaves canvas while still down
	if (canvas_dragging) {
	   var ignore = evt.target.id.startsWith('id-spectrum-dB');
      if (owrx.debug_drag) canvas_log('C-MOUT '+ evt.target.id + (ignore? ' SPEC-IGNORE':''));
      if (!ignore)
         canvas_end_drag2();
   }
   
   mouse_freq_remove(evt);
}

function canvas_get_carfreq(relativeX, incl_PBO)
{
   return canvas_get_carfreq_offset(relativeX, incl_PBO) + center_freq;
}

function canvas_get_carfreq_offset(relativeX, incl_PBO)
{
   var freq;
   var norm = relativeX/waterfall_width;
   if (wf.audioFFT_active) {
      var cur = center_freq + demodulators[0].offset_frequency;
      var iq = ext_is_IQ_or_stereo_curmode();
      norm -= iq? 0.5 : 0.25;
      var incr = norm * audio_input_rate * (iq? 2 : 1);
      freq = cur + incr;
      //console.log('canvas_get_carfreq_offset(audioFFT) f='+ freq +' cur='+ cur +' incr='+ incr +' norm='+ norm);
   } else {
      var bin = x_bin + norm * bins_at_cur_zoom();
      freq = bin_to_freq(bin);
      //console.log('canvas_get_carfreq_offset f='+ freq +' bin='+ bin +' norm='+ norm);
   }
	var offset = incl_PBO? passband_offset() : 0;
	var f = Math.round(freq - offset);
	var cfo = f - (bandwidth/2);
	//console.log("CGCFO f="+f+" off="+offset+" cfo="+cfo);
	return cfo;
}

function canvas_get_dspfreq(relativeX)
{
	return canvas_get_carfreq(relativeX, false);
}

canvas_dragging = false;
canvas_drag_min_delta = 1;
canvas_mouse_down_or_touch = false;
canvas_ignore_mouse_event = false;

var mouse = { LEFT:0, MIDDLE:1, RIGHT:2, BUTTONS_L:1, BUTTONS_M:4, BUTTONS_R:2 };

function canvas_start_drag(evt, x, y)
{
   //event_dump(evt, "CSD", 1);
	if (owrx.debug_drag) canvas_log('CSD');
	
	// Distinguish ctrl-click right-button meta event from actual right-button on mouse (or touchpad two-finger tap).
	if (evt.button == mouse.RIGHT && !evt.ctrlKey) {
		canvas_ignore_mouse_event = true;
		if (owrx.debug_drag) console.log('CSD-Rclick IME=set-true');
		if (owrx.debug_drag) canvas_log('ordinary-RCM');
		if (owrx.debug_drag) canvas_log('ordinary-RCM ma='+ w3_menu_active());
      if (w3_menu_active())
         w3_menu_close('2');     // close menu if another DTS while menu open
      else
		   right_click_menu(x, y, 2);
		canvas_ignore_mouse_event = false;
		if (owrx.debug_drag) console.log('CSD-Rclick IME=set-false');
		return;
	}
	
	if (owrx.scale_canvas.mouse_out) {
	   //event_dump(evt, "CSD-scale");
	   //console.log('SHOULD NOT OCCUR');
	   return;
	}

	if (evt.shiftKey && evt.target.id == 'id-dx-container') {
		canvas_ignore_mouse_event = true;
		if (owrx.debug_drag) console.log('CSD-DX IME=set-true');
		dx_show_edit_panel(evt, -1);
	} else

	// lookup mouse pointer frequency in online resource appropriate to the frequency band
	if (evt.shiftKey && (evt.ctrlKey || evt.altKey)) {
		canvas_ignore_mouse_event = true;
		if (owrx.debug_drag) console.log('CSD-lookup IME=set-true');
		freq_database_lookup(canvas_get_dspfreq(x), evt.ctrlKey? owrx.rcm_lookup : owrx.rcm_cluster);
	} else
	
	// page scrolling via ctrl & alt-key click
	if (evt.ctrlKey) {
		canvas_ignore_mouse_event = true;
		if (owrx.debug_drag) console.log('CSD-pageScroll1 IME=set-true');
		page_scroll(-page_scroll_amount);
	} else
	
	if (evt.altKey) {
		canvas_ignore_mouse_event = true;
		if (owrx.debug_drag) console.log('CSD-pageScroll2 IME=set-true');
		page_scroll(page_scroll_amount);
	}
	
	owrx.drag_count = 0;
	owrx.mobile_firsttime = 1;
	canvas_mouse_down_or_touch = true;
	canvas_dragging = false;
	owrx.canvas.drag_last_x = owrx.canvas.drag_start_x = x;
	owrx.canvas.drag_last_y = owrx.canvas.drag_start_y = y;
   owrx.canvas.target = evt.target;
	spectrum_tooltip_update(owrx.EV_START, evt, x, y);
   
	// must always cancel even so system context menu doesn't appear
	return cancelEvent(evt);
}

function canvas_mousedown(evt)
{
	if (owrx.debug_drag) canvas_log('$C-MD');
   //event_dump(evt, "C-MD w3_menu_active="+ TF(w3_menu_active()), true);
	mouse_button_state(evt);
   if (w3_menu_active())
      w3_menu_close('0');     // close menu if single click while menu open
   else
	   canvas_start_drag(evt, evt.pageX, evt.pageY);
	evt.preventDefault();	// don't show text selection mouse pointer
}

function canvas_touchStart(evt)
{
   var touches = evt.targetTouches.length;
   var x = Math.round(evt.targetTouches[0].pageX);
   var y = Math.round(evt.targetTouches[0].pageY);
	if (owrx.debug_drag) canvas_log("$C-TS"+ touches +'-x'+ x +'-y'+ y);
   owrx.double_touch_start = false;
   
   if (w3_menu_active()) {
      w3_menu_close('4');     // close menu if single tap while menu open
   } else

   if (touches == 1) {
      // don't drag the WF on a spectrum display touch/drag -- just update the tooltip
      if (evt.target.id.startsWith('id-spectrum')) {
         owrx.canvas.drag_last_x = owrx.canvas.drag_start_x = x;
         owrx.canvas.drag_last_y = owrx.canvas.drag_start_y = y;
	      if (owrx.debug_drag) canvas_log('*SPEC* -x'+ x +'-y'+ y);
         owrx.canvas.target = evt.target;
         spectrum_tooltip_update(owrx.EV_START, evt, x, y);
      } else {
		   canvas_start_drag(evt, x, y);
		}
   /*
		owrx.touch_pending_start_drag = true;
		owrx.touch_pending_evt = evt;

      owrx.touch_hold_start = (new Date()).getTime();
      owrx.touch_hold_pressed = true;
      owrx.touch_hold_interval =
         setInterval(function() {
            if ((new Date()).getTime() - owrx.touch_hold_start > 750) {
               owrx.touch_hold_pressed = owrx.touch_pending_start_drag = false;
               kiwi_clearInterval(owrx.touch_hold_interval);
               alert(x +' '+ y +' '+ evt.target.id);
               canvas_start_drag(touch_pending_evt, x, y);
            }
         }, 200);
	*/
	} else
	
	if (touches >= 2) {     // pinch
		canvas_start_drag(evt, x, y);
	   //alert('canvas_touchStart='+ evt.targetTouches.length);
		canvas_ignore_mouse_event = true;
		if (owrx.debug_drag) console.log('CTS-doubleTouch IME=set-true');
		owrx.touches_first_startx = x;
		var deltaX = evt.touches[0].pageX - evt.touches[1].pageX;
		var deltaY = evt.touches[0].pageY - evt.touches[1].pageY;
      owrx.pinch_distance_first =
         Math.round(Math.hypot(deltaX, deltaY));
      owrx.pinch_distance_last = owrx.pinch_distance_first;
      
      // two fingers horizontally next to each other, i.e. differentiate from pinch zoom
      // 50 gives better reliability against premature touchends on Android than 30
      owrx.double_touch_swipe = (Math.abs(deltaY) < 50);
		owrx.double_touch_start = true;
	}

	touch_restore_tooltip_visibility(owrx.canvas);
	evt.preventDefault();	// don't show text selection mouse pointer
}

function canvas_drag(evt, idx, x, y, clientX, clientY)
{
	if (!waterfall_setup_done) return;
	if (spectrum_tooltip_update(owrx.EV_DRAG, evt, clientX, clientY)) return;
	owrx.drag_count = !isNumber(owrx.drag_count)? 0 : (owrx.drag_count + 1);
	
	if (owrx.scale_canvas.mouse_out) {
	   //event_dump(evt, "CD", 1);
      //console.log('@mouse_out SCD');
	   scale_canvas_drag(evt, x, y);    // continue scale dragging even though positioned on other canvases
	   return;
	}
	
	var sx = canvas_mouse_down_or_touch? owrx.canvas.drag_start_x : x;
   if (owrx.debug_drag) canvas_log('CD#'+ owrx.drag_count +' x'+ x +' y'+ y +' dx'+ Math.abs(x - sx) +
      ' CMDT'+ (canvas_mouse_down_or_touch? 1:0) +' IME'+ (canvas_ignore_mouse_event? 1:0) +' DG'+ (canvas_dragging? 1:0));

   // drag_count > 10 was required on Lenovo TB-7104F / Android 8.1.0 to differentiate double-touch from true drag.
   // I.e. an excessive number of touch events seem to be sent by browser for a single double-touch.
   // For desktop, sometimes clicking produces a single drag count. So prevent that from stopping the click.
   var isMobileOptNew = mobileOptNew();
   //if (!canvas_mouse_down_or_touch) console.log('!canvas_mouse_down_or_touch');
   
	if (canvas_mouse_down_or_touch && (!canvas_ignore_mouse_event || owrx.double_touch_start || isMobileOptNew)) {
	   var min_drag_cnt = kiwi_isMobile()? 10 : 1;
	   var drag_cnt_b = (owrx.drag_count > min_drag_cnt);
	   var drag_delta_b = (Math.abs(x - owrx.canvas.drag_start_x) > canvas_drag_min_delta);
		if (!canvas_dragging && drag_cnt_b && drag_delta_b) {
		   if (owrx.debug_drag) canvas_log('### dc='+ owrx.drag_count +' >? '+ min_drag_cnt +' && '+
		      Math.abs(x - owrx.canvas.drag_start_x) +' >? '+ canvas_drag_min_delta +' '+
		      TF(drag_cnt_b) + TF(drag_delta_b));
			canvas_dragging = true;
			wf_container.style.cursor = "move";
		}

		if (canvas_dragging) {
		   var update_x = true;
			var deltaX = owrx.canvas.drag_last_x - x;
			var deltaY = owrx.canvas.drag_last_y - y;

         if (owrx.double_touch_start) {
            if (owrx.double_touch_swipe) {
               // double-touch tune
               if (idx == 0 && evt.touches.length >= 2) {
                  var midX = Math.round(Math.abs((evt.touches[0].pageX + evt.touches[1].pageX) / 2));
                  demodulator_set_frequency(owrx.FSET_DTOUCH_DRAG, canvas_get_carfreq(midX, true));
               }
            } else {
               // pinch zoom
               var dist = Math.round(Math.hypot(deltaX, deltaY));
               var dist_delta = Math.abs(dist - owrx.pinch_distance_last);
               if (owrx.debug_drag) canvas_log(dist.toFixed(0) +' '+ dist_delta.toFixed(0));
               if (dist_delta > 25) {
                  if (owrx.debug_drag) canvas_log('*');
                  var pinch_in = (dist <= owrx.pinch_distance_last);
                  zoom_step(pinch_in? ext_zoom.OUT : ext_zoom.IN);
                  if (owrx.debug_drag) canvas_log(pinch_in? 'IN' : 'OUT');
                  owrx.pinch_distance_last = dist;
               }
            }
         } else {
            if (isMobileOptNew) {  
               if (1) {
                  // #mobile-ui waterfall: scroll tuning in WF margins instead of stopping at WF end
                  // For desktop see below.
                  // Same behavior as double-touch tune above.
                  // Different than mousewheel / trackpad scroll which jumps by 1/2 width.
		            var area = canvas_freq_upd(owrx.FSET_TOUCH_DRAG, x);
                  var deltaAllX = owrx.canvas.drag_all_x - x;
                  //console.log('WF area='+ area +' x='+ x +'|'+ owrx.canvas.drag_all_x +'|'+ owrx.canvas.drag_last_x +' dx='+ deltaAllX +'|'+ deltaX);
                  if (area != owrx.NOT_MARGIN) {
                     if ((area == owrx.LEFT_MARGIN && deltaAllX < 0) || (area == owrx.RIGHT_MARGIN && deltaAllX > 0)) {
                        //update_x = false;
                     } else {
                        // PB is in margin: also scroll waterfall
                        var dx = deltaX;
                        var dbins = norm_to_bins(deltaX / waterfall_width);
                        waterfall_pan_canvases(-dbins);
                        owrx.canvas.drag_all_x = x;
                        update_x = false;
                     }
                  }
               } else {
                  demodulator_set_frequency(owrx.FSET_TOUCH_DRAG, canvas_get_carfreq(x, true));
               }
            } else {
               // desktop: only drag the waterfall -- leave the tuning/passband alone
               var dbins = norm_to_bins(deltaX / waterfall_width);
               waterfall_pan_canvases(dbins);
            }
         }

			if (update_x) owrx.canvas.drag_last_x = x;
         owrx.canvas.drag_all_x = x;
			owrx.canvas.drag_last_y = y;
		}
	}

}

// Can't add on a keydown event (e.g. shift key) because there is no associated x,y
// There were other problems trying to do that, so just add when shifted mousing occurs.
function mouse_freq_add(evt)
{
   if (isNumber(evt.pageX)) owrx.last_pageX = evt.pageX;
   if (isNumber(evt.pageY)) owrx.last_pageY = evt.pageY;
   //canvas_log2(owrx.last_pageX +':'+ owrx.last_pageY);
   
   if (kiwi_isMobile() || wf.audioFFT_active) return;
   var ctx = canvas_annotation.ctx;

   if (evt.target == canvas_annotation && (any_modifier_key(evt) || owrx.show_cursor_freq)) {
      //console.log('CURSOR_FREQ annotation='+ (evt.target == canvas_annotation) +' mod_key='+ any_modifier_key(evt) +' show_cursor_freq='+ owrx.show_cursor_freq +' '+ typeof(owrx.show_cursor_freq));
      var cx = evt.offsetX;
      var cy = evt.offsetY + 16;
      var tw, th = 15;
      ctx.font = px(th) +' monospace';
      if (owrx.mouse_freq.vis)
         ctx.clearRect(owrx.mouse_freq.tx, owrx.mouse_freq.cy, owrx.mouse_freq.tw,th);
      var f_kHz = (canvas_get_dspfreq(cx) + kiwi.freq_offset_Hz)/1000;
      owrx.mouse_freq.text = format_frequency("{x}", f_kHz, 1, freq_field_prec(f_kHz));

      //w3_innerHTML('id-mouse-freq', owrx.mouse_freq.text);
      tw = Math.ceil(ctx.measureText(owrx.mouse_freq.text).width);
      if (tw & 1) tw++;    // make even so /2 later isn't fractional
      owrx.mouse_freq.tw = tw;
      owrx.mouse_freq.th = th;
      owrx.mouse_freq.tx = cx-tw/2;
      ctx.fillStyle = 'black';
      ctx.fillRect(cx-tw/2, cy, tw,th);
      ctx.fillStyle = 'white';
      ctx.fillText(owrx.mouse_freq.text, owrx.mouse_freq.tx, cy+th-2);
      owrx.mouse_freq.cx = cx;
      owrx.mouse_freq.cy = cy;
      owrx.mouse_freq.vis = true;
   } else {
      mouse_freq_remove(evt);
   }
}

function mouse_freq_remove(evt)
{
   if (owrx.mouse_freq.vis) {
      canvas_annotation.ctx.clearRect(owrx.mouse_freq.tx, owrx.mouse_freq.cy, owrx.mouse_freq.tw, owrx.mouse_freq.th);
      owrx.mouse_freq.vis = false;
   }

   if (evt && evt.type == 'keyup')
      shift_event(evt, 'keyup');
}

function canvas_mousemove(evt)
{
	//if (owrx.debug_drag) console.log("C-MM");
   //event_dump(evt, "C-MM");
	canvas_drag(evt, 0, evt.pageX, evt.pageY, evt.clientX, evt.clientY);
}

function canvas_touchMove(evt)
{
	if (evt.touches.length >= 1) {
	   owrx.touches_first_lastx = evt.touches[0].pageX;
	}
	
	for (var i=0; i < evt.touches.length; i++) {
		var x = Math.round(evt.touches[i].pageX);
		var y = Math.round(evt.touches[i].pageY);
	   if (owrx.debug_drag) canvas_log('C-TM-x'+ x +'-y'+ y);

   /*
      // any movement cancels touch hold
      if (owrx.touch_hold_pressed) {
         owrx.touch_hold_pressed = false;
         kiwi_clearInterval(owrx.touch_hold_interval);
      }

      if (owrx.touch_pending_start_drag) {
		   canvas_start_drag(evt, x, y);
         owrx.touch_pending_start_drag = false;
      }
   */

		canvas_drag(evt, i, x, y, x, y);
	}
	evt.preventDefault();
}

function canvas_end_drag2()
{
	if (owrx.debug_drag) canvas_log("C-ED2");
	wf_container.style.cursor = owrx.wf_cursor;
	canvas_mouse_down_or_touch = false;
	canvas_ignore_mouse_event = false;
	if (owrx.debug_drag) { console.log("C-ED2 IME=set-false"); }
	canvas_dragging = false;
}

function canvas_end_drag(evt, x)
{
	if (owrx.scale_canvas.mouse_out) {
	   //event_dump(evt, "CED");
      //console.log('@mouse_out CED');
	   owrx.scale_canvas.mouse_out = false;
	   scale_canvas_end_drag(evt, x, /* canvas_mouse_up */ true);
	}

	if (!waterfall_setup_done) return;
	//console.log("MUP "+this.id+" ign="+canvas_ignore_mouse_event);
	
	if (owrx.debug_drag) canvas_log('C-ED IME'+ (canvas_ignore_mouse_event? 1:0) +' CD'+ (canvas_dragging? 1:0));

   if (!canvas_dragging) {
      var tooltip_complete = spectrum_tooltip_update(owrx.EV_END);
      if (owrx.debug_drag) canvas_log('MUP-ttip'+ (tooltip_complete? 1:0));
      //event_dump(evt, "MUP");
      
      // Don't set freq if mouseup without mousedown due to move into canvas from elsewhere.
      // Also, don't set freq if mouseup if after a spectrum tooltip operation.
      if (owrx.debug_drag) canvas_log('CMDT'+ (canvas_mouse_down_or_touch? 1:0) +' TL'+ owrx.tuning_locked);
      if (canvas_mouse_down_or_touch && !canvas_ignore_mouse_event && !tooltip_complete) {
         if (owrx.tuning_locked) canvas_log('TLOCK='+ owrx.tuning_locked);
         if (owrx.tuning_locked) {
            var el = w3_el('id-tuning-lock-container');
            el.style.opacity = 0.8;
            w3_show(el);
            var el2 = w3_el('id-tuning-lock');
            el2.style.marginTop = px(w3_center_in_window(el2, 'TL'));
            setTimeout(function() {
               el.style.opacity = 0;      // CSS is setup so opacity fades
               setTimeout(function() { w3_hide(el); }, 500);
            }, 300);
         } else
         if (!owrx.ignore_freq_tune_onmouseup) {
            // single-click in canvas area
            if (owrx.debug_drag) canvas_log('*click*');

	         // select waterfall on nearest appropriate boundary (1, 5 or 9/10 kHz depending on band & mode)
	         if ((evt.shiftKey && !(evt.ctrlKey || evt.altKey)) || owrx.wf_snap) {
               var fold = canvas_get_dspfreq(x);
               var b = find_band(fold);
               //if (b) console.log(b)
               var rv = freq_step_amount(b);
               var step_Hz = rv.step_Hz;
               var trunc = fold / step_Hz;
               var fnew = Math.round(trunc) * step_Hz;
               //console.log('SFT-CLICK '+cur_mode+' fold='+fold+' step='+step_Hz+' trunc='+trunc+' fnew='+fnew +' '+ rv.s);
               freqmode_set_dsp_kHz(fnew/1000, null);
            } else {
               demodulator_set_frequency(owrx.FSET_MOUSE_CLICK, canvas_get_carfreq(x, true));
            }
         }
      }
   } else {
      canvas_end_drag2();
   }
   owrx.ignore_freq_tune_onmouseup = false;
	
	canvas_mouse_down_or_touch = false;
	canvas_ignore_mouse_event = false;
	if (owrx.debug_drag) console.log('C-ED IME=set-false');
}

function canvas_mouseup(evt)
{
	if (owrx.debug_drag) console.log("C-MU");
	if (owrx.debug_drag) canvas_log("C-MU");
   //event_dump(evt, "C-MU");
	mouse_button_state(evt);
	canvas_end_drag(evt, evt.pageX);
	cancelEvent(evt);
}

function canvas_touchEnd(evt)
{
	var x = owrx.canvas.drag_last_x, y = owrx.canvas.drag_last_y;
	if (owrx.debug_drag)
	   canvas_log('C-TE-x'+ x +'-DTS'+ (owrx.double_touch_start? 1:0) +'-DTSw'+ (owrx.double_touch_swipe? 1:0) +'-DRAG'+ (canvas_dragging? 1:0));
   var was_dragging = canvas_dragging;

   try {
	   canvas_end_drag(evt, x);
	} catch(ex) {
	   canvas_catch_log(ex);
	}
/*
   owrx.touch_hold_pressed = false;
   kiwi_clearInterval(owrx.touch_hold_interval);
*/
	
	if (owrx.double_touch_start) {
	   if (owrx.debug_drag) canvas_log('DTS-D='+ TF(was_dragging));

      // last freq of a double-touch drag is saved in freq mem
      demodulator_set_frequency(owrx.FSET_TOUCH_DRAG_END, freq_car_Hz);

	   if (!was_dragging) {
         // ensure menu on narrow screen devices is visible to prevent off-screen placement
         if (mobileOpt())
            x = 10;
   
         if (owrx.debug_drag) canvas_log('*-RCM');
         if (owrx.debug_drag) console.log('*-RCM');
         if (w3_menu_active())
            w3_menu_close('3');     // close menu if another DTS while menu open
         else
            right_click_menu(x, y, 3);
      } else {
         //var pinch_in = (owrx.pinch_distance_first >= owrx.pinch_distance_last)? 1:0;
         //if (owrx.debug_drag) canvas_log('pinch-'+ owrx.pinch_distance_first +'-'+ owrx.pinch_distance_last + (pinch_in? '-IN' : '-OUT'));
      }
      
      canvas_ignore_mouse_event = false;
      if (owrx.debug_drag) console.log('CTE-doubleTouch IME=set-false');
		owrx.double_touch_start = false;
	}
	
   try {
	   touch_hide_tooltip(evt, owrx.canvas);
	} catch(ex) {
	   canvas_catch_log(ex);
	}
	
	evt.preventDefault();
}


////////////////////////////////
// #wheel
////////////////////////////////

var canvas_mousewheel_rlimit_zoom = kiwi_rateLimit(canvas_mousewheel_cb, /* msec */ 170);
var canvas_mousewheel_rlimit_tune;

function canvas_mousewheel_ev(evt)
{
   var flip_sense = (owrx.buttons == mouse.BUTTONS_M)? 1:0;
   if (owrx.wheel_tunes ^ flip_sense) {
      canvas_mousewheel_rlimit_tune(evt);
      //console.log('t');
   } else {
      canvas_mousewheel_rlimit_zoom(evt);
      //console.log('z');
   }
	evt.preventDefault();	
}


function canvas_mousewheel_cb(evt)
{
	if (!waterfall_setup_done) return;
	/*
	if (cur_mode == 'lsb') { console.log('tp'); owrx.jks_tp = kiwi_serializeEvent(evt); }
	if (cur_mode == 'usb') { console.log('mw'); owrx.jks_mw = kiwi_serializeEvent(evt); }
	if (cur_mode == 'am') console.log(owrx.jks_tp.length +' '+ owrx.jks_mw.length +' '+ ((owrx.jks_tp != owrx.jks_mw)? 'DIFF!':'same'));
	if (cur_mode == 'am' && owrx.jks_tp != owrx.jks_mw) {
	   console.log('diff='+ owrx.jks_tp.indexOf(owrx.jks_mw));
	   console.log('tp');
	   console.log(owrx.jks_tp);
	   console.log('mw');
	   console.log(owrx.jks_mw);
	}
	*/
	//console.log(evt);
	//event_dump(evt, 'canvas_mousewheel_cb');

   var x = evt.deltaX;
   var y = evt.deltaY;
   //var fwd_bak = (x < 0 || (x == 0 && y < 0));
   var fwd_bak = ((x < 0 && y <= 0) || (x >= 0 && y < 0)) ^ owrx.wheel_dir;
   var key_mod = shortcut_key_mod(evt);
   
   // wheel makes passband adjustment when cursor in frequency scale
   if (evt.target && evt.target.id && (evt.target.id == 'id-scale-canvas' || evt.target.id.startsWith('id-pb-'))) {
      //event_dump(evt, 'canvas_mousewheel_cb', true);
      var edge = owrx.PB_LO_EDGE;
      if (key_mod == shortcut.NO_MODIFIER) edge = owrx.PB_NO_EDGE; else
      if (key_mod == shortcut.SHIFT) edge = owrx.PB_HI_EDGE;
      passband_increment(fwd_bak, edge);
   } else {
      // Wheel makes tuning or zoom adjustment depending on a setting in the right-click-menu.
      // Holding down wheel (middle) button reverses sense of tune/zoom setting.
      var flip_sense = (owrx.buttons == mouse.BUTTONS_M)? 1:0;
      //console.log('canvas_mousewheel_cb flip_sense='+ flip_sense);
      if (owrx.wheel_tunes ^ flip_sense) {
         //console.log('wheel fwd_bak='+ fwd_bak +' key_mod='+ key_mod +' buttons='+ evt.buttons);
         // step
         // 0: -large, 1: -med, 2: -small || 3: +small, 4: +med, 5: +large
         // CTL_ALT    SHIFT    NO_MOD       NO_MOD     SHIFT    CTL_ALT
         //
         // No key modifier (NO_MOD) will drop into the variable tune rate code below
         // which tunes by the slowest rate unless the wheel is spun quickly.
         if (key_mod) {
            if (fwd_bak) {    // wheel fwd tunes higher
               if (key_mod != shortcut.SHIFT_PLUS_CTL_ALT) {
                  freqstep_cb(null, owrx.wf_snap? (5 - key_mod) : (3 + key_mod));
               } else {
                  dx_label_step(+1);
               }
            } else {
               if (key_mod != shortcut.SHIFT_PLUS_CTL_ALT) {
                  freqstep_cb(null, owrx.wf_snap? key_mod : (2 - key_mod));
               } else {
                  dx_label_step(-1);
               }
            }
         } else {
            var ay = Math.abs(y);
            var fast = (ay >= owrx.wheel_fast);
            if (fast && !owrx.lock_fast || owrx.lock_fast) {
            
               // For MacOS trackpad unlock delay has to be long enough to capture
               // ending scroll inertia that would otherwise produce trailing "slow" events.
               kiwi_clearTimeout(owrx.lock_fast_timeo);
               owrx.lock_fast_timeo = setTimeout(
                  function() {
                     owrx.lock_fast = false;
                     mkenvelopes(g_range);
                     //console.log('U');
                  }, owrx.wheel_unlock);
               owrx.lock_fast = true;
            }
            if (owrx.lock_fast) { fast = true; }
            //console.log(owrx.lock_fast? 'L' : (fast? 'F':'S') + y);
               
            if (fwd_bak)
               freqstep_cb(null, owrx.wf_snap? 5 : (fast? 5:3));
            else
               freqstep_cb(null, owrx.wf_snap? 0 : (fast? 0:2));
         }
      } else {
         // evt.ctrlKey is:
         //    true for trackpad pinch (zoom-to-passband),
         //    false for two-finger swipe across or up/down.
         //    false for actual USB-mouse mousewheel (zoom-to-cursor)
         // x != 0 is two-finger swipe L/R
         var zoom_to_pb_pinch = evt.ctrlKey;
         var two_finger_swipe_LR = (x != 0);
         var to_passband = ((zoom_to_pb_pinch || two_finger_swipe_LR) && !flip_sense);
         //console_nv('canvas_mousewheel_cb', {zoom_to_pb_pinch}, {two_finger_swipe_LR}, {flip_sense}, {to_passband});
         zoom_step(fwd_bak? ext_zoom.IN : ext_zoom.OUT, to_passband? ext_zoom.TO_PASSBAND : evt.pageX);
         
         // need to keep canvas drag click mouseup code from changing freq
         if (!to_passband) owrx.ignore_freq_tune_onmouseup = true;
         //owrx.trace_freq_update=1;
      }
   
      /*
      // scaling value is a scrolling sensitivity compromise between wheel mice and
      // laptop trackpads (and also Apple trackpad mice)
      zoom_level_f += evt.deltaY * -0.05;
      zoom_level_f = Math.max(Math.min(zoom_level_f, zoom_levels_max), 0);
      //console.log('mousewheel '+ zoom_level_f.toFixed(1));
      //w3_innerHTML('id-owner-info', 'mousewheel '+ zoom_level_f.toFixed(2) +' '+ evt.deltaY);
      zoom_step(ext_zoom.WHEEL, evt.pageX);
      */
   }
}

function canvas_mouse_wheel_set(set)
{
   //console.log('canvas_mouse_wheel_set wheel_tunes(cur)='+ TF(owrx.wheel_tunes) +' set='+ set);
   owrx.wheel_tunes = isUndefined(set)? (owrx.wheel_tunes ^ 1) : (isNull(set)? 0 : (+set));
   //console.log('canvas_mouse_wheel_set wheel_tunes(new)='+ TF(owrx.wheel_tunes));
   kiwi_storeWrite('wheel_tunes', owrx.wheel_tunes);
}

function wheel_dev_cb(path, idx, first)
{
   idx = +idx;
   if (first) idx = +kiwi_storeRead('wheel_dev', idx);
   owrx.wheel_dev = idx;
   console.log('wheel_dev_cb: idx='+ idx +' first='+ first);

   if (idx == owrx.WHEEL_CUSTOM) {
      owrx.wheel_poll = kiwi_storeRead('wheel_poll', owrx.wheel_poll_i[idx]);
      owrx.wheel_fast = kiwi_storeRead('wheel_fast', owrx.wheel_fast_i[idx]);
      owrx.wheel_unlock = kiwi_storeRead('wheel_unlock', owrx.wheel_unlock_i[idx]);
   } else {
      owrx.wheel_poll = owrx.wheel_poll_i[idx];
      owrx.wheel_fast = owrx.wheel_fast_i[idx];
      owrx.wheel_unlock = owrx.wheel_unlock_i[idx];
   }
   w3_set_value('id-wheel-dev', idx);     // for benefit of kiwi_storeRead() above
   kiwi_storeWrite('wheel_dev', idx);
   w3_set_value('id-wheel-poll', owrx.wheel_poll);
   w3_set_value('id-wheel-fast', owrx.wheel_fast);
   w3_set_value('id-wheel-unlock', owrx.wheel_unlock);
   canvas_mousewheel_rlimit_tune = kiwi_rateLimit(canvas_mousewheel_cb, /* msec */ owrx.wheel_poll);
   console.log('wheel_dev_cb: wheel_poll='+ owrx.wheel_poll +' wheel_fast='+ owrx.wheel_fast +' wheel_unlock='+ owrx.wheel_unlock);
}

function wheel_dir_cb(path, idx, first)
{
   idx = +idx;
   if (first) idx = +kiwi_storeRead('wheel_dir', idx);
   owrx.wheel_dir = idx;
   console.log('wheel_dir_cb: idx='+ idx +' first='+ first);

   w3_set_value('id-wheel-dir', idx);     // for benefit of kiwi_storeRead() above
   kiwi_storeWrite('wheel_dir', idx);
}

function wheel_param_cb(path, val, first, a_cb)
{
   val = +val;
   var which = +(a_cb[1]);
   console.log('wheel_param_cb: which='+ which +' first='+ first +' val='+ val);
   switch (which) {
      case 0: owrx.wheel_poll = val; break;
      case 1: owrx.wheel_fast = val; break;
      case 2: owrx.wheel_unlock = val; break;
   }
   if (owrx.wheel_dev == owrx.WHEEL_CUSTOM) {
      switch (which) {
         case 0: kiwi_storeWrite('wheel_poll', val); break;
         case 1: kiwi_storeWrite('wheel_fast', val); break;
         case 2: kiwi_storeWrite('wheel_unlock', val); break;
      }
   }
   w3_set_value(path, val);
   if (which == 0)
      canvas_mousewheel_rlimit_tune = kiwi_rateLimit(canvas_mousewheel_cb, /* msec */ owrx.wheel_poll);
   console.log('wheel_param_cb: wheel_poll='+ owrx.wheel_poll +' wheel_fast='+ owrx.wheel_fast +' wheel_unlock='+ owrx.wheel_unlock);
}

function wheel_help()
{
   var s = 
      w3_text('w3-medium w3-bold w3-text-aqua', 'Mouse wheel / trackpad help') +
      w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
         w3_div('w3-margin-R-8 w3-margin-bottom',
            'The mouse wheel (or trackpad emulation thereof) can control ' +
            'frequency tuning or waterfall zooming as set by an option in ' +
            'the right-click menu. Depressing the mouse wheel button while ' +
            'moving the wheel will temporarily select the opposite action, ' +
            'e.g. zooming if the wheel is set for tuning. ' +
            '<br><br>' +

            'Frequency tuning step size: When wheel tuning the same ' +
            'modifier keys work as with the tuning shortcut keys ' +
            '(i.e. <x1>j</x1>, <x1>i</x1>, left-arrow, right-arrow). ' +
            'Namely, of the three tuning rates as shown by hovering over the ' +
            '<x1>- - - + + +</x1> buttons the slowest rate (typically 10 Hz) ' +
            'is selected by using no modifier key. ' +
            'The middle rate with the shift key and the fastest rate using the ' +
            'control or alt/option key.' +
            '<br><br>' +
        
            'In lieu of the modifier keys there is also a simplistic two-speed detection ' +
            'of the wheel velocity. ' +
            'If the wheel is moved quickly enough it "locks" into moving at the fastest rate. ' +
            'It continues at that rate, even if the wheel is moved more slowly, until ' +
            'some time after the wheel stops. The passband color turns orange until the ' +
            'fast rate unlocks. ' +
            'If "snap to nearest" from the right-click menu is enabled then the ' +
            'fastest tuning rate is always used. ' +
            '<br><br>' +

            'The Javascript code running in the browser cannot detect ' +
            'the characteristics of the mouse or trackpad being used. ' +
            'Things like DPI and sensitivity. From the <x1>device preset</x1> ' +
            'menu you can choose whether a wheel mouse or trackpad is being used. ' +
            'The <x1>custom</x1> entry allows the specific ' +
            'parameters to be tuned for your mouse or trackpad. ' +
            'These settings are saved in browser storage. ' +
            '<br>' +
            
            '<ul><li><x1>Poll (msec)</x1> How often device is polled for input. ' +
            'Determines tracking speed. </li>' +
            '<li><x1>Fast threshold</x1> Velocity at which switch to fast tuning rate occurs.</li>' +
            '<li><x1>Unlock (msec)</x1> How long after movement stops before fast tuning unlocks.</li></ul>'
         )
      );
   confirmation_show_content(s, 600, 410);
   w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
}


////////////////////////////////
// #right click menu
////////////////////////////////

function right_click_menu_init()
{
   var i = 0;
   var m = [];
   m.push('DX database1'); owrx.rcm_db1 = i; i++;
   m.push('DX database2'); owrx.rcm_db2 = i; i++;
   m.push('open DX label panel'); owrx.rcm_open = i; i++;
   m.push('edit last selected DX label'); owrx.rcm_edit = i; i++;
   m.push('DX label filter'); owrx.rcm_filter = i; i++;
   m.push('<hr>'); i++;

   m.push('database lookup'); owrx.rcm_lookup = i; i++;
   //m.push('Utility database lookup'); owrx.rcm_util = i; i++;
   m.push('DX Cluster lookup'); owrx.rcm_cluster = i; i++;
   m.push('<hr>'); i++;

   m.push('mouse wheel zooms'); owrx.rcm_wheel = i; i++;

   m.push('snap to nearest'); owrx.rcm_snap = i; i++;

	owrx.show_cursor_freq = +kiwi_storeInit('wf_showCurF', 0);
   m.push((owrx.show_cursor_freq? 'hide' : 'show') +' cursor frequency'); owrx.rcm_cur_freq = i; i++;

	owrx.freq_dsp_1Hz = +kiwi_storeInit('freq_dsp_1Hz', 0);
   m.push((owrx.freq_dsp_1Hz? '10' : '1') +' Hz frequency display'); owrx.rcm_freq_dsp = i; i++;

   m.push('🔒 lock tuning'); owrx.rcm_lock = i; i++;
   m.push('restore passband'); owrx.rcm_pb = i; i++;
   m.push('save waterfall as JPG'); owrx.rcm_save = i; i++;
   m.push('<hr>'); i++;

   // "pointer-events:none" required to make document.elementFromPoint() not incorrectly match on <i>
   m.push('<i style="pointer-events:none">cal ADC clock (admin)</i>'); owrx.rcm_cal = i; i++;
   m.push('<i style="pointer-events:none">set freq offset (admin)</i>'); owrx.rcm_foff = i; i++;
   m.push('<i style="pointer-events:none">save noise defaults (admin)</i>'); owrx.rcm_noise = i; i++;
   owrx.right_click_menu_content = m;
   
   w3_menu('id-right-click-menu', 'right_click_menu_cb');

   // for tuning lock
   var s =
      w3_div('id-tuning-lock-container class-overlay-container w3-hide',
         w3_div('id-tuning-lock', w3_icon('', 'fa-lock', 192) + '<br>Tuning locked')
      );
   w3_create_appendElement('id-w3-misc-container', 'div', s);
}

function right_click_menu(x, y, which)
{
   if (owrx.debug_drag) canvas_log('RCM'+ which);
   
   // update menu items whose underlying value might have changed
   var db1 = (dx.db + 1) % dx.DB_N;
   var db2 = (dx.db + 2) % dx.DB_N;
   owrx.right_click_menu_content[owrx.rcm_db1] = 'DX '+ dx.db_short_s[db1] +' database';
   owrx.right_click_menu_content[owrx.rcm_db2] = 'DX '+ dx.db_short_s[db2] +' database';

   var kHz = freq_displayed_Hz/1000;
   var b = band_info();
   var db;

   if (kHz >= b.NDB_lo && kHz < b.NDB_hi) db = 'NDB';
   else
   if (kHz < b.LW_lo) db = 'VLF/LF';
   else
   if (kHz < b.MW_hi) db = 'LW/MW';
   else
      db = 'SWBC';

   owrx.right_click_menu_content[owrx.rcm_lookup] = db + ' database lookup';
   owrx.right_click_menu_content[owrx.rcm_wheel] = owrx.wheel_tunes? 'mouse wheel zooms' : 'mouse wheel tunes';
   owrx.right_click_menu_content[owrx.rcm_snap] = owrx.wf_snap? 'no snap' : 'snap to nearest';
   owrx.right_click_menu_content[owrx.rcm_freq_dsp] = (owrx.freq_dsp_1Hz? '10' : '1') +' Hz frequency display';
   owrx.right_click_menu_content[owrx.rcm_lock] = (owrx.tuning_locked? '🔓 unlock' : '🔒 lock') +' tuning';
   
   w3_menu_items('id-right-click-menu', owrx.right_click_menu_content);
   w3_menu_popup('id-right-click-menu',
      function(evt, first) {     // close_func(), return true to close menu
         var close = ((evt.type == 'keyup' && evt.key == 'Escape') ||
            (evt.type == 'click' && evt.button != mouse.RIGHT) ||
            (evt.type == 'touchend' && !first && !w3_contains(evt.target, 'w3-menu')));
         //canvas_log(close +' first='+ first +' '+ evt.type +' '+ (w3_contains(evt.target, 'w3-menu')? 'W3-MENU':''));
         //event_dump(evt, 'right-click-menu close_func='+ close, true);
         return close;
      },
      x, y);
}

function right_click_menu_cb(idx, x, cbp)
{
   idx = +idx;
   //if (owrx.debug_drag) canvas_log('RCM='+ cbp);
   //console.log('right_click_menu_cb idx='+ idx +' x='+ x +' f='+ canvas_get_dspfreq(x)/1e3);
   
   switch (idx) {
   
   case owrx.rcm_db2:
      dx.db = (dx.db + 1) % dx.DB_N;
      // fall through...
      
   case owrx.rcm_db1:
      dx.db = (dx.db + 1) % dx.DB_N;
      dx_database_cb('', dx.db);
      break;

   case owrx.rcm_lookup:   // database lookups
   case owrx.rcm_cluster:
		freq_database_lookup(canvas_get_dspfreq(x), idx);
      break;
   
   case owrx.rcm_wheel:  // mouse wheel zooms or tunes
      canvas_mouse_wheel_set();
      break;

   case owrx.rcm_snap:  // snap to nearest
      wf_snap();
      break;

   case owrx.rcm_cur_freq:  // cursor freq
      owrx.show_cursor_freq ^= 1;
      kiwi_storeWrite('wf_showCurF', owrx.show_cursor_freq);
      owrx.right_click_menu_content[owrx.rcm_cur_freq] = (owrx.show_cursor_freq? 'hide' : 'show') +' cursor frequency';
      break;
      
   case owrx.rcm_freq_dsp:  // freq display resolution
      owrx.freq_dsp_1Hz ^= 1;
      kiwi_storeWrite('freq_dsp_1Hz', owrx.freq_dsp_1Hz);
      freqset_update_ui(owrx.FSET_NOP);
      owrx.right_click_menu_content[owrx.rcm_freq_dsp] = (owrx.freq_dsp_1Hz? '10' : '1') +' Hz frequency display';
      break;
      
   case owrx.rcm_lock:  // tuning lock
      owrx.tuning_locked ^= 1;
      owrx.right_click_menu_content[owrx.rcm_lock] = (owrx.tuning_locked? '🔓 unlock' : '🔒 lock') +' tuning';
      break;
      
   case owrx.rcm_pb:  // restore passband
      restore_passband(cur_mode);
      demodulator_analog_replace(cur_mode);
      //if (owrx.debug_drag) canvas_log('RCM-RPB');
      break;
      
   case owrx.rcm_save:  // save waterfall image
      export_waterfall();
      break;
   
   case owrx.rcm_open:  // open/edit DX label panel using current freq/mode or last selected dx label info
   case owrx.rcm_edit:
      var gid = -1;
      var edit_gid = (idx == owrx.rcm_edit && dx.db == dx.DB_STORED && isDefined(owrx.dx_click_gid_last_stored));
      if (edit_gid)
         gid = owrx.dx_click_gid_last_stored;
      console.log('RCM rcm_'+ (edit_gid? 'edit' : 'open') +' gid='+ gid +' db='+ dx.db);
      dx_show_edit_panel(null, gid);
	   dx_schedule_update();
      break;
   
   case owrx.rcm_filter:  // open dx label filter
      dx_filter();
      break;
   
   case owrx.rcm_cal:  // cal ADC clock
      admin_pwd_query(function() {
         var r1k_kHz = Math.round(freq_displayed_Hz / 1e3);     // 1kHz windows on 1 kHz boundaries
         var r1k_Hz = r1k_kHz * 1e3;
         var clk_diff = r1k_Hz - freq_displayed_Hz;
         var clk_adj = Math.round(clk_diff * ext_adc_clock_Hz() / r1k_Hz);   // clock adjustment normalized to ADC clock frequency
         console.log('cal ADC clock: dsp='+ freq_displayed_Hz +' car='+ freq_car_Hz +' r1k_kHz='+ r1k_kHz +' clk_diff='+ clk_diff +' clk_adj='+ clk_adj);
         var new_adj = cfg.clk_adj + clk_adj;
         var ppm = new_adj * 1e6 / ext_adc_clock_Hz();
         console.log('cal ADC clock: prev_adj='+ cfg.clk_adj +' new_adj='+ new_adj +' ppm='+ ppm.toFixed(1));
         cal_adc_dialog(new_adj, clk_diff, r1k_kHz, ppm);
      });
      break;
   
   case owrx.rcm_foff:  // set freq offset
      admin_pwd_query(function() {
         set_freq_offset_dialog();
      });
      break;
   
   case owrx.rcm_noise:
      admin_pwd_query(function() {
         w3_call('noise_blank_save_defaults');
         w3_call('noise_filter_save_defaults');
      });
      break;
   
   case -1:
      break;

   default:
      break;
   }
}


////////////////////////////////
// #database lookup
////////////////////////////////

function freq_database_lookup(Hz, utility)
{
   var kHz = Hz/1000;
   var kHz_r10 = Math.round(Hz/10)/100;
   var kHz_r1k = Math.round(Hz/1000);
   //console.log('### Hz='+ Hz +' kHz='+ kHz +' kHz_r10='+ kHz_r10 +' kHz_r1k='+ kHz_r1k);
   var f;
   var url = "https://";

   var b = band_info();

   /*
   // expired cert!
   if (utility == owrx.rcm_util) {
      f = Math.floor(Hz/100) / 10000;	// round down to nearest 100 Hz, and express in MHz, for GlobalTuners
      url += "qrg.globaltuners.com/?q="+f.toFixed(4);
   } else
   */
   if (utility == owrx.rcm_lookup) {
      if (kHz >= b.NDB_lo && kHz < b.NDB_hi) {
         f = kHz_r1k.toFixed(0);		// 1kHz windows on 1 kHz boundaries for NDBs
         //url += "www.classaxe.com/dx/ndb/rww/signal_list/?mode=signal_list&submode=&targetID=&sort_by=khz&limit=-1&offset=0&show=list&"+
         //"type_DGPS=1&type_NAVTEX=1&type_NDB=1&filter_id=&filter_khz_1="+ f +"&filter_khz_2="+ f +
         //"&filter_channels=&filter_sp=&filter_sp_itu_clause=AND&filter_itu=&filter_continent=&filter_dx_gsq=&region=&"+
         //"filter_listener=&filter_heard_in=&filter_date_1=&filter_date_2=&offsets=&sort_by_column=khz";
         url += 'rxx.classaxe.com/en/rww/signals?types=DGPS,NAVTEX,NDB&khz='+ f +'&limit=-1';
      } else
   
      if (kHz < b.LW_lo) {		// VLF/LF
         f = Math.round(Hz/100) / 10;	// 100 Hz windows on 100 Hz boundaries
         console.log('kHz='+ kHz +' f='+ f);
         url += "www.mwlist.org/vlf.php?kHz="+f.toFixed(1);
      } else
   
      if (kHz < b.MW_hi) {		// LW/MW
         f = Math.round(kHz_r1k/b._9_10) * b._9_10;
         console.log('MW kHz='+ kHz +' f='+ f);
         var mwlist_area = [ 0, 1, 3, 2 ];	// mwlist_area = 1:ITU1(E) 2:ITU3(AP) 3:ITU2-SA(NA) 4:SA
         url += "www.mwlist.org/mwlist_quick_and_easy.php?area="+ mwlist_area[b.ITU_region] +"&kHz="+f.toFixed(0);
      } else
   
      {
         // HF: short-wave.info is only >= 2 MHz
         f = Math.round(kHz_r1k/5) * 5;	// 5kHz windows on 5 kHz boundaries -- intended for SWBC
         url += "www.short-wave.info/index.php?freq="+ f.toFixed(0) +"&timbus=NOW&ip="+ client_public_ip +"&porm=4";
      }
   } else
   if (utility == owrx.rcm_cluster) {
      f = Math.floor(Hz) / 1000;	// kHz for ve3sun dx cluster lookup
      url = 'http://ve3sun.com/KiwiSDR/DX.php?Search='+f.toFixed(1);
   }
   
   console.log('LOOKUP '+ kHz +' -> '+ f +' '+ url);
   var win = kiwi_open_or_reload_page({ url:url, tab:1 });
   if (win) win.focus();
}


////////////////////////////////
// #waterfall export
////////////////////////////////

// this also works if displaying audio FFT instead of waterfall
function export_waterfall() {

   // To get the proper scale width/alignment don't use the scale width itself because it includes
   // the width of the waterfall scrollbar since it extends to the far right of the screen (1440 for our laptop).
   // Use the actual width of the waterfall (minus the scrollbar) instead (1425 for our laptop).

   var fscale = w3_el('id-scale-canvas');
   //var fscaleW = Math.round(fscale.width);
   var wfW = waterfall_width;
   var fscaleW = wfW;
   var fscaleH = Math.round(fscale.height);
   var legendH = 50;
   var h = legendH;

   // include spectrum if selected and visible
   var specC = w3_el('id-spectrum-canvas');
   var specH = (specC && spec.source == spec.RF)? specC.height : 0;

   var e_canvas = document.createElement("canvas");
   var e_ctx = e_canvas.getContext("2d");
   var e_canW = e_canvas.width = wf_fft_size;
   var e_canH = e_canvas.height = legendH + fscaleH + specH + (old_canvases.length + wf_canvases.length) * wf_canvas_default_height;
   //w3_console.log({mult:wf.scroll_multiple, Ncan:wf_canvases.length, Nold:old_canvases.length, e_canH}, 'export_waterfall');
   e_ctx.fillStyle="black";
   e_ctx.fillRect(0, 0, e_canW, e_canH);

   if (specH) {
      e_ctx.drawImage(specC, 0,h);
      h += specH;
   }
   
   e_ctx.fillStyle="gray";
   e_ctx.fillRect(0, h, e_canW, fscaleH);
   //w3_console.log({fscaleW, e_canW, ratio:fscaleW/e_canW}, 'export_waterfall');
   e_ctx.drawImage(fscale, 0,0, fscaleW,fscaleH, 0,h, e_canW, fscaleH);
   h += fscaleH;

   wf_canvases.forEach(function(wf_c, i) {
      var wfH = wf_c.height;
      var wfY = 0;
      if (i == 0) {
         var top = unpx(wf_c.style.top);
         wfH = wfH + top;
         wfY = -top;
         //w3_console.log({top, wfH, wfY}, 'export_waterfall');
      }
      e_ctx.drawImage(wf_c, 0,wfY, e_canW,wfH, 0,h, e_canW,wfH);
      //e_ctx.fillStyle="red";
      //e_ctx.fillRect(0, h, e_canW, 2);
      h += wfH;
   });

   if (old_canvases.length > 1) {
      for (i = 1; i < old_canvases.length; i++) {
         e_ctx.drawImage(old_canvases[i], 0,h);
         //e_ctx.fillStyle="red";
         //e_ctx.fillRect(0, h, e_canW, 2);
         h += old_canvases[i].height;
      }
   }
   //e_ctx.fillStyle="red";
   //e_ctx.fillRect(0, h, e_canW, 2);
   
   e_ctx.font = "18px Arial";
   e_ctx.fillStyle = "lime";
   var flabel = kiwi_host() +'     '+ (new Date()).toUTCString() +'     '+ freq_displayed_kHz_str +' kHz  '+ cur_mode;
   var x = e_canW/2 - e_ctx.measureText(flabel).width/2;
   e_ctx.fillText(flabel, x, 35);

   // same format filename as .wav recording
   var imgURL = e_canvas.toDataURL("image/jpeg", 0.85);
   var dlLink = document.createElement('a');
   dlLink.download = kiwi_timestamp_filename('', '.jpg');
   dlLink.href = imgURL;
   dlLink.target = '_blank';  // opens new tab in iOS instead of download
   dlLink.dataset.downloadurl = ["image/jpeg", dlLink.download, dlLink.href].join(':');
   document.body.appendChild(dlLink);
   dlLink.click();
   document.body.removeChild(dlLink);
}


////////////////////////////////
// #zoom
////////////////////////////////

function zoomCorrection()
{
	return cfg.no_zoom_corr? 0 : (3/*dB*/ * zoom_level);
	//return 0 * zoom_level;		// gives constant noise floor when using USE_GEN
}

function zoom_finally()
{
	w3_innerHTML('id-nav-optbar-wf', 'WF'+ zoom_level.toFixed(0));
	wf_gnd_value = wf_gnd_value_base - zoomCorrection();
   extint_environment_changed( { zoom:1, passband_screen_location:1 } );
	freqset_select();
}

function zoom_dir_s(dir)
{
   if (dir == ext_zoom.OUT) return 'OUT';
   if (dir == ext_zoom.MAX_OUT) return 'MAX_OUT';
   return ['TO_BAND', 'IN', 'ABS', 'WHEEL', 'CUR', '5', '6', '7', 'NOM_IN', 'MAX_IN'][dir];
}

// Extensions can modify value to center waterfall signal between extension control panel and
// main control panel (keeps signal from bing obscured from a wide extension control panel).
// TDoA does this via zoom_center = 0.6
var zoom_center = 0.5;

var ZOOM_NOMINAL = 10, ZOOM_BAND = 6;
var zoom_nom = 0, zoom_old_nom = 0;
var zoom_levels_max = 0;
var zoom_level = 0;
var zoom_level_f = 0;
var zoom_freq = 0;
var zoom_maxin_s = ['id-maxin', 'id-maxin-nom', 'id-maxin-max'];

var x_bin = 0;				// left edge of waterfall in units of bin number

// called from mouse wheel and zoom button pushes
// x_rel: 0 .. waterfall_width, position of mouse cursor on window, -1 called from button push

function zoom_step(dir, arg2)
{
   if (wf.audioFFT_active) {
      audioFFT_update();
      return;
   }
   
	var out = (dir == ext_zoom.OUT);
	var dir_in = (dir == ext_zoom.IN);
	var not_band_and_not_abs = (dir != ext_zoom.TO_BAND && dir != ext_zoom.ABS);
	var ozoom = zoom_level;
	var x_obin = x_bin;
	var x_norm;
	var update_zoom_f = true;
	var b1, b2;
	var f, znew;
	
	if (dir == ext_zoom.WHEEL) {
	   if (arg2 == undefined) return;
	   update_zoom_f = false;
	   znew = Math.round(zoom_level_f);
	   if (znew == ozoom) return;
	   dir = (znew > ozoom)? ext_zoom.IN : ext_zoom.OUT;
	}
	
	if (dir == ext_zoom.CUR) {
	   dir = ext_zoom.ABS;
	   arg2 = zoom_level;
	}

	if (sb_trace) console.log('zoom_step dir='+ dir +'('+ zoom_dir_s(dir) +') arg2='+ arg2);
	if (dir == ext_zoom.MAX_OUT) {		// max out
		out = true;
		zoom_level = 0;
		x_bin = 0;
	} else {			// in/out, nom/max in, abs, band
	
		// clamp
		if (not_band_and_not_abs && ((out && zoom_level == 0) || (dir_in && zoom_level >= zoom_levels_max))) {
		   zoom_finally();
		   return;
		}

		if (dir == ext_zoom.TO_BAND) {
			// zoom to band
			f = center_freq + demodulators[0].offset_frequency;
			var cf;
			var band = arg2;     // band specified by caller
			if (band != undefined) {
			   b1 = band.b1;
			   b2 = band.b2;
				zoom_level = b2.zoom_level;
				cf = b2.cfHz;
				if (sb_trace)
					console.log("ZTB-user f="+f+" cf="+cf+" b="+b1.name+" z="+b2.zoom_level);
			} else {
	         var ITU_region = cfg.init.ITU_region + 1;    // cfg.init.ITU_region = 0:R1, 1:R2, 2:R3
            //console.log('zoom_band f='+ f +' ITU_region=R'+ ITU_region);
            
            var _dxcfg = dx_cfg_db();
            if (!_dxcfg) { zoom_finally(); return; }
            var _kiwi_bands = kiwi_bands_db(dx.INIT_BANDS_NO);
				for (i=0; i < _dxcfg.bands.length; i++) {    // search for first containing band
					b1 = _dxcfg.bands[i];
					b2 = _kiwi_bands[i];
		         if (!(b1.itu == kiwi.BAND_SCALE_ONLY || b1.itu == kiwi.ITU_ANY || b1.itu == ITU_region)) continue;
					if (f >= b2.minHz && f <= b2.maxHz) {
		            //console.log('zoom_band FOUND itu=R'+ b1.itu +' '+ b1.min +' '+ f +' '+ b1.max);
						break;
					}
				}
				if (i != _dxcfg.bands.length) {
					//console.log("ZTB-calc f="+f+" cf="+b2.cfHz+" b="+b1.name+" z="+b2.zoom_level);
					zoom_level = b2.zoom_level;
					cf = b2.cfHz;
				} else {
					zoom_level = ZOOM_BAND;	// not in a band -- pick a reasonable span
					cf = f;
					//console.log("ZTB-outside f="+f+" cf="+cf);
				}
			}
			out = (zoom_level < ozoom);
			x_bin = freq_to_bin(cf);		// center waterfall at middle of band
			x_bin -= norm_to_bins(zoom_center);
		} else
		
		if (dir == ext_zoom.ABS) {
			if (arg2 == undefined) { zoom_finally(); return; }		// no abs zoom value specified
			znew = arg2;
			//console.log('zoom_step ABS znew='+ znew +' zmax='+ zoom_levels_max +' zcur='+ zoom_level +' zoom_center='+ zoom_center);
			if ((znew < 0 || znew > zoom_levels_max || znew == zoom_level) && zoom_center == 0.5) {
			   zoom_finally();
			   return;
			}
			out = (znew < zoom_level);
			zoom_level = znew;
			// center waterfall at middle of passband
			x_bin = freq_to_bin(freq_passband_center());
			//console.log('ZOOM ABS z='+ znew +' out='+ out +' fpc='+ freq_passband_center() +' b='+ x_bin +'|'+ norm_to_bins(0.5) +'|'+ norm_to_bins(zoom_center));
			x_bin -= norm_to_bins(zoom_center);
		} else
		
		if (dir == ext_zoom.NOM_IN || dir == ext_zoom.MAX_IN) {
			
			// zoom max-in button toggle hack
			if (dir == ext_zoom.NOM_IN && arg2 != undefined && arg2 == 1 && zoom_level >= zoom_nom) {
				if (zoom_level == zoom_levels_max)
					zoom_level = zoom_nom;		// if at max toggle back to nom
				else
					zoom_level = zoom_levels_max;		// if at anything other than max go to max
			} else {
				zoom_level = (dir == ext_zoom.NOM_IN)? zoom_nom : zoom_levels_max;
			}
			
			out = (zoom_level < ozoom);
			
			// center max zoomed waterfall at middle of passband
			x_bin = freq_to_bin(freq_passband_center());
			x_bin -= norm_to_bins(zoom_center);
		} else {
		
			// in, out
			if (dbgUs) {
				if (sb_trace) console.log('ZOOM IN/OUT x='+ arg2);
				sb_trace=0;
			}
			if (arg2 != ext_zoom.TO_PASSBAND) {
				var x_rel = arg2;
				
				// zoom in or out centered on cursor position, not passband
				x_norm = x_rel / waterfall_width;	// normalized position (0..1) on waterfall
				//console.log('zoom to cursor xrel='+ x_rel);
			} else {
				// zoom in or out centered on passband, if visible, else middle of current waterfall
				var pv = passband_visible();
				if (pv.inside) {
					x_norm = (pv.bin - x_bin) / bins_at_cur_zoom();
				} else {
					x_norm = zoom_center;
				}
				//console.log('zoom to pb cen');
			}
			x_bin += norm_to_bins(x_norm);	// remove offset bin relative to current zoom
			zoom_level += dir;
			x_bin -= norm_to_bins(x_norm);	// add offset bin relative to new zoom
		}
	}
	
	if (update_zoom_f) zoom_level_f = zoom_level;
	//console.log("ZStep z"+zoom_level.toString()+" fLEFT="+canvas_get_dspfreq(0));
	
	var nom = (zoom_level == zoom_levels_max)? 2 : ((zoom_level >= zoom_nom)? 1:0);
	if (nom != zoom_old_nom) {
		w3_hide(zoom_maxin_s[zoom_old_nom]);
		w3_show(zoom_maxin_s[nom], 'w3-show-table-cell');
		zoom_old_nom = nom;
	}
	
	if (zoom_level == 0 || ozoom == 0) {
		w3_show_hide('id-maxout', zoom_level != 0, 'w3-show-table-cell');
		w3_show_hide('id-maxout-max', zoom_level == 0, 'w3-show-table-cell');
	}
	
	x_bin = clamp_xbin(x_bin);
	var dbins = out? (x_obin - x_bin) : (x_bin - x_obin);
	var pixel_dx = bins_to_pixels(1, dbins, out? zoom_level:ozoom);
	if (sb_trace) console.log('Zs z'+ ozoom +'>'+ zoom_level +' b='+ x_bin +'/'+ x_obin +'/'+ dbins +
	   ' bz='+ bins_at_zoom(ozoom) +' r='+ (dbins / bins_at_zoom(ozoom)) +' px='+ pixel_dx);
	var dz = zoom_level - ozoom;
	if (sb_trace) console.log('zoom_step oz='+ ozoom +' zl='+ zoom_level +' dz='+ dz +' pdx='+ pixel_dx);
	waterfall_zoom_canvases(dz, pixel_dx);
	mkscale();
	dx_schedule_update();
	if (sb_trace) console.log('$zoom_step SET Z'+ zoom_level +' start='+ x_bin);
	wf_send("SET zoom="+ zoom_level +" start="+ x_bin);
	need_maxmindb_update = true;
	kiwi_storeWrite('last_zoom', zoom_level);
	freq_link_update();
	vfo_update();
   zoom_finally();

   if (0 && dbgUs) {
      f = get_visible_freq_range();
      console.log('^ZOOM z'+ zoom_level +' '+ (f.start/1e3).toFixed(0) +'|'+
         (f.center/1e3).toFixed(0) +'|'+ (f.end/1e3).toFixed(0) +' ('+ (f.bw/1e3).toFixed(0) +' kHz)');
   }
}

function passband_increment(wider, pb_edge)
{
   if (isUndefined(pb_edge)) pb_edge = owrx.PB_NO_EDGE;
   
   var pb = ext_get_passband();
   var pb_width = Math.abs(pb.high - pb.low);
   var pb_inc;
   if (wider)
      pb_inc = ((pb_width * (1/0.80)) - pb_width) / 2;		// wider
   else
      pb_inc = (pb_width - (pb_width * 0.80)) / 2;				// narrower

   var rnd = (pb_inc > 10)? 10 : 1;
   pb_inc = Math.round(pb_inc/rnd) * rnd;
   console.log('PB w='+ pb_width +' inc='+ pb_inc +' lo='+ pb.low +' hi='+ pb.high +' pb_edge='+ pb_edge);
   if (pb_edge == owrx.PB_NO_EDGE || pb_edge == owrx.PB_HI_EDGE) {
      pb.high += wider? pb_inc : -pb_inc;
      pb.high = Math.round(pb.high/rnd) * rnd;
   }
   if (pb_edge == owrx.PB_NO_EDGE || pb_edge == owrx.PB_LO_EDGE) {
      pb.low += wider? -pb_inc : pb_inc;
      pb.low = Math.round(pb.low/rnd) * rnd;
   }
   //console.log('PB lo='+ pb.low +' hi='+ pb.high);
   ext_set_passband(pb.low, pb.high, true);
}

function zoom_click(evt, dir, arg2)
{
	if (any_alternate_click_event(evt)) {
		// currentTarget.parent=<span> currentTarget=<div> target=<img>
		var div = evt.currentTarget;
		//var parent = div.parentNode;		
		var zin = w3_contains(div, 'id-zoom-in');
		var zout = w3_contains(div, 'id-zoom-out');
		if (!zin && !zout) return;

      var edge;
      var key_mod = shortcut_key_mod(evt);
      if (key_mod == shortcut.SHIFT) edge = owrx.PB_NO_EDGE; else
      if (key_mod == shortcut.CTL_ALT) edge = owrx.PB_HI_EDGE; else
         edge = owrx.PB_LO_EDGE;
      passband_increment(zin, edge);
		return;
	}
	
	if (dir == ext_zoom.NOM_IN)
		zoom_step(dir, 1);		// zoom max-in button toggle hack
	else
		zoom_step(dir);
}

function zoom_over(evt)
{
	// not w3-disabled:  target=<img>       currentTarget=<div id-zoom-*>
	// w3-disabled:      target=<id-zoom-*> currentTarget=<div id-zoom-*>
   //event_dump(evt, 'MAG');
	var div = evt.currentTarget, el, disabled = false;
	if (w3_contains(div, 'w3-disabled')) {
	   el = div;
	   disabled = true;
	} else {
	   el = evt.target;
	}
	
	var zin = w3_contains(div, 'id-zoom-in');
   var key_mod = shortcut_key_mod(evt);
   var title = owrx.zoom_titles[key_mod][zin];
   if (disabled && key_mod == shortcut.NO_MODIFIER) title = '';
   //console.log('zoom_over key_mod='+ key_mod +' zin='+ zin +' title='+ title);
   el.title = title;
}


////////////////////////////////
// #mobile
////////////////////////////////

/*
   #mobile-ui mobileOptNew() current development:
      Drag in Fscale or WF scrolls WF when PB in L/R margin (same as Dtouch WF drag).
*/

function mobileOpt() { return ((kiwi_isMobile() && owrx.mobile && owrx.mobile.small) || kiwi.mdev); }
function mobileOptNew() { return (mobileOpt() && kiwi.mnew); }
function phone() { return (kiwi_isMobile() && owrx.mobile && owrx.mobile.phone); }
function tablet() { return (kiwi_isMobile() && owrx.mobile && owrx.mobile.tablet); }
function iPad() { return (kiwi_isMobile() && owrx.mobile && owrx.mobile.iPad); }

function mobile_init()
{
   if (!kiwi_isMobile()) return;
   
	// When a mobile device connects to a Kiwi while held in portrait orientation:
	// Remove top bar and minimize control panel if width < oldest iPad width (768px)
	// which should catch all iPhones but no iPads (iPhone X width = 414px).
	// Also scale control panel for small-screen tablets, e.g. 7" tablets with 600px portrait width.

	var mobile = owrx.mobile = ext_mobile_info();
   //console.log('$ wh='+ mobile.width +','+ mobile.height);
	
	// mobile optimization: remove top bar and switch control panel to "off".
	if (mobileOpt()) {
	   toggle_or_set_hide_bars(owrx.HIDE_TOPBAR);
	   keyboard_shortcut_nav('off');
	}
	
	// for narrow screen devices, i.e. phones and 7" tablets
	if (mobile.tablet) {
	   w3_hide('id-readme');   // don't show readme panel closed icon
	   
	   // remove top bar and band/label areas on phones
	   if (mobile.phone) {
	      toggle_or_set_hide_bars(owrx.HIDE_ALLBARS);
	   }
	}
	
   owrx.last_mobile = {};     // force rescale first time
   owrx.rescale_cnt = owrx.rescale_cnt2 = 0;

	setInterval(function() {
      mobile = owrx.mobile = ext_mobile_info(owrx.last_mobile);
      owrx.last_mobile = mobile;
      var el = w3_el('id-control');

      //canvas_log('Cwh='+ mobile.width +','+ mobile.height +' '+ mobile.orient_unchanged +
      //   '<br>r='+ owrx.rescale_cnt  +','+ owrx.rescale_cnt2 +' #'+ owrx.dseq);
      //owrx.dseq++;

	   if (0 && mobile_laptop_test) {
         canvas_log('whu='+ mobile.width +','+ mobile.height +','+ el.uiWidth +
            ' psn='+ mobile.isPortrait +','+ mobile.small +','+ mobile.narrow +' #'+ owrx.dseq);
         owrx.dseq++;
      }
   
      if (mobile.orient_unchanged) return;
      //canvas_log('ORIENT-CH');
      owrx.rescale_cnt++;
      if (owrx.rescale_cnt2 != 0) {
         toggle_panel('id-control', 1);   // always show control panel on orientation change
      }

      mobile_scale_control_panel(mobile);
	}, 500);
}

function mobile_scale_control_panel(mobile, noScale)
{
   mobile = mobile || owrx.mobile;
   if (!mobile) return;
   var el = w3_el('id-control');

   if ((noScale == true) || el.uiWidth < mobile.width) {
      el.style.transform = 'none';
      //canvas_log2(mobile.width +':'+ mobile.height +' NORM '+ el.clientWidth +':'+ el.clientHeight);
   } else {
      el.style.right = '0px';

      // scale control panel up or down to fit width of all narrow screens
      var scale = mobile.width / el.uiWidth * 0.95;
      el.style.transform = 'scale('+ scale.toFixed(2) +')';
      el.style.transformOrigin = 'bottom right';
      owrx.rescale_cnt2++;
      //canvas_log2(mobile.width +':'+ mobile.height +' SCALE '+ scale.toFixed(2) +' '+ el.clientWidth +':'+ el.clientHeight);
   }
}


////////////////////////////////
// #spectrum
////////////////////////////////

var spec = {
   height_spectrum_canvas: 200,
   tooltip_offset: 100,
   
   canvas: null,
   ctx: null,
   spectrum_image: null,
   colormap: null,
   colormap_transparent: null,
   dB: null,
   dB_ttip: null,
   
   update: 0,
   last_update: 0,
   need_update: 0,
   
   need_clear_avg: 0,
   clear_avg: 0,
   avg: [[], []],
   
   //slow_dev_color: '#66ffff',    // light-cyan hsl(180, 100%, 70%)
   slow_dev_color: '#d3d3d3',


   // spectrum & peak hold
   SPEC_RF: 0,
   SPEC_AF: 1,
   TRACE_0: 0,
   TRACE_1: 1,
   
   NONE: 0,
   RF: 1,
   AF: 2,
   CHOICES: 3,
   source: 0,
   
   PEAK_OFF: 0,
   PEAK_ON: 1,
   PEAK_HOLD: 2,
   peak_show: [0, 0],               // [TRACE_0|1]

   peak_clear_spec: [0, 0],         // [TRACE_0|1]
   peak_clear_btn: [0, 0],          // [TRACE_0|1]
   peak: [ [[], []], [[], []] ],    // [SPEC_RF|AF] [TRACE_0|1] [x]
   peak1_color: 'yellow',
   peak2_color: 'magenta',
   peak_alpha: 1.0,


   dB_bands: [],
   redraw_dB_scale: false,
};

function spectrum_init()
{
	spec.colormap = spec.ctx.createImageData(1, spec.canvas.height);
	spec.colormap_transparent = spec.ctx.createImageData(1, spec.canvas.height);
	update_maxmindb_sliders();
	spectrum_dB_bands();
	var spectrum_update_rate_Hz = kiwi_isMobile()? 10:10;  // limit update rate since rendering spectrum is currently expensive
	//if (kiwi_isMobile()) alert('spectrum_update_rate_Hz = '+ spectrum_update_rate_Hz +' Hz');
	setInterval(function() { spec.update++; }, 1000 / spectrum_update_rate_Hz);

   spec.spectrum_image = spec.ctx.createImageData(spec.canvas.width, spec.canvas.height);
   
   if (!wf.audioFFT_active && rx_chan >= wf_chans) {
		// clear entire spectrum canvas to black
		var sw = spec.canvas.width;
		var sh = spec.canvas.height;
		spec.ctx.fillStyle = "black";
		spec.ctx.fillRect(0,0,sw,sh);
		
	   spec.ctx.font = "18px sans-serif";
      spec.ctx.fillStyle = "white";
      var text =
         wf_chans?
            ('Spectrum not available for rx'+ rx_chan)
         :
            'Spectrum not allowed on this Kiwi';
      var tw = spec.ctx.measureText(text).width;
      spec.ctx.fillText(text, sw/2-tw/2, sh/2);
   }

   kiwi_load_js('extensions/colormap/colormap.js',
      function() {
         waterfall_controls_setup();
         waterfall_init();
         colormap_init();
      }
   );
}

// based on WF max/min range, compute color banding each 10 dB for spectrum display
function spectrum_dB_bands()
{
	spec.dB_bands = [];
	var i=0;
	//var color_shift_dB = -8;	// give a little floor room to the spectrum colormap
	var color_shift_dB = -12;	// give a little floor room to the spectrum colormap
	var s_maxdb = maxdb, s_mindb = mindb;

	// prevent an illegal configuration of mindb >= maxdb causing code below
	// to loop infinitely (i.e. s_full_scale = 0 => / 0)
	if (s_mindb >= s_maxdb) {
	   s_maxdb = -10;
	   s_mindb = -110;
	}

	var s_full_scale = s_maxdb - s_mindb;
	var barmax = s_maxdb, barmin = s_mindb + color_shift_dB;
	var rng = barmax - barmin;
	rng = rng? rng : 1;	// can't be zero
	//console.log("DB barmax="+barmax+" barmin="+barmin+" s_maxdb="+s_maxdb+" s_mindb="+s_mindb);
	var last_norm = 0;

   var anti_looping = 0;
	for (var dB = Math.floor(s_maxdb/10)*10; (s_mindb-dB) < 10; dB -= 10) {
		var norm = 1 - ((dB - s_mindb) / s_full_scale);
		if (norm > 1) norm = 1;
		var cmi = Math.round((dB - barmin) / rng * 255);
		var color = color_map[cmi];
		var color_transparent = color_map_transparent[cmi];
		var color_name = '#'+(color >>> 8).toString(16).leadingZeros(6);
		
		var ypos_f = function(norm) { return Math.round(norm * spec.canvas.height); };
		var ypos_last = ypos_f(last_norm);
		var ypos = ypos_f(norm);
		if (ypos_last == 0 && ypos == 0) continue;
		spec.dB_bands[i] = { dB:dB, y1:ypos_last, y2:ypos, norm:norm.toFixed(2), color:color_name };
		for (var y = ypos_last; y < ypos; y++) {
			for (var j=0; j<4; j++) {
				spec.colormap.data[y*4+j] = ((color>>>0) >> ((3-j)*8)) & 0xff;
				spec.colormap_transparent.data[y*4+j] = ((color_transparent>>>0) >> ((3-j)*8)) & 0xff;
			}
			if (anti_looping++ > 8192) break;
		}
		//console.log("DB"+i+' '+dB+' norm='+norm+' last_norm='+last_norm+' cmi='+cmi+' '+color_name+' sh='+spec.canvas.height);
		last_norm = norm;
		
		i++;
      if (anti_looping++ > 8192) break;
	}

	spec.redraw_dB_scale = true;
	w3_call('colormap_aper');
}


// Spectrum tooltip
// Not understood: The spectrum dB tooltip refuses to appear on all browsers using the Android tablets.
// We checked the element visibility, z-index, etc. and nothing helped. Works fine with iOS.

function spectrum_tooltip_cancel(from)
{
   var demod = demodulators[0];
   var allow_pb_adj = (!kiwi_isMobile() && owrx.allow_pb_adj);
   var is_active = (owrx.spec_pb_mkr_dragging != owrx.PB_NO_EDGE);

   if (allow_pb_adj) {
      //console.log('spectrum_tooltip_cancel from='+ from);
      //kiwi_trace();
      if (from != owrx.EV_OUTSIDE) demod.envelope.drag_end(0);
   }
   owrx.spec_pb_mkr_dragging = owrx.PB_NO_EDGE;
   owrx.allow_pb_adj_at_start = false;
   //console.log('is_active='+ is_active);
   return is_active;
}

function spectrum_tooltip_update(type, evt, clientX, clientY)
{
   // needed because owrx.allow_pb_adj can become false as pb is adjusted to be very small
   var mouse_L = (evt && evt.buttons & mouse.BUTTONS_L);
   if (!mouse_L) owrx.allow_pb_adj_at_start = owrx.allow_pb_adj;     // latch val when button still up
   var allow_pb_adj = (!kiwi_isMobile() && owrx.allow_pb_adj_at_start);
   //console.log(type +' '+ TF(allow_pb_adj) + TF(owrx.allow_pb_adj) + TF(owrx.allow_pb_adj_at_start));
   var is_active = (owrx.spec_pb_mkr_dragging != owrx.PB_NO_EDGE);
   var demod = demodulators[0];

   if (type == owrx.EV_END)
      return spectrum_tooltip_cancel(owrx.EV_END);

	var target = (evt && (evt.target == spec.dB || evt.currentTarget == spec.dB || evt.target == spec.dB_ttip || evt.currentTarget == spec.dB_ttip));
	//console.log('CD '+ target +' x='+ clientX +' tgt='+ evt.target.id +' ctg='+ evt.currentTarget.id);
	//if (kiwi_isMobile()) alert('CD '+ tf +' x='+ clientX +' tgt='+ evt.target.id +' ctg='+ evt.currentTarget.id);
   //console.log('STTU '+ type);

   // if mouse wanders outside spectrum cancel the tooltip update process
	if (!target) return spectrum_tooltip_cancel(owrx.EV_OUTSIDE);
   
   // if dragging, but there is no button down, then this could occur if the button was released
   // while mouse positioned over the freq scale preventing the canvas mouseup from occuring
   if (type == owrx.EV_DRAG && is_active && !mouse_L) {
      //console.log('mouse up cancel');
      return spectrum_tooltip_cancel(owrx.EV_MOUSEUP);
   }

   // This is a little tricky. The tooltip text span must be included as an event target so its position will update when the mouse
   // is moved upward over it. But doing so means when the cursor goes below the bottom of the tooltip container, the entire
   // spectrum div in this case, having included the tooltip text span will cause it to be re-positioned again. And the hover
   // doesn't go away unless the mouse is moved quickly. So to stop this we need to manually detect when the mouse is out of the
   // tooltip container and stop updating the tooltip text position so the hover will end.

   if (owrx.debug_drag) canvas_log('*STTU*');
   //event_dump(evt, 'SPEC');
   var h = spec.height_spectrum_canvas;
   if (clientY < 0 || clientY >= h) return false;

   var adj_pb = false, show_dB = true;
   var cx = clientX - (kiwi_isMobile()? spec.tooltip_offset : 0);
   if (owrx.debug_drag) canvas_log('['+  cx +','+ clientY +']');
   var f_kHz, f_text;
   var isRF = (spec.source == spec.RF);
   
   if (isRF) {
      var x1 = owrx.pbx1, x2 = owrx.pbx2;
      //console.log(x1 +'|'+ xx +'|'+ x2);
      var o = 5;
      var lo_edge = (cx >= (x1-o) && cx <= (x1+o));
      var hi_edge = (cx >= (x2-o) && cx <= (x2+o));
      
      // spec_pb_mkr_dragging logic is needed because delays in updating owrx.pbx[12] mean cx
      // might be outside {lo,hi}_edge if the mouse is moved quickly.
      if (allow_pb_adj && (lo_edge || hi_edge || owrx.spec_pb_mkr_dragging != owrx.PB_NO_EDGE)) {
         if (canvas_mouse_down_or_touch) {
            if (type == owrx.EV_START) {
               owrx.spec_pb_mkr_dragging = lo_edge? owrx.PB_LO_EDGE : owrx.PB_HI_EDGE;
               demod.envelope.env_drag_start(cx, 0, { shiftKey:false, altKey: false, ctrlKey: false });
            } else
            if (type == owrx.EV_DRAG) {
               demod.envelope.drag_move(cx);
            }
            adj_pb = true;
         }
         if (owrx.spec_pb_mkr_dragging != owrx.PB_NO_EDGE)
            lo_edge = (owrx.spec_pb_mkr_dragging == owrx.PB_LO_EDGE);
         f_text = (lo_edge? 'lo ' : 'hi ') + (lo_edge? demod.low_cut : demod.high_cut);
         spec.dB_ttip.innerHTML = f_text +', bw '+ Math.abs(demod.high_cut - demod.low_cut);
         spec.dB.style.cursor = 'col-resize';
         show_dB = false;
         spec.dB_ttip.style.left = px(cx - 20);
         spec.dB_ttip.style.bottom = px(h + 18 - clientY);
         spec.dB_ttip.style.width = px(180);
      } else {
         f_kHz = (canvas_get_dspfreq(cx) + kiwi.freq_offset_Hz)/1000;
         spec.dB.style.cursor = null;
      }
   } else {    // spec.AF
      var norm = cx/waterfall_width - 0.5;
      norm *= (waterfall_width + spec.af_margins)/waterfall_width;
      f_kHz = norm * audio_input_rate / 1000;
   }
   
   if (show_dB) {
      f_text = format_frequency("{x}", f_kHz, 1, freq_field_prec(f_kHz));
      var dB = (((h - clientY) / h) * full_scale) + mindb;
      spec.dB_ttip.innerHTML = f_text +' kHz'+ (isRF? '':' AF') +'<br>'+ dB.toFixed(0) +' dBm';
      spec.dB_ttip.style.left = px(cx);
      spec.dB_ttip.style.bottom = px(h + 10 - clientY);
      spec.dB_ttip.style.width = px(140);
   }
   
   return adj_pb;
}

function spectrum_update(data)
{
   // because of audio pipeline delay need to check
   if (spec.source == spec.NONE) {
      //console.log('spectrum_update: spec.NONE');
      return;
   }

	var i, trace, x, y, z, band;
   spec.last_update = spec.update;
   
   // clear entire spectrum canvas to black
   var ctx = spec.ctx;
   var dBtext_w = 25;
   var sw2 = spec.canvas.width;   // spec.AF width, 1024, spec.af_canvas
   var sw1 = sw2 - dBtext_w;      // spec.RF width,  999, spec.canvas
   var sw = (spec.source == spec.RF)? sw1 : sw2;
   var sh = spec.canvas.height;
   w3_visible('id-spectrum-af-canvas', spec.source == spec.AF);

   ctx.fillStyle = "black";
   ctx.fillRect(0,0, sw1,sh);
   spec.af_ctx.fillStyle = "black";
   spec.af_ctx.fillRect(0,0, spec.canvas.width,sh);
   
   // draw lines every 10 dB
   // spectrum data will overwrite
   ctx.fillStyle = "lightGray";
   spec.af_ctx.fillStyle = "lightGray";
   for (i=0; i < spec.dB_bands.length; i++) {
      band = spec.dB_bands[i];
      y = Math.round(band.norm * sh);
      ctx.fillRect(0,y, sw1,1);
      spec.af_ctx.fillRect(0,y, sw2,1);
   }
   
   // for audio spectrum annotate the passband with frequency divisions (vertical lines)
   if (spec.source == spec.AF) {
      var sr = ext_nom_sample_rate();
      var frac = sr % 1000;
      var sp = (sr - frac) - 1000;
      //console.log('sp='+ sp +' frac='+ frac);
      for (i = 1000 + frac/2; i <= sp + frac/2; i += 1000) {
         //if (i == sr/2) continue;
         x = Math.round(spec.canvas.width * i/sr);
         //console.log(x);
         spec.af_ctx.fillRect(x,0, 1,sh);
      }
      spec.af_ctx.fillStyle = 'lime';
      spec.af_ctx.fillRect(Math.round(spec.canvas.width/2)-1,0, 3,sh);
      spec.af_ctx.fillStyle = 'red';
      spec.af_ctx.fillRect(0,0, 3,sh);
      spec.af_ctx.fillRect(spec.canvas.width-3,0, 3,sh);
   } else {

      // RF spectrum passband marker
      var pbctx = spec.pb_ctx;
      var cvt = spec.canvas.width / waterfall_width;
      var x1 = w3_clamp(Math.round(owrx.pbx1 * cvt), 0, sw1);
      var x2 = w3_clamp(Math.round(owrx.pbx2 * cvt), 0, sw1);
      if (x1 != pbctx.x1 || x2 != pbctx.x2 || pbctx.seq != wfext.spb_color_seq ||
         pbctx.resize_seq != owrx.resize_seq) {
         //console.log('$ '+ x1 +':'+ x2);
         if (isNumber(pbctx.x1)) {
            pbctx.fillStyle = 'rgba(0,0,0,0)';
            pbctx.clearRect(pbctx.x1,0, pbctx.x2-pbctx.x1,sh);
            //console.log('clear '+ pbctx.x1 +','+ pbctx.x2);
         }
         
         // don't show if it takes up the whole space (e.g. z14)
         if ((x1 > 0 || x2 < sw1) && wfext.spb_on && owrx.allow_pb_adj) {
            pbctx.fillStyle = wfext.spb_color;
            pbctx.fillRect(x1,0, x2-x1,sh);
            //console.log('fill '+ wfext.spb_color +' '+ x1 +','+ x2);
         }
         pbctx.x1 = x1;
         pbctx.x2 = x2;
         pbctx.seq = wfext.spb_color_seq;
         pbctx.resize_seq = owrx.resize_seq;
      }
      //console.log('seq='+ owrx.resize_seq +' '+ cvt.toFixed(2) +' '+ spec.canvas.width +' '+ waterfall_width +' '+ owrx.pbx1 +','+ owrx.pbx2 +' '+ x1 +','+ x2 +' ['+ sw1);
      //console.log(ctx.globalAlpha.toFixed(2));
   }

   var rf_af = (spec.source == spec.RF)? spec.SPEC_RF : spec.SPEC_AF;
   if (spec.clear_avg || isNaN(spec.avg[rf_af][0])) {
      for (x=0; x < sw; x++) {
         spec.avg[rf_af][x] = color_index(data[x]);
      }
      spec.clear_avg = false;
      spec.peak_clear_spec[1] = spec.peak_clear_spec[0] = true;
   }
   
   for (trace = 0; trace < 2; trace++) {
   
      // if global clear, clear both RF and AF spectrums
      if (spec.peak_clear_spec[trace]) {
         for (x=0; x < sw1; x++) {
            spec.peak[spec.SPEC_RF][trace][x] = 0;
         }
         for (x=0; x < sw2; x++) {
            spec.peak[spec.SPEC_AF][trace][x] = 0;
         }
	      //toggle_or_set_spec_peak(toggle_e.SET, spec.PEAK_OFF, trace);
         spec.peak_clear_spec[trace] = false;
         
      }
      
      // if button, just clear currently selected spectrum
      if (spec.peak_clear_btn[trace]) {
         for (x=0; x < sw; x++) {
            spec.peak[rf_af][trace][x] = 0;
         }
         spec.peak_clear_btn[trace] = false;
      }
   }
   
   // if necessary, draw scale on right side
   if (spec.redraw_dB_scale) {
   
      // set sidebar background where the dB text will go
      for (x = sw1; x < spec.canvas.width; x++) {
         ctx.putImageData(spec.colormap_transparent, x, 0, 0, 0, 1, sh);
      }
      
      // the dB scale text
      ctx.fillStyle = "white";
      for (i=0; i < spec.dB_bands.length; i++) {
         band = spec.dB_bands[i];
         y = Math.round(band.norm * sh);
         ctx.fillText(band.dB, sw1+3, y-4);
         //console.log("SP x="+sw1+" y="+y+' '+dB);
      }
      spec.redraw_dB_scale = false;
   }
	
	// Add line to spectrum image
	if (spec.source == spec.AF) {
	   ctx = spec.af_ctx;
	}
	
   if (spectrum_slow_dev)
      ctx.fillStyle = spec.slow_dev_color;

   for (x=0; x < sw; x++) {
      z = color_index(wf_gnd? wf_gnd_value : data[x]);

      switch (spec_filter) {
      
      case wf_sp_menu_e.IIR:
         // non-linear spectrum filter from Rocky (Alex, VE3NEA)
         // see http://www.dxatlas.com/rocky/advanced.asp
         
         var iir_gain = 1 - Math.exp(-sp_param * z/255);
         if (iir_gain <= 0.01) iir_gain = 0.01;    // enforce minimum decay rate
         var z1 = spec.avg[rf_af][x];
         z1 = z1 + iir_gain * (z - z1);
         if (z1 > 255) z1 = 255; if (z1 < 0) z1 = 0;
         z = spec.avg[rf_af][x] = z1;
         break;
         
      case wf_sp_menu_e.MMA:
         if (z > 255) z = 255; if (z < 0) z = 0;
         spec.avg[rf_af][x] = ((spec.avg[rf_af][x] * (sp_param-1)) + z) / sp_param;
         z = spec.avg[rf_af][x];
         break;
         
      case wf_sp_menu_e.EMA:
         if (z > 255) z = 255; if (z < 0) z = 0;
         spec.avg[rf_af][x] += (z - spec.avg[rf_af][x]) / sp_param;
         z = spec.avg[rf_af][x];
         break;
         
      //case wf_sp_menu_e.OFF:
      default:
         if (z > 255) z = 255; if (z < 0) z = 0;
         break;
      }

      // accumulate peak trace(s)
      for (trace = 0; trace < 2; trace++) {
         if (spec.peak_show[trace] == spec.PEAK_ON && z > spec.peak[rf_af][trace][x])
            spec.peak[rf_af][trace][x] = z;
      }

      // draw the spectrum based on the spectrum colormap which should
      // color the 10 dB bands appropriately
      y = Math.round((1 - z/255) * sh);

      if (spectrum_slow_dev) {
         ctx.fillRect(x,y, 1,sh-y);
      } else {
         // this is very slow on underpowered mobile devices
         // hence need for "slow dev" button
         //ctx.putImageData(spec.colormap, x,0, 0,y, 1,sh-y+1);

         // This now runs as fast as "slow dev" mode.
         // But we left "slow dev" mode in because some people like the single-color fill.
         //y = x % 200;      // checks for missing values
         var first = true;
         for (i = 0; i < spec.dB_bands.length; i++) {
            var b = spec.dB_bands[i];
            if (first && y > b.y2) continue;
            first = false;
            ctx.fillStyle = b.color;
            var h = b.y2 - y;
            ctx.fillRect(x,y, 1,h);
            y = b.y2;
         }
      }
   }
   
   // draw peak trace(s)
   for (trace = 0; trace < 2; trace++) {
      if (spec.peak_show[trace] != spec.PEAK_OFF) {
         ctx.lineWidth = 1;
         ctx.strokeStyle = trace? spec.peak2_color : spec.peak1_color;
         ctx.beginPath();
         y = Math.round((1 - spec.peak[rf_af][trace][0]/255) * sh) - 1;
         ctx.moveTo(0,y);
         for (x=1; x < sw; x++) {
            y = Math.round((1 - spec.peak[rf_af][trace][x]/255) * sh) - 1;
            ctx.lineTo(x,y);
         }
         ctx.globalAlpha = spec.peak_alpha;
         //ctx.fill();
         ctx.stroke();
         ctx.globalAlpha = 1;
      }
   }

   if (spec.switch_container) {
      w3_show_hide('id-spectrum-container', true);
      w3_show_hide('id-top-container', false);
      spec.switch_container = false;
   }
}


////////////////////////////////
// #waterfall
////////////////////////////////

var waterfall_setup_done = 0;
var waterfall_timer;
var waterfall_ms;

var waterfall_scrollable_height;

var wf_canvases = [];
var old_canvases = [];
var wf_cur_canvas = null;
var wf_canvas_default_height = 200;
var wf_canvas_actual_line;
var wf_canvas_id_seq = 1;
var wf_canvas_maxshift = 0;

function wf_init()
{
   w3_do_when_cond(
      function() {
         //console.log('### '+ (okay_wf_init? 'GO' : 'WAIT') +' wf_init(okay_wf_init)');
         return okay_wf_init;
      },
      function() {
         wf_init_ready();
      }, null,
      200
   );
   // REMINDER: w3_do_when_cond() returns immediately
}

function wf_init_ready()
{
	init_wf_container();

   wf.audioFFT_active = (rx_chan >= wf_chans);
	resize_waterfall_container(false);
	resize_wf_canvases();
	bands_init();
	panels_setup();
	scale_setup();
	mkcolormap();
   mkscale();
	spectrum_init();
	dx_schedule_update();
	users_init( { user:1 } );
	ident_init();
	stats_init();
	openwebrx_resize();
	if (spectrum_show) setTimeout(spec_show, 2000);    // after control instantiation finishes
   
	audioFFT_setup();

	waterfall_ms = 900/wf_fps_max;
	waterfall_timer = window.setInterval(waterfall_dequeue, waterfall_ms);
	//console.log('waterfall_dequeue @ '+ waterfall_ms +' msec');
	
	// if extension going to be opened delay applying keys
   if (isNonEmptyArray(shortcut.keys) && !override_ext)
	   setTimeout(keyboard_shortcut_url_keys, 5000);
	
	canvas_mouse_wheel_set(kiwi_storeInit('wheel_tunes'));

	wf_snap(kiwi_storeInit('wf_snap'));

   mobile_init();
   
   dx_init();
   
   if (cfg.init.setup == owrx.SETUP_RF_SPEC_WF) {
      toggle_or_set_hide_bars(owrx.HIDE_ALLBARS);
      toggle_or_set_spec(toggle_e.SET, spec.RF);
   } else
   if (cfg.init.setup == owrx.SETUP_WF) {
      toggle_or_set_hide_bars(owrx.HIDE_ALLBARS);
   } else
   if (cfg.init.setup == owrx.SETUP_DX) {
      toggle_or_set_hide_bars(owrx.HIDE_TOPBAR);
   } else
   if (cfg.init.setup == owrx.SETUP_TOPBAR) {
      toggle_or_set_hide_bars(owrx.HIDE_BANDBAR);
   }

   if (cfg.init.tab) keyboard_shortcut_nav(kiwi.tab_s[cfg.init.tab]);

	openwebrx_resize();
	waterfall_setup_done = 1;
}

function add_wf_canvas()
{
   var new_canvas = create_canvas('id-wf-canvas', wf_fft_size, wf_canvas_default_height, waterfall_width, wf_canvas_default_height);
	new_canvas.id_seq = wf_canvas_id_seq++;
	wf_canvas_actual_line = wf_canvas_default_height-1;
	new_canvas.style.left = 0;
	new_canvas.openwebrx_height = wf_canvas_default_height;	

	// initially the canvas is one line "above" the top of the container
	new_canvas.openwebrx_top = (-wf_canvas_default_height+1);	
	new_canvas.style.top = px(new_canvas.openwebrx_top);

	new_canvas.oneline_image = new_canvas.ctx.createImageData(wf_fft_size, 1);

	wf_container.appendChild(new_canvas);
	add_canvas_listener(new_canvas);
	wf_canvases.unshift(new_canvas);		// add to front of array which is top of waterfall

	wf_cur_canvas = new_canvas;
	if (kiwi_gc_wf) new_canvas = null;	// gc
}

function wf_shift_canvases()
{
	// shift the canvases downward by increasing their individual top offsets
	wf_canvases.forEach(function(p) {
		p.style.top = px(p.openwebrx_top++);
	});
	
	// retire canvases beyond bottom of scroll-back window
	while (wf_canvases.length) {
		var p = wf_canvases[wf_canvases.length-1];	// oldest
		
		if (p == null || p.openwebrx_top < waterfall_scrollable_height) {
			if (kiwi_gc_wf) p = null;	// gc
			break;
		}
		var pp = wf_canvases[wf_canvases.length-2];
		if (pp) old_canvases.unshift(pp);
		if (old_canvases.length > 15) old_canvases.pop();  // save up to 10*wf_fps seconds 
		//p.style.display = "none";
		if (kiwi_gc_wf) remove_canvas_listener(p);	// gc
		wf_container.removeChild(p);
		if (kiwi_gc_wf) p = wf_canvases[wf_canvases.length-1] = null;	// gc
		wf_canvases.pop();
	}
	
	// set the height of the phantom to fill the unused space
	wf_canvas_maxshift++;
	if (wf_canvas_maxshift < wf_container.clientHeight) {
		canvas_phantom.style.top = px(wf_canvas_maxshift);
		canvas_phantom.style.height = px(wf_container.clientHeight - wf_canvas_maxshift);
		w3_show_block(canvas_phantom);
	} else
	if (!canvas_phantom.hidden) {
		w3_hide(canvas_phantom);
		canvas_phantom.hidden = true;
	}
	
	//wf_container.style.height = px(((wf_canvases.length-1)*wf_canvas_default_height)+(wf_canvas_default_height-wf_canvas_actual_line));
	//wf_container.style.height = "100%";
}

function resize_wf_canvases()
{
	window_width = wf_container.innerWidth;
	waterfall_width = wf_container.clientWidth;
	//console.log("RESIZE winW="+window_width+" wfW="+waterfall_width);

	new_width = px(waterfall_width);
	var zoom_value = 0;
	//console.log("RESIZE z"+zoom_level+" nw="+new_width+" zv="+zoom_value);

	wf_canvases.forEach(function(p) {
		p.style.width = new_width;
		p.style.left = zoom_value;
	});

	canvas_annotation.width = waterfall_width;
	canvas_annotation.style.width = new_width;

	canvas_phantom.style.width = new_width;
	canvas_phantom.style.left = zoom_value;

	spec.canvas.style.width = new_width;
	spec.pb_canvas.style.width = new_width;
	spec.af_canvas.style.width = px(waterfall_width - spec.af_margins);

   // above width change clears canvas, so redraw
   if (wf.audioFFT_active && !kiwi_isMobile()) {
      var reason;
      if (wf.no_wf) {
         reason = 'when \"no_wf\" URL option used.';
      } else {
         if (wf_chans == 0) {
            reason = 'because all waterfalls disabled<br>on this Kiwi.';
         } else {
            reason = 'on channels '+ wf_chans_real +'-'+ (rx_chans-1) +' of Kiwis<br>' +
               'configured for '+ rx_chans +' channels.';
         }
      }
      
      w3_innerHTML('id-annotation-div',
         w3_div('w3-section w3-container',
            w3_text('w3-large|color:cyan', 'Audio FFT<br>'),
            w3_text('w3-small|color:cyan', 'Zoom waterfall not available<br>' + reason),
            w3_text('w3-small|color:cyan', '<br>For details see the Kiwi forum.'
            )
         )
      );
   }

/*
   if (rx_chan >= wf_chans) {
		var sw = canvas_annotation.width;
		var sh = canvas_annotation.height;
      var ctx = canvas_annotation.ctx;
		ctx.clearRect(0,0,sw,sh);
	   ctx.font = "18px sans-serif";
	   ctx.textBaseline = "middle";
	   ctx.textAlign = "left";
	   if (wf.audioFFT_active) {
	      var x = sw/8;
         var y = sh/2;
         var text = 'AudioFFT';
         w3_fillText(ctx, x, y, 'AudioFFT', 'cyan');
         y += 18;
         w3_fillText(ctx, x, y, '(zoom waterfall not available)', null, '14px sans-serif');
	   } else {
         var text =
            wf_chans?
               ('Waterfall not available for rx'+ rx_chan)
            :
               'Waterfall not allowed on this Kiwi';
         w3_fillText(ctx, sw/2, sh/2, text, 'black');
      }
   }
*/
}

function waterfall_add_line(line)
{
   var c;
   var h = 2;
   line = Math.round(line);
   
   if (!wf.lineCanvas) {
      c = document.createElement('canvas');
      c.width = 16; c.height = 1;
      var ctx = c.getContext('2d');
      ctx.fillStyle = "white";
      ctx.fillRect(0, 0, 8, 1);
      ctx.fillStyle = "black";
      ctx.fillRect(8, 0, 8, 1);
      wf.lineCanvas = c;
   }
   
   c = wf_cur_canvas;
   c.ctx.rect(0, line, c.width, h);
   c.ctx.fillStyle = c.ctx.createPattern(wf.lineCanvas, 'repeat');
   c.ctx.fill();
   
   if (line + h > c.height) {       // overlaps end of canvas
      var c2 = wf_canvases[1];
      c2.ctx.rect(0, line - c.height + h, c2.width, h);
      c2.ctx.fillStyle = c2.ctx.createPattern(wf.lineCanvas, 'repeat');
      c2.ctx.fill();
   }
}

function waterfall_add_text(line, x, y, text, font, size, color, opts)
{
   line = Math.round(line);
   x = Math.round(x);
   y = Math.round(y);
   
   var c = wf_cur_canvas;
	w3_fillText_shadow(c, text, x, line + y, font, size, color, opts);

   if (line + 10 > c.height)  {     // overlaps end of canvas
      var c2 = wf_canvases[1];
      if (c2) w3_fillText_shadow(c2, text, x, line - c.height + y, font, size, color, opts);
   } 
}

function waterfall_timestamp()
{
   var tstamp = (wf.ts_tz == 0)? ((new Date()).toUTCString().substr(17,8) +' UTC') : ((new Date()).toString().substr(16,8) +' L');
   waterfall_add_text(wf_canvas_actual_line, 12, 12, tstamp, 'Arial', 14, 'lime', { left:1 });
}

function wf_snap(set)
{
   //console.log('wf_snap cur='+ owrx.wf_snap +' set='+ set);
   owrx.wf_snap = isUndefined(set)? (owrx.wf_snap ^ 1) : (isNull(set)? 0 : (+set));
   //console.log('wf_snap new='+ owrx.wf_snap);
   w3_el('id-waterfall-container').style.cursor = owrx.wf_cursor = owrx.wf_snap? 'col-resize' : 'crosshair';
   kiwi_storeWrite('wf_snap', owrx.wf_snap);
}

var page_scroll_amount = 0.8;

function page_scroll(norm_dir)
{
   if (!wf.audioFFT_active) {
      var dbins = norm_to_bins(norm_dir);
      waterfall_pan_canvases(dbins);		// < 0 = pan left (toward lower freqs)
   }
   freqset_select();
}

function page_scroll_icon_click(evt, norm_dir)
{
   if (any_alternate_click_event(evt)) {
      dx_label_step((norm_dir < 0)? -1:1);
   } else {
      page_scroll(norm_dir);
   }
}

var waterfall_dont_scale=0;
var need_maxmindb_update = false;
var need_clear_wf_sp_avg = false;

var need_clear_wfavg = false, clear_wfavg = true;
var wfavg = [];

// amounts empirically determined
var wf_swallow_samples = [ 2, 4, 8, 18 ];    // for zoom: 11, 12, 13, 14
var x_bin_server_last, wf_swallow = 0;

function waterfall_add(data_raw, audioFFT)
{
   var x, y, z;
	if (data_raw == null) return;
	//var canvas = wf_canvases[0];
	var canvas = wf_cur_canvas;
	if (canvas == null) return;
	
	var data_arr_u8, data, decomp_data;
   var w = wf_fft_size;
   var x_bin_server, x_zoom_server;
	
	if (audioFFT == 0) {
      var u32View = new Uint32Array(data_raw, 4, 3);
      x_bin_server = u32View[0];    // bin & zoom from server at time data was queued
      var u32 = u32View[1];
      if (kiwi_gc_wf) u32View = null;	// gc
      x_zoom_server = u32 & 0xffff;
      var flags = (u32 >> 16) & 0xffff;
   
      data_arr_u8 = new Uint8Array(data_raw, 16);	// unsigned dBm values, converted to signed later on
      var bytes = data_arr_u8.length;
   
      // when caught up, update the max/min db so lagging w/f data doesn't use wrong (newer) zoom correction
      if (need_maxmindb_update && zoom_level == x_zoom_server) {
         update_maxmindb_sliders();
         need_maxmindb_update = false;
      }
   
      // when caught up, clear spectrum using new data
      if (x_bin == x_bin_server && zoom_level == x_zoom_server) {
         if (need_clear_wf_sp_avg || spec.need_clear_avg) spec.clear_avg = true;
         if (need_clear_wf_sp_avg || need_clear_wfavg)   clear_wfavg = true;
         need_clear_wf_sp_avg = need_clear_wfavg = spec.need_clear_avg = false;
      }
      
      if (flags & wf.COMPRESSED) {
         decomp_data = new Uint8Array(bytes*2);
         var wf_adpcm = { index:0, previousValue:0 };
         decode_ima_adpcm_e8_u8(data_arr_u8, decomp_data, bytes, wf_adpcm);
         var ADPCM_PAD = 10;
         data = decomp_data.subarray(ADPCM_PAD);
      } else {
         data = data_arr_u8;
      }
      
      wf.no_sync = (flags & wf.NO_SYNC);
      
      // When zoom level is too high there is a glich in WF DDC data.
      // Swallow a few WF samples in that case (amount is zoom level dependent).
      if (wf_swallow) {
         wf_swallow--;
         return;
      }

      if (x_bin_server != x_bin_server_last) {
         x_bin_server_last = x_bin_server;
         if (zoom_level >= 11) {
            wf_swallow = wf_swallow_samples[zoom_level-11];
            if (wf_fps != wf_fps_max) {
               wf_swallow = Math.round(wf_swallow * wf_fps / wf_fps_max);
               //console.log('# wf_fps='+ wf_fps +' wf_swallow_samples='+ wf_swallow_samples[zoom_level-11] +' wf_swallow='+ wf_swallow);
            }
         }
      }
   } else {
      data = data_raw;     // unsigned dB values, converted to signed later on
      if (spec.source == spec.RF && spec.need_clear_avg) {
         spec.clear_avg = true;
         spec.need_clear_avg = false;
      }
   }
	
	// spectrum
	if (spec.source == spec.RF && spec.update != spec.last_update) {
	   spectrum_update(data);
	}

   // waterfall
	var oneline_image = canvas.oneline_image;

   for (x=0; x<w; x++) {
      z = color_index(wf_gnd? wf_gnd_value : data[x], wf.sqrt);
      
      if (audioFFT == 0)
      switch (wf_filter) {
      
         case wf_sp_menu_e.IIR:
            // non-linear spectrum filter from Rocky (Alex, VE3NEA)
            // see http://www.dxatlas.com/rocky/advanced.asp
         
            if (clear_wfavg) wfavg[x] = z;
            var iir_gain = 1 - Math.exp(-wf_param * z/255);
            if (iir_gain <= 0.01) iir_gain = 0.01;    // enforce minimum decay rate
            var z1 = wfavg[x];
            z1 = z1 + iir_gain * (z - z1);
            if (z1 > 255) z1 = 255; if (z1 < 0) z1 = 0;
            wfavg[x] = z1;
            z = Math.round(wfavg[x]);
            break;
         
         case wf_sp_menu_e.MMA:
            if (clear_wfavg) wfavg[x] = z;
            wfavg[x] = ((wfavg[x] * (wf_param-1)) + z) / wf_param;
            z = Math.round(wfavg[x]);
            break;
         
         case wf_sp_menu_e.EMA:
            if (clear_wfavg) wfavg[x] = z;
            wfavg[x] += (z - wfavg[x]) / wf_param;
            //if (x == 400) console.log('old='+ old +' clr='+ clear_wfavg +' z='+ z +' new='+ wfavg[x]);
            z = Math.round(wfavg[x]);
            break;
         
         //case wf_sp_menu_e.OFF:
         default:
            break;
      }

      /*
      var color = color_map[z];

      for (var i=0; i<4; i++) {
         oneline_image.data[x*4+i] = ((color>>>0) >> ((3-i)*8)) & 0xff;
      }
      */
      
      /*
      if (dbgUs && wf_canvas_actual_line == 0) {
         oneline_image.data[x*4  ] = 0;
         oneline_image.data[x*4+1] = 0xff;
         oneline_image.data[x*4+2] = 0;
      } else {
      */
         oneline_image.data[x*4  ] = color_map_r[z];
         oneline_image.data[x*4+1] = color_map_g[z];
         oneline_image.data[x*4+2] = color_map_b[z];
      //}
      oneline_image.data[x*4+3] = 0xff;
   }
   
   if (clear_wfavg) clear_wfavg = false;
	
	// debug shear problems
	/*
   if (x_bin != x_bin_server) {
      for (x=450; x<456; x++) {
         oneline_image.data[x*4  ] = 255;
         oneline_image.data[x*4+1] = 0;
         oneline_image.data[x*4+2] = 0;
      }
   }
   if (x_bin_server_last != x_bin_server) {
      for (x=0; x<1024; x++) {
         x_bin_server_last = x_bin_server;
         oneline_image.data[x*4  ] = wf_swallow? 255:0;
         oneline_image.data[x*4+1] = 255;
         oneline_image.data[x*4+2] = 0;
      }
   }
   */

	canvas.ctx.putImageData(oneline_image, 0, wf_canvas_actual_line);
	
	if (audioFFT == 0)
	   w3_call('IBP_scan_plot', oneline_image);
	
	// call waterfall_timestamp()
	if (typeof(wfext) !== 'undefined' /* undeclared */ && wfext.tstamp) {
      var d = new Date();
      var secs = d.getUTCMinutes() * 60 + d.getUTCSeconds();
      var new_sec_mark = ((secs % wfext.tstamp) == 0);
      //console.log(wfext.tstamp +' '+ secs +' '+ new_sec_mark);
      if (wf.prev_sec_mark == false && new_sec_mark == true) waterfall_timestamp();
      wf.prev_sec_mark = new_sec_mark;
   }
	
	// If data from server hasn't caught up to our panning or zooming then fix it.
	// This code is tricky and full of corner-cases.
	
	var fixup = false;
	if (audioFFT == 0 && !wf.no_sync) {
      var pixel_dx;
      if (sb_trace)
         console.log('$WF fixup '+ ((x_bin != x_bin_server)? 'X':' ') + ((zoom_level != x_zoom_server)? 'Z':' ') +
            ' bin='+ x_bin +'|'+ x_bin_server +' z='+ zoom_level +'|'+ x_zoom_server +' preview='+ (kiwi.wf_preview_mode? 1:0));
   
      // need to fix zoom before fixing the pan
      if (zoom_level != x_zoom_server) {
         var dz = zoom_level - x_zoom_server;
         var out = dz < 0;
         var dbins = out? (x_bin_server - x_bin) : (x_bin - x_bin_server);
         pixel_dx = bins_to_pixels(2, dbins, out? zoom_level:x_zoom_server);
         if (sb_trace)
            console.log("WF Z fix z"+x_zoom_server+'>'+zoom_level+' out='+out+' b='+x_bin+'/'+x_bin_server+'/'+dbins+" px="+pixel_dx);
         waterfall_zoom(canvas, dz, wf_canvas_actual_line, pixel_dx);
         
         // x_bin_server has changed now that we've zoomed
         x_bin_server += out? -dbins : dbins;
         if (sb_trace) console.log('WF z fixed');
         fixup = true;
      }
      
      if (x_bin != x_bin_server) {
         pixel_dx = bins_to_pixels(3, x_bin - x_bin_server, zoom_level);
         if (sb_trace)
            console.log("WF bin fix L="+wf_canvas_actual_line+" xb="+x_bin+" xbs="+x_bin_server+" pdx="+pixel_dx);
         waterfall_pan(canvas, wf_canvas_actual_line, pixel_dx);
         if (sb_trace) console.log('WF bin fixed');
         fixup = true;
      }
   
      if (sb_trace && x_bin == x_bin_server && zoom_level == x_zoom_server) {
         console.log('--WF FIXUP ALL DONE--');
         sb_trace=0;
      }
   }

   //console.log('need_autoscale='+ wf.need_autoscale +' fixup='+ fixup);
   if (wf.need_autoscale > 1) wf.need_autoscale--;
   
	if (wf.need_autoscale == 1 && !fixup) {
	   var pwr_dBm = [];
      var autoscale = wf.audioFFT_active? data.slice(256, 768) : data;
	   var alen = autoscale.length;
	   var signal, noise;
	   
	   // convert from transmitted values to true dBm
	   var j = 0;
	   for (var i = 0; i < alen; i++) {
	      var pwr = dB_wire_to_dBm(autoscale[i]);
	      if (pwr <= -190) continue;    // disregard masked areas
	      pwr_dBm[j] = pwr;
	      j++;
      }
      var len = pwr_dBm.length;
      if (len) {
         pwr_dBm.sort(kiwi_sort_numeric);
         noise = pwr_dBm[Math.floor(0.50 * len)];
         signal = pwr_dBm[Math.floor(0.95 * len)];
         //console_log_dbgUs('# autoscale len='+ len +' min='+ pwr_dBm[0] +' noise='+ noise +' signal='+ signal +' max='+ pwr_dBm[len-1]);

         var _10 = pwr_dBm[Math.floor(0.10 * len)];
         var _20 = pwr_dBm[Math.floor(0.20 * len)];
         //console_log_dbgUs('# autoscale min='+ pwr_dBm[0] +' 10%='+ _10 +' 20%='+ _20 +' 50%(noise)='+ noise +' 95%(signal)='+ signal +' max='+ pwr_dBm[len-1]);
      } else {
         signal = -110;
         noise = -120;
         //console_log_dbgUs('# autoscale len=0 sig=-110 noise=-120');
      }
      
      // empirical adjustments
	   signal += 30;
	   if (signal < -80) signal = -80;
      noise -= 10;
      //console_log_dbgUs('# autoscale FINAL noise(min)='+ noise +' signal(max)='+ signal);
      
      if (wf.audioFFT_active) {
         //noise = (dbgUs && devl.p4)? Math.round(devl.p4) : -110;
         noise = -110;
         console_log_dbgUs('audioFFT_active: force noise = '+ noise.toFixed(0) +' dBm');
      }

      var auto = (wf.aper == kiwi.APER_AUTO);
      if (!auto && wf.FNAS) {
         //console.log('$$SKIP !auto && wf.FNAS');
      } else {
         setmaxdb(signal, {done:1, no_fsel:auto});
         setmindb(noise, {done:1, no_fsel:auto});
         update_maxmindb_sliders();
      }
	   wf.need_autoscale = wf.FNAS = 0;
	}

	wf_canvas_actual_line--;
	wf_shift_canvases();
	if (wf_canvas_actual_line < 0) add_wf_canvas();
	
	if (kiwi_gc_wf) canvas = data_raw = data_arr_u8 = decomp_data = data = null;	// gc
}


//
// NB The panning and zooming code is tricky and full of corner-cases.
// It *still* probably has bugs.
//

function waterfall_pan(cv, line, dx)
{
	var ctx = cv.ctx;
	var y, w, h;
	var w0 = cv.width;
	
	if (line == -1) {
		y = 0;
		h = cv.height;
	} else {
		y = line;
		h = 1;
	}

	// fillRect() does not give expected result if coords are negative, so clip first
	var clip = function(v, min, max) { return ((v < min)? min : ((v > max)? max : v)); };

	try {
		if (sb_trace) console.log('waterfall_pan h='+ h +' dx='+ dx);
		
		if (dx < 0) {		// pan right (toward higher freqs)
			dx = -dx;
			w = w0-dx;
			if (sb_trace) console.log('PAN-L w='+ w +' dx='+ dx);
			if (w > 0) ctx.drawImage(cv, 0,y,w,h, dx,y,w,h);
			ctx.fillStyle = "Black";
			if (sb_trace) ctx.fillStyle = (line==-1)? "Lime":"Red";
			fx = clip(dx, 0, w0);
			ctx.fillRect(0,y, fx,h);
		} else {				// pan left (toward lower freqs)
			w = w0-dx;
			if (sb_trace) console.log('PAN-R w='+ w +' dx='+ dx);
			if (w > 0) ctx.drawImage(cv, dx,y,w,h, 0,y,w,h);
			ctx.fillStyle = "Black";
			if (sb_trace) ctx.fillStyle = (line==-1)? "Lime":"Red";
			fx = clip(w, 0, w0);
			ctx.fillRect(fx,y, w0,h);
		}
	} catch(ex) {
		console.log('EX WFPAN '+ ex.toString() +' dx='+ dx +' y='+ y +' w='+ w +' h='+ h);
	}
}

// have to keep track of fractional pixels while panning
var last_pixels_frac = 0;

function waterfall_pan_canvases(bins)
{
	if (!bins) return;
   if (wf.audioFFT_active) return;
	
	var x_obin = x_bin;
	x_bin += bins;
	x_bin = clamp_xbin(x_bin);
	//console.log("PAN lpf="+last_pixels_frac);
	var f_dx = bins_to_pixels_frac(4, x_bin - x_obin, zoom_level) + last_pixels_frac;		// actual movement allowed due to clamping
	var i_dx = Math.round(f_dx);
	last_pixels_frac = f_dx - i_dx;
	if (sb_trace) console.log("PAN-CAN z="+zoom_level+" xb="+x_bin+" db="+(x_bin - x_obin)+" f_dx="+f_dx+" i_dx="+i_dx+" lpf="+last_pixels_frac);
	if (!i_dx) {
	   //if (x_bin != x_obin) console.log('$i_dx == 0: x_bin='+ x_bin +' x_obin='+ x_obin +' dx='+ (x_bin - x_obin));
	   wf_send("SET zoom="+ zoom_level +" start="+ x_bin);
	   return;
	}

	wf_canvases.forEach(function(cv) {
		waterfall_pan(cv, -1, i_dx);
	});
	
	//console.log('$waterfall_pan_canvases SET Z'+ zoom_level +' start='+ x_bin);
	wf_send("SET zoom="+ zoom_level +" start="+ x_bin);
	
	mkscale();
	need_clear_wf_sp_avg = true;
	dx_schedule_update();
   extint_environment_changed( { waterfall_pan:1, passband_screen_location:1 } );

	// reset "select band" menu if freq is no longer inside band
	//console.log('page_scroll PBV='+ (passband_visible().visible) +' freq_car_Hz='+ freq_car_Hz);
	if (!passband_visible().visible)
		check_band(true);
}

function waterfall_zoom(cv, dz, line, x)
{
	var ctx = cv.ctx;
	var w = cv.width;
	var zf = 1 << Math.abs(dz);
	var pw = w / zf;
	var fx;
	var y, h;
	
	// fillRect() does not give expected result if coords are negative, so clip first
	var clip = function(v, min, max) { return ((v < min)? min : ((v > max)? max : v)); };
	
	if (line == -1) {
		y = 0;
		h = cv.height;
	} else {
		y = line;
		h = 1;
	}

	try {
		if (sb_trace) console.log('waterfall_zoom w='+ w +' h='+ h +' pw='+ pw +' zf='+ zf);

		if (dz < 0) {		// zoom out
			if (sb_trace) console.log('WFZ-out srcX=0/'+ w +' dstX='+ x +'/'+ (x+pw) +' (pw='+ pw +')');
			
			ctx.drawImage(cv, 0,y,w,h, x,y,pw,h);
			ctx.fillStyle = "Black";
			if (sb_trace) {
				console.log('chocolate 0:'+ x);
				ctx.fillStyle = "chocolate";
			}
			fx = clip(x, 0, w);
			ctx.fillRect(0,y, fx,y+h);
			if (sb_trace) {
				console.log('brown '+ (x+pw) +':'+ w);
				ctx.fillStyle = "brown";
			}
			fx = clip(x+pw, 0, w);
			ctx.fillRect(fx,y, w,y+h);
		} else {			// zoom in
			if (w != 0) {
				if (sb_trace) console.log('WFZ-in srcX='+ x +'/'+ (x+pw) +'(pw='+ pw +') dstX=0/'+ w);
				var fill_hi = false, fill_lo = false;
				if ((x+pw) > w) {
					fx = Math.round((w-x) * zf);
					if (sb_trace)
						console.log('CLAMP HI fx='+ fx);
					fill_hi = true;
				}
				if (x < 0) {
					fx = Math.round(-x * zf);
					if (sb_trace)
						console.log('CLAMP LO fx='+ fx);
					fill_lo = true;
				}
				
				ctx.drawImage(cv, x,y,pw,h, 0,y,w,h);
				ctx.fillStyle = "Black";
				if (sb_trace) ctx.fillStyle = "deepPink";
				if (fill_hi) ctx.fillRect(fx,y, w,y+h);
				if (fill_lo) ctx.fillRect(0,y, fx,y+h);
			}
		}
	} catch(ex) {
		console.log('EX WFZ '+ ex.toString() +' dz='+ dz +' x='+ x +' y='+ y +' w='+ w +' h='+ h);
	}
}

function waterfall_zoom_canvases(dz, x)
{
	if (sb_trace) console.log("ZOOM-CAN z"+zoom_level+" xb="+x_bin+" x="+x);

	wf_canvases.forEach(function(cv) {
		waterfall_zoom(cv, dz, -1, x);
	});
	
	need_clear_wf_sp_avg = true;
}

function waterfall_position(from, pos, freq_kHz)
{
   /*
      Trying to keep PB centered all the time has the undesirable side-effect
      of invalidating more of the WF on small frequency changes than the
      "scroll WF when PB in L/R margins" scheme we ended up implementing.
      
      if (from == owrx.FSET_DTOUCH_DRAG && mobileOptNew()) {
         pos = owrx.WF_POS_CENTER;     // passband always centered
         //kiwi_trace('waterfall_position');
      }
   */
   
   //console.log('waterfall_position='+ pos +' freq_kHz='+ freq_kHz +' freq_passband_center='+ (freq_passband_center()/1e3).toFixed(3));
   //kiwi_trace('waterfall_position');
   var wf_middle_bin = x_bin + bins_at_cur_zoom()/2;

   if (pos == owrx.WF_POS_AT_FREQ) {
      freq_kHz = freq_kHz || 1e3;
      var dbin = freq_to_bin(freq_kHz*1e3) - wf_middle_bin;    // < 0 = pan left (toward lower freqs)
      waterfall_pan_canvases(dbin);
   } else {
      var pv = passband_visible();
      if (pos == owrx.WF_POS_CENTER || (pos == owrx.WF_POS_RECENTER_IF_OUTSIDE && !pv.inside)) {
         var bins_to_recenter = pv.bin - wf_middle_bin;
         //console.log('RECEN YES pv.bin='+ pv.bin +' wfm='+ wf_middle_bin +' bins_to_recenter='+ bins_to_recenter +' z='+ zoom_level +' x_bin='+ x_bin +' bacz='+ bins_at_cur_zoom());
         waterfall_pan_canvases(bins_to_recenter);    // < 0 = pan left (toward lower freqs)
      }
   }
	
	// reset "select band" menu if freq is no longer inside band
	check_band();
}

function waterfall_tune(f_kHz)
{
   //console.log('waterfall_tune f_kHz='+ f_kHz);
   if (f_kHz == 0) {
      waterfall_position(owrx.FSET_SET_FREQ, owrx.WF_POS_AT_FREQ, +ext_get_freq_kHz());
   } else {
      waterfall_position(owrx.FSET_SET_FREQ, owrx.WF_POS_AT_FREQ, f_kHz);
      owrx.waterfall_tuned = 1;
   }
   w3_field_select('id-freq-input', {mobile:1, log:2});
   freqset_restore_ui();
}

// window:
//		top container
//		non-waterfall container
//		waterfall container
//
// need this because "w3_el('id-waterfall-container').clientHeight" includes an off-screen portion

function waterfall_height()
{
	var top_height = w3_el("id-top-container").clientHeight;
	var non_waterfall_height = w3_el("id-non-waterfall-container").clientHeight;
	var scale_height = w3_el("id-scale-container").clientHeight;
	owrx.scale_offsetY = top_height + non_waterfall_height - scale_height;
	//console.log(owrx.scale_offsetY +'='+ top_height +'+'+ non_waterfall_height +'-'+ scale_height);

	var wf_height = window.innerHeight - top_height - non_waterfall_height;
	//console.log('## waterfall_height: wf_height='+ wf_height +' winh='+ window.innerHeight +' th='+ top_height +' nh='+ non_waterfall_height);
	//console.log('## waterfall_height: scale_offsetY='+ owrx.scale_offsetY);
	return wf_height;
}

// resize_waterfall_container() can be called repeatedly while top photo sliding animation is ongoing.
// With certain screen sizes it is possible for wf_height to be negative for a period of time, so very
// important to set waterfall_scrollable_height = 0 in that case.

function resize_waterfall_container(check_init)
{
	if (check_init && !waterfall_setup_done) return;

	var wf_height = waterfall_height();

	if (wf_height >= 0) {
	
	   // canvas_annotation has to track wf_container height so as not to generate undesired mouseout events
		canvas_annotation.style.height = wf_container.style.height = px(wf_height);
		canvas_annotation.height = wf_height;
		waterfall_scrollable_height = wf_height * wf.scroll_multiple;
	} else {
		waterfall_scrollable_height = 0;
	}

	//console.log('## wf_h='+ wf_height +' wsh='+ waterfall_scrollable_height);
}

var waterfall_delay = 0;
var waterfall_queue = [];
var waterfall_last_add = 0;

function waterfall_add_queue(what, ws, firstChars)
{
   if (!kiwi.wf_preview_mode)
      waterfall_add_queue2(what, ws, firstChars);
   else
	   if (kiwi_gc_wf) what = null;  // gc
}

function waterfall_add_queue2(what, ws, firstChars)
{
   if (firstChars == 'DAT') {
      var u8View = new Uint8Array(what, 4);
      var cmd = u8View[0];
      if (cmd == 0) {
         for (var i = 0; i < 3*256; i++) {
            downloaded_colormap[i] = u8View[i+1];
         }
         mkcolormap();
         console.log('W/F DAT downloaded_colormap');
      } else
         console.log('W/F DAT unknown cmd='+ cmd);
      return;
   }
   
	var u32View = new Uint32Array(what, 4, 3);
	var seq = u32View[2];
	if (kiwi_gc_wf) u32View = null;	// gc

	var now = Date.now();
	var spacing = waterfall_last_add? (now - waterfall_last_add) : 0;
	waterfall_last_add = now;

	waterfall_queue.push({ data:what, audioFFT:0, seq:seq, spacing:spacing });
	if (kiwi_gc_wf) what = null;	// gc
}

var init_zoom_set = false;
var waterfall_last_out = 0;
var wf_dq_onesec = 0;

function waterfall_dequeue()
{
	/*
      wf_dq_onesec += waterfall_ms;
      if (wf_dq_onesec >= 1000) {
         console.log('WF Q='+ waterfall_queue.length);
         wf_dq_onesec = 0;
      }
	*/
	
	// demodulator must have been initialized before calling zoom_step()
	if (!init_zoom_set && demodulators[0]) {
		init_zoom = parseInt(init_zoom);
		if (init_zoom < 0 || init_zoom > zoom_levels_max) init_zoom = 0;
		//console.log("### init_zoom="+init_zoom);
		zoom_step(ext_zoom.ABS, init_zoom);
		init_zoom_set = true;
	}

	if (!waterfall_setup_done || waterfall_queue.length == 0) return;
	
	// synchronize waterfall to audio
	while (waterfall_queue.length != 0) {
         var now = Date.now();

      if (!wf.no_sync) {
         var seq = waterfall_queue[0].seq;
         var target = audio_ext_sequence + waterfall_delay;
         if (seq > target) {
            //console.log('SEQ too soon: s'+ seq +' > t'+ target +' ('+ (seq - target) +')');
            return;		// too soon
         }

         if (seq == audio_ext_sequence && now < (waterfall_last_out + waterfall_queue[0].spacing)) {
            //console.log('SEQ need spacing');
            //console.log('SEQ need spacing: s'+ seq +' sp'+ waterfall_queue[0].spacing +' ('+ (now - waterfall_last_out) +')');
            return;		// need spacing
         }
      }
	
		// seq < audio_ext_sequence or seq == audio_ext_sequence and spacing is okay
		waterfall_last_out = now;
		
		var data = waterfall_queue[0].data;
		if (kiwi_gc_wf) waterfall_queue[0].data = null;	// gc
		var audioFFT = waterfall_queue[0].audioFFT;
		waterfall_queue.shift();
		waterfall_add(data, audioFFT);
		if (kiwi_gc_wf) data = null;	// gc
	}
}

var downloaded_colormap = new Uint8Array(3*256);

// google turbo color_map, grab from here: https://gist.github.com/mikhailov-work/ee72ba4191942acecc03fe6da94fc73f
turbo_colormap_data = [0.18995,0.07176,0.23217,0.19483,0.08339,0.26149,0.19956,0.09498,0.29024,0.20415,0.10652,0.31844,0.20860,0.11802,0.34607,0.21291,0.12947,0.37314,0.21708,0.14087,0.39964,0.22111,0.15223,0.42558,0.22500,0.16354,0.45096,0.22875,0.17481,0.47578,0.23236,0.18603,0.50004,0.23582,0.19720,0.52373,0.23915,0.20833,0.54686,0.24234,0.21941,0.56942,0.24539,0.23044,0.59142,0.24830,0.24143,0.61286,0.25107,0.25237,0.63374,0.25369,0.26327,0.65406,0.25618,0.27412,0.67381,0.25853,0.28492,0.69300,0.26074,0.29568,0.71162,0.26280,0.30639,0.72968,0.26473,0.31706,0.74718,0.26652,0.32768,0.76412,0.26816,0.33825,0.78050,0.26967,0.34878,0.79631,0.27103,0.35926,0.81156,0.27226,0.36970,0.82624,0.27334,0.38008,0.84037,0.27429,0.39043,0.85393,0.27509,0.40072,0.86692,0.27576,0.41097,0.87936,0.27628,0.42118,0.89123,0.27667,0.43134,0.90254,0.27691,0.44145,0.91328,0.27701,0.45152,0.92347,0.27698,0.46153,0.93309,0.27680,0.47151,0.94214,0.27648,0.48144,0.95064,0.27603,0.49132,0.95857,0.27543,0.50115,0.96594,0.27469,0.51094,0.97275,0.27381,0.52069,0.97899,0.27273,0.53040,0.98461,0.27106,0.54015,0.98930,0.26878,0.54995,0.99303,0.26592,0.55979,0.99583,0.26252,0.56967,0.99773,0.25862,0.57958,0.99876,0.25425,0.58950,0.99896,0.24946,0.59943,0.99835,0.24427,0.60937,0.99697,0.23874,0.61931,0.99485,0.23288,0.62923,0.99202,0.22676,0.63913,0.98851,0.22039,0.64901,0.98436,0.21382,0.65886,0.97959,0.20708,0.66866,0.97423,0.20021,0.67842,0.96833,0.19326,0.68812,0.96190,0.18625,0.69775,0.95498,0.17923,0.70732,0.94761,0.17223,0.71680,0.93981,0.16529,0.72620,0.93161,0.15844,0.73551,0.92305,0.15173,0.74472,0.91416,0.14519,0.75381,0.90496,0.13886,0.76279,0.89550,0.13278,0.77165,0.88580,0.12698,0.78037,0.87590,0.12151,0.78896,0.86581,0.11639,0.79740,0.85559,0.11167,0.80569,0.84525,0.10738,0.81381,0.83484,0.10357,0.82177,0.82437,0.10026,0.82955,0.81389,0.09750,0.83714,0.80342,0.09532,0.84455,0.79299,0.09377,0.85175,0.78264,0.09287,0.85875,0.77240,0.09267,0.86554,0.76230,0.09320,0.87211,0.75237,0.09451,0.87844,0.74265,0.09662,0.88454,0.73316,0.09958,0.89040,0.72393,0.10342,0.89600,0.71500,0.10815,0.90142,0.70599,0.11374,0.90673,0.69651,0.12014,0.91193,0.68660,0.12733,0.91701,0.67627,0.13526,0.92197,0.66556,0.14391,0.92680,0.65448,0.15323,0.93151,0.64308,0.16319,0.93609,0.63137,0.17377,0.94053,0.61938,0.18491,0.94484,0.60713,0.19659,0.94901,0.59466,0.20877,0.95304,0.58199,0.22142,0.95692,0.56914,0.23449,0.96065,0.55614,0.24797,0.96423,0.54303,0.26180,0.96765,0.52981,0.27597,0.97092,0.51653,0.29042,0.97403,0.50321,0.30513,0.97697,0.48987,0.32006,0.97974,0.47654,0.33517,0.98234,0.46325,0.35043,0.98477,0.45002,0.36581,0.98702,0.43688,0.38127,0.98909,0.42386,0.39678,0.99098,0.41098,0.41229,0.99268,0.39826,0.42778,0.99419,0.38575,0.44321,0.99551,0.37345,0.45854,0.99663,0.36140,0.47375,0.99755,0.34963,0.48879,0.99828,0.33816,0.50362,0.99879,0.32701,0.51822,0.99910,0.31622,0.53255,0.99919,0.30581,0.54658,0.99907,0.29581,0.56026,0.99873,0.28623,0.57357,0.99817,0.27712,0.58646,0.99739,0.26849,0.59891,0.99638,0.26038,0.61088,0.99514,0.25280,0.62233,0.99366,0.24579,0.63323,0.99195,0.23937,0.64362,0.98999,0.23356,0.65394,0.98775,0.22835,0.66428,0.98524,0.22370,0.67462,0.98246,0.21960,0.68494,0.97941,0.21602,0.69525,0.97610,0.21294,0.70553,0.97255,0.21032,0.71577,0.96875,0.20815,0.72596,0.96470,0.20640,0.73610,0.96043,0.20504,0.74617,0.95593,0.20406,0.75617,0.95121,0.20343,0.76608,0.94627,0.20311,0.77591,0.94113,0.20310,0.78563,0.93579,0.20336,0.79524,0.93025,0.20386,0.80473,0.92452,0.20459,0.81410,0.91861,0.20552,0.82333,0.91253,0.20663,0.83241,0.90627,0.20788,0.84133,0.89986,0.20926,0.85010,0.89328,0.21074,0.85868,0.88655,0.21230,0.86709,0.87968,0.21391,0.87530,0.87267,0.21555,0.88331,0.86553,0.21719,0.89112,0.85826,0.21880,0.89870,0.85087,0.22038,0.90605,0.84337,0.22188,0.91317,0.83576,0.22328,0.92004,0.82806,0.22456,0.92666,0.82025,0.22570,0.93301,0.81236,0.22667,0.93909,0.80439,0.22744,0.94489,0.79634,0.22800,0.95039,0.78823,0.22831,0.95560,0.78005,0.22836,0.96049,0.77181,0.22811,0.96507,0.76352,0.22754,0.96931,0.75519,0.22663,0.97323,0.74682,0.22536,0.97679,0.73842,0.22369,0.98000,0.73000,0.22161,0.98289,0.72140,0.21918,0.98549,0.71250,0.21650,0.98781,0.70330,0.21358,0.98986,0.69382,0.21043,0.99163,0.68408,0.20706,0.99314,0.67408,0.20348,0.99438,0.66386,0.19971,0.99535,0.65341,0.19577,0.99607,0.64277,0.19165,0.99654,0.63193,0.18738,0.99675,0.62093,0.18297,0.99672,0.60977,0.17842,0.99644,0.59846,0.17376,0.99593,0.58703,0.16899,0.99517,0.57549,0.16412,0.99419,0.56386,0.15918,0.99297,0.55214,0.15417,0.99153,0.54036,0.14910,0.98987,0.52854,0.14398,0.98799,0.51667,0.13883,0.98590,0.50479,0.13367,0.98360,0.49291,0.12849,0.98108,0.48104,0.12332,0.97837,0.46920,0.11817,0.97545,0.45740,0.11305,0.97234,0.44565,0.10797,0.96904,0.43399,0.10294,0.96555,0.42241,0.09798,0.96187,0.41093,0.09310,0.95801,0.39958,0.08831,0.95398,0.38836,0.08362,0.94977,0.37729,0.07905,0.94538,0.36638,0.07461,0.94084,0.35566,0.07031,0.93612,0.34513,0.06616,0.93125,0.33482,0.06218,0.92623,0.32473,0.05837,0.92105,0.31489,0.05475,0.91572,0.30530,0.05134,0.91024,0.29599,0.04814,0.90463,0.28696,0.04516,0.89888,0.27824,0.04243,0.89298,0.26981,0.03993,0.88691,0.26152,0.03753,0.88066,0.25334,0.03521,0.87422,0.24526,0.03297,0.86760,0.23730,0.03082,0.86079,0.22945,0.02875,0.85380,0.22170,0.02677,0.84662,0.21407,0.02487,0.83926,0.20654,0.02305,0.83172,0.19912,0.02131,0.82399,0.19182,0.01966,0.81608,0.18462,0.01809,0.80799,0.17753,0.01660,0.79971,0.17055,0.01520,0.79125,0.16368,0.01387,0.78260,0.15693,0.01264,0.77377,0.15028,0.01148,0.76476,0.14374,0.01041,0.75556,0.13731,0.00942,0.74617,0.13098,0.00851,0.73661,0.12477,0.00769,0.72686,0.11867,0.00695,0.71692,0.11268,0.00629,0.70680,0.10680,0.00571,0.69650,0.10102,0.00522,0.68602,0.09536,0.00481,0.67535,0.08980,0.00449,0.66449,0.08436,0.00424,0.65345,0.07902,0.00408,0.64223,0.07380,0.00401,0.63082,0.06868,0.00401,0.61923,0.06367,0.00410,0.60746,0.05878,0.00427,0.59550,0.05399,0.00453,0.58336,0.04931,0.00486,0.57103,0.04474,0.00529,0.55852,0.04028,0.00579,0.54583,0.03593,0.00638,0.53295,0.03169,0.00705,0.51989,0.02756,0.00780,0.50664,0.02354,0.00863,0.49321,0.01963,0.00955,0.47960,0.01583,0.01055];

// adjusted so the overlaid white text of the spectrum scale doesn't get washed out
var spectrum_scale_color_map_transparency = 200;

var color_map = new Uint32Array(256);
var color_map_transparent = new Uint32Array(256);
var color_map_r = new Uint8Array(256);
var color_map_g = new Uint8Array(256);
var color_map_b = new Uint8Array(256);

function mkcolormap()
{
   var ccm;
   if (wf.cmap >= kiwi.cmap_e.custom_1)
      ccm = wf.custom_colormaps[wf.cmap - kiwi.cmap_e.custom_1];
   
	for (var i=0; i<256; i++) {
		var r0, g0, b0, s0, s1;
		var r, g, b;
		
		switch (wf.cmap) {
		
		case kiwi.cmap_e.kiwi:
			// new default
			if (i < 32) {
				r = 0; g = 0; b = i*255/31;
			} else if (i < 72) {
				r = 0; g = (i-32)*255/39; b = 255;
			} else if (i < 96) {
				r = 0; g = 255; b = 255-(i-72)*255/23;
			} else if (i < 116) {
				r = (i-96)*255/19; g = 255; b = 0;
			} else if (i < 184) {
				r = 255; g = 255-(i-116)*255/67; b = 0;
			} else {
				r = 255; g = 0; b = (i-184)*128/70;
			}
			break;

		case kiwi.cmap_e.CuteSDR:
			// old one from CuteSDR
			if (i<43) {
				r = 0; g = 0; b = i*255/43;
			} else if (i<87) {
				r = 0; g = (i-43)*255/43; b = 255;
			} else if (i<120) {
				r = 0; g = 255; b = 255-(i-87)*255/32;
			} else if (i<154) {
				r = (i-120)*255/33; g = 255; b = 0;
			} else if (i<217) {
				r = 255; g = 255-(i-154)*255/62; b = 0;
			} else {
				r = 255; g = 0; b = (i-217)*128/38;
			}
			break;

		case kiwi.cmap_e.greyscale:
			// greyscale
			black_level = 0.0;
			white_level = 48.0;
			var v = black_level + ((255 - black_level + white_level) * i/255);
			r = g = b = v;
			break;
			
		case kiwi.cmap_e.linear:
			var c_from = { r:255, g:0, b:255 };
			var c_to = { r:0, g:255, b:0 };
			var s = 1.0;
			r = c_from.r * (i/255) + c_to.r * ((255-i)/255); r = r * (s + (i/255) * (1-s));
			g = c_from.g * (i/255) + c_to.g * ((255-i)/255); g = g * (s + (i/255) * (1-s));
			b = c_from.b * (i/255) + c_to.b * ((255-i)/255); b = b * (s + (i/255) * (1-s));
			break;

		case kiwi.cmap_e.turbo:
			r = turbo_colormap_data[i * 3 + 0] * 255;
			g = turbo_colormap_data[i * 3 + 1] * 255;
			b = turbo_colormap_data[i * 3 + 2] * 255;
			break;
		
		case kiwi.cmap_e.SdrDx:
			/*
			   from: fyngyrz.com/sdrdxdoc/waterfall2apf.html
            0 0.0 0.0 0.0 
            45 0.0 0.0 0.25 
            60 0.0 0.0 0.5 
            75 1.0 1.0 0.0 
            90 1.0 0.0 0.0 
            105 1.0 1.0 1.0

            original above was too compressed, so what we use is expanded a bit:
            0 0.0 0.0 0.0 
            20 0.0 0.0 0.5 
            60 0.0 0.0 1.0 
            150 1.0 1.0 0.0 
            180 1.0 0.0 0.0 
            210 1.0 1.0 1.0
         */
         
			if (i <= 20) {
				r0 = 0; g0 = 0; b0 = 0;
				r = 0; g = 0; b = 0.5;
				s0 = 0;
				s1 = 20;
			} else
			if (i <= 60) {
				r0 = 0; g0 = 0; b0 = 0.5;
				r = 0; g = 0; b = 1.0;
				s0 = 21;
				s1 = 60;
			} else
			if (i <= 150) {
				r0 = 0; g0 = 0; b0 = 1.0;
				r = 1.0; g = 1.0; b = 0;
				s0 = 61;
				s1 = 150;
			} else
			if (i <= 180) {
				r0 = 1.0; g0 = 1.0; b0 = 0;
				r = 1.0; g = 0; b = 0;
				s0 = 151;
				s1 = 180;
			} else
			if (i <= 210) {
				r0 = 1.0; g0 = 0; b0 = 0;
				r = 1.0; g = 1.0; b = 1.0;
				s0 = 181;
				s1 = 210;
			} else {
				r0 = 1.0; g0 = 1.0; b0 = 1.0;
				r = 1.0; g = 1.0; b = 1.0;
				s0 = 211;
				s1 = 255;
			}

         var d = s1 - s0;
         var ni = i - s0;
         var f = ni / d;
		
         r = (r0 + ((r - r0) * f)) * 255;
         g = (g0 + ((g - g0) * f)) * 255;
         b = (b0 + ((b - b0) * f)) * 255;

         //if (wf.cmap == kiwi.cmap_e.SdrDx)
         //   console.log(i +' s0='+ s0 +' s1='+ s1 +' f='+ f.toFixed(3) +' r='+ r.toFixed(0) +' g='+ g.toFixed(0) +' b='+ b.toFixed(0));
			break;

		default:
		   /*
			r = downloaded_colormap[i * 3 + 0];
			g = downloaded_colormap[i * 3 + 1];
			b = downloaded_colormap[i * 3 + 2];
			*/
			
			r = ccm[i * 3 + 0];
			g = ccm[i * 3 + 1];
			b = ccm[i * 3 + 2];
			break;
		}
		
		r = Math.round(r); r = w3_clamp(r, 0, 255);
		g = Math.round(g); g = w3_clamp(g, 0, 255);
		b = Math.round(b); b = w3_clamp(b, 0, 255);

      // composite colormap used by spectrum
		color_map[i] = (r<<24) | (g<<16) | (b<<8) | 0xff;
		color_map_transparent[i] = (r<<24) | (g<<16) | (b<<8) | spectrum_scale_color_map_transparency;
		
		// component colormap for waterfall (also used by some extensions)
		color_map_r[i] = r;
		color_map_g[i] = g;
		color_map_b[i] = b;
	}
}

function dB_wire_to_dBm(db_value)
{
	// What is transmitted over the network are unsigned 55..255 values (compressed) which
	// correspond to -200..0 dBm. Convert here to back to <= 0 signed dBm.
	// Done this way because -127 is the smallest value in an int8 which isn't enough
	// to hold our smallest dBm value and also we don't expect dBm values > 0 to be needed.
	//
   // We map 0..-200 dBm to (u1_t) 255..55
   // If we map it the reverse way, (u1_t) 0..255 => 0..-255 dBm (which is more natural), then the
   // noise in the bottom bits due to the ADPCM compression will effect the high-order dBm bits
   // which is bad.
	
	if (db_value < 0) db_value = 0;
	if (db_value > 255) db_value = 255;
	var dBm = -(255 - db_value);
	return (dBm + wf.cal);
}

function color_index(db_value, sqrt)
{
   var dBm = dB_wire_to_dBm(db_value);
	if (dBm < mindb) dBm = mindb;
	if (dBm > maxdb) dBm = maxdb;
	var relative_value = dBm - mindb;

	var value_percent_default = relative_value/full_scale;
	var value_percent = value_percent_default;
	
	if (isDefined(sqrt)) {
      try {
         switch (+sqrt) {
      
         //case 0:
         default:
            break;
         case 1:
            value_percent = Math.sqrt(value_percent_default);
            break;
         case 2:
            if (value_percent_default > 0.21 && value_percent_default < 0.5)
               value_percent = 0.2 + (4 + Math.log10(value_percent_default - 0.2)) * 0.09;
            break;
         case 3:
            if (value_percent_default > 0.31 && value_percent_default < 0.6)
               value_percent = 0.3 + (5 + Math.log10(value_percent_default - 0.3)) * 0.07;
            break;
         case 4:
            if (value_percent_default > 0.41 && value_percent_default < 0.7)
               value_percent = 0.4 + (6 + Math.log10(value_percent_default - 0.4)) * 0.055;
            break;
         }
      } catch(ex) {
         value_percent = value_percent_default;
      }
   }
	
	var i = value_percent * 255;
	i = Math.round(i);
	if (i < 0) i = 0;
	if (i > 255) i = 255;
	return i;
}


// waterfall_color_index_max_min() and color_index_max_min() used by iq_display.js and FFT.js

function waterfall_color_index_max_min(db_value, maxdb, mindb)
{
	// convert to negative-only signed dBm (i.e. -256 to -1 dBm)
	// done this way because -127 is the smallest value in an int8 which isn't enough
	db_value = -(255 - db_value);		
	
	if (db_value < mindb) db_value = mindb;
	if (db_value > maxdb) db_value = maxdb;
	var relative_value = db_value - mindb;
	var fullscale = maxdb - mindb;
	fullscale = fullscale? fullscale : 1;	// can't be zero
	var value_percent = relative_value / fullscale;
	
	var i = value_percent * 255;
	i = Math.round(i);
	if (i < 0) i = 0;
	if (i > 255) i = 255;
	return i;
}

function color_index_max_min(value, maxdb, mindb)
{
	if (value < mindb) value = mindb;
	if (value > maxdb) value = maxdb;
	var relative_value = value - mindb;
	var fullscale = maxdb - mindb;
	fullscale = fullscale? fullscale : 1;	// can't be zero
	var value_percent = relative_value / fullscale;
	
	var i = value_percent * 255;
	i = Math.round(i);
	if (i < 0) i = 0;
	if (i > 255) i = 255;
	return i;
}

/* not used
//var color_scale=[0xFFFFFFFF, 0x000000FF];
//var color_scale=[0x000000FF, 0x000000FF, 0x3a0090ff, 0x10c400ff, 0xffef00ff, 0xff5656ff];
//var color_scale=[0x000000FF, 0x000000FF, 0x534b37ff, 0xcedffaff, 0x8899a9ff,  0xfff775ff, 0xff8a8aff, 0xb20000ff];

//var color_scale=[ 0x000000FF, 0xff5656ff, 0xffffffff];

//2014-04-22
var color_scale=[0x2e6893ff, 0x69a5d0ff, 0x214b69ff, 0x9dc4e0ff,  0xfff775ff, 0xff8a8aff, 0xb20000ff];

function waterfall_mkcolor(db_value)
{
	if (db_value < mindb) db_value = mindb;
	if (db_value > maxdb) db_value = maxdb;
	var relative_value = db_value - mindb;
	var value_percent = relative_value/full_scale;
	
	percent_for_one_color=1/(color_scale.length-1);
	index=Math.floor(value_percent/percent_for_one_color);
	remain=(value_percent-percent_for_one_color*index)/percent_for_one_color;
	return color_between(color_scale[index+1],color_scale[index],remain);
}

function color_between(first, second, percent)
{
	output=0;
	for (var i=0;i<4;i++)
	{
		add = ((((first&(0xff<<(i*8)))>>>0)*percent) + (((second&(0xff<<(i*8)))>>>0)*(1-percent))) & (0xff<<(i*8));
		output |= add>>>0;
	}
	return output>>>0;
}
*/
	

////////////////////////////////
// #audio FFT
////////////////////////////////

var afft = {
   init: false,
   comp_1x: false,
   prev_lsb: false,
   audioFFT_dynload: {},

   offt: 0,
   i_re: 0,
   i_im: 0,
   o_re: 0,
   o_im: 0,
   window_512: [],
   window_1k: [],
   window_2k: [],
   pwr_dB: [],
   dBi: [],

   CUTESDR_MAX_VAL: 32767,
};

function audioFFT_setup()
{
   if (!wf.audioFFT_active) return;
   zoom_level = 0;

   if (wf.aper != kiwi.APER_AUTO) {
      var last_AF_max_dB = kiwi_storeRead('last_AF_max_dB', maxdb);
      var last_AF_min_dB = kiwi_storeRead('last_AF_min_dB', mindb_un);
      setmaxdb(last_AF_max_dB, {done:1});
      setmindb(last_AF_min_dB, {done:1});
      update_maxmindb_sliders();
   }

   // Hanning
   var window = function(i, nsamp) {
      return (0.5 - 0.5 * Math.cos((2 * Math.PI * i)/(nsamp-1)));
   };

   for (i = 0; i < 512; i++) afft.window_512[i] = window(i, 512);
   for (i = 0; i < 1024; i++) afft.window_1k[i] = window(i, 1024);
   for (i = 0; i < 2048; i++) afft.window_2k[i] = window(i, 2048);
}

function audioFFT_update()
{
   mkscale();
   dx_schedule_update();
   freq_link_update();
   w3_innerHTML('id-nav-optbar-wf', 'WF');
   freqset_select();

   // clear waterfall
   if (wf.audioFFT_clear_wf) {
      wf_canvases.forEach(function(cv) {
         cv.ctx.fillStyle = "Black";
         cv.ctx.fillRect(0,0, cv.width,cv.height);
      });
      spec.need_clear_avg = true;
      wf.audioFFT_clear_wf = false;
   }
}

function wf_audio_FFT(audio_data, samps)
{
   if (!wf.audioFFT_active || !isDefined(cur_mode)) return;
   
   if (!kiwi_load_js_polled(afft.audioFFT_dynload, 'pkgs/js/Ooura_FFT32.js')) return;
   
   var i, j, k, ki;
   
   //console.log('iq='+ audio_mode_iq +' comp='+ audio_compression +' samps='+ samps);

   var iq = ext_is_IQ_or_stereo_curmode();
   var lsb = ext_mode(cur_mode).LSB_SAL;
   var fft_setup = (!afft.init || iq != afft.iq || audio_compression != afft.comp);

   if (fft_setup) {
      console.log('audio_FFT: SWITCHING iq='+ iq +' comp='+ audio_compression);
      var type;
      if (iq) {
         type = 'complex';
         afft.size = 1024;
         afft.offt = ooura32.FFT(afft.size, {"type":"complex", "radix":4});
         afft.i_re = ooura32.vectorArrayFactory(afft.offt);
         afft.i_im = ooura32.vectorArrayFactory(afft.offt);
      } else {
         type = 'real';
         afft.size = audio_compression? (afft.comp_1x? 2048 : 1024) : 512;
         afft.offt = ooura32.FFT(afft.size, {"type":"real", "radix":4});
         afft.i_re = ooura32.scalarArrayFactory(afft.offt);
      }
      afft.o_re = ooura32.vectorArrayFactory(afft.offt);
      afft.o_im = ooura32.vectorArrayFactory(afft.offt);
      //afft.scale = 10.0 * 2.0 / (afft.size * afft.size * afft.CUTESDR_MAX_VAL * afft.CUTESDR_MAX_VAL);
      // FIXME: What's the correct value to use here? Adding the third afft.size was just arbitrary.
      afft.scale = 10.0 * 2.0 / (afft.size * afft.size * afft.size * afft.CUTESDR_MAX_VAL * afft.CUTESDR_MAX_VAL);
      
      // do initial autoscale in case stored max/min values are unreasonable (UNLESS they were set from the URL)
      if (!afft.init && wf.aper != kiwi.APER_AUTO && wf_mm == '') {
         wf.need_autoscale = wf.audioFFT_active? 1:16;
      }
      
      afft.iq = iq;
      afft.comp = audio_compression;
      afft.init = true;
   }

   if (fft_setup || (lsb != afft.prev_lsb)) {
      for (i = 0; i < 1024; i++) afft.pwr_dB[i] = 0;
      afft.prev_lsb = lsb;
   }

   var re, im, pwr, dB;
   if (afft.iq) {
      for (i = 0, j = 0; i < 1024; i += 2, j++) {
         afft.i_re[j] = audio_data[i] * afft.window_512[j];
         afft.i_im[j] = audio_data[i+1] * afft.window_512[j];
      }
      afft.offt.fft(afft.offt, afft.i_re.buffer, afft.i_im.buffer, afft.o_re.buffer, afft.o_im.buffer);
      for (j = 0, k = 512; j < 256; j++, k++) {
         re = afft.o_re[j]; im = afft.o_im[j];
         pwr = re*re + im*im;
         dB = 10.0 * Math.log10(pwr * afft.scale + 1e-30);
         dB = Math.round(255 + dB);
         afft.pwr_dB[k] = dB;
      }
      for (j = 256, k = 256; j < 512; j++, k++) {
         re = afft.o_re[j]; im = afft.o_im[j];
         pwr = re*re + im*im;
         dB = 10.0 * Math.log10(pwr * afft.scale + 1e-30);
         dB = Math.round(255 + dB);
         afft.pwr_dB[k] = dB;
      }
      waterfall_queue.push({ data:afft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
   } else {
      if (audio_compression) {
         if (afft.comp_1x) {
            // 2048 real samples done as 1x 2048-pt FFT
            for (i = 0; i < 2048; i++) {
               afft.i_re[i] = audio_data[i] * afft.window_2k[i];
            }
            afft.offt.fft(afft.offt, afft.i_re.buffer, afft.o_re.buffer, afft.o_im.buffer);
            if (lsb) {
               k = 512; ki = -1;
            } else {
               k = 256; ki = +1;
            }
            for (j = 0; j < 1024; j++) {
               re = afft.o_re[j]; im = afft.o_im[j];
               pwr = re*re + im*im;
               afft.dBi[j&1] = Math.round(255 + (10.0 * Math.log10(pwr * afft.scale + 1e-30)));
               if (j & 1) {
                  afft.pwr_dB[k] = Math.max(afft.dBi[0], afft.dBi[1]);
                  k += ki;
               }
            }
         } else {
            // 2048 real samples done as 2x 1024-pt FFTs
            for (i = 0; i < 1024; i++) {
               afft.i_re[i] = audio_data[i] * afft.window_1k[i];
            }
            afft.offt.fft(afft.offt, afft.i_re.buffer, afft.o_re.buffer, afft.o_im.buffer);
            if (lsb) {
               k = 512; ki = -1;
            } else {
               k = 256; ki = +1;
            }
            for (j = 0; j < 512; j++, k += ki) {
               re = afft.o_re[j]; im = afft.o_im[j];
               pwr = re*re + im*im;
               afft.pwr_dB[k] = Math.round(255 + (10.0 * Math.log10(pwr * afft.scale + 1e-30)));
            }
            waterfall_queue.push({ data:afft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
      
            for (i = 1024; i < 2048; i++) {
               afft.i_re[i] = audio_data[i] * afft.window_2k[i-1024];
            }
            afft.offt.fft(afft.offt, afft.i_re.buffer, afft.o_re.buffer, afft.o_im.buffer);
            if (lsb) {
               k = 512; ki = -1;
            } else {
               k = 256; ki = +1;
            }
            for (j = 0; j < 512; j++, k += ki) {
               re = afft.o_re[j]; im = afft.o_im[j];
               pwr = re*re + im*im;
               afft.pwr_dB[k] = Math.round(255 + (10.0 * Math.log10(pwr * afft.scale + 1e-30)));
            }
         }
         waterfall_queue.push({ data:afft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
      } else {
         for (i = 0; i < 512; i++) {
            afft.i_re[i] = audio_data[i] * afft.window_512[i];
         }
         afft.offt.fft(afft.offt, afft.i_re.buffer, afft.o_re.buffer, afft.o_im.buffer);
         if (lsb) {
            k = 512; ki = -2;
         } else {
            k = 256; ki = +2;
         }
         for (j = 0; j < 256; j++, k += ki) {
            re = afft.o_re[j]; im = afft.o_im[j];
            pwr = re*re + im*im;
            afft.pwr_dB[k] = afft.pwr_dB[k+1] = Math.round(255 + (10.0 * Math.log10(pwr * afft.scale + 1e-30)));
         }
         waterfall_queue.push({ data:afft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
      }
   }
}

/*
         // 2048 real samples done as 2x 1024-pt FFTs
         
         for (i = 0; i < 1024; i++) {
            afft.i_re[i] = audio_data[i] * afft.window_1k[i];
         }
         afft.offt.fft(afft.offt, afft.i_re.buffer, afft.o_re.buffer, afft.o_im.buffer);
         for (j = 0, k = 256; j < 512; j++, k++) {
            var re = afft.o_re[j], im = afft.o_im[j];
            var pwr = re*re + im*im;
            afft.pwr_dB[k] = Math.round(255 + (10.0 * Math.log10(pwr * afft.scale + 1e-30)));
         }
         waterfall_queue.push({ data:afft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
   
         for (i = 1024; i < 2048; i++) {
            afft.i_re[i] = audio_data[i] * afft.window_1k[i-1024];
         }
         afft.offt.fft(afft.offt, afft.i_re.buffer, afft.o_re.buffer, afft.o_im.buffer);
         for (j = 0, k = 256; j < 512; j++, k++) {
            var re = afft.o_re[j], im = afft.o_im[j];
            var pwr = re*re + im*im;
            afft.pwr_dB[k] = Math.round(255 + (10.0 * Math.log10(pwr * afft.scale + 1e-30)));
         }
         waterfall_queue.push({ data:afft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
*/


////////////////////////////////
// #ui
////////////////////////////////

/*
	displayed vs carrier frequency rules:
		the demod freq/offset is the displayed freq so the passband is drawn correctly
		the carrier freq is passed to the server
		for ssb/cw the passband freq is the display freq and shown on the freq entry box (demod.usePBCenter)
	
	frequency settings paths:
	
	freqmode_set_dsp_kHz		-> demodulator_analog_replace -> demodulator_set_offset_frequency[1/3] -> freqset_car_Hz[1/2] (freq_car_Hz =)
									-> demodulator_set_offset_frequency[2/3] -> freqset_car_Hz (freq_car_Hz =)

	scale_canvas_mousemove	)		-> demodulator_set_offset_frequency[3/3] -> freqset_car_Hz (freq_car_Hz =)
	scale_canvas_end_drag 	) offsets with demod.usePBCenter
	canvas_mouseup				)
	
	(passband drag)			-> freqset_car_Hz[2/2] (freq_car_Hz =)
	
	(mode buttons)				-> demodulator_analog_replace -> demodulator_set_offset_frequency-> freqset_car_Hz (freq_car_Hz =)
	
	select_band					-> freqmode_set_dsp_kHz -> (above)
	dx_click
	freqset_complete
	freqstep_cb
	WF shift-click (nearest boundary)
	
*/

var freq_displayed_Hz, freq_displayed_kHz_str, freq_displayed_kHz_str_with_freq_offset, freq_car_Hz;
var freqset_tout;

function freq_dsp_to_car(fdsp)
{
	var demod = demodulators[0];
	if (isUndefined(demod)) {
	   console.log("freq_dsp_to_car no demod?");
		return fdsp;
	}
	var offset = demod.low_cut + (demod.high_cut - demod.low_cut)/2;
	fcar = demod.isCW? fdsp - offset : fdsp;
	// fixme: not good enough to contain passband in rx bandwidth
	if (fcar < 0) fcar = 0;
	if (fcar > bandwidth) fcar = bandwidth;
	//console.log("freq_dsp_to_car dsp="+fdsp+" car="+fcar+" isCW="+demod.isCW+" offset="+offset+" hcut="+demod.high_cut+" lcut="+demod.low_cut);
	return fcar;
}

function freq_car_to_dsp(fcar)
{
	var demod = demodulators[0];
	if (isUndefined(demod)) return fcar;
	var offset = demod.low_cut + (demod.high_cut - demod.low_cut)/2;
	fdsp = demod.isCW? fcar + offset : fcar;
	//console.log("freq_car_to_dsp dsp="+fdsp+" car="+fcar+" isCW="+demod.isCW+" offset="+offset+" hcut="+demod.high_cut+" lcut="+demod.low_cut);
	return fdsp;
}

function passband_offset()
{
	var offset=0;
	var usePBCenter = false;
	var demod = demodulators[0];
	if (isDefined(demod)) {
		usePBCenter = demod.usePBCenter;
		offset = usePBCenter? demod.low_cut + (demod.high_cut - demod.low_cut)/2 : 0;
	}
	//console.log("passband_offset: usePBCenter="+usePBCenter+' offset='+offset);
	return offset;
}

// Always return the PB center freq.
// The PB center is usually only added when demod.usePBCenter for the CW modes is true.
function freq_passband_center()
{
	var freq = 0;
	var demod = demodulators[0];
	if (isDefined(demod)) {
		freq = center_freq + demod.offset_frequency;
		freq += demod.low_cut + (demod.high_cut - demod.low_cut)/2;
	}
	return freq;
}

function freq_passband(pbc_Hz)
{
	var lo = 0, hi = 0;
	var demod = demodulators[0];
	if (isDefined(demod)) {
	   if (!isNumber(pbc_Hz)) pbc_Hz = center_freq + demod.offset_frequency;
		lo = pbc_Hz + demod.low_cut;
		hi = pbc_Hz + demod.high_cut;
	}
	return { pbc:pbc_Hz, lo:lo, hi:hi };
}

function passband_offset_dxlabel(mode, ext, pb_lo, pb_hi)
{
   // if custom passband use it else the default passband of mode
   if (pb_lo == 0 && pb_hi == 0) {
	   var pb = kiwi_passbands(mode);
	   pb_lo = pb.lo;
	   pb_hi = pb.hi;
	}

   // don't offset label for some extensions that use an offset mode (e.g. usb)
   // but whose base freq is really the passband center
	var ext_label_offset_ok = true;
	if (ext.startsWith('fax')) ext_label_offset_ok = false;

	var usePBCenter = ext_mode(mode).SSB;
	var offset = (usePBCenter && ext_label_offset_ok)? pb_lo + (pb_hi - pb_lo)/2 : 0;
	//console.log("passband_offset: m="+ mode +" use="+ usePBCenter +' o='+ offset);
	return offset;
}


////////////////////////////////
// #frequency entry
////////////////////////////////

function freqmode_set_dsp_kHz(fdsp_kHz, mode, opt)
{
   var dont_clear_wf = w3_opt(opt, 'dont_clear_wf', false);
   var open_ext = w3_opt(opt, 'open_ext', false);
   var no_set_freq = w3_opt(opt, 'no_set_freq', 0);
   var no_clear_last_gid = w3_opt(opt, 'no_clear_last_gid', 0);

	var fdsp_Hz = fdsp_kHz * 1000;
	//console.log('freqmode_set_dsp_kHz: fdsp_Hz=' +fdsp_Hz+ ' mode='+ mode);
	if (dont_clear_wf == false) {
	   wf.audioFFT_clear_wf = true;
	}
	if (!no_clear_last_gid) owrx.dx_click_gid_last_until_tune = undefined;
	dx.last_stepped_gid = -1;
	//console.log('$dx_click_gid_last_until_tune='+ owrx.dx_click_gid_last_until_tune);

	if (isArg(mode) && (mode != cur_mode || open_ext == true)) {
		//console.log("freqmode_set_dsp_kHz: calling demodulator_analog_replace");
		ext_set_mode(mode, fdsp_Hz, opt);
	} else {
		freq_car_Hz = freq_dsp_to_car(fdsp_Hz);
		if (!no_set_freq) {
         //console.log('freqmode_set_dsp_kHz: demodulator_set_frequency NEW freq_car_Hz=' +freq_car_Hz +' no_set_freq='+ no_set_freq);
         //kiwi_trace('FSET_SET_FREQ');
         demodulator_set_frequency(owrx.FSET_SET_FREQ, freq_car_Hz);
      }
	}
}

function freqset_car_Hz(fcar)
{
	freq_car_Hz = fcar;
	//console.log("freqset_car_Hz: NEW freq_car_Hz="+fcar);
}

var freq_dsp_set_last;

function freq_field_prec(f_kHz)
{
   // limit to 9 characters max: 12345.789 or 123456.89
   var limit = 100e3 - (cfg.max_freq? 32e3 : 30e3);
   var prec = (kiwi.freq_offset_kHz >= limit)? 2 : (owrx.freq_dsp_1Hz? 3 : 2);
   return prec;
}

function freq_field_prec_s(f_kHz)
{
   return f_kHz.toFixed(freq_field_prec(f_kHz));
}

function freq_field_width()
{
   var _1Hz = owrx.freq_dsp_1Hz;
/*
   var b, width;

   if (kiwi_isFirefox()) { b = 'Firefox'; size = _1Hz? 11:10; }
   else
   if (kiwi_isChrome()) { b = 'Chrome'; size = _1Hz? 9:8; }
   else
   if (kiwi_isSafari()) { b = 'Safari'; size = _1Hz? 8:7; }
   else {
      b = 'Firefox'; size = _1Hz? 9:8;
   }

   if (kiwi_isFirefox()) { b = 'Firefox'; width = _1Hz? 11:10; }
   else
   if (kiwi_isChrome()) { b = 'Chrome'; width = _1Hz? 9:8; }
   else
   if (kiwi_isSafari()) { b = 'Safari'; width = _1Hz? 8:7; }
   else {
      b = 'Firefox'; width = _1Hz? 9:8;
   }

   var s = 'FFS: '+ b + (_1Hz? ' 1Hz' : ' 10Hz') +' '+ width +'em';
   //console.log(s);
   //canvas_log2(s);
   return width +'em';
*/
   
   // limit to 9 characters max: 12345.789 or 123456.89
   var limit = 100e3 - (cfg.max_freq? 32e3 : 30e3);
   //var width = (kiwi.freq_offset_kHz >= limit)? 6.5 : (_1Hz? 6.5 : 6);
   var width = (kiwi.freq_offset_kHz >= limit)? 6.25 : (_1Hz? 6.25 : 5.75);
   return (width +'em');
}

function freqset_restore_ui()
{
	var obj = w3_el('id-freq-input');
	if (isNoArg(obj)) return null;      // can happen if SND comes up long before W/F

   var f_kHz_with_freq_offset = (freq_displayed_Hz + kiwi.freq_offset_Hz)/1000;
   freq_displayed_kHz_str_with_freq_offset = freq_field_prec_s(f_kHz_with_freq_offset);
   obj.value = freq_displayed_kHz_str_with_freq_offset;

	//console.log("FUPD obj="+ typeof(obj) +" val="+ obj.value);
	freqset_select();
	return obj;
}

function freqset_update_ui(from)
{
	//console.log('FUPD-UI freq_car_Hz='+ freq_car_Hz +' cf+of='+(center_freq + demodulators[0].offset_frequency));
	//console.log('FUPD-UI center_freq='+ center_freq +' offset_frequency='+ demodulators[0].offset_frequency);
	//kiwi_trace();
	
	freq_displayed_Hz = freq_car_to_dsp(freq_car_Hz);
	var f_kHz = freq_displayed_Hz/1000;
   freq_displayed_kHz_str = freq_field_prec_s(f_kHz);
   
   if (owrx.trace_freq_update) {
      console.log("FUPD-UI freq_car_Hz="+freq_car_Hz+' NEW freq_displayed_Hz='+freq_displayed_Hz +' from='+ from +'('+ owrx.fset_s[from] +')');
      //if (from == owrx.FSET_NOP) kiwi_trace('freqset_update_ui');
	   owrx.trace_freq_update = 0;
	}
	
	if (!waterfall_setup_done) return;
	
	if (freqset_restore_ui() == null) return;
	
	// re-center if the new passband is outside the current waterfall
   if (from == owrx.FSET_SET_FREQ && zoom_center != 0.5) {
      // let the zoom code handle it since it seems to work
      //console.log('FSET_SET_FREQ recen');
      zoom_step(ext_zoom.CUR);
   } else {
	   waterfall_position(from, owrx.WF_POS_RECENTER_IF_OUTSIDE);
	}
	
	kiwi_storeWrite('last_freq', freq_displayed_kHz_str_with_freq_offset);
	freq_dsp_set_last = freq_displayed_kHz_str_with_freq_offset;
	//console.log('## freq_dsp_set_last='+ freq_dsp_set_last);

	freq_step_update_ui();
	freq_link_update();
	vfo_update();
	
	// don't add to freq memory while tuning across scale except for final position
	if (kiwi.fmem_auto_save && !owrx.no_fmem_update && owrx.fmem_upd[from]) {
      //console.log('>>> freq_memory_add freq='+ freq_displayed_kHz_str_with_freq_offset);
      freq_memory_add(freq_displayed_kHz_str_with_freq_offset);
	}

	kiwi_clearTimeout(owrx.popup_keyboard_active_timeout);
	owrx.popup_keyboard_active_timeout = setTimeout(function() {
	   owrx.popup_keyboard_active = false;
	   //canvas_log('***');
	}, 2000);
}

function vfo_update(A_equal_B)
{
	if (owrx.vfo_first) {
      var vfo_default = freq_displayed_kHz_str_with_freq_offset + cur_mode +'z'+ zoom_level;
	   var last_vfo_a = parseInt(kiwi_storeInit('last_vfo_A', vfo_default));
	   var last_vfo_b = parseInt(kiwi_storeInit('last_vfo_B', vfo_default));
	   
	   // switch to VFO B if initial freq matches it and not VFO A
	   if (freq_displayed_kHz_str_with_freq_offset != last_vfo_a && freq_displayed_kHz_str_with_freq_offset == last_vfo_b) {
         owrx.vfo_ab = 1;
         w3_flip_colors('id-freq-vfo', 'w3-aqua w3-orange', owrx.vfo_ab);
         w3_innerHTML('id-freq-vfo', "AB"[owrx.vfo_ab]);
	   }
	   owrx.vfo_first = false;
	}

   var flip = (A_equal_B == true)? 1:0;
   var vfo = freq_displayed_kHz_str_with_freq_offset + cur_mode +'z'+ zoom_level;
	kiwi_storeWrite('last_vfo_'+ "AB"[owrx.vfo_ab ^ flip], vfo);
}

// owrx.popup_keyboard_active set while popup keyboard active so ext_mobile_info() works
function popup_keyboard_touchstart(event)
{
   //canvas_log('FTS');
   owrx.popup_keyboard_active = true;
}

function retain_input_focus()
{
   return w3_retain_input_focus({ext:extint.displayed, scan:extint.scanning});
}

function freqset_select()
{
   //console.log('$freqset_select');
	w3_field_select('id-freq-input', {mobile:1, log:1});
}

function modeset_update_ui(mode)
{
	if (owrx.last_mode_el != null) owrx.last_mode_el.style.color = "white";
	
	// if sound comes up before waterfall then the button won't be there
	var kmode = ext_mode(mode);
	var el = w3_el('id-mode-'+ kmode.str);
	el.innerHTML = mode.toUpperCase();
	if (el && el.style) el.style.color = "lime";
	owrx.last_mode_el = el;
	
   // set last_mode_col for case of ext_set_mode() called direct instead of via mode_button()
	owrx.last_mode_col = parseInt(el.id);

	squelch_setup(toggle_e.FROM_COOKIE);
	kiwi_storeWrite('last_mode', mode);
	freq_link_update();
	vfo_update();

	// disable compression button in iq or stereo modes
	var disabled = ext_is_IQ_or_stereo_mode(mode);
   w3_els('id-button-compression',
      function(el) {
	      w3_disable(el, disabled);
      });
   w3_els('id-deemp',
      function(el) {
	      w3_disable(el, disabled);
      });

   w3_hide2('id-sam-carrier-container', !kmode.SAx);  // also QAM
   w3_hide2('id-chan-null', mode != 'sam');
   w3_hide2('id-SAM-opts', kmode.SAx);
   
   //console.log('kmode.NBFM='+ kmode.NBFM);
   w3_hide2('id-deemp1-ofm',  kmode.NBFM);
   w3_hide2('id-deemp2-ofm',  kmode.NBFM);
   w3_hide2('id-deemp1-nfm', !kmode.NBFM);
   w3_hide2('id-deemp2-nfm', !kmode.NBFM);
}

// delay the UI updates called from the audio path until the waterfall UI setup is done
function try_freqset_update_ui(from)
{
	if (waterfall_setup_done) {
		freqset_update_ui(from);
		if (wf.audioFFT_active) {
         if (cur_mode != wf.audioFFT_prev_mode) {
            wf.audioFFT_clear_wf = true;
         }
		   audioFFT_update();
		   wf.audioFFT_prev_mode = cur_mode;
		} else {
		   mkenvelopes();
		}
	} else {
		setTimeout(try_freqset_update_ui, 1000, from);
	}
}

function try_modeset_update_ui(mode)
{
	if (waterfall_setup_done) {
		modeset_update_ui(mode);
	} else {
		setTimeout(function() {
			try_modeset_update_ui(mode);
		}, 1000);
	}
}

function freq_link_update()
{
	var host = kiwi_url_origin();
	var url = host +'/?f='+ freq_displayed_kHz_str_with_freq_offset + cur_mode +'z'+ zoom_level;
	w3_innerHTML('id-freq-link',
      w3_icon('w3-text-css-lime||title='+ dq('copy to clipboard:\n'+ url),
         'fa-external-link-square', 15, '', 'freq_link_update_cb', url
      )
	);
}

function freq_link_update_cb(path, param, first)
{
   if (first) return;
   w3_copy_to_clipboard(param);
}

function freqset_complete(from, ev)
{
	if (owrx.waterfall_tuned > 0) {
	   //kiwi_trace('freqset_complete: waterfall_tuned='+ owrx.waterfall_tuned);
	   owrx.waterfall_tuned--;
	   return;
	}
	
	if (isArg(ev) && isObject(ev) && ev.key == 'Enter' && ev.shiftKey) {
	   //console.log('prev_freq_kHz SHIFT-ENTER');
	   return;
	}

	var obj = w3_el('id-freq-input');
	//console.log("FCMPL from="+ from +" obj="+ typeof(obj) +" val="+ (obj.value).toString());
	kiwi_clearTimeout(freqset_tout);
	if (isUndefined(obj) || obj == null) return;		// can happen if SND comes up long before W/F
	
	var iPhone = kiwi_is_iPhone();
   var a, f, pb;
   var adj_pbw = obj.value.includes('/');
   var adj_pbc = obj.value.includes(':');
   var set_wf  = obj.value.startsWith('#');

   if (!set_wf) {
      if (!iPhone || adj_pbw || adj_pbc) {
         // ff ff. ff.ff [/pbw] [/pblo,pbhi] [:pbc] [:pbc,pbw]
         a = obj.value.split(/[\/\:]/);
         f = a[0];
         pb = (a.length >= 2)? a[1].split(',') : null;
      } else {
         // iPhone
         // ff ff. ff.f ff.f.pbw [ff]..pbw [ff]..pblo.pbhi
         a = obj.value.split('.');
         f = (a.length == 1)? a[0] : (a[0] +'.'+ a[1]);
         pb = a; pb.shift(); pb.shift();
         if (pb.length == 0) pb = null;
         adj_pbw = true;
      }
   }

	// "/" alone resets to default passband (".." for iPhone)
   if (obj.value == '/' || (iPhone && obj.value == '..')) {
      //console.log('restore_passband '+ cur_mode);
      restore_passband(cur_mode);
      demodulator_analog_replace(cur_mode);
      freqset_update_ui(owrx.FSET_NOP);      // restore previous
      return;
   }
   
   if (set_wf) {
      f = obj.value.slice(1);
      if (f == '') {    // '#' alone resets wf to current rx freq
         waterfall_tune(0);
         return;
      }
   }
   
	var restore = true;
   if (f != '') {       // f == '' for "/pb" or "..pb" cases
      // 'k' suffix is simply ignored since default frequencies are in kHz
      f = f.replace(',', '.').parseFloatWithUnits('M', 1e-3);    // Thanks Petri, OH1BDF
      if (f > 0 && !isNaN(f)) {
         f -= kiwi.freq_offset_kHz;
         if (f > 0 && !isNaN(f)) {
            if (set_wf) {
               waterfall_tune(f);
               return;
            } else {
               var dsp_f = +freq_displayed_kHz_str;
               var same = (f == dsp_f);
               //console.log('f='+ f +' dsp_f='+ dsp_f +' same='+ same);
               freqmode_set_dsp_kHz(f, null, same? { dont_clear_wf:true } : null);
               restore = false;
            }
            w3_field_select(obj, {mobile:1, log:2});
         }
      }
	}

   if (restore) freqset_update_ui(owrx.FSET_NOP);     // restore previous
	
	// accept "freq/pbw" or "/pbw" to quickly change passband width to a numeric value
	// also "lo,hi" in place of "pbw"
	// and ":pbc" or ":pbc,pbw" to set the pbc at the current pbw
	if (pb) {
      var lo = pb[0].parseFloatWithUnits('k');
      var hi = NaN;
      if (pb.length > 1) hi = pb[1].parseFloatWithUnits('k');
	   
	   //console.log('### pb '+ (adj_pbw? '/' : ':') +' '+ pb[0] +'|'+ lo +', '+ pb[1] +'|'+ hi);
      var cpb = ext_get_passband();
      var cpbhw = (cpb.high - cpb.low)/2;
      var cpbcf = cpb.low + cpbhw;
      var pbhw;
	   
      if (adj_pbw) {
         // adjust passband width about current pb center
         if (pb.length == 1) {
            // /pbw
            pbhw = lo/2;
            lo = cpbcf - pbhw;
            hi = cpbcf + pbhw;
         }
      } else {
         // adjust passband center using current or specified pb width
         var pbc = lo;
         if (pb.length == 1) {
            // :pbc
            lo = pbc - cpbhw;
            hi = pbc + cpbhw;
         } else {
            pbhw = Math.abs(hi/2);
            // :pbc,pbw
            lo = pbc - pbhw;
            hi = pbc + pbhw;
         }
      }

      ext_set_passband(lo, hi, true);     // does error checking for NaN, lo < hi, min pbw etc.
	}
}

var ignore_next_keyup_event = false;

// freqset_keyup() is called on each key-up while the frequency box is selected so that if a numeric
// entry is made, without a terminating <return> key, a setTimeout(freqset_complete()) can be done to
// arrange automatic completion.
//
// It is also necessary for some mobile devices when using the popup "telephone pad" mode for input
// (e.g. iPhone 6S) with its "done" button. In this case the onsubmit=freqset_complete() event is
// never triggered, unlike the regular alphanumeric keypad when the "go" button is used.
//
// Also, keyup is used instead of keydown because the global id-kiwi-body has an eventlistener for keydown
// to implement keyboard shortcut event interception before the keyup event of freqset (i.e. sequencing).
// We use w3-custom-events in the freq box w3_input() to allow both onkeydown and onkeyup event handlers.

function freqset_keyup(obj, evt)
{
	//console.log("FKU obj="+ typeof(obj) +" val="+ obj.value +' ignore_next_keyup_event='+ ignore_next_keyup_event);
	//console.log(obj); console.log(evt);
	kiwi_clearTimeout(freqset_tout);
	
	// Ignore modifier-key key-up events triggered because the frequency box is selected while
	// modifier-key-including mouse event occurs somewhere else.
	// Also keeps freqset_complete timeout from being set after return key.
	
	// But this is tricky. Key-up of a shift/ctrl/alt/cmd key can only be detected by looking for a
	// evt.key string length != 1, i.e. evt.shiftKey et al don't seem to be valid for the key-up event!
	// But also have to check for them with any_alternate_click_event() in case a modifier key of a
	// normal key is pressed (e.g. shift-$).
	if (evt != undefined && evt.key != undefined) {
		var klen = evt.key.length;
		
		// any_alternate_click_event_except_shift() allows e.g. "10M" for 10 MHz
		if (any_alternate_click_event_except_shift(evt) || (klen != 1 && evt.key != 'Backspace')) {
	
			// An escape while the the freq box has focus causes the browser to put input value back to the
			// last entered value directly by keyboard. This value is likely different than what was set by
			// the last "element.value = ..." assigned from a waterfall click. So we have to restore the value.
			if (evt.key == 'Escape') {
			   //event_dump(evt, 'Escape-freq');
				//console.log('** restore freq box');
				freqset_update_ui(owrx.FSET_NOP);
			}
	
			if (evt.key == 'Enter' && evt.shiftKey) {
			   var f_kHz = owrx.prev_freq_kHz[1] / 1000;
			   //console.log('prev_freq_kHz FLIP '+ f_kHz);
			   //console.log(owrx.prev_freq_kHz);
			   if (f_kHz != 0) tune(f_kHz);
			   freqset_update_ui(owrx.FSET_NOP);
			}
			
			//console.log('FKU IGNORE ign='+ ignore_next_keyup_event +' klen='+ klen);
			ignore_next_keyup_event = false;
			return;
		}
	}
	
	if (ignore_next_keyup_event) {
      ignore_next_keyup_event = false;
      return;
   }
   
   if (!kiwi_isMobile())
	   freqset_tout = setTimeout(function() {freqset_complete('t/o');}, 3000);
}

var num_step_buttons = 6;

var up_down = {
   // 0 means use special step logic
	am:   [ 0, -1, -0.1, 0.1, 1, 0 ],
	amn:  [ 0, -1, -0.1, 0.1, 1, 0 ],
	amw:  [ 0, -1, -0.1, 0.1, 1, 0 ],
	sam:  [ 0, -1, -0.1, 0.1, 1, 0 ],
	sal:  [ 0, -1, -0.1, 0.1, 1, 0 ],
	sau:  [ 0, -1, -0.1, 0.1, 1, 0 ],
	sas:  [ 0, -1, -0.1, 0.1, 1, 0 ],
	qam:  [ 0, -1, -0.1, 0.1, 1, 0 ],
	drm:  [ 0, -1, -0.1, 0.1, 1, 0 ],
	usb:  [ 0, -0.1, -0.01, 0.01, 0.1, 0 ],
	usn:  [ 0, -0.1, -0.01, 0.01, 0.1, 0 ],
	lsb:  [ 0, -0.1, -0.01, 0.01, 0.1, 0 ],
	lsn:  [ 0, -0.1, -0.01, 0.01, 0.1, 0 ],
	cw:   [ 0, -0.1, -0.01, 0.01, 0.1, 0 ],
	cwn:  [ 0, -0.1, -0.01, 0.01, 0.1, 0 ],
	nbfm: [ -5, -1, -0.1, 0.1, 1, 5 ],
	nnfm: [ -5, -1, -0.1, 0.1, 1, 5 ],
	iq:   [ 0, -1, -0.1, 0.1, 1, 0 ],
};

var NDB_400_1000_mode = 1;		// special 400/1000 step mode for NDB band

// nearest appropriate boundary (1, 5 or 9/10 kHz depending on band & mode)
function freq_step_amount(b)
{
   var kmode = ext_mode(cur_mode);
	var step_Hz = kmode.SSB_CW? 1000 : 5000;
	var s = kmode.SSB_CW? ' 1k default' : ' 5k default';
   var ITU_region = cfg.init.ITU_region + 1;
   var ham_80m_swbc_75m_overlap = (ITU_region == 2 && b && b.name == '75m');

	if (b && b.name == 'NDB') {
		if (kmode.CW) {
			step_Hz = NDB_400_1000_mode;
		}
		s = ' NDB';
	} else
	if (b && (b.name == 'LW' || b.name == 'MW')) {
		//console_log('special step', kmode.str, kmode.AM_SAx_IQ_DRM);
		if (kmode.AM_SAx_IQ_DRM) {
			step_Hz = step_9_10? 9000 : 10000;
		}
		s = ' LW/MW';
	} else {
	   var svc = b? band_svc_lookup(b.svc) : null;
      if (svc && svc.o.name.includes('Broadcast') && !ham_80m_swbc_75m_overlap) {      // SWBC bands
         if (kmode.AM_SAx_IQ_DRM) {
            step_Hz = 5000;
            s = ' SWBC 5k';
            //console.log('SFT-CLICK SWBC');
         }
      } else
      if (b && b.chan != 0) {
         step_Hz = b.chan * 1000;
         s = ' band='+ b.name +' chan_kHz='+ b.chan;
      }
   }
	
	return { step_Hz: step_Hz, s:s };
}

function special_step(b, sel, caller)
{
	var s = 'SPECIAL_STEP '+ caller +' sel='+ sel;

   var rv = freq_step_amount(b);
   var step_Hz = rv.step_Hz;
   s += rv.s;

	if (sel < num_step_buttons/2) step_Hz = -step_Hz;
	s += ' step='+ step_Hz;
	//console.log(s);
	return step_Hz;
}

function freqstep_cb(path, sel, first, ev)
{
   var shiftKey = ev? ev.shiftKey : false;
   var hold = (ev && ev.type == 'hold');
   var hold_done = (ev && ev.type == 'hold-done');
   //console.log('freqstep_cb: path='+ path +' sel='+ sel +' first='+ first +' ev='+ ev +' shiftKey='+ shiftKey +' hold='+ hold +' hold_done='+ hold_done);
   if (hold) {
      //console.log('freqstep_cb HOLD '+ sel);
      kiwi_clearInterval(owrx.freqstep_interval);
      owrx.freqstep_interval = setInterval(
         function() {
            freqstep_cb(null, sel);
         }, 100);
      w3_autorepeat_canceller(path, owrx.freqstep_interval);
      return;
   }
   if (hold_done) {
      //console.log('freqstep_cb HOLD-DONE '+ sel);
      kiwi_clearInterval(owrx.freqstep_interval);
      w3_autorepeat_canceller();
      return;
   }
	var step_Hz = up_down[cur_mode][sel]*1000;
	
	// set step size from band channel spacing
	if (step_Hz == 0) {
		var b = find_band(freq_displayed_Hz);
		step_Hz = special_step(b, sel, 'freqstep_cb');
	}

	var fnew = freq_displayed_Hz;
	var incHz = Math.abs(step_Hz);
	var posHz = (step_Hz >= 0)? 1:0;
	var trunc = fnew / incHz;
	trunc = (posHz? Math.ceil(trunc) : Math.floor(trunc)) * incHz;
	var took;

	if (incHz == NDB_400_1000_mode) {
	   if (shiftKey != true) {
         var kHz = fnew % 1000;
         if (posHz)
            kHz = (kHz < 400)? 400 : ( (kHz < 600)? 600 : 1000 );
         else
            kHz = (kHz == 0)? -400 : ( (kHz <= 400)? 0 : ( (kHz <= 600)? 400 : 600 ) );
         trunc = Math.floor(fnew/1000)*1000;
         fnew = trunc + kHz;
         took = '400/1000';
         //console.log("STEP -400/1000 kHz="+kHz+" trunc="+trunc+" fnew="+fnew);
      } else {
		   fnew += Math.sign(step_Hz) * 1000;
		   took = 'SHIFT';
      }
	} else
	if (shiftKey != true && freq_displayed_Hz != trunc) {
		fnew = trunc;
		took = 'TRUNC';
	} else {
		fnew += step_Hz;
		took = (shiftKey == true)? 'SHIFT' : 'INC';
	}
	//console.log('STEP '+sel+' '+cur_mode+' fold='+freq_displayed_Hz+' inc='+incHz+' trunc='+trunc+' fnew='+fnew+' '+took);
	
	// audioFFT mode: don't clear waterfall for small frequency steps
	freqmode_set_dsp_kHz(fnew/1000, null, { dont_clear_wf:true });
}

var freq_step_last_mode, freq_step_last_band;

function freq_step_update_ui(force)
{
	if (isUndefined(cur_mode) || kiwi_passbands(cur_mode) == undefined ) return;
	var b = find_band(freq_displayed_Hz);
	
	//console.log("freq_step_update_ui: lm="+freq_step_last_mode+' cm='+cur_mode);
	if (!force && freq_step_last_mode == cur_mode && freq_step_last_band == b) return;

	var show_9_10 = (b && (b.name == 'LW' || b.name == 'MW') && ext_mode(cur_mode).AM_SAx_IQ_DRM)? true:false;
	
	if (kiwi_isMobile()) {
	   w3_hide2('id-9-10-cell', !show_9_10);
	} else {
	   w3_hide2('id-9-10-cell', false);
	   w3_disable('id-9-10-cell', !show_9_10);
	}

	for (var i=0; i < num_step_buttons; i++) {
		var step_Hz = up_down[cur_mode][i]*1000;
		if (step_Hz == 0) {
			step_Hz = special_step(b, i, 'freq_step_update_ui');
		}

		var title;
		var absHz = Math.abs(step_Hz);
		var posHz = (step_Hz >= 0)? 1:0;
		if (absHz >= 1000)
			title = (posHz? '+':'')+(step_Hz/1000).toString()+'k';
		else
		if (absHz != NDB_400_1000_mode)
			title = (posHz? '+':'')+step_Hz.toString();
		else {
			title = (posHz? '+':'-')+'400/'+(posHz? '+':'-')+'1000';
		}
		w3_el('id-step-'+i).title = title +'\nclick-hold to repeat';
	}
	
	freq_step_last_mode = cur_mode;
	freq_step_last_band = b;
}


////////////////////////////////
// #frequency memory
////////////////////////////////

function freq_memory_init()
{
   kiwi.fmem_auto_save = +kiwi_storeInit('fmem_auto_save', 1);
   kiwi.fmem_mode_save = +kiwi_storeInit('fmem_mode_save', 0);

	var fmem;
	if (isNonEmptyString(owrx.override_fmem)) {
	   fmem = owrx.override_fmem.split(',');
	} else {
      fmem = kiwi_JSON_parse('freq_memory_init', kiwi_storeRead('freq_memory'));
      //console.log('freq_memory_init ENTER');
      //console.log(fmem);
	}
	if (isNull(fmem)) fmem = [10000];
	
	// clean up url param or stored memory
   var prec = owrx.freq_dsp_1Hz? 3:2;
   fmem.forEach(function(s, i) {
      if (isNumber(s)) s = s.toString();
      var as = s.split(' ');
      var f_n = as[0].parseFloatWithUnits('kM', 1e-3);
      fmem[i] = w3_clamp(f_n, 1, cfg.max_freq? 32000 : 30000).toFixed(prec);
      if (as[1]) fmem[i] += ' '+ as[1];      // add back mode
   });
	
	// remove any dups
	if (fmem) {
      kiwi.freq_memory =
         kiwi_dedup_array(fmem
            /*
            function(v) {
               var rv = { err: false };
               v = +v;
               if (!isNumber(v) || v <= 0)
                  rv.err = true;
               else
                  rv.v = v.toString();    // in case storage has a number instead of a string
               return rv;
            }
            */
         );
      //console.log('freq_memory_init DONE');
      //console.log(kiwi.freq_memory);
   }

   kiwi.fmem_help =
      w3_div('',
         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua', 'Frequency memory help'),
         w3_inline_percent('w3-padding-tiny',
            'The current frequency is saved in the memory list in two different ways ' +
            'depending on the <x1>auto save</x1> setting:<br>' +
            '<ul><li>enabled: saves occur automatically as you tune.</li>' +
            '<li>disabled: saves are done manually, via the <x1>save</x1> menu item or ' +
            'click-holding the memory icon until it turns green ' +
             w3_icon('', 'fa-bars w3-text-css-lime', 20) + '</li></ul>' +

            'If <x1>mode save</x1> is enabled the current mode is saved and restored along with ' +
            'the frequency.<br><br>' +
            
            'These two save modes are remembered in browser storage and used when this Kiwi is ' +
            'visited again.<br><br>' +
            
            'The shortcut keys ^1 thru ^9 (^ means the control key) recall the memory ' +
            'from positions 1 thru 9 in the memory list.'
         ),

         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua w3-margin-T-8', 'Shortcut keys'),
         w3_inline_percent('w3-padding-tiny', 'm', 15, 'toggle frequency memory menu'),
         w3_inline_percent('w3-padding-tiny', '^1 ^2 ...', 15, 'recall memory #1 #2 ... (^ is ctl key)'),
         w3_inline_percent('w3-padding-tiny', 'n N', 15,
            w3_inline('',
               'VFO A/B, VFO A=B or click-hold VFO icon until it turns red',
               w3_div('w3-margin-L-8 w3-text-in-circle w3-wh-20px w3-red', 'A')
            )
         )
      );
}

function freq_memory_menu_init()
{
   w3_menu('id-freq-memory-menu', 'freq_memory_menu_item_cb');
}

function freq_memory_menu_open(shortcut_key)
{
   if (w3_isVisible('id-freq-memory-menu')) {
      kiwi.freq_memory_menu_shown = 0;
      //canvas_log('FMS0');
      return;
   }
   kiwi.freq_memory_menu_shown = 1;
   //canvas_log('FMS1');

   //console.log('freq_memory_menu_open='+ kiwi.freq_memory_menu_shown);
   var x = owrx.last_pageX, y = owrx.last_pageY;
   if (shortcut_key != true)
      x += ((x - 128) >= 0)? -128 : 16;

   var fmem_copy = kiwi_dup_array(kiwi.freq_memory);

   /*
   // add per-freq delete icon
   fmem_copy.forEach(
      function(s, i) {
         //fmem_copy[i] = w3_icon('w3-margin-R-8', 'fa-times-circle', 14, '', 'freq_memory_menu_delete_cb', i) + s;
         fmem_copy[i] = w3_icon('w3-margin-R-8', 'fa-times-circle', 14, '') + s;
      }
   );
   */

   // add to menu top
   fmem_copy.unshift('<hr>');
   var s = 'freq';
   if (kiwi.fmem_mode_save) s += '/mode';
   if (kiwi.fmem_auto_save) {
      fmem_copy.unshift('!save '+ s);
      fmem_copy.unshift('disable auto save');
   } else {
      fmem_copy.unshift('save '+ s);
      fmem_copy.unshift('enable auto save');
   }
   
   // add to menu bottom
   fmem_copy.push('<hr>');
   fmem_copy.push('clear all');
   fmem_copy.push(kiwi.fmem_mode_save? 'disable mode save' : 'enable mode save');
   fmem_copy.push('VFO A=B');
   fmem_copy.push('help');

   kiwi.fmem_arr = fmem_copy;
   w3_menu_items('id-freq-memory-menu', fmem_copy, Math.min(fmem_copy.length, 10));
   w3_menu_popup('id-freq-memory-menu',
      function(evt, first) {     // close_func(), return true to close menu
         //event_dump(evt, 'close_func');
         // if 'm' key or click on freq-menu icon, take toggle state into account
         var close_keyup = (evt.type == 'keyup' && (evt.key != 'm' || !kiwi.freq_memory_menu_shown));
         var tgt_isMenu = w3_contains(evt.target, 'w3-menu');
         var tgt_isMenuButton = w3_contains(evt.target, 'w3-menu-button');
         var menu_shown = kiwi.freq_memory_menu_shown;
         var click = ((evt.type == 'click' || ((evt.type == 'mousedown' || evt.type == 'touchstart') && !tgt_isMenuButton) || evt.type == 'touchend'));
         var close_click = (click && !first && (!tgt_isMenu || !menu_shown));
         var close = (close_keyup || close_click);
         //canvas_log(close +' first='+ first +' '+ evt.type +' ='+ (kiwi.freq_memory_menu_shown? 'SH':'NS') +
         //   (w3_contains(evt.target, 'w3-menu')? 'W3-MENU':''));
         //event_dump(evt, 'freq-memory-menu close_func='+ close, true);
         //canvas_log('menu_popup close_func='+ TF(close) +' ev='+ evt.type +' kup='+ TF(close_keyup) +' clk='+ TF(close_click));
         //canvas_log(TF(click) + TF(first) + TF(tgt_isMenu) + TF(menu_shown) +'{TF(F||F)}');

         return close;
      },
      x, y);
}

function freq_memory_menu_icon_cb(el, val, trace, ev)
{
   var hold = (!kiwi.fmem_auto_save && ev && ev.type == 'hold');
   var hold_done = (!kiwi.fmem_auto_save && ev && ev.type == 'hold-done');
   w3_colors('id-freq-menu', 'w3-text-white', 'w3-text-css-lime', hold);
   if (hold) {
	   freq_memory_add(freq_displayed_kHz_str_with_freq_offset);
      return;
   } else
   if (hold_done) {
      return;
   }
   freq_memory_menu_open(/* shortcut_key */ false);
}

function freq_memory_at(idx)
{
   if (idx < 0 || idx >= kiwi.freq_memory.length) return null;
   var as = kiwi.freq_memory[idx].split(' ');
   return { freq:as[0], mode:as[1] };
}

function freq_memory_add(f, clear)
{
   //console.log('freq_memory update');
   //console.log(kiwi.freq_memory);
   if (!isNumber(+f)) return;
   if (+f < 1) f = '1';
   if (kiwi.fmem_mode_save) f += ' '+ cur_mode;
   if (clear == true) {
      kiwi.freq_memory = [f];
   } else
	if (f != kiwi.freq_memory[0]) {
      //canvas_log('add='+ f);
	   kiwi.freq_memory.unshift(f);
	   kiwi.freq_memory = kiwi_dedup_array(kiwi.freq_memory);
	}
	if (kiwi.freq_memory.length > 25) kiwi.freq_memory.pop();
   //canvas_log('mem2='+ kiwi.freq_memory.join());
   //console.log('freq_memory_add WRITE COOKIE');
   //console.log(kiwi.freq_memory);
	kiwi_storeWrite('freq_memory', JSON.stringify(kiwi.freq_memory));
}

/*
function freq_memory_menu_delete_cb(path, val, first, cb_param)
{
   console.log('freq_memory_menu_delete_cb path='+ path +' val='+ val +' cbp='+ cb_param);
   console.log(cb_param);
}
*/

function freq_memory_menu_item_cb(idx, x, cb_param, ev)
{
   var rv = w3.CLOSE_MENU;
   idx = +idx;
   //console.log('freq_memory_menu_item_cb idx='+ idx +' x='+ x);
   if (idx == -1) return rv;
   var f_m = null;

   var TOP = 3;
   var BOT = TOP + kiwi.freq_memory.length;
   switch (idx) {
   
      case 0:
         kiwi.fmem_auto_save ^= 1;
         kiwi_storeWrite('fmem_auto_save', kiwi.fmem_auto_save);
         break;
      
      case 1:
         if (!kiwi.fmem_auto_save) {
            //console.log('SAVE');
            freq_memory_add(freq_displayed_kHz_str_with_freq_offset);
         }
         break;
      
      case 2:     // <hr>
         break;
      
      default:
            //canvas_log('idx='+ idx +' M='+ kiwi.freq_memory);
            f_m = freq_memory_at(idx - TOP);
            //canvas_log('sel='+ f);
            if (f_m) freq_memory_add(f_m.freq);
         break;
      
      case BOT:   // <hr>
         break;
      
      case BOT+1:
         //console.log('CLEAR ALL');
         freq_memory_add(freq_displayed_kHz_str_with_freq_offset, true);
         break;
      
      case BOT+2:
         //console.log('save MODE');
         kiwi.fmem_mode_save ^= 1;
         kiwi_storeWrite('fmem_mode_save', kiwi.fmem_mode_save);
         break;
      
      case BOT+3:
         //console.log('VFO A=B');
         vfo_update(true);
         break;
      
      case BOT+4:
         freq_memory_help();
         break;
   }
      
   if (f_m) {
      ext_tune(f_m.freq - kiwi.freq_offset_kHz, f_m.mode, ext_zoom.CUR);
   } else {
      freqset_select();
   }

   return rv;
}

function freq_memory_recall(idx)
{
   var f_m = freq_memory_at(Math.min(9, idx));
   if (f_m) {
      owrx.no_fmem_update = true;   // don't re-push fmem when just simply recalling from it
      ext_tune(f_m.freq - kiwi.freq_offset_kHz, f_m.mode, ext_zoom.CUR);
      owrx.no_fmem_update = false;
   }
}

function freq_memory_help()
{
   confirmation_show_content(kiwi.fmem_help, 550, 380);
}


////////////////////////////////
// #band scale
// #band bars
////////////////////////////////

// deprecated config.js has:
// var svc = { ... }
// var bands = [ {} ... {} ]

function band_info()
{
	var _9_10 = (+cfg.init.AM_BCB_chan)? 10:9;

	var ITU_region = cfg.init.ITU_region + 1;    // cfg.init.ITU_region = 0:R1(EU/AFR), 1:R2(NA/SA), 2:R3(AS/PAC)
	var LW_lo = 153-9/2, NDB_lo, NDB_hi, MW_hi;
	
	if (ITU_region == 1) {		// really 526.5 in UK?
		NDB_lo = 279+9/2; NDB_hi = 531-9/2; MW_hi = 1602+9/2;
	} else
	if (ITU_region == 2) {
		NDB_lo = 191-1/2; NDB_hi = 540-_9_10/2; MW_hi = 1700+_9_10/2;
	} else {
		// ITU_region == 3
		NDB_lo = 191-1/2; NDB_hi = 540-_9_10/2; MW_hi = 1602+_9_10/2;
	}
	
	//console.log('ITU='+ ITU_region +' _9_10='+ _9_10 +' LW_lo='+ LW_lo +' NDB_lo='+ NDB_lo +' NDB_hi='+ NDB_hi +' MW_hi='+ MW_hi);
	return { ITU_region:ITU_region, _9_10:_9_10, LW_lo:LW_lo, NDB_lo:NDB_lo, NDB_hi:NDB_hi, MW_hi:MW_hi };
}

// for a particular config band_svc.key return its object element and index
// callers MUST be prepared to deal with null return value if no key match found
function band_svc_lookup(svc_key)
{
   var idx = null;
   var _dxcfg = dx_cfg_db();
   if (!_dxcfg) return null;
   _dxcfg.band_svc.forEach(function(a, i) {
      if (a.key == svc_key)
         idx = i;
   });
   if (idx == null) {
      console.log('$$$ band_svc_lookup NO KEY MATCH for <'+ svc_key +'>');
      return null;
   }
   return { o: _dxcfg.band_svc[idx], i: idx };
}

// initialize from configurations bands
// also convert from config.js bands[] if necessary
function bands_init()
{
	var i, j, k, c, z, b, bnew;
	
   // Direct references to dxcfg.* only occur in this routine (and not via "_dxcfg = dx_cfg_db(); _dxcfg.* ..."),
   // as is done elsewhere, because conversion from config.js only applies to the dxcfg case (i.e. dx.db == dx.DB_STORED)
   
   var _dxcfg = dx_cfg_db();
   // already converted to saving in the appropriate configuration file
	if (_dxcfg && _dxcfg.bands && _dxcfg.band_svc && _dxcfg.dx_type) {
	   console.log('BANDS: using stored _dxcfg.bands db='+ dx.db +' ---------------------------------');
	   bands_addl_info(0);
	   dx_color_init();
	   return;
	}
   
	// NB: The following ONLY applies to the stored database
	//
	// Should only get here if this is the first run after an update from a version prior to v1.602
	// And dx.json HAD been modified by the admin such that the $(DX_NEEDS_UPDATE) section in the Makefile
	// DIDN'T get triggered and hence updated kiwi.config/{dx.json, dx_config.json}
	//
	// In that case must do conversions from the old config.js file until the admin page is opened
	// and dxcfg/dx_config.json can be written
	
	// User connections repeatedly convert from config.js on each new connection until
	// an admin connection (or admin operation from a user connection) occurs that can store the
	// converted values in kiwi.json
   console.log('BANDS: using old config.js/bands db='+ dx.db +'-------------------------------------');
   if (dx.db != dx.DB_STORED) {
      console.log('bands_init: DANGER! dx_cfg_db() is null but db != DB_STORED?!?');
      return;
   }

   // do this here even though it's related to the dx labels
   dxcfg.dx_type = [];
   var max_i = -1;
   dx.legacy_stored_types.forEach(function(a, i) {
      dxcfg.dx_type[i] = { key: i, name: dx.legacy_stored_types[i], color: dx.legacy_stored_colors[i] };
      max_i = i;
   });
   j = dx_type2idx(dx.DX_MASKED);
   for (i = max_i + 1; i < j; i++) {
      dxcfg.dx_type[i] = { key: i, name: '', color: '' };
   }
   dxcfg.dx_type[j] = { key: i, name: 'masked', color: 'lightGrey' };
   dx_color_init();
   
   
   // config.js::svc{} may have been modified by the admin, so migrate to cfg also
   dxcfg.band_svc = [];
   w3_obj_enum(svc, function(key, i, o) {
      if (key == 'X' && o.name == 'Beacons') {
         svc.L = svc.X;
         key = 'L';
         delete svc.X;
      }
      //console.log('key='+ key);
      svc[key].key = key;     // to assist in conversion of bands[] later on
      
      dxcfg.band_svc[i] = { key: key, name: o.name, color: o.color };
      if (o.name == 'Industrial/Scientific') {
         dxcfg.band_svc[i].name = 'ISM';
         dxcfg.band_svc[i].longName = 'Industrial/Scientific';
      }
   });
   //console.log(dxcfg.band_svc);
   
   
	var _bands_new = [];

   // NB: the only place "bands" (from config.js) should be referenced
	for (i = j = 0; i < bands.length; i++) {
		b = bands[i];
		
		// apply some fixes to known deficiencies in the bands[] from the default config.js,
		// but in a way that hopefully won't upset any customizations to config.js
		// a user might have made (e.g. adjustment to freq ranges)
		//
		// old config.js files:
		//    Beacons (IBP)
		//       svc 'X' instead of newer 'L'
		//       prepend name with "IBP "
		//       cw => cwn
		
		if (b.name == 'LW' && b.region == 'E') b.region = '1';
		if (b.name == 'LW' && b.region == '>') b.region = '23';

      // not too important as LW/NDB/MW details are all custom fixed below
		if (b.name == 'MW' && (b.region == '3' || b.region == '>3')) b.region = '13';

		if (b.name == 'NDB' && b.region == 'E') b.region = '1';
		if (b.name == 'NDB' && b.region == 'U') b.region = '2';
		if (b.name == 'NDB' && b.region == '>') b.region = '3';
		
      // ham band fixes
		if (b.name == 'MF' && b.region == '>23') b.region = '*';
		
		// time freqs are really markers
		if (b.name.startsWith('Time ') && b.region == '*') b.region = 'm';

      // use kiwi_shallow_copy() so changing e.g. b2.itu will be private to b2 copy
      // but b.s.svc.* and b2.s.svc.* will refer to the same 2nd level svc.* object
      
		if (b.region) {
		
		   // give _bands_new[] separate entry per region of a single band[] entry
		   var len = b.region.length;
		   if (len == 1) {
            _bands_new[j] = kiwi_shallow_copy(b);
            bnew = _bands_new[j];
            j++;
            c = bnew.region.charAt(0);
            if (c >= '1' && c <= '3')
               bnew.itu = ord(c, '1') + 1;
            else
            if (c == '*' || c == '>')
               bnew.itu = kiwi.ITU_ANY;
            else
            if (c == 'm')
               bnew.itu = kiwi.BAND_MENU_ONLY;
            else
            if (c == '-')
               bnew.itu = kiwi.BAND_SCALE_ONLY;
            else {
               bnew.itu = kiwi.BAND_SCALE_ONLY;
            }
            //console.log('R'+ c +'='+ bnew.itu +' '+ dq(b.name));
            //console.log(bnew);
		   } else {
            for (k = 0; k < len; k++) {
               c = b.region.charAt(k);
               if (c == '>') continue;    // '>' alone is detected above in (len == 1)
               _bands_new[j] = kiwi_shallow_copy(b);
               bnew = _bands_new[j];
               j++;
               if (c >= '1' && c <= '3')
                  bnew.itu = ord(c, '1') + 1;
               else {
                  bnew.itu = kiwi.BAND_SCALE_ONLY;
               }
               //console.log('R'+ c +'='+ bnew.itu +' '+ dq(b.name));
               //console.log(bnew);
            }
         }
		} else {
		   // shouldn't happen but handle anyway
		   _bands_new[j] = kiwi_shallow_copy(b);
		   bnew = _bands_new[j];
		   bnew.itu = kiwi.ITU_ANY;
		   j++;
         //console.log('DEFAULT ITU_ANY='+ bnew.itu +' '+ dq(b.name));
         //console.log(bnew);
		}
		
		if (isNumber(bnew.sel)) bnew.sel = bnew.sel.toFixed(2);
		
		// Add prefix to IBP names to differentiate from ham band names.
		// Change IBP passband from CW to CWN.
		// A software release can't modify bands[] definition in config.js so do this here.
		// At some point config.js will be eliminated when admin page gets equivalent UI.
		if (isUndefined(bnew.s)) bnew.s = svc.L;     // we've seen a case of a user with a corrupt config.js
		if ((bnew.s.key == 'L' || bnew.s.key == 'X') && bnew.sel.includes('cw') && bnew.region == 'm') {
		   if (!bnew.name.includes('IBP'))
		      bnew.name = 'IBP '+ bnew.name;
		   if (!bnew.sel.includes('cwn'))
		      bnew.sel = bnew.sel.replace('cw', 'cwn');
		}
		if (bnew.name.includes('IBP'))
		   bnew.s = 'L';    // svc.L is not defined in older config.js files

	}

   // svc field
	for (i = 0; i < _bands_new.length; i++) {
		bnew = _bands_new[i];

		if (isString(bnew.s)) bnew.svc = bnew.s;
		else
		if (isObject(bnew.s))
		   bnew.svc = bnew.s.key;
		else {
		   //console.log('$$$ t/o(bnew.svc)='+ typeof(bnew.s));
		}
		
		delete bnew.s;
		delete bnew.region;
	}

   // setup dxcfg.bands from converted config.js values
   dxcfg.bands = [];
   
   for (i = 0; i < _bands_new.length; i++) {
      b = _bands_new[i];
      var sel = b.sel || '';
      dxcfg.bands.push({
         min:b.min, max:b.max, name:b.name, svc:b.svc, itu:b.itu, sel:sel, chan:b.chan
      });
   }

   console.log('_bands_new:');
   console.log(_bands_new);
   bands_addl_info(0);
}

function dx_cfg_db()
{
   var db = (dx.db == dx.DB_COMMUNITY)? dxcomm_cfg : dxcfg;
   //console.log('dx_cfg_db dx.db='+ dx.db);
   if (db == null) {
      console.log('dx_cfg_db DANGER db=null dxcfg='+ dxcfg +' dxcomm_cfg='+ dxcomm_cfg);
      console.log('dx_cfg_db DANGER dx.db('+ dx.db +')='+ ((dx.db == dx.DB_COMMUNITY)? 'DB_COMMUNITY' : 'DB_STORED'));
   }
   return db;
}

function kiwi_bands_db(init_bands)
{
   if (init_bands == dx.INIT_BANDS_YES) {
      if (dx.db == dx.DB_STORED) kiwi.bands = []; else
      if (dx.db == dx.DB_COMMUNITY) kiwi.bands_community = [];
   }
   //console.log('$kiwi_bands_db init_bands='+ init_bands +' dx.db='+ dx.db +' kiwi.bands_community...');
   //console.log(kiwi.bands_community);
   return (dx.db == dx.DB_COMMUNITY)? kiwi.bands_community : kiwi.bands;
}

// augment kiwi.bands* with info not stored in configuration
function bands_addl_info(isAdmin)
{
   var i, z, b1, b2;
//var bi = band_info();
   var offset_kHz = kiwi.freq_offset_kHz;
   
   console.log('BANDS: creating additional band info');
   if (isAdmin && dx.db != dx.DB_STORED) {
      console.log('bands_addl_info: isAdmin=1 but dx.db='+ dx.db +' !!!');
      return;
   }
   
   var _dxcfg = dx_cfg_db();
   if (!_dxcfg) return;
   var _kiwi_bands = kiwi_bands_db(dx.INIT_BANDS_YES);

	for (i=0; i < _dxcfg.bands.length; i++) {
		b1 = _dxcfg.bands[i];
		b2 = _kiwi_bands[i] = {};

		_dxcfg.bands[i].chan = isUndefined(b1.chan)? 0 : b1.chan;

		// If Kiwi has an offset, re-bias band bar min/max back to 0-30 MHz and set isOffset flag.
		// This minimizes code changes elsewhere to handle offset mode.
		var min = b1.min, max = b1.max;
		if (offset_kHz && b1.min >= offset_kHz) {
		   min -= offset_kHz;
		   max -= offset_kHz;
		   b2.isOffset = true;
		} else
		if (b1.min > 32000)        // an offset band bar when not in offset mode
		   b2.isOffset = true;
		else
		if (b2.isOffset != true)   // guard against bands_addl_info() being called multiple times
		   b2.isOffset = false;    // a 0-30 MHz band bar
		
		b2.minHz = min * 1000; b2.maxHz = max * 1000; b2.chanHz = b1.chan * 1000;
		var bw = b2.maxHz - b2.minHz;
		//console.log(b1.name +' bw='+ bw);
		for (z = zoom_nom; z >= 0; z--) {
		   var zbw = bandwidth / (1 << z);
		   //console.log('z'+ z +' '+ (bw / zbw * 100).toFixed(0) +'%');
			if (bw <= 0.80 * zbw)
				break;
		}
		b2.zoom_level = z;
		b2.cfHz = b2.minHz + (b2.maxHz - b2.minHz)/2;
		var svc = band_svc_lookup(b1.svc);
		if (svc != null) {
		   var longName = svc.o.longName || svc.o.name;
		   b2.longName = b1.name +' '+ longName;
		} else {
		   b2.longName = b1.name;
		}
		//console.log("BAND "+b1.name+" bw="+bw+" z="+z);
	}
}

var band_menu = [];

function setup_band_menu()
{
	var i, op = 0, service = null;
	var s = '<option value="0" selected disabled>select band</option>';
	band_menu[op++] = null;		// menu title
	var ITU_region = cfg.init.ITU_region + 1;    // cfg.init.ITU_region = 0:R1, 1:R2, 2:R3

   var _dxcfg = dx_cfg_db();
   if (!_dxcfg) return '';
   var _kiwi_bands = kiwi_bands_db(dx.INIT_BANDS_NO);
	for (i = 0; i < _dxcfg.bands.length; i++) {
		var b1 = _dxcfg.bands[i];
		var b2 = _kiwi_bands[i];

		// filter bands based on offset mode
		if (kiwi.isOffset) {
		   if (!b2.isOffset) continue;
		} else {
		   if (b2.isOffset) continue;
		}

		if (!(b1.itu == kiwi.BAND_MENU_ONLY || b1.itu == kiwi.ITU_ANY || b1.itu == ITU_region)) continue;

		if (service != b1.svc) {
			service = b1.svc;
			var svc = band_svc_lookup(b1.svc);
			if (!svc) continue;
			s += '<option value='+ dq(op) +' disabled>'+ svc.o.name.toUpperCase() +'</option>';
			band_menu[op++] = null;		// section title
		}
		s += '<option value='+ dq(op) +'>'+ b1.name +'</option>';
		//console.log("BAND-MENU"+ op +" i="+ i +' '+ b1.min +'/'+ b1.max);
		band_menu[op] = {};
		band_menu[op].b1 = b1;
		band_menu[op].b2 = b2;
		op++;
	}
	return s;
}

function mk_band_menu()
{
   //console.log('mk_band_menu');
   band_menu = [];
   owrx.last_selected_band = 0;
   var id = 'id-select-band';
   var el = w3_el(id);
   w3_innerHTML(el, setup_band_menu());
   //w3_event_listener(id, el);
}

// find_band() is only called by code related to setting the frequency step size.
// For this purpose always consider the top of the MW band ending at 1710 kHz even if the
// configured ITU region causes the band bar to display a lower frequency.
function find_band(freq)
{
	var ITU_region = cfg.init.ITU_region + 1;    // cfg.init.ITU_region = 0:R1, 1:R2, 2:R3
   //console.log('find_band f='+ freq +' ITU_region=R'+ ITU_region);

   var _dxcfg = dx_cfg_db();
   if (!_dxcfg) return null;
   var _kiwi_bands = kiwi_bands_db(dx.INIT_BANDS_NO);
	for (var i = 0; i < _dxcfg.bands.length; i++) {
		var b1 = _dxcfg.bands[i];
		var b2 = _kiwi_bands[i];
      if (!(b1.itu == kiwi.BAND_SCALE_ONLY || b1.itu == kiwi.ITU_ANY || b1.itu == ITU_region)) continue;
      var max = (b1.name == 'MW')? 1710000 : b2.maxHz;
		if (freq >= b2.minHz && freq <= max) {
		/*
		   if (b1.itu)
		      console.log('find_band FOUND itu=R'+ b1.itu +' '+ b1.min +' '+ freq +' '+ b1.max);
		   else
		      console.log('find_band FOUND '+ b1.min +' '+ freq +' '+ b1.max);
		*/
			return b1;
		}
	}
	return null;
}

	band_canvas_h = 30;
	band_canvas_top = 0;
		band_scale_h = 20;
		band_scale_top = 5;
			band_scale_text_top = 5;
	
	dx_container_h = 80;
	dx_container_top = band_canvas_h;
		dx_label_top = 5;
		dx_line_h = dx_container_h - dx_label_top;
	
	scale_canvas_h = 47;
	scale_canvas_top = band_canvas_h + dx_container_h;
	
scale_container_h = band_canvas_h + dx_container_h + scale_canvas_h;

function mk_bands_scale()
{
   //console.log('mk_bands_scale');
	var r = g_range;
	
	// band bars & station labels
	var tw = band_ctx.canvas.width;
	var i, x, y = band_scale_top, w, h = band_scale_h, ty = y + band_scale_text_top;
	var start = r.start;
	var end = r.end;
	//console.log('BB fftw='+ wf_fft_size +' tw='+ tw +' start='+ start +' end='+ end +' bw='+ (end - start));
	//console.log('BB pixS='+ scale_px_from_freq(r.start, g_range) +' pixE='+ scale_px_from_freq(r.end, g_range));
	band_ctx.globalAlpha = 1;
	band_ctx.fillStyle = "White";
	band_ctx.fillRect(0,band_canvas_top, tw,band_canvas_h);
	var ITU_region = cfg.init.ITU_region + 1;    // cfg.init.ITU_region = 0:R1, 1:R2, 2:R3

   var _dxcfg = dx_cfg_db();
   if (!_dxcfg) return;
   var _kiwi_bands = kiwi_bands_db(dx.INIT_BANDS_NO);
	for (i = 0; i < _dxcfg.bands.length; i++) {
		var b1 = _dxcfg.bands[i];
		var b2 = _kiwi_bands[i];

		// filter bands based on offset mode
		if (kiwi.isOffset) {
		   if (!b2.isOffset) continue;
		} else {
		   if (b2.isOffset) continue;
		}

		if (!(b1.itu == kiwi.BAND_SCALE_ONLY || b1.itu == kiwi.ITU_ANY || b1.itu == ITU_region)) continue;
		//console.log('mk_bands_scale CONSIDER '+ b1.name +' R'+ b1.itu +' min='+ b1.min);
		
		var x1 = -1, x2;
		var bmin = b2.minHz, bmax = b2.maxHz;
		var min_inside = (bmin >= start && bmin <= end)? 1:0;
		var max_inside = (bmax >= start && bmax <= end)? 1:0;
		if (min_inside && max_inside) { x1 = bmin; x2 = bmax; } else
		if (!min_inside && max_inside) { x1 = start; x2 = bmax; } else
		if (min_inside && !max_inside) { x1 = bmin; x2 = end; } else
		if (bmin < start && bmax > end) { x1 = start; x2 = end; }
		//console.log('start='+ start +' end='+ end +' bmin='+ bmin +' bmax='+ bmax +' min_inside='+ min_inside +' max_inside='+ max_inside);

		if (x1 == -1) {
		   //console.log('BANDS x1 == -1 SKIP');
		   continue;
		}
 
      x = scale_px_from_freq(x1, g_range); var xx = scale_px_from_freq(x2, g_range);
      w = xx - x;
      //console.log("BANDS x="+ x1 +'/'+ x +" y="+ x2 +'/'+ xx +" w="+ w +' '+ ((w < 3)? 'TOO NARROW - SKIP':''));
      if (w < 3) continue;
      //console.log('BANDS SHOW');

      var svc = band_svc_lookup(b1.svc);
      band_ctx.fillStyle = svc? svc.o.color : 'grey';
      band_ctx.globalAlpha = 0.2;
      //console.log("BB x="+x+" y="+y+" w="+w+" h="+h);
      band_ctx.fillRect(x,y,w,h);
      band_ctx.globalAlpha = 1;

      //band_ctx.fillStyle = "Black";
      band_ctx.font = "bold 12px sans-serif";
      band_ctx.textBaseline = "top";
      var tx = x + w/2;
      var txt = b2.longName;
      var mt = band_ctx.measureText(txt);
      //console.log("BB mt="+mt.width+" txt="+txt);
      if (w >= mt.width+4) {
         // long name fits in bar
      } else {
         txt = b1.name;
         mt = band_ctx.measureText(txt);
         //console.log("BB mt="+mt.width+" txt="+txt);
         if (w >= mt.width+4) {
            // short name fits in bar
         } else {
            txt = null;
         }
      }
      //if (txt) console.log("BANDS tx="+(tx - mt.width/2)+" ty="+ty+" mt="+mt.width+" txt="+txt);
      if (txt) band_ctx.fillText(txt, tx - mt.width/2, ty);
	}
}

// reminder about how advanced features of RegExp work:
// x*			matches x 0 or more times
// x+			matches x 1 or more times
// x?			matches x 0 or 1 time
// (x)		capturing parens, stores in array beginning at [1]
// (?:x)y	non-capturing parens, allows y to apply to multi-char x
//	x|y		alternative (or)
// x? x* x+	0/1, >=0, >=1 occurrences of x

function parse_freq_pb_mode_zoom(s)
{
	return new RegExp('([0-9.,kM]*)([\/:][-0-9.,k]*)?([^z]*)?z?([0-9]*)').exec(s);
}

// scroll to next/prev band menu entry, skipping null title entries
function band_scroll(dir)
{
   // happens if .json config file corrupt
   if (band_menu.length == 0 || (band_menu.length == 1 && band_menu[0] == null)) return;
   
   var i = owrx.last_selected_band;
   var b;

   do {
      i += dir;
      if (i < 0) i = band_menu.length - 1;
      if (i >= band_menu.length) i = 0;
      b = band_menu[i];
   } while (b == null);
   
   select_band(i);
   w3_set_value('id-select-band', i);
}

function select_band_cb(path, idx)
{
   select_band(idx);
}

function select_band(v, mode_s)
{
   var b, v_num = +v;
   
   //console.log('select_band t/o_v='+ typeof(v) +' v_num='+ v_num);
   if (isNumber(v_num)) {
      //console.log('select_band num v='+ v_num);
      b = band_menu[v_num];
   } else {
      //console.log('select_band str v='+ v);
      var i;
      for (i = 0; i < band_menu.length-1; i++) {
         if (band_menu[i] && band_menu[i].b1.name == v)
            break;
      }
      //console.log('select_band i='+ i);
      if (i < band_menu.length-1)
         select_band(i, mode_s);
      return;
   }
   
	if (b == null) {
	   check_band(true);
		return;
	}
	
	var b1 = b.b1;
	var b2 = b.b2;
	if (dbgUs) {
      console.log(b);
      console.log(b1);
      console.log(b2);
   }

	var freq = b2.cfHz/1000, mode = mode_s, zoom = null;

	if (b1.sel != '') {
		var p = parse_freq_pb_mode_zoom(b1.sel);
		//console.log(p);
		if (p[1]) freq = parseFloat(p[1]);
      if (p[3] && isUndefined(mode_s)) mode = p[3].toLowerCase();
      if (p[4]) zoom = +p[4];
	}

	//console.log("SEL BAND"+v_num+" "+b1.name+" freq="+freq+((mode != null)? " mode="+mode:""));
	owrx.last_selected_band = v_num;
	if (dbgUs) {
		//console.log("SET BAND cur z="+zoom_level+" xb="+x_bin);
		sb_trace=0;
	}
	freqmode_set_dsp_kHz(freq, mode);
	if (isNumber(zoom))
	   zoom_step(ext_zoom.ABS, zoom);
	else
	   zoom_step(ext_zoom.TO_BAND, b);		// pass band to disambiguate nested bands in band menu
	if (sb_trace) {
		//console.log("SET BAND after z="+zoom_level+" xb="+x_bin);
	}
}

function check_band(reset)
{
	// reset "select band" menu if no longer inside band
	if (owrx.last_selected_band) {
		var b = band_menu[owrx.last_selected_band];
		var b1 = b.b1;
		var b2 = b.b2;
		//console.log('check_band '+ owrx.last_selected_band+ ' reset='+ reset +' '+ b1.min +'/'+ b1.max);
		
		// check both carrier and pbc frequencies
	   var car = freq_car_Hz;
	   var pbc = freq_passband_center();
	   
		if ((reset != undefined && reset == -1) || ((car < b2.minHz || car > b2.maxHz) && (pbc < b2.minHz || pbc > b2.maxHz))) {
	      //console.log('check_band OUTSIDE BAND RANGE reset='+ reset +' car='+ car +' pbc='+ pbc +' last_selected_band='+ owrx.last_selected_band);
	      //console.log(band);
			w3_select_value('id-select-band', 0);
			owrx.last_selected_band = 0;
		}
	}
}


////////////////////////////////
// #confirmation panel
////////////////////////////////

var confirmation = {
   displayed: false,
   close_cb: null,
};

function confirmation_panel_init()
{
   w3_el('id-panels-container').innerHTML +=
      w3_div('id-confirmation class-panel|border: 1px solid white|data-panel-name="confirmation" data-panel-pos="center" data-panel-order="0" data-panel-size="600,100"');

	w3_innerHTML('id-confirmation',
		w3_divs('id-confirmation-container/class-panel-inner') +
		w3_div('id-confirmation-vis class-vis')
	);
}

function confirmation_panel_set_close_func(close_cb)
{
	w3_el('id-confirmation-close').onclick = close_cb;    // hook the close icon
	confirmation.close_cb = close_cb;
}

function confirmation_panel_init2()
{
   confirmation_panel_set_close_func(confirmation_panel_close);
	w3_el('id-kiwi-body').addEventListener('keyup',
	   function(evt) {
	      if (evt.key == 'Escape') {
	         confirmation.close_cb();
            toggle_or_set_hide_panels(0);    // cancel panel hide mode
	      }
	   }, true);
}

function confirmation_show_content(s, w, h, close_cb, bg_color)
{
   // can't simply call confirmation_panel_close() here because of how panel is toggled
   if (confirmation.displayed)
      w3_color('id-confirmation', null, '');    // remove any user applied bg_color
   
   w3_innerHTML('id-confirmation-container', s);
   w3_color('id-confirmation', null, bg_color);
   
   if (close_cb) confirmation_panel_set_close_func(close_cb);
   confirmation_panel_show(w, h);
   
   // If panels hidden by e.g. 'x' key, and new panel brought up by menu action,
   // then force panels visible again.
   toggle_or_set_hide_panels(0);
}

function confirmation_panel_show(w, h)
{
	el = w3_el('id-confirmation');
	el.style.zIndex = 1020;
	if (isNoArg(w)) w = 525;
	if (isNoArg(h)) h = 80;
	confirmation_panel_resize(w, h);

   //console.log('confirmation_panel_show CHECK was '+ confirmation.displayed);
   confirmation.displayed = !confirmation.displayed;
   toggle_panel('id-confirmation');
}

function confirmation_panel_resize(w, h)
{
	var el = w3_el('id-confirmation');
	panel_set_width_height('id-confirmation', w, h);
   el.style.left = px(window.innerWidth/2 - el.activeWidth/2);
   el.style.bottom = px(window.innerHeight/2 - el.uiHeight/2);
}

function confirmation_panel_close()
{
   //console.log('confirmation_panel_close CHECK displayed='+ confirmation.displayed);
   //kiwi_trace();
   if (confirmation.displayed) {
      w3_color('id-confirmation', null, '');    // remove any user applied bg_color
      toggle_panel('id-confirmation');
      confirmation_panel_set_close_func(confirmation_panel_close);   // set default
      confirmation.displayed = false;
      //console.log('confirmation_panel_close CLOSE');
   }
}


////////////////////////////////
// #confirmation panel: set freq offset
////////////////////////////////


function set_freq_offset_dialog()
{
	var s =
		w3_col_percent('/w3-text-aqua',
			w3_input_get('w3-margin-T-6', 'Frequency scale offset (kHz, 1 Hz resolution)', 'cfg.freq_offset', 'w3_float_set_cfg_cb|3'), 80
		);
   confirmation_show_content(s, 525, 80);

	// put the cursor in (i.e. select) the field
	w3_field_select('id-cfg.freq_offset', {mobile:1});
}

////////////////////////////////
// #confirmation panel: cal ADC clock
////////////////////////////////

var cal_adc_new_adj;

function cal_adc_dialog(new_adj, clk_diff, r1k, ppm)
{
   var s;
   var gps_correcting = (cfg.ADC_clk2_corr != kiwi.ADC_CLK_CORR_DISABLED && ext_adc_gps_clock_corr() > 3)? 1:0;
   if (gps_correcting) {
      s = w3_col_percent('/w3-valign',
            w3_div('w3-show-inline-block', 'GPS is automatically correcting ADC clock. <br> No manual calibration available.') +
            w3_button('w3-red w3-margin-left', 'Close', 'confirmation_panel_close'),
            80
         );
   } else {
      cal_adc_new_adj = new_adj;
      var adc_clock_ppm_limit = 100;
      var hz_limit = ext_adc_clock_nom_Hz() * adc_clock_ppm_limit / 1e6;
      
      if (new_adj < -hz_limit || new_adj > hz_limit) {
         console.log('cal ADC clock: ADJ TOO LARGE');
         s = w3_col_percent('/w3-valign',
               w3_div('w3-show-inline-block', 'ADC clock adjustment too large: '+ clk_diff +' Hz<br>' +
                  '(ADC clock '+ ppm.toFixed(1) +' ppm, limit is +/- '+ adc_clock_ppm_limit +' ppm)') +
               w3_button('w3-red w3-margin-left', 'Close', 'confirmation_panel_close'),
               80
            );
      } else {
         s = w3_col_percent('/w3-valign',
               w3_div('w3-show-inline-block', 'ADC clock will be adjusted by<br>'+ clk_diff +' Hz to '+ r1k +' kHz<br>' +
                  '(ADC clock '+ ppm.toFixed(1) +' ppm)') +
               w3_button('w3-green w3-margin-left', 'Confirm', 'cal_adc_confirm') +
               w3_button('w3-red w3-margin-left', 'Cancel', 'confirmation_panel_close'),
               80
            );
      }
   }
   
   confirmation_show_content(s, 525, 70);
}

function cal_adc_confirm()
{
   ext_send('SET clk_adj='+ cal_adc_new_adj);
   ext_set_cfg_param('cfg.clk_adj', cal_adc_new_adj, EXT_SAVE);
   confirmation_panel_close();
}


////////////////////////////////
// #admin pwd panel
////////////////////////////////

function admin_pwd_query(isAdmin_true_cb)
{
	ext_hasCredential('admin', admin_pwd_cb, isAdmin_true_cb, ws_wf);
}

function admin_pwd_cb(badp, isAdmin_true_cb)
{
	console.log('admin_pwd_cb badp='+ badp);
	if (badp == kiwi.BADP_OK) {
		isAdmin_true_cb();
		return;
	}

	var s =
		w3_col_percent('/w3-text-aqua',
			w3_input('kiwi-pw', 'Admin password', 'admin.pwd', '', 'admin_pwd_cb2'), 80
		);
   confirmation_show_content(s, 525, 80);

	// put the cursor in (i.e. select) the password field
	w3_field_select('id-admin.pwd', {mobile:1});
}

function admin_pwd_cb2(el, val)
{
   w3_string_cb(el, val);
   confirmation_panel_close();
	ext_valpwd('admin', val, ws_wf);
}


////////////////////////////////
// #dx labels
////////////////////////////////

var dx = {
   step_dbg: 0,
   step_superDARN: 0,      // testing
   sig_bw_vis: 'hidden',
   sig_bw_left: '0px',
   
   url_p: null,
   help: false,
   
   dxcfg_parse_error: null,
   dxcomm_cfg_parse_error: null,
   
   DB_STORED: 0,
   DB_EiBi: 1,
   DB_COMMUNITY: 2,
   DB_N: 3,
   db: 0,
   db_s: [ 'stored (writeable)', 'EiBi (read-only)', 'community (downloaded)' ],
   db_short_s: [ 'stored', 'EiBi', 'community' ],
   ignore_dx_update: false,
   last_community_download: '',

   INIT_BANDS_NO: 0,
   INIT_BANDS_YES: 1,

   DX_STEP: 2,
   DX_MKRS: 4,
   
   DX_JSON: 0,
   DX_CSV:1,
   IDX_USER: null,
   UPD_MOD: 0,
   UPD_ADD: 1,
   o: {},      // current edit object


   // stored DB
   legacy_stored_types: [ 'active', 'watch-list', 'sub-band', 'DGPS', 'special event', 'interference' ],
   legacy_stored_colors: [ 'cyan', 'lightPink', 'aquamarine', 'lavender', 'violet', 'violet', 'lightGrey' ],
   stored_colors_light: [],      // setup by dx_color_init()
   stored_lightness: 0.95,
   filter_tod: [1,1,1],
	dow_colors: [ 'lightCoral', 'lightSalmon', 'gold', 'yellowGreen', 'springGreen', 'deepSkyBlue', 'violet' ],


   // EiBi DB
   eibi_colors: [],  // setup by dx_color_init()
   eibi_hue: [       // HTML color name or hue value for hsl() color construction
         'cyan',                 /* magenta */ 300,      'deepSkyBlue',
         'silver',               /* gold */ 50,          'peachPuff',
         'mediumAquaMarine',     /* violet */ 270,       /* blue */ 180,
         /* olive */ 70,         'springGreen',          /* red */ 0
   ],
   eibi_light: [     // lightness value for hsl() color construction
         0,                      /* magenta */ 80,       0,
         0,                      /* gold */ 70,          0,
         0,                      /* violet */ 80,        80,
         /* olive */ 45,         0,                      75
   ],
   eibi_colors_light: [],     // setup by dx_color_init()
   eibi_lightness: 0.90,

   eibi_svc_s: [
      'Bcast',    'Utility',  'Time',
      'ALE',      'HFDL',     'Milcom',
      'CW',       'FSK',      'FAX',
      'Aero',     'Marine',   'Spy'
   ],
   eibi_ext: [
      '',         '',         'time',
      'ale',      'hfdl',     '',
      'cw',       'fsk',      'fax',
      '',         '',         ''
   ],


   DX_MODE:       0x0000000f,    // 32 modes

   DX_TYPE:       0x000001f0,    // 32 types
   DX_TYPE_SFT: 4,

   DX_N_STORED:   16,
   DX_STORED:     0x00000000,    // stored: 0x000, 0x010, ... 0x0f0 (16)
   DX_QUICK_0:    0x00000000,    // NB: unshifted value
   DX_QUICK_1:    0x00000001,    // NB: unshifted value
   DX_MASKED:     0x000000f0,

   DX_N_EiBi:     12,
   DX_EiBi:       0x00000100,    // EiBi: 0x100, 0x110, ... 0x1f0 (16)
   DX_BCAST:      0x00000100,
   DX_UTIL:	      0x00000110,
   DX_TIME:	      0x00000120,
   DX_ALE:	      0x00000130,
   DX_HFDL:	      0x00000140,
   DX_MILCOM:     0x00000150,
   DX_CW:	      0x00000160,
   DX_FSK:        0x00000170,
   DX_FAX:        0x00000180,
   DX_AERO:       0x00000190,
   DX_MARINE:     0x000001a0,
   DX_SPY:        0x000001b0,

   DX_DOW_SFT:    9,
   DX_DOW:        0x0000fe00,
   DX_MON:        0x00008000,
   
   DX_FLAGS:      0xffff0000,
   DX_FILTERED:   0x00010000,
   DX_HAS_EXT:    0x00020000,
   DX_MODE_16:    0x00040000,
   
   eibi_types_mask: 0xffffffff,
   
   // DB_COMMUNITY
   DX_N_COMM:     15,
   T0_RESERVED:   0,
   T1_CHANNEL:    1,
   T2_HAM:        2,
   T3_BCAST:      3,
   T4_UTIL:       4,
   T5_TIME:       5,
   T6_ALE:        6,
   T7_HFDL:       7,
   T8_MILCOM:     8,
   T9_CW:         9,
   T10_FSK:       10,
   T11_FAX:       11,
   T12_AERO:      12,
   T13_MARINE:    13,
   T14_SPY:       14,

   list: [],
   displayed: [],
   
   sig_bw_render_gid: -1,
   last_stepped_gid: -1,

   filter_ident: '',
   filter_notes: '',
   filter_case: 0,
   filter_wild: 0,
   filter_grep: 0,
   ctrl_click: false,
};

function dx_init()
{
   var open = false;
   dx.DX_DOW_BASE = dx.DX_DOW >> dx.DX_DOW_SFT;

   if ((dx.url_p = kiwi_url_param('dx', '', null)) != null) {
      dx.db = dx.DB_EiBi;     // unless overridden below
      
      // wait for panel init to occur
      w3_do_when_cond(
         function() {
            var el = w3_el('id-ext-controls'); return el? el.init : false;
         },
         function() {
            console.log('DX URL '+ dq(dx.url_p));
            var do_filter = false;
            if (dx.url_p != '') {
               var r, p = dx.url_p.split(',');
               p.forEach(function(a, i) {
                  console.log('DX URL <'+ a +'>');
                  if ((i == 0 && a == 0) || w3_ext_param('sto', a).match) {
                     dx.db = dx.DB_STORED;
                     dx.filter_tod[dx.db] = 0;
                  } else
                  if ((i == 0 && a == 2) || w3_ext_param('com', a).match) {
                     dx.db = dx.DB_COMMUNITY;
                     dx.filter_tod[dx.db] = 0;
                  } else
                  if (w3_ext_param('none', a).match) {
                     dx.eibi_types_mask = 0;
                     console.log('DX URL SET none dx.eibi_types_mask='+ dx.eibi_types_mask);
                  } else
                  if ((r = w3_ext_param('filter', a)).match) {
                     dx.filter_tod[dx.db] = r.has_value? r.num : 1;
                     console.log('DX URL SET filter_tod='+ dx.filter_tod[dx.db] +' r.has_value='+ r.has_value +' r.num='+ r.num);
                  } else
                  if (w3_ext_param('open', a).match) {
                     open = true;
                  } else
                  if (w3_ext_param('help', a).full_match) {
                     open = true;
                     dx.help = true;
                  } else
                  if ((r = w3_ext_param('ident', a)).match) {
                     dx.filter_ident = r.string_case;
                     do_filter = true;
                  } else
                  if ((r = w3_ext_param('notes', a)).match) {
                     dx.filter_notes = r.string_case;
                     do_filter = true;
                  } else
                  if (w3_ext_param('case', a).match) {
                     console.log('DX URL SET case');
                     dx_filter_opt_cb('dx.filter_case', 1);
                  } else
                  if (w3_ext_param('grep', a).match) {
                     console.log('DX URL SET grep');
                     dx_filter_opt_cb('dx.filter_grep', 1);
                  } else
                  w3_ext_param_array_match_str(dx.eibi_svc_s, a, function(i,a) {
                        var v = dx.eibi_types_mask & (1 << i);
                        if (v)
                           dx.eibi_types_mask &= ~(1 << i);
                        else
                           dx.eibi_types_mask |= 1 << i;
                        console.log('DX URL MATCH '+ a +' v='+ v.toHex(8) +'('+ i +') dx.eibi_types_mask='+ dx.eibi_types_mask.toHex(8));
                  });
               });
            }
         
            console.log('DX URL DONE db='+ dx.db +' open='+ open +' eibi_types_mask='+ dx.eibi_types_mask.toHex(8) +' help='+ dx.help);
            if (do_filter) {
               console.log('DX URL DONE ident=<'+ dx.filter_ident +'> notes=<'+ dx.filter_notes +'>');
               dx_filter_cb();
            }
            dx_schedule_update();
         }, null,
         500
      );
      // REMINDER: w3_do_when_cond() returns immediately
   } else {
      dx.db = kiwi_storeInit('last_db', cfg.dx_default_db);     // default specified by configuration
   }

   dx_database_cb('', dx.db, false, {open:open});
   
   if (dx.step_superDARN) setTimeout(
      function() {
         dx.filter_ident = 'superDARN';
         dx_filter_cb();
      }, 1000
   );
}

function dx_decode_mode(flags)
{
   return (  (((flags) & dx.DX_MODE_16)? 16:0) | ((flags) & dx.DX_MODE) );
}

function dx_encode_mode(mode)
{
   return (  (((mode) >= 16)? dx.DX_MODE_16:0) | ((mode) & dx.DX_MODE) );
}

// either type: EiBi or stored
function dx_type2idx(type)
{
   type = type & dx.DX_TYPE;
   if (type >= dx.DX_EiBi) type -= dx.DX_EiBi;     // remove EiBi bias
   return (type >> dx.DX_TYPE_SFT);
}

function dx_eibi_type2mask(type)
{
   return (1 << dx_type2idx(type));
}

function dx_set_type(type)
{
   dx.eibi_types_mask = dx_eibi_type2mask(type);
}

function dx_is_single_type(type)
{
   return (dx.db == dx.DB_EiBi && dx.eibi_types_mask == dx_eibi_type2mask(type));
}

function dx_color_init()
{
   var i;
   if (dx.db != dx.DB_EiBi) {
      var _dxcfg = dx_cfg_db();
      if (!_dxcfg) return;
   
      for (i = 0; i < _dxcfg.dx_type.length; i++) {
         var o = _dxcfg.dx_type[i];
         var color = (o.color == '')? 'white' : o.color;
         var colorObj = w3color(color);
         if (colorObj.valid) {   // only if the color name is valid
            if (dx.db == dx.DB_STORED)
               colorObj.lightness = dx.stored_lightness;
            dx.stored_colors_light[i] = colorObj.toHslString();
         }
      }
   } else {
      dx.eibi_hue.forEach(function(color_or_hue, i) {
         if (isString(color_or_hue)) {
            dx.eibi_colors[i] = color_or_hue;
         } else {
            dx.eibi_colors[i] = 'hsl('+ color_or_hue +',100%,'+ dx.eibi_light[i] +'%)';
         }
      
         var color = w3color(dx.eibi_colors[i]);
         color.lightness = dx.eibi_lightness;
         dx.eibi_colors_light[i] = color.toHslString();
      });

      for (i = 0; i < dx.DX_N_EiBi; i++) {
         w3_color('eibi-cbox'+ i +'-label', 'black', dx.eibi_colors[i]);
      }
   }
}

function dx_update_request()
{
   w3_do_when_cond(
      function() {
         //console.log('### '+ (waterfall_setup_done? 'GO' : 'WAIT') +' dx_update(waterfall_setup_done)');
         return waterfall_setup_done;
      },
      function() {
         dx_update();
         bands_init();
         mk_bands_scale();
         mk_band_menu();
      }, null,
      200
   );
   // REMINDER: w3_do_when_cond() returns immediately
}

function dx_show(show, gid)
{
   w3_visible(dx_canvas, show);
   dx.sig_bw_vis  = dx_canvas.style.visibility;
   dx.sig_bw_left = dx_canvas.style.left;
   w3_hide2('id-dx-sig', !show);
   if (isArg(gid)) dx.sig_bw_render_gid = gid;
}

var dx_update_delay = 350;
var dx_update_timeout;

function dx_schedule_update()
{
	kiwi_clearTimeout(dx_update_timeout);
	dx_div.innerHTML = '';
   dx_show(false);
	dx_update_timeout = setTimeout(dx_update, dx_update_delay);
}

function dx_update()
{
   var eibi = (dx.db == dx.DB_EiBi);
	//g_range = get_visible_freq_range();
	//console_log_dbgUs("DX min="+(g_range.start/1000)+" max="+(g_range.end/1000));
	
	// turn off anti-clutter for HDFL band mode
	var anti_clutter = (ext_panel_displayed('HFDL') && dx_is_single_type(dx.DX_HFDL) && zoom_level >= 4)? 0 : 1;
	var min = (g_range.start/1000 + kiwi.freq_offset_kHz).toFixed(3);
	var max = (g_range.end/1000 + kiwi.freq_offset_kHz).toFixed(3);
	wf_send('SET MARKER db='+ dx.db +' min='+ min +' max='+ max +
	   ' zoom='+ zoom_level +' width='+ waterfall_width +' eibi_types_mask='+ dx.eibi_types_mask.toHex() +
	   ' filter_tod='+ dx.filter_tod[dx.db] +' anti_clutter='+ anti_clutter);
	// responds with "MSG mkr="
	if (dx.step_dbg) console.log('UPDATE list');

	// refresh every 5 minutes to catch schedule changes
   kiwi_clearTimeout(dx.dx_refresh_timeout);
	if (dx.filter_tod[dx.db]) {
	//if (1) {    // so can observe change from dashed/lighter to solid/darker when not filtered
	   var _5_min_ms = 5*60*1e3;
	   //var _5_min_ms = 60*1e3;    // #ifdef TEST_DX_TIME_REV
	   var ms_delay = 2e3;        // wait for boundary to pass before checking
	   var ms_rem = _5_min_ms - (Date.now() % _5_min_ms);
	   //console_log_dbgUs('$ SET dx_refresh_timeout rem='+ (ms_rem/1e3).toFixed(3));
	   dx.dx_refresh_timeout = setTimeout(function() {
	      //console_log_dbgUs('$ GO dx_refresh_timeout ');
	      dx_div.innerHTML = '';
         dx_show(false);
	      dx_update();
	   }, ms_rem + ms_delay);
	}
}

// Why doesn't using addEventListener() to ignore mousedown et al not seem to work for
// div elements created appending to innerHTML?

var dx_ibp_list, dx_ibp_interval, dx_ibp_server_time_ms, dx_ibp_local_time_epoch_ms = 0;
var dx_ibp_freqs = { 14:0, 18:1, 21:2, 24:3, 28:4 };
var dx_ibp_lastsec = 0;

var dx_ibp_stations = {
	'4U1UN': 	{ value:0,  text: 'New York' },
	'VE8AT':		{ value:1,  text: 'Nunavut' },
	'W6WX':		{ value:2,  text: 'California' },
	'KH6RS': 	{ value:3,  text: 'Hawaii' },
	'ZL6B':		{ value:4,  text: 'New Zealand' },
	'VK6RBP':	{ value:5,  text: 'Australia' },
	'JA2IGY':	{ value:6,  text: 'Japan' },
	'RR9O':		{ value:7,  text: 'Siberia' },
	'VR2B':		{ value:8,  text: 'Hong Kong' },
	'4S7B':		{ value:9,  text: 'Sri Lanka' },
	'ZS6DN':		{ value:10, text: 'South Africa' },
	'5Z4B':		{ value:11, text: 'Kenya' },
	'4X6TU':		{ value:12, text: 'Israel' },
	'OH2B':		{ value:13, text: 'Finland' },
	'CS3B':		{ value:14, text: 'Madeira' },
	'LU4AA':		{ value:15, text: 'Argentina' },
	'OA4B':		{ value:16, text: 'Peru' },
	'YV5B':		{ value:17, text: 'Venezuela' }
};

function dx_label_render_cb(arr)
{
	var i, j, k;
	var hdr = arr[0];
	dx.last_hdr = hdr;
	
	var obj;
   var _dxcfg = dx_cfg_db();
   if (!_dxcfg) return;
   var stored = (dx.db == dx.DB_STORED);
   var eibi = (dx.db == dx.DB_EiBi);
   var community = (dx.db == dx.DB_COMMUNITY);
   var gap = eibi? 40 : 35;
   //var dbg_lbl = dbgUs;
   var dbg_lbl = false;
   
   console_log_lbl = function(s) { if (dbg_lbl) console.log(s); };
   
   dx_color_init();

	// process reply sent in response to our label step request
	if (hdr.t == dx.DX_STEP) {
	   var dl = arr[1];
	   if (dl) {
	      if (dx.step_dbg) console.log('dx_label_render_cb: DX_STEP [[[RESP]]] gid='+ dl.g +' f='+ dl.f);
	      if (dx.step_dbg) console.log(JSON.stringify(dl));
	      var mode = kiwi.modes_lc[dx_decode_mode(dl.fl)];
         freqmode_set_dsp_kHz(dl.f, mode);
         if (dl.lo != 0 && dl.hi != 0) {
            ext_set_passband(dl.lo, dl.hi);     // label has custom pb
         } else {
            var dpb = kiwi_passbands(mode);
            ext_set_passband(dpb.lo, dpb.hi);   // need to force default pb in case cpb is not default
         }
         dx.sig_bw_render_gid = dl.g;
      } else {
	      if (dx.step_dbg) console.log('dx_label_render_cb: DX_STEP [[[RESP]]]: NO LABEL FOUND');
         dx.sig_bw_render_gid = -1;
      }
	   return;
	}
	
	var errors = hdr.pe + hdr.fe;
	if (errors) {
	   var el = w3_el('id-dxcfg-err');
	   if (el && !el.innerHTML.startsWith('Warning')) {   // don't override dx_config.json corruption warning
         el.innerHTML = 'Warning: dx.json file has '+ errors +' label'+ plural(errors, ' error');
         w3_add(el, 'w3-text-css-orange');
      }
   }
	
	kiwi_clearInterval(dx_ibp_interval);
	dx_ibp_list = [];
	dx_ibp_server_time_ms = hdr.s * 1000 + (+hdr.m);
	dx_ibp_local_time_epoch_ms = Date.now();
	
	if (eibi && isDefined(hdr.ec)) for (i = 0; i < dx.DX_N_EiBi; i++) {
	   if (isDefined(hdr.ec[i]))
	      w3_innerHTML('eibi-cbox'+ i +'-label', dx.eibi_svc_s[i] +' ('+ hdr.ec[i] +')');
	}
	
	if (community && isDefined(hdr.tc)) for (i = 0; i <= dx.DX_N_COMM; i++) {
	   if (isDefined(hdr.tc[i]))
	      w3_innerHTML('id-dxcomm'+ i +'-label', dxcomm_cfg.dx_type[i].name +' ('+ hdr.tc[i] +')');
	}
	
	dx.types_n = hdr.tc;
	
	dx_filter_field_err(+hdr.f);
	
	var dx_idx, dx_z = 120;
	dx_div.innerHTML = '';
	dx.displayed = [];
	dx.last_freq = 0;
	dx.last_ident = '';
	dx.last_f_base = -1;
	dx.post_render = [];
	dx.color_fixup = [];
	dx.f_same = [];
   var same_x, lock_z = 0;
   var center_x1 = Math.floor(wf_container.clientWidth/2) - 1;
   var center_x2 = center_x1 + 2;
	var s_a = [];
	var gid;

	var optimize_label_layout = function(x, z, f) {
	   var i, j, k;
	   var spacing = 6;
	   var n = Math.ceil(gap / spacing);

      // postpone text width sorting until first label render establishes dx.font info for use by getTextWidth()
      if (dx.font) {
         dx.f_same.forEach(function(idx,i) {
            idx++;
            var o = arr[idx];
            o.di = kiwi_decodeURIComponent('', o.i);
            o.tw = getTextWidth(o.di, dx.font);
         });

         //console_log_dbgUs('$f_same '+ dx.f_same.length +' f='+ f +' x='+ x);
         dx.f_same.sort(
            function(i1, i2) {
               var o1 = arr[i1+1];
               var o2 = arr[i2+1];
               //if (f == 5935) console.log(o1.tw.toFixed(2) +' '+ o1.di +'    '+ o2.tw.toFixed(2) +' '+ o2.di);
               return o2.tw - o1.tw;
            }
         );

         /*
         if (f == 5935) dx.f_same.forEach(function(idx,i) {
            var o = arr[idx+1];
            console.log(o.f +' #'+ idx +' c.len='+ o.di.length +' f.len='+ o.tw.toFixed(2) +' '+ o.di);
         });
         */
      }
      
      for (i = 0; i <= dx.f_same.length; i++) {
         var idx = dx.f_same[i];
         j = i % (n+1);
		   dx.post_render[idx] = {
		      top: dx_label_top + (j * spacing),
		      ltop: dx_label_top,
		      x: x + j * (spacing - 1),
		      z: z + i
		      //f: f
		      //ident: '???'
		   };
		   if (j == n) {
		      x += gap;
		   }
      }
	};
	
	if (dbg_lbl) {
	   console.log('######## dx_label_render_cb db='+ dx.db +' arr.len='+ arr.length);
	   console.log(arr);
	}

	// first pass
	for (i = 1; i < arr.length; i++) {
		dx_idx = i-1;
		obj = arr[i];
      var freq = obj.f;
      //if (freq == 10000) console_log_dbgUs(obj);
		var flags = obj.fl;

		var type = flags & dx.DX_TYPE;
		
      // this gives zero-based index for stored/EiBi color
      var color_idx = dx_type2idx(type);

		var filtered = flags & dx.DX_FILTERED;
		var ident = obj.i;
		if (eibi && ident == dx.last_ident && freq == dx.last_freq) {
         console_log_lbl('1111 dx_idx='+ dx_idx +' '+ freq +' SAME AS LAST filtered='+ (filtered? 1:0) +' '+ ident);
         if (filtered == 0) dx.color_fixup[dx.last_idx] = dx.eibi_colors[color_idx];
         continue;
		}

		dx.last_idx = dx_idx;
		dx.last_freq = freq;
		dx.last_ident = ident;
      console_log_lbl('1111 dx_idx='+ dx_idx +' '+ freq +' OK filtered='+ (filtered? 1:0) +' '+ ident);
		
		gid = obj.g;
		var f_base = freq - kiwi.freq_offset_kHz;
		var mkr_off = obj.o;
		var notes = isDefined(obj.n)? obj.n : '';
		var params = isDefined(obj.p)? obj.p : '';
		
		if (eibi && (flags & dx.DX_HAS_EXT)) {
		   var ext_name = dx.eibi_ext[dx_type2idx(type)];
		   if (ident.includes('CHU')) ext_name = 'fsk';    // exception: CHU uses FSK, not timecode, extension
		   //params = ext_name +','+ freq.toFixedNZ(2);
		   params = ext_name +',*';
		   //console.log('DX EXT EiBi freq='+ freq +' params='+ params);
		}

		var f_baseHz = f_base * 1000;
      // always place label at center of passband
		var f_base_label_Hz = f_baseHz + passband_offset_dxlabel(kiwi.modes_lc[dx_decode_mode(flags)], params, obj.lo, obj.hi);
		var x = scale_px_from_freq(f_base_label_Hz, g_range) - 1;	// fixme: why are we off by 1?
		var cmkr_x = 0;		// optional carrier marker for NDBs and sig_bw
		var carrier = f_baseHz - mkr_off;
		if (mkr_off) cmkr_x = scale_px_from_freq(carrier, g_range);
	
		if (eibi && f_base_label_Hz == dx.last_f_base) {
		   dx.f_same.push(dx_idx);
		   same_x = x;
		   if (lock_z == 0) lock_z = dx_z;
		} else {
		   if (eibi) {
            if (dx.f_same.length > 2) optimize_label_layout(same_x, lock_z, dx.last_f_base/1e3);
            dx.f_same = [];
            dx.f_same.push(dx_idx);
            lock_z = 0;
         }
		}
		var top = dx_label_top + (gap * (dx_idx & 1));    // stagger the labels vertically
      dx.post_render[dx_idx] = { top: top, ltop: top, x: x /* , f: f_base_label_Hz/1e3, ident: ident */ };
		dx.last_f_base = f_base_label_Hz;

      var color;
      if (eibi) {
         color = filtered? dx.eibi_colors_light[color_idx] : dx.eibi_colors[color_idx];
      } else {
         var c = _dxcfg.dx_type[color_idx].color;
         if (c == '') c = 'white';
         color = filtered? dx.stored_colors_light[color_idx] : c;
      }

      // for backward compatibility, IBP color is fixed to aquamarine
      // unless type has been set non-zero (since dist.dx.json never specified a type)
		if (!eibi && color_idx == 0 && (ident == 'IBP' || ident == 'IARU%2fNCDXF')) {
		   color = 'aquamarine';
		}
		console_log_lbl('DX '+ dx_idx +' f='+ freq +' k='+ mkr_off +' FL='+ flags.toHex(8) +
		   ' m='+ kiwi.modes_uc[dx_decode_mode(flags)] +' <'+ ident +'> <'+ notes +'>');
		
		carrier = freq * 1000 - mkr_off;
		carrier /= 1000;
		var center = (x >= center_x1 && x <= center_x2)? 1:0;
		dx.list[gid] = { gid:gid, carrier:carrier, lo:obj.lo, hi:obj.hi, freq:freq, f_label:f_base_label_Hz, mkr_off:mkr_off, sig_bw:obj.s,
		   flags:flags, begin:obj.b, end:obj.e, ident:ident, notes:notes, params:params, color:color, center:center };
	   console_log_lbl('dx_label_render_cb db='+ dx.db +' dx_idx='+ dx_idx +'/'+ arr.length +' gid='+ gid +' dx.list.len='+ dx.list.length +' '+ freq.toFixed(2) +' x='+ center_x1 +'|'+ x +'|'+ center_x2 +' center='+ center);
      dx.displayed[dx_idx] = dx.list[gid];
		console_log_lbl(dx.list[gid]);
		var has_ext = (params != '');
		
	   var _class = w3_sb('w3-custom-events w3-hold cl-dx-label', has_ext? 'dx-has-ext':'',
	      filtered? 'cl-dx-label-filtered':'', (has_ext && !filtered)? 'cl-dx-label-ext':'');
	   var _style_attr = sprintf('|left:%s; z-index:%d; background:%s|id="id-dx-label_%s"',
	      px(x-10), dx_z, color, dx_idx);
		s_a[dx_idx] =
		   w3_button_path(_class + _style_attr, 'dx-'+ gid, '', 'dx_evt', w3_sbc(',', gid, cmkr_x)) +
		   w3_div(sprintf('cl-dx-line|left:%s; z-index:110|id="id-dx-line_%s"', px(x), dx_idx));
		
		dx_z++;
	}
   if (eibi && dx.f_same.length > 2) optimize_label_layout(same_x, lock_z, -1);
	console_log_lbl(dx.list);
	console_log_lbl(dx.displayed);
	
	// render labels
   w3_innerHTML(dx_div, w3_div('id-dx-sig cl-dx-sig w3-hide'));
   if (dx.step_dbg) console.log('RENDER START func='+ hdr.t +' len='+ (arr.length-1) +' sig_bw_render_gid='+ dx.sig_bw_render_gid);
   j = 0;
   var ds='';
	for (i = 0; i < arr.length-1; i++) {
	   if (!isDefined(s_a[i])) continue;
      w3_create_appendElement(dx_div, 'div', s_a[i]);
      gid = dx.displayed[i].gid;
      if (dx.step_dbg) ds += ' '+ gid +'('+ i +')';
      if (gid == dx.sig_bw_render_gid) {
         if (dx.step_dbg) console.log('RENDER dx_sig_bw() gid='+ gid);
         dx_sig_bw(gid);
         dx.last_stepped_gid = gid;
      }
      j++;
	}
   if (dx.step_dbg) console.log('RENDER '+ j +' labels'+ ds);
	dx.last_freq = 0;
	dx.last_ident = '';

   // postpone text width sorting until first label render establishes dx.font info for use by getTextWidth()
	if (!dx.font) {
	   el = w3_el('id-dx-label_0');
	   if (el) {
	      dx.font = css_style(el, 'font-size') +' '+ css_style(el, 'font-family');
	      console_log_lbl('dx.font=<'+ dx.font +'>');
	   }
	}

	var dx_title = function(obj) {
	   var dow_s = '';
	   var dow = obj.fl & dx.DX_DOW;
      if (dow == dx.DX_DOW) dow = 0;   // zero instead of DX_DOW only in this routine

      if (obj.b == 0 && obj.e == 0) {
         obj.e = 2400;
         kiwi_trace('$SHOULD NOT OCCUR');
      }
	   
	   if (dow) for (var i = 0; i < 7; i++) {
	      var bit = dx.DX_MON >> i;
	      var match = bit & dow;
	      var c = match? 'MTWTFSS'[i] : '_';
	      //console.log('m='+ dow.toHex(4) +' b='+ bit.toHex(4) + (match? ' T ':' F ') + c +' '+ obj.f.toFixed(2) +' '+ obj.i);
	      dow_s += c;
	   } else
	      dow_s = '7-days';

	   var s = obj.b.leadingZeros(4) +'-'+ obj.e.leadingZeros(4) +' '+ dow_s;

	   if (eibi) {
         var lang = '';
         if (obj.l != '' && obj.l.charAt(0) != '-') lang = ' lang: '+ obj.l;
	      s += lang +' target: '+ obj.t;
	   }

	   return s;
	};

	// second pass
	for (i = 1; i < arr.length; i++) {
		dx_idx = i-1;
		obj = arr[i];
		ident = obj.i;
		freq = obj.f;
		flags = obj.fl;
		
		if (eibi) {
		   if (ident == dx.last_ident && freq == dx.last_freq) {
		      console_log_lbl('2222 dx_idx='+ dx_idx +' last_dx_idx='+ dx.last_dx_idx +' '+ obj.f +' SAME AS LAST '+ ident);
		      el = w3_el('id-dx-label_'+ dx.last_dx_idx);
		      el.title += '\n'+ dx_title(obj);
		      continue;
		   }
		}
		dx.last_dx_idx = dx_idx;
		dx.last_freq = freq;
		dx.last_ident = ident;
      if (eibi) console_log_lbl('2222 dx_idx='+ dx_idx +' '+ obj.f +' OK '+ ident);

		notes = (isDefined(obj.n))? obj.n : '';
		el = w3_el('id-dx-label_'+ dx_idx);
		if (!el) continue;
		var _ident = kiwi_decodeURIComponent('dx_ident2', ident).trim();
		if (_ident == '') _ident = '&nbsp;&nbsp;';
      el.innerHTML = w3_json_to_html('ident', _ident);
      //el.innerHTML += ' '+ obj.g;      // debug: add gid to label text
		var idx = dx_type2idx(obj.fl & dx.DX_TYPE);

      if (eibi) {
         /*
		   var ex = (flags & dx.DX_HAS_EXT)? '\nshift or alt-click to not open extension' : '';
		   console_log_lbl(obj.i +' '+ idx +' '+ dx.eibi_ext[idx] +' '+ ex);
         el.title = dx.eibi_svc_s[idx] +' // home country: '+ obj.c + ex +'\n'+ dx_title(obj);
         */
         el.title = dx.eibi_svc_s[idx] +' // home country: '+ obj.c +'\n'+ dx_title(obj);
      } else {
         var title = kiwi_decodeURIComponent('dx_notes', notes);
         
         // NB: this replaces the two literal characters '\' and 'n' that occur when '\n' is entered into
         // the notes field with a proper single-character escaped newline '\n' so the text correctly spans multiple lines
         title = w3_html_to_nl(title);
	      /*
	      if (stored) title = w3_nl(title) +'shift-click to edit label\nshift-option-click to toggle active';
	      if (w3_contains(el, 'dx-has-ext'))
	         title = w3_nl(title) + (stored? '':'shift or ') +'alt-click to not open extension';
	      */
	      
	      // only add schedule info to title popup if non-default
	      var dow = obj.fl & dx.DX_DOW;
	      //console_log_dbgUs('dow='+ dow.toHex(4) +' b='+ obj.b +' e='+ obj.e +' '+ ident);
	      if (dow != dx.DX_DOW || (obj.b != 0 || !(obj.b == 0 && obj.e == 2400)))
	         title = w3_nl(title) + dx_title(obj);

	      el.title = title;
      }

      if (dx_idx < dx.post_render.length) {
         var sparse = dx.post_render[dx_idx];
         if (sparse) {
            top = sparse.top;
            var left = sparse.x - 10;
            //var f = sparse.f;
            //console_log_dbgUs('dx.post_render: dx_idx='+ dx_idx +' post_render.len='+ dx.post_render.length +
            //   ' top='+ top +' left='+ left +' '+ f.toFixed(2) +' '+ sparse.ident);
            //console.log(el);
            el.style.top = px(top);
            el.style.left = px(left);
            if (isDefined(sparse.z)) el.style.zIndex = sparse.z;

	         var el_line = w3_el('id-dx-line_'+ dx_idx);
            top = sparse.ltop;
            el_line.style.top = px(top);
            el_line.style.height = px(dx_container_h - top);
         }
      }
		
      if (dx_idx < dx.color_fixup.length) {
         sparse = dx.color_fixup[dx_idx];
         if (sparse) {
            el.style.backgroundColor = sparse;
            //console.log('COLOR FIXUP dx_idx='+ dx_idx +' '+ sparse);
            //console.log(el);
         }
      }
      
		// FIXME: merge this with the concept of labels that are TOD sensitive (e.g. SW BCB schedules)	
		if (!eibi && (ident == 'IBP' || ident == 'IARU%2fNCDXF')) {
			var off = dx_ibp_freqs[Math.trunc(freq / 1000)];
			dx_ibp_list.push({ idx:dx_idx, off:off });
			kiwi_clearInterval(dx_ibp_interval);
			dx_ibp_interval = setInterval(function() {
				var d = new Date(dx_ibp_server_time_ms + (Date.now() - dx_ibp_local_time_epoch_ms));
				var min = d.getMinutes();
				var sec = d.getSeconds();
				var rsec = sec % 10;
				if ((rsec == 0) && dx_ibp_lastsec == 9) {
					var slot = (min % 3) * 6 + Math.trunc(sec/10);
					for (var i=0; i < dx_ibp_list.length; i++) {
						var s = slot - dx_ibp_list[i].off;
						if (s < 0) s = 18 + s;
						//console_log_dbgUs('IBP '+ min +':'+ sec +' slot='+ slot +' off='+ off +' s='+ s);
						
						// label may now be out of DOM if we're panning & zooming around
						el = w3_el('id-dx-label_'+ dx_ibp_list[i].idx);
						var call, location;
						w3_obj_enum(dx_ibp_stations, function(key, i, o) {
						   if (i == s) { call = key; location = o.text; }
						});
						if (el) el.innerHTML = 'IBP: '+ call +' '+ location;
					}
				}
				dx_ibp_lastsec = rsec;
			}, 500);
		}
	}
}

function dx_filter(shift_plus_ctl_alt)
{
   //console.log('dx_filter shift_plus_ctl_alt='+ shift_plus_ctl_alt);
   if (shift_plus_ctl_alt == true) {
      dx_filter_opt_cb('dx.filter_case', 0);
      dx_filter_opt_cb('dx.filter_grep', 0);
      dx.filter_ident = dx.filter_notes = '';
      dx_filter_cb(null, null, false, /* no_close */ true);
      return;
   }
   
	var s =
		w3_div('w3-medium w3-text-aqua w3-bold', 'DX label filter') +
		w3_divs('w3-text-aqua',
		   w3_col_percent('',
            w3_divs('/w3-margin-T-8',
               w3_input('w3-label-inline/id-dx-filter-ident w3-input-any-change w3-padding-small', 'Ident', 'dx.filter_ident', dx.filter_ident, 'dx_filter_cb'),
               w3_input('w3-label-inline/id-dx-filter-notes w3-input-any-change w3-padding-small', 'Notes', 'dx.filter_notes', dx.filter_notes, 'dx_filter_cb')
            ), 90
         ),
         w3_inline('w3-halign-space-around w3-margin-T-8 w3-text-white/',
            w3_checkbox('w3-label-inline w3-label-not-bold', 'case sensitive', 'dx.filter_case', dx.filter_case, 'dx_filter_opt_cb'),

            // Wildcard pattern matching, in addition to grep, is implemented. But currently checkbox is not shown because
            // there is no clear advantage in using it. E.g. it doesn't do partial matching like grep. So you have to type
            // "*pattern*" to duplicate what simply typing "pattern" to grep would do. Neither of them has the syntax of e.g.
            // simple search engines which is what the user probably really wants.
            //w3_inline('',
               //w3_text('', 'pattern match:'),
               //w3_checkbox('w3-label-inline w3-label-not-bold', 'wildcard', 'dx.filter_wild', dx.filter_wild, 'dx_filter_opt_cb'),
            //)

            w3_checkbox('w3-margin-left w3-label-inline w3-label-not-bold', 'grep pattern match', 'dx.filter_grep', dx.filter_grep, 'dx_filter_opt_cb'),
            w3_button('w3-aqua w3-small w3-padding-small', 'reset', 'dx_filter_reset_cb')
         )
      );

   confirmation_show_content(s, 450, 140, dx_filter_panel_close);
   w3_field_select('id-dx-filter-ident', {mobile:1});    // select the field
}

function dx_filter_reset_cb(el, val)
{
   dx_filter(true);
}

function dx_filter_panel_close()
{
   //console_log_dbgUs('dx_filter_panel_close');
   confirmation_panel_close();
}

function dx_filter_cb(path, val, first, no_close)
{
   if (first) return;
	if (path) w3_string_cb(path, val);
	var filtered = (dx.filter_ident != '' || dx.filter_notes != '');
   //console.log('dx_filter_cb path='+ path +' val='+ val +' filtered='+ filtered);
   //console.log('DX_FILTER ident=<'+ dx.filter_ident +'> notes=<'+ dx.filter_notes +
   //   '> case='+ dx.filter_case +' wild='+ dx.filter_wild +' grep='+ dx.filter_grep);
	wf_send('SET DX_FILTER i='+ encodeURIComponent(dx.filter_ident +'x') +' n='+ encodeURIComponent(dx.filter_notes +'x') +
	   ' c='+ dx.filter_case +' w='+ dx.filter_wild +' g='+ dx.filter_grep);
	w3_remove_then_add('id-dx-container', 'whiteSmoke cl-dx-filtered', filtered? 'cl-dx-filtered' : 'whiteSmoke');
	if (!filtered && !no_close) dx_filter_panel_close();
}

function dx_filter_opt_cb(path, val, first)
{
   if (first) return;
   val = +val;
   w3_bool_cb(path, val);
   
   // changing e.g. grep checkbox may clear ident/notes field error condition, so call dx_filter_cb() to update
   // pass "no_close = true" to prevent panel closing if fields empty
   dx_filter_cb('dx.filter_ident', dx.filter_ident, false, /* no_close */ true);

   console.log('dx_filter_opt_cb path='+ path +' val='+ val);
   w3_checkbox_set(path, val);
   
   // wild and grep are exclusive
   if (val && path != 'dx.filter_case') {
      dx_filter_opt_cb((path == 'dx.filter_wild')? 'dx.filter_grep' : 'dx.filter_wild', 0);
   }
   w3_field_select('id-dx-filter-ident', {mobile:1});    // reselect the field
}

function dx_filter_field_err(err)
{
   //console.log('dx_filter_field_err='+ err);
   w3_set_props('id-dx-filter-ident', 'w3-pink', err & 1);
   w3_set_props('id-dx-filter-notes', 'w3-pink', err & 2);
}

// Step to next/prev visible label.
// If at display extremes request that server search for next available label (possibly filtered).
function dx_label_step(dir)
{
   //if (dx.step_dbg) console.log('dx_label_step f='+ (freq_car_Hz/1e3).toFixed(2) +'/'+ (freq_displayed_Hz/1e3).toFixed(2) +' m='+ cur_mode);
   var i, dl;
   var eibi = (dx.db == dx.DB_EiBi);
   var f_pbc_Hz = freq_passband_center();
   var f, f_ok, same_gid;
   
   if (dir == 1) {
      for (i = 0; i < dx.displayed.length; i++) {
         dl = dx.displayed[i];
         if (!dl) continue;
         f = Math.round(dl.f_label);
         same_gid = (dl.gid == dx.last_stepped_gid);
         f_ok = (f > f_pbc_Hz);
         //if (dx.step_dbg) console.log('>>> STEP consider #'+ i +' '+ dl.gid +'('+ dx.last_stepped_gid +') '+ (f/1e3).toFixed(2) +'|'+ (f_pbc_Hz/1e3).toFixed(2) + (same_gid? ' same_gid':'') + (f_ok? ' f_OK':' f_lo'));
         if (same_gid) continue;
         if (f_ok) break;
      }
      
      // search above frequency for match
      if (i == dx.displayed.length) {
	      wf_send('SET MARKER db='+ dx.db +' dir=1 freq='+ (freq_displayed_Hz/1e3).toFixed(3) +
	         ' eibi_types_mask='+ dx.eibi_types_mask.toHex() +
	         ' filter_tod='+ dx.filter_tod[dx.db]);
	      if (dx.step_dbg) console.log('>>> STEP DX_STEP [[[REQ]]]');
	      dx.last_stepped_gid = -1;
         return;
      }
   } else {
      for (i = dx.displayed.length - 1; i >= 0 ; i--) {
         dl = dx.displayed[i];
         if (!dl) continue;
         f = Math.round(dl.f_label);
         same_gid = (dl.gid == dx.last_stepped_gid);
         f_ok = (f < f_pbc_Hz);
         //if (dx.step_dbg) console.log('<<< STEP consider #'+ i +' '+ dl.gid +'('+ dx.last_stepped_gid +') '+ (f/1e3).toFixed(2) +'|'+ (f_pbc_Hz/1e3).toFixed(2) + (same_gid? ' same_gid':'') + (f_ok? ' f_OK':' f_hi'));
         if (same_gid) continue;
         if (f_ok) break;
      }
      
      // search below frequency for match
      if (i < 0) {
	      wf_send('SET MARKER db='+ dx.db +' dir=-1 freq='+ (freq_displayed_Hz/1e3).toFixed(3) +
	         ' eibi_types_mask='+ dx.eibi_types_mask.toHex() +
	         ' filter_tod='+ dx.filter_tod[dx.db]);
	      if (dx.step_dbg) console.log('<<< STEP DX_STEP [[[REQ]]]');
	      dx.last_stepped_gid = -1;
         return;
      }
   }
   
   // After changing display to this frequency the normal dx_schedule_update() process will
   // acquire a new set of labels.
   // i.e. freqset_update_ui() => waterfall_pan_canvases() => dx_schedule_update() => (server) => dx_update()
   var mode_s = kiwi.modes_lc[dx_decode_mode(dl.flags)];
   if (dx.step_dbg) console.log('STEP FOUND #'+ i +' f='+ (f/1e3).toFixed(2) +' f_pbc_Hz='+ (f_pbc_Hz/1e3).toFixed(2) +' dl.freq='+ dl.freq +' '+ dl.ident +' '+ mode_s +' '+ dl.lo +' '+ dl.hi);
   freqmode_set_dsp_kHz(dl.freq, mode_s);
   if (dl.lo != 0 && dl.hi != 0) {
      ext_set_passband(dl.lo, dl.hi);     // label has custom pb
   } else {
      var dpb = kiwi_passbands(mode_s);
      //console.log(dpb);
      ext_set_passband(dpb.lo, dpb.hi);   // need to force default pb in case cpb is not default
   }
   
   if (dx.step_dbg) console.log('STEP RENDER dx_sig_bw() gid='+ dl.gid);
   dx_sig_bw(dl.gid);
   dx.last_stepped_gid = dl.gid;
}

function dx_check_corrupt()
{
   var el;
   if (dx.dxcfg_parse_error) {
      el = w3_el('id-dxcfg-err');
      w3_innerHTML(el, 'Warning: corrupt dx_config.json file');
      w3_add(el, 'w3-text-css-orange');
   }

   if (dx.dxcomm_cfg_parse_error) {
      el = w3_el('id-dxcomm-cfg-err');
      w3_innerHTML(el, 'Warning: corrupt dx_community_config.json file');
      w3_remove(el, 'w3-text-css-yellow');
      w3_add(el, 'w3-text-css-orange');
   }
}

function dx_admin_pwd_cb(path, val)
{
   //console.log('dx_admin_pwd_cb path='+ path +' val='+ val);
   w3_string_cb(path, val);
	ext_valpwd('admin', val, ws_wf);
	// calls ext_hasCredential() cb func again with new badp value
}

// note that an entry can be cloned by selecting it, but then using the "add" button instead of "modify"
function dx_show_edit_panel(ev, gid, from_shortcut)
{
	dx.keys = ev? { shift:ev.shiftKey, alt:ev.altKey, ctrl:ev.ctrlKey, meta:ev.metaKey } : { shift:0, alt:0, ctrl:0, meta:0 };
	dx.o.gid = gid;
	
	//console.log('dx_show_edit_panel ws='+ ws_wf.stream);
   // must validate admin for write access to DB_STORED
	if (dx.db == dx.DB_STORED) {
      ext_hasCredential('admin',
         function(badp) {
            if (badp != kiwi.BADP_OK) {
               // show pwd entry panel
               // , 'dx_admin_pwd_cb')
               extint_panel_hide();    // committed to displaying edit panel, so remove any ext panel
               var s =
                  w3_inline('',
                     w3_div('w3-medium w3-text-aqua w3-bold', 'DX labels'),
                     w3_select('w3-margin-L-32/w3-label-inline w3-text-white /w3-text-red', 'database:', '', 'dx.db', dx.db, dx.db_s, 'dx_database_cb', 0)
                  ) +
                  w3_div('w3-margin-T-16',
                     w3_checkbox('/w3-label-inline w3-label-not-bold w3-text-white/',
                        'filter by time/day-of-week', dx.DB_STORED.toString() +'-filter-tod', dx.filter_tod[dx.DB_STORED], 'dx_time_dow_cb'),
                     w3_text('id-dxcfg-err w3-block')
                  ) +
                  w3_div('w3-margin-T-16',
			            w3_input('//w3-margin-T-8 w3-padding-small|width:80%', 'Admin password', 'dx.pwd', '', 'dx_admin_pwd_cb'),
			            w3_text('w3-margin-T-4', 'editing the stored DX labels of this Kiwi requires admin privileges')
			         );
	            ext_panel_set_name('dx');
               ext_panel_show(s, null, null, null, true);   // true: show help button
               ext_set_controls_width_height(550, 290);
               // put the cursor in (i.e. select) the password field
               if (from_shortcut != true)
                  w3_field_select('id-dx.pwd', {mobile:1});
	            dx_check_corrupt();
            } else {
               dx_show_edit_panel2();
            }
         }, null, ws_wf);
   } else {
      dx_show_edit_panel2();
   }
}

/*
	UI improvements:
		tab between fields
*/

function dx_show_edit_panel2()
{
   var cbox, cbox_container, label;
   dx_color_init();

   if (dx.db != dx.DB_COMMUNITY) {
      var gid = dx.o.gid;
      var mode = isArg(cur_mode)? cur_mode : 'am';
      var freq_kHz_str = isArg(freq_displayed_kHz_str)? freq_displayed_kHz_str : '10000';
      //console_log_dbgUs('dx_show_edit_panel2 gid='+ gid +' db='+ dx.db);
   
      if (gid == -1) {
         //console.log('DX EDIT new entry');
         console.log('DX EDIT new f='+ freq_car_Hz +'|'+ freq_displayed_Hz +'|'+ freq_kHz_str +' m='+ mode);
         dx.o.fr = +(freq_kHz_str) + kiwi.freq_offset_kHz;
         dx.o.lo = dx.o.hi = dx.o.o = dx.o.s = 0;
         dx.o.fm = kiwi.modes_idx[mode];
         dx.o.ft = dx.DX_QUICK_0;
         dx.o.fd = dx.DX_DOW_BASE;
         dx.o.begin = 0;
         dx.o.end = 0;
         dx.o.i = dx.o.n = dx.o.p = '';
      } else {
         label = dx.list[gid];
         console.log('DX EDIT entry #'+ gid +' prev: f='+ label.freq +'|'+ label.carrier.toFixed(2) +' flags='+ label.flags.toHex() +' i='+ label.ident +' n='+ label.notes);
         dx.o.fr = label.carrier.toFixed(2);		// starts as a string, validated to be a number
         dx.o.lo = label.lo;
         dx.o.hi = label.hi;
         dx.o.o = label.mkr_off;
         dx.o.s = label.sig_bw;
         dx.o.fm = dx_decode_mode(label.flags);
         dx.o.ft = ((label.flags & dx.DX_TYPE) >> dx.DX_TYPE_SFT);
         dx.o.fd = ((label.flags & dx.DX_DOW) >> dx.DX_DOW_SFT);
         if (dx.o.fd == 0) {
            dx.o.fd = dx.DX_DOW_BASE;
            kiwi_trace('$SHOULD NOT OCCUR');
         }
         dx.o.begin = label.begin;
         dx.o.end = label.end;
         dx.o.i = kiwi_decodeURIComponent('dx_ident', label.ident);
         dx.o.n = kiwi_decodeURIComponent('dx_notes', label.notes);
         dx.o.p = kiwi_decodeURIComponent('dx_params', label.params);
         //console.log('dx.o.i='+ dx.o.i +' len='+ dx.o.i.length);
      }

      // convert from dx mode order to mode menu order
      dx.o.mm = w3_array_el_seq(kiwi.mode_menu, kiwi.modes_uc[dx.o.fm]);
      //console_log_dbgUs(dx.o);
   
      // quick key combo to toggle 'active' type without bringing up panel
      if (dx.db == dx.DB_STORED && gid != -1 && dx.keys.shift && dx.keys.alt) {
         //console.log('DX COMMIT quick-active entry #'+ dx.o.gid +' f='+ dx.o.fr);
         //console.log(dx.o);
         var type = dx.o.ft;
         type = (type == dx.DX_QUICK_0)? dx.DX_QUICK_1 : dx.DX_QUICK_0;
         dx.o.ft = type;
         var flags = (dx.o.fd << dx.DX_DOW_SFT) | (type << dx.DX_TYPE_SFT) | dx_encode_mode(dx.o.fm);
         dx_send_update('DX_QUICK', dx.o.gid, dx.o.fr, flags, dx.o);
         return;
      }

      dx.o.fr = (parseFloat(dx.o.fr)).toFixed(2);
   
      dx.o.pb = '';
      if (dx.o.lo || dx.o.hi) {
         if (dx.o.lo == -dx.o.hi) {
            dx.o.pb = (Math.abs(dx.o.hi)*2).toFixed(0);
         } else {
            dx.o.pb = dx.o.lo.toFixed(0) +', '+ dx.o.hi.toFixed(0);
         }
      }
   }

   // Committed to displaying edit panel, so remove any extension panel that might be showing (and clear ext menu).
   // Important to do this here. Couldn't figure out reasonable way to do this in extint_panel_show()
	extint_panel_hide();		
	
	var s1 =
	   w3_inline('',
		   w3_div('w3-medium w3-text-aqua w3-bold', 'DX labels'),
		   w3_select('w3-margin-L-32/w3-label-inline w3-text-white /w3-text-red', 'database:', '', 'dx.db', dx.db, dx.db_s, 'dx_database_cb', 0)
		);
	
	var s2;
	if (dx.db == dx.DB_STORED) {
      var dow_s = '';
      var dow = dx.o.fd;
      if (dow == 0) dow = dx.o.fd = dx.DX_DOW_BASE;
      console.log('DX g='+ gid +' dow='+ dow +'|'+ dow.toHex(2) +' f='+ (+dx.o.fr).toFixed(2) +' b='+ dx.o.begin +' e='+ dx.o.end +' '+ dx.o.i);
	   for (var i = 0; i < 7; i++) {
	      var day = dow & (1 << (6-i));
	      var fg = day? 'black' : 'white';
	      var bg = day? dx.dow_colors[i] : 'darkGrey';
	      dow_s += w3_button('w3-dummy//w3-round w3-padding-tiny'+
	         (i? ' w3-margin-L-4':'') +'|color:'+ fg +'; background:'+ bg,
	         'MTWTFSS'[i], 'dx_dow_button', i);
	   }
	   dow_s = w3_inline('', dow_s);
	   
	   // undo 0000-2400 default setup for benefit of EiBi
	   var begin_s, end_s;
	   var begin = +(dx.o.begin), end = +(dx.o.end);
	   if (begin == 0 && (end == 0 || end == 2400)) {
	      begin_s = end_s = '';
	   } else {
	      begin_s = begin.toFixed(0).leadingZeros(4);
	      end_s = end.toFixed(0).leadingZeros(4);
	   }
	   
      // show "Tn" in type menu entries that are otherwise blank but have list entry users
      var type_menu = kiwi_deep_copy(dxcfg.dx_type);
      if (dx.types_n) {
         for (i = 0; i < dx.DX_N_STORED; i++) {
            if (dx.types_n[i] != 0 && type_menu[i].name == '') {
               type_menu[i].name = '(T'+ i +')';    // make placeholder entry in menu
               //console.log('empty type menu entry: T'+ i +' #'+ dx.types_n[i]);
            }
         }
      }
      //console.log(type_menu);

	   s2 =
         w3_divs('w3-text-white/w3-margin-T-8',
            w3_inline('w3-halign-space-between/',
               w3_input('w3-padding-small||size=10', 'Freq', 'dx.o.fr', dx.o.fr, 'dx_freq_cb'),
               w3_select('w3-text-red', 'Mode', '', 'dx.o.mm', dx.o.mm, kiwi.mode_menu, 'dx_sel_cb'),
               w3_input('w3-padding-small||size=10', 'Passband', 'dx.o.pb', dx.o.pb, 'dx_passband_cb'),
               w3_select('w3-text-red', 'Type', '', 'dx.o.ft', dx.o.ft, type_menu, 'dx_sel_cb'),
               w3_input('w3-padding-small||size=8', 'Offset', 'dx.o.o', dx.o.o, 'dx_num_cb')
            ),
      
            w3_input('w3-label-inline/w3-margin-L-8 w3-padding-small', 'Ident', 'dx.o.i', '', 'dx_string_cb'),
            w3_input('w3-label-inline/w3-margin-L-8 w3-padding-small', 'Notes', 'dx.o.n', '', 'dx_string_cb'),
            
            w3_inline('w3-halign-space-between/',
               w3_input('w3-label-inline/w3-margin-L-8 w3-padding-small||size=40', 'Extension', 'dx.o.p', '', 'dx_string_cb'),
               w3_input('/w3-nowrap w3-margin-left w3-label-inline/w3-margin-L-8 w3-padding-small||size=5', 'Sig bw', 'dx.o.s', dx.o.s, 'dx_num_cb')
            ),
      
            w3_inline('w3-hspace-16',
               w3_text('w3-text-white w3-bold', 'Schedule (UTC)'),
               dow_s,
               w3_input('w3-label-inline/w3-margin-L-8 w3-padding-small', 'Begin', 'dx.o.begin', begin_s, 'dx_sched_time_cb'),
               w3_input('w3-label-inline/w3-margin-L-8 w3-padding-small', 'End', 'dx.o.end', end_s, 'dx_sched_time_cb')
            ),

            w3_inline('w3-hspace-16',
               w3_button('id-dx-modify w3-yellow', 'Modify', 'dx_modify_cb'),
               w3_button('id-dx-add w3-green', 'Add', 'dx_add_cb'),
               w3_button('w3-red', 'Delete', 'dx_delete_cb'),
               w3_div('w3-margin-T-4',
                  w3_checkbox('/w3-label-inline w3-label-not-bold w3-text-white/',
                     'filter by time/day-of-week', dx.DB_STORED.toString() +'-filter-tod', dx.filter_tod[dx.DB_STORED], 'dx_time_dow_cb'),
                  w3_text('id-dxcfg-err w3-margin-T-4', 'Create new label with Add button')
               )
            )
         );
   } else
   
   if (dx.db == dx.DB_EiBi) {
      //console_log_dbgUs('dx_show_edit_panel2 DB_EiBi');
      cbox_container = 'w3-halign-space-around/|flex-basis:130px';
      cbox = '/w3-label-inline w3-label-not-bold w3-round w3-padding-small/';
      var checkbox = function(type) {
         var idx = dx_type2idx(type);
         var svc = (dx.eibi_types_mask & (1 << idx))? 1:0;
         //console.log('svc='+ svc +' m='+ dx.eibi_types_mask.toHex(8) +' type='+ type.toHex(4) +' idx='+ idx +' '+ dx.eibi_svc_s[idx]);
         return w3_checkbox(cbox, dx.eibi_svc_s[idx], 'eibi-cbox'+ idx, svc, 'dx_eibi_svc_cb', idx);
      };

	   s2 =
	      w3_inline('',
            //w3_div('w3-text-css-yellow w3-margin-TB-16', 'The EiBi database is read-only'),
            w3_checkbox('w3-margin-TB-16/w3-label-inline w3-label-not-bold w3-text-white/w3-hspace-32',
               'select all', 'eibi-all', dx.eibi_types_mask == 0xffffffff, 'dx_eibi_all_cb'),
            w3_checkbox('/w3-label-inline w3-label-not-bold w3-text-white/w3-margin-L-32',
               'filter by time/day-of-week', dx.DB_EiBi.toString() +'-filter-tod', dx.filter_tod[dx.DB_EiBi], 'dx_time_dow_cb')
         ) +
         
         w3_div('w3-valign-col w3-round w3-padding-TB-10 w3-gap-10|background:whiteSmoke',
            w3_inline(cbox_container,
               checkbox(dx.DX_BCAST),
               checkbox(dx.DX_UTIL),
               checkbox(dx.DX_TIME)
            ),
            w3_inline(cbox_container,
               checkbox(dx.DX_ALE),
               checkbox(dx.DX_HFDL),
               checkbox(dx.DX_MILCOM)
            ),
            w3_inline(cbox_container,
               checkbox(dx.DX_CW),
               checkbox(dx.DX_FSK),
               checkbox(dx.DX_FAX)
            ),
            w3_inline(cbox_container,
               checkbox(dx.DX_AERO),
               checkbox(dx.DX_MARINE),
               checkbox(dx.DX_SPY)
            )
         );
   } else
   
   // DB_COMMUNITY
   {
      //console_log_dbgUs('dx_show_edit_panel2 DB_COMMUNITY');
      cbox_container = 'w3-margin-L-32 w3-halign-space-around/|flex-basis:130px';
      cbox = 'w3-round w3-padding-small';
      label = function(idx) {
         var dx_type = dxcomm_cfg.dx_type[idx];
         return w3_label(cbox, dx_type.name, 'dxcomm'+ idx);
      };

      s2 =
         w3_checkbox('w3-margin-T-8/w3-label-inline w3-label-not-bold w3-text-white/',
            'filter by time/day-of-week', dx.DB_COMMUNITY.toString() +'-filter-tod', dx.filter_tod[dx.DB_COMMUNITY], 'dx_time_dow_cb') +
         w3_text('id-dxcomm-cfg-err w3-margin-T-4 w3-margin-B-12 w3-text-css-yellow',
            'The community database is read-only and downloaded periodically. <br>' + dx.last_community_download);

      if (!dx.dxcomm_cfg_parse_error)
         s2 +=
            w3_div('w3-valign-col w3-round w3-padding-TB-15 w3-gap-15|background:whiteSmoke',
               w3_inline(cbox_container,
                  label(dx.T3_BCAST),
                  label(dx.T4_UTIL),
                  label(dx.T5_TIME)
               ),
               w3_inline(cbox_container,
                  label(dx.T6_ALE),
                  label(dx.T7_HFDL),
                  label(dx.T8_MILCOM)
               ),
               w3_inline(cbox_container,
                  label(dx.T9_CW),
                  label(dx.T10_FSK),
                  label(dx.T11_FAX)
               ),
               w3_inline(cbox_container,
                  label(dx.T12_AERO),
                  label(dx.T13_MARINE),
                  label(dx.T14_SPY)
               ),
               w3_inline(cbox_container,
                  label(dx.T2_HAM),
                  label(dx.T1_CHANNEL),
                  label(dx.T0_RESERVED)
               )
            );
   }
	
	// can't do this as initial val passed to w3_input above when string contains quoting
	ext_panel_set_name('dx');
	ext_panel_show(s1 + s2, null,
	   function() {      //show func
         if (dx.db == dx.DB_STORED) {
            var el = w3_el('dx.o.i');
            el.value = dx.o.i;
            w3_el('dx.o.n').value = dx.o.n;
            w3_el('dx.o.p').value = dx.o.p;
      
            // change focus to input field
            // FIXME: why doesn't field select work?
            //console.log('dx.o.i='+ el.value);
            //w3_field_select(el, {mobile:1});
         } else
         if (dx.db == dx.DB_EiBi) {
            for (var i = 0; i < dx.DX_N_EiBi; i++) {
               w3_color('eibi-cbox'+ i +'-label', 'black', dx.eibi_colors[i]);
            }
         } else {    // DB_COMMUNITY
            for (i = 0; i <= dx.DX_N_COMM; i++) {
               w3_color('id-dxcomm'+ i +'-label', 'black', dx.stored_colors_light[i]);
               //console.log('dxcomm'+ i +' '+ dx.stored_colors_light[i]);
            }
         }
         if (dx.help) {
            setTimeout(function() {
               extint_help_click_now();
            }, 1000);
            dx.help = false;
         }
      },
	   function() {
	      //console.log('dx_show_edit_panel2 HIDE_FUNC');
	   },
	   true     // show help button
   );
   
	ext_set_controls_width_height(550, 290);
	dx_check_corrupt();
}

function dx_database_cb(path, idx, first, opt, from_shortcut)
{
   //console.log('first='+ first +' ctrlAlt='+ ctrlAlt);
   if (first) return;
   dx.list = [];
   console_log_dbgUs('DX DB-SWITCHED db='+ idx +'|'+ dx.db +' opt='+ kiwi_JSON(opt) +' from_shortcut='+ from_shortcut);

   if (w3_opt(opt, 'toggle')) {
      dx.filter_tod[dx.db] ^= 1;
      dx_time_dow_cb(dx.db.toString() +'-filter-tod', dx.filter_tod[dx.db]);
   } else {
      dx.db = +idx;
      var called_from_menu = (opt == '0');
      if (w3_opt(opt, 'open')) {
         dx_show_edit_panel(null, -1, from_shortcut);
      } else {
         if (!called_from_menu && ext_panel_displayed('dx')) {
            dx_close_edit_panel();
         }
         if (called_from_menu) {
            dx_show_edit_panel(null, -1, from_shortcut);    // show new panel contents
         }
      }
   }
   
   //console.log('last_db SAVE-select '+ dx.db);
   kiwi_storeWrite('last_db', dx.db);
   bands_init();
   mkscale();
   mk_band_menu();
	dx_schedule_update();
}

function dx_eibi_all_cb(path, checked, first)
{
   if (first) return;
   var v = checked? 1:0;
   for (var i = 0; i < dx.DX_N_EiBi; i++)
      dx_eibi_svc_cb('eibi-cbox'+ i, v, false, i);
}

function dx_time_dow_cb(path, checked, first)
{
   if (first) return;
   //console.log('dx_time_dow_cb path='+ path +' checked='+ checked);
   var db = parseInt(path);
   dx.filter_tod[db] = checked? 1:0;
   w3_checkbox_set(path, checked);
	dx_schedule_update();
}

function dx_eibi_svc_cb(path, checked, first, cbp)
{
   if (first) return;
   var idx = +cbp;
   var v = checked? 1:0;
   w3_checkbox_set(path, checked);
   if (v)
      dx.eibi_types_mask |= 1 << idx;
   else
      dx.eibi_types_mask &= ~(1 << idx);
	dx_schedule_update();
}

function dx_close_edit_panel(id)
{
	if (id) w3_radio_unhighlight(id);
	extint_panel_hide();
	
	// NB: Can't simply do a dx_schedule_update() here as there is a race for the server to
	// update the dx list before we can pull it again. Instead, the add/modify/delete ajax
	// response will call dx_update() directly when the server has updated.
}

function dx_send_update(from, gid, fr, flags, dx_o)
{
   //console.log('dx_send_update from='+ from +' gid='+ gid +' freq='+ fr);
   dx.retry = isNull(dx_o)? {} : kiwi_deep_copy(dx_o);
   dx.retry.gid = gid;
   dx.retry.fr = fr;
   dx.retry.flags = flags;
   
   if (gid != -1 && fr == -1) {
      // delete
	   wf_send('SET DX_UPD g='+ gid +' f=-1');
   } else {
      // modify or add
      wf_send('SET DX_UPD g='+ gid +' f='+ fr +' lo='+ dx_o.lo.toFixed(0) +' hi='+ dx_o.hi.toFixed(0) +
         ' o='+ dx_o.o.toFixed(0) +' s='+ dx_o.s.toFixed(0) +' fl='+ flags +' b='+ dx_o.begin +' e='+ dx_o.end +
         ' i='+ encodeURIComponent(dx_o.i +'x') +' n='+ encodeURIComponent(dx_o.n +'x') +' p='+ encodeURIComponent(dx_o.p +'x'));
   }
}

function dx_send_update_retry()
{
   dx_send_update('dx_send_update_retry', dx.retry.gid, dx.retry.fr, dx.retry.flags, dx.retry);
}

function dx_button_highlight()
{
	var el = w3_el((dx.o.gid == -1)? 'id-dx-add' : 'id-dx-modify');
	el.style.border = '5px double red';
}

function dx_modify_cb(id, val)
{
	console_log_dbgUs('DX COMMIT modify entry #'+ dx.o.gid +' f='+ dx.o.fr);
	console_log_dbgUs(dx.o);
	if (dx.o.gid == -1) return;
   var flags = (dx.o.fd << dx.DX_DOW_SFT) | (dx.o.ft << dx.DX_TYPE_SFT) | dx_encode_mode(dx.o.fm);

	if (dx.o.fr < 0) dx.o.fr = 0;
	dx_send_update('DX_MODIFY', dx.o.gid, dx.o.fr, flags, dx.o);
	var el = w3_el('id-dx-modify');
	setTimeout(function() {
	   dx_close_edit_panel(id);
	   el.style.border = '';
	}, 250);
}

function dx_add_cb(id, val)
{
	//console.log('DX COMMIT new entry');
	//console.log(dx.o);
   var flags = (dx.o.fd << dx.DX_DOW_SFT) | (dx.o.ft << dx.DX_TYPE_SFT) | dx_encode_mode(dx.o.fm);

	if (dx.o.fr < 0) dx.o.fr = 0;
	dx_send_update('DX_ADD', -1, dx.o.fr, flags, dx.o);
	setTimeout(function() {dx_close_edit_panel(id);}, 250);
	owrx.dx_click_gid_last_until_tune = owrx.dx_click_gid_last_stored = undefined;    // because gid's may get renumbered
   dx_show(false, -1);
}

function dx_delete_cb(id, val)
{
	//console.log('DX COMMIT delete entry #'+ dx.o.gid);
	//console.log(dx.o);
	if (dx.o.gid == -1) return;
	dx_send_update('DX_DELETE', dx.o.gid, -1, 0, null);
	setTimeout(function() {dx_close_edit_panel(id);}, 250);
	owrx.dx_click_gid_last_until_tune = owrx.dx_click_gid_last_stored = undefined;    // because gid's may get renumbered
   dx_show(false, -1);
}

function dx_evt(path, cb_param, first, evt)
{
   var p = cb_param.split(',');
   var gid = p[0];
   var cmkr_x = p[1];
   //console.log('dx_evt '+ evt.type +' gid='+ gid +' cmkr_x='+ cmkr_x);
   var rv = true;
   //canvas_log('DX '+ evt.type);
   
   switch (evt.type) {
      case 'mouseenter':   rv = dx_enter(evt, cmkr_x); break;
      case 'mouseleave':   rv = dx_leave(evt, cmkr_x); break;
      case 'click':        
      case 'hold':        
      case 'touchend':     rv = dx_click(evt, gid); break;
      default:             /* canvas_log('DEF '+ evt.type); */ rv = ignore(evt); break;
   }
   
   return rv;
}

function dx_sig_bw(gid)
{
   var label = (gid >= 0)? dx.list[gid] : null;
   if (label && isArg(label.sig_bw) && label.sig_bw != 0) {
      var el = w3_el('id-dx-sig');
      if (!el) {
         // Possible that label is currently off-screen because labels haven't been refreshed yet.
         // So mark it such that the refresh render process will find it.
         dx.sig_bw_render_gid = gid;
         if (dx.step_dbg) console.log('$dx_sig_bw OFF SCREEN sig_bw_render_gid='+ gid);
         return;
      }
      
      var hbw = label.sig_bw/2;
	   var f_base = label.freq - kiwi.freq_offset_kHz;
		var f_baseHz = f_base * 1000;
      // place sig bw around center of passband where label is
      var xl = scale_px_from_freq((f_base - hbw) * 1000, g_range);
      var xr = scale_px_from_freq((f_base + hbw) * 1000, g_range);
		var mkr_Hz = f_baseHz + passband_offset_dxlabel(kiwi.modes_lc[dx_decode_mode(label.flags)], label.params, label.lo, label.hi);
      //console.log('$dx_sig_bw f='+ f_base +' hbw='+ hbw +' xl='+ xl +' xr='+ xr +' mkr_Hz='+ mkr_Hz +' mkr_off='+ label.mkr_off);
      //console.log(label);
      el.style.left = px(xl);
      el.style.width = px(xr-xl);
      el.style.background = label.color;

      // check if mkr is already being used by offset (e.g. NDB label)
      if (label.mkr_off != 0) return;
      dx_canvas.style.left = px(scale_px_from_freq(mkr_Hz, g_range)-dx_car_w/2);
      dx_show(true);
      if (dx.step_dbg) console.log('$dx_sig_bw SHOW gid='+ gid);
      dx.sig_bw_render_gid = gid;
   } else {
      // this label has no sig-bw
      if (dx.step_dbg) console.log('$dx_sig_bw OFF');
		dx_show(false);
      dx.sig_bw_render_gid = -1;
   }
}

function dx_click(ev, gid)
{
   var hold = (ev.type == 'hold');
   //canvas_log('DXC '+ ev.type);
   //event_dump(ev, 'dx_click');
	if (!hold && ev.shiftKey && dx.db == dx.DB_STORED) {
		dx_show_edit_panel(ev, gid);
	} else {
	   // easier to do this way since it's about the only element that
	   // intercepts mousedown/touchstart without subsequent propagation
	   w3_menu_close('dx_click');
	   dx_sig_bw(gid);
	   
	   // allow anchor links within the ident to be clicked
	   if (ev.target && ev.target.nodeName == 'A') {
	      console.log('dx_click: link within label');
		   dx.ctrl_click = false;
		   return ev;     // let click to through to anchor element
	   }
	   
	   owrx.dx_click_gid_last_stored = (dx.db == dx.DB_STORED)? gid : undefined;
	   owrx.dx_click_gid_last_until_tune = gid;
	   var label = dx.list[gid];
	   var freq = label.freq;
	   var f_base = freq - kiwi.freq_offset_kHz;
		var mode = kiwi.modes_lc[dx_decode_mode(label.flags)];
		var lo = label.lo;
		var hi = label.hi;
		var params = label.params;
		
      extint.extname = extint.param = null;
      if (isArg(params)) {
         params = decodeURIComponent(params);
         
         var ap = params.split('&');
         //console.log(ap);
         ap.forEach(
            function(p,i) {
               if (p == '') return;
               //console.log('p=<'+ p +'>');
               var a = p.split(/[,=]+/);
               //console.log(a);
               extint.former_exts.forEach(
                  function(e) {
                     if (e.startsWith(a[0])) {
                        //console.log('MATCH '+ a[0]);
                        extint.param = isArg(a[1])? a[1] : '';
                        //console.log('dx_click: extint_open='+ e +' param='+ extint.param);
                        extint_open(e);
                        ap[i] = undefined;   // remove the param we just processed
                     }
                  }
               );
            }
         );
         //console.log(ap);
         ap = kiwi_array_remove_undefined(ap);
         params = ap.join('&');
         //console.log(params);
         
         var ext = (params == '')? [] : params.split(',');
         if (mode == 'drm') {
            extint.extname = 'drm';
            ext.push('lo:'+ lo);    // forward passband info from dx label panel to keep DRM ext from overriding it
            ext.push('hi:'+ hi);
         } else {
            extint.extname = ext[0];
            ext.shift();
         }
         if (ext[0] == '*') ext[0] = freq.toFixedNZ(2);
         extint.param = ext.join(',');
      }
      var extname = extint.extname? extint.extname : '';
		//console_log_dbgUs('dx_click gid='+ gid +' f='+ freq +'|'+ f_base +' mode='+ mode +' cur_mode='+ cur_mode +' lo='+ lo +' hi='+ hi +' params=<'+ params +'> extname=<'+ extname +'> param=<'+ extint.param +'>');

      // EiBi database frequencies are dial/carrier (i.e. not pbc)
      if (dx.db == dx.DB_EiBi) {
         
      }

      extint.mode_prior_to_dx_click = cur_mode;
		freqmode_set_dsp_kHz(f_base, mode, { open_ext:true, no_clear_last_gid:1 });
		if (lo || hi) {
		   ext_set_passband(lo, hi, false, f_base);
		}
		
		// open specified extension
		// setting DRM mode above opens DRM extension
		var check_ext;
		if (kiwi_isMobile()) {
		   check_ext = hold;    // on mobile open extension only on a hold, not just a click
		} else {
         //check_ext = (!hold && ((dx.db == dx.DB_EiBi)? ev.shiftKey : !any_alternate_click_event(ev)));
         check_ext = (!hold && !any_alternate_click_event(ev));
         //if (!check_ext && hold) check_ext = true;    // test hold on desktop
      }
      //canvas_log('CKEXT='+ check_ext);

		if (mode != 'drm' && check_ext && !dx.ctrl_click && params) {
         //canvas_log('OPEN '+ extint.extname);
		   console_log_dbgUs('dx_click ext='+ extint.extname +' <'+ extint.param +'>');
		   
		   // (we use touch-hold now)
		   /*
		   // give mobile a chance to abort
		   if (kiwi_isMobile()) {
		      dx.extname = extint.extname;
            var s =
               w3_divs('/w3-margin-B-8',
                  w3_text('w3-margin-left w3-text-white', 'Open '+ dx.extname +'<br>extension?'),
                  w3_button('w3-green w3-margin-left', 'Confirm', 'dx_ext_open'),
                  w3_button('w3-red w3-margin-left', 'Cancel', 'confirmation_panel_close')
               );
		      confirmation_show_content(s, 175, 140);
		   } else
		   */
		   {
			   extint_open(extint.extname, 250);
			}
		}
		
		dx.ctrl_click = false;
	}
	return cancelEvent(ev);		// keep underlying canvas from getting event
}

/*
function dx_ext_open()
{
   confirmation_panel_close();
   extint_open(dx.extname);
}
*/

// Any var we add to the div in dx_label_render_cb() is undefined in the div that appears here,
// so use the number embedded in id="" to create a reference.
// Even the "data-" thing doesn't work.

function dx_enter(ev, cmkr_x)
{
   var tgt = ev.target;
	dx.z_save = tgt.style.zIndex;
	tgt.style.zIndex = 999;
	dx.bg_color_save = tgt.style.backgroundColor;
	tgt.style.backgroundColor = w3_contains(tgt, 'cl-dx-label-filtered')? 'lightGrey' : (w3_contains(tgt, 'dx-has-ext')? 'magenta' : 'yellow');

	var dx_line = w3_el('id-dx-line_'+ tgt.id.parseIntEnd());
	//console.log('tgt.id='+ tgt.id +' id-dx-line_'+ tgt.id.parseIntEnd() +'='+ dx_line);
	dx_line.zIndex = 998;
	dx_line.style.width = '3px';
	
	if (+cmkr_x) {
		dx_canvas.style.left = px(cmkr_x-dx_car_w/2);
      w3_visible(dx_canvas, true);
	}
}

function dx_leave(ev, cmkr_x)
{
   var tgt = ev.target;
	tgt.style.zIndex = dx.z_save;
	tgt.style.backgroundColor = dx.bg_color_save;

	var dx_line = w3_el('id-dx-line_'+ tgt.id.parseIntEnd());
	dx_line.zIndex = 110;
	dx_line.style.width = '1px';
	
	if (+cmkr_x) {
      dx_canvas.style.visibility = dx.sig_bw_vis;
      dx_canvas.style.left = dx.sig_bw_left;
	}
}

function dx_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'DX label help') +
         w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
            w3_div('w3-margin-R-8',
               'There are three types of DX labels seen in the area above the frequency scale: <br>' +
               '1) Labels from a stored database, editable by the Kiwi owner/admin. <br>'+
               '2) Labels from a read-only copy of the <a href="http://www.eibispace.de" target="_blank">EiBi database</a> that cannot be modified. <br>' +
               '3) Labels from a downloaded, read-only, community database aggregated from contributions made on the Kiwi forum. <br>' +
               '<br>' +
               
               'The DX label control panel has a <i>database</i> menu to select between these three. ' +
               'Similarly, there are selection entries in the right-click menu (double-finger tap on mobile devices). ' +
               'The keyboard shortcut keys "\\" and "|" also switch between the databases with the later also opening the control panel.' +
               '<br><br>' +
               
               'When zoomed out the number of labels shown is limited to prevent overwhelming the display. ' +
               'This is why some labels will only appear as you zoom in. The same is true if too many EiBi categories ' +
               'are selected enabling a large number of labels.' +
               '<br><br>' +
               
               'Information about using the controls for editing the stored labels can be found ' +
               '<a href="http://kiwisdr.com/info#id-user-marker" target="_blank">here</a>.' +
               '<br><br>' +
               
               'Because there are so many EiBi labels (about 6000) they are organized into 12 categories with visibility ' +
               'selected by checkboxes on the control panel.' +
               '<br><br>' +
               
               'Labels from any of the databases can be filtered by their schedule (UTC time & day-of-week) ' +
               'specified in the database. If filtering is disabled labels outside their scheduled time appear with a lighter category color and a dashed border. ' +
               'When filtering is enabled the label display will refresh every 5 minutes so it reflects the current schedule.' +
               '<br><br>' +
               
               'When a label is moused-over a popup is shown with additional information. ' +
               'On Safari and Chrome the popups take a few seconds to appear. EiBi information includes the station\'s home country, transmission schedule, ' +
               'language and target area (if any). ' +
               'Information about the EiBi database, including descriptions of the language and target codes seen in the mouse-over popups, ' +
               '<a href="http://kiwisdr.com/files/EiBi/README.txt" target="_blank">can be found here.</a>' +
               '<br><br>' +
         
               'Labels that have an extension specified have a black bar on the right part of the label. <br>' +
               'This is for consistency between the desktop and mobile interfaces. <br>' +
               'Other behavior is device specific: <br><br>' +
               'Desktop:' +
                  '<ul>' +
                     '<li>Extension labels are magenta when moused-over as opposed to the usual yellow.</li>' +
                     '<li>Labels outside their scheduled time (if any) will mouse-over grey.</li>' +
                     '<li>Clicking the label sets the freq/mode and opens the extension.</li>' +
                     '<li>For the stored database shift-clicking opens the DX labels panel (admin only).</li>' +
                     '<li>PC</li><ul>' +
                        '<li>Alt-click sets the freq/mode without openning the extension.</li>' +
                        '<li>Shift-alt-click to toggle the label active (admin only).</li>' +
                     '</ul>' +
                     '<li>Mac</li><ul>' +
                        '<li>Alt/option-click sets the freq/mode without openning the extension.</li>' +
                        '<li>Shift-alt/option-click to toggle the label active (admin only).</li>' +
                        '<li>On Mac the control key can be used in place of alt/option.</li>' +
                     '</ul>' +
                  '</ul>' +
               'Mobile:' +
                  '<ul>' +
                     '<li>Touching a label sets the freq/mode.</li>' +
                     '<li>Touch-hold a label to set freq/mode and open the extension.</li>' +
                     '<li>After touching a label use the right-click menu to open edit panel for that label.</li>' +
                  '</ul>' +

               'For stored labels the extension is set per-label in the DX labels panel and the extension opened when the label is clicked. ' +
               'For EiBi labels an extension is automatically assumed for many of the categories, ' +
               'but is only opened if the label is shift-clicked (note difference from stored label behavior, ' +
               'a non-shift click simply selects the frequency/mode).' +
               '<br><br>' +
               
               'URL parameters: (use "dx=<i>comma separated parameters</i>" in the browser URL)<br>' +
               'Select database: ' + w3_text('|color:orange', '[012] <i>or</i> stored eibi community') + '<br>' +
               'All: ' + w3_text('|color:orange', 'open filter[:0] <br>') +
               '<br> EiBi: ' + w3_text('|color:orange', 'none bcast utility time ale hfdl milcom cw fsk fax aero marine spy <br>') + '<br>' +
               'An optional first digit specifies the database to display (EiBi "1" is the default). Or use the database name. ' +
               'Specify <i>open</i> to also open the DX labels panel. ' +
               'For EiBi: By default all the category checkboxes and "<i>filter by time/day-of-week</i>" are checked. ' +
               'Specifying <i>none</i> will uncheck all the categories. Specifying the individual category names will then toggle the checkbox state.' +
               '<br><br>' +

               'For example, to select only the EiBi ALE and FAX categories: <i>dx=none,ale,fax</i> <br>' +
               'To select all except the CW and Time categories: <i>dx=cw,time</i> <br>' +
               'To just select the EiBi database: <i>dx</i> (e.g. <i>my_kiwi:8073/?dx</i>)<br>' +
               'To also bring up the EiBi control panel: <i>dx=op</i> (e.g. <i>my_kiwi:8073/?dx=op</i>)<br>' +
               'Keywords are case-insensitive and can be abbreviated. For example: <br>' +
               '<i>dx=n,bc,f:0</i> (show only broadcast stations without time/day filtering) <br>' +
               'To select the stored label database: <i>dx=0</i> or <i>dx=sto</i><br>' +
               ''
            )
         );

      confirmation_show_content(s, 650, 375);
      w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
   }
   return true;
}


////////////////////////////////
// #s-meter
////////////////////////////////

var smeter_width;
var SMETER_RHS = 38;
var SMETER_SCALE_HEIGHT = 29;
var SMETER_BIAS = 127;
var SMETER_INPUT_MAX = 3.4;
var SMETER_INPUT_RANGE = (SMETER_BIAS + SMETER_INPUT_MAX);
var SMETER_DISPLAY_MAX = -6;     // don't display all the way to SMETER_INPUT_MAX
var SMETER_DISPLAY_RANGE = (SMETER_BIAS + SMETER_DISPLAY_MAX);
var sMeter_ctx;

// 6 dB / S-unit
var bars = {
	dBm:  [ -121, -109, -97,  -85,  -73,  -63,   -53,   -33,   -13   ],
	text: [ 'S1', 'S3', 'S5', 'S7', 'S9', '+10', '+20', '+40', '+60' ]
};

function smeter_dBm_biased_to_x(dBm_biased)
{
   if (dBm_biased > SMETER_DISPLAY_RANGE)
      dBm_biased = SMETER_DISPLAY_RANGE;     // clamp
	return Math.round(dBm_biased / SMETER_DISPLAY_RANGE * smeter_width);
}

function smeter_init(force_no_agc_thresh_smeter)
{
   owrx.force_no_agc_thresh_smeter = force_no_agc_thresh_smeter;
	var smeter_control = w3_el('id-control-smeter');
	if (!smeter_control) return;
	
	w3_innerHTML(smeter_control,
		'<canvas id="id-smeter-scale" class="class-smeter-scale" width="0" height="0"></canvas>',
		w3_div('id-smeter-dbm-value'),
		w3_div('id-smeter-dbm-units', 'dBm'),
		w3_div('id-smeter-ovfl w3-hide', 'OV'),
		w3_div('id-smeter-attn w3-hide', 'Attn')
	);

	var sMeter_canvas = w3_el('id-smeter-scale');

   var width = divControl? divControl.activeWidth : 350;
   var canvas_LR_border_pad = html_LR_border_pad(sMeter_canvas);
	smeter_width = width - SMETER_RHS - canvas_LR_border_pad;   // less our own border/padding
	
	var w = smeter_width, h = SMETER_SCALE_HEIGHT, y=h-8;
	var tw = w + SMETER_RHS;
	smeter_control.style.width = px(tw + canvas_LR_border_pad);
	
	sMeter_ctx = sMeter_canvas.getContext("2d");
	sMeter_ctx.canvas.width = tw;
	sMeter_ctx.canvas.height = h;
	sMeter_ctx.fillStyle = "gray";
	sMeter_ctx.globalAlpha = 1;
	sMeter_ctx.fillRect(0,0,tw,h);

	sMeter_ctx.font = "11px Verdana";
	sMeter_ctx.textBaseline = "middle";
	sMeter_ctx.textAlign = "center";
	
	sMeter_ctx.fillStyle = "white";
	for (var i=0; i < bars.text.length; i++) {
		var x = smeter_dBm_biased_to_x(bars.dBm[i] + SMETER_BIAS);
		line_stroke(sMeter_ctx, 1, 3, "white", x,y-8, x,y+8);
		sMeter_ctx.fillText(bars.text[i], x, y-15);
		//console.log('SM x='+ x +' y='+ y +' dBm='+ bars.dBm[i] +' '+ bars.text[i]);
	}

	line_stroke(sMeter_ctx, 0, 5, "black", 0,y, w,y);
	kiwi_clearInterval(owrx.smeter_interval);
	owrx.smeter_interval = setInterval(update_smeter, 100);
}

var sm_px = 0, sm_timeout = 0, sm_interval = 10;
var sm_ovfl_showing = false;
var sm_timer = 0;

function update_smeter()
{
	var x = smeter_dBm_biased_to_x(owrx.sMeter_dBm_biased);
	var y = SMETER_SCALE_HEIGHT-8;
	var w = smeter_width;
	sMeter_ctx.globalAlpha = 1;
	line_stroke(sMeter_ctx, 0, 5, "lime", 0,y, x,y);

   if (cfg.agc_thresh_smeter && owrx.force_no_agc_thresh_smeter != true) {
      var thold = ext_mode(cur_mode).CW? threshCW : thresh;
	   var x_thr = smeter_dBm_biased_to_x(-thold);
	   line_stroke(sMeter_ctx, 0, 2, "white", 0,y-3, w-x_thr,y-3);
	   line_stroke(sMeter_ctx, 0, 2, "black", w-x_thr,y-3, w,y-3);
	}
	
	if (sm_timeout-- == 0) {
		sm_timeout = sm_interval;
		if (x < sm_px) line_stroke(sMeter_ctx, 0, 5, "black", x,y, sm_px,y);
		sm_px = x;
	} else {
		if (x < sm_px) {
			line_stroke(sMeter_ctx, 0, 5, "red", x,y, sm_px,y);
		} else {
			sm_px = x;
			sm_timeout = sm_interval;
		}
	}
	
	sm_timer++;
	var attn_period = ((sm_timer & 0x1f) <= 0x07);
	//audio_ext_adc_ovfl = ((sm_timer % 32) < 16);    // test
	
	if (kiwi.rf_attn && attn_period) {
      w3_show('id-smeter-attn');
	   w3_innerHTML('id-smeter-dbm-value', (kiwi.rf_attn).toFixed(1));
	} else {
      w3_hide('id-smeter-attn');
      if (audio_ext_adc_ovfl && !sm_ovfl_showing) {
         w3_hide('id-smeter-dbm-units');
         w3_show('id-smeter-ovfl');
         w3_call('S_meter_adc_ovfl', true);
         sm_ovfl_showing = true;
      } else
      if (!audio_ext_adc_ovfl && sm_ovfl_showing) {
         w3_hide('id-smeter-ovfl');
         w3_show('id-smeter-dbm-units');
         w3_call('S_meter_adc_ovfl', false);
         sm_ovfl_showing = false;
      }
	   var prec = (owrx.sMeter_dBm <= -100)? 0:1;
	   w3_innerHTML('id-smeter-dbm-value', (owrx.sMeter_dBm).toFixed(prec));
	}
}


////////////////////////////////
// #user ident
////////////////////////////////

var ident_tout;
var ident_user = '';
var send_ident = false;

function ident_init()
{
	var len = Math.max(cfg.ident_len, kiwi.ident_min);
   if (user_url) {
      user_url = kiwi_decodeURIComponent('user_url', user_url);
      user_url = kiwi_strip_tags(user_url, '').substring(0, len);
      kiwi_storeWrite('ident', user_url);
   }
	var ident = kiwi_storeInit('ident', '');
   ident = kiwi_strip_tags(ident, '').substring(0, len);
	//console.log('ident PRE ident_user=<'+ ident +'> ident_len='+ len);
	ident = kiwi_strip_tags(ident, '').substring(0, len);
	//console.log('ident POST ident_user=<'+ ident +'> ident_len='+ len);

	var el1 = w3_el('id-ident-input1');
	var el2 = w3_el('id-ident-input2');
	w3_attribute(el1, 'maxlength', len);
	w3_attribute(el2, 'maxlength', len);
	el1.value = ident;
	el2.value = ident;
	ident_user = ident;
	if (ident_user != '')
	   send_ident = true;
	//console.log('ident_init: SET send_ident=true ident='+ ident_user);
}

function ident_complete(from, which)
{
	var i, el = [];
	for (i = 1; i <= 3; i++) {
	   el[i] = w3_el('id-ident-input'+ i);
	}
	var ident = el[which].value;
	var len = Math.max(cfg.ident_len, kiwi.ident_min);
   ident = kiwi_strip_tags(ident, '').substring(0, len);
   ident = ident.trim();   // no blank idents
	//console.log('ICMPL from='+ from +' which='+ which +' ident='+ ident);
	//canvas_log('IDC-'+ from +'-'+ which);
	//console.log('ICMPL ident_user=<'+ ident +'>');
	//canvas_log('CTO-'+ from);
	kiwi_clearTimeout(ident_tout);

	// okay for ident='' to erase it
	// SECURITY: input value length limited by "maxlength" attribute, but also guard against binary data injection?
	//w3_field_select(el, {mobile:1});
	if (which != 3 || (which == 3 && ident != '')) {    // don't highlight flash field being erased
      for (i = 1; i <= 3; i++) {
         if (el[i]) {
            el[i].value = ident;
            w3_schedule_highlight(el[i]);
         }
      }
   }
	freqset_select();    // don't keep ident field selected

	kiwi_storeWrite('ident', ident);
	ident_user = ident;
	send_ident = true;
	//console.log('ident_complete: SET which='+ which +' ident_user='+ ident_user +' send_ident=true');
	
	if (which == 3 && ident != '') {
	   if (0 && kiwi_isMobile()) {
	      // reload page to get "Try new mobile version" button
	      kiwi_open_or_reload_page();
	   } else {
	      play_button_click_cb();
	   }
	}
}

function ident_keyup(el, evt, which)
{
	//event_dump(evt, "IKU");
	//canvas_log('CTO-ku');
	kiwi_clearTimeout(ident_tout);
	//console.log("IKU el="+ typeof(el) +" val="+ el.value +' send_ident='+ send_ident);
	
	// Ignore modifier keys used with mouse events that also appear here.
	// Also keeps ident_complete timeout from being set after return key.
	//if (ignore_next_keyup_event) {
	if (evt != undefined && evt.key != undefined) {
		var klen = evt.key.length;
		if (any_alternate_click_event_except_shift(evt) || klen != 1 && evt.key != 'Backspace' && evt.key != 'Shift') {
		   //console.log('IDENT key='+ evt.key);
			if (evt.key == 'Enter') {
			   ident_complete('Enter', which);
			}
         //console.log("ignore shift-key ident_keyup");
         //ignore_next_keyup_event = false;
         return;
      }
	}
	
	if (!send_ident || which == 3) {    // ident at connect time always needs t/o
      //canvas_log('STO');
      //if (which != 3)
      ident_tout = setTimeout(function() { ident_complete('t/o', which); } , 5000);
   }
}


////////////////////////////////
// #shortcuts (keyboard)
////////////////////////////////

// letters avail: F G K L O Q tT U X Y

// abcdefghijklmnopqrstuvwxyz `~!@#$%^&*()-_=+[]{}\|;:'"<>? 0123456789.,/kM
// ..........F........ ......  ...F..     F .     .. F  ... FFFFFFFFFFFFFFF
// .. ..  ...  F. . ..  ..  .                               F: frequency entry keys
// ABCDEFGHIJKLMNOPQRSTUVWXYZ
// :space: :tab: :arrow-UDLR:
//    .              .

var shortcut = {
   nav_off: 0,
   keys: null,
   NO_MODIFIER: 0,
   SHIFT: 1,
   CTL_ALT: 2,
   SHIFT_PLUS_CTL_ALT: 3,
   KEYCODE_SFT: 16,
   KEYCODE_CTL: 17,
   KEYCODE_ALT: 18
};

function shortcut_key_mod(evt)
{
   var sft = evt.shiftKey;
   var ctl = evt.ctrlKey;
   var alt = evt.altKey;
   var meta = evt.metaKey;
   var ctlAlt = (ctl||alt);
   var rv =
      (( sft && !ctlAlt)? shortcut.SHIFT :
      ((!sft &&  ctlAlt)? shortcut.CTL_ALT :
      (( sft &&  ctlAlt)? shortcut.SHIFT_PLUS_CTL_ALT :
                          shortcut.NO_MODIFIER)));
   //console.log(evt);
   //console.log('shortcut_key_mod rv='+ rv);
   return rv;
}

function keyboard_shortcut_init()
{
   if ((kiwi_isMobile() && !mobile_laptop_test) || kiwi_isFirefox() < 47 || kiwi_isChrome() <= 49 || kiwi_isOpera() <= 36) return;
   
   shortcut.help =
      w3_div('',
         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua', 'Shortcut keys', 25, 'Function' + w3_link('w3-margin-L-64', 'kiwisdr.com/info', 'KiwiSDR Operating Information')),
         w3_inline_percent('w3-padding-tiny', 'g =', 25, 'select frequency entry field'),
         w3_inline_percent('w3-padding-tiny', 'j i LR-arrow-keys', 25, 'frequency step down/up, add shift or alt for faster,<br>shift plus alt to step to next/prev DX label'),
         w3_inline_percent('w3-padding-tiny', 'm n N', 25, 'toggle frequency memory menu, VFO A/B, VFO A=B'),
         w3_inline_percent('w3-padding-tiny', 'b B', 25, 'scroll band menu'),
         w3_inline_percent('w3-padding-tiny', 'e E', 25, 'scroll extension menu'),
         w3_inline_percent('w3-padding-tiny', 'a A d l u c f q', 25, 'toggle modes: AM SAM DRM LSB USB CW NBFM IQ,<br>add alt to toggle backwards (e.g. SAM modes)'),
         w3_inline_percent('w3-padding-tiny', 'p P alt-p', 25, 'passband narrow/widen, restore default'),
         w3_inline_percent('w3-padding-tiny', 'UD-arrow-keys', 25, 'passband adjust both, right(shift), left(alt) edges'),
         w3_inline_percent('w3-padding-tiny', 'r', 25, 'toggle audio recording'),
         w3_inline_percent('w3-padding-tiny', 'R', 25, 'switch to the RF tab'),
         w3_inline_percent('w3-padding-tiny', 'z Z', 25, 'zoom in/out, add alt for max in/out'),
         w3_inline_percent('w3-padding-tiny', '< >', 25, 'waterfall page down/up'),
         w3_inline_percent('w3-padding-tiny', 'w W', 25, 'waterfall min dB slider -/+ 1 dB, add alt for -/+ 10 dB'),
         w3_inline_percent('w3-padding-tiny', 'S D', 25, 'waterfall auto-scale, spectrum slow device mode'),
         w3_inline_percent('w3-padding-tiny', 's alt-s', 25, 'spectrum RF/AF/off toggle, add alt to toggle backwards'),
         w3_inline_percent('w3-padding-tiny', 'v V space', 25, 'volume less/more, mute'),
         w3_inline_percent('w3-padding-tiny', 'o', 25, 'toggle between option bar <x1>off</x1> <x1>user</x1> and <x1>stats</x1> mode,<br>others selected by related shortcut key'),
         w3_inline_percent('w3-padding-tiny', '!', 25, 'toggle aperture manual/auto menu'),
         w3_inline_percent('w3-padding-tiny', '$', 25, 'toggle 1 Hz frequency readout'),
         w3_inline_percent('w3-padding-tiny', '%', 25, 'toggle tuning lock'),
         w3_inline_percent('w3-padding-tiny', '^', 25, 'toggle mouse wheel tune/zoom'),
         w3_inline_percent('w3-padding-tiny', '&', 25, 'toggle snap to nearest'),
         w3_inline_percent('w3-padding-tiny', '@ alt-@', 25, 'open DX label filter, quick clear'),
         w3_inline_percent('w3-padding-tiny', '\\ |', 25, 'toggle (& open) DX stored/EiBi/community database,<br>alt to toggle <x1>filter by time/day-of-week</x1> checkbox'),
         w3_inline_percent('w3-padding-tiny', 'x y', 25, 'toggle visibility of control panels, top bar'),
         w3_inline_percent('w3-padding-tiny', '~', 25, 'open admin page in new tab'),
         w3_inline_percent('w3-padding-tiny', 'esc', 25, 'close/cancel action'),
         w3_inline_percent('w3-padding-tiny', '? h', 25, 'toggle this help list'),
         w3_inline_percent('w3-padding-tiny', 'H', 25, 'toggle extension or frequency entry field help'),
         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua', '', 25, 'Windows, Linux: use alt key, not control key'),
         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua', '', 25, 'Mac: use alt/option or control key')
      );
   
   shortcut.freq_help =
      w3_div('',
         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua', 'Frequency entry field options'),
         w3_inline_percent('w3-padding-tiny', '<i>freq</i>', 15, 'set rx frequency in kHz or MHz using <x1>M</x1> suffix<br>e.g. <x1>7020</x1> or <x1>10M</x1>'),
         w3_inline_percent('w3-padding-tiny', '<i>pb</i>', 15, 'set passband (described below) without changing frequency'),
         w3_inline_percent('w3-padding-tiny', '<i>freq pb</i>', 15, 'set frequency and passband together, e.g. <x1>7020/2k</x1>'),
         w3_inline_percent('w3-padding-tiny', '#<i>wf-freq</i>', 15, 'set waterfall frequency, e.g. <x1>#7020</x1> or <x1>#10M</x1>'),
         w3_inline_percent('w3-padding-tiny', '#', 15, 'returns waterfall to rx frequency'),
         w3_inline_percent('w3-padding-tiny', 'shift-return', 15, 'undo/redo last frequency change'),

         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua w3-margin-T-8', 'Passband specification'),
         w3_inline_percent('w3-padding-tiny', '', 15,
            'Passband values below are in Hz or kHz if followed by <x1>k</x1> <br>' +
            'Positive values are above the carrier, negative below. <br>' +
            'So a <i>low,high</i> of <x1>300,3k</x1> describes a USB passband <br>' +
            'and <x1>-2.7k,300</x1> a LSB passband.'
         ),
         w3_inline_percent('w3-padding-tiny', '/<i>low,high</i>', 15, 'set passband low and high values'),
         w3_inline_percent('w3-padding-tiny', '/<i>width</i>', 15, 'set width about current passband center'),
         w3_inline_percent('w3-padding-tiny', '/', 15, 'returns passband to default for current mode'),
         w3_inline_percent('w3-padding-tiny', ':<i>pbc</i>', 15, 'sets passband center retaining current width'),
         w3_inline_percent('w3-padding-tiny', ':<i>pbc,pbw</i>', 15, 'sets passband by center and width'),
         
         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua w3-margin-T-8', 'Passband adjustment'),
         w3_inline_percent('w3-padding-tiny', '', 15,
            'Other methods of adjusting the passband are ' +
            '<a href="http://kiwisdr.com/info#id-user-pbt" target="_blank">documented here</a>.'
         )
      );

	w3_el('id-kiwi-body').addEventListener('keydown', keyboard_shortcut_event, true);
}

function keyboard_shortcut_help()
{
   confirmation_show_content(shortcut.help, 550, 745);   // height +15 or 20 per added line
}

function freq_input_help()
{
   confirmation_show_content(shortcut.freq_help, 550, 475);
}

// FIXME: animate (light up) control panel icons?

function keyboard_shortcut_nav(nav)
{
   // bypass the "repeated click of OFF" calls toggle_or_set_hide_bars() behavior
   nav = nav.toLowerCase();
   kiwi.virt_optbar_click = true;
   w3_el('id-nav-optbar-'+ nav).click();
}

function keyboard_shortcut_url_keys()
{
   if (!isNonEmptyArray(shortcut.keys)) { shortcut.keys = null; return; }
   var key = shortcut.keys.shift();
   console.log('URL shortcut key='+ key);
   keyboard_shortcut(key, 0, 0);
   setTimeout(keyboard_shortcut_url_keys, 200);
}

function keyboard_shortcut(key, key_mod, ctlAlt, evt)
{
   var action = true;
   var dir = ctlAlt? -1 : 1;
   var no_step = false;
   
   var el = w3_elementAtPointer(owrx.last_pageX, owrx.last_pageY);
   //console.log(el);
   var inFreqIn = false, inFreqMenu = false;
   if (el) {
      var id = w3_id(el);
      //console.log(id);
      if (id == 'id-freq-input') inFreqIn = true;
      else
      if (id == 'id-freq-menu') inFreqMenu = true;
   }

   switch (key) {
   
   // mode
   case 'a': mode_button(null, w3_el('id-button-am'), dir); break;
   case 'A': mode_button(null, w3_el('id-button-sam'), dir); break;
   case 'd': ext_set_mode('drm', null, { open_ext:true }); break;
   case 'l': mode_button(null, w3_el('id-button-lsb'), dir); break;
   case 'u': mode_button(null, w3_el('id-button-usb'), dir); break;
   case 'c': mode_button(null, w3_el('id-button-cw'), dir); break;
   case 'f': mode_button(null, w3_el('id-button-nbfm'), dir); break;
   case 'q': ext_set_mode('iq'); break;
   
   // step
   // 0: -large, 1: -med, 2: -small || 3: +small, 4: +med, 5: +large
   case 'ArrowLeft':    // if cursor in freq entry box let arrow key move cursor
      if (inFreqIn) return true;    // don't cancel event
      // fall through...

   case 'j': case 'J':
      if (key_mod != shortcut.SHIFT_PLUS_CTL_ALT)
         freqstep_cb(null, owrx.wf_snap? key_mod : (2 - key_mod));
      else
         dx_label_step(-1);
      break;

   case 'ArrowRight':    // if cursor in freq entry box let arrow key move cursor
      if (inFreqIn) return true;    // don't cancel event
      // fall through...

   case 'i': case 'I':
      if (key_mod != shortcut.SHIFT_PLUS_CTL_ALT)
         freqstep_cb(null, owrx.wf_snap? (5 - key_mod) : (3 + key_mod));
      else
         dx_label_step(+1);
      break;
   
   // passband
   case 'p':
   case 'P':
      if (key_mod == shortcut.CTL_ALT) {
         restore_passband(cur_mode);
         demodulator_analog_replace(cur_mode);
      } else {
         passband_increment(key == 'P', owrx.PB_NO_EDGE);
      }
      break;
   
   case 'ArrowUp':
   case 'ArrowDown':
      var edge = owrx.PB_LO_EDGE;
      if (key_mod == shortcut.NO_MODIFIER) edge = owrx.PB_NO_EDGE; else
      if (key_mod == shortcut.SHIFT) edge = owrx.PB_HI_EDGE;
      passband_increment(key == 'ArrowUp', edge);
      break;
   
   // volume/mute
   case 'v': shortcut_setvolume(-5); break;
   case 'V': shortcut_setvolume(+5); break;
   case ' ': toggle_or_set_mute(); break;

   // frequency entry / memory list
   case 'g': case '=': freqset_select(); break;
   case 'm': freq_memory_menu_open(/* shortcut_key */ true); break;
   case 'b': band_scroll(1); break;
   case 'B': band_scroll(-1); break;
   case 'n': freq_vfo_cb(); break;
   case 'N': vfo_update(true); break;     // VFO A=B

   // page scroll
   case '<': page_scroll(-page_scroll_amount); break;
   case '>': page_scroll(+page_scroll_amount); break;

   // zoom
   case 'z': zoom_step(ctlAlt? ext_zoom.NOM_IN : ext_zoom.IN); break;
   case 'Z': zoom_step(ctlAlt? ext_zoom.MAX_OUT : ext_zoom.OUT); break;
   
   // waterfall
   case 'w': incr_mindb(1, ctlAlt? -10 : -1); break;
   case 'W': incr_mindb(1, ctlAlt? +10 : +1); break;
   
   // spectrum
   case 's': toggle_or_set_spec(null, null, dir); keyboard_shortcut_nav('wf'); break;
   case 'S': wf_autoscale_cb(); keyboard_shortcut_nav('wf'); break;
   case 'D': toggle_or_set_slow_dev(); keyboard_shortcut_nav('wf'); break;
   
   // colormap
   case '!': keyboard_shortcut_nav('wf'); wf_aper_cb('wf.aper', wf.aper ^ 1, false); break;

   // misc
   //case '#': if (dbgUs) extint_open('prefs'); break;
   case 'o':
      var nav = ['off', 'user', 'stat'][shortcut.nav_off];
      keyboard_shortcut_nav(nav);
      shortcut.nav_off = (shortcut.nav_off + 1) % 3;
      break;

   case 'r': toggle_or_set_rec(); break;
   case 'R': keyboard_shortcut_nav('rf'); break;
   case '$': owrx.freq_dsp_1Hz ^= 1; kiwi_storeWrite('freq_dsp_1Hz', owrx.freq_dsp_1Hz); freqset_update_ui(owrx.FSET_NOP); break;
   case '%': owrx.tuning_locked ^= 1; freqset_update_ui(owrx.FSET_NOP); break;
   case 'x': toggle_or_set_hide_panels(); break;
   case 'y': toggle_or_set_hide_bars(); break;
   case '@': dx_filter(key_mod == shortcut.SHIFT_PLUS_CTL_ALT); break;
   case 'e': extension_scroll(1); break;
   case 'E': extension_scroll(-1); break;
   case '~': admin_page_cb(); break;
   case '^': canvas_mouse_wheel_set(owrx.wheel_tunes ^ 1); break;
   case '&': wf_snap(owrx.wf_snap ^ 1); break;

   case '?': case 'h':
      console.log('inFreqIn='+ TF(inFreqIn) +' inFreqMenu='+ TF(inFreqMenu));
      if (inFreqIn) freq_input_help();
      else
      if (inFreqMenu) freq_memory_help();
      else
         keyboard_shortcut_help();
      break;

   case 'H':
      if (extint.current_ext_name)
         w3_call(extint.current_ext_name +'_help', true);
      else
         freq_input_help();
      break;

   // dx labels
   case '|':
      if (!ext_panel_displayed('dx')) no_step = true;
      // fall through...
   case '\\':
      if (key_mod == shortcut.CTL_ALT || key_mod == shortcut.SHIFT_PLUS_CTL_ALT) no_step = true;
      if (!no_step) dx.db = (dx.db + 1) % dx.DB_N;
      var opt = {};
      if (key_mod == shortcut.CTL_ALT || key_mod == shortcut.SHIFT_PLUS_CTL_ALT) opt.toggle = 1;
      else
      if (key_mod == shortcut.SHIFT) opt.open = 1;
      dx_database_cb('', dx.db, false, opt, /* from_shortcut */ true);
      break;
   
   // freq memory recall
   case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      if (key_mod == shortcut.CTL_ALT) {
         freq_memory_recall(key - '0' - 1);
      }
      break;
   
   default:
      var kc = evt.keyCode;
      if (evt && kc != shortcut.KEYCODE_SFT && kc != shortcut.KEYCODE_CTL && kc != shortcut.KEYCODE_ALT) {
         console.log('no shortcut key: '+ key_stringify(evt) +'('+ kc +')');
      }
      action = false;
      break;
   
   }
   
   ignore_next_keyup_event = true;     // don't trigger e.g. freqset_keyup()/freqset_complete()
   return false;
}

function keyboard_shortcut_event(evt)
{
   //event_dump(evt, 'shortcut', true);
   
   if (!evt.target) {
      //console.log('KEY no EVT');
      return;
   }
   
   var k = evt.key;
   
   // ignore esc and Fnn function keys
   if (k == 'Escape' || k.match(/F[1-9][12]?/)) {
      //event_dump(evt, 'Escape-shortcut');
      //console.log('KEY PASS Esc');
      return;
   }
   
   shift_event(evt, 'keydown');
   
   var id = evt.target.id;
   var suffix = '';
   if (id == '') {
      //event_dump(evt, 'shortcut');
      w3_iterate_classList(evt.target, function(className, idx) {
         if (className.startsWith('id-')) {
            id = className;
            suffix = ' (class)';
            //console.log('KEY shortcut className='+ id + suffix +' ###');
         }
      });
   }
   
   //{ event_dump(evt, 'shortcut'); return; }
   //event_dump(evt, 'shortcut');

   var sft = evt.shiftKey;
   var ctl = evt.ctrlKey;
   var alt = evt.altKey;
   var meta = evt.metaKey;
   var ctlAlt = (ctl||alt);
   var key_mod = shortcut_key_mod(evt);

   var field_input_key = (
         (k >= '0' && k <= '9' && !ctl) ||
         k == '.' || k == ',' ||                // ',' is alternate decimal point to '.' and used in passband spec
         k == '/' || k == ':' || k == '-' ||    // for passband spec, have to allow for negative passbands (e.g. lsb)
         k == '#' ||                            // for waterfall tuning
         k == 'k' || k == 'M' ||                // scale modifiers
         k == 'Enter' || k == 'Backspace' || k == 'Delete'
      );
   
   var field_or_slider = (evt.target.nodeName == 'INPUT');
   var slider_not_field = (evt.target.type == 'range');
   if (!field_or_slider || slider_not_field || (id == 'id-freq-input' && !field_input_key)) {
      
      // don't interfere with the meta key shortcuts of the browser
      if (kiwi_isMacOS()) {
         if (meta) {
            //console.log('ignore MacOS META '+ (k? k:''));
            return;
         }
      } else {
         if (ctl) {
            //console.log('ignore non-MacOS CTL '+ (k? k:''));
            return;
         }
      }
      
      // evt.key isn't what you'd expect when evt.altKey
      if (alt && !k.startsWith('Arrow')) {
         //event_dump(evt, 'shortcut alt');
         k = String.fromCharCode(evt.keyCode).toLowerCase();
         if (sft) k = k.toUpperCase();
         //console.log('shortcut alt k='+ k);
      }
      
      //console.log('keyboard_shortcut key=<'+ k +'> keyCode='+ evt.keyCode +' key_mod='+ key_mod +' ctlAlt='+ ctlAlt +' alt='+ alt);
      //if (k.startsWith('Arrow')) event_dump(evt, 'Arrow', true);
      if (keyboard_shortcut(k, key_mod, ctlAlt, evt))
         return;     // don't cancel event
      
      /*
      if (k != 'Shift' && k != 'Control' && evt.key != 'Alt') {
         if (!action) event_dump(evt, 'shortcut');
         console.log('KEY SHORTCUT <'+ k +'> '+ (sft? 'SFT ':'') + (ctl? 'CTL ':'') + (alt? 'ALT ':'') + (meta? 'META ':'') +
            ((evt.target.nodeName == 'INPUT')? 'id-freq-input' : evt.target.nodeName) +
            (action? ' ACTION':''));
      }
      */

      return cancelEvent(evt);
   } else {
      //console.log(evt.target);
      //console.log('KEY INPUT FIELD <'+ k +'> '+ id + suffix);
   }
}


////////////////////////////////
// #extensions
////////////////////////////////

function mk_ext_menu()
{
   w3_innerHTML('id-select-ext',
      '<option value="-1" selected disabled>extension</option>' +
      extint_select_build_menu()
   );
}

function extension_scroll(dir)
{
   console.log('extension_scroll dir='+ dir);
   //console.log(extint.ext_names);
   var el = w3_el('id-select-ext');
   //console.log(el);
   var ext_menu = el.childNodes;
   //console.log(ext_menu);
   var menu;

   // ext_menu[i: 1..n+1] for value = "0".."n"
   // value = "-1" when menu has no selection
   var i = (+el.value)+1;
   console.log('extension_scroll initial i='+ i);

   do {
      i += dir;
      if (i < 1) i = ext_menu.length - 1;
      if (i >= ext_menu.length) i = 1;
      menu = ext_menu[i];
   } while (menu.disabled);
   
   var value = +(menu.value);
   var idx = +(menu.getAttribute('kiwi_idx'));
   console.log('extension_scroll i='+ i +' val='+ value +' idx='+ idx +' '+ menu.innerHTML);
   w3_select_value('id-select-ext', value);
   if (owrx.sel_ext_to) kiwi_clearTimeout(owrx.sel_ext_to);
   owrx.sel_ext_to = setTimeout(function() { extint_select(value); }, 2000);
}


////////////////////////////////
// #panels
////////////////////////////////

var panel_margin = 10;
var ac_play_button;

function check_suspended_audio_state()
{
   // Because we don't know exactly when audio_init() will be called,
   // we create a new audio context here and check for the suspended state
   // if no-autoplay mode is in effect.
   // That determines if the "click to start" overlay is displayed or not.
   // If it is, then when it is clicked an attempt is made to resume() the
   // audio context that has most likely been setup by audio_init()
   // in the interim.
   try {
      window.AudioContext = window.AudioContext || window.webkitAudioContext;
      ac_play_button = new AudioContext();
      
      // Safari has to play something before audioContext.state is valid
      if (kiwi_isSafari()) {
         var bufsrc = ac_play_button.createBufferSource();
         bufsrc.connect(ac_play_button.destination);
         try { bufsrc.start(0); } catch(ex) { bufsrc.noteOn(0); }
      }
      
      setTimeout(test_audio_suspended, 500);    // check after a delay
   } catch(e) {
      console.log('#### no AudioContext?');
   }
}

function test_audio_suspended()
{
   //console.log('AudioContext.state='+ ac_play_button.state);
   var s1, onclick = null, input = false, ident = kiwi_storeInit('ident', '');
   //console.log('$test_audio_suspended require_id='+ cfg.require_id +' ident=<'+ ident +'>');
   var m_old = (kiwi.mdev && !kiwi.mnew);

   if (cfg.require_id && isEmptyString(ident)) {
      snd_send('SET require_id=1');
      s1 = w3_input('w3-flex w3-flex-col w3-valign-center//w3-custom-events w3-margin-T-10 w3-font-18px w3-normal'+
         '|padding:1px; width:300px|size=20'+
         ' onchange="ident_complete(\'el\', 3)" onkeyup="ident_keyup(this, event, 3)"',
         'Enter your name or callsign <br> to start KiwiSDR', 'ident-input3');
      onclick = '';
      input = true;
   } else
   if (ac_play_button.state != "running" || m_old) {
      s1 = (kiwi_isMobile()? 'Tap to':'Click to') +' start KiwiSDR';
      if (0 && (kiwi_isMobile() || m_old))
         s1 += '<br><br>' + w3_button('w3-round-xlarge w3-aqua', 'Try new<br>mobile features', 'try_mobile_cb');
         //s1 += '<br><br>' + w3_button('w3-round-xlarge w3-aqua', 'Don\'t use new<br>mobile features', 'try_mobile_cb');
      onclick = 'onclick="play_button_click_cb()"';
   }

   if (onclick != null) {
      var s2 =
         w3_div('id-play-button-container class-overlay-container w3-valign w3-halign-center||'+ onclick,
            w3_inline('id-play-button w3-relative w3-flex-col/',
               w3_img('', 'gfx/openwebrx-play-button.png', 150, 150), '<br><br>', s1
            )
         );
      w3_create_appendElement('id-kiwi-body', 'div', s2);

      if (input) {
         var el = w3_el('id-ident-input3');
         w3_attribute(el, 'maxlength', Math.max(cfg.ident_len, kiwi.ident_min));
         w3_field_select(el, {mobile:1});
      }

      el = w3_el('id-play-button');
      //el.style.top = px(w3_center_in_window(el, 'PB'));
      //alert('state '+ ac_play_button.state);
   }
}

function try_mobile_cb()
{
   kiwi.mdev = kiwi.mnew = true;
   mdev_log_resize();
   play_button_click_cb();
}

// Browsers now only play web audio after they have been started by clicking a button.
function play_button_click_cb()
{
	try {
	   if (kiwi_is_iOS()) {
         var actx = audio_context;
         var bufsrc = actx.createBufferSource();
         bufsrc.connect(actx.destination);
         try { bufsrc.start(0); } catch(ex) { bufsrc.noteOn(0); }
	   } else {
	      try {
	         if (audio_context) audio_context.resume();
	      } catch(ex) {
	         console.log('#### audio_context.resume() FAILED');
	      }
      }
   } catch(ex) { add_problem("audio start"); }

   // CSS is setup so opacity fades
	w3_opacity('id-play-button-container', 0);
	setTimeout(function() { w3_hide('id-play-button-container'); }, 1100);
	audio_reset();    // handle possible audio overflow condition during wait for button click
   freqset_select();
}

// called from wf_init()
function panels_setup()
{
   var el, s, s1, fmt;
   
   el = w3_el("id-ident");
	el.innerHTML =
		w3_input('w3-label-not-bold/w3-custom-events|padding:1px|size=20'+
		   ' onchange="ident_complete(\'el\', 1)" onkeyup="ident_keyup(this, event, 1)"',
		   'Your name or callsign:', 'ident-input1');

	// Need to capture click event to override parent (id-topbar) click event.
	// Otherwise when the ident input field is click freqset_select() is called and the focus is taken away.
	el.addEventListener('click', function(evt) { return cancelEvent(evt); }, true);
	
	var mobile = kiwi_isMobile()?
	   (' inputmode='+ dq(kiwi_is_iOS()? 'decimal' : 'tel') +' ontouchstart="popup_keyboard_touchstart(event)" onclick="this.select()"') : '';
	
	w3_el("id-control-freq1").innerHTML =
	   w3_inline('w3-halign-space-between/',
         w3_div('id-freq-cell',
            // NB: DO NOT remove the following <form> (3/2021)
            // The CATSync app depends on this API by using the following javascript injection:
            // Dim jsFreqKiwiSDR As String = "targetForm = document.forms['form_freq']; targetForm.elements[0].value = '" + frequency + "'; freqset_complete(0); false"
            // Form1.browser.ExecuteScriptAsync(jsFreqKiwiSDR)
            '<form id="id-freq-form" name="form_freq" action="#" onsubmit="freqset_complete(0, event); return false;">' +
               w3_input('w3-custom-events w3-font-16px|margin: 2px 0 0 2px; padding:0 4px; width:'+ freq_field_width() +
               '|type="text" title="type h or ? for help"' + mobile +
               ' onchange="freqset_complete(1, event)" onkeyup="freqset_keyup(this, event)"', '', 'id-freq-input') +
            '</form>'
         ) +

         w3_select('id-select-band w3-pointer', '', '', '', 0, '', 'select_band_cb') +

/*
         '<select id="id-select-ext" class="w3-pointer w3-select-menu" onchange="freqset_select(); extint_select(this.value)">' +
            '<option value="-1" selected disabled>extension</option>' +
            extint_select_build_menu() +
         '</select>'*/
         w3_select('id-select-ext w3-pointer||onchange="freqset_select(); extint_select(this.value)"', '', '', '', 0)
      );
   mk_band_menu();
   mk_ext_menu();
   
   check_suspended_audio_state();
   fmt = 'w3-hold w3-hold-done w3-padding-0 w3-575757';
	
	w3_el("id-control-freq2").innerHTML =
	   w3_inline('w3-halign-space-between w3-margin-T-4/',
         w3_icon('id-freq-menu w3-menu-button w3-hold w3-hold-done||title="freq memory\nclick-hold to save\ntype h or ? for help"', 'fa-bars w3-text-white', 20, '', 'freq_memory_menu_icon_cb'),

         w3_button('id-freq-vfo w3-text-in-circle w3-wh-20px w3-aqua w3-hold w3-hold-done||title="VFO A&slash;B\nclick-hold for A=B"', 'A', 'freq_vfo_cb'),

         kiwi_isMobile()? null : w3_div('id-freq-link|padding-left:0px'),

         w3_div('id-9-10-cell w3-hide',
            w3_button('id-button-9-10 class-button-small||title="LW&slash;MW 9&slash;10 kHz tuning step"', '10', 'button_9_10')
         ),

         w3_div('id-step-freq',
            w3_button(fmt, w3_img('id-step-0',                    'icons/stepdn.20.png'), 'freqstep_cb', 0),
            w3_button(fmt, w3_img('id-step-1|padding-bottom:1px', 'icons/stepdn.18.png'), 'freqstep_cb', 1),
            w3_button(fmt, w3_img('id-step-2|padding-bottom:2px', 'icons/stepdn.16.png'), 'freqstep_cb', 2),
            w3_button(fmt, w3_img('id-step-3|padding-bottom:2px', 'icons/stepup.16.png'), 'freqstep_cb', 3),
            w3_button(fmt, w3_img('id-step-4|padding-bottom:1px', 'icons/stepup.18.png'), 'freqstep_cb', 4),
            w3_button(fmt, w3_img('id-step-5',                    'icons/stepup.20.png'), 'freqstep_cb', 5)
         ),

         w3_div('',
            w3_button('id-button-spectrum class-button|font-size: 12px|title="toggle spectrum display"', 'Spectrum', 'toggle_or_set_spec')
         ),

         w3_div('',
            w3_div('fa-stack||title="record"',
               w3_icon('id-rec1', 'fa-repeat fa-stack-1x w3-text-pink', 22, '', 'toggle_or_set_rec')
            )
         ),

         w3_div('|width:8%|title="mute"',
            // from https://jsfiddle.net/cherrador/jomgLb2h since fa doesn't have speaker-slash
            w3_div('id-mute-no fa-stack|width:100%; color:lime',
               w3_icon('', 'fa-volume-up fa-stack-2x', 24, 'inherit', 'toggle_or_set_mute')
            ),
            w3_div('id-mute-yes fa-stack w3-hide|width:100%;color:magenta;',  // hsl(340, 82%, 60%) w3-text-pink but lighter
               w3_icon('', 'fa-volume-off fa-stack-2x fa-nudge-left', 24, '', 'toggle_or_set_mute'),
               w3_icon('', 'fa-times fa-right', 12, '', 'toggle_or_set_mute')
            ),
            w3_div('id-mute-exclaim fa-stack w3-hide|width:100%; color:yellow',
               w3_icon('', 'fa-exclamation-triangle fa-stack-1x', 20, 'inherit', 'freqset_select')
            )
         )
      );
   
   //w3_hide2('id-mouse-freq', kiwi_isMobile());
	
	if (!isNaN(override_9_10)) {
		step_9_10 = override_9_10;
		//console.log('STEP_9_10 init to override: '+ override_9_10);
	} else {
		var init_AM_BCB_chan = ext_get_cfg_param('init.AM_BCB_chan');
		if (init_AM_BCB_chan == null || init_AM_BCB_chan == undefined) {
			step_9_10 = 0;
			//console.log('STEP_9_10 init to default: 0');
		} else {
			step_9_10 = init_AM_BCB_chan? 0 : 1;
			//console.log('STEP_9_10 init from admin: '+ step_9_10);
		}
	}
	
	button_9_10(step_9_10);

	amode = [];
   mode_buttons.forEach(
	   function(mba, i) {
	      var ms = mba.s[0];
	      var mslc = ms.toLowerCase();
	      var disabled = mba.dis || (mslc == 'drm' && !kiwi.DRM_enable);
	      var attr = disabled? '' : ' onclick="mode_button(event, this)"';
	      attr += ' onmouseover="mode_over(event, this)"';
	      amode[i] = w3_button('id-button-'+ mslc + ' id-mode-'+ ext_mode(mslc).str +
	         ' class-button'+ (disabled? ' class-button-disabled':'') +
	         ' ||id="'+ i +'-id-mode-col"' + attr + ' onmousedown="cancelEvent(event)"', ms);
	   });
	w3_el("id-control-mode").innerHTML = w3_inline_array('w3-halign-space-between/', amode);

   var afd1 = wf.audioFFT_active? ' w3-disabled||onclick="freqset_select()" disabled=""' : '';
   var afd2 = wf.audioFFT_active? ' w3-disabled||disabled="" ' : '||';

	w3_el("id-control-zoom").innerHTML =
	   w3_inline('w3-halign-space-between/',
         w3_div('id-zoom-in class-icon'+  afd2 +'onclick="zoom_click(event, 1)" onmouseover="zoom_over(event)"',
            w3_img('', 'icons/zoomin.png', 32, 32)
         ),
         w3_div('id-zoom-out class-icon'+ afd2 +'onclick="zoom_click(event,-1)" onmouseover="zoom_over(event)"',
            w3_img('', 'icons/zoomout.png', 32, 32)
         ),
         w3_div('id-maxin'+ afd1,
            w3_div('class-icon||onclick="zoom_click(event,8)" title="max in"', w3_img('', 'icons/maxin.png', 32, 32))
         ),
         w3_div('id-maxin-nom w3-hide'+ afd1,
            w3_div('class-icon||onclick="zoom_click(event,8)" title="max in"', w3_img('', 'icons/maxin.nom.png', 32, 32))
         ),
         w3_div('id-maxin-max w3-hide'+ afd1,
            w3_div('class-icon||onclick="zoom_click(event,8)" title="max in"', w3_img('', 'icons/maxin.max.png', 32, 32))
         ),
         w3_div('id-maxout'+ afd1,
            w3_div('class-icon||onclick="zoom_click(event,-9)" title="max out"', w3_img('', 'icons/maxout.png', 32, 32))
         ),
         w3_div('id-maxout-max w3-hide'+ afd1,
            w3_div('class-icon||onclick="zoom_click(event,-9)" title="max out"', w3_img('', 'icons/maxout.max.png', 32, 32))
         ),
         w3_div('class-icon'+ afd1 +'||onclick="zoom_click(event,0)" title="zoom to band"',
            w3_img('|padding-top:13px; padding-bottom:13px', 'icons/zoomband.png', 32, 16)
         ),
         w3_div('class-icon'+ afd2 +'onclick="page_scroll_icon_click(event,-page_scroll_amount)" title="page down\nalt: label step"',
            w3_img('', 'icons/pageleft.png', 32, 32)
         ),
         w3_div('class-icon'+ afd2 +'onclick="page_scroll_icon_click(event,+page_scroll_amount)" title="page up\nalt: label step"',
            w3_img('', 'icons/pageright.png', 32, 32)
         )
		);


   // optbar
   var optbar_colors = [
      'w3-green',
      'w3-pink',
      'w3-blue',
      'w3-purple',
      'w3-aqua',
      'w3-yellow',
      //'w3-grey-white',
      'w3-black',
      
      // future use:
      'w3-red',
      'w3-amber',
      'w3-green',
      'w3-orange',
      'w3-lime',
      'w3-indigo',
      'w3-brown',
      'w3-teal'
   ];
	var ci = 0;
	
	//var psa1 = ' w3-center|width:15.2%';
	var psa1 = ' w3-center|width:12.8%';
	var psa2 = psa1 + ';margin-right:6px';
	w3_el('id-optbar').innerHTML =
      w3_navbar('id-navbar-optbar cl-optbar',
         // will call optbar_focus() optbar_blur() when navbar clicked
         w3_nav   (optbar_colors[ci++] + psa2, 'RF',    'id-navbar-optbar', 'optbar-rf',    'optbar'),
         w3_nav   (optbar_colors[ci++] + psa2, 'WF',    'id-navbar-optbar', 'optbar-wf',    'optbar'),
         w3_nav   (optbar_colors[ci++] + psa2, 'Audio', 'id-navbar-optbar', 'optbar-audio', 'optbar'),
         w3_nav   (optbar_colors[ci++] + psa2, 'AGC',   'id-navbar-optbar', 'optbar-agc',   'optbar'),
         w3_nav   (optbar_colors[ci++] + psa2, 'User',  'id-navbar-optbar', 'optbar-user',  'optbar'),
         w3_nav   (optbar_colors[ci++] + psa2, 'Stat',  'id-navbar-optbar', 'optbar-stat',  'optbar'),
         w3_navdef(optbar_colors[ci++] + psa1, 'Off',   'id-navbar-optbar', 'optbar-off',   'optbar')
      );


	wf_filter = kiwi_storeRead('last_wf_filter', wf_sp_menu_e.OFF);
	spec_filter = kiwi_storeRead('last_spec_filter', wf_sp_menu_e.IIR);
	
   if (isString(spectrum_show)) {
      var ss = spectrum_show.toLowerCase();
      wf_sp_menu_s.forEach(function(e, i) { if (ss == e.toLowerCase()) spec_filter = i; });
   }
   
   wf.max_min_sliders =
      w3_col_percent('w3-valign/class-slider',
         w3_text('w3-text-css-orange', 'WF max'), 19,
         w3_slider('id-input-maxdb w3-wheel-shift', '', '', maxdb, -100, 20, 1, 'setmaxdb_cb'), 60,
         w3_div('id-field-maxdb class-slider'), 19
      ) +
      w3_col_percent('w3-valign/class-slider',
         w3_text('w3-text-css-orange', 'WF min'), 19,
         w3_slider('id-input-mindb w3-wheel-shift', '', '', mindb, -190, -30, 1, 'setmindb_cb'), 60,
         w3_div('id-field-mindb class-slider'), 19
      );

   wf.floor_ceil_sliders =
      w3_col_percent('w3-valign/class-slider',
         w3_text('w3-text-css-orange', 'WF ceil'), 19,
         w3_slider('id-input-ceildb w3-wheel-shift', '', '', wf.auto_ceil.val, -30, 30, 1, 'setceildb_cb'), 60,
         w3_div('id-field-ceildb class-slider'), 19
      ) +
      w3_col_percent('w3-valign/class-slider',
         w3_text('w3-text-css-orange', 'WF floor'), 19,
         w3_slider('id-input-floordb w3-wheel-shift', '', '', wf.auto_floor.val, -30, 30, 1, 'setfloordb_cb'), 60,
         w3_div('id-field-floordb class-slider'), 19
      );


   // rf opt-rf
   fmt = 'id-rf-attn-disable w3-btn w3-padding-tiny w3-margin-R-8 ';
   //kiwi.rf_attn = (kiwi.model == kiwi.KiwiSDR_1)? 0 : +kiwi_storeInit('last_rf_attn', cfg.init.rf_attn);
   var rf_attn = +kiwi_storeInit('last_rf_attn', 0);
   //console.log('INIT rf_attn='+ rf_attn +' kiwi.model='+ kiwi.model);
   kiwi.rf_attn = (kiwi.model == kiwi.KiwiSDR_1)? 0 : rf_attn;
	w3_innerHTML('id-optbar-rf',
	   w3_div('',
         w3_col_percent('w3-valign/class-slider',
            w3_text('w3-text-css-orange', 'RF attn'), 19,
            w3_slider('id-rf-attn id-rf-attn-disable w3-wheel-shift', '', '', kiwi.rf_attn, 0, 31.5, 0.5, 'rf_attn_cb'), 60,
            w3_div('id-field-rf-attn class-slider'), 19
         ) +
         w3_col_percent('',
            '&nbsp', 19,
            w3_inline('',
               w3_button(fmt +'w3-green', '0 dB', 'rf_attn_preset_cb', 0),
               w3_button(fmt +'w3-grey-white', '5 dB', 'rf_attn_preset_cb', 5),
               w3_button(fmt +'w3-grey-white', '10 dB', 'rf_attn_preset_cb', 10),
               w3_button(fmt +'w3-grey-white', '20 dB', 'rf_attn_preset_cb', 20),
               w3_button(fmt +'w3-grey-white', '30 dB', 'rf_attn_preset_cb', 30)
            )
         )
      ) +
      
      w3_hr('cl-cpanel-hr') +
      w3_div('id-optbar-rf-antsw')
   );
   
   var no_attn = (kiwi.model == kiwi.KiwiSDR_1);
   var deny_not_local = false, deny_not_local_or_pwd = false;
   if (cfg.rf_attn_allow == kiwi.RF_ATTN_ALLOW_LOCAL_ONLY && ext_auth() != kiwi.AUTH_LOCAL) deny_not_local = true;
   if (cfg.rf_attn_allow == kiwi.RF_ATTN_ALLOW_LOCAL_OR_PASSWORD_ONLY && ext_auth() == kiwi.AUTH_USER) deny_not_local_or_pwd = true;

   if (no_attn || deny_not_local || deny_not_local_or_pwd) {
      kiwi.rf_attn_disabled = true;
      var el = w3_el('id-rf-attn');
      w3_disable_multi('id-rf-attn-disable', true);
      var title;
      if (no_attn) title = 'no RF attenuator on KiwiSDR 1';
      else
      if (deny_not_local) title = 'only available to local connections';
      else
      if (deny_not_local_or_pwd) title = 'only available to local connections or with password';
      w3_els('id-rf-attn-disable', function(el, i) { el.title = title; } );
   }


   // wf opt-wf
	w3_innerHTML('id-optbar-wf',
      w3_div('id-aper-data w3-hide w3-margin-B-5|left:0px; width:256px; height:16px; background-color:#575757; overflow:hidden; position:relative;',
   		'<canvas id="id-aper-canvas" width="256" height="16" style="position:absolute"></canvas>'
      ) +
      
      w3_col_percent('',
         w3_div('',
            w3_div('id-wf-sliders', wf.max_min_sliders),
            
            w3_col_percent('w3-valign/class-slider',
               w3_text('w3-text-css-orange', 'WF rate'), 19,
               w3_slider('id-slider-rate w3-wheel-shift'+ afd1, '', '', wf_speed, 0, 4, 1, 'setwfspeed_cb'), 60,
               w3_div('slider-rate-field class-slider'), 19
            ),

            w3_col_percent('w3-valign/class-slider',
               w3_button('id-wf-sp-button class-button w3-font-12px', 'Spec &Delta;', 'toggle_or_set_wf_sp_button'), 19,
               w3_slider('id-wf-sp-slider w3-wheel-shift', '', 'wf_sp_param', wf_sp_param, 0, 10, 1, 'wf_sp_slider_cb'), 60,
               w3_div('id-wf-sp-slider-field class-slider'), 19
            )
         ), 84,
         
         w3_divs('/w3-tspace-4 w3-font-11_25px',
            w3_div('w3-hcenter',
               w3_button('id-button-wf-autoscale class-button||title="waterfall auto scale"', 'Auto<br>Scale', 'wf_autoscale_cb')
            ),
            w3_div('w3-hcenter',
               w3_button('id-button-slow-dev class-button||title="slow device mode"', 'Slow<br>Dev', 'toggle_or_set_slow_dev')
            ),
            w3_button('id-button-spec-peak0 w3-margin-L-16 class-button w3-noactive w3-hold|border: 2px solid yellow; padding: 1px 3px|' +
               'title="toggle peak hold\n#1: off-on-hold\nshift: toggle backwards"', 'P1', 'toggle_or_set_spec_peak', 0)
         ), 16
      ) +

      w3_inline('w3-halign-space-between w3-margin-T-2/',
         w3_select('w3-text-red||title="colormap selection"', '', 'color<br>map', 'wf.cmap', wf.cmap, kiwi.cmap_s, 'wf_cmap_cb'),
         w3_select('w3-text-red||title="aperture selection"', '', 'aper', 'wf.aper', wf.aper, kiwi.aper_s, 'wf_aper_cb'),
         w3_select('w3-text-red||title="waterfall filter selection"', '', 'wf', 'wf_filter', wf_filter, wf_sp_menu_s, 'wf_sp_menu_cb', 1),
         w3_select('w3-text-red||title="spectrum filter selection"', '', 'spec', 'spec_filter', spec_filter, wf_sp_menu_s, 'wf_sp_menu_cb', 0),
         w3_icon('id-wf-gnd w3-momentary||title="disable waterfall input"', 'fa-caret-down', 22, 'white', 'wf_gnd_cb', 1),
         w3_button('id-button-spec-peak1 w3-margin-R-10 class-button w3-noactive w3-hold|border: 2px solid magenta; padding: 1px 3px|' +
            'title="toggle peak hold\n#2: off-on-hold\nshift: toggle backwards"', 'P2', 'toggle_or_set_spec_peak', 1)
      ) +
      
      w3_hr('cl-cpanel-hr') +
      w3_div('id-wf-more')
   );

   setwfspeed_cb('', wf_speed, 1);
   toggle_or_set_slow_dev(toggle_e.FROM_COOKIE | toggle_e.SET, 0);
   toggle_or_set_spec_peak(toggle_e.FROM_COOKIE | toggle_e.SET_URL, peak_initially1, 0);
   toggle_or_set_spec_peak(toggle_e.FROM_COOKIE | toggle_e.SET_URL, peak_initially2, 1);
   toggle_or_set_wf_sp_button(toggle_e.FROM_COOKIE | toggle_e.SET, 0);


   // audio opt-audio
	de_emphasis = kiwi_storeRead('last_de_emphasis', 0);
	de_emphasis = w3_clamp(de_emphasis, 0, 2, 0);
	de_emphasis_nfm = kiwi_storeRead('last_de_emphasis_nfm', 0);
	de_emphasis_nfm = w3_clamp(de_emphasis_nfm, 0, 2, 0);
	kiwi.pan = kiwi_storeRead('last_pan', 0);

   s =
		w3_inline_percent('w3-valign/w3-last-halign-end',
			w3_text('w3-text-css-orange cl-closer-spaced-label-text', 'Noise'), 17,
         w3_select('w3-text-red||title="noise blanker selection"', '', 'blanker', 'nb_algo', 0, noise_blank.menu_s, 'nb_algo_cb', 'm'), 24,
		   w3_div('w3-hcenter', w3_button('class-button w3-text-orange||title="noise blanker parameters"', 'More', 'noise_blank_view')), 19,
         w3_div('w3-hcenter ', w3_select('w3-text-red||title="noise filter selection"', '', 'filter', 'nr_algo', 0, noise_filter.menu_s, 'nr_algo_cb', 'm')), 27,
		   w3_button('class-button w3-text-orange||title="noise filter parameters"', 'More', 'noise_filter_view')
		) +
		w3_inline_percent('id-vol w3-valign w3-margin-T-2 w3-hide/class-slider w3-last-halign-end',
			w3_text('w3-text-css-orange', 'Volume'), 17,
         w3_slider('id-input-volume w3-wheel-shift', '', '', kiwi.volume, 0, 200, 1, 'setvolume_cb'), 50,
         '&nbsp;', 8,
         w3_div('',
            w3_select('id-deemp id-deemp1-ofm w3-text-red||title="de-emphasis"', '', 'de-<br>emp', 'de_emphasis', de_emphasis, de_emphasis_s, 'de_emp_cb', 0),
            w3_select('id-deemp id-deemp1-nfm w3-text-red w3-hide||title="de-emphasis"', '', 'de-<br>emp', 'de_emphasis_nfm', de_emphasis_nfm, de_emphasis_nfm_s, 'de_emp_cb', 1)
         )
		) +
		w3_inline_percent('id-vol-comp w3-valign w3-margin-T-2/class-slider w3-last-halign-end',
			w3_text('w3-text-css-orange', 'Volume'), 17,
         w3_slider('id-input-volume w3-wheel-shift', '', '', kiwi.volume, 0, 200, 1, 'setvolume_cb'), 50,
         w3_div('',
            w3_select('id-deemp id-deemp2-ofm w3-text-red||title="de-emphasis"', '', 'de-<br>emp', 'de_emphasis', de_emphasis, de_emphasis_s, 'de_emp_cb', 0),
            w3_select('id-deemp id-deemp2-nfm w3-text-red w3-hide', '', 'de-<br>emp', 'de_emphasis_nfm', de_emphasis_nfm, de_emphasis_nfm_s, 'de_emp_cb', 1)
         ), 28,
		   w3_button('id-button-compression class-button w3-hcenter||title="compression"', 'Comp', 'toggle_or_set_compression')
		) +
      w3_inline_percent('id-pan w3-valign w3-hide/class-slider w3-last-halign-end',
         w3_text('w3-text-css-orange', 'Pan'), 17,
         w3_slider('id-pan-value w3-wheel-shift', '', '', kiwi.pan, -1, 1, 0.01, 'setpan_cb'), 50,
         '&nbsp;', 3, w3_div('id-pan-field'), 8, '&nbsp;', 7,
		   w3_button('id-button-compression class-button w3-hcenter||title="compression"', 'Comp', 'toggle_or_set_compression')
      ) +
      w3_inline_percent('id-squelch w3-valign/class-slider w3-last-halign-end',
			w3_text('id-squelch-label', 'Squelch'), 15,
         //w3_icon('w3-momentary||title="momentary zero squelch"', 'fa-caret-down', 22, 'white', 'squelch_zero_cb', 1), 3,
         w3_icon('||title="toggle squelch"', 'fa-caret-down', 22, 'lime', 'squelch_zero_cb', 1), 3,
         w3_slider('id-squelch-value w3-wheel-shift', '', '', squelch, 0, 99, 1, 'set_squelch_cb'), 42,
         w3_div('id-squelch-field w3-center class-slider'), 14,
         w3_select('id-pre-rec w3-margin-R-4 w3-hide w3-text-red||title="pre-record time"', '', 'pre', 'pre_record', pre_record, pre_record_s, 'pre_record_cb'), 16,
         w3_select('id-squelch-tail w3-hide w3-text-red||title="squelch tail length"', '', 'tail', 'squelch_tail', squelch_tail, squelch_tail_s, 'squelch_tail_cb')
	   ) +
      w3_inline_percent('id-sam-carrier-container w3-hide w3-valign/',
         w3_inline('w3-halign-space-between/w3-last-halign-end',
            w3_text('w3-text-css-orange', 'SAM'),
            w3_text('id-sam-carrier')
         ), 50,
         w3_div(), 5,
         w3_inline('w3-halign-space-between/w3-last-halign-end',
            w3_select('w3-text-red||title="PLL speed"', '', 'PLL', 'owrx.sam_pll', owrx.sam_pll, owrx.sam_pll_s, 'sam_pll_cb'),
            w3_button('class-button w3-hcenter||title="PLL reset"', 'Reset', 'sam_pll_reset_cb')
         )
      ) +
      w3_inline('w3-margin-T-2 w3-valign w3-halign-space-between/class-slider',
         w3_button('w3-padding-tiny w3-yellow||title="restore passband"', 'PB default', 'pb_default_cb'),
         w3_inline('',
            w3_select('id-chan-null w3-text-red w3-margin-R-6 w3-hide||title="channel null"', '', 'channel<br>null', 'owrx.chan_null', owrx.chan_null, owrx.chan_null_s, 'chan_null_cb'),
            //w3_select('id-SAM-opts w3-text-red w3-hide||title="SAM options"', '', 'SAM<br>options', 'owrx.SAM_opts', owrx.SAM_opts, owrx.SAM_opts_s, 'SAM_opts_cb'),
            w3_select('id-ovld-mute w3-text-red||title="overload mute"', '', 'ovld<br>mute', 'owrx.ovld_mute', owrx.ovld_mute, owrx.ovld_mute_s, 'ovld_mute_cb')
         )
      );
      
      var slpct = 65;
      var step = 10;
      s1 =
         w3_div('',
            w3_inline_percent('w3-valign w3-margin-T-2/class-slider',
               w3_text('w3-text-css-orange', 'PB low'), 20,
               w3_slider('id-pb-lo w3-wheel-shift', '', '', 0, 0, 0, step, 'setpb_cb', owrx.PB_LO), slpct,
               w3_div('id-pb-lo-val w3-margin-L-10')
            ),
            w3_inline_percent('w3-valign w3-margin-T-2/class-slider',
               w3_text('w3-text-css-orange', 'PB high'), 20,
               w3_slider('id-pb-hi w3-wheel-shift', '', '', 0, 0, 0, step, 'setpb_cb', owrx.PB_HI), slpct,
               w3_div('id-pb-hi-val w3-margin-L-10')
            ),
            w3_inline_percent('w3-valign w3-margin-T-2/class-slider',
               w3_text('w3-text-css-orange', 'PB center'), 20,
               w3_slider('id-pb-center w3-wheel-shift', '', '', 0, 0, 0, step, 'setpb_cb', owrx.PB_CENTER), slpct,
               w3_div('id-pb-center-val w3-margin-L-10')
            ),
            w3_inline_percent('w3-valign w3-margin-T-2/class-slider',
               w3_text('w3-text-css-orange', 'PB width'), 20,
               w3_slider('id-pb-width w3-wheel-shift', '', '', 0, 0, 0, step, 'setpb_cb', owrx.PB_WIDTH), slpct,
               w3_div('id-pb-width-val w3-margin-L-10')
            )
         );

	w3_innerHTML('id-optbar-audio',
	   w3_div('id-audio-content w3-margin-R-6', s + s1) +
      w3_hr('cl-cpanel-hr') +
      w3_div('id-nblank-more') +
      w3_hr('cl-cpanel-hr') +
      w3_div('id-nfilter-more') +
      w3_hr('cl-cpanel-hr') +
      w3_div('id-ntest-more')
   );
      
   noise_blank_init();
   noise_filter_init();

   //toggle_or_set_test(0);
   //w3_show('id-button-test', dbgUs);
   //toggle_or_set_audio(toggle_e.FROM_COOKIE | toggle_e.SET, 1);
   //console.log('cfg.init.comp='+ cfg.init.comp);
   if (cfg.init.comp == owrx.COMP_LAST)
      toggle_or_set_compression(toggle_e.FROM_COOKIE | toggle_e.SET, 1);
   else
      toggle_or_set_compression(toggle_e.SET, (cfg.init.comp == owrx.COMP_ON)? 1:0);
   
	squelch_setup(toggle_e.FROM_COOKIE);
   audio_panner_ui_init();


   // agc opt-agc
	w3_innerHTML('id-optbar-agc',
		w3_inline('w3-valign/',
			w3_button('id-button-agc class-button||onmouseover="agc_over(event)"', 'AGC', 'toggle_agc'),
			w3_button('id-button-hang class-button w3-margin-L-10', 'Hang', 'toggle_or_set_hang'),
			w3_button('w3-padding-tiny w3-yellow w3-margin-L-10', 'Defaults', 'agc_load_defaults'),
         w3_button('w3-green w3-small w3-padding-small w3-margin-R-4 w3-btn-right', 'help', 'agc_help')
		) +
		w3_div('',
			w3_col_percent('w3-valign/class-slider',
            w3_divs('w3-show-inline-block/id-label-man-gain cl-closer-spaced-label-text', 'Manual gain'), 24,
            w3_slider('id-input-man-gain w3-wheel', '', '', manGain, 0, 120, 1, 'setManGain_cb'), 52,
            w3_div('id-field-man-gain w3-show-inline-block', manGain.toString()) +' dB'
			),
			w3_col_percent('w3-valign/class-slider',
				w3_div('label-threshold w3-show-inline-block', 'Threshold'), 24,
            w3_slider('id-input-threshold w3-wheel', '', '', thresh, -130, 0, 1, 'setThresh_cb'), 52,
				w3_div('id-field-threshold w3-show-inline-block', thresh.toString()) +' dBm'
			),
			w3_col_percent('w3-valign/class-slider',
				w3_div('label-threshCW w3-show-inline-block', 'Thresh CW'), 24,
            w3_slider('id-input-threshCW w3-wheel', '', '', threshCW, -130, 0, 1, 'setThreshCW_cb'), 52,
				w3_div('id-field-threshCW w3-show-inline-block', threshCW.toString()) +' dBm'
			),
			w3_col_percent('w3-valign/class-slider',
				w3_div('label-slope w3-show-inline-block', 'Slope'), 24,
            w3_slider('id-input-slope w3-wheel', '', '', slope, 0, 10, 1, 'setSlope_cb'), 52,
				w3_div('id-field-slope w3-show-inline-block', slope.toString()) +' dB'
			),
			w3_col_percent('w3-valign/class-slider',
				w3_div('label-decay w3-show-inline-block', 'Decay'), 24,
            w3_slider('id-input-decay w3-wheel', '', 'id-agc-decay', decay, 20, 5000, 10, 'setDecay_cb'), 52,
				w3_div('id-field-decay w3-show-inline-block', decay.toString()) +' msec'
			)
		)
	);
	setup_agc(toggle_e.FROM_COOKIE | toggle_e.SET);

	
	// user opt-user
	
	
	// stat opt-stat
	
	
	// optbar_setup
	//console.log('optbar_setup');
   w3_click_nav('id-navbar-optbar', kiwi_toggle(toggle_e.FROM_COOKIE | toggle_e.SET, 'optbar-wf', 'optbar-wf', 'last_optbar'), 'optbar', 'init');
   ant_switch_user_init();


	// id-news
	w3_el('id-news').style.backgroundColor = news_color;
	/*
	w3_el("id-news-inner").innerHTML =
		'<span style="font-size: 14pt; font-weight: bold;">' +
			'KiwiSDR now available on ' +
			'<a href="https://www.kickstarter.com/projects/1575992013/kiwisdr-beaglebone-software-defined-radio-sdr-with" target="_blank">' +
				w3_img('class-KS', 'icons/kickstarter-logo-light.150x18.png') +
			'</a>' +
		'</span>' +
		'';
	*/


	// id-readme
	
	el = w3_el('id-readme');
	el.style.backgroundColor = readme_color;

   // Allow a click anywhere in panel to toggle it.
	// Use a capturing click listener, then cancel propagation of the click
	// so vis handler doesn't call toggle_panel() twice.
	/*
	el.addEventListener("click", function(ev) {
	   //console.log('id-readme ev='+ ev);
	   cancelEvent(ev);
	   toggle_panel('id-readme');
	}, true);
	*/
	
	//console.log('README='+ cfg.panel_readme);
	var readme = cfg.panel_readme || '';
	if (readme != '') readme = w3_json_to_html('readme', readme);
	w3_el("id-readme-inner").innerHTML = isNonEmptyString(readme)? readme :
		('<span style="font-size: 15pt; font-weight: bold;">Welcome!</span>' +
		'&nbsp;&nbsp;&nbsp;Project website: <a href="http://kiwisdr.com" target="_blank">kiwisdr.com</a>&nbsp;&nbsp;&nbsp;&nbsp;Here are some tips:' +
		'<ul style="padding-left: 12px;">' +
		'<li> Show and hide the panels by using the circled arrows at the top right corner. </li>' +
		'<li> Most major browsers should work fine (except for Internet Explorer). </li>' +
		'<li> There is no support for small-screen mobile devices currently. </li>' +
		'<li> You can click and/or drag almost anywhere on the page to change settings. </li>' +
		'<li> Enter a numeric frequency (in kHz) in the box at right (top left corner). </li>' +
		'<li> Or use the "select band" menu to jump to a pre-defined band. </li>' +
		'<li> Use the zoom icons to control the waterfall span. </li>' +
		'<li> Tune by clicking on the waterfall, spectrum or the multi-colored station labels. </li>' +
		'<li> Control-shift or alt-shift click in the waterfall to lookup frequency in online databases. </li>' +
		'<li> Control or alt click to page spectrum down and up in frequency. </li>' +
		'<li> Adjust the "WF floor/min" slider for best waterfall colors. </li>' +
		"<li> Type 'h' or '?' to see the list of keyboard shortcuts. </li>" +
		'<li> See the <a href="http://www.kiwisdr.com/info" target="_blank">Operating information</a> page ' +
		      'and <a href="https://forum.kiwisdr.com" target="_blank">KiwiSDR Forum</a>. </li>' +
		'</ul>');


	// id-msgs
	
	var contact_admin = ext_get_cfg_param('contact_admin');
	if (isUndefined(contact_admin)) {
		// hasn't existed before: default to true
		//console.log('contact_admin: INIT true');
		contact_admin = true;
	}

	var admin_email = ext_get_cfg_param('admin_email');
	console.log('contact_admin='+ contact_admin +' admin_email='+ admin_email);
	admin_email = (isArg(contact_admin) && contact_admin == true && isNonEmptyString(admin_email))? admin_email : null;
	var email = '';
	if (admin_email != null) {
      if (kiwi_isFirefox()) {
         // see: stackoverflow.com/questions/42340698/change-window-location-href-in-firefox-without-closing-websockets
         email = '<a href='+ dq('mailto:'+ admin_email) +' target="iframe_mailto">Owner/Admin</a> | ';
      } else {
         // NB: have to double encode here because the "javascript:sendmail()" href below automatically undoes
         // one level of encoding causing problems when an email containing an underscore gets enc() to a backslash
	      admin_email = encodeURIComponent(encodeURIComponent(enc(admin_email)));
         email = '<a href="javascript:sendmail(\''+ admin_email +'\');">Owner/Admin</a> | ';
      }
   }

   w3_innerHTML('id-optbar-stat',
		w3_div('id-status-msg') +
		w3_div('',
	      w3_text('w3-text-css-orange', 'Links'),
	      '<iframe src="about:blank" name="iframe_mailto" class="ws_keep_conn"></iframe>',
	      w3_text('', email +
            '<a href="http://kiwisdr.com" target="_blank">KiwiSDR</a> ' +
            '| <a href="http://forum.kiwisdr.com/discussions" target="_blank">Forum</a> ' +
            '| <a href="https://kiwiirc.com/client/chat.freenode.net/#kiwisdr" target="_blank">Chat</a> '
         )
		) +
		w3_div('id-status-adc') +
		w3_div('id-status-config') +
		w3_div('id-status-gps') +
		w3_inline('w3-valign',
		   w3_div('id-status-audio'), ' ',
		   w3_div('id-status-problems')
		) +
		w3_div('id-status-stats-cpu') +
		w3_div('id-status-stats-xfer')
	);

	setTimeout(function() { setInterval(status_periodic, 5000); }, 1000);
}


////////////////////////////////
// #option bar
////////////////////////////////

function optbar_focus(next_id, cb_arg)
{
   // maintain independent scroll positions of optbar panels
   var scrollpos = owrx.optbar_last_scrollpos[next_id];
   //console.log('optbar_focus='+ next_id +' scrollPos='+ scrollpos);
   if (!isArg(scrollpos)) scrollpos = 0;
   w3_scrollTo('id-optbar-content', scrollpos);
   kiwi_storeWrite('last_optbar', next_id);
   
   var h;
   if (next_id == 'optbar-off') {
      // repeated clicks of "Off" toggles top/label bars
      if (cb_arg != 'init' && kiwi.cur_optbar == 'optbar-off' && !kiwi.virt_optbar_click) toggle_or_set_hide_bars();
      w3_hide('id-optbar-content');
      w3_el('id-control-top').style.paddingBottom = '8px';
   } else {
      w3_show_block('id-optbar-content');
      w3_el('id-control-top').style.paddingBottom = '';     // let id-optbar-content determine all spacing
   }
   kiwi.virt_optbar_click = false;
   w3_el('id-control').style.height = '';    // undo spec in index.html
   kiwi.cur_optbar = next_id;
   freqset_select();
}

function optbar_blur(cur_id)
{
   // maintain independent scroll positions of optbar panels
   var scrollpos = w3_scrolledPosition('id-optbar-content');
   owrx.optbar_last_scrollpos[cur_id] = scrollpos;
   //console.log('optbar_blur='+ cur_id +' scrollPos='+ scrollpos);

   if (cur_id == 'optbar-rf')
      ant_switch_blur();
}


////////////////////////////////
// #rf controls
////////////////////////////////

function rf_attn_cb(path, val, done, first, ui_only)
{
   //console.log('rf_attn_cb val='+ val +' done='+ done +' first='+ first +' ui_only='+ ui_only +' kiwi.rf_attn='+ kiwi.rf_attn);
   if (kiwi.rf_attn_disabled && !ui_only) return;
   
	var attn = parseFloat(val);
   var input_attn = w3_el('id-rf-attn');
   var field_attn = w3_el('id-field-rf-attn');
   if (!input_attn || !field_attn) return;
   kiwi.rf_attn = attn;
   input_attn.value = attn;
   field_attn.innerHTML = attn.toFixed(1) + ' dB';
   field_attn.style.color = "white";
   if (ui_only == true) return;

   //console.log('SET rf_attn='+ attn.toFixed(1));
   snd_send('SET rf_attn='+ attn.toFixed(1));

   if (done) {
      kiwi_storeWrite('last_rf_attn', val);
      freqset_select();
   }
}

function rf_attn_wheel_cb()
{
   if (kiwi.rf_attn_disabled) return;
   var nval = w3_slider_wheel('rf_attn_wheel_cb', 'id-rf-attn', kiwi.rf_attn, 0.5, 1);
   if (isNumber(nval)) rf_attn_cb(null, nval);
}

function rf_attn_preset_cb(path, val)
{
   //console.log('rf_attn_preset_cb: path='+ path +' val='+ val);
   rf_attn_cb(null, val, true);
}


////////////////////////////////
// #waterfall / spectrum controls
////////////////////////////////

var WF_SPEED_OFF = 0;
var WF_SPEED_1HZ = 1;
var WF_SPEED_SLOW = 2;
var WF_SPEED_MED = 3;
var WF_SPEED_FAST = 4;

var wf_speed = WF_SPEED_FAST;
var wf_speeds = ['off', '1 Hz', 'slow', 'med', 'fast'];

var wf = {
   no_wf: false,
   cal: 0,
   url_tstamp: 0,
   ts_tz: 0,
   
   COMPRESSED: 1,
   NO_SYNC: 2,
   
   scroll_multiple: 3,
   no_sync: false,
   
   cmap: 0,
   cmap_override: -1,
   custom_colormaps: [ new Uint8Array(3*256), new Uint8Array(3*256), new Uint8Array(3*256), new Uint8Array(3*256) ],

   aper: 0,
   aper_algo: 3,     // OFF
   aper_param: 0,

   sqrt: 0,
   contr: 0,
   last_zoom: -1,
   need_autoscale: 0,
   FNAS: 0,
   
   auto_ceil: { min:0, val:5, max:30 },
   auto_floor: { min:-30, val:0, max:0 },
   auto_maxdb: 0,
   auto_mindb: 0,
   
   audioFFT_active: false,
   audioFFT_prev_mode: '',
   audioFFT_clear_wf: false,
};

var maxdb;
var mindb_un;
var mindb;
var full_scale;

function init_scale_dB()
{
	maxdb = init_max_dB;
	mindb_un = init_min_dB;
	mindb = mindb_un - zoomCorrection();
	full_scale = maxdb - mindb;
	full_scale = full_scale? full_scale : 1;	// can't be zero

   wf.save_maxdb = maxdb;
   wf.save_mindb_un = mindb_un;
   wf.auto_ceil.val = +kiwi_storeInit('last_ceil_dB', cfg.init.ceil_dB);
   wf.auto_floor.val = +kiwi_storeInit('last_floor_dB', cfg.init.floor_dB);
}

var wf_contr_s = { 0:'normal', 1:'scheme 1', 2:'scheme 2', 3:'scheme 3', 4:'scheme 4' };

function wf_contr_cb(path, idx, first)
{
   //console.log('wf_contr_cb idx='+ idx +' first='+ first);
}

var wf_gnd = 0, wf_gnd_value_base = 150, wf_gnd_value = wf_gnd_value_base;

function wf_gnd_cb(path, idx, first)
{
   //console.log('wf_gnd_cb idx='+ idx +' first='+ first);
   idx = +idx;
   w3_color(path, idx? 'white':'lime');
   wf_gnd = !idx;
   if (idx == 0) freqset_select();
}


var wf_sp_param = 0;
var wf_param = 0, sp_param = 0;
var wf_sp_which = 0;   // 0 = spectrum, 1 = waterfall
var wf_filter = 0, spec_filter = 0;
var wf_sp_menu_s = [ 'IIR', 'MMA', 'EMA', 'off' ];
var wf_sp_menu_e = { IIR:0, MMA:1, EMA:2, OFF:3 };
var wf_sp_slider_s = [ 'gain', 'avgs', 'avgs', '' ];
var wf_sp_slider_d = [ 1, 0, 0, 0 ];

var wf_sp_filter_p = [
   {  // spectrum
      IIR_min:0, IIR_max:1, IIR_step:0.1, IIR_def:0.2, IIR_val:undefined,
      MMA_min:1, MMA_max:32, MMA_step:1, MMA_def:8, MMA_val:undefined,
      EMA_min:1, EMA_max:32, EMA_step:1, EMA_def:8, EMA_val:undefined,
      off_min:0, off_max:0, off_step:0, off_def:0, off_val:undefined
   },
   {  // waterfall
      IIR_min:0, IIR_max:2, IIR_step:0.1, IIR_def:0.8, IIR_val:undefined,
      MMA_min:1, MMA_max:16, MMA_step:1, MMA_def:2, MMA_val:undefined,
      EMA_min:1, EMA_max:16, EMA_step:1, EMA_def:2, EMA_val:undefined,
      off_min:0, off_max:0, off_step:0, off_def:0, off_val:undefined
   }
];

function spec_show()
{
   toggle_or_set_spec(toggle_e.SET, spec.RF);
   
   var sp = -1;
   spectrum_param = parseFloat(spectrum_param);
   if (!isNaN(spectrum_param) && spectrum_param != -1) sp = spectrum_param;
   
   wf_sp_which = 0;
   var f = wf_sp_menu_s[spec_filter];
   var f_p = wf_sp_filter_p[0];
   
   if (sp >= f_p[f+'_min'] && sp <= f_p[f+'_max']) {
      //console.log('spec_show sp='+ sp);
      wf_sp_slider_cb('id-wf-sp-slider', sp, /* done */ true, /* first */ false);
      toggle_or_set_wf_sp_button(toggle_e.SET, 0);
   }
}

function wf_sp_menu_cb(path, idx, first, param)    // param indicates which menu, wf or sp
{
   idx = +idx;
   //console.log('wf_sp_menu_cb ENTER path='+ path +' idx='+ idx +' first='+ first +' which='+ param);
   //kiwi_trace('wf_sp_menu_cb');
   wf_sp_which = +param;
   if (wf_sp_which) wf_filter = idx; else spec_filter = idx;

   var f_val = wf_sp_which? wf_filter : spec_filter;
   var f = wf_sp_menu_s[f_val];
   var f_p = wf_sp_filter_p[wf_sp_which];
   var val = f_p[f+'_val'];
   //console.log('wf_sp_menu_cb which='+ wf_sp_which +' f_val='+ f_val +' f_p='+ f_p +' val='+ val);
   
   // update button and slider to match which menu was changed
   wf_sp_slider_cb('id-wf-sp-slider', val, /* done */ true, /* first */ false);
   toggle_or_set_wf_sp_button(toggle_e.SET, wf_sp_which);
	kiwi_storeWrite(wf_sp_which? 'last_wf_filter':'last_spec_filter', f_val.toString());

	if (wf_sp_which) need_clear_wfavg = true; else spec.need_clear_avg = true;
   freqset_select();
   //console.log('wf_sp_menu_cb EXIT path='+ path +' wf_filter='+ wf_filter +' wf_param='+ wf_param +' spec_filter='+ spec_filter +' sp_param='+ sp_param);
}

function spectrum_filter(filter)
{
   wf_sp_menu_cb('', filter, false, 0);
}

function toggle_or_set_wf_sp_button(set, val)
{
   //console.log('toggle_or_set_wf_sp_button ENTER set='+ set +' val='+ val);
   //kiwi_trace('toggle_or_set_wf_sp_button');
	if (isNumber(set))
		wf_sp_which = +kiwi_toggle(set, +val, +val, 'last_wf_sp_filter');
	else
		wf_sp_which ^= 1;

   //console.log('toggle_or_set_wf_sp_button new which='+ wf_sp_which);
	w3_innerHTML('id-wf-sp-button', wf_sp_which? 'WF &Delta;':'Spec &Delta;');

   // update slider 
   var f_val = wf_sp_which? wf_filter : spec_filter;
   var f = wf_sp_menu_s[f_val];
   var f_p = wf_sp_filter_p[wf_sp_which];
   wf_sp_slider_cb('id-wf-sp-slider', f_p[f+'_val'], /* done */ true, /* first */ false);
	freqset_select();
   //console.log('toggle_or_set_wf_sp_button EXIT');
}

function wf_sp_slider_cb(path, val, done, first)
{
   if (first) return;
   val = +val;
   //console.log('wf_sp_slider_cb ENTER path='+ path +' val='+ val +' which='+ wf_sp_which);
   //kiwi_trace('wf_sp_slider_cb');
	var f_val = wf_sp_which? wf_filter : spec_filter;
   var f = wf_sp_menu_s[f_val];
   var f_p = wf_sp_filter_p[wf_sp_which];

   if (val == undefined || isNaN(val)) {
      val = f_p[f+'_def'];
      //console.log('wf_sp_slider_cb using default='+ val +'('+ typeof(val) +') which='+ wf_sp_which);
      var lsf = parseFloat(kiwi_storeRead(wf_sp_which? 'last_wf_filter':'last_spec_filter'));
      var lsfp = parseFloat(kiwi_storeRead(wf_sp_which? 'last_wf_filter_param':'last_spec_filter_param'));
      if (lsf == f_val && !isNaN(lsfp)) {
         //console.log('wf_sp_slider_cb USING READ_COOKIE last_filter_param='+ lsfp);
         val = lsfp;
      }
   }

   if (wf_sp_which) wf_param = val; else sp_param = val;
	//console.log('wf_sp_slider_cb UPDATE slider='+ val +' which='+ wf_sp_which +' menu='+ f_val +' done='+ done +' first='+ first);
	f_p[f+'_val'] = val;

   // for benefit of direct callers
   w3_slider_setup('id-wf-sp-slider', f_p[f+'_min'], f_p[f+'_max'], f_p[f+'_step'], val);
   w3_el('id-wf-sp-slider-field').innerHTML = (f_val == wf_sp_menu_e.OFF)? '' : (val.toFixed(wf_sp_slider_d[f_val]) +' '+ wf_sp_slider_s[f_val]);

   if (done) {
	   //console.log('wf_sp_slider_cb DONE WRITE_COOKIE last_filter_param='+ val.toFixed(2) +' which='+ wf_sp_which);
	   kiwi_storeWrite(wf_sp_which? 'last_wf_filter_param':'last_spec_filter_param', val.toFixed(2));
      freqset_select();
   }

   //console.log('wf_sp_slider_cb EXIT path='+ path);
}

function wf_sp_slider_wheel_cb()
{
   var el = w3_el('id-wf-sp-slider');
   var param = wf_sp_which? wf_param : sp_param;
   var nval = w3_slider_wheel('wf_sp_slider_wheel_cb', el, param, +el.step, +el.step * 5);
   //console.log('wf_sp_slider_wheel_cb min='+ el.min +' max='+ el.max +' step='+ el.step +' param='+ param +' up_down='+ up_down +' nval='+ nval);
   if (isNumber(nval)) wf_sp_slider_cb('', nval, 1);
}

function wf_cmap_cb(path, idx, first)
{
   if (first) return;      // colormap_init() handles setup
   idx = +idx;
   wf.cmap = w3_clamp(idx, 0, kiwi.cmap_s.length - 1, 0);
   //console_log_dbgUs('# wf_cmap_cb idx='+ idx +' first='+ first +' wf.cmap='+ wf.cmap +' audioFFT_active='+ wf.audioFFT_active);
   w3_select_value(path, idx, { all:1 });    // all:1 changes both menus together
   kiwi_storeWrite('last_cmap', idx);
   mkcolormap();
   
   spectrum_dB_bands();
   update_maxmindb_sliders();
   wf_send('SET cmap='+ wf.cmap);
   w3_call('colormap_select');
   freqset_select();
}

function wf_aper_cb(path, idx, first)
{
   if (first) return;      // colormap_init() handles setup
   idx = +idx;
   var aper = w3_clamp(idx, 0, kiwi.aper_s.length - 1, 0);
   //console_log_dbgUs('# wf_aper_cb idx='+ idx +' first='+ first +' aper='+ aper +' audioFFT_active='+ wf.audioFFT_active +' need_autoscale='+ wf.need_autoscale);

   if (wf.audioFFT_active) {
      w3_select_value(path, 0);
      w3_disable(path, true);
      return;
   } else {
      w3_select_value(path, idx);
      kiwi_storeWrite('last_aper', idx);
   }

   wf.aper = aper;
   var auto = (wf.aper == kiwi.APER_AUTO);
   w3_innerHTML('id-wf-sliders', auto? wf.floor_ceil_sliders : wf.max_min_sliders);
   w3_color('id-button-wf-autoscale', null, auto? w3_selection_green : '');

   if (auto) {
      wf.save_maxdb = maxdb;
      wf.save_mindb_un = mindb_un;
      //console_log_dbgUs('# save maxdb='+ maxdb +' mindb='+ mindb);
   } else {
      //console_log_dbgUs('# restore maxdb='+ wf.save_maxdb +' mindb='+ wf.save_mindb_un +' zcorr='+ zoomCorrection());
      setmaxdb(wf.save_maxdb, {done:1});
      setmindb(wf.save_mindb_un - zoomCorrection(), {done:1});
   }

   spectrum_dB_bands();
   update_maxmindb_sliders();
   colormap_update();
   freqset_select();
}

function setwfspeed_cb(path, str, done)
{
   if (wf.audioFFT_active) {
	   //console.log('# setwfspeed-FFT '+ wf_speed +' str='+ str +' done='+ done);
      freqset_select();
      return;
   }
   
	wf_speed = +str;
	//console.log('# setwfspeed '+ wf_speed +' done='+ done);
	w3_set_value('slider-rate', wf_speed);
   var el = w3_el('slider-rate-field');
   el.innerHTML = wf_speeds[wf_speed];
   el.style.color = wf_speed? 'white':'orange';
   wf_send('SET wf_speed='+ wf_speed.toFixed(0));
   if (done) freqset_select();
}

function setwfspeed_wheel_cb()
{
   var nval = w3_slider_wheel('setwfspeed_wheel_cb', 'id-slider-rate', wf_speed, 1, 1);
   if (isNumber(nval)) setwfspeed_cb('', nval, 1);
}

function setmaxdb_cb(path, val, done, first)
{
   setmaxdb(val, {done:done, no_fsel:0});
}

function setmaxdb(str, opt)
{
	var strdb = parseFloat(str);
	var wstore = false;

   if (wf.aper == kiwi.APER_AUTO) {
      maxdb = strdb + wf.auto_ceil.val;
   } else {
      var input_max = w3_el('id-input-maxdb');
      var field_max = w3_el('id-field-maxdb');
      var field_min = w3_el('id-field-mindb');
      if (!input_max || !field_max || !field_min) return;
      wstore = true;

      if (strdb <= mindb) {
         maxdb = mindb + 1;
         input_max.value = maxdb;
         field_max.innerHTML = maxdb.toFixed(0) + ' dB';
         field_max.style.color = "red"; 
      } else {
         maxdb = strdb;
         input_max.value = maxdb;
         field_max.innerHTML = strdb.toFixed(0) + ' dB';
         field_max.style.color = "white"; 
         field_min.style.color = "white";
      }
   }
	
	setmaxmindb({done:opt.done, wstore:wstore, no_fsel:opt.no_fsel});
}

function setmaxdb_wheel_cb()
{
   var nval = w3_slider_wheel('setmaxdb_wheel_cb', 'id-input-maxdb', maxdb, 1, 10);
   if (isNumber(nval)) setmaxdb(nval, {done:1});
}

function incr_mindb(done, incr)
{
   // first time just switch to wf tab without making any change
   if (kiwi.cur_optbar != 'optbar-wf') {
      keyboard_shortcut_nav('wf');
      return;
   }
   
   if (wf.aper != kiwi.APER_MAN) return;
   
   var incrdb = (+mindb) + incr;
   var val = Math.max(-190, Math.min(-30, incrdb));
   //console.log('incr_mindb mindb='+ mindb +' incrdb='+ incrdb +' val='+ val);
   setmindb(val.toFixed(0), {done:done});
}

function setmindb_cb(path, val, done, first)
{
   setmindb(val, {done:done, no_fsel:0});
}

function setmindb(str, opt)
{
	var strdb = parseFloat(str);
   //console.log('setmindb strdb='+ strdb +' maxdb='+ maxdb +' mindb='+ mindb +' done='+ opt.done);
	var wstore = false;

   if (wf.aper == kiwi.APER_AUTO) {
      mindb = strdb + wf.auto_floor.val;
   } else {
      var input_min = w3_el('id-input-mindb');
      var field_max = w3_el('id-field-maxdb');
      var field_min = w3_el('id-field-mindb');
      if (!input_min || !field_max || !field_min) return;
      wstore = true;

      if (maxdb <= strdb) {
         mindb = maxdb - 1;
         input_min.value = mindb;
         field_min.innerHTML = mindb.toFixed(0) + ' dB';
         field_min.style.color = "red";
      } else {
         mindb = strdb;
         //console.log('setmindb SET strdb='+ strdb +' maxdb='+ maxdb +' mindb='+ mindb +' done='+ opt.done);
         input_min.value = mindb;
         field_min.innerHTML = strdb.toFixed(0) + ' dB';
         field_min.style.color = "white";
         field_max.style.color = "white";
      }
   }

	mindb_un = mindb + zoomCorrection();
	setmaxmindb({done:opt.done, wstore:wstore, no_fsel:opt.no_fsel});
}

function setmindb_wheel_cb()
{
   var nval = w3_slider_wheel('setmindb_wheel_cb', 'id-input-mindb', mindb, 1, 10);
   if (isNumber(nval)) setmindb(nval, {done:1});
}

function setmaxmindb(opt)
{
	full_scale = maxdb - mindb;
	full_scale = full_scale? full_scale : 1;	// can't be zero
	spectrum_dB_bands();
   //console.log('setmaxmindb maxdb='+ maxdb.toFixed(0) +' mindb='+ mindb.toFixed(0) +' done='+ opt.done);
   wf_send('SET maxdb='+ maxdb.toFixed(0) +' mindb='+ mindb.toFixed(0));
	need_clear_wf_sp_avg = true;

   if (opt.done) {
      if (!opt.no_fsel) freqset_select();
   	if (opt.wstore) {
   	   kiwi_storeWrite(wf.audioFFT_active? 'last_AF_max_dB' : 'last_max_dB', maxdb.toFixed(0));
   	   kiwi_storeWrite(wf.audioFFT_active? 'last_AF_min_dB' : 'last_min_dB', mindb_un.toFixed(0));	// need to save the uncorrected (z0) value
	   }
      w3_call('waterfall_maxmin_cb');
	}
}

function update_maxmindb_sliders()
{
	var auto = (wf.aper == kiwi.APER_AUTO);
	if (!auto) mindb = mindb_un - zoomCorrection();
	
	full_scale = maxdb - mindb;
	full_scale = full_scale? full_scale : 1;	// can't be zero
	spectrum_dB_bands();
   //console_log_dbgUs('# update_maxmindb_sliders: '+ (auto? 'AUTO':'norm') +' min/max='+ mindb +'/'+ maxdb +' ceil='+ wf.auto_ceil.val +' floor='+ wf.auto_floor.val);

   var db1, db2, db1_s, db2_s, field1, field2;
   
   if (auto) {
      db1 = wf.auto_ceil.val;
      db2 = wf.auto_floor.val;
      db1_s = db1.toFixed(0).positiveWithSign();
      db2_s = db2.toFixed(0).positiveWithSign();
      w3_el('id-input-ceildb').value = db1;     // update slider since called from wf_aper_cb()
      w3_el('id-input-floordb').value = db2;
      field1 = w3_el('id-field-ceildb');
      field2 = w3_el('id-field-floordb');
   } else {
      db1 = maxdb;
      db2 = mindb;
      db1_s = db1.toFixed(0);
      db2_s = db2.toFixed(0);
      w3_el('id-input-maxdb').value = db1;
      w3_el('id-input-mindb').value = db2;
      field1 = w3_el('id-field-maxdb');
      field2 = w3_el('id-field-mindb');
   }
   
   field1.innerHTML = db1_s + ' dB';
   field2.innerHTML = db2_s + ' dB';
   w3_call('waterfall_maxmin_cb');
}

function setceildb_cb(path, str, done)
{
   var input_ceil = w3_el('id-input-ceildb');
   var field_ceil = w3_el('id-field-ceildb');
   if (!input_ceil || !field_ceil) return;

	var ceildb = parseFloat(str);
   input_ceil.value = ceildb;
   w3_innerHTML(field_ceil, ceildb.toFixed(0).positiveWithSign() + ' dB');
   wf.auto_ceil.val = ceildb;
	set_ceilfloordb({done:done});
}

function setceildb_wheel_cb()
{
   var nval = w3_slider_wheel('setceildb_wheel_cb', 'id-input-ceildb', wf.auto_ceil.val, 1, 5);
   if (isNumber(nval)) setceildb_cb('', nval, 1);
}

function setfloordb_cb(path, str, done)
{
   var input_floor = w3_el('id-input-floordb');
   var field_floor = w3_el('id-field-floordb');
   if (!input_floor || !field_floor) return;

	var floordb = parseFloat(str);
   input_floor.value = floordb;
   w3_innerHTML(field_floor, floordb.toFixed(0).positiveWithSign() + ' dB');
   wf.auto_floor.val = floordb;
	set_ceilfloordb({done:done});
}

function setfloordb_wheel_cb()
{
   var nval = w3_slider_wheel('setfloordb_wheel_cb', 'id-input-floordb', wf.auto_floor.val, 1, 5);
   if (isNumber(nval)) setfloordb_cb('', nval, 1);
}

function set_ceilfloordb(opt)
{
   maxdb = wf.auto_maxdb + wf.auto_ceil.val;
   mindb = wf.auto_mindb + wf.auto_floor.val;
   //console.log('$set_ceilfloordb amin/amax/ceil/floor='+ wf.auto_mindb +'/'+ wf.auto_maxdb +'/'+ wf.auto_ceil.val +'/'+ wf.auto_floor.val);
   var range = maxdb - mindb;
   if (cfg.spec_min_range) {
      if (range < cfg.spec_min_range) {
         var boost = cfg.spec_min_range - range;
         //console.log('$set_ceilfloordb BOOST orange='+ range +' min='+ mindb +' max='+ maxdb +'=>'+ (maxdb + boost) +' min_range='+ cfg.spec_min_range);
         maxdb += boost;
         range = maxdb - mindb;
      }
   }
	mindb_un = mindb + zoomCorrection();
	//console_log_dbgUs('$set_ceilfloordb min/max/un='+ mindb +'/'+ maxdb +'/'+ mindb_un +' range='+ range);
	setmaxmindb({done:opt.done, wstore:false, no_fsel:opt.no_fsel});
	w3_call('waterfall_maxmin_cb');

   if (opt.done) {
   	if (!opt.no_fsel) freqset_select();
   	kiwi_storeWrite('last_ceil_dB', wf.auto_ceil.val.toFixed(0));
   	kiwi_storeWrite('last_floor_dB', wf.auto_floor.val.toFixed(0));
	}
}

function wf_autoscale_cb()
{
   //console.log('wf_autoscale_cb');
   wf.need_autoscale = 1;
	colormap_update();
   freqset_select();
}

var spectrum_slow_dev = 0;

function toggle_or_set_slow_dev(set, val)
{
	if (isNumber(set))
		spectrum_slow_dev = kiwi_toggle(set, val, spectrum_slow_dev, 'last_slow_dev');
	else
		spectrum_slow_dev ^= 1;
	w3_color('id-button-slow-dev', spectrum_slow_dev? 'lime':'white');
	freqset_select();
	kiwi_storeWrite('last_slow_dev', spectrum_slow_dev.toString());
	if (spectrum_slow_dev && wf_speed == WF_SPEED_FAST)
	   setwfspeed_cb('', WF_SPEED_MED, 1);
}

function toggle_or_set_spec_peak(set, val, trace, ev)
{
   var hold = (ev && ev.type == 'hold');
   var shift_click = (ev && ev.type == 'click' && ev.shiftKey);

   // in active mode, if button held down more than one second clear trace (without changing the mode)
   // enabled by the use of w3-hold in w3_button() psa
   if (hold) {
      trace = +val;  // when called as a w3_button() callback, val is cb_param (trace) and trace is "first" value (always false).
      console.log('toggle_or_set_spec_peak HOLD trace='+ trace);
      if (spec.peak_show[trace] == spec.PEAK_ON) {
         spec.peak_clear_btn[trace] = true;
      }
      return;
   }
   
   var prev;
	if (isNumber(set)) {
	   trace = +trace;
      prev = spec.peak_show[trace];
      //console.log('toggle_or_set_spec_peak SET set='+ set +' val='+ val +' prev='+ prev +' trace='+ trace);
		spec.peak_show[trace] = kiwi_toggle(set, val, spec.peak_show[trace], 'last_spec_peak'+ (trace? '1':''));
	} else {
	   trace = +val;
      prev = spec.peak_show[trace];
      if (shift_click) {
         spec.peak_show[trace]--;
         if (spec.peak_show[trace] < 0) spec.peak_show[trace] = 2;
      } else {
		   spec.peak_show[trace] = (spec.peak_show[trace] + 1) % 3;
		}
      //console.log('toggle_or_set_spec_peak TOGGLE set='+ set +' val='+ val +' trace='+ trace +' NEW peak_show='+ spec.peak_show[trace]);
	}
	var peak_clear_btn = (prev != spec.PEAK_OFF && spec.peak_show[trace] == spec.PEAK_OFF);
	//console.log('toggle_or_set_spec_peak peak_clear_btn='+ peak_clear_btn +' trace='+ trace);
	if (peak_clear_btn) spec.peak_clear_btn[trace] = true;
	w3_color('id-button-spec-peak'+ trace, ['white', 'lime', 'orange'][spec.peak_show[trace]]);
	//w3_color('id-button-spec-peak'+ trace, null, ['', w3_selection_green, 'grey'][spec.peak_show[trace]]);
	freqset_select();
	kiwi_storeWrite('last_spec_peak'+ (trace? '1':''), (spec.peak_show[trace] == spec.PEAK_ON)? '1':'0');
}


////////////////////////////////
// #audio
////////////////////////////////

var muted_until_freq_set = true;
var recording = false;

function setvolume_cb(path, val, done, first, cb_param)
{
   setvolume(done, val);
}

function setvolume(done, str)
{
   //console.log('setvolume done='+ done +' str='+ str +' t/o='+ typeof(str) +' volume_f WAS='+ kiwi.volume_f);
   kiwi.volume = +str;
   if (!isNumber(kiwi.volume)) kiwi.volume = 50;
   
   // when 'v' shortcut key, or URL param, attempts to set < 0 clamp to 0
   kiwi.volume = w3_clamp3(kiwi.volume, 0, 200, 0, 50, 50);
   //console.log('setvolume POST-CLAMP vol='+ kiwi.volume +' t/o='+ typeof(kiwi.volume));

   // volume_f is the [0,2] value actually used by audio.js
   // don't set to zero because that triggers FF audio silence bug
   kiwi.volume_f = (kiwi.muted || kiwi.volume == 0)? 1e-6 : (kiwi.volume/100);
   //console.log('setvolume vol='+ kiwi.volume +' muted='+ kiwi.muted +' vol_f='+ kiwi.volume_f.toFixed(6));
   if (done) {
      //console.log('setvolume DONE vol='+ kiwi.volume +' t/o='+ typeof(kiwi.volume));
      w3_set_value('id-input-volume', kiwi.volume);
      kiwi_storeWrite('last_volume', kiwi.volume);
      freqset_select();
   }
}

function setvolume_wheel_cb()
{
   var nval = w3_slider_wheel('setvolume_wheel_cb', 'id-input-volume', kiwi.volume, 5, 15);
   if (isNumber(nval)) setvolume(1, nval);
}

function shortcut_setvolume(incr)
{
   toggle_or_set_mute(0);

   /*
   // first time just switch to audio tab without making any change
   if (kiwi.cur_optbar != 'optbar-audio') {
      keyboard_shortcut_nav('audio');
      return;
   }
   */
   
   setvolume(1, kiwi.volume + incr);
}

function toggle_or_set_mute(set)
{
	if (isNumber(set))
      kiwi.muted = set;
   else
	   kiwi.muted ^= 1;
   //console.log('$toggle_or_set_mute set='+ set +' muted='+ kiwi.muted);
	if (!owrx.squelched_overload) {
      w3_show_hide('id-mute-no', !kiwi.muted);
      w3_show_hide('id-mute-yes', kiwi.muted);
   }
   kiwi.volume_f = (kiwi.muted || kiwi.volume == 0)? 1e-6 : (kiwi.volume/100);
	//if (!isNumber(set)) console.log('vol='+ kiwi.volume +' muted='+ kiwi.muted +' vol_f='+ kiwi.volume_f.toFixed(6));
	//snd_send('SET mute='+ (kiwi.muted? 1:0));
   freqset_select();

   if (!isNumber(set)) canvas_log_clear();
   if (owrx.debug_drag) owrx.drag_count = 0;
}

var de_emphasis = 0, de_emphasis_nfm = 0;
var de_emphasis_s = [ 'off', '75 uS', '50 uS' ];
var de_emphasis_nfm_s = [ 'off', 'on', '+LF' ];

function de_emp_cb(path, idx, first, nfm)
{
	var kmode = ext_mode(cur_mode);
	//console.log('$ de_emp_cb path='+ path +' idx='+ idx +' nfm='+ nfm +' first='+ first +' kmode.NBFM='+ kmode.NBFM);
   if (+nfm) {
      de_emphasis_nfm = +idx;
      snd_send('SET de_emp='+ de_emphasis_nfm +' nfm=1');
      kiwi_storeWrite('last_de_emphasis_nfm', de_emphasis_nfm.toString());
   } else {
      de_emphasis = +idx;
      snd_send('SET de_emp='+ de_emphasis +' nfm=0');
      kiwi_storeWrite('last_de_emphasis', de_emphasis.toString());
   }
}

function setpan_cb(path, val, done, first, cb_param)
{
   setpan(done, val);
}

function setpan(done, str, no_write_cookie)
{
   var pan = +str;
   //console.log('pan='+ pan.toFixed(2));
   if (pan > -0.1 && pan < +0.1) pan = 0;    // snap-to-zero interval
   w3_set_value('id-pan-value', pan);
   w3_color('id-pan-value', null, (pan < 0)? 'rgba(255,0,0,0.3)' : (pan? 'rgba(0,255,0,0.2)':''));
   w3_innerHTML('id-pan-field', pan? ((Math.abs(pan)*100).toFixed(0) +' '+ ((pan < 0)? 'L':'R')) : 'L=R');
   audio_set_pan(pan);
   kiwi.pan = pan;

   if (done) {
      if (no_write_cookie != true)
         kiwi_storeWrite('last_pan', pan.toString());
      freqset_select();
   }
}

function setpan_wheel_cb()
{
   var nval = w3_slider_wheel('setpan_wheel_cb', 'id-pan-value', kiwi.pan, 0.1, 0.1);
   if (isNumber(nval)) setpan(1, nval);
}

// called from both audio_init() and panels_setup() since there is a race setting audio_panner
function audio_panner_ui_init()
{
   if (audio_panner) {
      w3_hide2('id-vol-comp');
      w3_hide2('id-vol', false);
      w3_hide2('id-pan', false);
      setpan(1, kiwi.pan, true);
   }
}

function pb_default_cb()
{
   restore_passband(cur_mode);
   demodulator_analog_replace(cur_mode);
   freqset_update_ui(owrx.FSET_NOP);      // restore previous
}

function setpb_cb(path, val, done, first, which)
{
   val = +val;
   which = +which;
 
   var slider_input = false;
   if (which < 0)
      which = -which - 1;     // called from other code with new lo/hi values
   else
      slider_input = true;    // called from sliders being adjusted
   
   var id = 'id-pb-'+ ['lo', 'hi', 'center', 'width'][which];
   //console.log('setpb_cb in='+ TF(slider_input) +' val='+ val +' '+ id + (slider_input? (' done='+ TF(done)) : ''));
   var hpb = _10Hz(audio_input_rate/2);
   var el = w3_el(id);
	var m = isDefined(cur_mode)? cur_mode.substr(0,2) : 'xx';
   if (m == 'us') {
      el.min = 0;
      el.max = (which == owrx.PB_WIDTH)? audio_input_rate : hpb;
   } else
   if (m == 'ls') {
      el.min = -hpb;
      el.max = (which == owrx.PB_WIDTH)? audio_input_rate : 0;
   } else {
      el.min = (which == owrx.PB_WIDTH)? 0 : -hpb;
      el.max = (which == owrx.PB_WIDTH)? audio_input_rate : hpb;
   }

   if (slider_input) {
      var demod = demodulators[0];
      if (!isArg(demod)) return;    // too early
      
      var lo = demod.low_cut, hi = demod.high_cut, center = _10Hz(lo + (hi-lo) / 2), width = hi-lo;
      var _lo = lo, _hi = hi;
      switch (which) {
         case owrx.PB_LO: lo = val; break;
         case owrx.PB_HI: hi = val; break;
         case owrx.PB_CENTER: center = val; lo = _10Hz(center - width/2); hi = _10Hz(center + width/2); break;
         case owrx.PB_WIDTH: width = val; lo = _10Hz(center - width/2); hi = _10Hz(center + width/2); break;
      }
      // owrx passband code will eventually callback this routine setting all 4 sliders
      //if (which == 3) console.log('pb width='+ width +' center='+ center);
      //console.log('setpb_cb '+ _lo +'|'+ _hi +' => '+ lo +'|'+ hi +' val='+ val +' hpb='+ hpb);
      var ok = 1;
      _lo = Math.max(lo, -hpb);
      _hi = Math.min(hi, hpb);
      var _width = _10Hz(_hi-_lo);
      var _center = _10Hz(_lo + _width / 2);
      if (which == 2 && _width != width) ok = 0;      // adjusting center: width must not change
      //if (which == 2 && _width != width) console.log('pb2 _width='+ _width +' width='+ width +' center='+ center +' '+ lo +'|'+ hi +' '+ _lo +'|'+ _hi);
      if (which == 3 && _center != center) ok = 0;    // adjusting width: center must not change
      //if (!ok) console.log('pb !ok _width='+ _width +' width='+ width);
      if (ok)
         ext_set_passband(_lo, _hi);
   } else {
      w3_set_value(id, val);
      w3_innerHTML(id +'-val', val);
   }
}


function toggle_or_set_hide_bars(set)
{
	if (isNumber(set))
      owrx.hide_bars = set;
   else {
	   owrx.hide_bars = (owrx.hide_bars + 1) & owrx.HIDE_ALLBARS;
	   
	   // there is no top container to hide if data container or spectrum in use
      if ((owrx.hide_bars & owrx.HIDE_TOPBAR) && (extint.using_data_container || spec.source != spec.NONE))
	      owrx.hide_bars = (owrx.hide_bars + 1) & owrx.HIDE_ALLBARS;
	}
   //console.log('toggle_or_set_hide_bars set='+ set +' hide_bars='+ owrx.hide_bars);
   w3_set_props('id-top-container', 'w3-panel-override-hide', owrx.hide_bars & owrx.HIDE_TOPBAR);
   w3_set_props('id-band-container', 'w3-panel-override-hide', owrx.hide_bars & owrx.HIDE_BANDBAR);
   openwebrx_resize();
}

var hide_panels = 0;
function toggle_or_set_hide_panels(set)
{
	if (isNumber(set))
      hide_panels = set;
   else
	   hide_panels ^= 1;

   //console.log('toggle_or_set_hide_panels set='+ set +' hide_panels='+ hide_panels);
   w3_iterate_children('id-panels-container', function(el) {
      if (w3_contains(el, 'class-panel')) {
         w3_set_props(el, 'w3-panel-override-hide', hide_panels);
      }
   });
}

var test_button;
function toggle_or_set_test(set)
{
	if (isNumber(set))
      test_button = set;
   else
	   test_button ^= 1;
   console.log('toggle_or_set_test set='+ set +' test_button='+ test_button);
   w3_color('id-button-test', test_button? 'lime':'white');
	snd_send('SET test='+ (test_button? 1:0));
   freqset_select();
}

function toggle_or_set_rec(set)
{
   console.log('toggle_or_set_rec set=' + set + ' recording=' + recording);
   
   if (isBoolean(set) || isNumber(set))
      recording = set;
   else
      recording = !recording;
   w3_spin('id-rec1', recording);
   w3_spin('id-rec2', recording);
   w3_remove_then_add('id-rec1', 'w3-text-white', 'w3-text-pink');      // in case squelched when recording stopped
   w3_remove_then_add('id-rec2', 'w3-text-white', 'w3-text-pink');      // in case squelched when recording stopped

   if (recording) {
      // Start recording. This is a 'window' property, so audio_recv(), where the
      // recording hooks are, can access it.
      window.recording_meta = {
         buffers: [],         // An array of ArrayBuffers with audio samples to concatenate
         data: null,          // DataView for the current buffer
         offset: 0,           // Current offset within the current ArrayBuffer
         total_size: 0,       // Total size of all recorded data in bytes
         filename: kiwi_timestamp_filename('', '.wav')
      };
      window.recording_meta.buffers.push(new ArrayBuffer(65536));
      window.recording_meta.data = new DataView(window.recording_meta.buffers[0]);
   } else

   if (window.recording_meta) {
      // Stop recording. Build a WAV file.
      var wav_header = new ArrayBuffer(44);
      var wav_data = new DataView(wav_header);
      wav_data.setUint32(0, 0x52494646);                                  // ASCII "RIFF"
      wav_data.setUint32(4, window.recording_meta.total_size + 36, true); // Little-endian size of the remainder of the file, excluding this field
      wav_data.setUint32(8, 0x57415645);                                  // ASCII "WAVE"
      wav_data.setUint32(12, 0x666d7420);                                 // ASCII "fmt "
      wav_data.setUint32(16, 16, true);                                   // Length of this section ("fmt ") in bytes
      wav_data.setUint16(20, 1, true);                                    // PCM coding
      var nch = ext_is_IQ_or_stereo_curmode()? 2 : 1;                     // Two channels for stereo modes, one channel otherwise
      wav_data.setUint16(22, nch, true);
      var srate = Math.round(audio_input_rate || 12000);
      wav_data.setUint32(24, srate, true);                                // Sample rate
      var bpsa = 16;                                                      // Bits per sample
      var Bpsa = bpsa/8;                                                  // Bytes per sample
      wav_data.setUint32(28, srate*nch*Bpsa, true);                       // Byte rate
      wav_data.setUint16(32, nch*Bpsa, true);                             // Block align
      wav_data.setUint16(34, bpsa, true);                                 // Bits per sample
      wav_data.setUint32(36, 0x64617461);                                 // ASCII "data"
      wav_data.setUint32(40, window.recording_meta.total_size, true);     // Little-endian size of all recorded samples in bytes
      window.recording_meta.buffers.unshift(wav_header);                  // Prepend the WAV header to the recorded audio
      var wav_file = new Blob(window.recording_meta.buffers, { type: 'octet/stream' });

      // Download the WAV file.
      var a = document.createElement('a');
      a.style = 'display: none';
      a.href = window.URL.createObjectURL(wav_file);
      a.download = window.recording_meta.filename;
      document.body.appendChild(a); // https://bugzilla.mozilla.org/show_bug.cgi?id=1218456
      a.click();
      window.URL.revokeObjectURL(a.href);
      document.body.removeChild(a);

      delete window.recording_meta;
      console.log('toggle_or_set_rec SAVED');
      audio_pre_record_buf_init();
   }
}

// squelch
var squelched = 0;
var squelch = 0;

var pre_record = 0;
var pre_record_s = [ '0s', '1s', '2s', '5s', '10s' ];
var pre_record_v = [ 0, 1, 2, 5, 10 ];

var squelch_tail = 0;
var squelch_tail_s = [ '0s', '.2s', '.5s', '1s', '2s' ];
var squelch_tail_v = [ 0, 0.2, 0.5, 1, 2 ];

function squelch_action(sq)
{
   squelched = sq;
   var mute_color = squelched? 'white' : kiwi.unmuted_color;
   var sq_color = mute_color;
   //console.log('squelch_action squelched='+ (squelched? 1:0) +' sMeter_dBm='+ owrx.sMeter_dBm.toFixed(0) +' sq_color='+ sq_color);

   owrx.squelched_overload = (owrx.sMeter_dBm >= cfg.overload_mute);
   if (owrx.prev_squelched_overload != owrx.squelched_overload) {
      //console.log('squelched_overload='+ (owrx.squelched_overload? 1:0));
      w3_show_hide('id-mute-exclaim', owrx.squelched_overload);
      w3_show_hide('id-mute-no', owrx.squelched_overload? false : !kiwi.muted);
      w3_show_hide('id-mute-yes', owrx.squelched_overload? false : kiwi.muted);
      owrx.prev_squelched_overload = owrx.squelched_overload;
   }   
   
   w3_color('id-squelch-label', sq_color);
   w3_color('id-mute-no', mute_color);
   
   if (recording) {
      var stop = (squelched || owrx.squelched_overload);
      w3_spin('id-rec1', !stop);
      w3_spin('id-rec2', !stop);
      w3_remove_then_add('id-rec1', 'w3-text-white w3-text-pink', stop? 'w3-text-white' : 'w3-text-pink');
      w3_remove_then_add('id-rec2', 'w3-text-white w3-text-pink', stop? 'w3-text-white' : 'w3-text-pink');
   }
}

function squelch_setup(flags)
{
   var nbfm = ext_mode(cur_mode).NBFM;
   
   if (flags & toggle_e.FROM_COOKIE) { 
      var sq = kiwi_storeRead('last_squelch'+ (nbfm? '':'_efm'));
      squelch = sq? +sq : 0;
      //console.log('$squelch_setup nbfm='+ nbfm +' sq='+ sq +' squelch='+ squelch);
      w3_el('id-squelch-value').max = nbfm? 99:40;
      w3_set_value('id-squelch-value', squelch);

      var pre = kiwi_storeInit('last_pre', 0);
      pre_record = pre? +pre : 0;
      w3_select_value('id-pre-rec', pre_record);

      var tail = kiwi_storeInit('last_tail', 0);
      squelch_tail = tail? +tail : 0;
      w3_select_value('id-squelch-tail', squelch_tail);
   }
   
   set_squelch_cb('', squelch, true, false, true);
   pre_record_cb('', pre_record, false, true);
   squelch_tail_cb('', squelch_tail, false, true);

	if (nbfm) {
	   squelch_action(squelched);
   } else {
      w3_color('id-mute-no', kiwi.unmuted_color);
   }

   w3_hide2('id-pre-rec', nbfm);
   w3_hide2('id-squelch-tail', nbfm);
   w3_hide2('id-squelch', cur_mode == 'drm');
}

function send_squelch()
{
   var nbfm = ext_mode(cur_mode).NBFM;
   snd_send("SET squelch="+ squelch.toFixed(0) +' param='+ (nbfm? squelch_threshold : squelch_tail_v[squelch_tail]).toFixed(2));
}

function set_squelch_cb(path, str, done, first, no_write_cookie)
{
   var nbfm = ext_mode(cur_mode).NBFM;
   //console.log('$set_squelch_cb path='+ path +' str='+ str +' done='+ done +' first='+ first +' no_write_cookie='+ no_write_cookie +' nbfm='+ nbfm);
   if (first) return;

   squelch = parseFloat(str);
   var sq_s = squelch.toFixed(0);
	w3_el('id-squelch-field').innerHTML = squelch? (sq_s + (nbfm? '':' dB')) : 'off';
   w3_set_value('id-squelch-value', squelch);
   w3_color('id-squelch-value', null, squelch? '' : 'rgba(255,0,0,0.3)');
   send_squelch();

   if (done) {
      if (no_write_cookie != true) {
         kiwi_storeWrite('last_squelch'+ (nbfm? '':'_efm'), sq_s);
      }
      freqset_select();
   }
}

function set_squelch_wheel_cb()
{
   var nval = w3_slider_wheel('set_squelch_wheel_cb', 'id-squelch-value', squelch, 1, 5);
   if (isNumber(nval)) set_squelch_cb('', nval, 1);
}

var squelch_enable = 1;
var squelch_save = null;

function squelch_zero_cb(path, idx, first)
{
   //console.log('squelch_zero_cb idx='+ idx +' first='+ first +' squelch_save='+ squelch_save +' squelch='+ squelch);
   squelch_enable ^= 1;
   w3_color(path, squelch_enable? 'lime':'magenta');
   var squelch_new;
   if (squelch_enable) {
      // enable
      squelch_new = (squelch_save == null)? squelch : squelch_save;
      freqset_select();
      w3_disable('id-squelch-value', false);
   } else {
      // disable
      squelch_save = squelch;
      squelch_new = 0;
   }
   set_squelch_cb('', squelch_new, true, false, true);
   if (!squelch_enable)
      w3_disable('id-squelch-value', true);
}

function pre_record_cb(path, val, first, no_write_cookie)
{
   //console.log('pre_record_cb path='+ path +' val='+ val +' first='+ first +' no_write_cookie='+ no_write_cookie);
   if (first) return;
   pre_record = +val;
   audio_pre_record_buf_init();

   // nbfm has no pre menu
   if (no_write_cookie != true && /* safety net */ !ext_mode(cur_mode).NBFM)
      kiwi_storeWrite('last_pre', pre_record);
}

function squelch_tail_cb(path, val, first, no_write_cookie)
{
   //console.log('squelch_tail_cb path='+ path +' val='+ val +' first='+ first +' no_write_cookie='+ no_write_cookie);
   if (first) return;
   squelch_tail = +val;
   send_squelch();

   // nbfm has no tail menu
   if (no_write_cookie != true && /* safety net */ !ext_mode(cur_mode).NBFM)
      kiwi_storeWrite('last_tail', squelch_tail);
}

function sam_pll_reset_cb(path, val, first)
{
   //console.log('sam_pll_reset_cb path='+ path +' val='+ val +' first='+ first);
   snd_send('SET sam_pll=-1');
}

function sam_pll_cb(path, val, first)
{
   //console.log('sam_pll_cb path='+ path +' val='+ val +' first='+ first);
   owrx.sam_pll = +val;
   snd_send('SET sam_pll='+ owrx.sam_pll);
}

function chan_null_cb(path, val, first)
{
   //console.log('$chan_null_cb path='+ path +' val='+ val +' first='+ first);
   if (first) return;   // cur_mode still undefined this early
   owrx.chan_null = +val;
   ext_set_mode(cur_mode);    // set mode so chan_null value passed to server via mparam
}

function SAM_opts_cb(path, val, first)
{
   //console.log('$SAM_opts_cb path='+ path +' val='+ val +' first='+ first);
   if (first) return;   // cur_mode still undefined this early
   owrx.SAM_opts = +val;
   ext_set_mode(cur_mode);    // set mode so SAM_opts value passed to server
}

function ovld_mute_cb(path, val, first)
{
   if (first) return;
   owrx.ovld_mute = +val;
   snd_send('SET ovld_mute='+ (owrx.ovld_mute? 1:0));
}

// less buffering and compression buttons
var btn_less_buffering = 0;

function toggle_or_set_audio(set, val)
{
	if (isNumber(set))
		btn_less_buffering = kiwi_toggle(set, val, btn_less_buffering, 'last_audio');
	else
		btn_less_buffering ^= 1;

   var el = w3_el('id-button-buffering');
   if (el) {
      el.style.color = btn_less_buffering? 'lime':'white';
      el.style.visibility = 'visible';
      freqset_select();
   }
	kiwi_storeWrite('last_audio', btn_less_buffering.toString());
	
	// if toggling (i.e. not the first time during setup) reinitialize audio with specified buffering
	if (!isNumber(set)) {
	   // haven't got audio_init() calls to work yet (other than initial)
	   //audio_init(null, btn_less_buffering, btn_compression);
      window.location.reload();
   }
}

var btn_compression = 0;

function toggle_or_set_compression(set, val)
{
   // Prevent compression setting changes while recording.
   if (recording) {
      return;
   }

	//console.log('toggle_or_set_compression set='+ set +' val='+ val);
	if (isNumber(set))
		btn_compression = kiwi_toggle(set, val, btn_compression, 'last_compression');
	else
		btn_compression ^= 1;

   w3_els('id-button-compression',
      function(el) {
         el.style.color = btn_compression? 'lime':'white';
         el.style.visibility = 'visible';
         freqset_select();
      });
   
	var opt = isNumber(set)? set : 0;
   if (!(opt & toggle_e.NO_WRITE_COOKIE))
	   kiwi_storeWrite('last_compression', btn_compression.toString());
	//console.log('SET compression='+ btn_compression.toFixed(0));
	snd_send('SET compression='+ btn_compression.toFixed(0));
}


////////////////////////////////
// #AGC
////////////////////////////////

function set_agc()
{
   var isCW = ext_mode(cur_mode).CW;
   //console.log('isCW='+ isCW +' agc='+ agc);
   w3_color('label-threshold', (!isCW && agc)? 'orange' : 'white');
   w3_color('label-threshCW',  ( isCW && agc)? 'orange' : 'white');
   var thold = isCW? threshCW : thresh;
   //console.log('AGC SET thresh='+ thold);
	snd_send('SET agc='+ agc +' hang='+ hang +' thresh='+ thold +' slope='+ slope +' decay='+ decay +' manGain='+ manGain);
}

var agc = 0;

function toggle_agc(path, cbp, first, evt)
{
	if (any_alternate_click_event(evt)) {
		setup_agc(toggle_e.SET);
	} else {
		toggle_or_set_agc();
	}

	return cancelEvent(evt);
}

function agc_over(evt)
{
	w3_el('id-button-agc').title = any_alternate_click_event(evt)? 'restore AGC params':'';
}

function agc_load_defaults(path, cbp, first, evt)
{
   setup_agc(toggle_e.SET);
	return cancelEvent(evt);
}

var default_agc = 1;
var default_hang = 0;
var default_manGain = 50;
var default_thresh = -100;
var default_threshCW = -130;
var default_slope = 6;
var default_decay = 1000;

function setup_agc(toggle_flags)
{
	agc = kiwi_toggle(toggle_flags, default_agc, agc, 'last_agc'); toggle_or_set_agc(agc);
	hang = kiwi_toggle(toggle_flags, default_hang, hang, 'last_hang'); toggle_or_set_hang(hang);
	manGain = kiwi_toggle(toggle_flags, default_manGain, manGain, 'last_manGain'); setManGain_cb('', manGain, 1);
	thresh = kiwi_toggle(toggle_flags, default_thresh, thresh, 'last_thresh'); setThresh_cb('', thresh, 1);
	threshCW = kiwi_toggle(toggle_flags, default_threshCW, threshCW, 'last_threshCW'); setThreshCW_cb('', threshCW, 1);
	slope = kiwi_toggle(toggle_flags, default_slope, slope, 'last_slope'); setSlope_cb('', slope, 1);
	decay = kiwi_toggle(toggle_flags, default_decay, decay, 'last_decay'); setDecay_cb('', decay, 1);
}

function toggle_or_set_agc(set)
{
	if (isNumber(set))
		agc = set;
	else
		agc ^= 1;
	
	w3_el('id-button-agc').style.color = agc? 'lime':'white';
	if (agc) {
		w3_el('id-label-man-gain').style.color = 'white';
		w3_el('id-button-hang').style.borderColor = w3_el('label-slope').style.color = w3_el('label-decay').style.color = 'orange';
	} else {
		w3_el('id-label-man-gain').style.color = 'orange';
		w3_el('id-button-hang').style.borderColor = w3_el('label-slope').style.color = w3_el('label-decay').style.color = 'white';
	}
	set_agc();
	kiwi_storeWrite('last_agc', agc.toString());
   freqset_select();
}

var hang = 0;

function toggle_or_set_hang(set)
{
   console.log('toggle_or_set_hang set='+ set +' hang='+ hang);
	if (isNumber(set))
		hang = set;
	else
		hang ^= 1;

	w3_el('id-button-hang').style.color = hang? 'lime':'white';
	set_agc();
	kiwi_storeWrite('last_hang', hang.toString());
   freqset_select();
}

var manGain = 0;

function setManGain_cb(path, str, done, first)
{
   if (first) return;
   manGain = parseFloat(str);
   w3_el('id-input-man-gain').value = manGain;
   w3_el('id-field-man-gain').innerHTML = str;
	set_agc();
	kiwi_storeWrite('last_manGain', manGain.toString());
   if (done) freqset_select();
}

function setManGain_wheel_cb()
{
   var nval = w3_slider_wheel('setManGain_wheel_cb', 'id-input-man-gain', manGain, 1, 10);
   if (isNumber(nval)) setManGain_cb('', nval, 1);
}

var thresh = 0;

function setThresh_cb(path, str, done, first)
{
   if (first) return;
   thresh = parseFloat(str);
   w3_el('id-input-threshold').value = thresh;
   w3_el('id-field-threshold').innerHTML = str;
	set_agc();
	kiwi_storeWrite('last_thresh', thresh.toString());
   if (done) freqset_select();
}

function setThresh_wheel_cb()
{
   var nval = w3_slider_wheel('setThresh_wheel_cb', 'id-input-threshold', thresh, 1, 10);
   if (isNumber(nval)) setThresh_cb('', nval, 1);
}

var threshCW = 0;

function setThreshCW_cb(path, str, done, first)
{
   if (first) return;
   threshCW = parseFloat(str);
   w3_el('id-input-threshCW').value = threshCW.toFixed(0);
   w3_el('id-field-threshCW').innerHTML = str;
	set_agc();
	kiwi_storeWrite('last_threshCW', threshCW.toString());
   if (done) freqset_select();
}

function setThreshCW_wheel_cb()
{
   var nval = w3_slider_wheel('setThreshCW_wheel_cb', 'id-input-threshCW', threshCW, 1, 10);
   if (isNumber(nval)) setThreshCW_cb('', nval, 1);
}

var slope = 0;

function setSlope_cb(path, str, done, first)
{
   if (first) return;
   slope = parseFloat(str);
   w3_el('id-input-slope').value = slope;
   w3_el('id-field-slope').innerHTML = str;
	set_agc();
	kiwi_storeWrite('last_slope', slope.toString());
   if (done) freqset_select();
}

function setSlope_wheel_cb()
{
   var nval = w3_slider_wheel('setSlope_wheel_cb', 'id-input-slope', slope, 1, 1);
   if (isNumber(nval)) setSlope_cb('', nval, 1);
}

var decay = 0;

function setDecay_cb(path, str, done, first)
{
   if (first) return;
   decay = parseFloat(str);
   w3_el('id-input-decay').value = decay;
   w3_el('id-field-decay').innerHTML = str;
	set_agc();
	kiwi_storeWrite('last_decay', decay.toString());
   if (done) freqset_select();
}

function setDecay_wheel_cb()
{
   var nval = w3_slider_wheel('setDecay_wheel_cb', 'id-input-decay', decay, 100, 500);
   if (isNumber(nval)) setDecay_cb('', nval, 1);
}

function agc_help()
{
   var s = 
      w3_text('w3-medium w3-bold w3-text-aqua', 'AGC help') +
      w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
         w3_div('w3-margin-R-8 w3-margin-bottom',
            'To be supplied...'
         )
      );
   confirmation_show_content(s, 600, 300);
   w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
}


////////////////////////////////
// #users
////////////////////////////////

function users_setup()
{
   var s = '';
	for (var i=0; i < rx_chans; i++) {
	   s +=
	      w3_div('',
            w3_div('w3-show-inline-block w3-left w3-margin-R-5 ' +
               ((i == rx_chan)? 'w3-text-css-lime' : 'w3-text-css-orange'), 'RX'+ i),
            w3_div('id-optbar-user-'+ i +' w3-show-inline-block')
         );
	}

   var mobile1 = kiwi_isMobile()? ' w3-font-16px' : '';
   var mobile2 = ' ontouchstart="popup_keyboard_touchstart(event)"';
	s +=
		w3_inline('w3-valign w3-margin-R-8/1:w3-ialign-right 1:w3-ialign-bottom 1:w3-margin-B-4',
         w3_input('w3-margin-TB-4/w3-label-not-bold/w3-custom-events' + mobile1 +
            '|padding:1px|size=100 onchange="ident_complete(\'el\', 2)" onkeyup="ident_keyup(this, event, 2)"'+ mobile2,
            'Your name or callsign:', 'ident-input2'),
         w3_button('id-admin-page-btn w3-aqua w3-small w3-padding-small w3-margin-L-16', 'admin page', 'admin_page_cb')
         
      ) +

      w3_hr('cl-cpanel-hr') +
      w3_div('',
         w3_div('w3-text-aqua', '<b>Mouse wheel / trackpad setup</b>'),
         w3_inline('w3-margin-T-8 w3-margin-R-8/',
            w3_select('id-wheel-dev w3-text-red||title=""', '', 'device preset', 'owrx.wheel_dev', owrx.wheel_dev, owrx.wheel_dev_s, 'wheel_dev_cb'),
            w3_select('id-wheel-dir w3-text-red w3-margin-L-8||title=""', '', 'direction', 'owrx.wheel_dir', owrx.wheel_dir, owrx.wheel_dir_s, 'wheel_dir_cb'),
            w3_button('w3-green w3-small w3-padding-small w3-margin-R-4 w3-btn-right', 'help', 'wheel_help')
         ),
         w3_inline('w3-margin-T-8 w3-margin-B-8 w3-margin-R-8/w3-margin-between-8',
            w3_input('id-wheel-poll w3-padding-tiny', 'Poll (msec)', 'wheel_poll', 0, 'wheel_param_cb|0'),
            w3_input('id-wheel-fast w3-padding-tiny', 'Fast threshold', 'wheel_fast', 0, 'wheel_param_cb|1'),
            w3_input('id-wheel-unlock w3-padding-tiny', 'Unlock (msec)', 'wheel_unlock', 0, 'wheel_param_cb|2')
         )
      );

	w3_innerHTML('id-optbar-user', w3_div('w3-nowrap', s));
}

function admin_page_cb() {
   // NB: Without the indirection provided by the minimal delay the browser will give a popup warning.
   // This does not occur for a button callback calling admin_page_cb()
   kiwi_open_or_reload_page({ path:'admin', tab:1, delay:1 });
}


////////////////////////////////
// #control panel
////////////////////////////////

// icon callbacks
function freq_vfo_cb(el, val, trace, ev)
{
   if (ev) {
      var hold = (ev.type == 'hold');
      var hold_done = (ev.type == 'hold-done');
      if (hold) {
         w3_remove_then_add(el, 'w3-aqua w3-orange', 'w3-red');
         vfo_update(true);
         return;
      } else
      if (hold_done) {
         w3_remove_then_add(el, 'w3-red', owrx.vfo_ab? 'w3-orange' : 'w3-aqua');
         return;
      }
      // else regular click
   }

   owrx.vfo_ab ^= 1;
   //console.log('VFO A/B ='+ owrx.vfo_ab);
   w3_flip_colors('id-freq-vfo', 'w3-aqua w3-orange', owrx.vfo_ab);
   w3_innerHTML('id-freq-vfo', "AB"[owrx.vfo_ab]);
   
	var vfo = parse_freq_pb_mode_zoom(kiwi_storeRead('last_vfo_'+ "AB"[owrx.vfo_ab]));
	var mode, zoom = 0;
	if (vfo[3]) mode = vfo[3].toLowerCase();
	if (vfo[4]) zoom = +vfo[4];
	//console.log('VFO TUNE '+ vfo[1] +' '+ mode +' '+ zoom);
	ext_tune(vfo[1], mode, ext_zoom.ABS, zoom);
}

function toggle_or_set_spec(set, val, dir, ev)
{
   var no_close_ext = false;
   var shift_click = (ev && ev.type == 'click' && ev.shiftKey);

	if (isNumber(set) && (set & toggle_e.SET)) {
		spec.source = +val;
		no_close_ext = ((set & toggle_e.NO_CLOSE_EXT) != 0);
      //console.log('toggle_or_set_spec SET val='+ val);
	} else {
	   if ((isUndefined(dir) || dir == false || dir == 1) && !shift_click) {
		   spec.source = (spec.source + 1) % spec.CHOICES;
		} else {
		   spec.source--;
		   if (spec.source < 0) spec.source = spec.CHOICES - 1;
		}
      //console.log('toggle_or_set_spec NEXT spec.source='+ spec.source +' dir='+ dir);
	}
   var isSpec = (spec.source != spec.NONE);
   //console.log('toggle_or_set_spec isSpec='+ isSpec);

	// close the extension first if it's using the data container and the spectrum button is pressed
	if (!no_close_ext && extint.using_data_container && isSpec) {
	   //console.log('toggle_or_set_spec: extint_panel_hide()..');
		extint_panel_hide( /* skip_calling_hide_spec */ true);
	}

   //console.log('toggle_or_set_spec: source='+ spec.source);
   var el = w3_el('id-button-spectrum');
   el.innerHTML = ['Spectrum', 'Spec RF', 'Spec AF'][spec.source];
	el.style.color = isSpec? 'lime':'white';
	if (isSpec) {
	   // delay switching container visibility until data update to prevent flash of previous spectrum
	   spec.switch_container = true;
	} else {
      w3_show_hide('id-spectrum-container', false);
      w3_show_hide('id-top-container', true);
   }
   freqset_select();
   snd_send('SET spc_='+ spec.source);
   openwebrx_resize('spec', 500);   // delay a bit until visibility stabilizes
}

function mode_over(evt, el)
{
   var mode = el.innerHTML.toLowerCase();
   //console.log('mode_over mode='+ mode);
   var s;
   
   switch (mode) {
   
      case 'sam': s = 'synchronous AM'; break;
      case 'sal': s = 'synchronous AM LSB'; break;
      case 'sau': s = 'synchronous AM USB'; break;
      case 'sas': s = 'synchronous AM stereo'; break;
      case 'qam': s = 'C-QUAM AM stereo'; break;
      
      default: s = ''; break;
   }

   el.title = evt.shiftKey? 'restore passband' : s;
}

function any_alternate_click_event(evt)
{
	return (evt && (evt.shiftKey || evt.ctrlKey || evt.altKey || evt.button == mouse.MIDDLE || evt.button == mouse.RIGHT));
}

function any_alternate_click_event_except_shift(evt)
{
	return (evt && (evt.ctrlKey || evt.altKey || evt.button == mouse.MIDDLE || evt.button == mouse.RIGHT));
}

function any_modifier_key(evt)
{
	return (evt && (evt.shiftKey || evt.ctrlKey || evt.altKey));
}

function restore_passband(mode)
{
   owrx.last_lo[mode] = kiwi_passbands(mode).lo;
   owrx.last_hi[mode] = kiwi_passbands(mode).hi;
}

var mode_buttons = [
   { s:[ 'AM', 'AMN', 'AMW' ],                  dis:0 },
   { s:[ 'SAM', 'SAL', 'SAU', 'SAS', 'QAM' ],   dis:0 },
   { s:[ 'DRM' ],                               dis:0 },
   { s:[ 'LSB', 'LSN' ],                        dis:0 },
   { s:[ 'USB', 'USN' ],                        dis:0 },
   { s:[ 'CW', 'CWN' ],                         dis:0 },
   { s:[ 'NBFM', 'NNFM' ],                      dis:0 },
   { s:[ 'IQ' ],                                dis:0 },
];

function mode_button(evt, el, dir)
{
   var mode = el.innerHTML.toLowerCase();
   if (isUndefined(dir)) dir = 1;

	// reset passband to default parameters
	var any_alt = any_alternate_click_event(evt);
	//if (evt) console.log('mode_button any_alt='+ TF(any_alt) +' shiftKey='+ TF(evt.shiftKey) +' button='+ evt.button +'|'+ mouse.MIDDLE);
	if (any_alt) {
	   if (evt.shiftKey || evt.button == mouse.MIDDLE) {
         restore_passband(mode);
         ext_set_mode(mode, null, { open_ext:true });
         return;
      }
      
      dir = -1;
	}

	// cycle button to next/prev value
	//console.log(el);
   var col = parseInt(el.id);

   if (isUndefined(owrx.last_mode_col)) {
      owrx.last_mode_col = parseInt(owrx.last_mode_el.id);
      //console.log('#### mode_button: last_mode_col UNDEF set to '+ owrx.last_mode_col);
   }
   
   if (owrx.last_mode_col != col) {
      //console.log('#### mode_button1: col '+ owrx.last_mode_col +'>'+ col +' '+ mode);

      // Prevent going between mono and stereo modes while recording
      // FIXME: do something better like disable ineligible mode buttons and show reason in mouseover tooltip 
      if (recording) {
         var c_iq_or_stereo = ext_is_IQ_or_stereo_curmode()? 1:0;
         var m_iq_or_stereo = ext_is_IQ_or_stereo_mode(mode)? 1:0;
         //console.log('#### mode_button1: c_iq_or_stereo='+ c_iq_or_stereo +' m_iq_or_stereo='+ m_iq_or_stereo +' rtn='+ (c_iq_or_stereo ^ m_iq_or_stereo));
         if (c_iq_or_stereo ^ m_iq_or_stereo || mode == 'drm')
            return;
      }

      owrx.last_mode_col = col;
   } else {
      var sa = mode_buttons[col].s;
      var mode_uc = mode.toUpperCase();
      var i = sa.indexOf(mode_uc) + dir;
      if (i <= -1) i = sa.length - 1;
      if (i >= sa.length) i = 0;
      mode_uc = sa[i];
      mode = mode_uc.toLowerCase();
      //console.log('#### mode_button2: col '+ col +' '+ mode);

      if (recording) {
         var c_iq_or_stereo = ext_is_IQ_or_stereo_curmode()? 1:0;
         var m_iq_or_stereo = ext_is_IQ_or_stereo_mode(mode)? 1:0;
         //console.log('#### mode_button2: c_iq_or_stereo='+ c_iq_or_stereo +' m_iq_or_stereo='+ m_iq_or_stereo +' rtn='+ (c_iq_or_stereo ^ m_iq_or_stereo));
         if (c_iq_or_stereo ^ m_iq_or_stereo || mode == 'drm')
		      return;
	   }

      el.innerHTML = mode_uc;
      //console.log('#### mode_button: col '+ col +' '+ mode);
   }

   ext_set_mode(mode, null, { open_ext:true });
}

var step_9_10;

function button_9_10(set)
{
   //console.log('button_9_10 set='+ set);
	if (isNumber(set))
		step_9_10 = set;
	else
		step_9_10 ^= 1;
	//console.log('button_9_10 '+ step_9_10);
	kiwi_storeWrite('last_9_10', step_9_10);
	freq_step_update_ui(true);
	var el = w3_el('id-button-9-10');
	//el.style.color = step_9_10? 'red':'blue';
	//el.style.backgroundColor = step_9_10? kiwi_rgb(255, 255*0.8, 255*0.8) : kiwi_rgb(255*0.8, 255*0.8, 255);
	el.innerHTML = step_9_10? '9':'10';
   freqset_select();
}

function pop_bottommost_panel(from)
{
	min_order=parseInt(from[0].getAttribute('data-panel-order'));
	min_index=0;
	for (var i=0;i<from.length;i++)	
	{
		actual_order=parseInt(from[i].getAttribute('data-panel-order'));
		if (actual_order<min_order) 
		{
			min_index=i;
			min_order=actual_order;
		}
	}
	to_return=from[min_index];
	from.splice(min_index,1);
	return to_return;
}

function place_panels()
{
	var left_col = [];
	var right_col = [];

	w3_iterate_children('id-panels-container', function(c) {
		if (c.classList.contains('class-panel')) {
			var position = c.getAttribute('data-panel-pos');
			//console.log('place_panels: '+ c.id +' '+ position);
			if (!position) return;
			var newSize = c.getAttribute('data-panel-size').split(",");
			if (position == "left") { left_col.push(c); }
			else if (position == "right") { right_col.push(c); }
			
			c.idName = c.id.replace('id-', '');
			c.style.width = newSize[0]+"px";
			c.style.height = newSize[1]+"px";
			c.style.margin = px(panel_margin);
			c.uiWidth = parseInt(newSize[0]);
			c.uiHeight = parseInt(newSize[1]);
			c.defaultWidth = parseInt(newSize[0]);
			c.defaultHeight = parseInt(newSize[1]);
			
			// Since W3.CSS includes the normalized use of "box-sizing: border-box"
			// style.width no longer represents the active content area.
			// So compute it ourselves for those who need it later on.
			var border_pad = html_LR_border_pad(c);
			c.activeWidth = c.uiWidth - border_pad;
			//console.log('place_panels: id='+ c.id +' uiW='+ c.uiWidth +' bp='+ border_pad + 'active='+ c.activeWidth);
			
			if (position == 'center') {
				//console.log('place_panels CENTER '+ c.id +' '+ px(window.innerHeight) +' '+ px(c.uiHeight));
				c.style.left = px(window.innerWidth/2 - c.activeWidth/2);
				c.style.bottom = px(window.innerHeight/2 - c.uiHeight/2);
				c.style.visibility = "hidden";
			}

			if (position == 'bottom-left') {
				//console.log("L/B "+ px(window.innerHeight).toString()+"px "+(c.uiHeight));
				c.style.left = 0;
				c.style.bottom = 0;
				c.style.visibility = "hidden";
			}
		}
	});
	
	y=0;
	while (left_col.length > 0) {
		p = pop_bottommost_panel(left_col);
		p.style.left = 0;
		p.style.bottom = px(y);
		p.style.visibility = "visible";
		y += p.uiHeight+3*panel_margin;
		w3_call('panel_setup_'+ p.idName, p);
	}
	
	y=0;
	while (right_col.length > 0) {
		p = pop_bottommost_panel(right_col);
		p.style.right = kiwi_isMobile()? 0 : px(kiwi_scrollbar_width());		// room for waterfall scrollbar
		p.style.bottom = px(y);
		p.style.visibility = "visible";
		y += p.uiHeight+3*panel_margin;
		w3_call('panel_setup_'+ p.idName, p);
	}
}


////////////////////////////////
// #panel setup
////////////////////////////////

var divControl;

// called indirectly by a w3_call()
function panel_setup_control(el)
{
	divControl = el;
	el.style.marginBottom = '0';

   var el2 = w3_el('id-control-inner');
   w3_add(el2, 'w3-no-copy-popup');    // so popup won't appear when touching anywhere in the panel
	el2.innerHTML =
      w3_div('id-control-top',
         w3_div('id-control-freq1'),
         w3_div('id-control-freq2'),
         w3_div('id-control-mode w3-margin-T-6'),
         w3_div('id-control-zoom w3-margin-T-6'),
         w3_div('id-optbar w3-margin-T-4')
      ) +
	   w3_div('id-optbar-content w3-margin-T-6 w3-scroll-y|height:'+ px(kiwi.OPTBAR_CONTENT_HEIGHT),
	      w3_div('id-optbar-rf w3-hide w3-scroll-y'),
	      w3_div('id-optbar-wf w3-hide w3-scroll-y'),
	      w3_div('id-optbar-audio w3-hide w3-scroll-y'),
	      w3_div('id-optbar-agc w3-hide'),
	      w3_div('id-optbar-user w3-hide w3-scroll-x'),
	      w3_div('id-optbar-stat w3-hide')
	   ) +

	   w3_div('id-control-smeter');

	// make first line of controls full width less vis button
	w3_el('id-control-freq1').style.width = px(el.activeWidth - visIcon - 6);
	
   w3_show_hide('id-mute-no', !kiwi.muted);
   w3_show_hide('id-mute-yes', kiwi.muted);
}

function panel_set_vis_button(id)
{
	var el = w3_el(id);
	var vis = w3_el(id +'-vis');
	var visOffset = el.activeWidth - visIcon;
	//console.log('left='+ visOffset +' id='+ (id +'-vis') +' '+ el.activeWidth +' '+ visIcon +' '+ visBorder);
	vis.style.left = px(visOffset + visBorder);
}

function panel_set_width_height(id, width, height)
{
	var panel = w3_el(id);

   //console.log('panel_set_width_height '+ id +' w='+ width +' '+ panel.defaultWidth +' h='+ height +' '+ panel.defaultHeight);
	if (width == undefined)
		width = panel.defaultWidth;
	panel.style.width = px(width);
	panel.uiWidth = width;

	if (height == undefined)
		height = panel.defaultHeight;
	panel.style.height = px(height);
	panel.uiHeight = height;

	var border_pad = html_LR_border_pad(panel);
	panel.activeWidth = panel.uiWidth - border_pad;
	panel_set_vis_button(id);
}

////////////////////////////////
// #misc
////////////////////////////////

function tune(fdsp, mode, zoom, zarg)
{
	ext_tune(fdsp, mode, ext_zoom.ABS, zoom, zarg);
}

// NB: use kiwi_log() instead of console.log() in here
function add_problem(what, append, sticky, el_id)
{
   var id = el_id? el_id : 'id-status-problems';
	//kiwi_log('add_problem '+ what +' sticky='+ sticky +' id='+ id);
	el = w3_el(id);
	if (!el) return;
	if (isDefined(el.children)) {
		for (var i=0; i < el.children.length; i++) {
		   if (el.children[i].innerHTML == what) {
		      return;
		   }
		}
		if (append == false) {
		   while (el.firstChild) {
		      el.removeChild(el.firstChild);
		   }
		}
	}
	var new_span = w3_create_appendElement(el, "span", what);
	if (sticky != true) {
		window.setTimeout(function(ps, ns) { try { ps.removeChild(ns); } catch(ex) {} }, 1000, el, new_span);
	}
}

function set_gen(freq, attn_ampl)
{
   //console.log('set_gen freq='+ freq +' attn_ampl='+ attn_ampl);
	snd_send("SET genattn="+ attn_ampl.toFixed(0));
	snd_send("SET gen="+ freq +" mix=-1");
}


////////////////////////////////
// #websocket
////////////////////////////////

var bin_server = 0;
var zoom_server = 0;

// Process owrx server-to-client MSGs from SND or W/F web sockets.
// Not called if MSG handled first by kiwi.js:kiwi_msg()
function owrx_msg_cb(param, ws)     // #msg-proc #MSG
{
   //console.log('$$ owrx_msg_cb '+ (ws? ws.stream : '[ws?]') +' MSG '+ param[0] +'='+ param[1]);
   
	switch (param[0]) {
		case "wf_setup":
			   wf_init();
			break;

		case "have_ext_list":
			// now that we have the list of extensions see if there is an override
			//console.log('extint_list_json override_ext='+ override_ext +' waterfall_setup_done='+ waterfall_setup_done);
         w3_do_when_cond(
            function() {
               //console.log('### '+ (waterfall_setup_done? 'GO' : 'WAIT') +' extint_open('+ override_ext +')');
               return waterfall_setup_done;
            },
            function() {
               if (override_ext) {
                  extint_open(override_ext, 1000);
               }
            }, null,
            200
         );
         // REMINDER: w3_do_when_cond() returns immediately
			break;

		case "bandwidth":
			bandwidth = parseInt(param[1]);
			break;		
		case "center_freq":
			center_freq = parseInt(param[1]);
			break;

		case "wf_fft_size":
			wf_fft_size = parseInt(param[1]);
			break;

		case "wf_fps_max":
			wf_fps_max = parseInt(param[1]);
		   //console.log('# wf_fps_max='+ wf_fps_max);
			break;

		case "wf_fps":
			wf_fps = parseInt(param[1]);
		   //console.log('# wf_fps='+ wf_fps);
			break;

		case "wf_cal":    // for benefit of kiwirecorder as well
		   wf.cal = parseInt(param[1]);
		   break;
		case "start":
			bin_server = parseInt(param[1]);
			break;

		case "zoom":
			zoom_server = parseInt(param[1]);
			break;

		case "zoom_max":
			zoom_levels_max = parseInt(param[1]);
			zoom_nom = Math.min(ZOOM_NOMINAL, zoom_levels_max);
			break;

		case "audio_init":
         toggle_or_set_audio(toggle_e.FROM_COOKIE | toggle_e.SET, 1);
			audio_init(+param[1], btn_less_buffering, kiwi_toggle(toggle_e.FROM_COOKIE | toggle_e.SET, 1, 1, 'last_compression'));
			break;

		case "audio_camp":
         var p = param[1].split(',');
			audio_camp(+p[0], +p[1], false, false);
			break;

		case "audio_rate":
			audio_rate(parseFloat(param[1]));
			break;

		case "audio_adpcm_state":
         var p = param[1].split(',');
			audio_adpcm_state(+p[0], +p[1]);
			break;

		case "audio_passband":
         var p = param[1].split(',');
			audio_recompute_LPF(1, +p[0], +p[1]);
			break;

		case "audio_flags2":
			audio_recv_flags2(param[1]);
			break;

		case "kiwi_up":
			kiwi_up(parseInt(param[1]));
			break;

		case "gps":
			toggle_or_set_spec(toggle_e.SET, spec.RF);
			break;

		case "fft_mode":
			kiwi_fft_mode();
			break;

		case "maxdb":
		   if (wf.aper != kiwi.APER_AUTO) {    // probably keyboard shortcut switched aperture mode
		      //console.log($SKIP MSG maxdb wf.aper='+ wf.aper);
		      break;
		   }
		   wf.auto_maxdb = +param[1];
		   //console.log('$SET wf.auto_maxdb='+ wf.auto_maxdb);
         setmaxdb(wf.auto_maxdb, {done:1, no_fsel:1});
			break;

		case "mindb":
		   if (wf.aper != kiwi.APER_AUTO) {    // probably keyboard shortcut switched aperture mode
		      //console.log($SKIP MSG mindb wf.aper='+ wf.aper);
		      break;
		   }
		   wf.auto_mindb = +param[1];
		   //console.log('$SET wf.auto_mindb='+ wf.auto_mindb);
         setmindb(wf.auto_mindb, {done:1, no_fsel:1});
         w3_call('waterfall_maxmin_cb');
         set_ceilfloordb({done:1, no_fsel:1});
			break;

		case "max_thr":
			owrx.overload_mute = Math.round(+param[1]);
			break;

		case "last_community_download":
		   dx.last_community_download = decodeURIComponent(param[1]);
		   break;
		   
		case "rf_attn":
		   //console.log('UPD rf_attn='+ param[1]);
         rf_attn_cb(null, +param[1], false, false, /* ui_only */ true);
		   break;
		   
		default:
		   return false;
	}

	return true;
}

function owrx_ws_open_snd(cb, cbp)
{
	ws_snd = open_websocket('SND', cb, cbp, owrx_msg_cb, audio_recv, on_ws_error, owrx_close_cb);
	return ws_snd;
}

function owrx_ws_open_wf(cb, cbp)
{
	ws_wf = open_websocket('W/F', cb, cbp, owrx_msg_cb, waterfall_add_queue, on_ws_error);
	return ws_wf;
}

function owrx_close_cb()
{
   toggle_or_set_rec(0);
   
   // Stopping fax recording is difficult because a couple of transactions on the SND stream
   // to the server are required to run the png converter program. So chicken-and-egg problem.
   //w3_call('fax_file_cb', 'close');
}

function on_ws_error()
{
	console.log("WebSocket error");
}

function snd_send(s)
{
	try {
		//console.log('WS SND <'+ s +'>');
		ws_snd.send(s);
		return 0;
	} catch(ex) {
		console.log("CATCH snd_send('"+s+"') ex="+ex);
		kiwi_trace();
		return -1;
	}
}

function wf_send(s)
{
	try {
		//console.log('WS W/F <'+ s +'>');
		ws_wf.send(s);
		return 0;
	} catch(ex) {
		console.log("CATCH wf_send('"+s+"') ex="+ex);
		kiwi_trace();
		return -1;
	}
}

function ws_any()
{
   if (ws_snd) return ws_snd;
   if (ws_wf) return ws_wf;
   return null;
}

var need_geo = true;
var need_status = true;

function send_keepalive()
{
	for (var i=0; i<1; i++) {
		if (!ws_snd.up || snd_send("SET keepalive") < 0)
			break;
	
		// these are done here because we know the audio connection is up and can receive messages
		if (need_geo && kiwi_geo() != "") {
			if (msg_send("SET geoloc="+kiwi_geo()) < 0)
				break;
			if (msg_send("SET geojson="+kiwi_geojson()) < 0)
				break;
			need_geo = false;
		}
		
		if (send_ident) {
			//console.log('send_ident: SET ident_user='+ ident_user);
			if (!ident_user) ident_user = '';
			if (snd_send("SET ident_user="+ encodeURIComponent(ident_user)) < 0)
				break;
         snd_send('SET require_id=0');
         //canvas_log('send_ident');
			send_ident = false;
		}
	
		if (need_status) {
			if (snd_send("SET need_status=1") < 0)
				break;
			need_status = false;
		}
	}

	if (!ws_wf.up || wf_send("SET keepalive") < 0)
		return;
}
