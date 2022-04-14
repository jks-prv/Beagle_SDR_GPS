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

// Copyright (c) 2015 - 2021 John Seamons, ZL/KF6VO

var owrx = {
   height_top_bar_parts: 67,
   hide_bars: 0,
   HIDE_TOPBAR: 1,
   HIDE_BANDBAR: 2,
   HIDE_ALLBARS: 3,
   
   cfg_loaded: false,
   mobile: null,
   wf_snap: 0,
   wf_cursor: 'crosshair',
   
   last_freq: -1,
   last_mode: '',
   last_locut: -1,
   last_hicut: -1,
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
   tuning_locked: 0,
   
   dx_click_gid_last: undefined,
   
   sam_pll: 1,
   sam_pll_s: [ 'DX', 'med', 'fast' ],
   
   chan_null: 0,
   chan_null_s: [ 'normal', 'null LSB', 'null USB' ],
   
   ovld_mute: 0,
   ovld_mute_s: [ 'off', 'on' ],
   
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
   
   __last: 0
};

// key freq concepts:
//		all freqs in Hz
//		center_freq is center of entire sampled bandwidth (e.g. 15 MHz)
//		offset_frequency (-bandwidth/2 .. bandwidth/2) = center_freq - freq
//		freq = center_freq + offset_frequency

// constants, passed from server
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
var wf_compression = 1;
var wf_interp = 13;  // WF_DROP + WF_CIC_COMP by default
var wf_winf = 2;     // WIN_BLACKMAN_HARRIS
var debug_v = 0;		// a general value settable from the URI to be used during debugging
var sb_trace = 0;
var kiwi_gc = 1;
var kiwi_gc_snd = 0;
var kiwi_gc_wf = -1;
var kiwi_gc_recv = -1;
var kiwi_gc_wspr = -1;
var override_ext = null;
var muted_initially = 0;
var peak_initially = null;
var param_nocache = false;
var nocache = false;
var param_ctrace = false;
var ctrace = false;
var no_clk_corr = false;
var override_pbw = '';
var override_pbc = '';
var nb_click = false;
var no_geoloc = false;
var mobile_laptop_test = false;
var user_url = null;

var freq_memory = [];
var freq_memory_pointer = -1;

var wf_rates = { '0':0, 'off':0, '1':1, '1hz':1, 's':2, 'slow':2, 'm':3, 'med':3, 'f':4, 'fast':4 };

var okay_wf_init = false;

function kiwi_main()
{
	override_freq = parseFloat(readCookie('last_freq'));
	override_mode = readCookie('last_mode');
	override_zoom = parseFloat(readCookie('last_zoom'));
	override_9_10 = parseFloat(readCookie('last_9_10'));
	override_max_dB = parseFloat(readCookie('last_max_dB'));
	override_min_dB = parseFloat(readCookie('last_min_dB'));
	
	var last_vol = readCookie('last_volume', 50);
   //console.log('last_vol='+ last_vol);
	setvolume(true, last_vol);
	
	var f_cookie = readCookie('freq_memory');
	if (f_cookie) {
      var obj = kiwi_JSON_parse('kiwi_main', f_cookie);
      if (obj) {
         freq_memory = obj;
         freq_memory_pointer = freq_memory.length-1;
      }
   }
	
	console.log('LAST f='+ override_freq +' m='+ override_mode +' z='+ override_zoom
		+' 9_10='+ override_9_10 +' min='+ override_min_dB +' max='+ override_max_dB);

	// process URL query string parameters
	var num_win = 16;     // FIXME: should really be rx_chans, but that's not available yet 
	console.log('URL: '+ window.location.href);
	
	var qs_parse = function(s) {
		var qd = {};
		if (s) s.split("&").forEach(function(item) {
			var a = item.split("=");
			qd[a[0]] = a[1]? a[1] : 1;		// &foo& shorthand for &foo=1&
		});
		return qd;
	}
	
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

	var host = window.location.href.split('?')[0];
	g_qs_idx = 2;     // NB: global var
	for (w = 2; w <= num_win; w++) {
      if (qs[w]) {
         setTimeout(function() {
               var url = host +'?'+ qs[g_qs_idx];
               g_qs_idx++;
               //console.log('OPEN '+ url);
               var win = window.open(url, '_blank');
               //console.log(win);
               if (win == null)
                  alert('Do you have popups blocked for this site? '+ url);
            }, (w-1) * 2000
         );
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

	var q = qd[1];
	//console.log('this window/tab:');
	//console.log(q);
	
	var s = 'f'; if (q[s]) {
		var p = new RegExp('([0-9.,kM]*)([\/:][-0-9.,k]*)?([^z]*)?z?([0-9]*)').exec(q[s]);
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
	s = 'sp'; if (q[s]) spectrum_show = q[s];
	s = 'spec'; if (q[s]) spectrum_show = q[s];
	s = 'spp'; if (q[s]) spectrum_param = parseFloat(q[s]);
	s = 'vol'; if (q[s]) { setvolume(true, parseInt(q[s])); }
	s = 'mute'; if (q[s]) muted_initially = parseInt(q[s]);
	s = 'wf'; if (q[s]) wf_rate = q[s];
	s = 'wfm'; if (q[s]) wf_mm = q[s];
	s = 'cmap'; if (q[s]) wf.cmap_override = w3_clamp(parseInt(q[s]), 0, kiwi.cmap_s.length - 1, 0);
	s = 'sqrt'; if (q[s]) wf.sqrt = w3_clamp(parseInt(q[s]), 0, 4, 0);
	s = 'wfts'; if (q[s]) wf.url_tstamp = w3_clamp(parseInt(q[s]), 2, 60*60, 2);
	s = 'peak'; if (q[s]) peak_initially = parseInt(q[s]);
	s = 'no_geo'; if (q[s]) no_geoloc = true;
	s = 'keys'; if (q[s]) shortcut.keys = q[s];
	s = 'user'; if (q[s]) user_url = q[s];
	s = 'u'; if (q[s]) user_url = q[s];
	// 'no_wf' is handled in kiwi_util.js

   // development
	s = 'sqth'; if (q[s]) squelch_threshold = parseFloat(q[s]);
	s = 'click'; if (q[s]) nb_click = true;
	s = 'nocache'; if (q[s]) { param_nocache = true; nocache = parseInt(q[s]); }
	s = 'ncc'; if (q[s]) no_clk_corr = parseInt(q[s]);
	s = 'wfdly'; if (q[s]) waterfall_delay = parseFloat(q[s]);
	s = 'wf_comp'; if (q[s]) wf_compression = parseInt(q[s]);
	s = 'wfi'; if (q[s]) wf_interp = parseInt(q[s]);
	s = 'wfw'; if (q[s]) wf_winf = parseInt(q[s]);
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
	s = 'mobile'; if (q[s]) mobile_laptop_test = true;
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

	kiwi_xdLocalStorage_init();
	kiwi_get_init_settings();
	if (!no_geoloc) kiwi_geolocate();

	init_top_bar();
	init_rx_photo();
	right_click_menu_init();
	keyboard_shortcut_init();
	confirmation_panel_init();
	ext_panel_init();
	place_panels();
	init_panels();
   okay_wf_init = true;
	confirmation_panel_init2();
	smeter_init();
	time_display_setup('id-topbar-right-container');
	
	window.setInterval(send_keepalive, 5000);
	window.addEventListener("resize", openwebrx_resize);
        
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
	snd_send("SET dbug_v="+ debug_v);

   w3_do_when_cond(function() { return (isArg(rx_chan) && owrx.cfg_loaded); }, function() {
      if (rx_chan != 0) return;

      if (gen_attn != 0) {
         var dB = gen_attn + cfg.waterfall_cal;
         var ampl_gain = Math.pow(10, -dB/20);		// use the amplitude form since we are multipling a signal
         gen_attn = 0x01ffff * ampl_gain;
         //console.log('### GEN dB='+ dB +' ampl_gain='+ ampl_gain +' attn='+ gen_attn +' / '+ gen_attn.toHex() +' offset='+ cfg.waterfall_cal);
      }
   
      // always setup gen so it will get disables (attn=0) if an rx0 page reloads using a URL where no gen is
      // specified, but it was previously enabled by the URL (i.e. so the gen doesn't continue to run).
      set_gen(gen_freq, gen_attn);
   }, 0, 500);

	snd_send("SET mod=am low_cut=-4000 high_cut=4000 freq=1000");
	set_agc();
	snd_send("SET browser="+navigator.userAgent);
	
	wf_send("SERVER DE CLIENT openwebrx.js W/F");
	wf_send("SET send_dB=1");
	// fixme: okay to remove this now?
	wf_send("SET zoom=0 start=0");
	wf_send("SET maxdb=0 mindb=-100");

	if (wf_compression == 0) wf_send('SET wf_comp=0');
	if (wf_interp != 0) wf_send('SET interp='+ wf_interp);
	if (wf_winf != 0) wf_send('SET window_func='+ wf_winf);
	wf_speed = wf_rates[wf_rate];
	//console.log('wf_rate="'+ wf_rate +'" wf_speed='+ wf_speed);
	if (wf_speed == undefined) wf_speed = WF_SPEED_FAST;
	wf_send('SET wf_speed='+ wf_speed);
}


////////////////////////////////
// panel support
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

	var readme_firsttime = updateCookie('readme', 'seen2');
	if (kiwi_isMobile()) readme_firsttime = false;     // don't show readme panel at all on mobile devices
	init_panel_toggle(ptype.TOGGLE, 'id-readme', false, readme_firsttime? popt.PERSIST : popt.CLOSE, readme_color);

	//init_panel_toggle(ptype.TOGGLE, 'id-msgs', true, kiwi_isMobile()? popt.CLOSE : popt.PERSIST);
	//init_panel_toggle(ptype.POPUP, 'id-msgs', true, popt.CLOSE);

	var news_firsttime = (readCookie('news', 'seen') == null);
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
			   '<img id='+ dq(panel +'-close-img') +' src="icons/close.24.png" width="24" height="24" />' +
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
		updateCookie(panel, 'seen');
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
		from = hideWidth; to = kiwi_scrollbar_width();
	}
	
	animate(divPanel, rightSide? 'right':'left', "px", from, to, 0.93, kiwi_isMobile()? 1:1000, 60, 0);
	
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
	freqset_select();
}

function openwebrx_resize(a)
{
   a = (isString(a) && a.startsWith('orient'))? a : 'event';
	resize_wf_canvases();
	resize_waterfall_container(true);
   extint_environment_changed( { resize:1, passband_screen_location:1 } );
	resize_scale(a);
	check_top_bar_congestion();
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

var offsetDiff_init = false;
var p_left, p_owner, p_mid, p_right;

function offsetDiff(name, color, lrw, p_lrw)
{
	var el = w3_el('id-tbar-'+ name.toLowerCase() +'-bbox');
	el.style.left = px(lrw.x1);
	el.style.width = px(lrw.w);
	el.style.height = px(lrw.h);
	el.style.backgroundColor = color;

	console.log(name +' L/R/W='+ lrw.x1 +'/'+ lrw.x2 +'/'+ lrw.w +' ('+
	   (p_lrw.x1-lrw.x1) +'/'+ (p_lrw.x2-lrw.x2) +'/'+ (p_lrw.w-lrw.w) +')');
}

function check_top_bar_congestion()
{
	var left = w3_boundingBox_children('id-left-info-container');
	var owner = w3_boundingBox_children('id-mid-owner-container');
	var mid = w3_boundingBox_children('id-mid-info-container');
	var right = w3_boundingBox_children('id-topbar-right-container');
	
	// position owner info in the middle of the gap between left and mid bbox
	var owner_left = left.x2 + (mid.x1 - left.x2)/2 - owner.w/2;
	//console.log('owner_left='+ owner_left);
	w3_el('id-owner-info').style.left = px(owner_left);
	owner = w3_boundingBox_children('id-mid-owner-container');     // recompute after change
	
	// Selectively hide all but the left bbox.
	// Can't use w3_show_hide() because otherwise bbox values would be zero when hidden
	// and bbox would never reappear.
	w3_visible('id-mid-owner-container', owner.x1 >= left.x2);
	w3_visible('id-mid-info-container', mid.x1 >= left.x2);
	w3_visible('id-topbar-right-container', right.x1 >= left.x2);
	
	// place photo open arrow right after left bbox
	w3_el('id-rx-details-arrow').style.left = px(left.x2);

   /*
      if (!offsetDiff_init) {
         offsetDiff_init = true;
         p_left = left;
         p_owner = owner;
         p_mid = mid;
         p_right = right;
         w3_el('id-top-bar').innerHTML +=
            '<div class="id-tbar-left-bbox cl-tbar-bbox"></div>' +
            '<div class="id-tbar-owner-bbox cl-tbar-bbox"></div>' +
            '<div class="id-tbar-mid-bbox cl-tbar-bbox"></div>' +
            '<div class="id-tbar-right-bbox cl-tbar-bbox"></div>';
      }
      
      console.log('----');
      offsetDiff('LEFT', 'yellow', left, p_left);
      p_left = left;
      offsetDiff('OWNER', 'cyan', owner, p_owner);
      p_owner = owner;
      offsetDiff('MID', 'green', mid, p_mid);
      p_mid = mid;
      offsetDiff('RIGHT', 'red', right, p_right);
      p_right = right;
   */
}

function init_top_bar()
{
   w3_append_innerHTML('id-rx-desc',
      w3_link('id-rx-gmap', '', '[map]', '', 'dont_toggle_rx_photo') +
      w3_div('id-rx-snr w3-show-inline')
   );
}

var rx_photo_spacer_height = owrx.height_top_bar_parts;

function init_rx_photo()
{
   w3_el('id-top-photo-img').style.paddingLeft = RX_PHOTO_LEFT_MARGIN? '50px' : 0;
	RX_PHOTO_HEIGHT += rx_photo_spacer_height;
	w3_el("id-top-photo-clip").style.maxHeight = px(RX_PHOTO_HEIGHT);
	//window.setTimeout(function() { animate(html("id-rx-photo-title"),"opacity","",1,0,1,500,30); },1000);
	//window.setTimeout(function() { animate(html("id-rx-photo-desc"),"opacity","",1,0,1,500,30); },1500);
	if (dbgUs || kiwi_isMobile()) {
		close_rx_photo();
	} else {
		window.setTimeout(function() { close_rx_photo() },3000);
	}
}

var rx_photo_state=1;

function open_rx_photo()
{
	rx_photo_state=1;
	html("id-rx-photo-desc").style.opacity=1;
	html("id-rx-photo-title").style.opacity=1;
	animate_to(html("id-top-photo-clip"),"maxHeight","px",RX_PHOTO_HEIGHT,0.93,kiwi_isMobile()? 1:1000,60,function(){resize_waterfall_container(true);});
	w3_hide('id-rx-details-arrow-down');
	w3_show_block('id-rx-details-arrow-up');
}

function close_rx_photo()
{
	rx_photo_state=0;
	animate_to(html("id-top-photo-clip"),"maxHeight","px",rx_photo_spacer_height,0.93,kiwi_isMobile()? 1:1000,60,function(){resize_waterfall_container(true);});
	w3_show_block('id-rx-details-arrow-down');
	w3_hide('id-rx-details-arrow-up');
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


function animate(object, style_name, unit, from, to, accel, time_ms, fps, to_exec)
{
	//console.log(object.className);
	if (isUndefined(to_exec)) to_exec=0;
	object.style[style_name]=from.toString()+unit;
	object.anim_i=0;
	n_of_iters=time_ms/(1000/fps);
	if (n_of_iters < 1) n_of_iters = 1;
	change=(to-from)/(n_of_iters);
	if (isDefined(object.anim_timer)) { window.clearInterval(object.anim_timer);  }

	object.anim_timer = window.setInterval(
		function(){
			if (object.anim_i++<n_of_iters)
			{
				if (accel==1) object.style[style_name] = (parseFloat(object.style[style_name]) + change).toString() + unit;
				else 
				{ 
					remain=parseFloat(object.style[style_name])-to;
					if (Math.abs(remain)>9||unit!="px") new_val=(to+accel*remain);
					else {if (Math.abs(remain)<2) new_val=to;
					else new_val=to+remain-(remain/Math.abs(remain));}
					object.style[style_name]=new_val.toString()+unit;
				}
			}
			else 
				{object.style[style_name]=to.toString()+unit; window.clearInterval(object.anim_timer); delete object.anim_timer;}
			if (to_exec!=0) to_exec();
		},1000/fps);
}

function animate_to(object, style_name, unit, to, accel, time_ms, fps, to_exec)
{
	from = parseFloat(style_value(object, style_name));
	//console.log("FROM "+style_name+'='+from);
	animate(object, style_name, unit, from, to, accel, time_ms, fps, to_exec);
}

function style_value(of_what, which)
{
	if (of_what.currentStyle)
		return of_what.currentStyle[which];
	else
	if (window.getComputedStyle)
		return document.defaultView.getComputedStyle(of_what,null)[which];
	else
		return of_what.style[which];
}

/*function fade(something,from,to,time_ms,fps)
{
	something.style.opacity=from;
	something.fade_i=0;
	n_of_iters=time_ms/(1000/fps);
	change=(to-from)/(n_of_iters-1);
	
	something.fade_timer=window.setInterval(
		function(){
			if (something.fade_i++<n_of_iters)
				something.style.opacity=parseFloat(something.style.opacity)+change;
			else 
				{something.style.opacity=to; window.clearInterval(something.fade_timer); }
		},1000/fps);
}*/


////////////////////////////////
// demodulator
////////////////////////////////

demodulators = []

demodulator_color_index = 0;
demodulator_colors = ["#ffff00", "#00ff00", "#00ffff", "#058cff", "#ff9600", "#a1ff39", "#ff4e39", "#ff5dbd"]

function demodulators_get_next_color()
{
	if (demodulator_color_index >= demodulator_colors.length) demodulator_color_index = 0;
	return (demodulator_colors[demodulator_color_index++]);
}

function demod_envelope_draw(range, from, to, color, line)
{  //                                               ____
	// Draws a standard filter envelope like this: _/    \_
   // Parameters are given in offset frequency (Hz).
   // Envelope is drawn on the scale canvas.
	// A "drag range" object is returned, containing information about the draggable areas of the envelope
	// (beginning, ending and the line showing the offset frequency).
	if (isUndefined(color)) color = 'lime';
	
	var env_bounding_line_w = 5;   //    
	var env_att_w = 5;             //     _______   ___env_h2 in px   ___|_____
	var env_h1 = 17;               //   _/|      \_ ___env_h1 in px _/   |_    \_
	var env_h2 = 5;                //   |||env_att_w                     |_env_lineplus
	var env_lineplus = 1;          //   ||env_bounding_line_w
	var env_line_click_area = 8;
	var env_slop = 5;
	
	//range = get_visible_freq_range();
	var from_px = scale_px_from_freq(from, range);
	var to_px = scale_px_from_freq(to, range);
	if (to_px < from_px) /* swap'em */ { var temp_px = to_px; to_px = from_px; from_px = temp_px; }
	
	pb_adj_cf.style.left = px(from_px);
	pb_adj_cf.style.width = px(to_px - from_px);

	pb_adj_lo.style.left = px(from_px - env_bounding_line_w - 2*env_slop);
	pb_adj_lo.style.width = px(env_bounding_line_w + env_att_w + 2*env_slop);

	pb_adj_hi.style.left = px(to_px - env_bounding_line_w);
	pb_adj_hi.style.width = px(env_bounding_line_w + env_att_w + 2*env_slop);
	
	/* from_px -= env_bounding_line_w/2;
	to_px += env_bounding_line_w/2; */
	from_px -= (env_att_w + env_bounding_line_w);
	to_px += (env_att_w + env_bounding_line_w);
	
	// do drawing:
	scale_ctx.lineWidth = 3;
	scale_ctx.strokeStyle = color;
	scale_ctx.fillStyle = color;
	var drag_ranges = { envelope_on_screen: false, line_on_screen: false, allow_pb_adj: false };
	
	if (!(to_px < 0 || from_px > window.innerWidth)) {    // out of screen?
		drag_ranges.beginning = {x1: from_px, x2: from_px + env_bounding_line_w + env_att_w + env_slop};
		drag_ranges.ending = {x1: to_px - env_bounding_line_w - env_att_w - env_slop, x2: to_px};
		drag_ranges.whole_envelope = {x1: from_px, x2: to_px};
		drag_ranges.envelope_on_screen = true;
		drag_ranges.allow_pb_adj = ((to_px - from_px) >= (dbgUs? 100:30));
		
		if (!drag_ranges.allow_pb_adj)
		   scale_ctx.strokeStyle = scale_ctx.fillStyle = 'yellow';
		
      scale_ctx.beginPath();
      scale_ctx.moveTo(from_px, env_h1);
      scale_ctx.lineTo(from_px + env_bounding_line_w, env_h1);
      scale_ctx.lineTo(from_px + env_bounding_line_w + env_att_w, env_h2);
      scale_ctx.lineTo(to_px - env_bounding_line_w - env_att_w, env_h2);
      scale_ctx.lineTo(to_px - env_bounding_line_w, env_h1);
      scale_ctx.lineTo(to_px, env_h1);
      scale_ctx.globalAlpha = 0.3;
      scale_ctx.fill();
      scale_ctx.globalAlpha = 1;
      scale_ctx.stroke();
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
	in_range = function(x, g_range) { return g_range.x1 <= x && g_range.x2 >= x && y < scale_canvas_h/2; }
	dr = demodulator.draggable_ranges;
	//console.log('demod_envelope_where_clicked x='+ x +' y='+ y +' in_pb='+ ((y < scale_canvas_h/2)? 1:0) +' allow_pb_adj='+ drag_ranges.allow_pb_adj);
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
		//else { console.log('drag pb hi/lo ignored'); canvas_log('ign pb_adj'); }

		// Last priority: having clicked anything else on the envelope, without holding the shift key
		if (in_range(x, drag_ranges.whole_envelope)) return dr.anything_else; 
	}

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
	this.color = demodulators_get_next_color();
	this.stop = function(){};
}

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
} // to which parameter these correspond in demod_envelope_draw()

//******* class demodulator_default_analog *******
// This can be used as a base for basic audio demodulators.
// It already supports most basic modulations used for ham radio and commercial services: AM/FM/LSB/USB

demodulator_response_time = 100; 
//in ms; if we don't limit the number of SETs sent to the server, audio will underrun (possibly output buffer is cleared on SETs in GNU Radio

/*
set in configuration now (cfg.passbands)
see also cfg.cpp::_cfg_load_json()

var passbands = {
	am:		{ lo: -4900,	hi:  4900 },            // 9.8 kHz instead of 10 to avoid adjacent channel heterodynes in SW BCBs
	amn:		{ lo: -2500,	hi:  2500 },
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
	iq:		{ lo: -5000,	hi:  5000 },
};
*/

function demodulator_default_analog(offset_frequency, subtype, locut, hicut)
{
   if (cfg.passbands[subtype] == null) subtype = 'am';
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

//if (!isNaN(locut)) { console.log('#### demodulator_default_analog locut='+ locut); kiwi_trace(); }
//if (!isNaN(hicut)) { console.log('#### demodulator_default_analog hicut='+ hicut); kiwi_trace(); }
	var lo = isNaN(locut)? cfg.passbands[subtype].last_lo : locut;
	var hi = isNaN(hicut)? cfg.passbands[subtype].last_hi : hicut;
	if (lo == 'undefined' || lo == null) {
		lo = cfg.passbands[subtype].last_lo = cfg.passbands[subtype].lo;
	}
	if (hi == 'undefined' || hi == null) {
		hi = cfg.passbands[subtype].last_hi = cfg.passbands[subtype].hi;
	}
	
	if (override_pbw != '') {
	   var center = lo + (hi-lo)/2;
	   var min = this.filter.min_passband;
	   //console.log('### override_pbw cur_lo='+ lo +' cur_hi='+ hi +' cur_center='+ center +' min='+ min);
	   override_pbw = decodeURIComponent(override_pbw);
	   var p = override_pbw.split(',');
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
	   var min = this.filter.min_passband;
	   //console.log('### override_pbc cur_lo='+ lo +' cur_hi='+ hi +' cpbc='+ cpbc +' cpbhw='+ cpbhw +' min='+ min);
	   override_pbc = decodeURIComponent(override_pbc);
	   var p = override_pbc.split(',');
	   var pbc = p[0].parseFloatWithUnits('k');
      console.log('pbc="'+ p[0] +'" '+ nlo);
	   var pbw = NaN;
	   if (p.length > 1) {
	      pbw = p[1].parseFloatWithUnits('k');
         console.log('pbw="'+ p[1] +'" '+ nlo);
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
	   case 'sam':
	   case 'sal':
	   case 'sau':
	   case 'sas':
	   case 'qam':
	   case 'drm':
	   case 'nbfm':
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
	}

	// this function sends demodulator parameters to the server
	this.doset = function() {
		//console.log('DOSET fcar='+freq_car_Hz);
		//if (dbgUs && dbgUsFirst) { dbgUsFirst = false; console.trace(); }
		
		var freq = (freq_car_Hz/1000).toFixed(3);
		var mode = this.server_mode;
		var locut = this.low_cut.toString();
		var hicut = this.high_cut.toString();
		var mparam = (mode == 'sam')? (' param='+ owrx.chan_null) : '';
		//console.log('$mode '+ mode);
		var s = 'SET mod='+ mode +' low_cut='+ locut +' high_cut='+ hicut +' freq='+ freq + mparam;
		snd_send(s);
		//console.log('$'+ s);

      var changed = null;
      if (freq != owrx.last_freq) {
         changed = changed || {};
         changed.freq = 1;
         owrx.last_freq = freq;
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
		   toggle_or_set_mute(muted_initially);
		   muted_until_freq_set = false;
		}
		
		if (audio_meas_dly_ena) {
		   //console.log('audio_meas_dly_start');
		   audio_meas_dly_start = (new Date()).getTime();
		}
	}
	// this.set(); //we set parameters on object creation

	//******* envelope object *******
   // for drawing the filter envelope above scale
	this.envelope.parent = this;

	this.envelope.draw = function(visible_range) 
	{
		this.visible_range = visible_range;
		this.drag_ranges = demod_envelope_draw(g_range,
				center_freq + this.parent.offset_frequency + this.parent.low_cut,
				center_freq + this.parent.offset_frequency + this.parent.high_cut,
				this.color,
				center_freq + this.parent.offset_frequency);
		var bw = Math.abs(this.parent.high_cut - this.parent.low_cut);
		pb_adj_lo_ttip.innerHTML = 'lo '+ this.parent.low_cut.toString() +', bw '+ bw.toString();
		pb_adj_hi_ttip.innerHTML = 'hi '+ this.parent.high_cut.toString() +', bw '+ bw.toString();
		pb_adj_cf_ttip.innerHTML = 'cf '+ (this.parent.low_cut + Math.abs(this.parent.high_cut - this.parent.low_cut)/2).toString();
		pb_adj_car_ttip.innerHTML = ((center_freq + this.parent.offset_frequency)/1000 + kiwi.freq_offset_kHz).toFixed(2) +' kHz';
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
		var dr = demodulator.draggable_ranges;
		if (this.dragged_range == dr.none) {
			//console.log('move none');
			return false; // we return if user is not dragging (us) at all
		}

		freq_change = Math.round(this.visible_range.hpp * (x-this.drag_origin.x));
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
				return true;
			}
			//nor the filter passband be too small
			if (new_hi - new_lo < this.parent.filter.min_passband) {
				//console.log('lo min');
				return true;
			}
			//sanity check to prevent GNU Radio "firdes check failed: fa <= fb"
			if (new_lo >= new_hi) {
				//console.log('lo wrap');
				return true;
			}
		}
		
		if (do_hi) {
			//we don't let high_cut go beyond its limits
			if (new_hi > this.parent.filter.high_cut_limit) {
				//console.log('hi limit');
				return true;
			}
			//nor the filter passband be too small
			if (new_hi - new_lo < this.parent.filter.min_passband) {
				//console.log('hi min');
				return true;
			}
			//sanity check to prevent GNU Radio "firdes check failed: fa <= fb"
			if (new_hi <= new_lo) {
				//console.log('hi wrap');
				return true;
			}
		}
		
		// make the proposed changes
		if (do_lo) {
			cfg.passbands[this.parent.server_mode].last_lo = this.parent.low_cut = new_lo;
			//console.log('DRAG-MOVE lo=', new_lo.toFixed(0));
		}
		
		if (do_hi) {
			cfg.passbands[this.parent.server_mode].last_hi = this.parent.high_cut = new_hi;
			//console.log('DRAG-MOVE hi=', new_hi.toFixed(0));
		}
		
		if (this.dragged_range == dr.anything_else || is_adj_BFO) {
			//when any other part of the envelope is dragged, the offset frequency is changed (whole passband also moves with it)
			new_value = this.drag_origin.offset_frequency + freq_change;
			if (new_value > bandwidth/2 || new_value < -bandwidth/2) {
				//console.log('bfo range');
				return true; //we don't allow tuning above Nyquist frequency :-)
			}
			this.parent.offset_frequency = new_value;
			//console.log('DRAG-MOVE off=', new_value.toFixed(0));
		}
		
		//now do the actual modifications:
		mkenvelopes(this.visible_range);
		freqset_car_Hz(this.parent.offset_frequency + center_freq);
		this.parent.set();
		freqset_update_ui();

		//will have to change this when changing to multi-demodulator mode:
		//html("id-control-freq1").innerHTML=format_frequency("{x} MHz",center_freq+this.parent.offset_frequency,1e6,4);
		return true;
	};
	
	this.envelope.drag_end=function(x)
	{ //in this demodulator we've already changed values in the drag_move() function so we shouldn't do too much here.
		to_return = this.dragged_range != demodulator.draggable_ranges.none; //this part is required for cliking anywhere on the scale to set offset
		this.dragged_range = demodulator.draggable_ranges.none;
		return to_return;
	};
	
}

demodulator_default_analog.prototype = new demodulator();

function mkenvelopes(visible_range) //called from mkscale
{
	scale_ctx.clearRect(0,0,scale_ctx.canvas.width,22); //clear the upper part of the canvas (where filter envelopes reside)
	for (var i=0;i<demodulators.length;i++)
	{
		demodulators[i].envelope.draw(visible_range);
	}
}

function demodulator_remove(which)
{
	demodulators[which].stop();
	demodulators.splice(which,1);
}

function demodulator_add(what)
{
	demodulators.push(what);
	if (waterfall_setup_done) mkenvelopes(get_visible_freq_range());
}

function demodulator_analog_replace(subtype, freq)
{ //this function should only exist until the multi-demodulator capability is added
   //console.log('demodulator_analog_replace subtype='+ subtype);
   if (cfg.passbands[subtype] == null) subtype = 'am';

	var offset = 0, prev_pbo = 0, low_cut = NaN, high_cut = NaN;
	var wasCW = false, toCW = false, fromCW = false;

	if (demodulators.length) {
		wasCW = demodulators[0].isCW;
		offset = demodulators[0].offset_frequency;
		prev_pbo = passband_offset();
		demodulator_remove(0);
	} else {
		var i_freqHz = Math.round((init_frequency - kiwi.freq_offset_kHz) * 1000);
      offset = (i_freqHz <= 0 || i_freqHz > bandwidth)? 0 : (i_freqHz - center_freq);
		//console.log('### init_freq='+ init_frequency +' freq_offset_kHz='+ kiwi.freq_offset_kHz +' i_freqHz='+ i_freqHz +' offset='+ offset +' init_mode='+ init_mode);
		subtype = init_mode;
	}
	
	// initial offset, but doesn't consider demod.isCW since it isn't valid yet
	if (isArg(freq)) {
		offset = freq - center_freq;
	}
	
	//console.log("DEMOD-replace calling add: INITIAL offset="+(offset+center_freq));
	demodulator_add(new demodulator_default_analog(offset, subtype, low_cut, high_cut));
	
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
		if (toCW || fromCW)
		   wf.audioFFT_clear_wf = true;
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
	demodulator_set_offset_frequency(0, offset);
	
	try_modeset_update_ui(subtype);
}

function demodulator_set_offset_frequency(which, offset)
{
	if (offset > bandwidth/2 || offset < -bandwidth/2) return;
	offset = Math.round(offset);
	//console.log("demodulator_set_offset_frequency: offset="+(offset + center_freq));
	
	// set carrier freq before demodulators[0].set() below
	// requires demodulators[0].isCW to be valid
	freqset_car_Hz(offset + center_freq);
	
	var demod = demodulators[0];
	demod.offset_frequency = offset;
	demod.set();
	try_freqset_update_ui();
}

function owrx_cfg()
{
   w3_do_when_rendered('id-rx-gmap',
      function() {
         w3_link_set('id-rx-gmap', 'https://www.google.com/maps/place/'+ cfg.index_html_params.RX_GMAP);
      }
   );
   
   owrx.cfg_loaded = true;
}


////////////////////////////////
// scale
////////////////////////////////

var scale_ctx, band_ctx, dx_ctx;
var pb_adj_cf, pb_adj_cf_ttip, pb_adj_lo, pb_adj_lo_ttip, pb_adj_hi, pb_adj_hi_ttip, pb_adj_car, pb_adj_car_ttip;
var scale_canvas, band_canvas, dx_div, dx_canvas;

function scale_setup()
{
	w3_el('id-scale-container').addEventListener("mouseout", scale_container_mouseout, false);
   
	scale_canvas = html("id-scale-canvas");
	scale_ctx = scale_canvas.getContext("2d");
	add_scale_listner(scale_canvas);

	pb_adj_car = html("id-pb-adj-car");
	pb_adj_car.innerHTML = '<span id="id-pb-adj-car-ttip" class="class-passband-adjust-car-tooltip class-tooltip-text"></span>';
	pb_adj_car_ttip = html("id-pb-adj-car-ttip");
	add_scale_listner(pb_adj_car);

	pb_adj_lo = html("id-pb-adj-lo");
	pb_adj_lo.innerHTML = '<span id="id-pb-adj-lo-ttip" class="class-passband-adjust-cut-tooltip class-tooltip-text"></span>';
	pb_adj_lo_ttip = html("id-pb-adj-lo-ttip");
	add_scale_listner(pb_adj_lo);

	pb_adj_hi = html("id-pb-adj-hi");
	pb_adj_hi.innerHTML = '<span id="id-pb-adj-hi-ttip" class="class-passband-adjust-cut-tooltip class-tooltip-text"></span>';
	pb_adj_hi_ttip = html("id-pb-adj-hi-ttip");
	add_scale_listner(pb_adj_hi);

	pb_adj_cf = html("id-pb-adj-cf");
	pb_adj_cf.innerHTML = '<span id="id-pb-adj-cf-ttip" class="class-passband-adjust-cf-tooltip class-tooltip-text"></span>';
	pb_adj_cf_ttip = html("id-pb-adj-cf-ttip");
	add_scale_listner(pb_adj_cf);

	band_canvas = html("id-band-canvas");
	band_ctx = band_canvas.getContext("2d");
	add_canvas_listner(band_canvas);
	
	dx_div = html('id-dx-container');
	add_canvas_listner(dx_div);

	dx_canvas = html("id-dx-canvas");
	dx_ctx = dx_canvas.getContext("2d");
	add_canvas_listner(dx_canvas);
	
	resize_scale('setup');
}

function add_scale_listner(obj)
{
	obj.addEventListener("mousedown", scale_canvas_mousedown, false);
	obj.addEventListener("mousemove", scale_canvas_mousemove, false);
	obj.addEventListener("mouseup", scale_canvas_mouseup, false);
	obj.addEventListener("contextmenu", scale_canvas_contextmenu, false);

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

var scale_canvas_drag_params = {
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
	with (scale_canvas_drag_params) {
		drag = false;
		start_x = evt.pageX;
		start_y = evt.pageY;
		key_modifiers.shiftKey = evt.shiftKey;
		key_modifiers.altKey = evt.altKey;
		key_modifiers.ctrlKey = evt.ctrlKey;
	}
	scale_canvas_start_drag(evt, 1);
	evt.preventDefault();
}

function scale_canvas_touchStart(evt)
{
   if (evt.targetTouches.length == 1) {
		with (scale_canvas_drag_params) {
			drag = false;
			last_x = start_x = evt.targetTouches[0].pageX;
			last_y = start_y = evt.targetTouches[0].pageY;
			key_modifiers.shiftKey = false;
			key_modifiers.altKey = false;
			key_modifiers.ctrlKey = false;
		}
	   scale_canvas_start_drag(evt, 0);
	}
	
	touch_restore_tooltip_visibility(owrx.scale_canvas);
	evt.preventDefault();
}

var scale_canvas_ignore_mouse_event = false;

function scale_canvas_start_drag(evt, isMouse)
{
	// Distinguish ctrl-click right-button meta event from actual right-button on mouse (or touchpad two-finger tap).
	if (evt.button == mouse.right && !evt.ctrlKey) {
		right_click_menu(scale_canvas_drag_params.start_x, scale_canvas_drag_params.start_y);
      scale_canvas_drag_params.mouse_down = owrx.scale_canvas.mouse_out = false;
		scale_canvas_ignore_mouse_event = true;
		return;
	}

   scale_canvas_drag_params.mouse_down = true;
   owrx.scale_canvas.target = evt.target;
   if (isMouse) scale_canvas_mousemove(evt);
}

function scale_offset_carfreq_from_px(x, visible_range)
{
	if (isUndefined(visible_range)) visible_range = get_visible_freq_range();
	var offset = passband_offset();
	var f = visible_range.start + visible_range.bw*(x/wf_container.clientWidth);
	//console.log("SOCFFPX f="+f+" off="+offset+" f-o="+(f-offset)+" rtn="+(f - center_freq - offset));
	return f - center_freq - offset;
}

function scale_canvas_drag(evt, x, y)
{
   if (scale_canvas_ignore_mouse_event) return;
   
	//event_dump(evt, 'SC-MDRAG', 1);
	var event_handled = 0;
	var relX = Math.abs(x - scale_canvas_drag_params.start_x);

	if (scale_canvas_drag_params.mouse_down && !scale_canvas_drag_params.drag /* && relX > canvas_drag_min_delta */ ) {
		// we can use the main drag_min_delta thing of the main canvas
		scale_canvas_drag_params.drag = true;
		// call env_drag_start() for all demodulators (and they will decide if they're dragged, based on X coordinate)
		for (var i=0; i < demodulators.length; i++)
			event_handled |= demodulators[i].envelope.env_drag_start(x, y, scale_canvas_drag_params.key_modifiers);
		//console.log("MOV1 evh? "+event_handled);
		evt.target.style.cursor = "move";
		//console.log('sc cursor');
	//} else
	}     // scale is different from waterfall: mousedown alone immediately changes frequency

	if (scale_canvas_drag_params.drag) {
		// call the drag_move for all demodulators (and they will decide if they're dragged)
		for (var i=0; i < demodulators.length; i++)
			event_handled |= demodulators[i].envelope.drag_move(x);
		//console.log("MOV2 evh? "+event_handled);
		if (!event_handled)
			demodulator_set_offset_frequency(0, scale_offset_carfreq_from_px(x));
		scale_canvas_drag_params.last_x = x;
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
   //   console.log('IME='+ scale_canvas_ignore_mouse_event +' SCMD='+ scale_canvas_drag_params.mouse_down +' ET='+ evt.type);
   
   if (scale_canvas_ignore_mouse_event) {
      scale_canvas_ignore_mouse_event = false;
      return;
   }


   // mouse was down when it went out -- let dragging continue in adjacent canvas
   // which will call us again when canvas_mouse_up occurs
   if (scale_canvas_drag_params.mouse_down == true && evt.type == "mouseout") {
      owrx.scale_canvas.mouse_out = true;
      return;
   }

	scale_canvas_drag_params.drag = false;
	var event_handled = false;
	
	if (scale_canvas_drag_params.mouse_down == true) {
      for (var i=0; i < demodulators.length; i++) event_handled |= demodulators[i].envelope.drag_end(x);
      //console.log("MED evh? "+event_handled);
      if (!event_handled) demodulator_set_offset_frequency(0, scale_offset_carfreq_from_px(x));
   }

	scale_canvas_drag_params.mouse_down = false;
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
	scale_canvas_end_drag(evt, evt.pageX);
}


// Solves this problem: Using a mouse, the tooltip will eventually disappear when the mouse wanders
// outside of the tooltip, because mouse-move events still occur even though the mouse button is up.
// But for touch events a touch-end *may* occur while still positioned over the tooltip or its
// container. When there are no more touches the tooltip remains displayed until the next touch-start.
//
// This is (maybe) not so desirable. What we do is force-hide tooltip on touch-end after a delay and
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
	scale_canvas_end_drag(evt, scale_canvas_drag_params.last_x);
	touch_hide_tooltip(evt, owrx.scale_canvas);
	evt.preventDefault();
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
	
	if (wf.audioFFT_active && cur_mode) {
	   var off, span;
      var srate = Math.round(audio_input_rate || 12000);
	   if (ext_is_IQ_or_stereo_curmode()) {
	      off = 0;
	      span = srate;
	   } else
	   if (cur_mode && (cur_mode.substr(0,2) == 'ls' || cur_mode == 'sal')) {
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
	}

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
   //console.log('mkfs z'+ zoom_level +' kiwi.freq_offset_kHz='+ kiwi.freq_offset_kHz +' offset_frac='+ kHz(kiwi.offset_frac) +' marker_hz='+ kHz(marker_hz) +'|'+ kHz(marker_hz_offset));
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
	}
	
	var last_large;
   var conv_ct=0;

	for (;;) {
      conv_ct++;
      if (conv_ct > 1000) break;
		var x = scale_px_from_freq_offset(marker_hz, g_range);
      //console.log('mkfs marker_hz|HPLM|0?='+ kHz(marker_hz) +'|'+ kHz(marker_hz_offset) +' '+ kHz(spacing.params.hz_per_large_marker) +' '+ (marker_hz_offset % spacing.params.hz_per_large_marker) +' x='+ x);
		if (x > window.innerWidth) break;
		scale_ctx.beginPath();		
		scale_ctx.moveTo(x, 22);

		if (marker_hz_offset % spacing.params.hz_per_large_marker == 0) {

			//large marker
			if (isUndefined(first_large)) var first_large = marker_hz; 
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
		var x = scale_px_from_freq_offset(f, g_range);
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
var dx_car_w = dx_car_h = dx_car_border*2 + dx_car_size;

function resize_scale(a)
{
	band_ctx.canvas.width  = window.innerWidth;
	band_ctx.canvas.height = band_canvas_h;

	dx_div.style.width = px(window.innerWidth);

	dx_ctx.canvas.width = dx_car_w;
	dx_ctx.canvas.height = dx_car_h;
	dx_canvas.style.top = px(scale_canvas_top - dx_car_h + 2);
	dx_canvas.style.left = 0;
	dx_canvas.style.zIndex = 99;
	
	// the dx canvas is used to form the "carrier" marker symbol (yellow triangle) seen when
	// an NDB dx label is entered by the mouse
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
}


////////////////////////////////
// conversions
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
	if (xbin < 0) xbin = 0;
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
	if (isUndefined(demodulators[0])) return x_bin;	// punt if no demod yet
	var f = freq_passband_center();
	var pb_bin = freq_to_bin(f);
	//console.log("PBV f="+f+" x_bin="+x_bin+" BACZ="+bins_at_cur_zoom()+" max_bin="+(x_bin+bins_at_cur_zoom())+" pb_bin="+pb_bin);
	pb_bin = (pb_bin >= x_bin && pb_bin < (x_bin+bins_at_cur_zoom()))? pb_bin : -pb_bin-1;
	//console.log("PBV ="+pb_bin+' '+((pb_bin<0)? 'outside':'inside'));
	return pb_bin;
}


////////////////////////////////
// canvas
////////////////////////////////

var debug_canvas_drag = false;

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
	add_canvas_listner(canvas_annotation);
	
	// annotation div for text containing links etc.
	annotation_div = w3_el('id-annotation-div');
	add_canvas_listner(annotation_div);
	annotation_div.style.pointerEvents = 'none';    // prevent premature end of canvas dragging

	// a phantom one at the end
	// not an actual canvas but a <div> spacer
	canvas_phantom = html("id-phantom-canvas");
	add_canvas_listner(canvas_phantom);
	canvas_phantom.style.width = px(wf_container.clientWidth);

	// the first one to get started
	add_wf_canvas();

   spec.canvas = create_canvas('id-spectrum-canvas', wf_fft_size, spec.height_spectrum_canvas, waterfall_width, spec.height_spectrum_canvas);
	html("id-spectrum-container").appendChild(spec.canvas);
	spec.ctx = spec.canvas.ctx;
	add_canvas_listner(spec.canvas);
	spec.ctx.font = "10px sans-serif";
	spec.ctx.textBaseline = "middle";
	spec.ctx.textAlign = "left";

	spec.dB = html("id-spectrum-dB");
	spec.dB.style.height = px(spec.height_spectrum_canvas);
	spec.dB.style.width = px(waterfall_width);
	spec.dB.innerHTML = '<span id="id-spectrum-dB-ttip" class="class-spectrum-dB-tooltip class-tooltip-text"></span>';
	add_canvas_listner(spec.dB);
	spec.dB_ttip = html("id-spectrum-dB-ttip");
}

function add_canvas_listner(obj)
{
	obj.addEventListener("mouseover", canvas_mouseover, false);
	obj.addEventListener("mouseout", canvas_mouseout, false);
	obj.addEventListener("mousedown", canvas_mousedown, false);
	obj.addEventListener("mousemove", canvas_mousemove, false);
	obj.addEventListener("mouseup", canvas_mouseup, false);
	obj.addEventListener("contextmenu", canvas_contextmenu, false);
	obj.addEventListener("wheel", canvas_mousewheel, false);

	if (kiwi_isMobile()) {
		obj.addEventListener('touchstart', canvas_touchStart, false);
		obj.addEventListener('touchmove', canvas_touchMove, false);
		obj.addEventListener('touchend', canvas_touchEnd, false);
	}
}

function remove_canvas_listner(obj)
{
	obj.removeEventListener("mouseover", canvas_mouseover, false);
	obj.removeEventListener("mouseout", canvas_mouseout, false);
	obj.removeEventListener("mousedown", canvas_mousedown, false);
	obj.removeEventListener("mousemove", canvas_mousemove, false);
	obj.removeEventListener("mouseup", canvas_mouseup, false);
	obj.removeEventListener("contextmenu", canvas_contextmenu, false);
	obj.removeEventListener("wheel", canvas_mousewheel, false);

	if (kiwi_isMobile()) {
		obj.removeEventListener('touchstart', canvas_touchStart, false);
		obj.removeEventListener('touchmove', canvas_touchMove, false);
		obj.removeEventListener('touchend', canvas_touchEnd, false);
	}
}

function canvas_log(s)
{
   if (isUndefined(owrx.news_acc_s)) owrx.news_acc_s = '';

   if (s == '\f') {
      owrx.news_acc_s = '';
   } else
   if (s.charAt(0) == '$') {
      owrx.news_acc_s = '<br><br>'+ s;
   } else {
      owrx.news_acc_s += ((owrx.news_acc_s != '')? ' | ' : '') + s;
   }

   extint_news(owrx.news_acc_s);
}

function canvas_contextmenu(evt)
{
	//console.log('## CMENU tgt='+ evt.target.id +' Ctgt='+ evt.currentTarget.id);
	
	if (evt.target.id == 'id-wf-canvas') {
		// TBD: popup menu with database lookup, etc.
	}
	
	// let ctrl-dx_click thru
	if (evt && evt.currentTarget && evt.currentTarget.id == 'id-dx-container') {
	   //console.log('canvas_contextmenu: synthetic ctrl-dx_click');
	   dx.ctrl_click = true;
	   w3_el(evt.target.id).click();
	}
	
	// must always cancel even so system context menu doesn't appear
	return cancelEvent(evt);
}

function canvas_mouseover(evt)
{
	if (!waterfall_setup_done) return;
	//html("id-freq-show").style.visibility="visible";	
}

function canvas_mouseout(evt)
{
	if (!waterfall_setup_done) return;
	if (debug_canvas_drag) event_dump(evt, 'canvas_mouseout', 1);
	//html("id-freq-show").style.visibility="hidden";

   // must also end dragging when mouse leaves canvas while still down
	if (canvas_dragging) {
      if (debug_canvas_drag) canvas_log("C-MOUT");
      canvas_end_drag2();
   }
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
	return canvas_get_carfreq_offset(relativeX, false) + center_freq;
}

canvas_dragging = false;
canvas_drag_min_delta = 1;
canvas_mouse_down = false;
canvas_ignore_mouse_event = false;

var mouse = { 'left':0, 'middle':1, 'right':2 };

function canvas_start_drag(evt, x, y)
{
	if (debug_canvas_drag) canvas_log('CSD');
	
	// Distinguish ctrl-click right-button meta event from actual right-button on mouse (or touchpad two-finger tap).
	if (evt.button == mouse.right && !evt.ctrlKey) {
		canvas_ignore_mouse_event = true;
		if (debug_canvas_drag) console.log('CSD-Rclick IME=set-true');
		if (debug_canvas_drag) canvas_log('ordinary-RCM');
		right_click_menu(x, y);
      owrx.right_click_menu_active = true;
		canvas_ignore_mouse_event = false;
		if (debug_canvas_drag) console.log('CSD-Rclick IME=set-false');
		return;
	}
	
	if (owrx.scale_canvas.mouse_out) {
	   event_dump(evt, "CSD-scale");
	   console.log('SHOULD NOT OCCUR');
	   return;
	}

	if (evt.shiftKey && evt.target.id == 'id-dx-container') {
		canvas_ignore_mouse_event = true;
		if (debug_canvas_drag) console.log('CSD-DX IME=set-true');
		dx_show_edit_panel(evt, -1);
	} else

	// select waterfall on nearest appropriate boundary (1, 5 or 9/10 kHz depending on band & mode)
	if ((evt.shiftKey && !(evt.ctrlKey || evt.altKey)) || owrx.wf_snap) {
		canvas_ignore_mouse_event = true;
		if (debug_canvas_drag) console.log('CSD-Wboundary IME=set-true');
		var fold = canvas_get_dspfreq(x);

		var b = find_band(fold);
		//if (b) console.log(b)
      var rv = freq_step_amount(b);
		var step_Hz = rv.step_Hz;
		
		var trunc = fold / step_Hz;
		var fnew = Math.round(trunc) * step_Hz;
		//console.log('SFT-CLICK '+cur_mode+' fold='+fold+' step='+step_Hz+' trunc='+trunc+' fnew='+fnew +' '+ rv.s);
		freqmode_set_dsp_kHz(fnew/1000, null);
	} else

	// lookup mouse pointer frequency in online resource appropriate to the frequency band
	if (evt.shiftKey && (evt.ctrlKey || evt.altKey)) {
		canvas_ignore_mouse_event = true;
		if (debug_canvas_drag) console.log('CSD-lookup IME=set-true');
		freq_database_lookup(canvas_get_dspfreq(x), evt.altKey);
	} else
	
	// page scrolling via ctrl & alt-key click
	if (evt.ctrlKey) {
		canvas_ignore_mouse_event = true;
		if (debug_canvas_drag) console.log('CSD-pageScroll1 IME=set-true');
		page_scroll(-page_scroll_amount);
	} else
	
	if (evt.altKey) {
		canvas_ignore_mouse_event = true;
		if (debug_canvas_drag) console.log('CSD-pageScroll2 IME=set-true');
		page_scroll(page_scroll_amount);
	}
	
	owrx.drag_count = 0;
	canvas_mouse_down = true;
	canvas_dragging = false;
	owrx.canvas.drag_last_x = owrx.canvas.drag_start_x = x;
	owrx.canvas.drag_last_y = owrx.canvas.drag_start_y = y;
   owrx.canvas.target = evt.target;
}

function canvas_mousedown(evt)
{
	if (debug_canvas_drag) canvas_log('$C-MD RCMA'+ (owrx.right_click_menu_active? 1:0));
   //event_dump(evt, "C-MD");
	canvas_start_drag(evt, evt.pageX, evt.pageY);
	evt.preventDefault();	// don't show text selection mouse pointer
}

function canvas_touchStart(evt)
{
   var touches = evt.targetTouches.length;
   var x = Math.round(evt.targetTouches[0].pageX);
   var y = Math.round(evt.targetTouches[0].pageY);
	if (debug_canvas_drag) canvas_log("$C-TS"+ touches +'-x'+ x +'-y'+ y);
   owrx.double_touch_start = false;
   
   if (touches == 1) {
		canvas_start_drag(evt, x, y);
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
	
	if (touches == 2) {
		canvas_start_drag(evt, x, y);
	   //alert('canvas_touchStart='+ evt.targetTouches.length);
		canvas_ignore_mouse_event = true;
		if (debug_canvas_drag) console.log('CTS-doubleTouch IME=set-true');
		owrx.touches_first_startx = x;
      owrx.pinch_distance_first =
         Math.round(Math.hypot(evt.touches[0].pageX - evt.touches[1].pageX, evt.touches[0].pageY - evt.touches[1].pageY));
      owrx.pinch_distance_last = owrx.pinch_distance_first;
		owrx.double_touch_start = true;
	}

	touch_restore_tooltip_visibility(owrx.canvas);
	evt.preventDefault();	// don't show text selection mouse pointer
}

function canvas_drag(evt, x, y, clientX, clientY)
{
	if (!waterfall_setup_done) return;
	//element=html("id-freq-show");
	var relativeX = x;
	var relativeY = y;
	spectrum_tooltip_update(evt, clientX, clientY);
	owrx.drag_count++;

	if (owrx.scale_canvas.mouse_out) {
	   //event_dump(evt, "CD", 1);
	   scale_canvas_drag(evt, x, y);    // continue scale dragging even though positioned on other canvases
	   return;
	}
	   
   if (debug_canvas_drag)
      canvas_log('CD#'+ owrx.drag_count +' x'+ x +' y'+ y +' CMD'+ (canvas_mouse_down? 1:0) +' IME'+ (canvas_ignore_mouse_event? 1:0) +' DG'+ (canvas_dragging? 1:0));

   // drag_count > 10 was required on Lenovo TB-7104F / Android 8.1.0 to differentiate double-touch from true drag.
   // I.e. an excessive number of touch events seem to be sent by browser for a single double-touch.
	if (canvas_mouse_down && (!canvas_ignore_mouse_event || owrx.double_touch_start)) {
		if (!canvas_dragging && owrx.drag_count > 10 && Math.abs(x - owrx.canvas.drag_start_x) > canvas_drag_min_delta) {
			canvas_dragging = true;
			wf_container.style.cursor = "move";
		}
		if (canvas_dragging) {
			var deltaX = owrx.canvas.drag_last_x - x;
			var deltaY = owrx.canvas.drag_last_y - y;

         if (owrx.double_touch_start) {
            var dist = Math.round(Math.hypot(owrx.canvas.drag_last_x - x, owrx.canvas.drag_last_y - y));
            var delta = Math.abs(dist - owrx.pinch_distance_last);
            if (debug_canvas_drag) canvas_log(dist.toFixed(0) +' '+ delta.toFixed(0));
            if (delta > 25) {
               if (debug_canvas_drag) canvas_log('*');
               var pinch_in = (dist <= owrx.pinch_distance_last);
               zoom_step(pinch_in? ext_zoom.OUT : ext_zoom.IN);
               if (debug_canvas_drag) canvas_log(pinch_in? 'IN' : 'OUT');
               owrx.pinch_distance_last = dist;
            }
         } else {
            var dbins = norm_to_bins(deltaX / waterfall_width);
            waterfall_pan_canvases(dbins);
         }

			owrx.canvas.drag_last_x = x;
			owrx.canvas.drag_last_y = y;
		}
	} else {
		w3_innerHTML('id-mouse-unit', format_frequency("{x}", canvas_get_dspfreq(relativeX) + kiwi.freq_offset_Hz, 1e3, 2));
		//console.log("MOU rX="+relativeX.toFixed(1)+" f="+canvas_get_dspfreq(relativeX).toFixed(1));
	}
}

function canvas_mousemove(evt)
{
	//if (debug_canvas_drag) console.log("C-MM");
   //event_dump(evt, "C-MM");
	canvas_drag(evt, evt.pageX, evt.pageY, evt.clientX, evt.clientY);
}

function canvas_touchMove(evt)
{
	if (evt.touches.length >= 1) {
	   owrx.touches_first_lastx = evt.touches[0].pageX;
	}
	
	for (var i=0; i < evt.touches.length; i++) {
		var x = Math.round(evt.touches[i].pageX);
		var y = Math.round(evt.touches[i].pageY);
	   //if (debug_canvas_drag) canvas_log('C-TM-x'+ x +'-y'+ y);

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
   
		canvas_drag(evt, x, y, x, y);
	}
	evt.preventDefault();
}

function canvas_end_drag2()
{
	if (debug_canvas_drag) canvas_log("C-ED2");
	wf_container.style.cursor = owrx.wf_cursor;
	canvas_mouse_down = false;
	canvas_ignore_mouse_event = false;
	if (debug_canvas_drag) { console.log("C-ED2 IME=set-false"); }
}

function canvas_end_drag(evt, x)
{
	if (owrx.scale_canvas.mouse_out) {
	   //event_dump(evt, "CED");
	   owrx.scale_canvas.mouse_out = false;
	   scale_canvas_end_drag(evt, x, /* canvas_mouse_up */ true);
	}

	if (debug_canvas_drag) canvas_log('C-ED IME'+ (canvas_ignore_mouse_event? 1:0) +' CD'+ (canvas_dragging? 1:0));

	if (!waterfall_setup_done) return;
	//console.log("MUP "+this.id+" ign="+canvas_ignore_mouse_event);
	var relativeX = x;

	if (canvas_ignore_mouse_event) {
	   //console.log('## canvas_ignore_mouse_event');
		//ignore_next_keyup_event = true;
	} else {
		if (!canvas_dragging) {
			//event_dump(evt, "MUP");
			
			// don't set freq if mouseup without mousedown due to move into canvas from elsewhere
			if (debug_canvas_drag) canvas_log('CMD'+ (canvas_mouse_down? 1:0) +' RCMA'+ (owrx.right_click_menu_active? 1:0) +' TL'+ owrx.tuning_locked);
			if (canvas_mouse_down) {
			
			   // mobile mode (touch screen): hack to close menu when touch outside of menu area.
			   // Desktop does this instead by intercepting mousedown and keyboard escape events.
			   // Intercepting touchstart didn't work hence this hack.
			   if (owrx.right_click_menu_active) {
			      w3int_menu_onclick(null, 'id-right-click-menu');
			      owrx.right_click_menu_active = false;
			   } else {
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
               } else {
                  // single-click in canvas area
                  if (debug_canvas_drag) canvas_log('*click*');
                  demodulator_set_offset_frequency(0, canvas_get_carfreq_offset(relativeX, true));
               }
            }
			}
		} else {
			canvas_end_drag2();
		}
	}
	
	canvas_mouse_down = false;
	canvas_ignore_mouse_event = false;
	if (debug_canvas_drag) console.log('C-ED IME=set-false');
}

function canvas_mouseup(evt)
{
	if (debug_canvas_drag) console.log("C-MU");
   //event_dump(evt, "C-MU");
	canvas_end_drag(evt, evt.pageX);
}

function canvas_touchEnd(evt)
{
	var x = owrx.canvas.drag_last_x, y = owrx.canvas.drag_last_y;
	if (debug_canvas_drag) canvas_log('C-TE-x'+ x +'-DTS'+ (owrx.double_touch_start? 1:0) +'-DRAG'+ (canvas_dragging? 1:0));
	canvas_end_drag(evt, x);
/*
   owrx.touch_hold_pressed = false;
   kiwi_clearInterval(owrx.touch_hold_interval);
*/
	spectrum_tooltip_update(evt, x, y);
	
	if (owrx.double_touch_start) {
	   if (debug_canvas_drag) canvas_log('dr'+ canvas_dragging);

	   if (!canvas_dragging) {
         // ensure menu on narrow screen devices is visible to prevent off-screen placement
         if (kiwi_isMobile() && owrx.mobile && owrx.mobile.small)
            x = 10;
   
         if (debug_canvas_drag) canvas_log('*');
         right_click_menu(x, y);
         owrx.right_click_menu_active = true;
      } else {
         //var pinch_in = (owrx.pinch_distance_first >= owrx.pinch_distance_last)? 1:0;
         //if (debug_canvas_drag) canvas_log('pinch-'+ owrx.pinch_distance_first +'-'+ owrx.pinch_distance_last + (pinch_in? '-IN' : '-OUT'));
      }
      
      canvas_ignore_mouse_event = false;
      if (debug_canvas_drag) console.log('CTE-doubleTouch IME=set-false');
		owrx.double_touch_start = false;
	}
	
	touch_hide_tooltip(evt, owrx.canvas);
	evt.preventDefault();
}

var canvas_mousewheel_rlimit = kiwi_rateLimit(canvas_mousewheel_cb, 170);

function canvas_mousewheel(evt)
{
   canvas_mousewheel_rlimit(evt);
	evt.preventDefault();	
}

function canvas_mousewheel_cb(evt)
{
	if (!waterfall_setup_done) return;
	//console.log(evt);
   zoom_step((evt.deltaY < 0)? ext_zoom.IN : ext_zoom.OUT, evt.pageX);
	
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


////////////////////////////////
// right click menu
////////////////////////////////

function right_click_menu_init()
{
   var i = 0;
   var m = [];
   m.push('DX stored database'); owrx.rcm_stored = i; i++;
   m.push('DX EiBi database'); owrx.rcm_eibi = i; i++;
   m.push('edit last selected DX label'); owrx.rcm_edit = i; i++;
   m.push('DX label filter'); owrx.rcm_filter = i; i++;
   m.push('<hr>'); i++;

   m.push('database lookup'); owrx.rcm_db = i; i++;
   //m.push('Utility database lookup'); owrx.rcm_util = i; i++;
   m.push('DX Cluster lookup'); owrx.rcm_cluster = i; i++;
   m.push('<hr>'); i++;

   m.push('snap to nearest'); owrx.rcm_snap = i; i++;
   m.push('🔒 lock tuning'); owrx.rcm_lock = i; i++;
   m.push('restore passband'); owrx.rcm_pb = i; i++;
   m.push('save waterfall as JPG'); owrx.rcm_save = i; i++;
   m.push('<hr>'); i++;

   m.push('<i>cal ADC clock (admin)</i>'); owrx.rcm_cal = i; i++;
   m.push('<i>set freq offset (admin)</i>'); owrx.rcm_foff = i; i++;
   owrx.right_click_menu_content = m;
   
   w3_menu('id-right-click-menu', 'right_click_menu_cb');

   // for tuning lock
   var s =
      w3_div('id-tuning-lock-container class-overlay-container w3-hide',
         w3_div('id-tuning-lock', w3_icon('', 'fa-lock', 192) + '<br>Tuning locked')
      );
   w3_create_appendElement('id-main-container', 'div', s);
}

function right_click_menu(x, y)
{
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

   owrx.right_click_menu_content[owrx.rcm_db] = db + ' database lookup';
   owrx.right_click_menu_content[owrx.rcm_snap] = owrx.wf_snap? 'no snap' : 'snap to nearest';
   
   // disable menu item if last label gid is not set
   var edit = (dx.db == dx.DB_STORED && isDefined(owrx.dx_click_gid_last));
   owrx.right_click_menu_content[owrx.rcm_edit] = edit? 'edit last selected DX label' : 'open DX label edit panel';

   w3_menu_items('id-right-click-menu', owrx.right_click_menu_content);
   w3_menu_popup('id-right-click-menu', x, y);
}

function right_click_menu_cb(idx, x)
{
   //console.log('right_click_menu_cb idx='+ idx +' x='+ x +' f='+ canvas_get_dspfreq(x)/1e3);
   
   switch (idx) {
   
   case owrx.rcm_stored:
   case owrx.rcm_eibi:
      dx_database_cb('', (idx == owrx.rcm_stored)? dx.DB_STORED : dx.DB_EiBi);
      break;

   case owrx.rcm_db:  // database lookups
   case owrx.rcm_cluster:
		freq_database_lookup(canvas_get_dspfreq(x), idx);
      break;
   
   case owrx.rcm_snap:  // snap to nearest
      wf_snap();
      break;

   case owrx.rcm_lock:  // tuning lock
      owrx.tuning_locked ^= 1;
      owrx.right_click_menu_content[owrx.rcm_lock] = (owrx.tuning_locked? '🔓 unlock' : '🔒 lock') +' tuning';
      break;
      
   case owrx.rcm_pb:  // restore passband
      restore_passband(cur_mode);
      demodulator_analog_replace(cur_mode);
      break;
      
   case owrx.rcm_save:  // save waterfall image
      export_waterfall();
      break;
   
   case owrx.rcm_edit:  // edit last selected dx label
      var gid = -1;
      if (dx.db == dx.DB_STORED && isDefined(owrx.dx_click_gid_last))
         gid = owrx.dx_click_gid_last;
      console.log('RCM rcm_edit: gid='+ gid +' db='+ dx.db);
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
   
   case -1:
   default:
      break;
   }

   owrx.right_click_menu_active = false;
}


////////////////////////////////
// database lookup
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
   }
   */
   if (utility == owrx.rcm_db) {
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
   }
   if (utility == owrx.rcm_cluster) {
      f = Math.floor(Hz) / 1000;	// kHz for ve3sun dx cluster lookup
      url = 'http://ve3sun.com/KiwiSDR/DX.php?Search='+f.toFixed(1);
   }
   
   console.log('LOOKUP '+ kHz +' -> '+ f +' '+ url);
   var win = window.open(url, '_blank');
   if (win) win.focus();
}


////////////////////////////////
// export waterfall
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
   var specH = (specC && spec.source_wf)? specC.height : 0;

   var canvas = document.createElement("canvas");
   var ctx = canvas.getContext("2d");
   var canW = canvas.width = wf_fft_size;
   var canH = canvas.height = legendH + fscaleH + specH + (old_canvases.length + wf_canvases.length) * wf_canvas_default_height;
   //w3_console.log({mult:wf.scroll_multiple, Ncan:wf_canvases.length, Nold:old_canvases.length, canH}, 'export_waterfall');
   ctx.fillStyle="black";
   ctx.fillRect(0, 0, canW, canH);

   if (specH) {
      ctx.drawImage(specC, 0,h);
      h += specH;
   }
   
   ctx.fillStyle="gray";
   ctx.fillRect(0, h, canW, fscaleH);
   //w3_console.log({fscaleW, canW, ratio:fscaleW/canW}, 'export_waterfall');
   ctx.drawImage(fscale, 0,0, fscaleW,fscaleH, 0,h, canW, fscaleH);
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
      ctx.drawImage(wf_c, 0,wfY, canW,wfH, 0,h, canW,wfH);
      //ctx.fillStyle="red";
      //ctx.fillRect(0, h, canW, 2);
      h += wfH;
   });

   if (old_canvases.length > 1) {
      for (i = 1; i < old_canvases.length; i++) {
         ctx.drawImage(old_canvases[i], 0,h);
         //ctx.fillStyle="red";
         //ctx.fillRect(0, h, canW, 2);
         h += old_canvases[i].height;
      }
   }
   //ctx.fillStyle="red";
   //ctx.fillRect(0, h, canW, 2);
   
   ctx.font = "18px Arial";
   ctx.fillStyle = "lime";
   var flabel = kiwi_host() +'     '+ (new Date()).toUTCString() +'     '+ freq_displayed_kHz_str +' kHz  '+ cur_mode;
   var x = canW/2 - ctx.measureText(flabel).width/2;
   ctx.fillText(flabel, x, 35);

   // same format filename as .wav recording
   var fileName = kiwi_host() +'_'+ new Date().toISOString().replace(/:/g, '_').replace(/\.[0-9]+Z$/, 'Z') +'_'+ w3_el('id-freq-input').value +'_'+ cur_mode +'.jpg';
   var imgURL = canvas.toDataURL("image/jpeg", 0.85);
   var dlLink = document.createElement('a');
   dlLink.download = fileName;
   dlLink.href = imgURL;
   dlLink.target = '_blank';  // opens new tab in iOS instead of download
   dlLink.dataset.downloadurl = ["image/jpeg", dlLink.download, dlLink.href].join(':');
   document.body.appendChild(dlLink);
   dlLink.click();
   document.body.removeChild(dlLink);
}


////////////////////////////////
// zoom
////////////////////////////////

function zoom_finally()
{
	w3_innerHTML('id-nav-optbar-wf', 'WF'+ zoom_level.toFixed(0));
	wf_gnd_value = wf_gnd_value_base - zoomCorrection();
   extint_environment_changed( { zoom:1, passband_screen_location:1 } );
	freqset_select();
}

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
	
	if (dir == ext_zoom.WHEEL) {
	   if (arg2 == undefined) return;
	   update_zoom_f = false;
	   var znew = Math.round(zoom_level_f);
	   if (znew == ozoom) return;
	   dir = (znew > ozoom)? ext_zoom.IN : ext_zoom.OUT;
	}

	//console.log('zoom_step dir='+ dir +' arg2='+ arg2);
	if (dir == ext_zoom.MAX_OUT) {		// max out
		out = true;
		zoom_level = 0;
		x_bin = 0;
	} else {			// in/out, nom/max in, abs, band
	
		// clamp
		if (not_band_and_not_abs && ((out && zoom_level == 0) || (dir_in && zoom_level >= zoom_levels_max))) { zoom_finally(); return; }

		if (dir == ext_zoom.TO_BAND) {
			// zoom to band
			var f = center_freq + demodulators[0].offset_frequency;
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
				for (i=0; i < cfg.bands.length; i++) {    // search for first containing band
					b1 = cfg.bands[i];
					b2 = kiwi.bands[i];
		         if (!(b1.itu == kiwi.BAND_SCALE_ONLY || b1.itu == kiwi.ITU_ANY || b1.itu == ITU_region)) continue;
					if (f >= b2.minHz && f <= b2.maxHz) {
		            //console.log('zoom_band FOUND itu=R'+ b1.itu +' '+ b1.min +' '+ f +' '+ b1.max);
						break;
					}
				}
				if (i != cfg.bands.length) {
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
			x_bin -= norm_to_bins(0.5);
		} else
		
		if (dir == ext_zoom.ABS) {
			if (arg2 == undefined) { zoom_finally(); return; }		// no abs zoom value specified
			var znew = arg2;
			//console.log('zoom_step ABS znew='+ znew +' zmax='+ zoom_levels_max +' zcur='+ zoom_level);
			if (znew < 0 || znew > zoom_levels_max || znew == zoom_level) { zoom_finally(); return; }
			out = (znew < zoom_level);
			zoom_level = znew;
			// center waterfall at middle of passband
			x_bin = freq_to_bin(freq_passband_center());
			x_bin -= norm_to_bins(0.5);
			//console.log("ZOOM ABS z="+znew+" out="+out+" b="+x_bin);
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
			x_bin -= norm_to_bins(0.5);
		} else {
		
			// in, out
			if (dbgUs) {
				if (sb_trace) console.log('ZOOM IN/OUT');
				sb_trace=0;
			}
			if (arg2 != undefined) {
				var x_rel = arg2;
				
				// zoom in or out centered on cursor position, not passband
				x_norm = x_rel / waterfall_width;	// normalized position (0..1) on waterfall
			} else {
				// zoom in or out centered on passband, if visible, else middle of current waterfall
				var pb_bin = passband_visible();
				if (pb_bin >= 0) {		// visible
					x_norm = (pb_bin - x_bin) / bins_at_cur_zoom();
				} else {
					x_norm = 0.5;
				}
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
	if (sb_trace) console.log("Zs z"+ozoom+'>'+zoom_level+' b='+x_bin+'/'+x_obin+'/'+dbins+' bz='+bins_at_zoom(ozoom)+' r='+(dbins / bins_at_zoom(ozoom))+' px='+pixel_dx);
	var dz = zoom_level - ozoom;
	if (sb_trace) console.log('zoom_step oz='+ ozoom +' zl='+ zoom_level +' dz='+ dz +' pdx='+ pixel_dx);
	waterfall_zoom_canvases(dz, pixel_dx);
	mkscale();
	dx_schedule_update();
	if (sb_trace) console.log("SET Z"+zoom_level+" xb="+x_bin);
	wf_send("SET zoom="+ zoom_level +" start="+ x_bin);
	need_maxmindb_update = true;
	writeCookie('last_zoom', zoom_level);
	freq_link_update();
   zoom_finally();

   if (dbgUs) {
      var f = get_visible_freq_range();
      console.log('^ZOOM z'+ zoom_level +' '+ (f.start/1e3).toFixed(0) +'|'+
         (f.center/1e3).toFixed(0) +'|'+ (f.end/1e3).toFixed(0) +' ('+ (f.bw/1e3).toFixed(0) +' kHz)');
   }
}

function passband_increment(wider)
{
   var pb = ext_get_passband();
   var pb_width = Math.abs(pb.high - pb.low);
   var pb_inc;
   if (wider)
      pb_inc = ((pb_width * (1/0.80)) - pb_width) / 2;		// wider
   else
      pb_inc = (pb_width - (pb_width * 0.80)) / 2;				// narrower

   var rnd = (pb_inc > 10)? 10 : 1;
   pb_inc = Math.round(pb_inc/rnd) * rnd;
   //console.log('PB w='+ pb_width +' inc='+ pb_inc +' lo='+ pb.low +' hi='+ pb.high);
   pb.low += wider? -pb_inc : pb_inc;
   pb.low = Math.round(pb.low/rnd) * rnd;
   pb.high += wider? pb_inc : -pb_inc;
   pb.high = Math.round(pb.high/rnd) * rnd;
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
		passband_increment(zin);
		return;
	}
	
	if (dir == ext_zoom.NOM_IN)
		zoom_step(dir, 1);		// zoom max-in button toggle hack
	else
		zoom_step(dir);
}

function zoom_over(evt)
{
	// currentTarget.parent=<span> currentTarget=<div> target=<img>
	if (evt.target.nodeName != 'IMG') return;
	var img = evt.target;
	var div = evt.currentTarget;
	//console.log(div);
	//var parent = div.parentNode;
	//console.log(parent);
	
	var zin = w3_contains(div, 'id-zoom-in');
	
	// apply shift-key title to inner IMG element so when removed the default of the parent div applies
	if (any_alternate_click_event(evt)) {
		img.title = zin? 'passband widen' : 'passband narrow';
	} else {
		img.removeAttribute('title');
	}
}


////////////////////////////////
// mobile
////////////////////////////////

function mobile_init()
{
	// When a mobile device connects to a Kiwi while held in portrait orientation:
	// Remove top bar and minimize control panel if width < oldest iPad width (768px)
	// which should catch all iPhones but no iPads (iPhone X width = 414px).
	// Also scale control panel for small-screen tablets, e.g. 7" tablets with 600px portrait width.

	var mobile = owrx.mobile = ext_mobile_info();
   //console.log('$ wh='+ mobile.width +','+ mobile.height);
	
	// anything smaller than iPad: remove top bar and switch control panel to "off".
	if (mobile.small) {
	   toggle_or_set_hide_bars(owrx.HIDE_TOPBAR);
	   optbar_focus('optbar-off', 'init');
	}
	
	// for narrow screen devices, i.e. phones and 7" tablets
	if (mobile.narrow) {
	   w3_hide('id-readme');   // don't show readme panel closed icon
	   
	   // remove top bar and band/label areas on phones
	   if (mobile.width < 600) {
	      toggle_or_set_hide_bars(owrx.HIDE_ALLBARS);
	   }
	}
	
   owrx.last_mobile = {};     // force rescale first time
   owrx.rescale_cnt = owrx.rescale_cnt2 = 0;

	setInterval(function() {
      mobile = owrx.mobile = ext_mobile_info(owrx.last_mobile);
      owrx.last_mobile = mobile;

      //extint_news('Cwh='+ mobile.width +','+ mobile.height +' '+ mobile.orient_unchanged +
      //   '<br>r='+ owrx.rescale_cnt  +','+ owrx.rescale_cnt2 +' #'+ owrx.dseq);
      //owrx.dseq++;

      var el = w3_el('id-control');

	   if (mobile_laptop_test) {
         extint_news('whu='+ mobile.width +','+ mobile.height +','+ el.uiWidth +
            ' psn='+ mobile.isPortrait +','+ mobile.small +','+ mobile.narrow +' #'+ owrx.dseq);
         owrx.dseq++;
      }
   
      if (mobile.orient_unchanged) return;
      owrx.rescale_cnt++;

      if (mobile.narrow) {
         // scale control panel up or down to fit width of all narrow screens
         var scale = mobile.width / el.uiWidth * 0.95;
         el.style.transform = 'scale('+ scale.toFixed(2) +')';
         el.style.transformOrigin = 'bottom right';
         el.style.right = '0px';
         owrx.rescale_cnt2++;
      } else {
         el.style.transform = 'none';
         el.style.right = '10px';
      }
	}, 500);
}


////////////////////////////////
// spectrum
////////////////////////////////

var spec = {
   height_spectrum_canvas: 200,
   tooltip_offset: 100,
   
   source_wf: 0,
   source_audio: 0,
   
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
   avg: [],
   
   peak_show: 0,
   peak_clear: 0,
   peak: [],
   
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
	setInterval(function() { spec.update++ }, 1000 / spectrum_update_rate_Hz);

   spec.spectrum_image = spec.ctx.createImageData(spec.canvas.width, spec.canvas.height)
   
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

   kiwi_load_js_dir('extensions/', ['waterfall/waterfall.js', 'colormap/colormap.js'],
      function() {
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
		var cmi = Math.round((dB - barmin) / rng * 255);
		var color = color_map[cmi];
		var color_transparent = color_map_transparent[cmi];
		var color_name = '#'+(color >>> 8).toString(16).leadingZeros(6);
		spec.dB_bands[i] = { dB:dB, norm:norm, color:color_name };
		
		var ypos = function(norm) { return Math.round(norm * spec.canvas.height) }
		for (var y = ypos(last_norm); y < ypos(norm); y++) {
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

function spectrum_tooltip_update(evt, clientX, clientY)
{
	var target = (evt.target == spec.dB || evt.currentTarget == spec.dB || evt.target == spec.dB_ttip || evt.currentTarget == spec.dB_ttip);
	//console.log('CD '+ target +' x='+ clientX +' tgt='+ evt.target.id +' ctg='+ evt.currentTarget.id);
	//if (kiwi_isMobile()) alert('CD '+ tf +' x='+ clientX +' tgt='+ evt.target.id +' ctg='+ evt.currentTarget.id);

	if (target) {
		//event_dump(evt, 'SPEC');
		
		// This is a little tricky. The tooltip text span must be included as an event target so its position will update when the mouse
		// is moved upward over it. But doing so means when the cursor goes below the bottom of the tooltip container, the entire
		// spectrum div in this case, having included the tooltip text span will cause it to be re-positioned again. And the hover
		// doesn't go away unless the mouse is moved quickly. So to stop this we need to manually detect when the mouse is out of the
		// tooltip container and stop updating the tooltip text position so the hover will end.
		
		if (clientY >= 0 && clientY < spec.height_spectrum_canvas) {
			spec.dB_ttip.style.left = px(clientX - (kiwi_isMobile()? spec.tooltip_offset : 0));
			spec.dB_ttip.style.bottom = px(spec.height_spectrum_canvas + 10 - clientY);
			var dB = (((spec.height_spectrum_canvas - clientY) / spec.height_spectrum_canvas) * full_scale) + mindb;
			spec.dB_ttip.innerHTML = dB.toFixed(0) +' dBm';
		}
	}
}

function spectrum_update(data)
{
	var i, x, y, z, sw, sh, tw=25;
	
   spec.last_update = spec.update;

   // clear entire spectrum canvas to black
   var ctx = spec.ctx;
   sw = spec.canvas.width-tw;
   sh = spec.canvas.height;
   ctx.fillStyle = "black";
   ctx.fillRect(0,0,sw,sh);
   
   // draw lines every 10 dB
   // spectrum data will overwrite
   ctx.fillStyle = "lightGray";
   for (i=0; i < spec.dB_bands.length; i++) {
      var band = spec.dB_bands[i];
      y = Math.round(band.norm * sh);
      ctx.fillRect(0,y,sw,1);
   }

   if (spec.clear_avg) {
      for (x=0; x<sw; x++) {
         spec.avg[x] = color_index(data[x]);
      }
      spec.clear_avg = false;
      spec.peak_clear = true;
   }
   
   if (spec.peak_clear) {
      for (x=0; x<sw; x++) {
         spec.peak[x] = 0;
      }
      spec.peak_clear = false;
   }
   
   // if necessary, draw scale on right side
   if (spec.redraw_dB_scale) {
   
      // set sidebar background where the dB text will go
      /*
      ctx.fillStyle = "black";
      ctx.fillRect(sw,0,tw,sh);
      */
      for (x = sw; x < spec.canvas.width; x++) {
         ctx.putImageData(spec.colormap_transparent, x, 0, 0, 0, 1, sh);
      }
      
      // the dB scale text
      ctx.fillStyle = "white";
      for (i=0; i < spec.dB_bands.length; i++) {
         var band = spec.dB_bands[i];
         y = Math.round(band.norm * sh);
         ctx.fillText(band.dB, sw+3, y-4);
         //console.log("SP x="+sw+" y="+y+' '+dB);
      }
      spec.redraw_dB_scale = false;
   }
	
	// Add line to spectrum image
   ctx.fillStyle = 'hsl(180, 100%, 70%)';

   for (x=0; x<sw; x++) {
      z = color_index(wf_gnd? wf_gnd_value : data[x]);

      switch (spec_filter) {
      
      case wf_sp_menu_e.IIR:
         // non-linear spectrum filter from Rocky (Alex, VE3NEA)
         // see http://www.dxatlas.com/rocky/advanced.asp
         
         var iir_gain = 1 - Math.exp(-sp_param * z/255);
         if (iir_gain <= 0.01) iir_gain = 0.01;    // enforce minimum decay rate
         var z1 = spec.avg[x];
         z1 = z1 + iir_gain * (z - z1);
         if (z1 > 255) z1 = 255; if (z1 < 0) z1 = 0;
         z = spec.avg[x] = z1;
         break;
         
      case wf_sp_menu_e.MMA:
         if (z > 255) z = 255; if (z < 0) z = 0;
         spec.avg[x] = ((spec.avg[x] * (sp_param-1)) + z) / sp_param;
         z = spec.avg[x];
         break;
         
      case wf_sp_menu_e.EMA:
         if (z > 255) z = 255; if (z < 0) z = 0;
         spec.avg[x] += (z - spec.avg[x]) / sp_param;
         z = spec.avg[x];
         break;
         
      case wf_sp_menu_e.OFF:
      default:
         if (z > 255) z = 255; if (z < 0) z = 0;
         break;
      }

      if (z > spec.peak[x]) spec.peak[x] = z; 

      // draw the spectrum based on the spectrum colormap which should
      // color the 10 dB bands appropriately
      y = Math.round((1 - z/255) * sh);

      if (spectrum_slow_dev) {
         ctx.fillRect(x,y, 1,sh-y);
      } else {
         // fixme: could optimize amount of data moved like smeter
         ctx.putImageData(spec.colormap, x,0, 0,y, 1,sh-y+1);
      }
   }
   
   if (spec.peak_show) {
      ctx.lineWidth = 1;
      //ctx.fillStyle = 'yellow';
      ctx.strokeStyle = 'yellow';
      ctx.beginPath();
      y = Math.round((1 - spec.peak[0]/255) * sh) - 1;
      ctx.moveTo(0,y);
      for (x=1; x<sw; x++) {
         y = Math.round((1 - spec.peak[x]/255) * sh) - 1;
         ctx.lineTo(x,y);
      }
      ctx.globalAlpha = 0.55;
      //ctx.fill();
      ctx.stroke();
      ctx.globalAlpha = 1;
   }
}


////////////////////////////////
// waterfall
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
   if (!okay_wf_init) {
      return;
   }
   
	init_wf_container();

   wf.audioFFT_active = (rx_chan >= wf_chans);
	resize_waterfall_container(false);
	resize_wf_canvases();
	bands_init();
	panels_setup();
	ident_init();
	scale_setup();
	mkcolormap();
   mkscale();
	spectrum_init();
	dx_schedule_update();
	users_init( { user:1 } );
	stats_init();
	check_top_bar_congestion();
	if (spectrum_show) setTimeout(spec_show, 2000);    // after control instantiation finishes
   
	audioFFT_setup();

	waterfall_ms = 900/wf_fps_max;
	waterfall_timer = window.setInterval(waterfall_dequeue, waterfall_ms);
	//console.log('waterfall_dequeue @ '+ waterfall_ms +' msec');
	
	if (shortcut.keys != '') setTimeout(keyboard_shortcut_url_keys, 3000);
	
	wf_snap(kiwi_localStorage_getItem('wf_snap'));

   if (kiwi_isMobile() || mobile_laptop_test)
      mobile_init();
   
   dx_init();

	waterfall_setup_done=1;
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
	add_canvas_listner(new_canvas);
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
		if (kiwi_gc_wf) remove_canvas_listner(p);	// gc
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

function resize_wf_canvases(zoom)
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

   // above width change clears canvas, so redraw
   if (wf.audioFFT_active) {
      w3_innerHTML('id-annotation-div',
         w3_div('w3-section w3-container',
            w3_text('w3-large|color:cyan', 'Audio FFT<br>'),
            w3_text('w3-small|color:cyan',
                  'Zoom waterfall not available<br>' +
                  (wf.no_wf?
                     'when \"no_wf\" URL option used.<br>'
                  :
                     ('on channels '+ wf_chans_real +'-'+ (rx_chans-1) +' of Kiwis<br>' +
                     'configured for '+ rx_chans +' channels.<br>')
                  )
            ),
            w3_text('w3-small|color:cyan',
               'For details see the Kiwi forum.'
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

function waterfall_timestamp()
{
   var tstamp = (wf.ts_tz == 0)? (new Date()).toUTCString().substr(17,8) : (new Date()).toString().substr(16,8);
   var al = wf_canvas_actual_line;

   var c = wf_cur_canvas;
   var off = 12;
	w3_fillText_shadow(c, tstamp, off, al+off, 'Arial', 14, 'lime');

   if (al+10 > c.height)  {      // overlaps end of canvas
      var c2 = wf_canvases[1];
      if (c2) w3_fillText_shadow(c2, tstamp, off, al-c.height+off, 'Arial', 14, 'lime');
   } 
}

function wf_snap(set)
{
   //console.log('wf_snap cur='+ owrx.wf_snap +' set='+ set);
   owrx.wf_snap = isUndefined(set)? (owrx.wf_snap ^ 1) : (isNull(set)? 0 : (+set));
   //console.log('wf_snap new='+ owrx.wf_snap);
   w3_el('id-waterfall-container').style.cursor = owrx.wf_cursor = owrx.wf_snap? 'col-resize' : 'crosshair';
   kiwi_localStorage_setItem('wf_snap', owrx.wf_snap);
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
	
	if (audioFFT == 0) {
      var u32View = new Uint32Array(data_raw, 4, 3);
      var x_bin_server = u32View[0];		// bin & zoom from server at time data was queued
      var u32 = u32View[1];
      if (kiwi_gc_wf) u32View = null;	// gc
      var x_zoom_server = u32 & 0xffff;
      var flags = (u32 >> 16) & 0xffff;
      var wf_flags = { COMPRESSED:1, NO_SYNC:2 };
   
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
      
      if (flags & wf_flags.COMPRESSED) {
         decomp_data = new Uint8Array(bytes*2);
         var wf_adpcm = { index:0, previousValue:0 };
         decode_ima_adpcm_e8_u8(data_arr_u8, decomp_data, bytes, wf_adpcm);
         var ADPCM_PAD = 10;
         data = decomp_data.subarray(ADPCM_PAD);
      } else {
         data = data_arr_u8;
      }
      
      wf.no_sync = (flags & wf_flags.NO_SYNC);
      
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
      if (spec.source_wf && spec.need_clear_avg) {
         spec.clear_avg = true;
         spec.need_clear_avg = false;
      }
   }
	
	// spectrum
	if (spec.source_wf && spec.update != spec.last_update) {
	   spectrum_update(data);
	}

   // waterfall
	var oneline_image = canvas.oneline_image;

   for (x=0; x<w; x++) {
      z = color_index(wf_gnd? wf_gnd_value : data[x], wf.sqrt);
      
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
         
      case wf_sp_menu_e.OFF:
      default:
         break;
      }

      /*
      var color = color_map[z];

      for (var i=0; i<4; i++) {
         oneline_image.data[x*4+i] = ((color>>>0) >> ((3-i)*8)) & 0xff;
      }
      */
      oneline_image.data[x*4  ] = color_map_r[z];
      oneline_image.data[x*4+1] = color_map_g[z];
      oneline_image.data[x*4+2] = color_map_b[z];
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
      if (sb_trace) console.log('WF fixup bin='+x_bin+'/'+x_bin_server+' z='+zoom_level+'/'+x_zoom_server);
   
      // need to fix zoom before fixing the pan
      if (zoom_level != x_zoom_server) {
         var dz = zoom_level - x_zoom_server;
         var out = dz < 0;
         var dbins = out? (x_bin_server - x_bin) : (x_bin - x_bin_server);
         var pixel_dx = bins_to_pixels(2, dbins, out? zoom_level:x_zoom_server);
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
   
   if (wf.need_autoscale > 1) wf.need_autoscale--;
   
	if (wf.need_autoscale == 1 && !fixup) {
	   var pwr_dBm = [];
      var autoscale = wf.audioFFT_active? data.slice(256, 768) : data;
	   var alen = autoscale.length;
	   
	   // convert from transmitted values to true dBm
	   var j = 0;
	   for (var i = 0; i < alen; i++) {
	      var pwr = dB_wire_to_dBm(autoscale[i]);
	      if (pwr <= -190) continue;    // disregard masked areas
	      pwr_dBm[j] = pwr;
	      j++
      }
      var len = pwr_dBm.length;
      if (len) {
         pwr_dBm.sort(function(a,b) {return a-b});
         var noise = pwr_dBm[Math.floor(0.50 * len)];
         var signal = pwr_dBm[Math.floor(0.95 * len)];
         console_log_dbgUs('# autoscale len='+ len +' min='+ pwr_dBm[0] +' noise='+ noise +' signal='+ signal +' max='+ pwr_dBm[len-1]);

         var _10 = pwr_dBm[Math.floor(0.10 * len)];
         var _20 = pwr_dBm[Math.floor(0.20 * len)];
         console_log_dbgUs('# autoscale min='+ pwr_dBm[0] +' 10%='+ _10 +' 20%='+ _20 +' 50%(noise)='+ noise +' 95%(signal)='+ signal +' max='+ pwr_dBm[len-1]);
      } else {
         signal = -110;
         noise = -120;
      }
	   signal += 30;
	   if (signal < -80) signal = -80;
      noise -= 10;
      
      if (wf.audioFFT_active) {
         noise = (dbgUs && devl.p4)? Math.round(devl.p4) : -110;
         console_log_dbgUs('audioFFT_active: force noise = '+ noise.toFixed(0) +' dBm');
      }

      setmaxdb(1, signal);
      setmindb(1, noise);
      update_maxmindb_sliders();
	   wf.need_autoscale = 0;
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
	if (!i_dx) return;

	wf_canvases.forEach(function(cv) {
		waterfall_pan(cv, -1, i_dx);
	});
	
	wf_send("SET zoom="+ zoom_level +" start="+ x_bin);
	
	mkscale();
	need_clear_wf_sp_avg = true;
	dx_schedule_update();
   extint_environment_changed( { waterfall_pan:1, passband_screen_location:1 } );

	// reset "select band" menu if freq is no longer inside band
	//console.log('page_scroll PBV='+ passband_visible() +' freq_car_Hz='+ freq_car_Hz);
	if (passband_visible() < 0)
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

// window:
//		top container
//		non-waterfall container
//		waterfall container

function waterfall_height()
{
	var top_height = html("id-top-container").clientHeight;
	var non_waterfall_height = html("id-non-waterfall-container").clientHeight;
	var scale_height = html("id-scale-container").clientHeight;
	owrx.scale_offsetY = top_height + non_waterfall_height - scale_height;

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
		waterfall_scrollable_height = wf_height * wf.scroll_multiple;

      // Don't change the height because that clears the canvas.
      // Instead just pick a large initial height value and depend on the draw clipping.
		//canvas_annotation.height = wf_height;
		//canvas_annotation.style.height = px(wf_height);
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
	return (dBm + cfg.waterfall_cal);
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
      
         case 0:
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
// audio FFT
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

   if (wf.aper != kiwi.aper_e.auto) {
      var last_AF_max_dB = readCookie('last_AF_max_dB', maxdb);
      var last_AF_min_dB = readCookie('last_AF_min_dB', mindb_un);
      setmaxdb(1, last_AF_max_dB);
      setmindb(1, last_AF_min_dB);
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
   
      // colormap auto
      if (wf.aper == kiwi.aper_e.auto) {
         if (wf.audioFFT_active) wf.need_autoscale = 16;    // delay a bit before autoscaling
      }
   }
}

function wf_audio_FFT(audio_data, samps)
{
   if (!wf.audioFFT_active || !isDefined(cur_mode)) return;
   
   if (!kiwi_load_js_polled(afft.audioFFT_dynload, ['pkgs/js/Ooura_FFT32.js'])) return;
   
   var i, j, k, ki;
   
   //console.log('iq='+ audio_mode_iq +' comp='+ audio_compression +' samps='+ samps);

   var iq = ext_is_IQ_or_stereo_curmode();
   var lsb = (cur_mode.substr(0,2) == 'ls' || cur_mode == 'sal');
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
      
      // do initial autoscale in case stored max/min values are unreasonable
      if (!afft.init && wf.aper != kiwi.aper_e.auto)
         wf.need_autoscale = 16;
      
      afft.iq = iq;
      afft.comp = audio_compression;
      afft.init = true;
   }

   if (fft_setup || (lsb != afft.prev_lsb)) {
      for (i = 0; i < 1024; i++) afft.pwr_dB[i] = 0;
      afft.prev_lsb = lsb;
   }

   if (afft.iq) {
      for (i = 0, j = 0; i < 1024; i += 2, j++) {
         afft.i_re[j] = audio_data[i] * afft.window_512[j];
         afft.i_im[j] = audio_data[i+1] * afft.window_512[j];
      }
      afft.offt.fft(afft.offt, afft.i_re.buffer, afft.i_im.buffer, afft.o_re.buffer, afft.o_im.buffer);
      for (j = 0, k = 512; j < 256; j++, k++) {
         var re = afft.o_re[j], im = afft.o_im[j];
         var pwr = re*re + im*im;
         var dB = 10.0 * Math.log10(pwr * afft.scale + 1e-30);
         dB = Math.round(255 + dB);
         afft.pwr_dB[k] = dB;
      }
      for (j = 256, k = 256; j < 512; j++, k++) {
         var re = afft.o_re[j], im = afft.o_im[j];
         var pwr = re*re + im*im;
         var dB = 10.0 * Math.log10(pwr * afft.scale + 1e-30);
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
            if (lsb)
               k = 512, ki = -1;
            else
               k = 256, ki = +1;
            for (j = 0; j < 1024; j++) {
               var re = afft.o_re[j], im = afft.o_im[j];
               var pwr = re*re + im*im;
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
            if (lsb)
               k = 512, ki = -1;
            else
               k = 256, ki = +1;
            for (j = 0; j < 512; j++, k += ki) {
               var re = afft.o_re[j], im = afft.o_im[j];
               var pwr = re*re + im*im;
               afft.pwr_dB[k] = Math.round(255 + (10.0 * Math.log10(pwr * afft.scale + 1e-30)));
            }
            waterfall_queue.push({ data:afft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
      
            for (i = 1024; i < 2048; i++) {
               afft.i_re[i] = audio_data[i] * afft.window_2k[i-1024];
            }
            afft.offt.fft(afft.offt, afft.i_re.buffer, afft.o_re.buffer, afft.o_im.buffer);
            if (lsb)
               k = 512, ki = -1;
            else
               k = 256, ki = +1;
            for (j = 0; j < 512; j++, k += ki) {
               var re = afft.o_re[j], im = afft.o_im[j];
               var pwr = re*re + im*im;
               afft.pwr_dB[k] = Math.round(255 + (10.0 * Math.log10(pwr * afft.scale + 1e-30)));
            }
         }
         waterfall_queue.push({ data:afft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
      } else {
         for (i = 0; i < 512; i++) {
            afft.i_re[i] = audio_data[i] * afft.window_512[i];
         }
         afft.offt.fft(afft.offt, afft.i_re.buffer, afft.o_re.buffer, afft.o_im.buffer);
         if (lsb)
            k = 512, ki = -2;
         else
            k = 256, ki = +2;
         for (j = 0; j < 256; j++, k += ki) {
            var re = afft.o_re[j], im = afft.o_im[j];
            var pwr = re*re + im*im;
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
// ui
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
	freqstep
	WF shift-click (nearest boundary)
	
*/

var freq_displayed_Hz, freq_displayed_kHz_str, freq_car_Hz;
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

function passband_offset_dxlabel(mode)
{
	var pb = cfg.passbands[mode];
	var m = mode.substr(0,2);
	var usePBCenter = (m == 'us' || m == 'ls');
	var offset = usePBCenter? pb.lo + (pb.hi - pb.lo)/2 : 0;
	//console.log("passband_offset: m="+mode+" use="+usePBCenter+' o='+offset);
	return offset;
}


////////////////////////////////
// frequency entry
////////////////////////////////

function freqmode_set_dsp_kHz(fdsp, mode, opt)
{
   var dont_clear_wf = w3_opt(opt, 'dont_clear_wf', false);
   var open_ext = w3_opt(opt, 'open_ext', false);
   var no_set_freq = w3_opt(opt, 'no_set_freq', 0);

	fdsp *= 1000;
	//console.log("freqmode_set_dsp_kHz: fdsp="+fdsp+' mode='+mode);
	if (dont_clear_wf == false) wf.audioFFT_clear_wf = true;

	if (isArg(mode) && (mode != cur_mode || open_ext == true)) {
		//console.log("freqmode_set_dsp_kHz: calling demodulator_analog_replace");
		ext_set_mode(mode, fdsp, opt);
	} else {
		freq_car_Hz = freq_dsp_to_car(fdsp);
		if (!no_set_freq) {
         //console.log('freqmode_set_dsp_kHz: demodulator_set_offset_frequency NEW freq_car_Hz=' +freq_car_Hz +' no_set_freq='+ no_set_freq);
         demodulator_set_offset_frequency(0, freq_car_Hz - center_freq);
      }
	}
}

function freqset_car_Hz(fcar)
{
	freq_car_Hz = fcar;
	//console.log("freqset_car_Hz: NEW freq_car_Hz="+fcar);
}

var freq_dsp_set_last;

function freqset_update_ui()
{
	//console.log('FUPD-UI freq_car_Hz='+ freq_car_Hz +' cf+of='+(center_freq + demodulators[0].offset_frequency));
	//console.log('FUPD-UI center_freq='+ center_freq +' offset_frequency='+ demodulators[0].offset_frequency);
	//kiwi_trace();
	freq_displayed_Hz = freq_car_to_dsp(freq_car_Hz);
   freq_displayed_kHz_str = (freq_displayed_Hz/1000).toFixed(2);
   //console.log("FUPD-UI freq_car_Hz="+freq_car_Hz+' NEW freq_displayed_Hz='+freq_displayed_Hz);
	
	if (!waterfall_setup_done) return;
	
	var obj = w3_el('id-freq-input');
	if (isUndefined(obj) || obj == null) return;		// can happen if SND comes up long before W/F

   var f_with_freq_offset = freq_displayed_Hz + kiwi.freq_offset_Hz;
   var freq_displayed_kHz_str_with_freq_offset = (f_with_freq_offset/1000).toFixed((f_with_freq_offset > 100e6)? 1:2);
   obj.value = freq_displayed_kHz_str_with_freq_offset;

	//console.log("FUPD obj="+ typeof(obj) +" val="+ obj.value);
	freqset_select();
	
	// re-center if the new passband is outside the current waterfall 
	var pb_bin = -passband_visible() - 1;
	//console.log("RECEN pb_bin="+pb_bin+" x_bin="+x_bin+" f(x_bin)="+bin_to_freq(x_bin));
	if (pb_bin >= 0) {
		var wf_middle_bin = x_bin + bins_at_cur_zoom()/2;
		//console.log("RECEN YES pb_bin="+pb_bin+" wfm="+wf_middle_bin+" dbins="+(pb_bin - wf_middle_bin));
		waterfall_pan_canvases(pb_bin - wf_middle_bin);		// < 0 = pan left (toward lower freqs)
	}
	
	// reset "select band" menu if freq is no longer inside band
	check_band();
	
	writeCookie('last_freq', freq_displayed_kHz_str_with_freq_offset);
	freq_dsp_set_last = freq_displayed_kHz_str_with_freq_offset;
	//console.log('## freq_dsp_set_last='+ freq_dsp_set_last);

	freq_step_update_ui();
	freq_link_update();
	
	// update history list
   //console.log('freq_memory update');
   //console.log(freq_memory);
	if (freq_displayed_kHz_str_with_freq_offset != freq_memory[freq_memory_pointer]) {
	   freq_memory.push(freq_displayed_kHz_str_with_freq_offset);
	   //console.log('freq_memory push='+ freq_displayed_kHz_str_with_freq_offset);
	}
	if (freq_memory.length > 25) freq_memory.shift();
	freq_memory_pointer = freq_memory.length-1;
   //console.log('freq_memory ptr='+ freq_memory_pointer);
   //console.log(freq_memory);
	writeCookie('freq_memory', JSON.stringify(freq_memory));
}

// using keydown event allows key autorepeat to work
function freqset_keydown(event)
{
   if (event.keyCode == '38') {  // up-arrow
      freq_memory_up_down(1);
   }
   else if (event.keyCode == '40') { // down-arrow
      freq_memory_up_down(0);
   }
}

var freq_up_down_timeout = -1;

function freq_memory_up_down(up)
{
   kiwi_clearTimeout(freq_up_down_timeout);

   var obj = w3_el('id-freq-input');
   if (up) {
      if (freq_memory_pointer > 0) --freq_memory_pointer;
      if (freq_memory[freq_memory_pointer]) obj.value = freq_memory[freq_memory_pointer];
   } else {
      if (freq_memory_pointer < freq_memory.length-1) ++freq_memory_pointer; else return;
      if (freq_memory[freq_memory_pointer]) obj.value = freq_memory[freq_memory_pointer];
   }
   //console.log(freq_memory);
   //console.log('### freq_memory_up_down '+ (up? 'UP':'DOWN') +' len='+ freq_memory.length +' ptr='+ freq_memory_pointer +' f='+ freq_memory[freq_memory_pointer]);

   freq_up_down_timeout = setTimeout(function() {
      if (!kiwi_isMobile()) {
         // revert to current frequency if no selection made
         freq_memory_pointer = freq_memory.length-1;
         w3_el('id-freq-input').value = freq_dsp_set_last;
      }
      
      freqset_complete('u/d t/o');
   }, kiwi_isMobile()? 2000:3000);
}

function freqset_select()
{
   // only switch focus to id-freq-input if active element doesn't specify w3-retain-input-focus (or w3-ext-retain-input-focus)
   var ae = document.activeElement;
   //console.log(ae);
   
   // extensions use w3-ext-retain-input-focus instead of w3-retain-input-focus so we can handle the following situation:
   var closed_ext_input_still_holding_focus = (ae && w3_contains(ae, 'w3-ext-retain-input-focus') && !extint.displayed);
   if (closed_ext_input_still_holding_focus) {
      //console.log('#### closed_ext_input_still_holding_focus');
   }

   var retain_input_focus = (ae && (w3_contains(ae, 'w3-retain-input-focus') || w3_contains(ae, 'w3-ext-retain-input-focus')));
   if (retain_input_focus) {
      //console.log('#### retain_input_focus');
   }
   
   if (extint.scanning) {
      //console.log('freqset_select: scanning, so skip id-freq-input select');
      return;
   }

   if (!ae || !retain_input_focus || closed_ext_input_still_holding_focus) {
	   w3_field_select('id-freq-input', {mobile:1});
	} else {
      //console.log('#### activeElement w3-retain-input-focus:');
      //console.log(ae);
	}
}

function modeset_update_ui(mode)
{
	if (owrx.last_mode_el != null) owrx.last_mode_el.style.color = "white";
	
	// if sound comes up before waterfall then the button won't be there
	var m = mode.substr(0,2);
	if (m == 'qa') m = 'sa';   // QAM -> SAM case
	var el = w3_el('id-mode-'+ m);
	el.innerHTML = mode.toUpperCase();
	if (el && el.style) el.style.color = "lime";
	owrx.last_mode_el = el;

	squelch_setup(toggle_e.FROM_COOKIE);
	writeCookie('last_mode', mode);
	freq_link_update();

	// disable compression button in iq or stereo modes
	var disabled = ext_is_IQ_or_stereo_mode(mode);
   w3_els('id-button-compression',
      function(el) {
	      w3_set_props(el, 'w3-disabled', disabled);
      });

   w3_hide2('id-sam-carrier-container', m != 'sa');  // also QAM
   w3_hide2('id-chan-null', mode != 'sam');
}

// delay the UI updates called from the audio path until the waterfall UI setup is done
function try_freqset_update_ui()
{
	if (waterfall_setup_done) {
		freqset_update_ui();
		if (wf.audioFFT_active) {
         if (cur_mode != wf.audioFFT_prev_mode)
            wf.audioFFT_clear_wf = true;
		   audioFFT_update();
		   wf.audioFFT_prev_mode = cur_mode;
		} else {
		   mkenvelopes(get_visible_freq_range());
		}
	} else {
		setTimeout(try_freqset_update_ui, 1000);
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
	var url = host +'/?f='+ freq_displayed_kHz_str + cur_mode +'z'+ zoom_level;
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

function freqset_complete(from)
{
	var obj = w3_el('id-freq-input');
	//console.log("FCMPL from="+ from +" obj="+ typeof(obj) +" val="+ (obj.value).toString());
	kiwi_clearTimeout(freqset_tout);
   kiwi_clearTimeout(freq_up_down_timeout);
	if (isUndefined(obj) || obj == null) return;		// can happen if SND comes up long before W/F

   var p = obj.value.split(/[\/:]/);
	var slash = obj.value.includes('/');
   // 'k' suffix is simply ignored since default frequencies are in kHz
	var f = p[0].replace(',', '.').parseFloatWithUnits('M', 1e-3);    // Thanks Petri, OH1BDF
	var err = true;

   if (obj.value == '/') {
      //console.log('restore_passband '+ cur_mode);
      restore_passband(cur_mode);
      demodulator_analog_replace(cur_mode);
      freqset_update_ui();    // restore previous
      return;
   } else
	if (f > 0 && !isNaN(f)) {
	   f -= kiwi.freq_offset_kHz;
	   if (f > 0 && !isNaN(f)) {
         freqmode_set_dsp_kHz(f, null);
	      w3_field_select(obj, {mobile:1});
	      err = false;
      }
	}
   if (err) freqset_update_ui();    // restore previous
	
	// accept "freq/pbw" or "/pbw" to quickly change passband width to a numeric value
	// also "lo,hi" in place of "pbw"
	// and ":pbc" or ":pbc,pbw" to set the pbc at the current pbw
	// "/" alone resets to default passband
	if (p[1]) {
	   p2 = p[1].split(',');
	   var lo = p2[0].parseFloatWithUnits('k');
	   var hi = NaN;
	   if (p2.length > 1) {
	      hi = p2[1].parseFloatWithUnits('k');
	   }
	   //console.log('### <'+ (slash? '/' : ':') +'> '+ p2[0] +'/'+ lo +', '+ p2[1] +'/'+ hi);
      var cpb = ext_get_passband();
      var cpbhw = (cpb.high - cpb.low)/2;
      var cpbcf = cpb.low + cpbhw;
	   
      if (slash) {
         // adjust passband width about current pb center
         if (p2.length == 1) {
            // /pbw
            var pbhw = lo/2;
            lo = cpbcf - pbhw, hi = cpbcf + pbhw;
         }
      } else {
         // adjust passband center using current or specified pb width
         var pbc = lo;
         if (p2.length == 1) {
            // :pbc
            lo = pbc - cpbhw;
            hi = pbc + cpbhw;
         } else {
            var pbhw = Math.abs(hi/2);
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
// Also, keyup is used instead of keydown because the global id-kiwi-body has an eventlistener for keydown
// to implement keyboard shortcut event interception before the keyup event of freqset (i.e. sequencing).
// We use w3-custom-events in the freq box w3_input() to allow both onkeydown and onkeyup event handlers.
// freqset_keydown() called by onkeydown is used to implement key autorepeat of history arrow keys.

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
				freqset_update_ui();
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
   
	freqset_tout = setTimeout(function() {freqset_complete('t/o');}, 3000);
}

var num_step_buttons = 6;

var up_down = {
   // 0 means use special step logic
	am:   [ 0, -1, -0.1, 0.1, 1, 0 ],
	amn:  [ 0, -1, -0.1, 0.1, 1, 0 ],
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
	iq:   [ 0, -1, -0.1, 0.1, 1, 0 ],
};

var NDB_400_1000_mode = 1;		// special 400/1000 step mode for NDB band

// nearest appropriate boundary (1, 5 or 9/10 kHz depending on band & mode)
function freq_step_amount(b)
{
   var cm = cur_mode.substr(0,2);
   var ssb_cw = (cm == 'ls' || cm == 'us' || cm == 'cw');
	var step_Hz = ssb_cw? 1000 : 5000;
	var s = ssb_cw? ' 1k default' : ' 5k default';
	var am_sax_iq_drm = (cm == 'am' || cm == 'sa' || cm == 'qa' || cm == 'ls' || cm == 'us' || cm == 'iq' || cm == 'dr');
   var ITU_region = cfg.init.ITU_region + 1;
   var ham_80m_swbc_75m_overlap = (ITU_region == 2 && b && b.name == '75m');

	if (b && b.name == 'NDB') {
		if (cm == 'cw') {
			step_Hz = NDB_400_1000_mode;
		}
		s = ' NDB';
	} else
	if (b && (b.name == 'LW' || b.name == 'MW')) {
		//console_log('special step', cm, am_sax_iq_drm);
		if (am_sax_iq_drm) {
			step_Hz = step_9_10? 9000 : 10000;
		}
		s = ' LW/MW';
	} else
   if (b && band_svc_lookup(b.svc).o.name.includes('Broadcast') && !ham_80m_swbc_75m_overlap) {      // SWBC bands
      if (am_sax_iq_drm) {
         step_Hz = 5000;
         s = ' SWBC 5k';
         //console.log('SFT-CLICK SWBC');
      }
	} else
	if (b && b.chan != 0) {
		step_Hz = b.chan;
		s = ' band='+ b.name +' chan='+ b.chan;
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

function freqstep(sel)
{
	var step_Hz = up_down[cur_mode][sel]*1000;
	
	// set step size from band channel spacing
	if (step_Hz == 0) {
		var b = find_band(freq_displayed_Hz);
		step_Hz = special_step(b, sel, 'freqstep');
	}

	var fnew = freq_displayed_Hz;
	var incHz = Math.abs(step_Hz);
	var posHz = (step_Hz >= 0)? 1:0;
	var trunc = fnew / incHz;
	trunc = (posHz? Math.ceil(trunc) : Math.floor(trunc)) * incHz;
	var took;

	if (incHz == NDB_400_1000_mode) {
		var kHz = fnew % 1000;
		if (posHz)
			kHz = (kHz < 400)? 400 : ( (kHz < 600)? 600 : 1000 );
		else
			kHz = (kHz == 0)? -400 : ( (kHz <= 400)? 0 : ( (kHz <= 600)? 400 : 600 ) );
		trunc = Math.floor(fnew/1000)*1000;
		fnew = trunc + kHz;
		took = '400/1000';
		//console.log("STEP -400/1000 kHz="+kHz+" trunc="+trunc+" fnew="+fnew);
	} else
	if (freq_displayed_Hz != trunc) {
		fnew = trunc;
		took = 'TRUNC';
	} else {
		fnew += step_Hz;
		took = 'INC';
	}
	//console.log('STEP '+sel+' '+cur_mode+' fold='+freq_displayed_Hz+' inc='+incHz+' trunc='+trunc+' fnew='+fnew+' '+took);
	
	// audioFFT mode: don't clear waterfall for small frequency steps
	freqmode_set_dsp_kHz(fnew/1000, null, { dont_clear_wf:true });
}

var freq_step_last_mode, freq_step_last_band;

function freq_step_update_ui(force)
{
	if (isUndefined(cur_mode) || cfg.passbands[cur_mode] == undefined ) return;
	var b = find_band(freq_displayed_Hz);
	
	//console.log("freq_step_update_ui: lm="+freq_step_last_mode+' cm='+cur_mode);
	if (!force && freq_step_last_mode == cur_mode && freq_step_last_band == b) {
		//console.log("freq_step_update_ui: return "+freq_step_last_mode+' '+cur_mode);
		return;
	}

   var cm = cur_mode.substr(0,2);
   var am_sax_iq_drm = (cm == 'am' || cm == 'sa' || cm == 'qa' || cm == 'iq' || cm == 'dr');
	var show_9_10 = (b && (b.name == 'LW' || b.name == 'MW') && am_sax_iq_drm)? true:false;
	w3_visible('id-9-10-cell', show_9_10);

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
		html('id-step-'+i).title = title;
	}
	
	freq_step_last_mode = cur_mode;
	freq_step_last_band = b;
}


////////////////////////////////
// band scale
// band bars
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
	return { ITU_region:ITU_region, _9_10:_9_10, LW_lo:LW_lo, NDB_lo:NDB_lo, NDB_hi:NDB_hi, MW_hi:MW_hi }
}

// for a particular cfg.band_svc.key return its object element and index
function band_svc_lookup(svc_key)
{
   var idx = null;
   cfg.band_svc.forEach(function(a, i) {
      if (a.key == svc_key)
         idx = i;
   });
   if (idx == null) {
      console.log('$$$ band_svc_lookup NO KEY <'+ svc_key +'>');
      kiwi_trace();
      return null;
   }
   return { o: cfg.band_svc[idx], i: idx };
}

// initialize kiwi.cfg_bands_new[] from cfg.bands
// also convert from config.js bands[] if necessary
function bands_init()
{
	var i, j, k, z, b, bnew;
	
	if (cfg.bands && cfg.band_svc && cfg.dx_type) {   // already converted to saving in the configuration file
	   console.log('BANDS: using stored cfg.bands');
	   bands_addl_info();
	   dx_color_init();
	   return;
	}
	
   console.log('BANDS: using old config.js::bands');


   // do this here even though it's related to the dx labels
   cfg.dx_type = [];
   var max_i = -1;
   dx.legacy_stored_types.forEach(function(a, i) {
      cfg.dx_type[i] = { key: i, name: dx.legacy_stored_types[i], color: dx.legacy_stored_colors[i] };
      max_i = i;
   });
   j = dx_type2idx(dx.DX_MASKED);
   for (i = max_i + 1; i < j; i++) {
      cfg.dx_type[i] = { key: i, name: '', color: '' };
   }
   cfg.dx_type[j] = { key: i, name: 'masked', color: 'lightGrey' };
   dx_color_init();
   
   
   // config.js::svc{} may have been modified by the admin, so migrate to cfg also
   cfg.band_svc = [];
   w3_obj_enum(svc, function(key, i, o) {
      if (key == 'X' && o.name == 'Beacons') {
         svc.L = svc.X;
         key = 'L';
         delete svc.X;
      }
      console.log('key='+ key);
      svc[key].key = key;     // to assist in conversion of bands[] later on
      
      cfg.band_svc[i] = { key: key, name: o.name, color: o.color };
      if (o.name == 'Industrial/Scientific') {
         cfg.band_svc[i].name = 'ISM';
         cfg.band_svc[i].longName = 'Industrial/Scientific';
      }
   });
   //console.log(cfg.band_svc);
   
   
	kiwi.cfg_bands_new = [];

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
		
		   // give kiwi.cfg_bands_new[] separate entry per region of a single band[] entry
		   var len = b.region.length;
		   if (len == 1) {
            kiwi.cfg_bands_new[j] = kiwi_shallow_copy(b);
            bnew = kiwi.cfg_bands_new[j];
            j++;
            var c = bnew.region.charAt(0);
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
            console.log('R'+ c +'='+ bnew.itu +' '+ dq(b.name));
            console.log(bnew);
		   } else {
            for (k = 0; k < len; k++) {
               var c = b.region.charAt(k);
               if (c == '>') continue;    // '>' alone is detected above in (len == 1)
               kiwi.cfg_bands_new[j] = kiwi_shallow_copy(b);
               bnew = kiwi.cfg_bands_new[j];
               j++;
               if (c >= '1' && c <= '3')
                  bnew.itu = ord(c, '1') + 1;
               else {
                  bnew.itu = kiwi.BAND_SCALE_ONLY;
               }
               console.log('R'+ c +'='+ bnew.itu +' '+ dq(b.name));
               console.log(bnew);
            }
         }
		} else {
		   // shouldn't happen but handle anyway
		   kiwi.cfg_bands_new[j] = kiwi_shallow_copy(b);
		   bnew = kiwi.cfg_bands_new[j];
		   bnew.itu = kiwi.ITU_ANY;
		   j++;
         console.log('DEFAULT ITU_ANY='+ bnew.itu +' '+ dq(b.name));
         console.log(bnew);
		}
		
		if (isNumber(bnew.sel)) bnew.sel = bnew.sel.toFixed(2);
		
		// Add prefix to IBP names to differentiate from ham band names.
		// Change IBP passband from CW to CWN.
		// A software release can't modify bands[] definition in config.js so do this here.
		// At some point config.js will be eliminated when admin page gets equivalent UI.
		if ((bnew.s.key == 'L' || bnew.s.key == 'X') && bnew.sel.includes('cw') && bnew.region == 'm') {
		   if (!bnew.name.includes('IBP'))
		      bnew.name = 'IBP '+ bnew.name;
		   if (!bnew.sel.includes('cwn'))
		      bnew.sel = bnew.sel.replace('cw', 'cwn');
		}
		if (bnew.name.includes('IBP'))
		   bnew.s = 'L';    // svc.L is not defined in older config.js files

	}

	for (i = 0; i < kiwi.cfg_bands_new.length; i++) {
		bnew = kiwi.cfg_bands_new[i];

		if (isString(bnew.s)) bnew.svc = bnew.s;
		else
		if (isObject(bnew.s))
		   bnew.svc = bnew.s.key;
		else {
		   console.log('$$$ t/o(bnew.svc)='+ typeof(bnew.s));
		}
		
		delete bnew.s;
		delete bnew.region;
	}

   cfg.bands = [];
   
   for (i = 0; i < kiwi.cfg_bands_new.length; i++) {
      b = kiwi.cfg_bands_new[i];
      var sel = b.sel || '';
      cfg.bands.push({
         min:b.min, max:b.max, name:b.name, svc:b.svc, itu:b.itu, sel:sel, chan:b.chan
      });
   }

   console.log('kiwi.cfg_bands_new');
   console.log(kiwi.cfg_bands_new);
   bands_addl_info();
}

// augment cfg.bands[] with info not stored in configuration
function bands_addl_info()
{
   var i, z, b1, b2;
//var bi = band_info();
   var offset_kHz = kiwi.freq_offset_kHz;
   
   console.log('BANDS: creating additional info in kiwi.bands');
   kiwi.bands = [];

	for (i=0; i < cfg.bands.length; i++) {
		b1 = cfg.bands[i];
		b2 = kiwi.bands[i] = {};

		cfg.bands[i].chan = isUndefined(b1.chan)? 0 : b1.chan;

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
		   b2.longName = '';
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

	for (i = 0; i < cfg.bands.length; i++) {
		var b1 = cfg.bands[i];
		var b2 = kiwi.bands[i];

		// filter bands based on offset mode
		if (kiwi.isOffset) {
		   if (!b2.isOffset) continue;
		} else {
		   if (b2.isOffset) continue;
		}

		if (!(b1.itu == kiwi.BAND_MENU_ONLY || b1.itu == kiwi.ITU_ANY || b1.itu == ITU_region)) continue;

		if (service != b1.svc) {
			service = b1.svc; s += '<option value='+ dq(op) +' disabled>'+ band_svc_lookup(b1.svc).o.name.toUpperCase() +'</option>';
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
   console.log('mk_band_menu');
   console.log(cfg.bands);
   band_menu = [];
   owrx.last_selected_band = 0;
   w3_innerHTML('id-select-band', setup_band_menu());
}

// find_band() is only called by code related to setting the frequency step size.
// For this purpose always consider the top of the MW band ending at 1710 kHz even if the
// configured ITU region causes the band bar to display a lower frequency.
function find_band(freq)
{
	var ITU_region = cfg.init.ITU_region + 1;    // cfg.init.ITU_region = 0:R1, 1:R2, 2:R3
   //console.log('find_band f='+ freq +' ITU_region=R'+ ITU_region);

	for (var i = 0; i < cfg.bands.length; i++) {
		var b1 = cfg.bands[i];
		var b2 = kiwi.bands[i];
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

	for (i = 0; i < cfg.bands.length; i++) {
		var b1 = cfg.bands[i];
		var b2 = kiwi.bands[i];

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

      band_ctx.fillStyle = band_svc_lookup(b1.svc).o.color;
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
         ;     // long name fits in bar
      } else {
         txt = b1.name;
         mt = band_ctx.measureText(txt);
         //console.log("BB mt="+mt.width+" txt="+txt);
         if (w >= mt.width+4) {
            ;  // short name fits in bar
         } else {
            txt = null;
         }
      }
      //if (txt) console.log("BANDS tx="+(tx - mt.width/2)+" ty="+ty+" mt="+mt.width+" txt="+txt);
      if (txt) band_ctx.fillText(txt, tx - mt.width/2, ty);
	}
}

function parse_freq_mode(freq_mode)
{
   var s = new RegExp("([0-9.]*)([^&#]*)").exec(freq_mode);
}

// scroll to next/prev band menu entry, skipping null title entries
function band_scroll(dir)
{
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

function select_band(v, mode)
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
         select_band(i, mode);
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
	var freq;

	if (b1.sel != '') {
		freq = parseFloat(b1.sel);
		if (isUndefined(mode)) {
			mode = b1.sel.search(/[a-z]/i);
			mode = (mode == -1)? null : b1.sel.slice(mode);
		}
	} else {
		freq = b2.cfHz/1000;
	}

	//console.log("SEL BAND"+v_num+" "+b1.name+" freq="+freq+((mode != null)? " mode="+mode:""));
	owrx.last_selected_band = v_num;
	if (dbgUs) {
		//console.log("SET BAND cur z="+zoom_level+" xb="+x_bin);
		sb_trace=0;
	}
	freqmode_set_dsp_kHz(freq, mode);
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

function tune(fdsp, mode, zoom, zarg)
{
	ext_tune(fdsp, mode, ext_zoom.ABS, zoom, zarg);
}

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
   wf.auto_ceil.val = +initCookie('last_ceil_dB', cfg.init.ceil_dB);
   wf.auto_floor.val = +initCookie('last_floor_dB', cfg.init.floor_dB);
}


////////////////////////////////
// confirmation panel
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
	if (w == undefined) w = 525;
	if (h == undefined) h = 80;
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
// set freq offset confirmation panel
////////////////////////////////


function set_freq_offset_dialog()
{
	var s =
		w3_col_percent('/w3-text-aqua',
			w3_input_get('', 'Frequency scale offset (kHz, 1 Hz resolution)', 'cfg.freq_offset', 'w3_float_set_cfg_cb|3'), 80
		);
   confirmation_show_content(s, 525, 80);

	// put the cursor in (i.e. select) the password field
	w3_field_select('id-cfg.freq_offset', {mobile:1});
}

////////////////////////////////
// cal ADC clock confirmation panel
////////////////////////////////

var cal_adc_new_adj;

function cal_adc_dialog(new_adj, clk_diff, r1k, ppm)
{
   var s;
   var gps_correcting = (cfg.ADC_clk_corr && ext_adc_gps_clock_corr() > 3)? 1:0;
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
   ext_set_cfg_param('cfg.clk_adj', cal_adc_new_adj, true);
   confirmation_panel_close();
}


////////////////////////////////
// admin pwd panel
////////////////////////////////

function admin_pwd_query(isAdmin_true_cb)
{
	ext_hasCredential('admin', admin_pwd_cb, isAdmin_true_cb, ws_wf);
}

function admin_pwd_cb(badp, isAdmin_true_cb)
{
	console.log('admin_pwd_cb badp='+ badp);
	if (!badp) {
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
// dx labels
////////////////////////////////

var dx = {
   DB_STORED: 0,
   DB_EiBi: 1,
   db: 0,
   db_s: [ 'stored (writeable)', 'EiBi-B21 (read-only)' ],
   url_p: null,
   help: false,
   ignore_dx_update: false,

   DX_STEP: 2,
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
   stored_filter_tod: 1,
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
      'CW',       'FSK',      'Fax',
      'Aero',     'Maritime', 'Spy'
   ],
   eibi_ext: [
      '',         '',         'time',
      'ale',      'hfdl',     '',
      'cw',       'fsk',      'fax',
      '',         '',         ''
   ],
   eibi_filter_tod: 1,


   DX_MODE:       0x0000000f,    // 16 modes

   DX_TYPE:       0x000001f0,    // 32 types
   DX_TYPE_SFT: 4,

   DX_N_STORED:   16,
   DX_STORED:     0x00000000,    // stored: 0x000, 0x010, ... 0x0f0 (16)
   DX_QUICK_0:    0x00000000,
   DX_QUICK_1:    0x00000010,
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
   
   eibi_types_mask: 0xffffffff,
   
   list: [],
   displayed: [],

   filter_ident: '',
   filter_notes: '',
   filter_case: 0,
   filter_wild: 0,
   filter_grep: 0,
   ctrl_click: false,
};

function dx_init()
{
   dx.DX_DOW_BASE = dx.DX_DOW >> dx.DX_DOW_SFT;

   if ((dx.url_p = kiwi_url_param('dx', '', null)) != null) {
      dx.db = dx.DB_EiBi;
      
      // wait for panel init to occur
      w3_do_when_cond(function() { var el = w3_el('id-ext-controls'); return el? el.init : false; }, function() {
         console.log('DX url_p="'+ dx.url_p +'"');
         if (dx.url_p != '') {
            var r, p = dx.url_p.split(',');
            p.forEach(function(a, i) {
               console.log('DX url_p=<'+ a +'>');
               if (w3_ext_param('help', a).match) {
                  dx.help = true;
               } else
               if (i == 0 && a == 0) {
                  dx.db = dx.DB_STORED;
                  dx.stored_filter_tod = 0;
                  freq_displayed_kHz_str = '0';
                  cur_mode = 'am';
               } else
               if (w3_ext_param('none', a).match) {
                  dx.eibi_types_mask = 0;
                  console.log('SET none dx.eibi_types_mask='+ dx.eibi_types_mask);
               } else
               if ((r = w3_ext_param('filter', a)).match) {
                  if (dx.db == dx.DB_STORED) {
                     dx.stored_filter_tod = r.has_value? r.num : 1;
                     console.log('SET stored_filter_tod='+ dx.eibi_filter_tod +' r.has_value='+ r.has_value +' r.num='+ r.num);
                  } else {
                     dx.eibi_filter_tod = r.has_value? r.num : 1;
                     console.log('SET eibi_filter_tod='+ dx.eibi_filter_tod +' r.has_value='+ r.has_value +' r.num='+ r.num);
                  }
               } else
               w3_ext_param_array_match_str(dx.eibi_svc_s, a, function(i,a) {
                  var v = dx.eibi_types_mask & (1 << i);
                  if (v)
                     dx.eibi_types_mask &= ~(1 << i);
                  else
                     dx.eibi_types_mask |= 1 << i;
                  console.log('MATCH '+ a +' v='+ v.toHex(8) +'('+ i +') dx.eibi_types_mask='+ dx.eibi_types_mask.toHex(8));
               });
            });
         }
         
         console.log('FINAL dx.eibi_types_mask='+ dx.eibi_types_mask.toHex(8) +' dx.help='+ dx.help);
         dx_show_edit_panel(null, -1);
	      dx_schedule_update();
      }, 0, 500);
   }
}

// either type: EiBi or stored
function dx_type2idx(type)
{
   var type = type & dx.DX_TYPE;
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
   for (var i = 0; i < cfg.dx_type.length; i++) {
      var o = cfg.dx_type[i];
      var color = (o.color == '')? 'white' : o.color;
      var colorObj = w3color(color);
      if (colorObj.valid) {   // only if the color name is valid
         colorObj.lightness = dx.stored_lightness;
         dx.stored_colors_light[i] = colorObj.toHslString();
      }
   }

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

   for (var i = 0; i < dx.DX_N_EiBi; i++) {
      w3_color('eibi-cbox'+ i +'-label', 'black', dx.eibi_colors[i]);
   }
}

var dx_update_delay = 350;
var dx_update_timeout, dx_seq=0;

function dx_schedule_update()
{
	kiwi_clearTimeout(dx_update_timeout);
	dx_div.innerHTML = "";
	dx_update_timeout = setTimeout(dx_update, dx_update_delay);
}

function dx_update()
{
   var eibi = (dx.db == dx.DB_EiBi);
	dx_seq++;
	//g_range = get_visible_freq_range();
	//console_log_dbgUs("DX min="+(g_range.start/1000)+" max="+(g_range.end/1000));
	
	// turn off anti-clutter for HDFL band mode
	var anti_clutter = (ext_panel_displayed('HFDL') && dx_is_single_type(dx.DX_HFDL) && zoom_level >= 4)? 0 : 1;
	wf_send('SET MARKER db='+ dx.db +' min='+ (g_range.start/1000).toFixed(3) +' max='+ (g_range.end/1000).toFixed(3) +
	   ' zoom='+ zoom_level +' width='+ waterfall_width +' eibi_types_mask='+ dx.eibi_types_mask.toHex() +
	   ' filter_tod='+ (eibi? dx.eibi_filter_tod : dx.stored_filter_tod) +' anti_clutter='+ anti_clutter);

	// refresh every 5 minutes to catch schedule changes
   kiwi_clearTimeout(dx.dx_refresh_timeout);
	//if ((eibi && dx.eibi_filter_tod) || (!eibi && dx.stored_filter_tod)) {
	if (1) {    // so can observe change from dashed/lighter to solid/darker when not filtered
	   var _5_min_ms = 5*60*1e3;
	   //var _5_min_ms = 60*1e3;    // #ifdef TEST_DX_TIME_REV
	   var ms_delay = 2e3;        // wait for boundary to pass before checking
	   var ms_rem = _5_min_ms - (Date.now() % _5_min_ms);
	   //console_log_dbgUs('$ SET dx_refresh_timeout rem='+ (ms_rem/1e3).toFixed(3));
	   dx.dx_refresh_timeout = setTimeout(function() {
	      //console_log_dbgUs('$ GO dx_refresh_timeout ');
	      dx_div.innerHTML = "";
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

function dx_label_cb(arr)
{
	var i, j, k;
	var hdr = arr[0];
	var obj;
   var eibi = (dx.db == dx.DB_EiBi);
   var gap = eibi? 40 : 35;
   
   dx_color_init();

	// reply to label step request
	if (hdr.t == dx.DX_STEP) {
	   var dl = arr[1];
	   if (dl) {
	      //console_log_dbgUs('dx_label_cb: type=2 f='+ dl.f);
	      //console_log_dbgUs(dl);
	      var mode = kiwi.modes_l[dl.fl & dx.DX_MODE];
         freqmode_set_dsp_kHz(dl.f, mode);
         if (dl.lo != 0 && dl.hi != 0) {
            ext_set_passband(dl.lo, dl.hi);     // label has custom pb
         } else {
            var dpb = cfg.passbands[mode];
            ext_set_passband(dpb.lo, dpb.hi);   // need to force default pb in case cpb is not default
         }
      } else {
	      //console_log_dbgUs('dx_label_cb: type=2 NO LABEL FOUND');
      }
	   return;
	}
	
	kiwi_clearInterval(dx_ibp_interval);
	dx_ibp_list = [];
	dx_ibp_server_time_ms = hdr.s * 1000 + (+hdr.m);
	dx_ibp_local_time_epoch_ms = Date.now();
	
	if (eibi) for (i = 0; i < dx.DX_N_EiBi; i++) {
	   if (isDefined(hdr.ec[i]))
	      w3_innerHTML('eibi-cbox'+ i +'-label', dx.eibi_svc_s[i] +' ('+ hdr.ec[i] +')');
	}
	
	dx.types_n = hdr.tc;
	
	dx_filter_field_err(+hdr.f);
	
	var dx_idx, dx_z = 120;
	dx_div.innerHTML = '';
	dx.displayed = [];
	dx.last_ident = '';
	dx.last_freq = -1;
	dx.post_render = [];
	dx.color_fixup = [];
	dx.f_same = [];
   var same_x, lock_z = 0;
	var s_a = [];

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
	
	//console_log_dbgUs('######## dx_label_cb db='+ dx.db +' arr.len='+ arr.length);

	// first pass
	for (i = 1; i < arr.length; i++) {
		dx_idx = i-1;
		obj = arr[i];
      //if (obj.f == 10000) console_log_dbgUs(obj);
		var flags = obj.fl;

		var type = flags & dx.DX_TYPE;
		
      // this gives zero-based index for stored/EiBi color
      var color_idx = dx_type2idx(type);

		var filtered = flags & dx.DX_FILTERED;
		var ident = obj.i;
		if (eibi && ident == dx.last_ident) {
         //console_log_dbgUs('1111 dx_idx='+ dx_idx +' '+ obj.f +' SAME AS LAST filtered='+ (filtered? 1:0) + ident);
         if (filtered == 0) dx.color_fixup[dx.last_idx] = dx.eibi_colors[color_idx];
         continue;
		}

		dx.last_idx = dx_idx;
		dx.last_ident = ident;
      //console_log_dbgUs('1111 dx_idx='+ dx_idx +' '+ obj.f +' OK filtered='+ (filtered? 1:0) + ident);
		
		var gid = obj.g;
		var freq = obj.f;
		var moff = obj.o;
		var notes = isDefined(obj.n)? obj.n : '';
		var params = isDefined(obj.p)? obj.p : '';
		
		if (eibi && (flags & dx.DX_HAS_EXT)) params = dx.eibi_ext[dx_type2idx(type)];

		var freqHz = freq * 1000;
		var freq_label_Hz = freqHz + passband_offset_dxlabel(kiwi.modes_l[flags & dx.DX_MODE]);	// always place label at center of passband
		var x = scale_px_from_freq(freq_label_Hz, g_range) - 1;	// fixme: why are we off by 1?
		var cmkr_x = 0;		// optional carrier marker for NDBs
		var carrier = freqHz - moff;
		if (moff) cmkr_x = scale_px_from_freq(carrier, g_range);
	
		if (eibi && freq_label_Hz == dx.last_freq) {
		   dx.f_same.push(dx_idx);
		   same_x = x;
		   if (lock_z == 0) lock_z = dx_z;
		} else {
		   if (eibi) {
            if (dx.f_same.length > 2) optimize_label_layout(same_x, lock_z, dx.last_freq/1e3);
            dx.f_same = [];
            dx.f_same.push(dx_idx);
            lock_z = 0;
         }
		}
		var top = dx_label_top + (gap * (dx_idx & 1));    // stagger the labels vertically
      dx.post_render[dx_idx] = { top: top, ltop: top, x: x /*, f: freq_label_Hz/1e3, ident: ident*/ };
		dx.last_freq = freq_label_Hz;

      var color;
      if (eibi) {
         color = filtered? dx.eibi_colors_light[color_idx] : dx.eibi_colors[color_idx];
      } else {
         var c = cfg.dx_type[color_idx].color;
         if (c == '') c = 'white';
         color = filtered? dx.stored_colors_light[color_idx] : c;
      }

      // for backward compatibility, IBP color is fixed to aquamarine
      // unless type has been set non-zero (since dist.dx.json never specified a type)
		if (!eibi && color_idx == 0 && (ident == 'IBP' || ident == 'IARU%2fNCDXF')) {
		   color = 'aquamarine';
		}
		//console_log_dbgUs("DX "+dx_seq+':'+dx_idx+" f="+freq+" o="+loff+" k="+moff+" F="+flags+" m="+kiwi.modes_u[flags & dx.DX_MODE]+" <"+ident+"> <"+notes+'>');
		
		carrier /= 1000;
		dx.list[gid] = { "gid":gid, "carrier":carrier, "lo":obj.lo, "hi":obj.hi, "freq":freq, "moff":moff, "flags":flags, "begin":obj.b, "end":obj.e, "ident":ident, "notes":notes, "params":params };
	   //console_log_dbgUs('dx_label_cb db='+ dx.db +' dx_idx='+ dx_idx +'/'+ arr.length +' gid='+ gid +' dx.list.len='+ dx.list.length +' '+ freq.toFixed(2) +' x='+ x);
      dx.displayed[dx_idx] = dx.list[gid];
		//console_log_dbgUs(dx.list[gid]);
		var has_ext = (params != '')? ' id-has-ext' : '';
		
		s_a[dx_idx] =
			'<div id="'+ dx_idx +'-id-dx-label" class="cl-dx-label '+ (filtered? 'cl-dx-label-filtered':'') +' '+ gid +'-id-dx-gid'+ has_ext +'" ' +
			   'style="left:'+ (x-10) +'px; z-index:'+ dx_z +'; ' +
				'background-color:'+ color +';" ' +
				
				// overrides underlying canvas listeners for the dx labels
				'onmouseenter="dx_enter(this,'+ cmkr_x +', event)" onmouseleave="dx_leave(this,'+ cmkr_x +')" ' +
				'onmousedown="ignore(event)" onmousemove="ignore(event)" onmouseup="ignore(event)" ontouchmove="ignore(event)" ontouchend="ignore(event)" ' +
				'onclick="dx_click(event,'+ gid +')" ontouchstart="dx_click(event,'+ gid +')" name="'+ ((params == '')? 0:1) +'">' +
			'</div>' +
			'<div class="cl-dx-line" id="'+ dx_idx +'-id-dx-line" style="left:'+ x +'px; z-index:110"></div>';
		
		dx_z++;
	}
   if (eibi && dx.f_same.length > 2) optimize_label_layout(same_x, lock_z, -1);
	//console_log_dbgUs(dx.list);
	//console_log_dbgUs(dx.displayed);
	
	var s = '';
	for (i = 0; i < arr.length-1; i++)
	   if (isDefined(s_a[i]))
	      s += s_a[i];
	dx_div.innerHTML = s;
	dx.last_ident = '';

	if (!dx.font) {
	   var el = w3_el('0-id-dx-label');
	   if (el) {
	      dx.font = css_style(el, 'font-size') +' '+ css_style(el, 'font-family');
	      console_log_dbgUs('dx.font=<'+ dx.font +'>');
	   }
	}

	var dx_title = function(obj, stored_db) {
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

	   if (!stored_db) {
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
		var el;
		var ident = obj.i;
		var freq = obj.f;
		
		if (eibi) {
		   if (ident == dx.last_ident) {
		      //console_log_dbgUs('2222 dx_idx='+ dx_idx +' last_dx_idx='+ dx.last_dx_idx +' '+ obj.f +' SAME AS LAST '+ ident);
		      el = w3_el(dx.last_dx_idx +'-id-dx-label');
		      el.title += '\n'+ dx_title(obj);
		      continue;
		   }
		}
		dx.last_dx_idx = dx_idx;
		dx.last_ident = ident;
      //if (eibi) console_log_dbgUs('2222 dx_idx='+ dx_idx +' '+ obj.f +' OK '+ ident);

		var notes = (isDefined(obj.n))? obj.n : '';
		el = w3_el(dx_idx +'-id-dx-label');
		if (!el) continue;
		var _ident = kiwi_decodeURIComponent('dx_ident2', ident).trim();
		if (_ident == '') _ident = '&nbsp;&nbsp;';
      el.innerHTML = _ident.replace(/\\n/g, '<br>');
		var idx = dx_type2idx(obj.fl & dx.DX_TYPE);
		var ex = (eibi && dx.eibi_ext[idx] != '')? '\nshift-click to open extension' : '';
		//if (eibi) console.log(obj.i +' '+ idx +' '+ dx.eibi_ext[idx] +' '+ ex);

      if (eibi) {
         el.title = dx.eibi_svc_s[idx] +' // home country: '+ obj.c + ex +'\n'+ dx_title(obj);
      } else {
         var title = kiwi_decodeURIComponent('dx_notes', notes);
         
         // NB: this replaces the two literal characters '\' and 'n' that occur when '\n' is entered into
         // the notes field with a proper newline escape '\n' so the text correctly spans multiple lines
	      title = title.replace(/<br>/g, '\n');
	      title = title.replace(/\\n/g, '\n');
	      
	      // only add schedule info to title popup if non-default
	      var dow = obj.fl & dx.DX_DOW;
	      //console_log_dbgUs('dow='+ dow.toHex(4) +' b='+ obj.b +' e='+ obj.e +' '+ ident);
	      if (dow != dx.DX_DOW || (obj.b != 0 || !(obj.b == 0 && obj.e == 2400)))
	         title = title + ((title != '')? '\n':'') + dx_title(obj, true);

	      el.title = title;
      }

      if (dx_idx < dx.post_render.length) {
         var sparse = dx.post_render[dx_idx];
         if (sparse) {
            var top = sparse.top;
            var left = sparse.x - 10;
            //var f = sparse.f;
            //console_log_dbgUs('dx.post_render: dx_idx='+ dx_idx +' post_render.len='+ dx.post_render.length +
            //   ' top='+ top +' left='+ left +' '+ f.toFixed(2) +' '+ sparse.ident);
            //console.log(el);
            el.style.top = px(top);
            el.style.left = px(left);
            if (isDefined(sparse.z)) el.style.zIndex = sparse.z;

            var el_line = w3_el(dx_idx +'-id-dx-line');
            top = sparse.ltop;
            el_line.style.top = px(top);
            el_line.style.height = px(dx_container_h - top);
         }
      }
		
      if (dx_idx < dx.color_fixup.length) {
         var sparse = dx.color_fixup[dx_idx];
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
						el = w3_el(dx_ibp_list[i].idx +'-id-dx-label');
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

function dx_filter()
{
	var s =
		w3_div('w3-medium w3-text-aqua w3-bold', 'DX label filter') +
		w3_divs('w3-text-aqua',
		   w3_col_percent('',
            w3_divs('/w3-margin-T-8',
               w3_input('w3-label-inline/id-dx-filter-ident w3-retain-input-focus w3-input-any-change w3-padding-small', 'Ident', 'dx.filter_ident', dx.filter_ident, 'dx_filter_cb'),
               w3_input('w3-label-inline/id-dx-filter-notes w3-retain-input-focus w3-input-any-change w3-padding-small', 'Notes', 'dx.filter_notes', dx.filter_notes, 'dx_filter_cb')
            ), 90
         ),
         w3_inline('w3-halign-space-around w3-margin-T-8 w3-text-white/',
            w3_checkbox('w3-retain-input-focus w3-label-inline w3-label-not-bold', 'case sensitive', 'dx.filter_case', dx.filter_case, 'dx_filter_opt_cb'),

            // Wildcard pattern matching, in addition to grep, is implemented. But currently checkbox is not shown because
            // there is no clear advantage in using it. E.g. it doesn't do partial matching like grep. So you have to type
            // "*pattern*" to duplicate what simply typing "pattern" to grep would do. Neither of them has the syntax of e.g.
            // simple search engines which is what the user probably really wants.
            w3_inline('',
               w3_text('', 'pattern match:'),
               //w3_checkbox('w3-retain-input-focus w3-label-inline w3-label-not-bold', 'wildcard', 'dx.filter_wild', dx.filter_wild, 'dx_filter_opt_cb'),
               w3_checkbox('w3-margin-left w3-retain-input-focus w3-label-inline w3-label-not-bold', 'grep', 'dx.filter_grep', dx.filter_grep, 'dx_filter_opt_cb')
            )
         )
      );

   confirmation_show_content(s, 450, 140, dx_filter_panel_close);
   w3_field_select('id-dx-filter-ident', {mobile:1});    // select the field
}

// Use a custom panel close routine because we need to remove focus if any of our elements are the
// active element at close time. This is due to how the w3-retain-input-focus logic works to keep
function dx_filter_panel_close()
{
   //console_log_dbgUs('dx_filter_panel_close');
   var ae = document.activeElement;
   //console_log_dbgUs(ae);
   if (ae && ae.id && (ae.id.startsWith('id-dx-filter') || ae.id.startsWith('id-dx.filter'))) {
      //console_log_dbgUs('### activeElement='+ ae.id);
      ae.blur();
   }

   confirmation_panel_close();
}

function dx_filter_cb(path, val, first, no_close)
{
   if (first) return;
	w3_string_cb(path, val);
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

   //console.log('dx_filter_opt_cb path='+ path +' val='+ val);
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
   //console.log('dx_label_step f='+ freq_car_Hz +'/'+ freq_displayed_Hz +' m='+ cur_mode);
   var i, dl;

   if (dir == 1) {
      for (i = 0; i < dx.displayed.length; i++) {
         dl = dx.displayed[i];
         if (!dl) continue;
         var f = Math.round(dl.freq * 1e3);
         //console.log('consider #'+ i +' '+ f);
         if (f > freq_displayed_Hz) break;
      }
      if (i == dx.displayed.length) {
	      wf_send('SET MARKER db='+ dx.db +' dir=1 freq='+ (freq_displayed_Hz/1e3).toFixed(3));
         return;
      }
   } else {
      for (i = dx.displayed.length - 1; i >= 0 ; i--) {
         dl = dx.displayed[i];
         if (!dl) continue;
         var f = Math.round(dl.freq * 1e3);
         //console.log('consider #'+ i +' '+ f);
         if (f < freq_displayed_Hz) break;
      }
      if (i < 0) {
	      wf_send('SET MARKER db='+ dx.db +' dir=-1 freq='+ (freq_displayed_Hz/1e3).toFixed(3));
         return;
      }
   }
   
   // after changing display to this frequency the normal dx_schedule_update() process will
   // acquire a new set of labels
   var mode = kiwi.modes_l[dl.flags & dx.DX_MODE];
   //console.log('FOUND #'+ i +' '+ f +' '+ dl.ident +' '+ mode +' '+ dl.lo +' '+ dl.hi);
   freqmode_set_dsp_kHz(dl.freq, mode);
   if (dl.lo != 0 && dl.hi != 0) {
      ext_set_passband(dl.lo, dl.hi);     // label has custom pb
   } else {
      var dpb = cfg.passbands[mode];
      //console.log(dpb);
      ext_set_passband(dpb.lo, dpb.hi);   // need to force default pb in case cpb is not default
   }
}

var dx_keys;

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
	dx_keys = ev? { shift:ev.shiftKey, alt:ev.altKey, ctrl:ev.ctrlKey, meta:ev.metaKey } : { shift:0, alt:0, ctrl:0, meta:0 };
	dx.o.gid = gid;
	
	//console.log('dx_show_edit_panel ws='+ ws_wf.stream);
   // must validate admin for write access to DB_STORED
	if (dx.db == dx.DB_STORED) {
	   dx.badp = -1;
	   
      ext_hasCredential('admin',
         function(badp) {
            dx.badp = badp;
            if (badp) {
               // show pwd entry panel
               // , 'dx_admin_pwd_cb')
               extint_panel_hide();    // committed to displaying edit panel, so remove any ext panel
               var s =
                  w3_inline('',
                     w3_div('w3-medium w3-text-aqua w3-bold', 'DX label edit'),
                     w3_select('w3-margin-L-64/w3-label-inline w3-text-white /w3-text-red', 'database', '', 'dx.db', dx.db, dx.db_s, 'dx_database_cb')
                  ) +
                  w3_div('',
			            w3_input('w3-margin-T-16//w3-margin-T-8 w3-padding-small w3-width-80pct', 'Admin password', 'dx.pwd', '', 'dx_admin_pwd_cb'),
			            w3_text('w3-margin-T-4', 'editing the stored DX labels of this Kiwi requires admin privileges')
			         );
	            ext_panel_set_name('dx');
               ext_panel_show(s, null, null, null, true);   // true: show help button
               ext_set_controls_width_height(550, 260);
               // put the cursor in (i.e. select) the password field
               if (from_shortcut != true)
                  w3_field_select('id-dx.pwd', {mobile:1});
            } else {
               dx_show_edit_panel2();
            }
         }, null, ws_wf);

      if (dx.badp != 0) return;
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
	var gid = dx.o.gid;
	//console_log_dbgUs('dx_show_edit_panel2 gid='+ gid +' db='+ dx.db);
	
	if (gid == -1) {
		//console.log('DX EDIT new entry');
		//console.log('DX EDIT new f='+ freq_car_Hz +'/'+ freq_displayed_Hz +' m='+ cur_mode);
		dx.o.fr = freq_displayed_kHz_str;
		dx.o.lo = dx.o.hi = dx.o.o = 0;
		dx.o.fm = kiwi.modes_s[cur_mode];
		dx.o.ft = dx.DX_QUICK_0;
		dx.o.fd = dx.DX_DOW_BASE;
		dx.o.begin = 0;
		dx.o.end = 0;
		dx.o.i = dx.o.n = dx.o.p = '';
	} else {
      var label = dx.list[gid];
		//console.log('DX EDIT entry #'+ gid +' prev: f='+ label.freq +' flags='+ label.flags.toHex() +' i='+ label.ident +' n='+ label.notes);
		dx.o.fr = label.carrier.toFixed(2);		// starts as a string, validated to be a number
      dx.o.lo = label.lo;
      dx.o.hi = label.hi;
		dx.o.o = label.moff;
		dx.o.fm = (label.flags & dx.DX_MODE);
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
	//console_log_dbgUs(dx.o);
	
	// quick key combo to toggle 'active' type without bringing up panel
	if (dx.db == dx.DB_STORED && gid != -1 && dx_keys.shift && dx_keys.alt) {
		//console.log('DX COMMIT quick-active entry #'+ dx.o.gid +' f='+ dx.o.fr);
		//console.log(dx.o);
		var type = dx.o.ft;
		type = (type == dx.DX_QUICK_0)? dx.DX_QUICK_1 : dx.DX_QUICK_0;
		dx.o.ft = type;
		var flags = (dx.o.fd << dx.DX_DOW_SFT) | (type << dx.DX_TYPE_SFT) | dx.o.fm;
		wf_send('SET DX_UPD g='+ dx.o.gid +' f='+ dx.o.fr +' lo='+ dx.o.lo.toFixed(0) +' hi='+ dx.o.hi.toFixed(0) +
		   ' o='+ dx.o.o.toFixed(0) +' fl='+ flags +' b='+ dx.o.begin +' e='+ dx.o.end +
			' i='+ encodeURIComponent(dx.o.i +'x') +' n='+ encodeURIComponent(dx.o.n +'x') +' p='+ encodeURIComponent(dx.o.p +'x'));
		return;
	}

   // Committed to displaying edit panel, so remove any extension panel that might be showing (and clear ext menu).
   // Important to do this here. Couldn't figure out reasonable way to do this in extint_panel_show()
	extint_panel_hide();		
	
	dx.o.fr = (parseFloat(dx.o.fr) + kiwi.freq_offset_kHz).toFixed(2);
	
	dx.o.pb = '';
	if (dx.o.lo || dx.o.hi) {
      if (dx.o.lo == -dx.o.hi) {
         dx.o.pb = (Math.abs(dx.o.hi)*2).toFixed(0);
      } else {
         dx.o.pb = dx.o.lo.toFixed(0) +', '+ dx.o.hi.toFixed(0);
      }
	}

	var s1 =
	   w3_inline('',
		   w3_div('w3-medium w3-text-aqua w3-bold', 'DX label edit'),
		   w3_select('w3-margin-L-64/w3-label-inline w3-text-white /w3-text-red', 'database', '', 'dx.db', dx.db, dx.db_s, 'dx_database_cb', 0)
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
      var type_menu = kiwi_deep_copy(cfg.dx_type);
      if (dx.types_n) {
         for (i = 0; i < dx.DX_N_STORED; i++) {
            if (dx.types_n[i] != 0 && type_menu[i].name == '') {
               type_menu[i].name = '(T'+ i +')';    // make placeholder entry in menu, but not cfg.dx_type
               //console.log('empty type menu entry: T'+ i +' #'+ dx.types_n[i]);
            }
         }
      }
      //console.log(type_menu);

	   s2 =
         w3_divs('w3-text-white/w3-margin-T-8',
            w3_inline('w3-halign-space-between/',
               w3_input('w3-padding-small||size=8', 'Freq', 'dx.o.fr', dx.o.fr, 'dx_num_cb'),
               w3_select('w3-text-red', 'Mode', '', 'dx.o.fm', dx.o.fm, kiwi.modes_u, 'dx_sel_cb'),
               w3_input('w3-padding-small||size=10', 'Passband', 'dx.o.pb', dx.o.pb, 'dx_passband_cb'),
               w3_select('w3-text-red', 'Type', '', 'dx.o.ft', dx.o.ft, type_menu, 'dx_sel_cb'),
               w3_input('w3-padding-small||size=8', 'Offset', 'dx.o.o', dx.o.o, 'dx_num_cb')
            ),
      
            w3_input('w3-label-inline/w3-padding-small', 'Ident', 'dx.o.i', '', 'dx_string_cb'),
            w3_input('w3-label-inline/w3-padding-small', 'Notes', 'dx.o.n', '', 'dx_string_cb'),
            w3_input('w3-label-inline/w3-padding-small', 'Extension', 'dx.o.p', '', 'dx_string_cb'),
      
            w3_inline('w3-hspace-16',
               w3_text('w3-text-white w3-bold', 'Schedule (UTC)'),
               dow_s,
               w3_input('w3-label-inline/w3-padding-small', 'Begin', 'dx.o.begin', begin_s, 'dx_sched_time_cb'),
               w3_input('w3-label-inline/w3-padding-small', 'End', 'dx.o.end', end_s, 'dx_sched_time_cb')
            ),

            w3_inline('w3-hspace-16',
               w3_button('w3-yellow', 'Modify', 'dx_modify_cb'),
               w3_button('w3-green', 'Add', 'dx_add_cb'),
               w3_button('w3-red', 'Delete', 'dx_delete_cb'),
               w3_div('w3-margin-T-4',
                  w3_checkbox('/w3-label-inline w3-label-not-bold w3-text-white/',
                     'filter by time/day-of-week', 'dx.stored_filter_tod', dx.stored_filter_tod, 'dx_time_dow_cb'),
                  w3_text('w3-margin-T-4', 'Create new label with Add button')
               )
            )
         );
   } else {    // EiBi
      var cbox_container = 'w3-halign-space-around/|flex-basis:130px';
      var cbox = '/w3-label-inline w3-label-not-bold w3-round w3-padding-small/';
      var checkbox = function(type) {
         var idx = dx_type2idx(type);
         var svc = (dx.eibi_types_mask & (1 << idx))? 1:0;
         //console.log('svc='+ svc +' m='+ dx.eibi_types_mask.toHex(8) +' type='+ type.toHex(4) +' idx='+ idx +' '+ dx.eibi_svc_s[idx]);
         return w3_checkbox(cbox, dx.eibi_svc_s[idx], 'eibi-cbox'+ idx, svc, 'dx_eibi_svc_cb', idx);
      };

	   s2 =
	      w3_inline('',
            //w3_div('w3-css-text-css-yellow w3-margin-TB-16', 'The EiBi database is read-only'),
            w3_checkbox('w3-margin-TB-16/w3-label-inline w3-label-not-bold w3-text-white/w3-hspace-32',
               'select all', 'eibi-all', dx.eibi_types_mask == 0xffffffff, 'dx_eibi_all_cb'),
            w3_checkbox('/w3-label-inline w3-label-not-bold w3-text-white/w3-margin-L-32',
               'filter by time/day-of-week', 'dx.eibi_filter_tod', dx.eibi_filter_tod, 'dx_time_dow_cb')
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
   }
	
	// can't do this as initial val passed to w3_input above when string contains quoting
	ext_panel_set_name('dx');
	ext_panel_show(s1 + s2, null,
	   function() {
         if (dx.db == dx.DB_STORED) {
            var el = w3_el('dx.o.i');
            el.value = dx.o.i;
            w3_el('dx.o.n').value = dx.o.n;
            w3_el('dx.o.p').value = dx.o.p;
      
            // change focus to input field
            // FIXME: why doesn't field select work?
            //console.log('dx.o.i='+ el.value);
            //w3_field_select(el, {mobile:1});
         } else {
            for (var i = 0; i < dx.DX_N_EiBi; i++) {
               w3_color('eibi-cbox'+ i +'-label', 'black', dx.eibi_colors[i]);
            }
         }
      },
	   function() {
	      //console.log('dx_show_edit_panel2 HIDE_FUNC');
	   },
	   true     // show help button
   );
	ext_set_controls_width_height(550, 290);
	if (dx.help) {
	   extint_help_click_now();
	   dx.help = false;
	}
}

function dx_database_cb(path, idx, first, ctrlAlt, from_shortcut)
{
   //console.log('first='+ first +' ctrlAlt='+ ctrlAlt);
   if (first) return;
   dx.db = +idx;
   dx.list = [];
   console_log_dbgUs('DX DB-SWITCHED db='+ dx.db);
   if (ctrlAlt)
      dx_show_edit_panel(null, -1, from_shortcut);
   else {
      if (ext_panel_displayed('dx'))
         dx_close_edit_panel();
   }
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
   setVarFromString(path, checked? 1:0);
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

function dx_modify_cb(id, val)
{
	console_log_dbgUs('DX COMMIT modify entry #'+ dx.o.gid +' f='+ dx.o.fr);
	console_log_dbgUs(dx.o);
	if (dx.o.gid == -1) return;
   var flags = (dx.o.fd << dx.DX_DOW_SFT) | (dx.o.ft << dx.DX_TYPE_SFT) | dx.o.fm;
	dx.o.fr -= kiwi.freq_offset_kHz;
	if (dx.o.fr < 0) dx.o.fr = 0;
	wf_send('SET DX_UPD g='+ dx.o.gid +' f='+ dx.o.fr +' lo='+ dx.o.lo.toFixed(0) +' hi='+ dx.o.hi.toFixed(0) +
	   ' o='+ dx.o.o.toFixed(0) +' fl='+ flags +' b='+ dx.o.begin +' e='+ dx.o.end +
		' i='+ encodeURIComponent(dx.o.i +'x') +' n='+ encodeURIComponent(dx.o.n +'x') +' p='+ encodeURIComponent(dx.o.p +'x'));
	setTimeout(function() {dx_close_edit_panel(id);}, 250);
}

function dx_add_cb(id, val)
{
	//console.log('DX COMMIT new entry');
	//console.log(dx.o);
   var flags = (dx.o.fd << dx.DX_DOW_SFT) | (dx.o.ft << dx.DX_TYPE_SFT) | dx.o.fm;
	dx.o.fr -= kiwi.freq_offset_kHz;
	if (dx.o.fr < 0) dx.o.fr = 0;
	wf_send('SET DX_UPD g=-1 f='+ dx.o.fr +' lo='+ dx.o.lo.toFixed(0) +' hi='+ dx.o.hi.toFixed(0) +
	   ' o='+ dx.o.o.toFixed(0) +' fl='+ flags +' b='+ dx.o.begin +' e='+ dx.o.end +
		' i='+ encodeURIComponent(dx.o.i +'x') +' n='+ encodeURIComponent(dx.o.n +'x') +' p='+ encodeURIComponent(dx.o.p +'x'));
	setTimeout(function() {dx_close_edit_panel(id);}, 250);
	owrx.dx_click_gid_last = undefined;    // because gid's may get renumbered
}

function dx_delete_cb(id, val)
{
	//console.log('DX COMMIT delete entry #'+ dx.o.gid);
	//console.log(dx.o);
	if (dx.o.gid == -1) return;
	wf_send('SET DX_UPD g='+ dx.o.gid +' f=-1');
	setTimeout(function() {dx_close_edit_panel(id);}, 250);
	owrx.dx_click_gid_last = undefined;    // because gid's may get renumbered
}

function dx_click(ev, gid)
{
   //event_dump(ev, 'dx_click');
	if (ev.shiftKey && dx.db == dx.DB_STORED) {
		dx_show_edit_panel(ev, gid);
	} else {
	   owrx.dx_click_gid_last = (dx.db == dx.DB_STORED)? gid : undefined;
	   var label = dx.list[gid];
	   var freq = label.freq;
		var mode = kiwi.modes_l[label.flags & dx.DX_MODE];
		var lo = label.lo;
		var hi = label.hi;
		var params = label.params;
		
      extint.extname = extint.param = null;
      if (isArg(params)) {
         var ext = (params == '')? [] : decodeURIComponent(params).split(',');
         if (mode == 'drm') {
            extint.extname = 'drm';
            ext.push('lo:'+ lo);    // forward passband info from dx label panel to keep DRM ext from overriding it
            ext.push('hi:'+ hi);
            extint.param = ext.join(',');
         } else {
            extint.extname = ext[0];
            extint.param = ext.slice(1).join(',');
         }
      }
      var extname = extint.extname? extint.extname : '';
		console_log_dbgUs('dx_click f='+ label.freq +' mode='+ mode +' cur_mode='+ cur_mode +' lo='+ lo +' hi='+ hi +' params=<'+ params +'> extname=<'+ extname +'> param=<'+ extint.param +'>');

      // EiBi database frequencies are dial/carrier (i.e. not pbc)
      if (dx.db == dx.DB_EiBi) {
         
      }

		freqmode_set_dsp_kHz(freq, mode, { open_ext:true });
		if (lo || hi) {
		   ext_set_passband(lo, hi, false, freq);
		}
		
		// open specified extension
		// setting DRM mode above opens DRM extension
		var qual = (dx.db == dx.DB_STORED)? !any_alternate_click_event(ev) : ev.shiftKey;
		//var alt_click = any_alternate_click_event(ev);
		if (mode != 'drm' && qual && !dx.ctrl_click && params) {
		   console_log_dbgUs('dx_click ext='+ extint.extname +' <'+ extint.param +'>');
			extint_open(extint.extname, 250);
		}
		
		dx.ctrl_click = false;
	}
	return cancelEvent(ev);		// keep underlying canvas from getting event
}

// Any var we add to the div in dx_label() is undefined in the div that appears here,
// so use the number embedded in id="" to create a reference.
// Even the "data-" thing doesn't work.

var dx_z_save, dx_bg_color_save;

function dx_enter(dx, cmkr_x, ev)
{
	dx_z_save = dx.style.zIndex;
	dx.style.zIndex = 999;
	dx_bg_color_save = dx.style.backgroundColor;
	dx.style.backgroundColor = w3_contains(dx, 'cl-dx-label-filtered')? 'lightGrey' : (w3_contains(dx, 'id-has-ext')? 'magenta' : 'yellow');

	var dx_line = w3_el(parseInt(dx.id)+'-id-dx-line');
	dx_line.zIndex = 998;
	dx_line.style.width = '3px';
	
	if (cmkr_x) {
		dx_canvas.style.left = px(cmkr_x-dx_car_w/2);
		dx_canvas.style.zIndex = 100;
	}
}

function dx_leave(dx, cmkr_x)
{
	dx.style.zIndex = dx_z_save;
	dx.style.backgroundColor = dx_bg_color_save;

	var dx_line = w3_el(parseInt(dx.id)+'-id-dx-line');
	dx_line.zIndex = 110;
	dx_line.style.width = '1px';
	
	if (cmkr_x) {
		dx_canvas.style.zIndex = 99;
	}
}

function dx_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'DX label edit help') +
         w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
            w3_div('w3-margin-R-8',
               'There are two types of DX labels seen in the area above the frequency scale: <br>' +
               '1) Labels from a stored database, editable by the Kiwi owner/admin. <br>'+
               '2) Labels from a read-only copy of the <a href="http://www.eibispace.de" target="_blank">EiBi database</a> that cannot be modified.' +
               '<br><br>' +
               
               'The DX label edit control panel has a <i>database</i> menu to select between the two. ' +
               'Similarly, there are two selection entries in the right-click menu (double-finger tap on mobile devices). ' +
               'The keyboard shortcut keys "\\" and "|" also switch between the two databases with the later also opening the control panel.' +
               '<br><br>' +
               
               'When zoomed out the number of labels shown is limited to prevent overwhelming the display. ' +
               'This is why some labels will only appear as you zoom in. The same is true if too many categories ' +
               'are selected enabling a large number of labels.' +
               '<br><br>' +
               
               'Information about using the editing controls for the stored labels can be found ' +
               '<a href="http://kiwisdr.com/quickstart/index.html#id-user-marker" target="_blank">here</a>.' +
               '<br><br>' +
               
               'Because there are so many EiBi labels (about 17000) they are organized into 12 categories with visibility ' +
               'selected by checkboxes on the control panel. In addition labels from either database can be filtered by their schedule (UTC time & day-of-week) ' +
               'specified in the database. If filtering is disabled labels outside their scheduled time appear with a lighter category color and a dashed border. ' +
               'When filtering is enabled the label display will refresh every 5 minutes so it reflects the current schedule.' +
               '<br><br>' +
               
               'Labels that have an extension specified turn magenta when moused-over as opposed to the usual yellow. ' +
               'And labels outside their scheduled time (if any) will mouse-over grey. ' +
               'For stored labels the extension is set per-label in the label edit panel and the extension opened when the label is clicked ' +
               '(a shift-click opens that label in the edit panel). ' +
               'For EiBi labels an extension is automatically assumed for the ALE, CW, FSK, Fax and Time categories, ' +
               'but is only opened if the label is shift-clicked (note difference from stored label behavior, ' +
               'a non-shift click simply selects the frequency/mode).' +
               '<br><br>' +
               
               'When a label is moused-over a popup is shown with optional information (stored labels) or database information (EiBi labels). ' +
               'On Safari and Chrome the popups take a few second to appear. EiBi information includes the station\'s home country, transmission schedule, ' +
               'language and target area (if any). ' +
               'Information about the EiBi database, including descriptions of the language and target codes seen in the mouse-over popups, ' +
               '<a href="http://kiwisdr.com/files/EiBi/README.txt" target="_blank">can be found here.</a>' +
               '<br><br>' +
         
               'EiBi URL parameters: (use "dx=..." in the browser URL)<br>' +
               w3_text('|color:orange',
               'none bcast utility time ale hfdl milcom cw fsk fax aero maritime spy filter[:0]') + '<br>' +
               'By default all the category checkboxes and "<i>filter by time/day-of-week</i>" are checked. <br>' +
               'Specifying <i>none</i> will uncheck all the categories. Specifying the individual category names will then ' +
               'toggle the checkbox state.' +
               '<br><br>' +

               'For example, to select only the ALE and Fax categories: <i>dx=none,ale,fax</i> <br>' +
               'To select all except the CW and Time categories: <i>dx=cw,time</i> <br>' +
               'To just bring up the EiBi control panel: <i>dx</i> (e.g. <i>my_kiwi:8073/?spec&dx</i>)<br>' +
               'Keywords are case-insensitive and can be abbreviated. For example: <br>' +
               '<i>dx=n,bc,f:0</i> (show all broadcast stations without time/day filtering) <br>' +
               ''
            )
         );

      confirmation_show_content(s, 610, 375);
      w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
   }
   return true;
}


////////////////////////////////
// s-meter
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
var smeter_ovfl;

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
		w3_div('id-smeter-ovfl w3-hide', 'OV'),
		w3_div('id-smeter-dbm-value'),
		w3_div('id-smeter-dbm-units', 'dBm')
	);

	var sMeter_canvas = w3_el('id-smeter-scale');
	smeter_ovfl = w3_el('smeter-ovfl');

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
		//console.log("SM x="+x+' dBm='+bars.dBm[i]+' '+bars.text[i]);
	}

	line_stroke(sMeter_ctx, 0, 5, "black", 0,y, w,y);
	kiwi_clearInterval(owrx.smeter_interval);
	owrx.smeter_interval = setInterval(update_smeter, 100);
}

var sm_px = 0, sm_timeout = 0, sm_interval = 10;
var sm_ovfl_showing = false;
//var audio_ext_adc_ovfl_test = 0;

function update_smeter()
{
	var x = smeter_dBm_biased_to_x(owrx.sMeter_dBm_biased);
	var y = SMETER_SCALE_HEIGHT-8;
	var w = smeter_width;
	sMeter_ctx.globalAlpha = 1;
	line_stroke(sMeter_ctx, 0, 5, "lime", 0,y, x,y);

   if (cfg.agc_thresh_smeter && owrx.force_no_agc_thresh_smeter != true) {
      var thold = (cur_mode && cur_mode.substr(0,2) == 'cw')? threshCW : thresh;
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
	
	//audio_ext_adc_ovfl_test++;
	//audio_ext_adc_ovfl = ((audio_ext_adc_ovfl_test % 32) < 16);
	
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
	
	w3_innerHTML('id-smeter-dbm-value', (owrx.sMeter_dBm).toFixed(0));
}


////////////////////////////////
// user ident
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
      writeCookie('ident', user_url);
   }
	var ident = initCookie('ident', '');
   ident = kiwi_strip_tags(ident, '').substring(0, len);
	//console.log('ident PRE ident_user=<'+ ident +'> ident_len='+ len);
	ident = kiwi_strip_tags(ident, '').substring(0, len);
	//console.log('ident POST ident_user=<'+ ident +'> ident_len='+ len);
	var el = w3_el('id-ident-input');
	w3_attribute(el, 'maxlength', len);
	el.value = ident;
	ident_user = ident;
	send_ident = true;
	//console.log('ident_init: SET ident='+ ident_user);
}

function ident_complete(from)
{
	var el = w3_el('id-ident-input');
	var ident = el.value;
	var len = Math.max(cfg.ident_len, kiwi.ident_min);
   ident = kiwi_strip_tags(ident, '').substring(0, len);
	//console.log('ICMPL from='+ from +' ident='+ ident);
	el.value = ident;
	//console.log('ICMPL el='+ typeof(el) +' ident_user=<'+ ident +'>');
	kiwi_clearTimeout(ident_tout);

	// okay for ident='' to erase it
	// SECURITY: input value length limited by "maxlength" attribute, but also guard against binary data injection?
	//w3_field_select(el, {mobile:1});
	w3_schedule_highlight(el);
	freqset_select();    // don't keep ident field selected

	writeCookie('ident', ident);
	ident_user = ident;
	send_ident = true;
	//console.log('ident_complete: SET ident_user='+ ident_user);
}

function ident_keyup(el, evt)
{
	//event_dump(evt, "IKU");
	kiwi_clearTimeout(ident_tout);
	//console.log("IKU el="+ typeof(el) +" val="+ el.value);
	
	// Ignore modifier keys used with mouse events that also appear here.
	// Also keeps ident_complete timeout from being set after return key.
	//if (ignore_next_keyup_event) {
	if (evt != undefined && evt.key != undefined) {
		var klen = evt.key.length;
		if (any_alternate_click_event_except_shift(evt) || klen != 1 && evt.key != 'Backspace' && evt.key != 'Shift') {
		   //console.log('IDENT key='+ evt.key);
			if (evt.key == 'Enter') {
			   ident_complete('Enter');
			}
         //console.log("ignore shift-key ident_keyup");
         //ignore_next_keyup_event = false;
         return;
      }
	}
	
	ident_tout = setTimeout(function() { ident_complete('t/o'); } , 5000);
}


////////////////////////////////
// keyboard shortcuts
////////////////////////////////

var shortcut = {
   nav_off: 0,
   keys: '',
   SHIFT: 1,
   CTL_OR_ALT: 2,
   SHIFT_PLUS_CTL_OR_ALT: 3,
   KEYCODE_ALT: 18
};

function keyboard_shortcut_init()
{
   if (kiwi_isMobile() || kiwi_isFirefox() < 47 || kiwi_isChrome() <= 49 || kiwi_isOpera() <= 36) return;
   
   shortcut.help =
      w3_div('',
         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua', 'Keys', 25, 'Function'),
         w3_inline_percent('w3-padding-tiny', 'g =', 25, 'select frequency entry field'),
         w3_inline_percent('w3-padding-tiny', 'j i LR-arrow-keys', 25, 'frequency step down/up, add shift or alt/ctrl for faster<br>shift plus alt/ctrl to step to next/prev DX label'),
         w3_inline_percent('w3-padding-tiny', 't T UD-arrow-keys', 25, 'scroll frequency memory list'),
         w3_inline_percent('w3-padding-tiny', 'b B', 25, 'scroll band menu'),
         w3_inline_percent('w3-padding-tiny', 'e E', 25, 'scroll extension menu'),
         w3_inline_percent('w3-padding-tiny', 'a A d l u c f q', 25, 'toggle modes: AM SAM DRM LSB USB CW NBFM IQ<br>add alt/ctrl to toggle backwards (e.g. SAM modes)'),
         w3_inline_percent('w3-padding-tiny', 'p P', 25, 'passband narrow/widen'),
         w3_inline_percent('w3-padding-tiny', 'r', 25, 'toggle audio recording'),
         w3_inline_percent('w3-padding-tiny', 'z Z', 25, 'zoom in/out, add alt/ctrl for max in/out'),
         w3_inline_percent('w3-padding-tiny', '< >', 25, 'waterfall page down/up'),
         w3_inline_percent('w3-padding-tiny', 'w W', 25, 'waterfall min dB slider -/+ 1 dB, add alt/ctrl for -/+ 10 dB'),
         w3_inline_percent('w3-padding-tiny', 'S', 25, 'waterfall auto-scale'),
         w3_inline_percent('w3-padding-tiny', 's D', 25, 'spectrum on/off toggle, slow device mode'),
         w3_inline_percent('w3-padding-tiny', 'v V m space', 25, 'volume less/more, mute'),
         w3_inline_percent('w3-padding-tiny', 'o', 25, 'toggle between option bar "off" and "stats" mode,<br>others selected by related shortcut key'),
         w3_inline_percent('w3-padding-tiny', '!', 25, 'toggle aperture manual/auto menu'),
         w3_inline_percent('w3-padding-tiny', '@', 25, 'DX label filter'),
         w3_inline_percent('w3-padding-tiny', '\\ |', 25, 'toggle (toggle & open) DX stored/EiBi database'),
         //w3_inline_percent('w3-padding-tiny', '#', 25, 'Open user preferences extension'),
         w3_inline_percent('w3-padding-tiny', 'x y', 25, 'toggle visibility of control panels, top bar'),
         w3_inline_percent('w3-padding-tiny', 'esc', 25, 'close/cancel action'),
         w3_inline_percent('w3-padding-tiny', '? h', 25, 'toggle this help list'),
         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua', '', 25, 'Windows, Linux: use alt, not ctrl'),
         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua', '', 25, 'Mac: use ctrl or alt')
      );

	w3_el('id-kiwi-body').addEventListener('keydown', keyboard_shortcut_event, true);
}

function keyboard_shortcut_help()
{
   confirmation_show_content(shortcut.help, 550, 565);   // height +15 per added line
}

// FIXME: animate (light up) control panel icons?

function keyboard_shortcut_nav(nav)
{
   w3_el('id-nav-optbar-'+ nav).click();
   shortcut.nav_click = true;
}

function keyboard_shortcut_key_proc()
{
   if (shortcut.key_i >= shortcut.keys.length) return;
   var key = shortcut.keys[shortcut.key_i];
   //console.log('key='+ key);
   keyboard_shortcut(key, 0, 0);
   shortcut.key_i++;
   setTimeout(keyboard_shortcut_key_proc, 100);
}

function keyboard_shortcut_url_keys()
{
   shortcut.keys = shortcut.keys.split('');
   shortcut.key_i = 0;
   keyboard_shortcut_key_proc();
}

// abcdefghijklmnopqrstuvwxyz `~!@#$%^&*()-_=+[]{}\|;:'"<>? 0123456789.,/kM
// ..........F.. ............   ...       F .     .. F  ... FFFFFFFFFFFFFFF
// .. ..   ..  F  .  .. ..  .                               F: frequency entry keys
// ABCDEFGHIJKLMNOPQRSTUVWXYZ
// :space: :tab:
//    .

function keyboard_shortcut(key, mod, ctlAlt, keyCode)
{
   var action = true;
   var dir = ctlAlt? -1 : 1;
   shortcut.nav_click = false;
   
   switch (key) {
   
   // mode
   case 'a': mode_button(null, w3_el('id-button-am'), dir); break;
   case 'A': mode_button(null, w3_el('id-button-sam'), dir); break;
   case 'd': ext_set_mode('drm', null, { open_ext:true }); break;
   case 'l': mode_button(null, w3_el('id-button-lsb'), dir); break;
   case 'u': mode_button(null, w3_el('id-button-usb'), dir); break;
   case 'c': mode_button(null, w3_el('id-button-cw'), dir); break;
   case 'f': ext_set_mode('nbfm'); break;
   case 'q': ext_set_mode('iq'); break;
   
   // step
   case 'j': case 'J': case 'ArrowLeft':
      if (mod != shortcut.SHIFT_PLUS_CTL_OR_ALT)
         freqstep(owrx.wf_snap? mod : (2-mod));
      else
         dx_label_step(-1);
      break;
   case 'i': case 'I': case 'ArrowRight':
      if (mod != shortcut.SHIFT_PLUS_CTL_OR_ALT)
         freqstep(owrx.wf_snap? (5-mod) : (3+mod));
      else
         dx_label_step(+1);
      break;
   
   // passband
   case 'p': passband_increment(false); break;
   case 'P': passband_increment(true); break;
   
   // volume/mute
   case 'v': setvolume(1, kiwi.volume-10); toggle_or_set_mute(0); keyboard_shortcut_nav('audio'); break;
   case 'V': setvolume(1, kiwi.volume+10); toggle_or_set_mute(0); keyboard_shortcut_nav('audio'); break;
   case 'm':
   case ' ': toggle_or_set_mute(); shortcut.nav_click = true; break;

   // frequency entry / memory list
   case 'g': case '=': freqset_select(); break;
   case 't': freq_up_down_cb(null, 1); break;
   case 'T': freq_up_down_cb(null, 0); break;
   case 'b': band_scroll(1); break;
   case 'B': band_scroll(-1); break;

   // page scroll
   case '<': page_scroll(-page_scroll_amount); break;
   case '>': page_scroll(+page_scroll_amount); break;

   // zoom
   case 'z': zoom_step(ctlAlt? ext_zoom.NOM_IN : ext_zoom.IN); break;
   case 'Z': zoom_step(ctlAlt? ext_zoom.MAX_OUT : ext_zoom.OUT); break;
   
   // waterfall
   case 'w': incr_mindb(1, ctlAlt? -10 : -1); keyboard_shortcut_nav('wf'); break;
   case 'W': incr_mindb(1, ctlAlt? +10 : +1); keyboard_shortcut_nav('wf'); break;
   
   // spectrum
   case 's': toggle_or_set_spec(); keyboard_shortcut_nav('wf'); break;
   case 'S': wf_autoscale_cb(); keyboard_shortcut_nav('wf'); break;
   case 'D': toggle_or_set_slow_dev(); keyboard_shortcut_nav('wf'); break;
   
   // colormap
   case '!': keyboard_shortcut_nav('wf'); wf_aper_cb('wf.aper', wf.aper ^ 1, false); break;

   // misc
   case '#': if (dbgUs) extint_open('prefs'); break;
   case 'o': keyboard_shortcut_nav(shortcut.nav_off? 'status':'off'); shortcut.nav_off ^= 1; break;
   case 'r': toggle_or_set_rec(); break;
   case 'x': toggle_or_set_hide_panels(); break;
   case 'y': toggle_or_set_hide_bars(); break;
   case '@': dx_filter(); shortcut.nav_click = true; break;
   case 'e': extension_scroll(1); break;
   case 'E': extension_scroll(-1); break;
   case '\\': case '|': dx_database_cb('', (dx.db == dx.DB_STORED)? dx.DB_EiBi : dx.DB_STORED, false, key == '|', /* from_shortcut */ true); break;
   case '?': case 'h': keyboard_shortcut_help(); break;

   default:
      if (key.length == 1 && keyCode != shortcut.KEYCODE_ALT) console.log('no shortcut key <'+ key +'>');
      action = false; break;
   
   }
   
   //if (action && !shortcut.nav_click) keyboard_shortcut_nav('users');

   ignore_next_keyup_event = true;     // don't trigger e.g. freqset_keyup()/freqset_complete()
}

function keyboard_shortcut_event(evt)
{
   if (evt.target) {
      var k = evt.key;
      
      // ignore esc and Fnn function keys
      if (k == 'Escape' || k.match(/F[1-9][12]?/)) {
         //event_dump(evt, 'Escape-shortcut');
         //console.log('KEY PASS Esc');
         return;
      }
      
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
      var mod = (sft && !ctlAlt)? shortcut.SHIFT : ( (!sft & ctlAlt)? shortcut.CTL_OR_ALT : ( (sft && ctlAlt)? shortcut.SHIFT_PLUS_CTL_OR_ALT : 0 ) );

      var field_input_key = (
            (k >= '0' && k <= '9' && !ctl) ||
            k == '.' || k == ',' ||                // ',' is alternate decimal point to '.'
            k == '/' || k == ':' || k == '-' ||    // for passband spec, have to allow for negative passbands (e.g. lsb)
            k == 'k' || k == 'M' ||                // scale modifiers
            k == 'Enter' || k == 'ArrowUp' || k == 'ArrowDown' || k == 'Backspace' || k == 'Delete'
         );

      if (evt.target.nodeName != 'INPUT' || (id == 'id-freq-input' && !field_input_key)) {
         
         // don't interfere with the meta key shortcuts of the browser
         if (kiwi_isOSX()) {
            if (meta) {
               //console.log('ignore OSX META '+ (k? k:''));
               return;
            }
         } else {
            if (ctl) {
               //console.log('ignore non-OSX CTL '+ (k? k:''));
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
         
         //console.log('keyboard_shortcut key=<'+ k +'> keyCode='+ evt.keyCode +' mod='+ mod +' ctlAlt='+ ctlAlt +' alt='+ alt);
         keyboard_shortcut(k, mod, ctlAlt, evt.keyCode);
         
         /*
         if (k != 'Shift' && k != 'Control' && evt.key != 'Alt') {
            if (!action) event_dump(evt, 'shortcut');
            console.log('KEY SHORTCUT <'+ k +'> '+ (sft? 'SFT ':'') + (ctl? 'CTL ':'') + (alt? 'ALT ':'') + (meta? 'META ':'') +
               ((evt.target.nodeName == 'INPUT')? 'id-freq-input' : evt.target.nodeName) +
               (action? ' ACTION':''));
         }
         */

         cancelEvent(evt);
         return;
      } else {
         //console.log('KEY INPUT FIELD <'+ k +'> '+ id + suffix);
      }
   } else {
      //console.log('KEY no EVT');
   }
}

function extension_scroll(dir)
{
   console.log('extension_scroll dir='+ dir);
   console.log(extint_names);
   var el = w3_el('id-select-ext');
   console.log(el);
   var ext_menu = el.childNodes;
   console.log(ext_menu);
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
// panels
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
   if (ac_play_button.state != "running") {
      var s =
         w3_div('id-play-button-container class-overlay-container||onclick="play_button()"',
            w3_div('id-play-button',
               '<img src="gfx/openwebrx-play-button.png" width="150" height="150" /><br><br>' +
               (kiwi_isMobile()? 'Tap to':'Click to') +' start OpenWebRX'
            )
         );
      w3_create_appendElement('id-kiwi-body', 'div', s);
      el = w3_el('id-play-button');
      el.style.marginTop = px(w3_center_in_window(el));
      //alert('state '+ ac_play_button.state);
   }
}

// Safari on iOS only plays webaudio after it has been started by clicking a button.
// Same now for Chrome and Safari 12 on OS X.
function play_button()
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
	w3_el('id-play-button-container').style.opacity = 0;
	setTimeout(function() { w3_hide('id-play-button-container'); }, 1100);
	audio_reset();    // handle possible audio overflow condition during wait for button click
   freqset_select();
}

// called from wf_init()
function panels_setup()
{
   var el;
   var s;
   
	w3_el("id-ident").innerHTML =
		w3_input('w3-label-not-bold/w3-custom-events|padding:1px|size=20 onkeyup="ident_keyup(this, event)"', 'Your name or callsign:', 'ident-input');
	
	w3_el("id-control-freq1").innerHTML =
	   w3_inline('',
	      w3_inline('',
            w3_div('id-freq-cell',
               // NB: DO NOT remove the following <form> (3/2021)
               // The CATSync app depends on this API by using the following javascript injection:
               // Dim jsFreqKiwiSDR As String = "targetForm = document.forms['form_freq']; targetForm.elements[0].value = '" + frequency + "'; freqset_complete(0); false"
               // Form1.browser.ExecuteScriptAsync(jsFreqKiwiSDR)
               '<form id="id-freq-form" name="form_freq" action="#" onsubmit="freqset_complete(0); return false;">' +
                  w3_input('w3-custom-events|padding:0 4px|size=9 onkeydown="freqset_keydown(event)" onkeyup="freqset_keyup(this, event)"', '', 'freq-input') +
               '</form>'
            ),

            w3_div('|padding:0 0 0 3px',
               w3_icon('w3-show-block w3-text-orange||title="prev"', 'fa-arrow-circle-up', 15, '', 'freq_up_down_cb', 1),
               w3_icon('w3-show-block w3-text-aqua||title="next"', 'fa-arrow-circle-down', 15, '', 'freq_up_down_cb', 0)
            )
         ),

	      w3_inline('w3-halign-space-around/',
            w3_div('id-select-band-cell|padding:0 4px',
               '<select id="id-select-band" class="w3-pointer w3-select-menu" onchange="select_band(this.value)">' +
                  setup_band_menu() +
               '</select>'
            ),

            w3_div('id-select-ext-cell|padding:0',
               '<select id="id-select-ext" class="w3-pointer w3-select-menu" onchange="freqset_select(); extint_select(this.value)">' +
                  '<option value="-1" selected disabled>extension</option>' +
                  extint_select_build_menu() +
               '</select>'
            )
         )
      );

   check_suspended_audio_state();
	
	w3_el("id-control-freq2").innerHTML =
	   w3_inline('w3-halign-space-between w3-margin-T-4/',
         w3_div('id-mouse-freq',
            w3_div('id-mouse-unit', '-----.--')
         ),
         w3_div('id-link-cell',
            w3_div('id-freq-link|padding-left:0px')
         ),
         w3_div('id-9-10-cell',
            w3_div('id-button-9-10 class-button-small||title="LW/MW 9/10 kHz tuning step" onclick="button_9_10()"', '10')
         ),
         w3_div('id-step-freq',
            '<img id="id-step-0" src="icons/stepdn.20.png" onclick="freqstep(0)" />',
            '<img id="id-step-1" src="icons/stepdn.18.png" onclick="freqstep(1)" style="padding-bottom:1px" />',
            '<img id="id-step-2" src="icons/stepdn.16.png" onclick="freqstep(2)" style="padding-bottom:2px" />',
            '<img id="id-step-3" src="icons/stepup.16.png" onclick="freqstep(3)" style="padding-bottom:2px" />',
            '<img id="id-step-4" src="icons/stepup.18.png" onclick="freqstep(4)" style="padding-bottom:1px" />',
            '<img id="id-step-5" src="icons/stepup.20.png" onclick="freqstep(5)" />'
         ),
         w3_div('',
            w3_button('id-button-spectrum class-button||title="toggle spectrum display"', 'Spec', 'toggle_or_set_spec')
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

	s = '';
   mode_buttons.forEach(
	   function(mba, i) {
	      var ms = mba.s[0];
	      var mslc = ms.toLowerCase();
	      var disabled = mba.dis || (mslc == 'drm' && !kiwi.DRM_enable);
	      var attr = disabled? '' : ' onclick="mode_button(event, this)"';
	      attr += ' onmouseover="mode_over(event, this)"';
	      s += w3_div('id-button-'+ mslc + ' id-mode-'+ mslc.substr(0,2) +
	         ' class-button'+ (disabled? ' class-button-disabled':'') +
	         ' ||id="'+ i +'-id-mode-col"' + attr + ' onmousedown="cancelEvent(event)"', ms);
	   });
	w3_el("id-control-mode").innerHTML = w3_inline('w3-halign-space-between/', s);

   var d = wf.audioFFT_active? ' w3-disabled' : '';
   var d2 = wf.audioFFT_active? ' w3-disabled||onclick="audioFFT_update()"' : '';

	w3_el("id-control-zoom").innerHTML =
	   w3_inline('w3-halign-space-between/',
         w3_div('id-zoom-in class-icon'+ d +'||onclick="zoom_click(event,1)" onmouseover="zoom_over(event)" title="zoom in"', '<img src="icons/zoomin.png" width="32" height="32" />'),
         w3_div('id-zoom-out class-icon'+ d +'||onclick="zoom_click(event,-1)" onmouseover="zoom_over(event)" title="zoom out"', '<img src="icons/zoomout.png" width="32" height="32" />'),
         w3_div('id-maxin'+ d2,
            w3_div('class-icon||onclick="zoom_click(event,8)" title="max in"', '<img src="icons/maxin.png" width="32" height="32" />')
         ),
         w3_div('id-maxin-nom w3-hide'+ d2,
            w3_div('class-icon||onclick="zoom_click(event,8)" title="max in"', '<img src="icons/maxin.nom.png" width="32" height="32" />')
         ),
         w3_div('id-maxin-max w3-hide'+ d2,
            w3_div('class-icon||onclick="zoom_click(event,8)" title="max in"', '<img src="icons/maxin.max.png" width="32" height="32" />')
         ),
         w3_div('id-maxout'+ d2,
            w3_div('class-icon||onclick="zoom_click(event,-9)" title="max out"', '<img src="icons/maxout.png" width="32" height="32" />')
         ),
         w3_div('id-maxout-max w3-hide'+ d2,
            w3_div('class-icon||onclick="zoom_click(event,-9)" title="max out"', '<img src="icons/maxout.max.png" width="32" height="32" />')
         ),
         w3_div('class-icon'+ d +'||onclick="zoom_click(event,0)" title="zoom to band"',
            '<img src="icons/zoomband.png" width="32" height="16" style="padding-top:13px; padding-bottom:13px;"/>'
         ),
         w3_div('class-icon'+ d +'||onclick="page_scroll_icon_click(event,'+ -page_scroll_amount +')" title="page down\nalt: label step"', '<img src="icons/pageleft.png" width="32" height="32" />'),
         w3_div('class-icon'+ d +'||onclick="page_scroll_icon_click(event,'+ page_scroll_amount +')" title="page up\nalt: label step"', '<img src="icons/pageright.png" width="32" height="32" />')
		);


   // optbar
   var optbar_colors = [
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
	
	var psa1 = ' w3-center|width:15.2%';
	var psa2 = psa1 + ';margin-right:6px';
	w3_el('id-optbar').innerHTML =
      w3_navbar('cl-optbar',
         // will call optbar_focus() optbar_blur() when navbar clicked
         w3_navdef(optbar_colors[ci++] + psa2, 'WF', 'optbar-wf', 'optbar'),
         w3_nav(optbar_colors[ci++] + psa2, 'Audio', 'optbar-audio', 'optbar'),
         w3_nav(optbar_colors[ci++] + psa2, 'AGC', 'optbar-agc', 'optbar'),
         w3_nav(optbar_colors[ci++] + psa2, 'Users', 'optbar-users', 'optbar'),
         w3_nav(optbar_colors[ci++] + psa2, 'Stats', 'optbar-status', 'optbar'),
         w3_nav(optbar_colors[ci++] + psa1, 'Off', 'optbar-off', 'optbar')
      );


	wf_filter = readCookie('last_wf_filter', wf_sp_menu_e.OFF);
	spec_filter = readCookie('last_spec_filter', wf_sp_menu_e.IIR);
	
   if (isString(spectrum_show)) {
      var ss = spectrum_show.toLowerCase();
      wf_sp_menu_s.forEach(function(e, i) { if (ss == e.toLowerCase()) spec_filter = i; });
   }
   
   wf.max_min_sliders =
      w3_col_percent('w3-valign/class-slider',
         w3_text(optbar_prefix_color, 'WF max'), 19,
         '<input id="id-input-maxdb" type="range" min="-100" max="20" value="'+ maxdb
            +'" step="1" onchange="setmaxdb(1,this.value)" oninput="setmaxdb(0,this.value)">', 60,
         w3_div('id-field-maxdb class-slider'), 19
      ) +
      w3_col_percent('w3-valign/class-slider',
         w3_text(optbar_prefix_color, 'WF min'), 19,
         '<input id="id-input-mindb" type="range" min="-190" max="-30" value="'+ mindb
            +'" step="1" onchange="setmindb(1,this.value)" oninput="setmindb(0,this.value)">', 60,
         w3_div('id-field-mindb class-slider'), 19
      );

   wf.floor_ceil_sliders =
      w3_col_percent('w3-valign/class-slider',
         w3_text(optbar_prefix_color, 'WF ceil'), 19,
         '<input id="id-input-ceildb" type="range" min="-30" max="30" value="'+ wf.auto_ceil.val
            +'" step="5" onchange="setceildb(1,this.value)" oninput="setceildb(0,this.value)">', 60,
         w3_div('id-field-ceildb class-slider'), 19
      ) +
      w3_col_percent('w3-valign/class-slider',
         w3_text(optbar_prefix_color, 'WF floor'), 19,
         '<input id="id-input-floordb" type="range" min="-30" max="30" value="'+ wf.auto_floor.val
            +'" step="5" onchange="setfloordb(1,this.value)" oninput="setfloordb(0,this.value)">', 60,
         w3_div('id-field-floordb class-slider'), 19
      );

   // wf
	w3_el("id-optbar-wf").innerHTML =
      w3_div('id-aper-data w3-hide w3-margin-B-5|left:0px; width:256px; height:16px; background-color:#575757; overflow:hidden; position:relative;',
   		'<canvas id="id-aper-canvas" width="256" height="16" style="position:absolute"></canvas>'
      ) +
      
      w3_col_percent('',
         w3_div('',
            w3_div('id-wf-sliders', wf.max_min_sliders),
            
            w3_col_percent('w3-valign/class-slider',
               w3_text(optbar_prefix_color, 'WF rate'), 19,
               '<input id="slider-rate" class="'+ d +'" type="range" min="0" max="4" value="'+
                  wf_speed +'" step="1" onchange="setwfspeed(1,this.value)" oninput="setwfspeed(0,this.value)">', 60,
               w3_div('slider-rate-field class-slider'), 19
            ),

            w3_col_percent('w3-valign/class-slider',
               //w3_text(optbar_prefix_color, 'SP parm'), 19,
               w3_button('id-wf-sp-button class-button w3-font-12px', 'Spec &Delta;', 'toggle_or_set_wf_sp_button'), 19,
               w3_slider('id-wf-sp-slider', '', 'wf_sp_param', wf_sp_param, 0, 10, 1, 'wf_sp_slider_cb'), 60,
               w3_div('id-wf-sp-slider-field class-slider'), 19
            )
         ), 85,
         
         w3_divs('/w3-tspace-4 w3-hcenter w3-font-11_25px',
            w3_button('id-button-wf-autoscale class-button||title="waterfall auto scale"', 'Auto<br>Scale', 'wf_autoscale_cb'),
            w3_button('id-button-slow-dev class-button||title="slow device mode"', 'Slow<br>Dev', 'toggle_or_set_slow_dev'),
            w3_inline('',
               w3_button('id-button-spec-peak class-button||title="toggle peak hold"', 'Pk', 'toggle_or_set_spec_peak'),
               w3_icon('id-wf-gnd w3-margin-L-4 w3-momentary', 'fa-caret-down', 22, 'white', 'wf_gnd_cb', 1)
            )
         ), 15
      ) +

      w3_inline('w3-halign-space-between w3-margin-T-2/',
         w3_select('w3-text-red||title="colormap selection"', '', 'colormap', 'wf.cmap', wf.cmap, kiwi.cmap_s, 'wf_cmap_cb'),
         w3_select('w3-text-red||title="aperture selection"', '', 'aper', 'wf.aper', wf.aper, kiwi.aper_s, 'wf_aper_cb'),
         w3_select('w3-text-red||title="waterfall filter selection"', '', 'wf', 'wf_filter', wf_filter, wf_sp_menu_s, 'wf_sp_menu_cb', 1),
         w3_select('w3-text-red||title="spectrum filter selection"', '', 'spec', 'spec_filter', spec_filter, wf_sp_menu_s, 'wf_sp_menu_cb', 0),
         w3_button('id-button-wf-more class-button w3-text-orange||onclick="extint_open(\'waterfall\'); freqset_select();" title="more waterfall params"', 'More')

         //w3_select('w3-text-red', '', 'contrast', 'wf.contr', W3_SELECT_SHOW_TITLE, wf_contr_s, 'wf_contr_cb'),
         //w3_div('w3-hcenter', w3_button('id-button-spec-peak class-button', 'Peak', 'toggle_or_set_spec_peak'))
         //w3_button('id-button-wf-gnd class-button w3-momentary', 'G', 'wf_gnd_cb', 1)
      );

   setwfspeed(1, wf_speed);
   toggle_or_set_slow_dev(toggle_e.FROM_COOKIE | toggle_e.SET, 0);
   toggle_or_set_spec_peak(toggle_e.FROM_COOKIE | toggle_e.SET_URL, peak_initially);
   toggle_or_set_wf_sp_button(toggle_e.FROM_COOKIE | toggle_e.SET, 0);

   // audio & nb
   var nb_algo_s = [ ['off',1], ['std',1], ['Wild',1] ];
   var nr_algo_s = [ ['off',1], ['wdsp',1], ['LMS',1], ['spec',1] ];
	de_emphasis = readCookie('last_de_emphasis', 0);
	de_emphasis = w3_clamp(de_emphasis, 0, 1, 0);
	pan = readCookie('last_pan', 0);

	w3_el('id-optbar-audio').innerHTML =
		w3_inline_percent('w3-valign/w3-last-halign-end',
			w3_text(optbar_prefix_color +' cl-closer-spaced-label-text', 'Noise'), 17,
         w3_select_conditional('w3-text-red||title="noise blanker selection"', '', 'blanker', 'nb_algo', 0, nb_algo_s, 'nb_algo_cb'), 24,
			w3_div('w3-hcenter', w3_div('class-button w3-text-orange||onclick="extint_open(\'noise_blank\'); freqset_select();" title="noise blanker parameters"', 'More')), 19,
         w3_div('w3-hcenter ', w3_select_conditional('w3-text-red||title="noise filter selection"', '', 'filter', 'nr_algo', 0, nr_algo_s, 'nr_algo_cb')), 27,
			w3_div('class-button w3-text-orange||onclick="extint_open(\'noise_filter\'); freqset_select();" title="noise filter parameters"', 'More')
		) +
		w3_inline_percent('id-vol w3-valign w3-margin-T-2 w3-hide/class-slider w3-last-halign-end',
			w3_text(optbar_prefix_color, 'Volume'), 17,
			'<input id="id-input-volume" type="range" min="0" max="200" value="'+ kiwi.volume +'" step="1" onchange="setvolume(1, this.value)" oninput="setvolume(0, this.value)">', 50,
         '&nbsp;', 8,
         w3_select('w3-text-red||title="de-emphasis selection"', '', 'de-emp', 'de_emphasis', de_emphasis, de_emphasis_s, 'de_emp_cb')
		) +
		w3_inline_percent('id-vol-comp w3-valign w3-margin-T-2/class-slider w3-last-halign-end',
			w3_text(optbar_prefix_color, 'Volume'), 17,
			'<input id="id-input-volume" type="range" min="0" max="200" value="'+ kiwi.volume +'" step="1" onchange="setvolume(1, this.value)" oninput="setvolume(0, this.value)">', 40,
         w3_select('w3-text-red', '', 'de-emp', 'de_emphasis', de_emphasis, de_emphasis_s, 'de_emp_cb'), 28,
		   w3_button('id-button-compression class-button w3-hcenter||title="compression"', 'Comp', 'toggle_or_set_compression')
		) +
      w3_inline_percent('id-pan w3-valign w3-hide/class-slider w3-last-halign-end',
         w3_text(optbar_prefix_color, 'Pan'), 17,
         '<input id="id-pan-value" type="range" min="-1" max="1" value="'+ pan +'" step="0.01" onchange="setpan(1,this.value)" oninput="setpan(0,this.value)">', 50,
         '&nbsp;', 3, w3_div('id-pan-field'), 8, '&nbsp;', 7,
		   w3_button('id-button-compression class-button w3-hcenter||title="compression"', 'Comp', 'toggle_or_set_compression')
      ) +
      w3_inline_percent('id-squelch w3-valign/class-slider w3-last-halign-end',
			w3_text('id-squelch-label', 'Squelch'), 17,
         w3_slider('', '', 'squelch-value', squelch, 0, 99, 1, 'set_squelch_cb'), 50,
         '&nbsp;', 3, w3_div('id-squelch-field class-slider'), 14,
         w3_select('id-squelch-tail w3-hide w3-text-red||title="squelch tail length"', '', 'tail', 'squelch_tail', squelch_tail, squelch_tail_s, 'squelch_tail_cb')
	   ) +
      w3_inline_percent('id-sam-carrier-container w3-hide w3-valign/class-slider w3-last-halign-end',
         w3_text(optbar_prefix_color, 'SAM'), 17,
         w3_text('id-sam-carrier'), 42,
         '&nbsp;', 23,
         w3_select('w3-text-red', '', 'PLL', 'owrx.sam_pll', owrx.sam_pll, owrx.sam_pll_s, 'sam_pll_cb')
      ) +
      w3_inline('w3-margin-T-2 w3-valign w3-halign-end/class-slider',
         w3_select('id-chan-null w3-text-red w3-hide', '', 'channel<br>null', 'owrx.chan_null', owrx.chan_null, owrx.chan_null_s, 'chan_null_cb'),
         w3_select('id-ovld-mute w3-text-red', '', 'ovld<br>mute', 'owrx.ovld_mute', owrx.ovld_mute, owrx.ovld_mute_s, 'ovld_mute_cb')
      );
      
      //w3_button('id-button-test class-button w3-hcenter w3-hide', 'Test', 'toggle_or_set_test')

   kiwi_load_js_dir('extensions/', ['noise_blank/noise_blank.js', 'noise_filter/noise_filter.js'],
      function() {
         noise_blank_init();
         noise_filter_init();
      }
   );

   //toggle_or_set_test(0);
   //w3_show('id-button-test', dbgUs);
   //toggle_or_set_audio(toggle_e.FROM_COOKIE | toggle_e.SET, 1);
   toggle_or_set_compression(toggle_e.FROM_COOKIE | toggle_e.SET, 1);
	squelch_setup(toggle_e.FROM_COOKIE);
   audio_panner_ui_init();


   // agc
	w3_el('id-optbar-agc').innerHTML =
		w3_col_percent('w3-valign w3-margin-B-4/class-slider',
			'<div id="id-button-agc" class="class-button" onclick="toggle_agc(event)" onmousedown="cancelEvent(event)" onmouseover="agc_over(event)">AGC</div>', 13,
			'<div id="id-button-hang" class="class-button" onclick="toggle_or_set_hang();">Hang</div>', 17,
			w3_divs('w3-show-inline-block/id-label-man-gain cl-closer-spaced-label-text', 'Manual<br>gain'), 15,
			'<input id="input-man-gain" type="range" min="0" max="120" value="'+ manGain +'" step="1" onchange="setManGain(1,this.value)" oninput="setManGain(0,this.value)">', 40,
			w3_div('field-man-gain w3-show-inline-block', manGain.toString()) +' dB', 15
		) +
		w3_div('',
			w3_col_percent('w3-valign/class-slider',
				w3_div('label-threshold w3-show-inline-block', 'Threshold'), 20,
				'<input id="input-threshold" type="range" min="-130" max="0" value="'+ thresh +'" step="1" onchange="setThresh(1,this.value)" oninput="setThresh(0,this.value)">', 52,
				w3_div('field-threshold w3-show-inline-block', thresh.toString()) +' dBm'
			),
			w3_col_percent('w3-valign/class-slider',
				w3_div('label-threshCW w3-show-inline-block', 'Thresh CW'), 20,
				'<input id="input-threshCW" type="range" min="-130" max="0" value="'+ threshCW +'" step="1" onchange="setThreshCW(1,this.value)" oninput="setThreshCW(0,this.value)">', 52,
				w3_div('field-threshCW w3-show-inline-block', threshCW.toString()) +' dBm'
			),
			w3_col_percent('w3-valign/class-slider',
				w3_div('label-slope w3-show-inline-block', 'Slope'), 20,
				'<input id="input-slope" type="range" min="0" max="10" value="'+ slope +'" step="1" onchange="setSlope(1,this.value)" oninput="setSlope(0,this.value)">', 52,
				w3_div('field-slope w3-show-inline-block', slope.toString()) +' dB'
			),
			w3_col_percent('w3-valign/class-slider',
				w3_div('label-decay w3-show-inline-block', 'Decay'), 20,
				'<input id="input-decay" type="range" min="20" max="5000" value="'+ decay +'" step="1" onchange="setDecay(1,this.value)" oninput="setDecay(0,this.value)">', 52,
				w3_div('field-decay w3-show-inline-block', decay.toString()) +' msec'
			)
		);
	setup_agc(toggle_e.FROM_COOKIE | toggle_e.SET);

	
	// users
	
	
	// status
	
	
	// optbar_setup
	//console.log('optbar_setup');
   w3_click_nav(kiwi_toggle(toggle_e.FROM_COOKIE | toggle_e.SET, 'optbar-wf', 'optbar-wf', 'last_optbar'), 'optbar', 'init');
	

	// id-news
	w3_el('id-news').style.backgroundColor = news_color;
	w3_el("id-news-inner").innerHTML =
		'<span style="font-size: 14pt; font-weight: bold;">' +
			'KiwiSDR now available on ' +
			'<a href="https://www.kickstarter.com/projects/1575992013/kiwisdr-beaglebone-software-defined-radio-sdr-with" target="_blank">' +
				'<img class="class-KS" src="icons/kickstarter-logo-light.150x18.png" />' +
			'</a>' +
		'</span>' +
		'';


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

	w3_el("id-readme-inner").innerHTML =
		'<span style="font-size: 15pt; font-weight: bold;">Welcome!</span>' +
		'&nbsp;&nbsp;&nbsp;Project website: <a href="http://kiwisdr.com" target="_blank">kiwisdr.com</a>&nbsp;&nbsp;&nbsp;&nbsp;Here are some tips:' +
		'<ul style="padding-left: 12px;">' +
		'<li> Windows: Firefox, Chrome & Edge work; IE does not work. </li>' +
		'<li> Mac & Linux: Safari, Firefox, Chrome & Opera should work fine. </li>' +
		'<li> Open and close the panels by using the circled arrows at the top right corner. </li>' +
		'<li> You can click and/or drag almost anywhere on the page to change settings. </li>' +
		'<li> Enter a numeric frequency in the box marked "kHz" at right. </li>' +
		'<li> Or use the "select band" menu to jump to a pre-defined band. </li>' +
		'<li> Use the zoom icons to control the waterfall span. </li>' +
		'<li> Tune by clicking on the waterfall, spectrum or the cyan/red-colored station labels. </li>' +
		'<li> Ctrl-shift or alt-shift click in the waterfall to lookup frequency in online databases. </li>' +
		'<li> Control or option/alt click to page spectrum down and up in frequency. </li>' +
		'<li> Adjust the "WF min" slider for best waterfall colors or use the "Auto Scale" button. </li>' +
		"<li> Type 'h' or '?' to see the list of keyboard shortcuts. </li>" +
		'<li> See the <a href="http://www.kiwisdr.com/quickstart/" target="_blank">Operating information</a> page' +
		     'and <a href="https://www.dropbox.com/s/i1bjyp1acghnc16/KiwiSDR.design.review.pdf?dl=1" target="_blank">Design review document</a>. </li>' +
		'</ul>';


	// id-msgs
	
	var contact_admin = ext_get_cfg_param('contact_admin');
	if (isUndefined(contact_admin)) {
		// hasn't existed before: default to true
		//console.log('contact_admin: INIT true');
		contact_admin = true;
	}

	var admin_email = ext_get_cfg_param('admin_email');
	//console.log('contact_admin='+ contact_admin +' admin_email='+ admin_email);
	admin_email = (contact_admin != 'undefined' && contact_admin != null && contact_admin == true && admin_email != 'undefined' && admin_email != null)? admin_email : null;
	
	// NB: have to double encode here because the "javascript:sendmail()" href below automatically undoes
	// one level of encoding causing problems when an email containing an underscore gets enc() to a backslash
	if (admin_email != null) admin_email = encodeURIComponent(encodeURIComponent(enc(admin_email)));

   w3_el('id-optbar-status').innerHTML =
		w3_div('id-status-msg') +
		w3_div('',
	      w3_text(optbar_prefix_color, 'Links'),
	      w3_text('',
            (admin_email? '<a href="javascript:sendmail(\''+ admin_email +'\');">Owner/Admin</a> | ' : '') +
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
		w3_div('id-status-stats-xfer');

	setTimeout(function() { setInterval(status_periodic, 5000); }, 1000);
}


////////////////////////////////
// option bar
////////////////////////////////

var prev_optbar = null;

function optbar_focus(next_id, cb_arg)
{
   writeCookie('last_optbar', next_id);
   
   var h;
   if (next_id == 'optbar-off') {
      if (cb_arg != 'init' && prev_optbar == 'optbar-off') toggle_or_set_hide_bars();
      w3_hide('id-optbar-content');
      w3_el('id-control-top').style.paddingBottom = '8px';
   } else {
      w3_show_block('id-optbar-content');
      w3_el('id-control-top').style.paddingBottom = '';     // let id-optbar-content determine all spacing
   }
   w3_el('id-control').style.height = '';    // undo spec in index.html
   prev_optbar = next_id;
   freqset_select();
}

function optbar_blur(cur_id)
{
   //console.log('optbar_blur='+ cur_id);
}


function zoomCorrection()
{
	return 3/*dB*/ * zoom_level;
	//return 0 * zoom_level;		// gives constant noise floor when using USE_GEN
}


////////////////////////////////
// waterfall / spectrum controls
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
   url_tstamp: 0,
   ts_tz: 0,
   
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
   
   auto_ceil: { min:0, val:5, max:30 },
   auto_floor: { min:-30, val:0, max:0 },
   auto_maxdb: 0,
   auto_mindb: 0,
   
   audioFFT_active: false,
   audioFFT_prev_mode: '',
   audioFFT_clear_wf: false,
};

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
   toggle_or_set_spec(toggle_e.SET, 1);
   
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

function wf_sp_menu_cb(path, idx, first, param)    // param indicated which menu, wf or sp
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
	writeCookie(wf_sp_which? 'last_wf_filter':'last_spec_filter', f_val.toString());

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
      var lsf = parseFloat(readCookie(wf_sp_which? 'last_wf_filter':'last_spec_filter'));
      var lsfp = parseFloat(readCookie(wf_sp_which? 'last_wf_filter_param':'last_spec_filter_param'));
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
   w3_el('id-wf-sp-slider-field').innerHTML = (f_val == wf_sp_menu_e.OFF)? '' : (val +' '+ wf_sp_slider_s[f_val]);

   if (done) {
	   //console.log('wf_sp_slider_cb DONE WRITE_COOKIE last_filter_param='+ val.toFixed(2) +' which='+ wf_sp_which);
	   writeCookie(wf_sp_which? 'last_wf_filter_param':'last_spec_filter_param', val.toFixed(2));
      freqset_select();
   }

   //console.log('wf_sp_slider_cb EXIT path='+ path);
}

function wf_cmap_cb(path, idx, first)
{
   if (first) return;      // colormap_init() handles setup
   idx = +idx;
   wf.cmap = w3_clamp(idx, 0, kiwi.cmap_s.length - 1, 0);
   //console_log_dbgUs('# wf_cmap_cb idx='+ idx +' first='+ first +' wf.cmap='+ wf.cmap +' audioFFT_active='+ wf.audioFFT_active);
   w3_select_value(path, idx, { all:1 });    // all:1 changes both menus together
   writeCookie('last_cmap', idx);
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
   wf.aper = w3_clamp(idx, 0, kiwi.aper_s.length - 1, 0);
   //console_log_dbgUs('# wf_aper_cb idx='+ idx +' first='+ first +' wf.aper='+ wf.aper +' audioFFT_active='+ wf.audioFFT_active);
   w3_select_value(path, idx);
   writeCookie('last_aper', idx);

   var auto = (wf.aper == kiwi.aper_e.auto);
   w3_innerHTML('id-wf-sliders', auto? wf.floor_ceil_sliders : wf.max_min_sliders);
   w3_color('id-button-wf-autoscale', null, auto? w3_selection_green : '');

   if (auto) {
      wf.save_maxdb = maxdb;
      wf.save_mindb_un = mindb_un;
      //console_log_dbgUs('# save maxdb='+ maxdb +' mindb='+ mindb);

      if (wf.audioFFT_active)
         wf.need_autoscale = 16;    // delay a bit before autoscaling
   } else {
      //console_log_dbgUs('# restore maxdb='+ wf.save_maxdb +' mindb='+ wf.save_mindb_un +' zcorr='+ zoomCorrection());
      setmaxdb(1, wf.save_maxdb);
      setmindb(1, wf.save_mindb_un - zoomCorrection());
   }

   spectrum_dB_bands();
   update_maxmindb_sliders();
   w3_show_hide_inline('id-wfext-maxmin', auto);
   w3_show_hide_inline('id-wfext-maxmin-spacer', !auto);
   colormap_update();
   freqset_select();
}

function setwfspeed(done, str)
{
   if (wf.audioFFT_active) {
      freqset_select();
      return;
   }
   
	wf_speed = +str;
	//console.log('# setwfspeed '+ wf_speed +' done='+ done);
	w3_set_value('slider-rate', wf_speed)
   w3_el('slider-rate-field').innerHTML = wf_speeds[wf_speed];
   w3_el('slider-rate-field').style.color = wf_speed? 'white':'orange';
   wf_send('SET wf_speed='+ wf_speed.toFixed(0));
   if (done) freqset_select();
}

function setmaxdb(done, str, no_freqset_select)
{
	var strdb = parseFloat(str);
	var write_cookies = false;

   if (wf.aper == kiwi.aper_e.auto) {
      maxdb = strdb + wf.auto_ceil.val;
   } else {
      var input_max = w3_el('id-input-maxdb');
      var field_max = w3_el('id-field-maxdb');
      var field_min = w3_el('id-field-mindb');
      if (!input_max || !field_max || !field_min) return;
      write_cookies = true;

      if (strdb <= mindb) {
         maxdb = mindb + 1;
         html(input_max).value = maxdb;
         html(field_max).innerHTML = maxdb.toFixed(0) + ' dB';
         html(field_max).style.color = "red"; 
      } else {
         maxdb = strdb;
         html(field_max).innerHTML = strdb.toFixed(0) + ' dB';
         html(field_max).style.color = "white"; 
         html(field_min).style.color = "white";
      }
   }
	
	setmaxmindb(done, write_cookies, no_freqset_select);
}

function incr_mindb(done, incr)
{
   if (wf.aper != kiwi.aper_e.man) return;
   var incrdb = (+mindb) + incr;
   var val = Math.max(-190, Math.min(-30, incrdb));
   //console.log('incr_mindb mindb='+ mindb +' incrdb='+ incrdb +' val='+ val);
   setmindb(done, val.toFixed(0));
}

function setmindb(done, str, no_freqset_select)
{
	var strdb = parseFloat(str);
   //console.log('setmindb strdb='+ strdb +' maxdb='+ maxdb +' mindb='+ mindb +' done='+ done);
	var write_cookies = false;

   if (wf.aper == kiwi.aper_e.auto) {
      mindb = strdb + wf.auto_floor.val;
   } else {
      var input_min = w3_el('id-input-mindb');
      var field_max = w3_el('id-field-maxdb');
      var field_min = w3_el('id-field-mindb');
      if (!input_min || !field_max || !field_min) return;
      write_cookies = true;

      if (maxdb <= strdb) {
         mindb = maxdb - 1;
         html(input_min).value = mindb;
         html(field_min).innerHTML = mindb.toFixed(0) + ' dB';
         html(field_min).style.color = "red";
      } else {
         mindb = strdb;
         //console.log('setmindb SET strdb='+ strdb +' maxdb='+ maxdb +' mindb='+ mindb +' done='+ done);
         html(input_min).value = mindb;
         html(field_min).innerHTML = strdb.toFixed(0) + ' dB';
         html(field_min).style.color = "white";
         html(field_max).style.color = "white";
      }
   }

	mindb_un = mindb + zoomCorrection();
	setmaxmindb(done, write_cookies, no_freqset_select);
}

function setmaxmindb(done, write_cookies, no_freqset_select)
{
	full_scale = maxdb - mindb;
	full_scale = full_scale? full_scale : 1;	// can't be zero
	spectrum_dB_bands();
   wf_send("SET maxdb="+maxdb.toFixed(0)+" mindb="+mindb.toFixed(0));
	need_clear_wf_sp_avg = true;

   if (done) {
      if (no_freqset_select != true)
   	   freqset_select();
   	if (write_cookies) {
   	   writeCookie(wf.audioFFT_active? 'last_AF_max_dB' : 'last_max_dB', maxdb.toFixed(0));
   	   writeCookie(wf.audioFFT_active? 'last_AF_min_dB' : 'last_min_dB', mindb_un.toFixed(0));	// need to save the uncorrected (z0) value
	   }
	}
}

function update_maxmindb_sliders()
{
	var auto = (wf.aper == kiwi.aper_e.auto);
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
}

function setceildb(done, str)
{
   var input_ceil = w3_el('id-input-ceildb');
   var field_ceil = w3_el('id-field-ceildb');
   if (!input_ceil || !field_ceil) return;

	var ceildb = parseFloat(str);
   w3_innerHTML(field_ceil, ceildb.toFixed(0).positiveWithSign() + ' dB');
   wf.auto_ceil.val = ceildb;
	set_ceilfloordb(done);
}

function setfloordb(done, str)
{
   var input_floor = w3_el('id-input-floordb');
   var field_floor = w3_el('id-field-floordb');
   if (!input_floor || !field_floor) return;

	var floordb = parseFloat(str);
   w3_innerHTML(field_floor, floordb.toFixed(0).positiveWithSign() + ' dB');
   wf.auto_floor.val = floordb;
	set_ceilfloordb(done);
}

function set_ceilfloordb(done)
{
   maxdb = wf.auto_maxdb + wf.auto_ceil.val;
   mindb = wf.auto_mindb + wf.auto_floor.val;
	mindb_un = mindb + zoomCorrection();
   //console_log_dbgUs('# set_ceilfloordb min/max='+ mindb +'/'+ maxdb +' min_un='+ mindb_un);
	setmaxmindb(done, false);
	w3_call('waterfall_maxmin_cb');

   if (done) {
   	freqset_select();
   	writeCookie('last_ceil_dB', wf.auto_ceil.val.toFixed(0));
   	writeCookie('last_floor_dB', wf.auto_floor.val.toFixed(0));
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
	writeCookie('last_slow_dev', spectrum_slow_dev.toString());
	if (spectrum_slow_dev && wf_speed == WF_SPEED_FAST)
	   setwfspeed(1, WF_SPEED_MED);
}

function toggle_or_set_spec_peak(set, val)
{
	if (isNumber(set))
		spec.peak_show = kiwi_toggle(set, val, spec.peak_show, 'last_spec_peak');
	else
		spec.peak_show ^= 1;
	if (spec.peak_show) spec.peak_clear = true;
	w3_color('id-button-spec-peak', spec.peak_show? 'lime':'white');
	freqset_select();
	writeCookie('last_spec_peak', spec.peak_show.toString());
}


////////////////////////////////
// audio
////////////////////////////////

var muted_until_freq_set = true;
var recording = false;

function setvolume(done, str)
{
   //console.log('setvolume str='+ str +' t/o='+ typeof(str));
   kiwi.volume = +str
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
      writeCookie('last_volume', kiwi.volume);
      freqset_select();
   }
}

function toggle_or_set_mute(set)
{
	if (isNumber(set))
      kiwi.muted = set;
   else
	   kiwi.muted ^= 1;
   //console.log('toggle_or_set_mute set='+ set +' muted='+ kiwi.muted);
	if (!owrx.squelched_overload) {
      w3_show_hide('id-mute-no', !kiwi.muted);
      w3_show_hide('id-mute-yes', kiwi.muted);
   }
   kiwi.volume_f = (kiwi.muted || kiwi.volume == 0)? 1e-6 : (kiwi.volume/100);
	//if (!isNumber(set)) console.log('vol='+ kiwi.volume +' muted='+ kiwi.muted +' vol_f='+ kiwi.volume_f.toFixed(6));
   freqset_select();
}

var de_emphasis = 0;
//var de_emphasis_s = [ 'off', '75us', '50us' ];
var de_emphasis_s = [ 'off', 'on' ];

function de_emp_cb(path, idx, first)
{
   de_emphasis = +idx;
	snd_send('SET de_emp='+ de_emphasis);
   writeCookie('last_de_emphasis', de_emphasis.toString());
}

var pan = 0;

function setpan(done, str)
{
   var pan = +str;
   //console.log('pan='+ pan.toFixed(1));
   if (pan > -0.1 && pan < +0.1) pan = 0;    // snap-to-zero interval
   w3_set_value('id-pan-value', pan);
   w3_color('id-pan-value', null, (pan < 0)? 'rgba(255,0,0,0.3)' : (pan? 'rgba(0,255,0,0.2)':''));
   w3_innerHTML('id-pan-field', pan? ((Math.abs(pan)*100).toFixed(0) +' '+ ((pan < 0)? 'L':'R')) : 'L=R');
   audio_set_pan(pan);
   if (done) {
      writeCookie('last_pan', pan.toString());
      freqset_select();
   }
}

// called from both audio_init() and panels_setup() since there is a race setting audio_panner
function audio_panner_ui_init()
{
   if (audio_panner) {
      w3_hide2('id-vol-comp');
      w3_hide2('id-vol', false);
      w3_hide2('id-pan', false);
      setpan(1, pan);
   }
}


function toggle_or_set_hide_bars(set)
{
	if (isNumber(set))
      owrx.hide_bars = set;
   else {
	   owrx.hide_bars = (owrx.hide_bars + 1) & owrx.HIDE_ALLBARS;
	   
	   // there is no top container to hide if data container or spectrum in use
      if ((owrx.hide_bars & owrx.HIDE_TOPBAR) && (extint.using_data_container || spec.source_wf || spec.source_audio))
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
   w3_remove_then_add('id-rec1', 'fa-spin', recording? 'fa-spin':'');
   w3_remove_then_add('id-rec2', 'fa-spin', recording? 'fa-spin':'');
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
         filename: kiwi_host() +'_'+ new Date().toISOString().replace(/:/g, '_').replace(/\.[0-9]+Z$/, 'Z') +'_'+ w3_el('id-freq-input').value +'_'+ cur_mode +'.wav'
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
   }
}

// squelch
var squelched = 0;
var squelch = 0;
var squelch_tail = 0;
var squelch_tail_s = [ '0', '.2s', '.5s', '1s', '2s' ];
var squelch_tail_v = [ 0, 0.2, 0.5, 1, 2 ];

function squelch_action(sq)
{
   squelched = sq;
   //console.log('squelch_action squelched='+ (squelched? 1:0) +' sMeter_dBm='+ owrx.sMeter_dBm.toFixed(0));
   var mute_color = squelched? 'white' : kiwi.unmuted_color;
   var sq_color = mute_color;

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
      w3_remove_then_add('id-rec1', 'fa-spin', stop? '':'fa-spin');
      w3_remove_then_add('id-rec2', 'fa-spin', stop? '':'fa-spin');
      w3_remove_then_add('id-rec1', 'w3-text-white w3-text-pink', stop? 'w3-text-white' : 'w3-text-pink');
      w3_remove_then_add('id-rec2', 'w3-text-white w3-text-pink', stop? 'w3-text-white' : 'w3-text-pink');
   }
}

function squelch_setup(flags)
{
   var nbfm = (cur_mode == 'nbfm');
   
   if (flags & toggle_e.FROM_COOKIE) { 
      var sq = readCookie('last_squelch'+ (nbfm? '':'_efm'));
      squelch = sq? +sq : 0;
      //console.log('$squelch_setup nbfm='+ nbfm +' sq='+ sq +' squelch='+ squelch);
      w3_el('id-squelch-value').max = nbfm? 99:40;
      w3_set_value('id-squelch-value', squelch);
   }
   
   set_squelch_cb('', squelch, true, false, true);

	if (nbfm) {
	   squelch_action(squelched);
   } else {
      w3_color('id-mute-no', kiwi.unmuted_color);
   }

   w3_hide2('id-squelch-tail', nbfm);
   w3_hide2('id-squelch', cur_mode == 'drm');
}

function send_squelch()
{
   var nbfm = (cur_mode == 'nbfm');
   snd_send("SET squelch="+ squelch.toFixed(0) +' param='+ (nbfm? squelch_threshold : squelch_tail_v[squelch_tail]).toFixed(2));
}

function set_squelch_cb(path, str, done, first, no_write_cookie)
{
   var nbfm = (cur_mode == 'nbfm');
   //console.log('$set_squelch_cb path='+ path +' str='+ str +' done='+ done +' first='+ first +' no_write_cookie='+ no_write_cookie +' nbfm='+ nbfm);
   if (first) return;

   squelch = parseFloat(str);
	w3_el('id-squelch-field').innerHTML = squelch? (str + (nbfm? '':' dB')) : 'off';
   w3_color('id-squelch-value', null, squelch? '' : 'rgba(255,0,0,0.3)');

   if (done) {
      send_squelch();
      if (no_write_cookie != true) {
         writeCookie('last_squelch'+ (nbfm? '':'_efm'), str);
      }
      freqset_select();
   }
}

function squelch_tail_cb(path, val, first)
{
   //console.log('squelch_tail_cb path='+ path +' val='+ val +' first='+ first);
   if (first) return;
   squelch_tail = +val;
   send_squelch();
}

function sam_pll_cb(path, val, first)
{
   //console.log('$sam_pll_cb path='+ path +' val='+ val +' first='+ first);
   owrx.sam_pll = +val;
   snd_send('SET sam_pll='+ owrx.sam_pll);
}

function chan_null_cb(path, val, first)
{
   //console.log('$chan_null_cb path='+ path +' val='+ val +' first='+ first);
   if (first) return;   // cur_mode still undefined this early
   owrx.chan_null = +val;
   ext_set_mode(cur_mode);
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
	writeCookie('last_audio', btn_less_buffering.toString());
	
	// if toggling (i.e. not the first time during setup) reinitialize audio with specified buffering
	if (!isNumber(set)) {
	   // haven't got audio_init() calls to work yet (other than initial)
	   //audio_init(0, null, btn_less_buffering, btn_compression);
      window.location.reload(true);
   }
}

var btn_compression = 0;

function toggle_or_set_compression(set, val)
{
   // Prevent compression setting changes while recording.
   if (recording) {
      return;
   }

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
	writeCookie('last_compression', btn_compression.toString());
	//console.log('SET compression='+ btn_compression.toFixed(0));
	snd_send('SET compression='+ btn_compression.toFixed(0));
}


////////////////////////////////
// AGC
////////////////////////////////

function set_agc()
{
   var isCW = (cur_mode && cur_mode.substr(0,2) == 'cw');
   //console.log('isCW='+ isCW +' agc='+ agc);
   w3_color('label-threshold', (!isCW && agc)? 'orange' : 'white');
   w3_color('label-threshCW',  ( isCW && agc)? 'orange' : 'white');
   var thold = isCW? threshCW : thresh;
   //console.log('AGC SET thresh='+ thold);
	snd_send('SET agc='+ agc +' hang='+ hang +' thresh='+ thold +' slope='+ slope +' decay='+ decay +' manGain='+ manGain);
}

var agc = 0;

function toggle_agc(evt)
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
	manGain = kiwi_toggle(toggle_flags, default_manGain, manGain, 'last_manGain'); setManGain(true, manGain);
	thresh = kiwi_toggle(toggle_flags, default_thresh, thresh, 'last_thresh'); setThresh(true, thresh);
	threshCW = kiwi_toggle(toggle_flags, default_threshCW, threshCW, 'last_threshCW'); setThreshCW(true, threshCW);
	slope = kiwi_toggle(toggle_flags, default_slope, slope, 'last_slope'); setSlope(true, slope);
	decay = kiwi_toggle(toggle_flags, default_decay, decay, 'last_decay'); setDecay(true, decay);
}

function toggle_or_set_agc(set)
{
	if (set != undefined)
		agc = set;
	else
		agc ^= 1;
	
	html('id-button-agc').style.color = agc? 'lime':'white';
	if (agc) {
		html('id-label-man-gain').style.color = 'white';
		html('id-button-hang').style.borderColor = html('label-slope').style.color = html('label-decay').style.color = 'orange';
	} else {
		html('id-label-man-gain').style.color = 'orange';
		html('id-button-hang').style.borderColor = html('label-slope').style.color = html('label-decay').style.color = 'white';
	}
	set_agc();
	writeCookie('last_agc', agc.toString());
   freqset_select();
}

var hang = 0;

function toggle_or_set_hang(set)
{
	if (set != undefined)
		hang = set;
	else
		hang ^= 1;

	html('id-button-hang').style.color = hang? 'lime':'white';
	set_agc();
	writeCookie('last_hang', hang.toString());
   freqset_select();
}

var manGain = 0;

function setManGain(done, str)
{
   manGain = parseFloat(str);
   html('input-man-gain').value = manGain;
   html('field-man-gain').innerHTML = str;
	set_agc();
	writeCookie('last_manGain', manGain.toString());
   if (done) freqset_select();
}

var thresh = 0;

function setThresh(done, str)
{
   thresh = parseFloat(str);
   html('input-threshold').value = thresh;
   html('field-threshold').innerHTML = str;
	set_agc();
	writeCookie('last_thresh', thresh.toString());
   if (done) freqset_select();
}

var threshCW = 0;

function setThreshCW(done, str)
{
   threshCW = parseFloat(str);
   html('input-threshCW').value = threshCW;
   html('field-threshCW').innerHTML = str;
	set_agc();
	writeCookie('last_threshCW', threshCW.toString());
   if (done) freqset_select();
}

var slope = 0;

function setSlope(done, str)
{
   slope = parseFloat(str);
   html('input-slope').value = slope;
   html('field-slope').innerHTML = str;
	set_agc();
	writeCookie('last_slope', slope.toString());
   if (done) freqset_select();
}

var decay = 0;

function setDecay(done, str)
{
   decay = parseFloat(str);
   html('input-decay').value = decay;
   html('field-decay').innerHTML = str;
	set_agc();
	writeCookie('last_decay', decay.toString());
   if (done) freqset_select();
}


////////////////////////////////
// users
////////////////////////////////

function users_setup()
{
   var s = '';
	for (var i=0; i < rx_chans; i++) {
	   s +=
	      w3_div('',
            w3_div('w3-show-inline-block w3-left w3-margin-R-5 ' +
               ((i == rx_chan)? 'w3-text-css-lime' : optbar_prefix_color), 'RX'+ i),
            w3_div('id-optbar-user-'+ i +' w3-show-inline-block')
         );
	}
	w3_innerHTML('id-optbar-users', w3_div('w3-nowrap', s));
}


////////////////////////////////
// control panel
////////////////////////////////

// icon callbacks
function freq_up_down_cb(path, up)
{
   freq_memory_up_down(+up);
	w3_field_select('id-freq-input', { focus_only:1 });
}

function toggle_or_set_spec(set, val)
{
   console.log('toggle_or_set_spec set='+ set +' val='+ val);
   
	if (isNumber(set) && (set & toggle_e.SET)) {
		spec.source_wf = val;
	} else {
		spec.source_wf ^= 1;
	}

	// close the extension first if it's using the data container and the spectrum button is pressed
	if (extint.using_data_container && spec.source_wf) {
	   //console.log('toggle_or_set_spec: extint_panel_hide()..');
		extint_panel_hide();
	}

   //console.log('toggle_or_set_spec: source_wf='+ spec.source_wf);
	html('id-button-spectrum').style.color = spec.source_wf? 'lime':'white';
	w3_show_hide('id-spectrum-container', spec.source_wf);
	w3_show_hide('id-top-container', !spec.source_wf);
   freqset_select();
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
	return (evt && (evt.shiftKey || evt.ctrlKey || evt.altKey || evt.button == mouse.middle || evt.button == mouse.right));
}

function any_alternate_click_event_except_shift(evt)
{
	return (evt && (evt.ctrlKey || evt.altKey || evt.button == mouse.middle || evt.button == mouse.right));
}

function restore_passband(mode)
{
   cfg.passbands[mode].last_lo = cfg.passbands[mode].lo;
   cfg.passbands[mode].last_hi = cfg.passbands[mode].hi;
   //writeCookie('last_locut', cfg.passbands[mode].last_lo.toString());
   //writeCookie('last_hicut', cfg.passbands[mode].last_hi.toString());
   //console.log('DEMOD PB reset');
}

var mode_buttons = [
   { s:[ 'AM', 'AMN' ],                         dis:0 },    // fixme: add AMW dynamically if 20 kHz mode? would have to deal with last_mode=ANW cookie
   { s:[ 'SAM', 'SAL', 'SAU', 'SAS', 'QAM' ],   dis:0 },
   { s:[ 'DRM' ],                               dis:0 },
   { s:[ 'LSB', 'LSN' ],                        dis:0 },
   { s:[ 'USB', 'USN' ],                        dis:0 },
   { s:[ 'CW', 'CWN' ],                         dis:0 },
   { s:[ 'NBFM' ],                              dis:0 },
   { s:[ 'IQ' ],                                dis:0 },
];

function mode_button(evt, el, dir)
{
   var mode = el.innerHTML.toLowerCase();
   if (isUndefined(dir)) dir = 1;

	// reset passband to default parameters
	if (any_alternate_click_event(evt)) {
	   if (evt.shiftKey || evt.button == mouse.middle) {
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
	if (set != undefined)
		step_9_10 = set;
	else
		step_9_10 ^= 1;
	//console.log('button_9_10 '+ step_9_10);
	writeCookie('last_9_10', step_9_10);
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
	var plist = html("id-panels-container").children;
	
	for (var i=0; i < plist.length; i++) {
		var c = plist[i];
		if (c.classList.contains('class-panel')) {
			var position = c.getAttribute('data-panel-pos');
			//console.log('place_panels: '+ c.id +' '+ position);
			if (!position) continue;
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
	}
	
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
		p.style.right = px(kiwi_scrollbar_width());		// room for waterfall scrollbar
		p.style.bottom = px(y);
		p.style.visibility = "visible";
		y += p.uiHeight+3*panel_margin;
		w3_call('panel_setup_'+ p.idName, p);
	}
}


// panel-specific setup

var divControl;

// fixed height instead of content dependent so height is constant between different optbar types
var OPTBAR_CONTENT_HEIGHT = 150;

// called indirectly by a w3_call()
function panel_setup_control(el)
{
	divControl = el;
	el.style.marginBottom = '0';

   var el2 = w3_el('id-control-inner');
   //el2.style.position = 'relative';
   //el2.style.paddingTop = px(8);
	el2.innerHTML =

	   // Workaround for Firefox bug:
	   //
	   // [Problem went away with larger panel height. Something still doesn't make sense.
	   // Relative div measuring from wrong parent in FF possibly?]
	   //
	   // Initially the upper (orange) freq history button is fine. But when you reload the page in
	   // FF the top of the icon gets chopped off. The entire innerHTML of id-control-inner seems to
	   // be shifted up a few pixels. Even the S-meter at the bottom is shifted up. No other browsers
	   // do this. The only way to fix it is to make the first table row position:relative and then
	   // shift it down via top:2px. This looks okay in the other browsers and in FF before any reloads.
	   //w3_table('id-control-freq1|position:relative; top:2px') +
	   //w3_table('id-control-freq1|position:relative;') +

      w3_div('id-control-top',
         w3_div('id-control-freq1'),
         w3_div('id-control-freq2'),
         w3_div('id-control-mode w3-margin-T-6'),
         w3_div('id-control-zoom w3-margin-T-6'),
         w3_div('id-optbar w3-margin-T-4')
      ) +
	   w3_div('id-optbar-content w3-margin-T-6 w3-scroll-y|height:'+ px(OPTBAR_CONTENT_HEIGHT),
	      w3_div('id-optbar-wf w3-hide'),
	      w3_div('id-optbar-audio w3-hide'),
	      w3_div('id-optbar-agc w3-hide'),
	      w3_div('id-optbar-users w3-hide w3-scroll-x'),
	      w3_div('id-optbar-status w3-hide')
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
// misc
////////////////////////////////

function event_dump(evt, id, oneline)
{
   if (oneline) {
      var trel = (isDefined(evt.relatedTarget) && evt.relatedTarget)? (' Trel='+ evt.relatedTarget.id) : '';
      console.log('event_dump '+ id +' '+ evt.type +' T='+ evt.target.id +' Tcur='+ evt.currentTarget.id + trel);
   } else {
      console.log('================================');
      if (!isArg(evt)) {
         console.log('EVENT_DUMP: '+ id +' bad evt');
         kiwi_trace();
         return;
      }
      console.log('EVENT_DUMP: '+ id +' type='+ evt.type);
      console.log((evt.shiftKey? 'SFT ':'') + (evt.ctrlKey? 'CTL ':'') + (evt.altKey? 'ALT ':'') + (evt.metaKey? 'META ':'') +'key='+ evt.key);
      console.log('this.id='+ this.id +' tgt.name='+ evt.target.nodeName +' tgt.id='+ evt.target.id +' ctgt.id='+ evt.currentTarget.id);
      console.log('button='+ evt.button +' buttons='+ evt.buttons +' detail='+ evt.detail +' which='+ evt.which);
      
      if (evt.type.startsWith('touch')) {
         console.log('pageX='+ evt.pageX +' clientX='+ evt.clientX +' screenX='+ evt.screenX);
         console.log('pageY='+ evt.pageY +' clientY='+ evt.clientY +' screenY='+ evt.screenY);
      } else {
         console.log('pageX='+ evt.pageX +' clientX='+ evt.clientX +' screenX='+ evt.screenX +' offX(EXP)='+ evt.offsetX +' layerX(DEPR)='+ evt.layerX);
         console.log('pageY='+ evt.pageY +' clientY='+ evt.clientY +' screenY='+ evt.screenY +' offY(EXP)='+ evt.offsetY +' layerY(DEPR)='+ evt.layerY);
      }
      
      console.log('evt, evt.target, evt.currentTarget, evt.relatedTarget:');
      console.log(evt);
      console.log(evt.target);
      console.log(evt.currentTarget);
      console.log(evt.relatedTarget);
      console.log('----');
   }
}

// NB: use kiwi_log() instead of console.log() in here
function add_problem(what, append, sticky, el_id)
{
   var id = el_id? el_id : 'id-status-problems'
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

function set_gen(freq, attn)
{
   //console.log('set_gen freq='+ freq +' attn='+ attn);
	snd_send("SET genattn="+ attn.toFixed(0));
	snd_send("SET gen="+ freq +" mix=-1");
}


////////////////////////////////
// websocket
////////////////////////////////

var bin_server = 0;
var zoom_server = 0;

// Process owrx server-to-client MSGs from SND or W/F web sockets.
// Not called if MSG handled first by kiwi.js:kiwi_msg()
function owrx_msg_cb(param, ws)
{
	switch (param[0]) {
		case "wf_setup":
			   wf_init();
			break;					
		case "extint_list_json":
			extint_list_json(param[1]);
			
			// now that we have the list of extensions see if there is an override
			if (override_ext)
				extint_open(override_ext, 3000);
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
			audio_camp(+p[0], +p[1], btn_less_buffering, kiwi_toggle(toggle_e.FROM_COOKIE | toggle_e.SET, 1, 1, 'last_compression'));
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
			toggle_or_set_spec(toggle_e.SET, 1);
			break;
		case "fft_mode":
			kiwi_fft_mode();
			break;
		case "maxdb":
		   wf.auto_maxdb = +param[1];
         setmaxdb(1, wf.auto_maxdb, true);
			break;
		case "mindb":
		   wf.auto_mindb = +param[1];
         setmindb(1, wf.auto_mindb, true);
	      w3_call('waterfall_maxmin_cb');
         update_maxmindb_sliders(true);
			break;
		case "max_thr":
			owrx.overload_mute = Math.round(+param[1]);
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
