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

// Copyright (c) 2015 - 2018 John Seamons, ZL/KF6VO

var owrx = {
   last_freq: -1,
   last_mode: '',
   last_locut: -1,
   last_hicut: -1,
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

// UI geometry
var height_top_bar_parts = 67;
var height_spectrum_canvas = 200;

var cur_mode;
var wf_fps, wf_fps_max;

var ws_snd, ws_wf;

var spectrum_show = 0, spectrum_param = -1;
var gen_freq = 0, gen_attn = 0;
var squelch_threshold = 0;
var wf_rate = '';
var wf_mm = '';
var wf_compression = 1;
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

var freq_memory = [];
var freq_memory_pointer = -1;

var wf_rates = { '0':0, 'off':0, '1':1, '1hz':1, 's':2, 'slow':2, 'm':3, 'med':3, 'f':4, 'fast':4 };

var okay_waterfall_init = false;

function kiwi_main()
{
	override_freq = parseFloat(readCookie('last_freq'));
	override_mode = readCookie('last_mode');
	override_zoom = parseFloat(readCookie('last_zoom'));
	override_9_10 = parseFloat(readCookie('last_9_10'));
	override_max_dB = parseFloat(readCookie('last_max_dB'));
	override_min_dB = parseFloat(readCookie('last_min_dB'));
	
	var f_cookie = readCookie('freq_memory');
	if (f_cookie) {
	   freq_memory = JSON.parse(f_cookie);
	   freq_memory_pointer = freq_memory.length-1;
   }
	
	console.log('LAST f='+ override_freq +' m='+ override_mode +' z='+ override_zoom
		+' 9_10='+ override_9_10 +' min='+ override_min_dB +' max='+ override_max_dB);

	// process URL query string parameters
	var num_win = 4;     // FIXME: should really be rx_chans, but that's not available yet 
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
	
	s = 'f'; if (q[s]) {
		var p = new RegExp('([0-9.,kM]*)([\/:][-0-9.,k]*)?([^z]*)?z?([0-9]*)').exec(q[s]);
      // 'k' suffix is simply ignored since default frequencies are in kHz
      if (p[1]) override_freq = p[1].replace(',', '.').parseFloatWithUnits('M', 1e-3);
      if (p[1]) console.log('p='+ p[1] +' override_freq='+ override_freq);
      if (p[2]) console.log('override_pbw/pbc='+ p[2]);
		if (p[2] && p[2].charAt(0) == '/') override_pbw = p[2].substr(1);     // remove leading '/'
		if (p[2] && p[2].charAt(0) == ':') override_pbc = p[2].substr(1);     // remove leading ':'
		if (p[3]) override_mode = p[3].toLowerCase();
		if (p[4]) override_zoom = p[4];
		//console.log('### f=['+ q[s] +'] len='+ p.length +' f=['+ p[1] +'] p=['+ p[2] +'] m=['+ p[3] +'] z=['+ p[4] +']');
	}

	s = 'ext'; if (q[s]) {
	   var ext = q[s].split(',');
		override_ext = ext[0];
		extint.param = ext.slice(1).join(',');
		console.log('URL: ext='+ override_ext +' ext_p='+ extint.param);
	}

	s = 'pbw'; if (q[s]) override_pbw = q[s];
	s = 'pb'; if (q[s]) override_pbw = q[s];
	s = 'pbc'; if (q[s]) override_pbc = q[s];
	s = 'sp'; if (q[s]) spectrum_show = q[s];
	s = 'spp'; if (q[s]) spectrum_param = parseFloat(q[s]);
	s = 'vol'; if (q[s]) { volume = parseInt(q[s]); volume = Math.max(0, volume); volume = Math.min(400, volume); }
	s = 'mute'; if (q[s]) muted_initially = parseInt(q[s]);
	s = 'wf'; if (q[s]) wf_rate = q[s];
	s = 'wfm'; if (q[s]) wf_mm = q[s];
	s = 'cmap'; if (q[s]) colormap_select = parseInt(q[s]);
	s = 'sqrt'; if (q[s]) colormap_sqrt = parseInt(q[s]);
	s = 'peak'; if (q[s]) peak_initially = parseInt(q[s]);
	s = 'no_geo'; if (q[s]) no_geoloc = true;
	s = 'keys'; if (q[s]) shortcut.keys = q[s];

   // development
	s = 'sqth'; if (q[s]) squelch_threshold = parseFloat(q[s]);
	s = 'click'; if (q[s]) nb_click = true;
	s = 'nocache'; if (q[s]) { param_nocache = true; nocache = parseInt(q[s]); }
	s = 'ncc'; if (q[s]) no_clk_corr = parseInt(q[s]);
	s = 'wfdly'; if (q[s]) waterfall_delay = parseFloat(q[s]);
	s = 'wf_comp'; if (q[s]) wf_compression = parseInt(q[s]);
	s = 'gen'; if (q[s]) gen_freq = parseFloat(q[s]);
	s = 'attn'; if (q[s]) gen_attn = parseInt(q[s]);
	s = 'blen'; if (q[s]) audio_buffer_min_length_sec = parseFloat(q[s])/1000;
	s = 'audio'; if (q[s]) audio_meas_dly_ena = parseFloat(q[s]);
	s = 'gc'; if (q[s]) kiwi_gc = parseInt(q[s]);
	s = 'gc_snd'; if (q[s]) kiwi_gc_snd = parseInt(q[s]);
	s = 'gc_wf'; if (q[s]) kiwi_gc_wf = parseInt(q[s]);
	s = 'gc_recv'; if (q[s]) kiwi_gc_recv = parseInt(q[s]);
	s = 'gc_wspr'; if (q[s]) kiwi_gc_wspr = parseInt(q[s]);
	s = 'ctrace'; if (q[s]) { param_ctrace = true; ctrace = parseInt(q[s]); }
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

	init_rx_photo();
	right_click_menu_init();
	keyboard_shortcut_init();
	confirmation_panel_init();
	ext_panel_init();
	place_panels();
	init_panels();
   okay_waterfall_init = true;
	confirmation_panel_init2();
	smeter_init();
	time_display_setup('id-topbar-right-container');
	
	window.setTimeout(function() {window.setInterval(send_keepalive, 5000);}, 5000);
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
	snd_send("SET debug_v="+ debug_v);
	snd_send("SET squelch=0 max="+ squelch_threshold.toFixed(0));
	snd_send("SET lms_denoise=0");
	snd_send("SET lms_autonotch=0");

   if (gen_attn != 0) {
      var dB = gen_attn;
      var ampl_gain = Math.pow(10, -dB/20);		// use the amplitude form since we are multipling a signal
      gen_attn = 0x01ffff * ampl_gain;
      console.log('### GEN dB='+ dB +' ampl_gain='+ ampl_gain +' attn='+ gen_attn +' / '+ gen_attn.toHex());
   }
	set_gen(gen_freq, gen_attn);

	snd_send("SET mod=am low_cut=-4000 high_cut=4000 freq=1000");
	set_agc();
	snd_send("SET browser="+navigator.userAgent);
	
	wf_send("SERVER DE CLIENT openwebrx.js W/F");
	wf_send("SET send_dB=1");
	// fixme: okay to remove this now?
	wf_send("SET zoom=0 start=0");
	wf_send("SET maxdb=0 mindb=-100");
	if (wf_compression == 0) wf_send('SET wf_comp=0');
	wf_speed = wf_rates[wf_rate];
	//console.log('wf_rate="'+ wf_rate +'" wf_speed='+ wf_speed);
	if (wf_speed == undefined) wf_speed = WF_SPEED_FAST;
	wf_send('SET wf_speed='+ wf_speed);

	if (nb_click) nb_send(-1, noiseThresh);
}

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
			'<a id="'+panel+'-close" onclick="toggle_panel('+ sq(panel) +');"><img src="icons/close.24.png" width="24" height="24" /></a>';
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
		if (timeo) {
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
}

function toggle_panel(panel)
{
	var divPanel = w3_el(panel);
	var divVis = w3_el(panel +'-vis');
	//console.log('toggle_panel '+ panel +' ptype='+ divPanel.ptype +' panelShown='+ divPanel.panelShown);

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
   a = (typeof a == 'string' && a.startsWith('orient'))? a : 'event';
	resize_canvases();
	resize_waterfall_container(true);
   extint_environment_changed( { resize:1 } );
	resize_scale(a);
	check_top_bar_congestion();
}

function orientation_change() {
   //openwebrx_resize('orient '+ orientation);
}

try {
   window.onorientationchange = orientation_change;
} catch(ex) {}

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

var rx_photo_spacer_height = height_top_bar_parts;

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


////////////////////////////////
// =================  >ANIMATION ROUTINES  ================
////////////////////////////////

function animate(object, style_name, unit, from, to, accel, time_ms, fps, to_exec)
{
	//console.log(object.className);
	if(typeof to_exec=="undefined") to_exec=0;
	object.style[style_name]=from.toString()+unit;
	object.anim_i=0;
	n_of_iters=time_ms/(1000/fps);
	if (n_of_iters < 1) n_of_iters = 1;
	change=(to-from)/(n_of_iters);
	if(typeof object.anim_timer!="undefined") { window.clearInterval(object.anim_timer);  }

	object.anim_timer = window.setInterval(
		function(){
			if(object.anim_i++<n_of_iters)
			{
				if (accel==1) object.style[style_name] = (parseFloat(object.style[style_name]) + change).toString() + unit;
				else 
				{ 
					remain=parseFloat(object.style[style_name])-to;
					if(Math.abs(remain)>9||unit!="px") new_val=(to+accel*remain);
					else {if(Math.abs(remain)<2) new_val=to;
					else new_val=to+remain-(remain/Math.abs(remain));}
					object.style[style_name]=new_val.toString()+unit;
				}
			}
			else 
				{object.style[style_name]=to.toString()+unit; window.clearInterval(object.anim_timer); delete object.anim_timer;}
			if(to_exec!=0) to_exec();
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
			if(something.fade_i++<n_of_iters)
				something.style.opacity=parseFloat(something.style.opacity)+change;
			else 
				{something.style.opacity=to; window.clearInterval(something.fade_timer); }
		},1000/fps);
}*/


////////////////////////////////
// ================  >DEMODULATOR ROUTINES  ===============
////////////////////////////////

demodulators=[]

demodulator_color_index=0;
demodulator_colors=["#ffff00", "#00ff00", "#00ffff", "#058cff", "#ff9600", "#a1ff39", "#ff4e39", "#ff5dbd"]

function demodulators_get_next_color()
{
	if(demodulator_color_index>=demodulator_colors.length) demodulator_color_index=0;
	return(demodulator_colors[demodulator_color_index++]);
}

function demod_envelope_draw(range, from, to, color, line)
{  //                                               ____
	// Draws a standard filter envelope like this: _/    \_
   // Parameters are given in offset frequency (Hz).
   // Envelope is drawn on the scale canvas.
	// A "drag range" object is returned, containing information about the draggable areas of the envelope
	// (beginning, ending and the line showing the offset frequency).
	if(typeof color == "undefined") color="#ffff00"; //yellow
	
	env_bounding_line_w=5;   //    
	env_att_w=5;             //     _______   ___env_h2 in px   ___|_____
	env_h1=17;               //   _/|      \_ ___env_h1 in px _/   |_    \_
	env_h2=5;                //   |||env_att_w                     |_env_lineplus
	env_lineplus=1;          //   ||env_bounding_line_w
	env_line_click_area=8;
	env_slop=5;
	
	//range=get_visible_freq_range();
	from_px = scale_px_from_freq(from,range);
	to_px = scale_px_from_freq(to,range);
	if (to_px < from_px) /* swap'em */ { temp_px=to_px; to_px=from_px; from_px=temp_px; }
	
	pb_adj_cf.style.left = (from_px) +'px';
	pb_adj_cf.style.width = (to_px-from_px) +'px';

	pb_adj_lo.style.left = (from_px-env_bounding_line_w-2*env_slop) +'px';
	pb_adj_lo.style.width = (env_bounding_line_w+env_att_w+2*env_slop) +'px';

	pb_adj_hi.style.left = (to_px-env_bounding_line_w) +'px';
	pb_adj_hi.style.width = (env_bounding_line_w+env_att_w+2*env_slop) +'px';
	
	/*from_px-=env_bounding_line_w/2;
	to_px += env_bounding_line_w/2;*/
	from_px -= (env_att_w+env_bounding_line_w);
	to_px += (env_att_w+env_bounding_line_w);
	
	// do drawing:
	scale_ctx.lineWidth = 3;
	scale_ctx.strokeStyle = color;
	scale_ctx.fillStyle = color;
	var drag_ranges = { envelope_on_screen: false, line_on_screen: false };
	
	if (!(to_px<0 || from_px>window.innerWidth)) // out of screen?
	{
		drag_ranges.beginning={x1:from_px, x2: from_px+env_bounding_line_w+env_att_w+env_slop};
		drag_ranges.ending={x1:to_px-env_bounding_line_w-env_att_w-env_slop, x2: to_px};
		drag_ranges.whole_envelope={x1:from_px, x2: to_px};
		drag_ranges.envelope_on_screen=true;
		
		scale_ctx.beginPath();
		scale_ctx.moveTo(from_px,env_h1);
		scale_ctx.lineTo(from_px+env_bounding_line_w, env_h1);
		scale_ctx.lineTo(from_px+env_bounding_line_w+env_att_w, env_h2);
		scale_ctx.lineTo(to_px-env_bounding_line_w-env_att_w, env_h2);
		scale_ctx.lineTo(to_px-env_bounding_line_w, env_h1);
		scale_ctx.lineTo(to_px, env_h1);
		scale_ctx.globalAlpha = 0.3;
		scale_ctx.fill();
		scale_ctx.globalAlpha = 1;
		scale_ctx.stroke();
	}
	
	if (typeof line != "undefined") // out of screen? 
	{
		line_px = scale_px_from_freq(line,range);
		if (!(line_px<0 || line_px>window.innerWidth))
		{
			drag_ranges.line={x1:line_px-env_line_click_area/2, x2: line_px+env_line_click_area/2};
			drag_ranges.line_on_screen=true;
			scale_ctx.moveTo(line_px,env_h1+env_lineplus);
			scale_ctx.lineTo(line_px,env_h2-env_lineplus);
			scale_ctx.stroke();

			pb_adj_car.style.left = drag_ranges.line.x1 +'px';
			pb_adj_car.style.width = env_line_click_area +'px';

		}
	}
	return drag_ranges;
}

function demod_envelope_where_clicked(x, drag_ranges, key_modifiers)
{
	// Check exactly what the user has clicked based on ranges returned by demod_envelope_draw().
	in_range = function(x, g_range) { return g_range.x1 <= x && g_range.x2 >= x; }
	dr = demodulator.draggable_ranges;
	//console.log('demod_envelope_where_clicked x='+ x);
	//console.log(drag_ranges);
	//console.log(key_modifiers);

	if(key_modifiers.shiftKey)
	{
		//Check first: shift + center drag emulates BFO knob
		if(drag_ranges.line_on_screen && in_range(x,drag_ranges.line)) return dr.bfo;
		
		//Check second: shift + envelope drag emulates PBS knob
		if(drag_ranges.envelope_on_screen && in_range(x,drag_ranges.whole_envelope)) return dr.pbs;
	}
	
	if(key_modifiers.altKey)
	{
		if(drag_ranges.envelope_on_screen && in_range(x,drag_ranges.beginning)) return dr.bwlo;
		if(drag_ranges.envelope_on_screen && in_range(x,drag_ranges.ending)) return dr.bwhi;
	}
	
	if(drag_ranges.envelope_on_screen)
	{ 
		// For low and high cut:
		if(in_range(x,drag_ranges.beginning)) return dr.beginning;
		if(in_range(x,drag_ranges.ending)) return dr.ending;
		// Last priority: having clicked anything else on the envelope, without holding the shift key
		if(in_range(x,drag_ranges.whole_envelope)) return dr.anything_else; 
	}
	return dr.none; //User doesn't drag the envelope for this demodulator
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

//ranges on filter envelope that can be dragged:
demodulator.draggable_ranges = {
	none: 0,
	beginning: 1,	// from
	ending: 2,		// to
	anything_else: 3,
	bfo: 4,			// line (while holding shift)
	pbs: 5,			// passband (while holding shift)
	bwlo: 6,			// bandwidth (while holding alt in envelope beginning)
	bwhi: 7,			// bandwidth (while holding alt in envelope ending)
} //to which parameter these correspond in demod_envelope_draw()

//******* class demodulator_default_analog *******
// This can be used as a base for basic audio demodulators.
// It already supports most basic modulations used for ham radio and commercial services: AM/FM/LSB/USB

demodulator_response_time=100; 
//in ms; if we don't limit the number of SETs sent to the server, audio will underrun (possibly output buffer is cleared on SETs in GNU Radio

var passbands = {
	am:		{ lo: -4900,	hi:  4900 },   // 9.8 kHz instead of 10 to avoid adjacent channel heterodynes in SW BCBs
	amn:		{ lo: -2500,	hi:  2500 },
	lsb:		{ lo: -2700,	hi:  -300 },	// cf = 1500 Hz, bw = 2400 Hz
	usb:		{ lo:   300,	hi:  2700 },	// cf = 1500 Hz, bw = 2400 Hz
	cw:		{ lo:   300,	hi:   700 },	// cf = 500 Hz, bw = 400 Hz
	cwn:		{ lo:   470,	hi:   530 },	// cf = 500 Hz, bw = 60 Hz
	nbfm:		{ lo: -6000,	hi:  6000 },	// FIXME: set based on current srate?
	iq:		{ lo: -5000,	hi:  5000 },
	s4285:	{ lo:   600,	hi:  3000 },	// cf = 1800 Hz, bw = 2400 Hz, s4285 compatible
//	s4285:	{ lo:   400,	hi:  3200 },	// cf = 1800 Hz, bw = 2800 Hz, made things a little worse?
};

function demodulator_default_analog(offset_frequency, subtype, locut, hicut)
{
   if (passbands[subtype] == null) subtype = 'am';
   
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

//jks
//if (!isNaN(locut)) { console.log('#### demodulator_default_analog locut='+ locut); kiwi_trace(); }
//if (!isNaN(hicut)) { console.log('#### demodulator_default_analog hicut='+ hicut); kiwi_trace(); }
	var lo = isNaN(locut)? passbands[subtype].last_lo : locut;
	var hi = isNaN(hicut)? passbands[subtype].last_hi : hicut;
	if (lo == 'undefined' || lo == null) {
		lo = passbands[subtype].last_lo = passbands[subtype].lo;
	}
	if (hi == 'undefined' || hi == null) {
		hi = passbands[subtype].last_hi = passbands[subtype].hi;
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
	}
	
	this.low_cut = Math.max(lo, this.filter.low_cut_limit);
	this.high_cut = Math.min(hi, this.filter.high_cut_limit);
	//console.log('DEMOD set lo='+ this.low_cut, ' hi='+ this.high_cut);
	
	this.usePBCenter = false;
	this.isCW = false;

	if(subtype=="am")
	{
	}	
	else if(subtype=="amn")
	{
	}
	else if(subtype=="lsb")
	{
		this.usePBCenter=true;
	}
	else if(subtype=="usb")
	{
		this.usePBCenter=true;
	}
	else if(subtype=="cw")
	{
		this.usePBCenter=true;
		this.isCW=true;
	} 
	else if(subtype=="cwn")
	{
		this.usePBCenter=true;
		this.isCW=true;
	} 
	else if(subtype=="nbfm")
	{
	}
	else if(subtype=="iq")
	{
	}
	else if(subtype=="s4285")
	{
		// FIXME: hack for custom s4285 passband
		this.usePBCenter=true;
		this.server_mode='usb';
	}
	else console.log("DEMOD-new: unknown subtype="+subtype);

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
		var hicut = this.high_cut.toString()
		snd_send('SET mod='+ mode +' low_cut='+ locut +' high_cut='+ hicut +' freq='+ freq);

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
      if (changed != null) extint_environment_changed(changed);

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
		pb_adj_car_ttip.innerHTML = ((center_freq + this.parent.offset_frequency)/1000 + cfg.freq_offset).toFixed(2) +' kHz';
	};

	// event handlers
	this.envelope.drag_start = function(x, key_modifiers)
	{
		this.key_modifiers = key_modifiers;
		this.dragged_range = demod_envelope_where_clicked(x, this.drag_ranges, key_modifiers);
		//console.log("DRAG-START dr="+ this.dragged_range.toString());
		this.drag_origin={
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
			passbands[this.parent.server_mode].last_lo = this.parent.low_cut = new_lo;
			//console.log('DRAG-MOVE lo=', new_lo.toFixed(0));
		}
		
		if (do_hi) {
			passbands[this.parent.server_mode].last_hi = this.parent.high_cut = new_hi;
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
   if (passbands[subtype] == null) subtype = 'am';

	var offset = 0, prev_pbo = 0, low_cut = NaN, high_cut = NaN;
	var wasCW = false, toCW = false, fromCW = false;
	
	if (demodulators.length) {
		wasCW = demodulators[0].isCW;
		offset = demodulators[0].offset_frequency;
		prev_pbo = passband_offset();
		demodulator_remove(0);
	} else {
		//console.log("### init_freq="+init_frequency+" init_mode="+init_mode);
		offset = (init_frequency*1000).toFixed(0) - center_freq;
		subtype = init_mode;
	}
	
	// initial offset, but doesn't consider demod.isCW since it isn't valid yet
	if (freq != undefined) {
		offset = freq - center_freq;
	}
	
	//console.log("DEMOD-replace calling add: INITIAL offset="+(offset+center_freq));
	demodulator_add(new demodulator_default_analog(offset, subtype, low_cut, high_cut));
	
	if (!wasCW && demodulators[0].isCW)
		toCW = true;
	if (wasCW && !demodulators[0].isCW)
		fromCW = true;
	
	// determine actual offset now that demod.isCW is valid
	if (freq != undefined) {
		freq_car_Hz = freq_dsp_to_car(freq);
		//console.log('DEMOD-replace SPECIFIED freq='+ freq +' car='+ freq_car_Hz);
		offset = freq_car_Hz - center_freq;
		audioFFT_clear_wf = true;
	} else {
		// Freq not changing, just mode. Do correct thing for switch to/from cw modes: keep display freq constant
		var pbo = 0;
		if (toCW) pbo = -passband_offset();
		if (fromCW) pbo = prev_pbo;	// passband offset calculated _before_ demod was changed
		offset += pbo;
		
		// clear switching to/from cw mode because of frequency offset
		if (toCW || fromCW)
		   audioFFT_clear_wf = true;
		//console.log('DEMOD-replace SAME freq='+ (offset + center_freq) +' PBO='+ pbo +' prev='+ prev_pbo +' toCW='+ toCW +' fromCW='+ fromCW);
	}

	cur_mode = subtype;
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


////////////////////////////////
// scale
////////////////////////////////

var scale_ctx, band_ctx, dx_ctx;
var pb_adj_cf, pb_adj_cf_ttip, pb_adj_lo, pb_adj_lo_ttip, pb_adj_hi, pb_adj_hi_ttip, pb_adj_car, pb_adj_car_ttip;
var scale_canvas, band_canvas, dx_div, dx_canvas;

function scale_setup()
{
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
	//console.log("SC-MDN");
	with (scale_canvas_drag_params) {
		drag = false;
		start_x = evt.pageX;
		start_y = evt.pageY;
		key_modifiers.shiftKey = evt.shiftKey;
		key_modifiers.altKey = evt.altKey;
		key_modifiers.ctrlKey = evt.ctrlKey;
	}
	scale_canvas_start_drag(evt);
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
	   scale_canvas_start_drag(evt);
	}
	evt.preventDefault();
}

var scale_canvas_ignore_mouse_event = false;

function scale_canvas_start_drag(evt)
{
	// Distinguish ctrl-click right-button meta event from actual right-button on mouse (or touchpad two-finger tap).
	// Must ignore true_right_click case even though contextmenu event is being handled elsewhere.
	var true_right_click = false;
	if (evt.button == mouse.right && !evt.ctrlKey) {
		//dump_event = true;
		true_right_click = true;
		right_click_menu(scale_canvas_drag_params.start_x, scale_canvas_drag_params.start_y);
      scale_canvas_drag_params.mouse_down = false;
		scale_canvas_ignore_mouse_event = true;
		return;
	}

   scale_canvas_drag_params.mouse_down = true;
}

function scale_offset_carfreq_from_px(x, visible_range)
{
	if (typeof visible_range === "undefined") visible_range = get_visible_freq_range();
	var offset = passband_offset();
	var f = visible_range.start + visible_range.bw*(x/canvas_container.clientWidth);
	//console.log("SOCFFPX f="+f+" off="+offset+" f-o="+(f-offset)+" rtn="+(f - center_freq - offset));
	return f - center_freq - offset;
}

function scale_canvas_drag(evt, x)
{
   if (scale_canvas_ignore_mouse_event) return;
   
	var event_handled = 0;
	var relX = Math.abs(x - scale_canvas_drag_params.start_x);

	if (scale_canvas_drag_params.mouse_down && !scale_canvas_drag_params.drag && relX > canvas_drag_min_delta) {
		//we can use the main drag_min_delta thing of the main canvas
		scale_canvas_drag_params.drag = true;
		//call the drag_start for all demodulators (and they will decide if they're dragged, based on X coordinate)
		for (var i=0; i<demodulators.length; i++)
			event_handled |= demodulators[i].envelope.drag_start(x, scale_canvas_drag_params.key_modifiers);
		//console.log("MOV1 evh? "+event_handled);
		evt.target.style.cursor = "move";
		//evt.target.style.cursor = "ew-resize";
		//console.log('sc cursor');
	} else

	if (scale_canvas_drag_params.drag) {
		//call the drag_move for all demodulators (and they will decide if they're dragged)
		for (var i=0; i<demodulators.length; i++)
			event_handled |= demodulators[i].envelope.drag_move(x);
		//console.log("MOV2 evh? "+event_handled);
		if (!event_handled)
			demodulator_set_offset_frequency(0, scale_offset_carfreq_from_px(x));
		scale_canvas_drag_params.last_x = x;
	}
}

function scale_canvas_mousemove(evt)
{
	scale_canvas_drag(evt, evt.pageX);
}

function scale_canvas_touchMove(evt)
{
	for (var i=0; i < evt.touches.length; i++) {
		scale_canvas_drag(evt, evt.touches[i].pageX);
	}
	evt.preventDefault();
}

function scale_canvas_end_drag(evt, x)
{
   if (scale_canvas_ignore_mouse_event) {
      scale_canvas_ignore_mouse_event = false;
      return;
   }

	scale_canvas_drag_params.drag = false;
	scale_canvas_drag_params.mouse_down = false;
	var event_handled = false;
	
	for (var i=0;i<demodulators.length;i++) event_handled |= demodulators[i].envelope.drag_end(x);
	//console.log("MED evh? "+event_handled);
	if (!event_handled) demodulator_set_offset_frequency(0, scale_offset_carfreq_from_px(x));

	evt.target.style.cursor = null;		// re-enable default mouseover cursor in .css (if any)
}

function scale_canvas_mouseup(evt)
{
	//console.log("SC-MUP");
	//console.log('sc default');
	scale_canvas_end_drag(evt, evt.pageX);
}

function scale_canvas_touchEnd(evt)
{
	scale_canvas_end_drag(evt, scale_canvas_drag_params.last_x);
	evt.preventDefault();
}

function scale_px_from_freq(f,range) { return Math.round(((f-range.start)/range.bw)*canvas_container.clientWidth); }

function get_visible_freq_range()
{
	out={};
	
	if (audioFFT_active && cur_mode != undefined) {
	   var off, span;
      var srate = Math.round(audio_input_rate || 12000);
	   if (cur_mode == 'iq') {
	      off = 0;
	      span = srate;
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
	   //console.log('GVFR'+ (audioFFT_active? '(audioFFT)':'') +" mode="+cur_mode+" xb="+x_bin+" s="+out.start+" c="+out.center+" e="+out.end+" bw="+out.bw+" hpp="+out.hpp+" cw="+scale_canvas.clientWidth);
	} else {
	   var bins = bins_at_cur_zoom();
      out.start = bin_to_freq(x_bin);
      out.center = bin_to_freq(x_bin + bins/2);
      out.end = bin_to_freq(x_bin + bins);
      out.bw = out.end-out.start;
      out.hpp = out.bw / scale_canvas.clientWidth;
	   //console.log("GVFR z="+zoom_level+" xb="+x_bin+" BACZ="+bins+" s="+out.start+" c="+out.center+" e="+out.end+" bw="+out.bw+" hpp="+out.hpp+" cw="+scale_canvas.clientWidth);
	}
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

/*
var scale_markers_levels = [
	{
		"hz_per_large_marker":10000000, //large
		"estimated_text_width":70,
		"format":"{x}a ",
		"pre_divide":1000000,
		"decimals":0
	},
	{
		"hz_per_large_marker":5000000,
		"estimated_text_width":70,
		"format":"{x}b ",
		"pre_divide":1000000,
		"decimals":0
	},
	{
		"hz_per_large_marker":1000000,
		"estimated_text_width":70,
		"format":"{x}c ",
		"pre_divide":1000000,
		"decimals":0
	},
	{
		"hz_per_large_marker":500000,
		"estimated_text_width":70,
		"format":"{x}d ",
		"pre_divide":1000000,
		"decimals":1
	},
	{
		"hz_per_large_marker":100000,
		"estimated_text_width":70,
		"format":"{x}e ",
		"pre_divide":1000000,
		"decimals":1
	},
	{
		"hz_per_large_marker":50000,
		"estimated_text_width":70,
		"format":"{x}f ",
		"pre_divide":1000000,
		"decimals":2
	},
	{
		"hz_per_large_marker":10000,
		"estimated_text_width":70,
		"format":"{x}g ",
		"pre_divide":1000000,
		"decimals":2
	},
	{
		"hz_per_large_marker":5000,
		"estimated_text_width":70,
		"format":"{x}h ",
		"pre_divide":1000000,
		"decimals":3
	},
	{
		"hz_per_large_marker":1000,
		"estimated_text_width":70,
		"format":"{x}i ",
		"pre_divide":1000000,
		"decimals":3
	}
];
*/

var scale_min_space_btwn_texts = 50;
var scale_min_space_btwn_small_markers = 7;

function get_scale_mark_spacing(range)
{
	out = {};
	fcalc = function(mkr_spacing) { 
		out.numlarge = (range.bw/mkr_spacing);
		out.pxlarge = canvas_container.clientWidth/out.numlarge; 	//distance between large markers (these have text)
		out.ratio = 5; 															//(ratio-1) small markers exist per large marker
		out.pxsmall = out.pxlarge/out.ratio; 								//distance between small markers
		if (out.pxsmall < scale_min_space_btwn_small_markers) return false; 
		if (out.pxsmall/2 >= scale_min_space_btwn_small_markers && mkr_spacing.toString()[0] != "5") { out.pxsmall/=2; out.ratio*=2; }
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
	//console.log(canvas_container);
	//console.log(range);
	//console.log(mp);
	//console.log(out);
	out.params = mp;
	return out;
}

var g_range;

function mk_freq_scale()
{
	//clear the lower part of the canvas (where frequency scale resides; the upper part is used by filter envelopes):
	g_range = get_visible_freq_range();
	mkenvelopes(g_range); //when scale changes we will always have to redraw filter envelopes, too

	scale_ctx.clearRect(0,22,scale_ctx.canvas.width,scale_ctx.canvas.height-22);
	scale_ctx.strokeStyle = "#fff";
	scale_ctx.font = "bold 12px sans-serif";
	scale_ctx.textBaseline = "top";
	scale_ctx.fillStyle = "#fff";
	
	var spacing = get_scale_mark_spacing(g_range);
	//console.log(spacing);
	marker_hz = Math.ceil(g_range.start/spacing.smallbw) * spacing.smallbw;
	text_y_pos = 22+10 + (kiwi_isFirefox()? 3:0);
	var text_to_draw;
	
	var ftext = function(f) {
		var pre_divide = spacing.params.pre_divide;
		var decimals = spacing.params.decimals;
		f += cfg.freq_offset*1e3;
		if (f < 1e6) {
			pre_divide /= 1000;
			decimals = 0;
		}
		text_to_draw = format_frequency(spacing.params.format+((f < 1e6)? 'kHz':'MHz'), f, pre_divide, decimals);
	}
	
	var last_large;
   var conv_ct=0;

	for (;;) {
      conv_ct++;
      if (conv_ct > 1000) break;
		var x = scale_px_from_freq(marker_hz,g_range);
		if (x > window.innerWidth) break;
		scale_ctx.beginPath();		
		scale_ctx.moveTo(x, 22);

		if (marker_hz % spacing.params.hz_per_large_marker == 0) {

			//large marker
			if (typeof first_large == "undefined") var first_large = marker_hz; 
			last_large = marker_hz;
			scale_ctx.lineWidth = 3.5;
			scale_ctx.lineTo(x,22+11);
			ftext(marker_hz);
			var text_measured = scale_ctx.measureText(text_to_draw);
			scale_ctx.textAlign = "center";

			//advanced text drawing begins
			//console.log('text_to_draw='+ text_to_draw);
			if (zoom_level==0 && g_range.start+spacing.smallbw*spacing.ratio > marker_hz) {

				//if this is the first overall marker when zoomed all the way out
				//console.log('case 1');
				if (x < text_measured.width/2) {
				   //and if it would be clipped off the screen
					if (scale_px_from_freq(marker_hz+spacing.smallbw*spacing.ratio,g_range)-text_measured.width >= scale_min_space_btwn_texts) {
					   //and if we have enough space to draw it correctly without clipping
						scale_ctx.textAlign = "left";
						scale_ctx.fillText(text_to_draw, 0, text_y_pos); 
					}
				}
			} else
			
			if (zoom_level==0 && g_range.end-spacing.smallbw*spacing.ratio < marker_hz) {

			   //if this is the last overall marker when zoomed all the way out
				//console.log('case 2');
				if (x > window.innerWidth-text_measured.width/2) {
				   //and if it would be clipped off the screen
					if (window.innerWidth-text_measured.width-scale_px_from_freq(marker_hz-spacing.smallbw*spacing.ratio,g_range) >= scale_min_space_btwn_texts) {
					   //and if we have enough space to draw it correctly without clipping
						scale_ctx.textAlign = "right";
						scale_ctx.fillText(text_to_draw, window.innerWidth, text_y_pos); 
					}	
				} else {
					// last large marker is not the last marker, so draw normally
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
		scale_ctx.stroke();
	}

   if (conv_ct > 1000) { console.log("CONV_CT > 1000!!!"); kiwi_trace(); }

	if (zoom_level != 0) {	// if zoomed, we don't want the texts to disappear because their markers can't be seen
		// on the left side
		scale_ctx.textAlign = "center";
		var f = first_large-spacing.smallbw*spacing.ratio;
		var x = scale_px_from_freq(f,g_range);
		ftext(f);
		var w = scale_ctx.measureText(text_to_draw).width;
		if (x+w/2 > 0) scale_ctx.fillText(text_to_draw, x, 22+10);

		// on the right side
		f = last_large+spacing.smallbw*spacing.ratio;
		x = scale_px_from_freq(f,g_range);
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

	dx_div.style.width = window.innerWidth+'px';

	dx_ctx.canvas.width = dx_car_w;
	dx_ctx.canvas.height = dx_car_h;
	dx_canvas.style.top = (scale_canvas_top - dx_car_h)+'px';
	dx_canvas.style.left = 0;
	dx_canvas.style.zIndex = 99;
	
	// the dx canvas is used to form the "carrier" marker symbol (yellow triangle) seen when
	// an NDB dx label is entered by the mouse
	dx_ctx.beginPath();
	dx_ctx.lineWidth = dx_car_border-1;
	dx_ctx.strokeStyle='black';
	var o = dx_car_border;
	var x = dx_car_size;
	dx_ctx.moveTo(o,o);
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
	//mk_spurs();
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
	if (typeof demodulators[0] == "undefined") return x_bin;	// punt if no demod yet
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
	if(!waterfall_setup_done) return;
	//html("id-freq-show").style.visibility="visible";	
}

function canvas_mouseout(evt)
{
	if(!waterfall_setup_done) return;
	//html("id-freq-show").style.visibility="hidden";
}

function canvas_get_carfreq_offset(relativeX, incl_PBO)
{
   var freq;
   var norm = relativeX/waterfall_width;
   if (audioFFT_active) {
      var cur = center_freq + demodulators[0].offset_frequency;
      norm -= (cur_mode == 'iq')? 0.5 : 0.25;
      var incr = norm * audio_input_rate * ((cur_mode == 'iq')? 2 : 1);
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


////////////////////////////////
// right click menu
////////////////////////////////

function right_click_menu_init()
{
   w3_menu('id-right-click-menu', 'right_click_menu_cb');
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

   w3_menu_items('id-right-click-menu',
      db +' database lookup',
      'Utility database lookup',
      'DX Cluster lookup',
      '<hr>',
      'restore passband',
      'save waterfall as JPG',
      'DX label filter',
      '<hr>',
      '<i>cal ADC clock (admin)</i>'
   );

   w3_menu_popup('id-right-click-menu', x, y);
}

function right_click_menu_cb(idx, x)
{
   //console.log('right_click_menu_cb idx='+ idx +' x='+ x +' f='+ canvas_get_dspfreq(x)/1e3);
   
   switch (idx) {
   
   case 0:  // database lookups
   case 1:
   case 2:
		freq_database_lookup(canvas_get_dspfreq(x), idx);
      break;
   
   case 3:  // restore passband
      restore_passband(cur_mode);
      demodulator_analog_replace(cur_mode);
      break;
      
   case 4:  // save waterfall image
      export_waterfall(canvas_get_dspfreq(x));
      break;
   
   case 5:
      dx_filter();
      break;
   
   case 6:  // cal ADC clock
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
   
   case -1:
   default:
      break;
   }
}

function freq_database_lookup(Hz, utility)
{
   var kHz = Hz/1000;
   var kHz_r10 = Math.round(Hz/10)/100;
   var kHz_r1k = Math.round(Hz/1000);
   //console.log('### Hz='+ Hz +' kHz='+ kHz +' kHz_r10='+ kHz_r10 +' kHz_r1k='+ kHz_r1k);
   var f;
   var url = "http://";

   var b = band_info();

   
   if (utility == 1) {
      f = Math.floor(Hz/100) / 10000;	// round down to nearest 100 Hz, and express in MHz, for GlobalTuners
      url += "qrg.globaltuners.com/?q="+f.toFixed(4);
   } 
   if (utility == 0) {
      if (kHz >= b.NDB_lo && kHz < b.NDB_hi) {
         f = kHz_r1k.toFixed(0);		// 1kHz windows on 1 kHz boundaries for NDBs
         url += "www.classaxe.com/dx/ndb/rww/signal_list/?mode=signal_list&submode=&targetID=&sort_by=khz&limit=-1&offset=0&show=list&"+
         "type_DGPS=1&type_NAVTEX=1&type_NDB=1&filter_id=&filter_khz_1="+ f +"&filter_khz_2="+ f +
         "&filter_channels=&filter_sp=&filter_sp_itu_clause=AND&filter_itu=&filter_continent=&filter_dx_gsq=&region=&"+
         "filter_listener=&filter_heard_in=&filter_date_1=&filter_date_2=&offsets=&sort_by_column=khz";
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
         url += "www.short-wave.info/index.php?freq="+f.toFixed(0)+"&timbus=NOW&ip="+client_public_ip+"&porm=4";
      }
   }
   if (utility == 2) {
      f = Math.floor(Hz) / 1000;	// KHz for ve3sun dx cluster lookup
      url += 've3sun.com/KiwiSDR/DX.php?Search='+f.toFixed(1);
   }
   
   console.log('LOOKUP '+ kHz +' -> '+ f +' '+ url);
   var win = window.open(url, '_blank');
   if (win) win.focus();
}

function export_waterfall(Hz) {

    f = get_visible_freq_range()
    var fileName = Math.floor(f.center/100)/10 +'+-'+Math.floor((f.end-f.center)/100)/10 + 'KHz.jpg'

    var PNGcanvas = document.createElement("canvas");
    PNGcanvas.width = wf_fft_size;
    PNGcanvas.height = (old_canvases.length+wf_canvases.length) * wf_canvas_default_height;
    PNGcanvas.ctx = PNGcanvas.getContext("2d");
    PNGcanvas.ctx.fillStyle="black";
    PNGcanvas.ctx.fillRect(0, 0, PNGcanvas.width, PNGcanvas.height);

    PNGcanvas.ctx.strokeStyle="red";
    
    var h = 0;
    wf_canvases.forEach(function(wf_c) {
        
        PNGcanvas.ctx.drawImage(wf_c,0,h);
        h += wf_c.height;
        });
    if (old_canvases.length > 1)
        {
        for (i=1; i < old_canvases.length; i++)
           {
           PNGcanvas.ctx.drawImage(old_canvases[i],0,h);
           h += old_canvases[i].height;
           }
        }
        // old_canvases.forEach(function(wf_c) {
        //PNGcanvas.ctx.drawImage(wf_c,0,h);
        //h += wf_c.height;
        // });
    var arrow;
    if (!Hz) Hz = f.center;
    arrow = wf_fft_size*(Hz-f.start)/(f.end-f.start);

    PNGcanvas.ctx.moveTo(arrow, 0); 
    PNGcanvas.ctx.lineTo(arrow, 50); 
    
//    var arrowMinus = wf_fft_size*(1000*(Math.floor(Hz/1000))-f.start)/(f.end-f.start);
//    var arrowPlus  = wf_fft_size*(1000*(Math.floor((1000+Hz)/1000))-f.start)/(f.end-f.start);
//    PNGcanvas.ctx.moveTo(arrowMinus, 50);
//    PNGcanvas.ctx.lineTo(arrowMinus, 40);
//    PNGcanvas.ctx.lineTo(arrowPlus, 40);
//    PNGcanvas.ctx.lineTo(arrowPlus, 50);
            
//    for(h=1200; h < PNGcanvas.height; h += 1200)
//       {
//       PNGcanvas.ctx.moveTo(0,h);
//       PNGcanvas.ctx.lineTo(PNGcanvas.width,h);
//       } 

    PNGcanvas.ctx.stroke(); 
    
    var flabel = Math.floor(Hz/100)/10;
    flabel = flabel + ' KHz ';
    PNGcanvas.ctx.font = "18px Arial";
    PNGcanvas.ctx.fillStyle = "lime";
    flabel += window.location.href.substring(7); 
    flabel = flabel.substring(0,flabel.indexOf(':'));

//    if (!Hz && document.getElementById('id-rx-title')) flabel = document.getElementById('id-rx-title').innerHTML;
    PNGcanvas.ctx.fillText(flabel,arrow+10,35);
    
    var fdate = (new Date()).toUTCString();
    PNGcanvas.ctx.fillText(fdate,arrow-PNGcanvas.ctx.measureText(fdate).width-10,35);
    
    var imgURL = PNGcanvas.toDataURL("image/jpeg",0.85);

    var dlLink = document.createElement('a');
    dlLink.download = fileName;
    dlLink.href = imgURL;
    dlLink.dataset.downloadurl = ["image/jpeg", dlLink.download, dlLink.href].join(':');
//    alert(dlLink.dataset.downloadurl.length/1024);
    document.body.appendChild(dlLink);
    dlLink.click();
    document.body.removeChild(dlLink);
}

function canvas_start_drag(evt, x, y)
{
	var dump_event = false;
	
	// Distinguish ctrl-click right-button meta event from actual right-button on mouse (or touchpad two-finger tap).
	// Must ignore true_right_click case even though contextmenu event is being handled elsewhere.
	var true_right_click = false;
	if (evt.button == mouse.right && !evt.ctrlKey) {
		//dump_event = true;
		true_right_click = true;
		canvas_ignore_mouse_event = true;
		right_click_menu(x, y);
		return;
	}
	
	if (dump_event)
		event_dump(evt, "MDN");

	if (evt.shiftKey && evt.target.id == 'id-dx-container') {
		canvas_ignore_mouse_event = true;
		dx_show_edit_panel(evt, -1);
	} else

	// select waterfall on nearest appropriate boundary (1, 5 or 9/10 kHz depending on band)
	if (evt.shiftKey && !(evt.ctrlKey || evt.altKey)) {
		canvas_ignore_mouse_event = true;
		var step_Hz = 1000;
		var fold = canvas_get_dspfreq(x);
		var b = find_band(fold);
		if (b != null && (b.name == 'LW' || b.name == 'MW')) {
			if (cur_mode == 'am' || cur_mode == 'amn' || cur_mode == 'lsb' || cur_mode == 'usb') {
				step_Hz = step_9_10? 9000 : 10000;
				//console.log('SFT-CLICK 9_10');
			}
		} else
		if (b != null && (b.s == svc.B)) {		// SWBC bands
			if (cur_mode == 'am' || cur_mode == 'amn' || cur_mode == 'lsb' || cur_mode == 'usb') {
				step_Hz = 5000;
				//console.log('SFT-CLICK SWBC');
			}
		}
		
		var trunc = fold / step_Hz;
		var fnew = Math.round(trunc) * step_Hz;
		//console.log('SFT-CLICK '+cur_mode+' fold='+fold+' step='+step_Hz+' trunc='+trunc+' fnew='+fnew);
		freqmode_set_dsp_kHz(fnew/1000, null);
	} else

	// lookup mouse pointer frequency in online resource appropriate to the frequency band
	if (evt.shiftKey && (evt.ctrlKey || evt.altKey)) {
		canvas_ignore_mouse_event = true;
		freq_database_lookup(canvas_get_dspfreq(x), evt.altKey);
	} else
	
	// page scrolling via ctrl & alt-key click
	if (evt.ctrlKey) {
		canvas_ignore_mouse_event = true;
		page_scroll(-page_scroll_amount);
	} else
	
	if (evt.altKey) {
		canvas_ignore_mouse_event = true;
		page_scroll(page_scroll_amount);
	}
	
	canvas_mouse_down = true;
	canvas_dragging = false;
	canvas_drag_last_x = canvas_drag_start_x = x;
	canvas_drag_last_y = canvas_drag_start_y = y;
}

function canvas_mousedown(evt)
{
	//console.log("C-MD");
   //event_dump(evt, "C-MD");
	canvas_start_drag(evt, evt.pageX, evt.pageY);
	evt.preventDefault();	// don't show text selection mouse pointer
}

function canvas_touchStart(evt)
{
   if (evt.targetTouches.length == 1) {
		canvas_start_drag(evt, evt.targetTouches[0].pageX, evt.targetTouches[0].pageY);
	}
	evt.preventDefault();	// don't show text selection mouse pointer
}

function spectrum_tooltip_update(evt, clientX, clientY)
{
	var target = (evt.target == spectrum_dB || evt.currentTarget == spectrum_dB || evt.target == spectrum_dB_ttip || evt.currentTarget == spectrum_dB_ttip);
	//console.log('CD '+ target +' x='+ clientX +' tgt='+ evt.target.id +' ctg='+ evt.currentTarget.id);
	//if (kiwi_isMobile()) alert('CD '+ tf +' x='+ clientX +' tgt='+ evt.target.id +' ctg='+ evt.currentTarget.id);

	if (target) {
		//event_dump(evt, 'SPEC');
		
		// This is a little tricky. The tooltip text span must be included as an event target so its position will update when the mouse
		// is moved upward over it. But doing so means when the cursor goes below the bottom of the tooltip container, the entire
		// spectrum div in this case, having included the tooltip text span will cause it to be re-positioned again. And the hover
		// doesn't go away unless the mouse is moved quickly. So to stop this we need to manually detect when the mouse is out of the
		// tooltip container and stop updating the tooltip text position so the hover will end.
		
		if (clientY >= 0 && clientY < height_spectrum_canvas) {
			spectrum_dB_ttip.style.left = px(clientX);
			spectrum_dB_ttip.style.bottom = px(200 + 10 - clientY);
			var dB = (((height_spectrum_canvas - clientY) / height_spectrum_canvas) * full_scale) + mindb;
			spectrum_dB_ttip.innerHTML = dB.toFixed(0) +' dBm';
		}
	}
}

function canvas_drag(evt, x, y, clientX, clientY)
{
	if (!waterfall_setup_done) return;
	//element=html("id-freq-show");
	var relativeX = x;
	var relativeY = y;
	spectrum_tooltip_update(evt, clientX, clientY);

	if (canvas_mouse_down && !canvas_ignore_mouse_event) {
		if (!canvas_dragging && Math.abs(x - canvas_drag_start_x) > canvas_drag_min_delta) {
			canvas_dragging = true;
			canvas_container.style.cursor = "move";
			//console.log('cc move');
		}
		if (canvas_dragging) {
			var deltaX = canvas_drag_last_x - x;
			var deltaY = canvas_drag_last_y - y;

			var dbins = norm_to_bins(deltaX / waterfall_width);
			waterfall_pan_canvases(dbins);

			canvas_drag_last_x = x;
			canvas_drag_last_y = y;
		}
	} else {
		w3_innerHTML('id-mouse-unit', format_frequency("{x}", canvas_get_dspfreq(relativeX) + cfg.freq_offset*1e3, 1e3, 2));
		//console.log("MOU rX="+relativeX.toFixed(1)+" f="+canvas_get_dspfreq(relativeX).toFixed(1));
	}
}

function canvas_mousemove(evt)
{
	//console.log("C-MM");
   //event_dump(evt, "C-MM");
	canvas_drag(evt, evt.pageX, evt.pageY, evt.clientX, evt.clientY);
}

function canvas_touchMove(evt)
{
	for (var i=0; i < evt.touches.length; i++) {
		var x = evt.touches[i].pageX;
		var y = evt.touches[i].pageY;
		canvas_drag(evt, x, y, x, y);
	}
	evt.preventDefault();
}

function canvas_end_drag2()
{
	canvas_container.style.cursor = "crosshair";
	canvas_mouse_down = false;
	canvas_ignore_mouse_event = false;
}

function canvas_container_mouseout(evt)
{
	canvas_end_drag2();
}

//function body_mouseup() { canvas_end_drag2(); console.log("body_mouseup"); }
//function window_mouseout() { canvas_end_drag2(); console.log("document_mouseout"); }

function canvas_end_drag(evt, x)
{
	if (!waterfall_setup_done) return;
	//console.log("MUP "+this.id+" ign="+canvas_ignore_mouse_event);
	var relativeX = x;

	if (canvas_ignore_mouse_event) {
	   //console.log('## canvas_ignore_mouse_event');
		//ignore_next_keyup_event = true;
	} else {
		if (!canvas_dragging) {
			//event_dump(evt, "MUP");
			demodulator_set_offset_frequency(0, canvas_get_carfreq_offset(relativeX, true));		
		} else {
			canvas_end_drag2();
		}
	}
	
	canvas_mouse_down = false;
	canvas_ignore_mouse_event = false;
}

function canvas_mouseup(evt)
{
	//console.log("C-MU");
   //event_dump(evt, "C-MU");
	canvas_end_drag(evt, evt.pageX);
}

function canvas_touchEnd(evt)
{
	canvas_end_drag(evt, canvas_drag_last_x);
	spectrum_tooltip_update(evt, canvas_drag_last_x, canvas_drag_last_y);
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
// zoom
////////////////////////////////

function zoom_finally()
{
	w3_innerHTML('id-nav-optbar-wf', 'WF'+ zoom_level.toFixed(0));
	wf_gnd_value = wf_gnd_value_base - zoomCorrection();
   extint_environment_changed( { zoom:1 } );
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
   if (audioFFT_active) {
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
			var b = arg2;	// band specified by caller
			if (b != undefined) {
				zoom_level = b.zoom_level;
				cf = b.cf;
				if (sb_trace)
					console.log("ZTB-user f="+f+" cf="+cf+" b="+b.name+" z="+b.zoom_level);
			} else {
				for (i=0; i < bands.length; i++) {		// search for first containing band
					b = bands[i];
					if (f >= b.min && f <= b.max)
						break;
				}
				if (i != bands.length) {
					//console.log("ZTB-calc f="+f+" cf="+b.cf+" b="+b.name+" z="+b.zoom_level);
					zoom_level = b.zoom_level;
					cf = b.cf;
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

var page_scroll_amount = 0.8;

function page_scroll(norm_dir)
{
   if (!audioFFT_active) {
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


var window_width;
var waterfall_width;
var waterfall_scrollable_height;

var canvas_container;
var canvas_annotation;
var canvas_phantom;

var annotation_div;

var wf_canvases = [];
var old_canvases = [];
var wf_cur_canvas = null;
var wf_canvas_default_height = 200;
var wf_canvas_actual_line;
var wf_canvas_id_seq = 1;

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

function add_canvas()
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

	canvas_container.appendChild(new_canvas);
	add_canvas_listner(new_canvas);
	wf_canvases.unshift(new_canvas);		// add to front of array which is top of waterfall

	wf_cur_canvas = new_canvas;
	if (kiwi_gc_wf) new_canvas = null;	// gc
}

var spectrum_canvas, spectrum_ctx;
var spectrum_dB, spectrum_dB_ttip;

function init_canvas_container()
{
	window_width = window.innerWidth;		// window width minus any scrollbar
	canvas_container = html("id-waterfall-container");
	waterfall_width = canvas_container.clientWidth;
	//console.log("init_canvas_container ww="+waterfall_width);
	canvas_container.addEventListener("mouseout", canvas_container_mouseout, false);
	//window.addEventListener("mouseout",window_mouseout,false);
	//document.body.addEventListener("mouseup",body_mouseup,false);

	// annotation canvas for FSK shift markers etc.
   canvas_annotation = create_canvas('id-annotation-canvas', wf_fft_size, wf_canvas_default_height, waterfall_width, wf_canvas_default_height);
   canvas_annotation.style.zIndex = 1;    // make on top of waterfall
	canvas_container.appendChild(canvas_annotation);
	add_canvas_listner(canvas_annotation);
	
	// annotation div for text containing links etc.
	annotation_div = w3_el('id-annotation-div');
	add_canvas_listner(annotation_div);

	// a phantom one at the end
	// not an actual canvas but a <div> spacer
	canvas_phantom = html("id-phantom-canvas");
	add_canvas_listner(canvas_phantom);
	canvas_phantom.style.width = px(canvas_container.clientWidth);

	// the first one to get started
	add_canvas();

   spectrum_canvas = create_canvas('id-spectrum-canvas', wf_fft_size, height_spectrum_canvas, waterfall_width, height_spectrum_canvas);
	html("id-spectrum-container").appendChild(spectrum_canvas);
	spectrum_ctx = spectrum_canvas.ctx;
	add_canvas_listner(spectrum_canvas);
	spectrum_ctx.font = "10px sans-serif";
	spectrum_ctx.textBaseline = "middle";
	spectrum_ctx.textAlign = "left";

	spectrum_dB = html("id-spectrum-dB");
	spectrum_dB.style.height = px(height_spectrum_canvas);
	spectrum_dB.style.width = px(waterfall_width);
	spectrum_dB.innerHTML = '<span id="id-spectrum-dB-ttip" class="class-spectrum-dB-tooltip class-tooltip-text"></span>';
	add_canvas_listner(spectrum_dB);
	spectrum_dB_ttip = html("id-spectrum-dB-ttip");
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

wf_canvas_maxshift = 0;

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
		canvas_container.removeChild(p);
		if (kiwi_gc_wf) p = wf_canvases[wf_canvases.length-1] = null;	// gc
		wf_canvases.pop();
	}
	
	// set the height of the phantom to fill the unused space
	wf_canvas_maxshift++;
	if (wf_canvas_maxshift < canvas_container.clientHeight) {
		canvas_phantom.style.top = px(wf_canvas_maxshift);
		canvas_phantom.style.height = px(canvas_container.clientHeight - wf_canvas_maxshift);
		w3_show_block(canvas_phantom);
	} else
	if (!canvas_phantom.hidden) {
		w3_hide(canvas_phantom);
		canvas_phantom.hidden = true;
	}
	
	//canvas_container.style.height = px(((wf_canvases.length-1)*wf_canvas_default_height)+(wf_canvas_default_height-wf_canvas_actual_line));
	//canvas_container.style.height = "100%";
}

function resize_canvases(zoom)
{
	window_width = canvas_container.innerWidth;
	waterfall_width = canvas_container.clientWidth;
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

	spectrum_canvas.style.width = new_width;

   // above width change clears canvas, so redraw
   if (rx_chan >= wf_chans) {
      w3_innerHTML('id-annotation-div',
         w3_div('w3-section w3-container',
            w3_text('w3-large|color:cyan', 'Audio FFT<br>'),
            w3_text('w3-small|color:cyan',
                  'Zoom waterfall not available<br>' +
                  'on channels 2-6 of Kiwis<br>' +
                  'configured for 8 channels.<br>'
            ),
            w3_text('w3-small|color:cyan',
               'For details see: '+
               w3_link('w3-small', 'valentfx.com/vanilla/discussion/1309', 'forum post') + '.'
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
	   if (audioFFT_active) {
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


////////////////////////////////
// mobile
////////////////////////////////

var mobile_cur_portrait;
//var mobile_seq = 0;

function mobile_poll_orientation()
{
	//if (mobile_seq & 1) w3_innerHTML('id-rx-title', 'w='+ screen.width +' h='+ screen.height +' '+ mobile_seq);
	//if (mobile_seq & 1) w3_innerHTML('id-rx-title', 'overflowY='+ css_style(w3_el('id-waterfall-container'), 'overflow-y') +' '+ mobile_seq);
	//mobile_seq++;

	var isPortrait = (screen.width < screen.height);
	if (isPortrait == mobile_cur_portrait) return;
	mobile_cur_portrait = isPortrait;

	var el = w3_el('id-control');
   if (isPortrait && screen.width <= 600) {
	   // scale control panel up or down to fit width of all narrow screens
	   var scale = screen.width / el.uiWidth * 0.95;
	   //alert('scnW='+ screen.width +' cpW='+ el.uiWidth +' sc='+ scale.toFixed(2));
      el.style.transform = 'scale('+ scale.toFixed(2) +')';
      el.style.transformOrigin = 'bottom right';
      el.style.right = '0px';
   } else {
      el.style.transform = 'none';
      el.style.right = '10px';
   }
}

function mobile_init()
{
	// When a mobile device connects to a Kiwi while held in portrait orientation:
	// Remove top bar and minimize control panel if width < oldest iPad width (768px)
	// which should catch all iPhones but no iPads (iPhone X width = 414px).
	// Also scale control panel for small-screen tablets, e.g. 7" tablets with 600px portrait width.

	var isPortrait = (screen.width < screen.height);
	mobile_cur_portrait = undefined;
	
	// anything smaller than iPad: remove top bar and switch control panel to "off".
	if (isPortrait && screen.width < 768) {
	   toggle_or_set_hide_topbar(1);
	   optbar_focus('optbar-off', 'init');
	}
	
	// for narrow screen devices, i.e. phones and 7" tablets
	if (isPortrait && screen.width <= 600) {
	
	   w3_hide('id-readme');   // don't show readme panel closed icon
	   
	   // remove top bar and band/label areas on phones
	   if (screen.width < 600) {
	      toggle_or_set_hide_topbar(3);
	   }
	}
	
	setInterval(mobile_poll_orientation, 500);
}


////////////////////////////////
// waterfall / spectrum
////////////////////////////////

var waterfall_setup_done=0;
var waterfall_timer;
var waterfall_ms;
var audioFFT_active, audioFFT_prev_mode, audioFFT_clear_wf;

function waterfall_init()
{
   if (!okay_waterfall_init) {
      return;
   }
   
	init_canvas_container();
   //audioFFT_active = (dbgUs && rx_chan >= wf_chans);
   audioFFT_active = (rx_chan >= wf_chans);
	resize_waterfall_container(false);
	resize_canvases();
	panels_setup();
	ident_init();
	scale_setup();
	mkcolormap();
	bands_init();
	spectrum_init();
	dx_schedule_update();
	users_init(false);
	stats_init();
	check_top_bar_congestion();
	if (spectrum_show) setTimeout(spec_show, 2000);    // after control instantiation finishes
	audioFFT_setup();

	waterfall_ms = 900/wf_fps_max;
	waterfall_timer = window.setInterval(waterfall_dequeue, waterfall_ms);
	console.log('waterfall_dequeue @ '+ waterfall_ms +' msec');
	
	if (shortcut.keys != '') setTimeout(keyboard_shortcut_url_keys, 3000);

   if (kiwi_isMobile())
      mobile_init();
	waterfall_setup_done=1;
}

var dB_bands = [];

/*
var color_bands = [
	"#993333",
	"#9966ff", "#0066ff", "#00ccff", "#00ffcc", "#66ff33", "#ccff33", "#ffcc00", "#ff6600",
	"#ff0066", "#ff33cc", "#ff00ff", "#ffffff"
];
var color_bands_dB = [
	9999,
	-120,	-110,	-100,	-90,	-80,	-70,	-60,	-50,
	-40,	-30,	-20,	-10
];
*/

var redraw_spectrum_dB_scale = false;
var spectrum_colormap, spectrum_colormap_transparent;
var spectrum_update = 0, spectrum_last_update = 0;
var spectrum_image;

function spectrum_init()
{
	spectrum_colormap = spectrum_ctx.createImageData(1, spectrum_canvas.height);
	spectrum_colormap_transparent = spectrum_ctx.createImageData(1, spectrum_canvas.height);
	update_maxmindb_sliders();
	spectrum_dB_bands();
	var spectrum_update_rate_Hz = kiwi_isMobile()? 10:10;  // limit update rate since rendering spectrum is currently expensive
	//if (kiwi_isMobile()) alert('spectrum_update_rate_Hz = '+ spectrum_update_rate_Hz +' Hz');
	setInterval(function() { spectrum_update++ }, 1000 / spectrum_update_rate_Hz);

   spectrum_image = spectrum_ctx.createImageData(spectrum_canvas.width, spectrum_canvas.height)
   
   if (!audioFFT_active && rx_chan >= wf_chans) {
		// clear entire spectrum canvas to black
		var sw = spectrum_canvas.width;
		var sh = spectrum_canvas.height;
		spectrum_ctx.fillStyle = "black";
		spectrum_ctx.fillRect(0,0,sw,sh);
		
	   spectrum_ctx.font = "18px sans-serif";
      spectrum_ctx.fillStyle = "white";
      var text =
         wf_chans?
            ('Spectrum not available for rx'+ rx_chan)
         :
            'Spectrum not allowed on this Kiwi';
      var tw = spectrum_ctx.measureText(text).width;
      spectrum_ctx.fillText(text, sw/2-tw/2, sh/2);
   }
}

// based on WF max/min range, compute color banding each 10 dB for spectrum display
function spectrum_dB_bands()
{
	dB_bands = [];
	var i=0;
	var color_shift_dB = -8;	// give a little headroom to the spectrum colormap
	var s_maxdb = maxdb, s_mindb = mindb;
	var s_full_scale = s_maxdb - s_mindb;
	var barmax = s_maxdb, barmin = s_mindb + color_shift_dB;
	var rng = barmax - barmin;
	rng = rng? rng : 1;	// can't be zero
	//console.log("DB barmax="+barmax+" barmin="+barmin+" s_maxdb="+s_maxdb+" s_mindb="+s_mindb);
	var last_norm = 0;
	for (var dB = Math.floor(s_maxdb/10)*10; (s_mindb-dB) < 10; dB -= 10) {
		var norm = 1 - ((dB - s_mindb) / s_full_scale);
		var cmi = Math.round((dB - barmin) / rng * 255);
		var color = color_map[cmi];
		var color_transparent = color_map_transparent[cmi];
		var color_name = '#'+(color >>> 8).toString(16).leadingZeros(6);
		dB_bands[i] = { dB:dB, norm:norm, color:color_name };
		
		var ypos = function(norm) { return Math.round(norm * spectrum_canvas.height) }
		for (var y = ypos(last_norm); y < ypos(norm); y++) {
			for (var j=0; j<4; j++) {
				spectrum_colormap.data[y*4+j] = ((color>>>0) >> ((3-j)*8)) & 0xff;
				spectrum_colormap_transparent.data[y*4+j] = ((color_transparent>>>0) >> ((3-j)*8)) & 0xff;
			}
		}
		//console.log("DB"+i+' '+dB+' norm='+norm+' last_norm='+last_norm+' cmi='+cmi+' '+color_name+' sh='+spectrum_canvas.height);
		last_norm = norm;
		
		i++;
	}
	redraw_spectrum_dB_scale = true;
}

var waterfall_dont_scale=0;
var need_maxmindb_update = false;

var need_clear_specavg = false, clear_specavg = true;
var specavg = [], specpeak = [];

// amounts empirically determined
var wf_swallow_samples = [ 2, 4, 8, 18 ];    // for zoom: 11, 12, 13, 14
var x_bin_server_last, wf_swallow = 0;

function waterfall_add(data_raw, audioFFT)
{
   var x, y;
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
      var wf_flags = { COMPRESSED:1 };
   
      data_arr_u8 = new Uint8Array(data_raw, 16);	// unsigned dBm values, converted to signed later on
      var bytes = data_arr_u8.length;
   
      // when caught up, update the max/min db so lagging w/f data doesn't use wrong (newer) zoom correction
      if (need_maxmindb_update && zoom_level == x_zoom_server) {
         update_maxmindb_sliders();
         need_maxmindb_update = false;
      }
   
      // when caught up, clear spectrum using new data
      if (need_clear_specavg && x_bin == x_bin_server && zoom_level == x_zoom_server) {
         clear_specavg = true;
         need_clear_specavg = false;
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
               //console.log('wf_fps='+ wf_fps +' wf_swallow_samples='+ wf_swallow_samples[zoom_level-11] +' wf_swallow='+ wf_swallow);
            }
         }
      }
   } else {
      data = data_raw;     // unsigned dB values, converted to signed later on
      if (need_clear_specavg) {
         clear_specavg = true;
         need_clear_specavg = false;
      }
   }
	
	var sw, sh, tw=25;
	var need_spectrum_update = false;
	
	if (spectrum_display && spectrum_update != spectrum_last_update) {
		spectrum_last_update = spectrum_update;
		need_spectrum_update = true;

		// clear entire spectrum canvas to black
		sw = spectrum_canvas.width-tw;
		sh = spectrum_canvas.height;
		spectrum_ctx.fillStyle = "black";
		spectrum_ctx.fillRect(0,0,sw,sh);
		
		// draw lines every 10 dB
		// spectrum data will overwrite
		spectrum_ctx.fillStyle = "lightGray";
		for (var i=0; i < dB_bands.length; i++) {
			var band = dB_bands[i];
			y = Math.round(band.norm * sh);
			spectrum_ctx.fillRect(0,y,sw,1);
		}

		if (clear_specavg) {
			for (x=0; x<sw; x++) {
				specavg[x] = spectrum_color_index(data[x]);
			}
			clear_specavg = false;
			spectrum_peak_clear = true;
		}
		
		if (spectrum_peak_clear) {
			for (x=0; x<sw; x++) {
				specpeak[x] = 0;
			}
			spectrum_peak_clear = false;
		}
		
		// if necessary, draw scale on right side
		if (redraw_spectrum_dB_scale) {
		
			// set sidebar background where the dB text will go
			/*
			spectrum_ctx.fillStyle = "black";
			spectrum_ctx.fillRect(sw,0,tw,sh);
			*/
			for (x = sw; x < spectrum_canvas.width; x++) {
				spectrum_ctx.putImageData(spectrum_colormap_transparent, x, 0, 0, 0, 1, sh);
			}
			
			// the dB scale text
			spectrum_ctx.fillStyle = "white";
			for (var i=0; i < dB_bands.length; i++) {
				var band = dB_bands[i];
				y = Math.round(band.norm * sh);
				spectrum_ctx.fillText(band.dB, sw+3, y-4);
				//console.log("SP x="+sw+" y="+y+' '+dB);
			}
			redraw_spectrum_dB_scale = false;
		}
	}
	
	// Add line to waterfall image			
	
	var oneline_image = canvas.oneline_image;
	var zwf, color;
	
   // spectrum
	if (spectrum_display && need_spectrum_update) {
      spectrum_ctx.fillStyle = 'hsl(180, 100%, 70%)';

		for (x=0; x<sw; x++) {
         var ysp = spectrum_color_index(wf_gnd? wf_gnd_value : data[x]);

         switch (spec_filter) {
         
         case spec_filter_e.IIR:
            // non-linear spectrum filter from Rocky (Alex, VE3NEA)
            // see http://www.dxatlas.com/rocky/advanced.asp
            
            var iir_gain = 1 - Math.exp(-sp_param * ysp/255);
            if (iir_gain <= 0.01) iir_gain = 0.01;    // enforce minimum decay rate
            var z = specavg[x];
            z = z + iir_gain * (ysp - z);
            if (z > 255) z = 255; if (z < 0) z = 0;
            specavg[x] = z;
            break;
            
         case spec_filter_e.MMA:
            z = ysp;
            if (z > 255) z = 255; if (z < 0) z = 0;
            specavg[x] = ((specavg[x] * (sp_param-1)) + z) / sp_param;
            z = specavg[x];
            break;
            
         case spec_filter_e.EMA:
            z = ysp;
            if (z > 255) z = 255; if (z < 0) z = 0;
            specavg[x] += (z - specavg[x]) / sp_param;
            z = specavg[x];
            break;
            
         case spec_filter_e.OFF:
         default:
            z = ysp;
            if (z > 255) z = 255; if (z < 0) z = 0;
            break;
         }

         if (z > specpeak[x]) specpeak[x] = z; 

         // draw the spectrum based on the spectrum colormap which should
         // color the 10 dB bands appropriately
         y = Math.round((1 - z/255) * sh);

         if (spectrum_slow_dev) {
            spectrum_ctx.fillRect(x,y, 1,sh-y);
         } else {
            // fixme: could optimize amount of data moved like smeter
            spectrum_ctx.putImageData(spectrum_colormap, x,0, 0,y, 1,sh-y+1);
         }
		}
		
		if (spectrum_peak) {
         spectrum_ctx.lineWidth = 1;
	      //spectrum_ctx.fillStyle = 'yellow';
         spectrum_ctx.strokeStyle = 'yellow';
         spectrum_ctx.beginPath();
         y = Math.round((1 - specpeak[0]/255) * sh) - 1;
         spectrum_ctx.moveTo(0,y);
         for (x=1; x<sw; x++) {
            y = Math.round((1 - specpeak[x]/255) * sh) - 1;
            spectrum_ctx.lineTo(x,y);
         }
         spectrum_ctx.globalAlpha = 0.55;
         //spectrum_ctx.fill();
         spectrum_ctx.stroke();
         spectrum_ctx.globalAlpha = 1;
      }
	}

   // waterfall
   for (x=0; x<w; x++) {
      zwf = waterfall_color_index(wf_gnd? wf_gnd_value : data[x]);
      
      /*
      color = color_map[zwf];

      for (var i=0; i<4; i++) {
         oneline_image.data[x*4+i] = ((color>>>0) >> ((3-i)*8)) & 0xff;
      }
      */
      oneline_image.data[x*4  ] = color_map_r[zwf];
      oneline_image.data[x*4+1] = color_map_g[zwf];
      oneline_image.data[x*4+2] = color_map_b[zwf];
      oneline_image.data[x*4+3] = 0xff;
   }
	
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
	if (audioFFT == 0 && typeof(IBP_scan_plot) !== 'undefined') IBP_scan_plot(oneline_image);
	
	if (need_wf_autoscale) {
	   var pwr_dBm = [];
      var autoscale = audioFFT_active? data.slice(256, 768) : data;
	   var len = autoscale.length;
	   
	   // convert from transmitted values to true dBm
	   for (var i = 0; i < len; i++) {
	      pwr_dBm[i] = dB_wire_to_dBm(autoscale[i]);
      }
	   pwr_dBm.sort(function(a,b) {return a-b});
	   var noise = pwr_dBm[Math.floor(0.5 * len)];
	   var signal = pwr_dBm[Math.floor(0.98 * len)];
	   //console.log('wf autoscale: min='+ pwr_dBm[0] +' noise='+ noise +' signal='+ signal +' max='+ pwr_dBm[len-1]);
      setmaxdb(1, signal + 30);
      setmindb(1, noise - 10);
      update_maxmindb_sliders();
	   need_wf_autoscale = false;
	}
   
	// If data from server hasn't caught up to our panning or zooming then fix it.
	// This code is tricky and full of corner-cases.
	
	if (audioFFT == 0) {
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
      }
      
      if (x_bin != x_bin_server) {
         pixel_dx = bins_to_pixels(3, x_bin - x_bin_server, zoom_level);
         if (sb_trace)
            console.log("WF bin fix L="+wf_canvas_actual_line+" xb="+x_bin+" xbs="+x_bin_server+" pdx="+pixel_dx);
         waterfall_pan(canvas, wf_canvas_actual_line, pixel_dx);
         if (sb_trace) console.log('WF bin fixed');
      }
   
      if (sb_trace && x_bin == x_bin_server && zoom_level == x_zoom_server) {
         console.log('--WF FIXUP ALL DONE--');
         sb_trace=0;
      }
   }
   
	wf_canvas_actual_line--;
	wf_shift_canvases();
	if (wf_canvas_actual_line < 0) add_canvas();
	
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
   if (audioFFT_active) return;
	
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
	need_clear_specavg = true;
	dx_schedule_update();
   extint_environment_changed( { waterfall_pan:1 } );

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
	
	need_clear_specavg = true;
}

// window:
//		top container
//		non-waterfall container
//		waterfall container

function waterfall_height()
{
	var top_height = html("id-top-container").clientHeight;
	var non_waterfall_height = html("id-non-waterfall-container").clientHeight;
	var wf_height = window.innerHeight - top_height - non_waterfall_height;
	//console.log('## waterfall_height: wf_height='+ wf_height +' winh='+ window.innerHeight +' th='+ top_height +' nh='+ non_waterfall_height);
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
		canvas_container.style.height = px(wf_height);
		waterfall_scrollable_height = wf_height * 3;

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

function waterfall_add_queue(what)
{
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
	
	while (waterfall_queue.length != 0) {

		var seq = waterfall_queue[0].seq;
		var target = audio_ext_sequence + waterfall_delay;
		if (seq > target) {
			//console.log('SEQ too soon: '+ seq +' > '+ target +' ('+ (seq - target) +')');
			return;		// too soon
		}

		var now = Date.now();
		if (seq == audio_ext_sequence && now < (waterfall_last_out + waterfall_queue[0].spacing)) {
			//console.log('SEQ need spacing');
			return;		// need spacing
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

var colormap_select = 1;
var colormap_sqrt = 0;

// adjusted so the overlaid white text of the spectrum scale doesn't get washed out
var spectrum_scale_color_map_transparency = 200;

var color_map = new Uint32Array(256);
var color_map_transparent = new Uint32Array(256);
var color_map_r = new Uint8Array(256);
var color_map_g = new Uint8Array(256);
var color_map_b = new Uint8Array(256);

function mkcolormap()
{
	for (var i=0; i<256; i++) {
		var r, g, b;
		
		switch (colormap_select) {
		
		case 1:
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
			
		case 2:
			// greyscale
			black_level = 0.0;
			white_level = 48.0;
			var v = black_level + Math.round((255 - black_level + white_level) * i/255);
			if (v > 255) v = 255;
			r = g = b = v;
			break;
			
		case 3:
			// user 1
			var c_from = { r:255, g:0, b:255 };
			var c_to = { r:0, g:255, b:0 };
			var s = 1.0;
			r = c_from.r*(i/255) + c_to.r*((255-i)/255);
			r = Math.round(r * (s + (i/255)*(1-s)));
			g = c_from.g*(i/255) + c_to.g*((255-i)/255);
			g = Math.round(g * (s + (i/255)*(1-s)));
			b = c_from.b*(i/255) + c_to.b*((255-i)/255); 
			b = Math.round(b * (s + (i/255)*(1-s)));
			break;
			
		case 0:
		default:
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
		}

		color_map[i] = (r<<24) | (g<<16) | (b<<8) | 0xff;
		color_map_transparent[i] = (r<<24) | (g<<16) | (b<<8) | spectrum_scale_color_map_transparency;
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

function do_waterfall_index(db_value, sqrt)
{
   var dBm = dB_wire_to_dBm(db_value);
	if (dBm < mindb) dBm = mindb;
	if (dBm > maxdb) dBm = maxdb;
	var relative_value = dBm - mindb;

	var value_percent_default = relative_value/full_scale;
	var value_percent = value_percent_default;
	
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
	
	var i = value_percent*255;
	i = Math.round(i);
	if (i<0) i=0;
	if (i>255) i=255;
	return i;
}

function spectrum_color_index(db_value)
{
	return do_waterfall_index(db_value, 0);
}

function waterfall_color_index(db_value)
{
	return do_waterfall_index(db_value, colormap_sqrt);
}

function waterfall_color_index_max_min(db_value, maxdb, mindb)
{
	// convert to negative-only signed dBm (i.e. -256 to -1 dBm)
	// done this way because -127 is the smallest value in an int8 which isn't enough
	db_value = -(255 - db_value);		
	
	if (db_value < mindb) db_value = mindb;
	if (db_value > maxdb) db_value = maxdb;
	var relative_value = db_value - mindb;
	var fullscale = maxdb - mindb;
	fullscale = fullscale? full_scale : 1;	// can't be zero
	var value_percent = relative_value / fullscale;
	
	var i = value_percent*255;
	i = Math.round(i);
	if (i<0) i=0;
	if (i>255) i=255;
	return i;
}

function color_index_max_min(value, maxdb, mindb)
{
	if (value < mindb) value = mindb;
	if (value > maxdb) value = maxdb;
	var relative_value = value - mindb;
	var fullscale = maxdb - mindb;
	fullscale = fullscale? full_scale : 1;	// can't be zero
	var value_percent = relative_value / fullscale;
	
	var i = value_percent*255;
	i = Math.round(i);
	if (i<0) i=0;
	if (i>255) i=255;
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

var fft = {
   init: false,
   comp_1x: false,
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
   if (!audioFFT_active) return;
   zoom_level = 0;
   var last_AF_max_dB = readCookie('last_AF_max_dB');
   if (last_AF_max_dB == null) last_AF_max_dB = maxdb;
   var last_AF_min_dB = readCookie('last_AF_min_dB');
   if (last_AF_min_dB == null) last_AF_min_dB = mindb_un;
   setmaxdb(1, last_AF_max_dB);
   setmindb(1, last_AF_min_dB);
   update_maxmindb_sliders();

   // Hanning
   var window = function(i, nsamp) {
      return (0.5 - 0.5 * Math.cos((2 * Math.PI * i)/(nsamp-1)));
   };

   for (i = 0; i < 512; i++) fft.window_512[i] = window(i, 512);
   for (i = 0; i < 1024; i++) fft.window_1k[i] = window(i, 1024);
   for (i = 0; i < 2048; i++) fft.window_2k[i] = window(i, 2048);
}

function audioFFT_update()
{
   mkscale();
   dx_schedule_update();
   freq_link_update();
   w3_innerHTML('id-nav-optbar-wf', 'WF');
   freqset_select();

   // clear waterfall
   if (audioFFT_clear_wf) {
      wf_canvases.forEach(function(cv) {
         cv.ctx.fillStyle = "Black";
         cv.ctx.fillRect(0,0, cv.width,cv.height);
      });
      need_clear_specavg = true;
      audioFFT_clear_wf = false;
   }
}

function wf_audio_FFT(audio_data, samps)
{
   if (!audioFFT_active) return;
   
   if (!kiwi_load_js_polled(fft.audioFFT_dynload, ['pkgs/js/Ooura_FFT32.js'])) return;
   
   var i, j, k;
   
   //console.log('iq='+ audio_mode_iq +' comp='+ audio_compression +' samps='+ samps);

   var iq = (ext_get_mode() == 'iq');
   if (!fft.init || iq != fft.iq || audio_compression != fft.comp) {
      console.log('audio_FFT: SWITCHING iq='+ iq +' comp='+ audio_compression);
      var type;
      if (iq) {
         type = 'complex';
         fft.size = 1024;
         fft.offt = ooura32.FFT(fft.size, {"type":"complex", "radix":4});
         fft.i_re = ooura32.vectorArrayFactory(fft.offt);
         fft.i_im = ooura32.vectorArrayFactory(fft.offt);
      } else {
         type = 'real';
         fft.size = audio_compression? (fft.comp_1x? 2048 : 1024) : 512;
         fft.offt = ooura32.FFT(fft.size, {"type":"real", "radix":4});
         fft.i_re = ooura32.scalarArrayFactory(fft.offt);
      }
      fft.o_re = ooura32.vectorArrayFactory(fft.offt);
      fft.o_im = ooura32.vectorArrayFactory(fft.offt);
      //fft.scale = 10.0 * 2.0 / (fft.size * fft.size * fft.CUTESDR_MAX_VAL * fft.CUTESDR_MAX_VAL);
      // FIXME: What's the correct value to use here? Adding the third fft.size was just arbitrary.
      fft.scale = 10.0 * 2.0 / (fft.size * fft.size * fft.size * fft.CUTESDR_MAX_VAL * fft.CUTESDR_MAX_VAL);
      
      for (i = 0; i < 1024; i++) fft.pwr_dB[i] = 0;
      fft.iq = iq;
      fft.comp = audio_compression;
      fft.init = true;
   }

   if (fft.iq) {
      for (i = 0, j = 0; i < 1024; i += 2, j++) {
         fft.i_re[j] = audio_data[i] * fft.window_512[j];
         fft.i_im[j] = audio_data[i+1] * fft.window_512[j];
      }
      fft.offt.fft(fft.offt, fft.i_re.buffer, fft.i_im.buffer, fft.o_re.buffer, fft.o_im.buffer);
      for (j = 0, k = 512; j < 256; j++, k++) {
         var re = fft.o_re[j], im = fft.o_im[j];
         var pwr = re*re + im*im;
         var dB = 10.0 * Math.log10(pwr * fft.scale + 1e-30);
         dB = Math.round(255 + dB);
         fft.pwr_dB[k] = dB;
      }
      for (j = 256, k = 256; j < 512; j++, k++) {
         var re = fft.o_re[j], im = fft.o_im[j];
         var pwr = re*re + im*im;
         var dB = 10.0 * Math.log10(pwr * fft.scale + 1e-30);
         dB = Math.round(255 + dB);
         fft.pwr_dB[k] = dB;
      }
      waterfall_queue.push({ data:fft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
   } else {
      if (audio_compression) {
         if (fft.comp_1x) {
            // 2048 real samples done as 1x 2048-pt FFT
            for (i = 0; i < 2048; i++) {
               fft.i_re[i] = audio_data[i] * fft.window_2k[i];
            }
            fft.offt.fft(fft.offt, fft.i_re.buffer, fft.o_re.buffer, fft.o_im.buffer);
            for (j = 0, k = 256; j < 1024; j++) {
               var re = fft.o_re[j], im = fft.o_im[j];
               var pwr = re*re + im*im;
               fft.dBi[j&1] = Math.round(255 + (10.0 * Math.log10(pwr * fft.scale + 1e-30)));
               if (j & 1) {
                  fft.pwr_dB[k] = Math.max(fft.dBi[0], fft.dBi[1]);
                  k++;
               }
            }
         } else {
            // 2048 real samples done as 2x 1024-pt FFTs
            for (i = 0; i < 1024; i++) {
               fft.i_re[i] = audio_data[i] * fft.window_1k[i];
            }
            fft.offt.fft(fft.offt, fft.i_re.buffer, fft.o_re.buffer, fft.o_im.buffer);
            for (j = 0, k = 256; j < 512; j++, k++) {
               var re = fft.o_re[j], im = fft.o_im[j];
               var pwr = re*re + im*im;
               fft.pwr_dB[k] = Math.round(255 + (10.0 * Math.log10(pwr * fft.scale + 1e-30)));
            }
            waterfall_queue.push({ data:fft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
      
            for (i = 1024; i < 2048; i++) {
               fft.i_re[i] = audio_data[i] * fft.window_2k[i-1024];
            }
            fft.offt.fft(fft.offt, fft.i_re.buffer, fft.o_re.buffer, fft.o_im.buffer);
            for (j = 0, k = 256; j < 512; j++, k++) {
               var re = fft.o_re[j], im = fft.o_im[j];
               var pwr = re*re + im*im;
               fft.pwr_dB[k] = Math.round(255 + (10.0 * Math.log10(pwr * fft.scale + 1e-30)));
            }
         }
         waterfall_queue.push({ data:fft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
      } else {
         for (i = 0; i < 512; i++) {
            fft.i_re[i] = audio_data[i] * fft.window_512[i];
         }
         fft.offt.fft(fft.offt, fft.i_re.buffer, fft.o_re.buffer, fft.o_im.buffer);
         for (j = 0, k = 256; j < 256; j++, k += 2) {
            var re = fft.o_re[j], im = fft.o_im[j];
            var pwr = re*re + im*im;
            fft.pwr_dB[k] = fft.pwr_dB[k+1] = Math.round(255 + (10.0 * Math.log10(pwr * fft.scale + 1e-30)));
         }
         waterfall_queue.push({ data:fft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
      }
   }
}

/*
         // 2048 real samples done as 2x 1024-pt FFTs
         
         for (i = 0; i < 1024; i++) {
            fft.i_re[i] = audio_data[i] * fft.window_1k[i];
         }
         fft.offt.fft(fft.offt, fft.i_re.buffer, fft.o_re.buffer, fft.o_im.buffer);
         for (j = 0, k = 256; j < 512; j++, k++) {
            var re = fft.o_re[j], im = fft.o_im[j];
            var pwr = re*re + im*im;
            fft.pwr_dB[k] = Math.round(255 + (10.0 * Math.log10(pwr * fft.scale + 1e-30)));
         }
         waterfall_queue.push({ data:fft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
   
         for (i = 1024; i < 2048; i++) {
            fft.i_re[i] = audio_data[i] * fft.window_1k[i-1024];
         }
         fft.offt.fft(fft.offt, fft.i_re.buffer, fft.o_re.buffer, fft.o_im.buffer);
         for (j = 0, k = 256; j < 512; j++, k++) {
            var re = fft.o_re[j], im = fft.o_im[j];
            var pwr = re*re + im*im;
            fft.pwr_dB[k] = Math.round(255 + (10.0 * Math.log10(pwr * fft.scale + 1e-30)));
         }
         waterfall_queue.push({ data:fft.pwr_dB, audioFFT:1, seq:0, spacing:0 });
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
	if (typeof demod == "undefined") {
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
	if (typeof demod == "undefined") return fcar;
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
	if (typeof demod != "undefined") {
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
	if (typeof demod != "undefined") {
		freq = center_freq + demod.offset_frequency;
		freq += demod.low_cut + (demod.high_cut - demod.low_cut)/2;
	}
	return freq;
}

function passband_offset_dxlabel(mode)
{
	var pb = passbands[mode];
	var usePBCenter = (mode == 'usb' || mode == 'lsb');
	var offset = usePBCenter? pb.lo + (pb.hi - pb.lo)/2 : 0;
	//console.log("passband_offset: m="+mode+" use="+usePBCenter+' o='+offset);
	return offset;
}


////////////////////////////////
// frequency entry
////////////////////////////////

function freqmode_set_dsp_kHz(fdsp, mode, dont_clear_wf)
{
	fdsp *= 1000;
	//console.log("freqmode_set_dsp_kHz: fdsp="+fdsp+' mode='+mode);
	if (dont_clear_wf != true) audioFFT_clear_wf = true;

	if (mode != undefined && mode != null && mode != cur_mode) {
		//console.log("freqmode_set_dsp_kHz: calling demodulator_analog_replace");
		demodulator_analog_replace(mode, fdsp);
	} else {
		freq_car_Hz = freq_dsp_to_car(fdsp);
		//console.log("freqmode_set_dsp_kHz: calling demodulator_set_offset_frequency NEW freq_car_Hz="+freq_car_Hz);
		demodulator_set_offset_frequency(0, freq_car_Hz - center_freq);
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
	//console.log("FUPD-UI demod-ofreq="+(center_freq + demodulators[0].offset_frequency));
	//kiwi_trace();
	freq_displayed_Hz = freq_car_to_dsp(freq_car_Hz);
	freq_displayed_kHz_str = (freq_displayed_Hz/1000).toFixed(2);
	//console.log("FUPD-UI freq_car_Hz="+freq_car_Hz+' NEW freq_displayed_Hz='+freq_displayed_Hz);
	
	if (!waterfall_setup_done) return;
	
	var obj = w3_el('id-freq-input');
	if (typeof obj == "undefined" || obj == null) return;		// can happen if SND comes up long before W/F

	//obj.value = freq_displayed_kHz_str;
	var f = freq_displayed_Hz + cfg.freq_offset*1e3;
	obj.value = (f/1000).toFixed((f > 100e6)? 1:2);

	//console.log("FUPD obj="+(typeof obj)+" val="+obj.value);
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
	
	writeCookie('last_freq', freq_displayed_kHz_str);
	freq_dsp_set_last = freq_displayed_kHz_str;
	//console.log('## freq_dsp_set_last='+ freq_dsp_set_last);

	freq_step_update_ui();
	freq_link_update();
	
	// update history list
   //console.log('freq_memory update');
   //console.log(freq_memory);
	if (freq_displayed_kHz_str != freq_memory[freq_memory_pointer]) {
	   freq_memory.push(freq_displayed_kHz_str);
	   //console.log('freq_memory push='+ freq_displayed_kHz_str);
	}
	if (freq_memory.length > 25) freq_memory.shift();
	freq_memory_pointer = freq_memory.length-1;
   //console.log('freq_memory ptr='+ freq_memory_pointer);
   //console.log(freq_memory);
	writeCookie('freq_memory', JSON.stringify(freq_memory));
}

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
      
      freqset_complete(2);
   }, kiwi_isMobile()? 2000:3000);
}

function freqset_select(id)
{
   // only switch focus to id-freq-input if active element doesn't specify w3-retain-input-focus
   var ae = document.activeElement;
   //console.log(ae);
   if (!ae || !w3_contains(ae, 'w3-retain-input-focus')) {
	   w3_field_select('id-freq-input', {mobile:1});
	} else {
      //console.log('#### activeElement w3-retain-input-focus');
	}
}

var last_mode_obj = null;

function modeset_update_ui(mode)
{
	if (last_mode_obj != null) last_mode_obj.style.color = "white";
	
	// if sound comes up before waterfall then the button won't be there
	var obj = w3_el('id-button-'+mode);
	if (obj && obj.style) obj.style.color = "lime";
	last_mode_obj = obj;
	squelch_setup(0);
	writeCookie('last_mode', mode);
	freq_link_update();

	// disable compression button in iq mode
	w3_set_props('id-button-compression', 'w3-disabled', (mode == 'iq'));
}

// delay the UI updates called from the audio path until the waterfall UI setup is done
function try_freqset_update_ui()
{
	if (waterfall_setup_done) {
		freqset_update_ui();
		if (audioFFT_active) {
		
         // if not already clearing then clear on iq mode change
		   if (!audioFFT_clear_wf && ((cur_mode == 'iq' && audioFFT_prev_mode != 'iq') || (cur_mode != 'iq' && audioFFT_prev_mode == 'iq')))
		      audioFFT_clear_wf = true;
		   audioFFT_update();
		   audioFFT_prev_mode = cur_mode;
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
	var url = host + '/?f='+ freq_displayed_kHz_str + cur_mode +'z'+ zoom_level;
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
	//console.log("FCMPL from="+ from +" obj="+(typeof obj)+" val="+(obj.value).toString());
	kiwi_clearTimeout(freqset_tout);
   kiwi_clearTimeout(freq_up_down_timeout);
	if (typeof obj == "undefined" || obj == null) return;		// can happen if SND comes up long before W/F

   var p = obj.value.split(/[\/:]/);
	var slash = obj.value.includes('/');
   // 'k' suffix is simply ignored since default frequencies are in kHz
	var f = p[0].replace(',', '.').parseFloatWithUnits('M', 1e-3);    // Thanks Petri, OH1BDF
	var err = true;
	if (f > 0 && !isNaN(f)) {
	   f -= cfg.freq_offset;
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

      ext_set_passband(lo, hi);     // does error checking for NaN, lo < hi, min pbw etc.
	}
}

var ignore_next_keyup_event = false;

// freqset_keyup is called on each key-up while the frequency box is selected so that if a numeric
// entry is made, without a terminating <return> key, a setTimeout(freqset_complete()) can be done to
// arrange automatic completion.

function freqset_keyup(obj, evt)
{
	//console.log("FKU obj="+(typeof obj)+" val="+obj.value +' ignore_next_keyup_event='+ ignore_next_keyup_event);
	//console.log(obj); console.log(evt);
	kiwi_clearTimeout(freqset_tout);
	
	// Ignore modifier-key key-up events triggered because the frequency box is selected while
	// modifier-key-including mouse event occurs somewhere else.
	// Also keeps ident_complete timeout from being set after return key.
	
	// But this is tricky. Key-up of a shift/ctrl/alt/cmd key can only be detected by looking for a
	// evt.key string length != 1, i.e. evt.shiftKey et al don't seem to be valid for the key-up event!
	// But also have to check for them with any_alternate_click_event() in case a modifier key of a
	// normal key is pressed (e.g. shift-$).
	if (evt != undefined && evt.key != undefined) {
		var klen = evt.key.length;
		if (any_alternate_click_event(evt) || (klen != 1 && evt.key != 'Backspace')) {
	
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
   
	freqset_tout = setTimeout(function() {freqset_complete(1);}, 3000);
}

var num_step_buttons = 6;

var up_down = {
	am: [ 0, -1, -0.1, 0.1, 1, 0 ],
	amn: [ 0, -1, -0.1, 0.1, 1, 0 ],
	usb: [ 0, -1, -0.1, 0.1, 1, 0 ],
	lsb: [ 0, -1, -0.1, 0.1, 1, 0 ],
	cw: [ 0, -0.1, -0.01, 0.01, 0.1, 0 ],
	cwn: [ 0, -0.1, -0.01, 0.01, 0.1, 0 ],
	nbfm: [ -5, -1, -0.1, 0.1, 1, 5 ],		// FIXME
	iq: [ -5, -1, -0.1, 0.1, 1, 5 ],
	s4285: [ -5, -1, -0.1, 0.1, 1, 5 ]
};

var up_down_default = {
	am: [ 5, 0, 0, 0, 0, 5 ],
	amn: [ 5, 0, 0, 0, 0, 5 ],
	usb: [ 5, 0, 0, 0, 0, 5 ],
	lsb: [ 5, 0, 0, 0, 0, 5 ],
	cw: [ 1, 0, 0, 0, 0, 1 ],
	cwn: [ 1, 0, 0, 0, 0, 1 ],
};

var NDB_400_1000_mode = 1;		// special 400/1000 step mode for NDB band

function special_step(b, sel, caller)
{
	var s = 'SPECIAL_STEP '+ caller +' sel='+ sel;
	var step_Hz;

	if (b != null && b.name == 'NDB') {
		if (cur_mode == 'cw' || cur_mode == 'cwn') {
			step_Hz = NDB_400_1000_mode;
		} else {
			step_Hz = -1;
		}
		s += ' NDB';
	} else
	if (b != null && (b.name == 'LW' || b.name == 'MW')) {
		if (cur_mode == 'am' || cur_mode == 'amn' || cur_mode == 'lsb' || cur_mode == 'usb') {
			step_Hz = step_9_10? 9000 : 10000;
		} else {
			step_Hz = -1;
		}
		s += ' LW/MW';
	} else
	if (b != null && b.chan != 0) {
		step_Hz = b.chan;
		s += ' band='+ b.name +' ';
	} else {
		step_Hz = -1;
		s += ' no band chan found, use default';
	}
	if (step_Hz == -1) step_Hz = up_down_default[cur_mode][sel]*1000;
	if (sel < num_step_buttons/2) step_Hz = -step_Hz;
	s += ' '+ step_Hz;
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
	
	// audioFFT mode: don't clear waterfall for small frequency steps (third arg = true)
	freqmode_set_dsp_kHz(fnew/1000, null, true);
}

var freq_step_last_mode, freq_step_last_band;

function freq_step_update_ui(force)
{
	if (typeof cur_mode == "undefined" || passbands[cur_mode] == undefined ) return;
	var b = find_band(freq_displayed_Hz);
	
	//console.log("freq_step_update_ui: lm="+freq_step_last_mode+' cm='+cur_mode);
	if (!force && freq_step_last_mode == cur_mode && freq_step_last_band == b) {
		//console.log("freq_step_update_ui: return "+freq_step_last_mode+' '+cur_mode);
		return;
	}

	var show_9_10 = (b != null && (b.name == 'LW' || b.name == 'MW') &&
		(cur_mode == 'am' || cur_mode == 'amn' || cur_mode == 'lsb' || cur_mode == 'usb'))? true:false;
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

function band_info()
{
	var _9_10 = (+cfg.init.AM_BCB_chan)? 10:9;

	var ITU_region = cfg.init.ITU_region + 1;
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

// augment bands[] found in the config.js configuration file
function bands_init()
{
	var i, z;

	var bi = band_info();

	for (i=0; i < bands.length; i++) {
		var b = bands[i];
		bands[i].chan = (typeof b.chan == "undefined")? 0 : b.chan;
		b.min -= b.chan/2; b.max += b.chan/2;
		
		// fix LW/NDB/MW band definitions based on ITU region and MW channel spacing configuration settings
		if (b.name == 'LW') {
			b.min = bi.LW_lo; b.max = bi.NDB_lo; b.chan = 9;
		}
		if (b.name == 'NDB') {
			b.min = bi.NDB_lo; b.max = bi.NDB_hi; b.chan = 0;
		}
		if (b.name == 'MW') {
			b.min = bi.NDB_hi; b.max = bi.MW_hi; b.chan = bi._9_10;
		}
		
		b.min *= 1000; b.max *= 1000; b.chan *= 1000;
		var bw = b.max - b.min;
		for (z=zoom_nom; z >= 0; z--) {
			if (bw <= bandwidth / (1 << z))
				break;
		}
		bands[i].zoom_level = z;
		bands[i].cf = b.min + (b.max - b.min)/2;
		bands[i].longName = b.name +' '+ b.s.name;
		//console.log("BAND "+b.name+" bw="+bw+" z="+z);
		
		// FIXME: Change IBP passband from CW to CWN.
		// A software release can't modify bands[] definition in config.js so do this here.
		// At some point config.js will be eliminated when admin page gets equivalent UI.
		if ((b.s == svc.L || b.s == svc.X) && b.region == 'm') {
		   if (!b.sel.includes('cwn'))
		      b.sel = b.sel.replace('cw', 'cwn');
		}
	}

	mkscale();
}

function find_band(freq)
{
	var b;
	for (var i=0; i < bands.length; i++) {
		b = bands[i];
		if (b.region != "-" && b.region != "*" && b.region.charAt(0) != '>') continue;
		if (freq >= b.min && freq <= b.max)
			return b;;
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
	var r = g_range;
	
	// band bars & station labels
	var tw = band_ctx.canvas.width;
	var i, x, y=band_scale_top, w, h=band_scale_h, ty=y+band_scale_text_top;
	//console.log("BB fftw="+wf_fft_size+" tw="+tw+" rs="+r.start+" re="+r.end+" bw="+(r.end-r.start));
	//console.log("BB pixS="+scale_px_from_freq(r.start, g_range)+" pixE="+scale_px_from_freq(r.end, g_range));
	band_ctx.globalAlpha = 1;
	band_ctx.fillStyle = "White";
	band_ctx.fillRect(0,band_canvas_top,tw,band_canvas_h);

	for (i=0; i < bands.length; i++) {
		var b = bands[i];
		if (b.region != "-" && b.region != "*" && b.region.charAt(0) != '>') continue;

		var x1=0, x2;
		var min_inside = (b.min >= r.start && b.min <= r.end)? 1:0;
		var max_inside = (b.max >= r.start && b.max <= r.end)? 1:0;
		if (min_inside && max_inside) { x1 = b.min; x2 = b.max; } else
		if (!min_inside && max_inside) { x1 = r.start; x2 = b.max; } else
		if (min_inside && !max_inside) { x1 = b.min; x2 = r.end; } else
		if (b.min < r.start && b.max > r.end) { x1 = r.start; x2 = r.end; }

		if (x1) {
			x = scale_px_from_freq(x1, g_range); var xx = scale_px_from_freq(x2, g_range);
			w = xx - x;
			//console.log("BANDS x="+x1+'/'+x+" y="+x2+'/'+xx+" w="+w);
			if (w >= 3) {
				band_ctx.fillStyle = b.s.color;
				band_ctx.globalAlpha = 0.2;
				//console.log("BB x="+x+" y="+y+" w="+w+" h="+h);
				band_ctx.fillRect(x,y,w,h);
				band_ctx.globalAlpha = 1;

				//band_ctx.fillStyle = "Black";
				band_ctx.font = "bold 12px sans-serif";
				band_ctx.textBaseline = "top";
				var tx = x + w/2;
				var txt = b.longName;
				var mt = band_ctx.measureText(txt);
				//console.log("BB mt="+mt.width+" txt="+txt);
				if (w >= mt.width+4) {
					;
				} else {
					txt = b.name;
					mt = band_ctx.measureText(txt);
					//console.log("BB mt="+mt.width+" txt="+txt);
					if (w >= mt.width+4) {
						;
					} else {
						txt = null;
					}
				}
				//if (txt) console.log("BANDS tx="+(tx - mt.width/2)+" ty="+ty+" mt="+mt.width+" txt="+txt);
				if (txt) band_ctx.fillText(txt, tx - mt.width/2, ty);
			}
		}
	}
}

function mk_spurs()
{
	scale_ctx.fillStyle = "red";
	var h = 12;
	var y = scale_canvas_h - h;
	for (var z=0; z <= zoom_level && z < spurs.length; z++) {
		for (var i=0; i < spurs[z].length; i++) {
			var x = scale_px_from_freq(spurs[z][i]*1000, g_range);
			//console.log("SPUR "+spurs[z][i]+" @"+x);
			if (x > window.innerWidth) break;
			if (x < 0) continue;
			scale_ctx.fillRect(x-1,y,2,h);
		}
	}
}

function parse_freq_mode(freq_mode)
{
   var s = new RegExp("([0-9.]*)([^&#]*)").exec(freq_mode);
}

var last_selected_band = 0;

function select_band(v, mode)
{
   var v_num = +v;
   //console.log('select_band t/o_v='+ (typeof v) +' v_num='+ v_num);
   if (!isNaN(v_num)) {
      //console.log('select_band num v='+ v_num);
      b = band_menu[v_num];
   } else {
      //console.log('select_band str v='+ v);
      var i;
      for (i = 0; i < band_menu.length-1; i++) {
         if (band_menu[i] && band_menu[i].name == v)
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
	
	var freq;
	if (typeof b.sel != "undefined") {
		freq = parseFloat(b.sel);
		if (typeof mode == "undefined" && typeof b.sel == "string") {
			mode = b.sel.search(/[a-z]/i);
			mode = (mode == -1)? null : b.sel.slice(mode);
		}
	} else {
		freq = b.cf/1000;
	}

	//console.log("SEL BAND"+v_num+" "+b.name+" freq="+freq+((mode != null)? " mode="+mode:""));
	last_selected_band = v_num;
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
	if (last_selected_band) {
		band = band_menu[last_selected_band];
		//console.log("check_band "+last_selected_band+" reset="+reset+' '+band.min+'/'+band.max);
		
		// check both carrier and pbc frequencies
	   var car = freq_car_Hz;
	   var pbc = freq_passband_center();
	   
		if ((reset != undefined && reset == -1) || ((car < band.min || car > band.max) && (pbc < band.min || pbc > band.max))) {
	      //console.log('check_band OUTSIDE BAND RANGE reset='+ reset +' car='+ car +' pbc='+ pbc +' last_selected_band='+ last_selected_band);
	      //console.log(band);
			w3_select_value('id-select-band', 0);
			last_selected_band = 0;
		}
	}
}
	
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

function confirmation_show_content(s, w, h, close_cb)
{
   w3_innerHTML('id-confirmation-container', s);
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
      toggle_panel('id-confirmation');
      confirmation.displayed = false;
      //console.log('confirmation_panel_close CLOSE');
   }
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
   filter_ident: '',
   filter_notes: '',
   filter_case: 0,
   filter_wild: 0,
   filter_grep: 0,
   ctrl_click: false,
   displayed: [],
};

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
	dx_seq++;
	//g_range = get_visible_freq_range();
	//console.log("DX min="+(g_range.start/1000)+" max="+(g_range.end/1000));
	wf_send('SET MKR min='+ (g_range.start/1000).toFixed(3) +' max='+ (g_range.end/1000).toFixed(3) +' zoom='+ zoom_level +' width='+ waterfall_width);
}

// Why doesn't using addEventListener() to ignore mousedown et al not seem to work for
// div elements created appending to innerHTML?

var dx_ibp_list, dx_ibp_interval, dx_ibp_server_time_ms, dx_ibp_local_time_epoch_ms = 0;
var dx_ibp_freqs = { 14:0, 18:1, 21:2, 24:3, 28:4 };
var dx_ibp_lastsec = 0;

var dx_ibp = [
	'4U1UN', 	'New York',
	'VE8AT',		'Nunavut',
	'W6WX',		'California',
	'KH6RS', 	'Hawaii',
	'ZL6B',		'New Zealand',
	'VK6RBP',	'Australia',
	'JA2IGY',	'Japan',
	'RR9O',		'Siberia',
	'VR2B',		'Hong Kong',
	'4S7B',		'Sri Lanka',
	'ZS6DN',		'South Africa',
	'5Z4B',		'Kenya',
	'4X6TU',		'Israel',
	'OH2B',		'Finland',
	'CS3B',		'Madeira',
	'LU4AA',		'Argentina',
	'OA4B',		'Peru',
	'YV5B',		'Venezuela'
];

var dx_list = [];

function dx_label_cb(arr)
{
	var i;
	var obj = arr[0];
	
	// reply to label step request
	if (obj.t == 2) {
	   var dl = arr[1];
	   if (dl) {
	      //console.log('dx_label_cb: type=2 f='+ dl.f);
	      //console.log(dl);
	      var mode = modes_l[dl.b & DX_MODE];
         freqmode_set_dsp_kHz(dl.f, mode);
         if (dl.lo != 0 && dl.hi != 0) {
            ext_set_passband(dl.lo, dl.hi);     // label has custom pb
         } else {
            var dpb = passbands[mode];
            ext_set_passband(dpb.lo, dpb.hi);   // need to force default pb in case cpb is not default
         }
      } else {
	      //console.log('dx_label_cb: type=2 NO LABEL FOUND');
      }
	   return;
	}
	
	kiwi_clearInterval(dx_ibp_interval);
	dx_ibp_list = [];
	dx_ibp_server_time_ms = obj.s * 1000 + (+obj.m);
	dx_ibp_local_time_epoch_ms = Date.now();
	
	dx_filter_field_err(+obj.f);
	
	var dx_idx, dx_z = 120;
	dx_div.innerHTML = '';
	var s = '';
	dx.displayed = [];
	
	for (i = 1; i < arr.length; i++) {
		obj = arr[i];
		dx_idx = i-1;
		var gid = obj.g;
		var freq = obj.f;
		var moff = obj.o;
		var flags = obj.b;
		var ident = obj.i;
		var notes = (typeof obj.n != 'undefined')? obj.n : '';
		var params = (typeof obj.p != 'undefined')? obj.p : '';

		var freqHz = freq * 1000;
		var loff = passband_offset_dxlabel(modes_l[flags & DX_MODE]);	// always place label at center of passband
		var x = scale_px_from_freq(freqHz + loff, g_range) - 1;	// fixme: why are we off by 1?
		var cmkr_x = 0;		// optional carrier marker for NDBs
		var carrier = freqHz - moff;
		if (moff) cmkr_x = scale_px_from_freq(carrier, g_range);
	
		var t = dx_label_top + (30 * (dx_idx&1));		// stagger the labels vertically
		var h = dx_container_h - t;
		var color = type_colors[flags & DX_TYPE];
		if (ident == 'IBP' || ident == 'IARU%2fNCDXF') color = type_colors[0x20];		// FIXME: hack for now
		//console.log("DX "+dx_seq+':'+dx_idx+" f="+freq+" o="+loff+" k="+moff+" F="+flags+" m="+modes_u[flags & DX_MODE]+" <"+ident+"> <"+notes+'>');
		
		carrier /= 1000;
		dx_list[gid] = { "gid":gid, "carrier":carrier, "lo":obj.lo, "hi":obj.hi, "freq":freq, "moff":moff, "flags":flags, "ident":ident, "notes":notes, "params":params };
      dx.displayed[dx_idx] = dx_list[gid];
		//console.log(dx_list[gid]);
		
		s +=
			'<div id="'+dx_idx+'-id-dx-label" class="class-dx-label '+ gid +'-id-dx-gid'+ ((params != '')? ' id-has-ext':'') +'" ' +
			   'style="left:'+(x-10)+'px; top:'+t+'px; z-index:'+dx_z+'; ' +
				'background-color:'+ color +';" ' +
				
				// overrides underlying canvas listeners for the dx labels
				'onmouseenter="dx_enter(this,'+ cmkr_x +', event)" onmouseleave="dx_leave(this,'+ cmkr_x +')" ' +
				'onmousedown="ignore(event)" onmousemove="ignore(event)" onmouseup="ignore(event)" ontouchmove="ignore(event)" ontouchend="ignore(event)" ' +
				'onclick="dx_click(event,'+ gid +')" ontouchstart="dx_click(event,'+ gid +')" name="'+ ((params == '')? 0:1) +'">' +
			'</div>' +
			'<div class="class-dx-line" id="'+dx_idx+'-id-dx-line" style="left:'+x+'px; top:'+t+'px; height:'+h+'px; z-index:110"></div>';
		//console.log(s);
		
		dx_z++;
	}
	//console.log(dx.displayed);
	
	dx_div.innerHTML = s;

	for (i = 1; i < arr.length; i++) {
		obj = arr[i];
		dx_idx = i-1;
		var ident = obj.i;
		var notes = (typeof obj.n != 'undefined')? obj.n : '';
		//var params = (typeof obj.p != 'undefined')? obj.p : '';
		var el = w3_el(dx_idx +'-id-dx-label');
		
		try {
			el.innerHTML = decodeURIComponent(ident);
		} catch(ex) {
			el.innerHTML = 'bad URI decode';
		}
		
		try {
			el.title = decodeURIComponent(notes);
		} catch(ex) {
			el.title = 'bad URI decode';
		}
		
		// FIXME: merge this with the concept of labels that are TOD sensitive (e.g. SW BCB schedules)	
		if (ident == 'IBP' || ident == 'IARU%2fNCDXF') {
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
						//console.log('IBP '+ min +':'+ sec +' slot='+ slot +' off='+ off +' s='+ s +' '+ dx_ibp[s*2] +' '+ dx_ibp[s*2+1]);
						
						// label may now be out of DOM if we're panning & zooming around
						var el = w3_el(dx_ibp_list[i].idx +'-id-dx-label');
						if (el) el.innerHTML = 'IBP: '+ dx_ibp[s*2] +' '+ dx_ibp[s*2+1];
					}
					//IBP_monitor(slot);
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
   //console.log('dx_filter_panel_close');
   var ae = document.activeElement;
   //console.log(ae);
   if (ae && ae.id && (ae.id.startsWith('id-dx-filter') || ae.id.startsWith('id-dx.filter'))) {
      //console.log('### activeElement='+ ae.id);
      ae.blur();
   }

   confirmation_panel_set_close_func(confirmation_panel_close);   // restore default
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
         var f = Math.round(dl.freq * 1e3);
         //console.log('consider #'+ i +' '+ f);
         if (f > freq_displayed_Hz) break;
      }
      if (i == dx.displayed.length) {
	      wf_send('SET MKR dir=1 freq='+ (freq_displayed_Hz/1e3).toFixed(3));
         return;
      }
   } else {
      for (i = dx.displayed.length - 1; i >= 0 ; i--) {
         dl = dx.displayed[i];
         var f = Math.round(dl.freq * 1e3);
         //console.log('consider #'+ i +' '+ f);
         if (f < freq_displayed_Hz) break;
      }
      if (i < 0) {
	      wf_send('SET MKR dir=-1 freq='+ (freq_displayed_Hz/1e3).toFixed(3));
         return;
      }
   }
   
   // after changing display to this frequency the normal dx_schedule_update() process will
   // acquire a new set of labels
   var mode = modes_l[dl.flags & DX_MODE];
   //console.log('FOUND #'+ i +' '+ f +' '+ dl.ident +' '+ mode +' '+ dl.lo +' '+ dl.hi);
   freqmode_set_dsp_kHz(dl.freq, mode);
   if (dl.lo != 0 && dl.hi != 0) {
      ext_set_passband(dl.lo, dl.hi);     // label has custom pb
   } else {
      var dpb = passbands[mode];
      //console.log(dpb);
      ext_set_passband(dpb.lo, dpb.hi);   // need to force default pb in case cpb is not default
   }
}

var dx_panel_customize = false;
var dx_keys;

// note that an entry can be cloned by selecting it, but then using the "add" button instead of "modify"
function dx_show_edit_panel(ev, gid)
{
	dx_keys = { shift:ev.shiftKey, alt:ev.altKey, ctrl:ev.ctrlKey, meta:ev.metaKey };
	dxo.gid = gid;
	
	if (!dx_panel_customize) {
		html('id-ext-controls').className = 'class-panel class-dx-panel';
		dx_panel_customize = true;
	}
	
	//console.log('dx_show_edit_panel ws='+ ws_wf.stream);
	admin_pwd_query(function() {
      dx_show_edit_panel2();
	});
}

/*
	UI improvements:
		tab between fields
*/

function dx_show_edit_panel2()
{
	var gid = dxo.gid;
	
	if (gid == -1) {
		//console.log('DX EDIT new entry');
		//console.log('DX EDIT new f='+ freq_car_Hz +'/'+ freq_displayed_Hz +' m='+ cur_mode);
		dxo.f = freq_displayed_kHz_str;
		dxo.lo = dxo.hi = dxo.o = 0;
		dxo.m = modes_s[cur_mode];
		dxo.y = types_s.active;
		dxo.i = dxo.n = dxo.p = '';
	} else {
		//console.log('DX EDIT entry #'+ gid +' prev: f='+ dx_list[gid].freq +' flags='+ dx_list[gid].flags.toHex() +' i='+ dx_list[gid].ident +' n='+ dx_list[gid].notes);
		dxo.f = dx_list[gid].carrier.toFixed(2);		// starts as a string, validated to be a number
      dxo.lo = dx_list[gid].lo;
      dxo.hi = dx_list[gid].hi;
		dxo.o = dx_list[gid].moff;
		dxo.m = (dx_list[gid].flags & DX_MODE);
		dxo.y = ((dx_list[gid].flags & DX_TYPE) >> DX_TYPE_SFT);

		try {
			dxo.i = decodeURIComponent(dx_list[gid].ident);
		} catch(ex) {
			dxo.i = 'bad URI decode';
		}
	
		try {
			dxo.n = decodeURIComponent(dx_list[gid].notes);
		} catch(ex) {
			dxo.n = 'bad URI decode';
		}
	
		try {
			dxo.p = decodeURIComponent(dx_list[gid].params);
		} catch(ex) {
			dxo.p = 'bad URI decode';
		}
	
		//console.log('dxo.i='+ dxo.i +' len='+ dxo.i.length);
	}
	//console.log(dxo);
	
	// quick key combo to toggle 'active' mode without bringing up panel
	if (gid != -1 && dx_keys.shift && dx_keys.alt) {
		//console.log('DX COMMIT quick-active entry #'+ dxo.gid +' f='+ dxo.f);
		//console.log(dxo);
		var type = dxo.y;
		type = (type == types_s.active)? types_s.watch_list : types_s.active;
		dxo.y = type;
		var mode = dxo.m;
		mode |= (type << DX_TYPE_SFT);
		wf_send('SET DX_UPD g='+ dxo.gid +' f='+ dxo.f +' lo='+ dxo.lo.toFixed(0) +' hi='+ dxo.hi.toFixed(0) +' o='+ dxo.o.toFixed(0) +' m='+ mode +
			' i='+ encodeURIComponent(dxo.i +'x') +' n='+ encodeURIComponent(dxo.n +'x') +' p='+ encodeURIComponent(dxo.p +'x'));
		return;
	}

	extint_panel_hide();		// committed to displaying edit panel, so remove any ext panel
	
	dxo.f = (parseFloat(dxo.f) + cfg.freq_offset).toFixed(2);
	
	dxo.pb = '';
	if (dxo.lo || dxo.hi) {
      if (dxo.lo == -dxo.hi) {
         dxo.pb = (Math.abs(dxo.hi)*2).toFixed(0);
      } else {
         dxo.pb = dxo.lo.toFixed(0) +', '+ dxo.hi.toFixed(0);
      }
	}

	var s =
		w3_div('w3-medium w3-text-aqua w3-bold', 'DX label edit') +
		w3_divs('w3-text-aqua/w3-margin-T-8',
         w3_inline('w3-hspace-16',
				w3_input('w3-padding-small', 'Freq', 'dxo.f', dxo.f, 'dx_num_cb'),
				w3_select('', 'Mode', '', 'dxo.m', dxo.m, modes_u, 'dx_sel_cb'),
				w3_input('w3-padding-small', 'Passband', 'dxo.pb', dxo.pb, 'dx_passband_cb'),
				w3_select('', 'Type', '', 'dxo.y', dxo.y, types, 'dx_sel_cb'),
				w3_input('w3-padding-small', 'Offset', 'dxo.o', dxo.o, 'dx_num_cb')
			),
		
			w3_input('w3-label-inline/w3-padding-small w3-retain-input-focus', 'Ident', 'dxo.i', '', 'dx_string_cb'),
			w3_input('w3-label-inline/w3-padding-small', 'Notes', 'dxo.n', '', 'dx_string_cb'),
			w3_input('w3-label-inline/w3-padding-small', 'Extension', 'dxo.p', '', 'dx_string_cb'),
		
			w3_inline('w3-hspace-16',
				w3_button('w3-yellow', 'Modify', 'dx_modify_cb'),
				w3_button('w3-green', 'Add', 'dx_add_cb'),
				w3_button('w3-red', 'Delete', 'dx_delete_cb')
			)
		);
	
	// can't do this as initial val passed to w3_input above when string contains quoting
	ext_panel_show(s, null, function() {
		var el = w3_el('dxo.i');
		el.value = dxo.i;
		w3_el('dxo.n').value = dxo.n;
		w3_el('dxo.p').value = dxo.p;
		
		// change focus to input field
		// FIXME: why doesn't field select work?
		//console.log('dxo.i='+ el.value);
		w3_field_select(el, {mobile:1});
	});
	ext_set_controls_width_height(525, 260);
}

function dx_close_edit_panel(id)
{
	w3_radio_unhighlight(id);
	extint_panel_hide();
	
	// NB: Can't simply do a dx_schedule_update() here as there is a race for the server to
	// update the dx list before we can pull it again. Instead, the add/modify/delete ajax
	// response will call dx_update() directly when the server has updated.
}

function dx_modify_cb(id, val)
{
	//console.log('DX COMMIT modify entry #'+ dxo.gid +' f='+ dxo.f);
	//console.log(dxo);
	if (dxo.gid == -1) return;
	var mode = dxo.m;
	var type = dxo.y << DX_TYPE_SFT;
	mode |= type;
	dxo.f -= cfg.freq_offset;
	if (dxo.f < 0) dxo.f = 0;
	wf_send('SET DX_UPD g='+ dxo.gid +' f='+ dxo.f +' lo='+ dxo.lo.toFixed(0) +' hi='+ dxo.hi.toFixed(0) +' o='+ dxo.o.toFixed(0) +' m='+ mode +
		' i='+ encodeURIComponent(dxo.i +'x') +' n='+ encodeURIComponent(dxo.n +'x') +' p='+ encodeURIComponent(dxo.p +'x'));
	setTimeout(function() {dx_close_edit_panel(id);}, 250);
}

function dx_add_cb(id, val)
{
	//console.log('DX COMMIT new entry');
	//console.log(dxo);
	var mode = dxo.m;
	var type = dxo.y << DX_TYPE_SFT;
	mode |= type;
	dxo.f -= cfg.freq_offset;
	if (dxo.f < 0) dxo.f = 0;
	wf_send('SET DX_UPD g=-1 f='+ dxo.f +' lo='+ dxo.lo.toFixed(0) +' hi='+ dxo.hi.toFixed(0) +' o='+ dxo.o.toFixed(0) +' m='+ mode +
		' i='+ encodeURIComponent(dxo.i +'x') +' n='+ encodeURIComponent(dxo.n +'x') +' p='+ encodeURIComponent(dxo.p +'x'));
	setTimeout(function() {dx_close_edit_panel(id);}, 250);
}

function dx_delete_cb(id, val)
{
	//console.log('DX COMMIT delete entry #'+ dxo.gid);
	//console.log(dxo);
	if (dxo.gid == -1) return;
	wf_send('SET DX_UPD g='+ dxo.gid +' f=-1');
	setTimeout(function() {dx_close_edit_panel(id);}, 250);
}

function dx_click(ev, gid)
{
   //event_dump(ev, 'dx_click');
	if (ev.shiftKey) {
		dx_show_edit_panel(ev, gid);
	} else {
	   var freq = dx_list[gid].freq;
		var mode = modes_l[dx_list[gid].flags & DX_MODE];
		var lo = dx_list[gid].lo;
		var hi = dx_list[gid].hi;
		//console.log("DX-click f="+ dx_list[gid].freq +" mode="+ mode +" cur_mode="+ cur_mode +' lo='+ lo +' hi='+ hi);
		freqmode_set_dsp_kHz(freq, mode);
		if (lo || hi) {
		   ext_set_passband(lo, hi, undefined, freq);
		}
		
		// open specified extension
		if (!any_alternate_click_event(ev) && !dx.ctrl_click && dx_list[gid].params) {
		   var p = decodeURIComponent(dx_list[gid].params);
		   //console.log('### dx_click extension <'+ p +'>');
         var ext = p.split(',');
         extint.param = ext.slice(1).join(',');
			extint_open(ext[0], 250);
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
	dx.style.backgroundColor = (w3_contains(dx, 'id-has-ext') && !any_alternate_click_event(ev))? 'magenta':'yellow';

	var dx_line = w3_el(parseInt(dx.id)+'-id-dx-line');
	dx_line.zIndex = 998;
	dx_line.style.width = '3px';
	
	if (cmkr_x) {
		dx_canvas.style.left = (cmkr_x-dx_car_w/2)+'px';
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
var sMeter_dBm_biased = 0;
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

function smeter_init()
{
	w3_innerHTML('id-control-smeter',
		'<canvas id="id-smeter-scale" class="class-smeter-scale" width="0" height="0"></canvas>',
		w3_div('id-smeter-ovfl w3-hide', 'OV'),
		w3_div('id-smeter-dbm-value'),
		w3_div('id-smeter-dbm-units', 'dBm')
	);

	var sMeter_canvas = w3_el('id-smeter-scale');
	smeter_ovfl = w3_el('smeter-ovfl');

	smeter_width = divControl.activeWidth - SMETER_RHS - html_LR_border_pad(sMeter_canvas);   // less our own border/padding
	
	var w = smeter_width, h = SMETER_SCALE_HEIGHT, y=h-8;
	var tw = w + SMETER_RHS;
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
		line_stroke(sMeter_ctx, 1, 3, "white", x,y-8,x,y+8);
		sMeter_ctx.fillText(bars.text[i], x, y-15);
		//console.log("SM x="+x+' dBm='+bars.dBm[i]+' '+bars.text[i]);
	}

	line_stroke(sMeter_ctx, 0, 5, "black", 0,y,w,y);
	setInterval(update_smeter, 100);
}

var sm_px = 0, sm_timeout = 0, sm_interval = 10;
var sm_ovfl_showing = false;

function update_smeter()
{
	var x = smeter_dBm_biased_to_x(sMeter_dBm_biased);
	var y = SMETER_SCALE_HEIGHT-8;
	var w = smeter_width;
	sMeter_ctx.globalAlpha = 1;
	line_stroke(sMeter_ctx, 0, 5, "lime", 0,y,x,y);
	
	if (sm_timeout-- == 0) {
		sm_timeout = sm_interval;
		if (x < sm_px) line_stroke(sMeter_ctx, 0, 5, "black", x,y,sm_px,y);
		//if (x < sm_px) line_stroke(sMeter_ctx, 0, 5, "black", x,y,w,y);
		sm_px = x;
	} else {
		if (x < sm_px) {
			line_stroke(sMeter_ctx, 0, 5, "red", x,y,sm_px,y);
		} else {
			sm_px = x;
			sm_timeout = sm_interval;
		}
	}
	
	if (audio_ext_adc_ovfl && !sm_ovfl_showing) {
	   w3_hide('id-smeter-dbm-units');
	   w3_show('id-smeter-ovfl');
	   sm_ovfl_showing = true;
	} else
	if (!audio_ext_adc_ovfl && sm_ovfl_showing) {
	   w3_hide('id-smeter-ovfl');
	   w3_show('id-smeter-dbm-units');
	   sm_ovfl_showing = false;
	}
	
	w3_innerHTML('id-smeter-dbm-value', (sMeter_dBm_biased - SMETER_BIAS).toFixed(0));
}


////////////////////////////////
// user ident
////////////////////////////////

var ident_tout;
var ident_user = '';
var need_ident = false;

function ident_init()
{
	var ident = initCookie('ident', '');
	ident = kiwi_strip_tags(ident, '');
	//console.log('IINIT ident_user=<'+ ident +'>');
	var el = w3_el('id-ident-input');
	el.value = ident;
	ident_user = ident;
	need_ident = true;
	//console.log('ident_init: SET ident='+ ident_user);
}

function ident_complete()
{
	var el = w3_el('id-ident-input');
	var ident = el.value;
	ident = kiwi_strip_tags(ident, '');
	el.value = ident;
	//console.log('ICMPL el='+ (typeof el) +' ident_user=<'+ ident +'>');
	kiwi_clearTimeout(ident_tout);

	// okay for ident='' to erase it
	// SECURITY: size limited by <input size=...> but guard against binary data injection?
	w3_field_select(el, {mobile:1});
	writeCookie('ident', ident);
	ident_user = ident;
	need_ident = true;
	//console.log('ident_complete: SET ident_user='+ ident_user);
}

function ident_keyup(el, evt)
{
	//event_dump(evt, "IKU");
	kiwi_clearTimeout(ident_tout);
	//console.log("IKU el="+(typeof el)+" val="+el.value);
	
	// Ignore modifier keys used with mouse events that also appear here.
	// Also keeps ident_complete timeout from being set after return key.
	//if (ignore_next_keyup_event) {
	if (evt != undefined && evt.key != undefined) {
		var klen = evt.key.length;
		if (any_alternate_click_event(evt) || klen != 1) {
         //console.log("ignore shift-key ident_keyup");
         //ignore_next_keyup_event = false;
         return;
      }
	}
	
	ident_tout = setTimeout(ident_complete, 5000);
}


////////////////////////////////
// keyboard shortcuts
////////////////////////////////

var shortcut = {
   nav_off: 0,
   keys: '',
};

function keyboard_shortcut_init()
{
   if (kiwi_isMobile() || kiwi_isFirefox() < 47 || kiwi_isChrome() <= 49 || kiwi_isOpera() <= 36) return;
   
   shortcut.help =
      w3_div('',
         w3_inline_percent('w3-padding-tiny w3-bold w3-text-aqua', 'Keys', 25, 'Function'),
         w3_inline_percent('w3-padding-tiny', 'g =', 25, 'select frequency entry field'),
         w3_inline_percent('w3-padding-tiny', 'j i LR-arrow-keys', 25, 'frequency step down/up, add shift or alt/ctrl for faster<br>shift plus alt/ctrl to step to next/prev DX label'),
         w3_inline_percent('w3-padding-tiny', 't T', 25, 'scroll frequency memory list'),
         w3_inline_percent('w3-padding-tiny', 'a l u c f q', 25, 'select mode: AM LSB USB CW NBFM IQ'),
         w3_inline_percent('w3-padding-tiny', 'p P', 25, 'passband narrow/widen'),
         w3_inline_percent('w3-padding-tiny', 'r', 25, 'toggle audio recording'),
         w3_inline_percent('w3-padding-tiny', 'z Z', 25, 'zoom in/out, add alt/ctrl for max in/out'),
         w3_inline_percent('w3-padding-tiny', '< >', 25, 'waterfall page down/up'),
         w3_inline_percent('w3-padding-tiny', 'w W', 25, 'waterfall min dB slider -/+ 1 dB, add alt/ctrl for -/+ 10 dB'),
         w3_inline_percent('w3-padding-tiny', 'S', 25, 'waterfall auto-scale'),
         w3_inline_percent('w3-padding-tiny', 's d', 25, 'spectrum on/off toggle, slow device mode'),
         w3_inline_percent('w3-padding-tiny', 'v V m', 25, 'volume less/more, mute'),
         w3_inline_percent('w3-padding-tiny', 'o', 25, 'toggle between option bar "off" and "stats" mode,<br>others selected by related shortcut key'),
         w3_inline_percent('w3-padding-tiny', '@', 25, 'DX label filter'),
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
   confirmation_show_content(shortcut.help, 550, 465);
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

function keyboard_shortcut(key, mod, ctlAlt)
{
   var action = true;
   var mode = ext_get_mode();
   shortcut.nav_click = false;
   
   switch (key) {
   
   // mode
   case 'a': ext_set_mode((mode == 'am')? 'amn' : ((mode == 'amn')? 'am' : 'am')); break;
   case 'l': ext_set_mode('lsb'); break;
   case 'u': ext_set_mode('usb'); break;
   case 'c': ext_set_mode((mode == 'cw')? 'cwn' : ((mode == 'cwn')? 'cw' : 'cw')); break;
   case 'f': ext_set_mode('nbfm'); break;
   case 'q': ext_set_mode('iq'); break;
   
   // step
   case 'j': case 'J': case 'ArrowLeft':  if (mod < 3) freqstep(2-mod); else dx_label_step(-1); break;
   case 'i': case 'I': case 'ArrowRight': if (mod < 3) freqstep(3+mod); else dx_label_step(+1); break;
   
   // passband
   case 'p': passband_increment(false); break;
   case 'P': passband_increment(true); break;
   
   // volume/mute
   case 'v': setvolume(1, volume-10); toggle_or_set_mute(0); keyboard_shortcut_nav('audio'); break;
   case 'V': setvolume(1, volume+10); toggle_or_set_mute(0); keyboard_shortcut_nav('audio'); break;
   case 'm': toggle_or_set_mute(); shortcut.nav_click = true; break;

   // frequency entry / memory list
   case 'g': case '=': freqset_select(); break;
   case 't': freq_up_down_cb(null, 1); break;
   case 'T': freq_up_down_cb(null, 0); break;

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
   case 'S': wf_autoscale(); keyboard_shortcut_nav('wf'); break;
   case 'd': toggle_or_set_slow_dev(); keyboard_shortcut_nav('wf'); break;

   // misc
   case 'o': keyboard_shortcut_nav(shortcut.nav_off? 'status':'off'); shortcut.nav_off ^= 1; break;
   case 'r': toggle_or_set_rec(); break;
   case 'x': toggle_or_set_hide_panels(); break;
   case 'y': toggle_or_set_hide_topbar(); break;
   case '@': dx_filter(); shortcut.nav_click = true; break;
   case '?': case 'h': keyboard_shortcut_help(); break;

   default: action = false; break;
   
   }
   
   if (action && !shortcut.nav_click) keyboard_shortcut_nav('users');

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

      var sft = evt.shiftKey;
      var ctl = evt.ctrlKey;
      var alt = evt.altKey;
      var meta = evt.metaKey;
      var ctlAlt = (ctl||alt);
      var mod = (sft && !ctlAlt)? 1 : ( (!sft & ctlAlt)? 2 : ( (sft && ctlAlt)? 3:0 ) );

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
         
         keyboard_shortcut(k, mod, ctlAlt);
         
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


////////////////////////////////
// panels
////////////////////////////////

var panel_margin = 10;
var ac_play_button;

function test_audio_suspended()
{
   //console.log('AudioContext.state='+ ac_play_button.state);
   if (ac_play_button.state != "running") {
      var s =
         w3_div('id-play-button-container||onclick="play_button()"',
            w3_div('id-play-button',
               '<img src="gfx/openwebrx-play-button.png" width="150" height="150" /><br><br>' +
               (kiwi_isMobile()? 'Tap to':'Click to') +' start OpenWebRX'
            )
         );
      w3_appendElement('id-main-container', 'div', s);
      el = w3_el('id-play-button');
      el.style.marginTop = px(w3_center_in_window(el));
      //alert('state '+ ac_play_button.state);
   }
}

// called from waterfall_init()
function panels_setup()
{
   var el;
   
	w3_el("id-ident").innerHTML =
		'<form id="id-ident-form" action="#" onsubmit="ident_complete(); return false;">' +
			w3_input('w3-label-not-bold/id-ident-input|padding:1px|size=20 onkeyup="ident_keyup(this, event)"', 'Your name or callsign:') +
		'</form>';
	
	w3_el("id-control-freq1").innerHTML =
	   w3_inline('',
         w3_div('id-freq-cell',
            '<form id="id-freq-form" name="form_freq" action="#" onsubmit="freqset_complete(0); return false;">' +
               w3_input('id-freq-input|padding:0 4px;max-width:74px|size=8 onkeydown="freqset_keydown(event)" onkeyup="freqset_keyup(this, event)"') +
            '</form>'
         ),

         w3_div('|padding:0 0 0 3px',
            w3_icon('w3-show-block w3-text-orange||title="prev"', 'fa-arrow-circle-up', 15, '', 'freq_up_down_cb', 1) +
            w3_icon('w3-show-block w3-text-aqua||title="next"', 'fa-arrow-circle-down', 15, '', 'freq_up_down_cb', 0)
         ),

         w3_div('id-select-band-cell|padding:0 4px',
            '<select id="id-select-band" class="w3-pointer" onchange="select_band(this.value)">' +
               '<option value="0" selected disabled>select band</option>' +
               setup_band_menu() +
            '</select>'
         ),

         w3_div('select-ext-cell|padding:0',
            '<select id="select-ext" class="w3-pointer" onchange="freqset_select(); extint_select(this.value)">' +
               '<option value="-1" selected disabled>extension</option>' +
               extint_select_menu() +
            '</select>'
         )
      );

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
            w3_button('id-button-spectrum class-button', 'Spec', 'toggle_or_set_spec')
         ),
         w3_div('',
            w3_div('fa-stack||title="record"',
               w3_icon('id-rec1', 'fa-circle fa-nudge-down fa-stack-2x w3-text-pink', 22, '', 'toggle_or_set_rec'),
               w3_icon('id-rec2', 'fa-stop fa-stack-1x w3-text-pink w3-hide', 10, '', 'toggle_or_set_rec')
            )
         ),
         w3_div('|width:8%|title="mute"',
            // from https://jsfiddle.net/cherrador/jomgLb2h since fa doesn't have speaker-slash
            w3_div('id-mute-no fa-stack|width:100%;',
               w3_icon('', 'fa-volume-up fa-stack-2x fa-nudge-right-OFF', 24, 'lime', 'toggle_or_set_mute')
            ),
            w3_div('id-mute-yes fa-stack w3-hide|width:100%;color:magenta;',  // hsl(340, 82%, 60%) w3-text-pink but lighter
               w3_icon('', 'fa-volume-off fa-stack-2x fa-nudge-left', 24, '', 'toggle_or_set_mute'),
               w3_icon('', 'fa-times fa-right fa-nudge-left-OFF', 12, '', 'toggle_or_set_mute')
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

	w3_el("id-control-mode").innerHTML =
	   w3_inline('w3-halign-space-between/',
		   w3_div('id-button-am class-button||onclick="mode_button(event, this)" onmousedown="cancelEvent(event)" onmouseover="mode_over(event, this)"', 'AM'),
		   w3_div('id-button-amn class-button||onclick="mode_button(event, this)" onmousedown="cancelEvent(event)" onmouseover="mode_over(event, this)"', 'AMN'),
		   w3_div('id-button-lsb class-button||onclick="mode_button(event, this)" onmousedown="cancelEvent(event)" onmouseover="mode_over(event, this)"', 'LSB'),
		   w3_div('id-button-usb class-button||onclick="mode_button(event, this)" onmousedown="cancelEvent(event)" onmouseover="mode_over(event, this)"', 'USB'),
		   w3_div('id-button-cw class-button||onclick="mode_button(event, this)" onmousedown="cancelEvent(event)" onmouseover="mode_over(event, this)"', 'CW'),
		   w3_div('id-button-cwn class-button||onclick="mode_button(event, this)" onmousedown="cancelEvent(event)" onmouseover="mode_over(event, this)"', 'CWN'),
		   w3_div('id-button-nbfm class-button||onclick="mode_button(event, this)" onmousedown="cancelEvent(event)" onmouseover="mode_over(event, this)"', 'NBFM'),
		   w3_div('id-button-iq class-button||onclick="mode_button(event, this)" onmousedown="cancelEvent(event)" onmouseover="mode_over(event, this)"', 'IQ')
		);

   var d = audioFFT_active? ' w3-disabled' : '';
   var d2 = audioFFT_active? ' w3-disabled||onclick="audioFFT_update()"' : '';

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
      'w3-red',
      'w3-khaki',
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


   // wf
	spec_filter = readCookie('last_spec_filter');
	if (spec_filter == null) spec_filter = 0;
	
   if (typeof(spectrum_show) == 'string') {
      var ss = spectrum_show.toLowerCase();
      spec_filter_s.forEach(function(e, i) { if (ss == e.toLowerCase()) spec_filter = i; });
   }
   
	w3_el("id-optbar-wf").innerHTML =
      w3_col_percent('',
         w3_div('',
            w3_col_percent('w3-valign/class-slider',
               w3_text(optbar_prefix_color, 'WF max'), 19,
               '<input id="id-input-maxdb" type="range" min="-100" max="20" value="'+ maxdb
                  +'" step="1" onchange="setmaxdb(1,this.value)" oninput="setmaxdb(0, this.value)">', 60,
               w3_div('id-field-maxdb class-slider'), 19
            ),
            w3_col_percent('w3-valign/class-slider',
               w3_text(optbar_prefix_color, 'WF min'), 19,
               '<input id="id-input-mindb" type="range" min="-190" max="-30" value="'+ mindb
                  +'" step="1" onchange="setmindb(1,this.value)" oninput="setmindb(0, this.value)">', 60,
               w3_div('id-field-mindb class-slider'), 19
            ),
            w3_col_percent('w3-valign/class-slider',
               w3_text(optbar_prefix_color, 'WF rate'), 19,
               '<input id="slider-rate" class="'+ d +'" type="range" min="0" max="4" value="'+
                  wf_speed +'" step="1" onchange="setwfspeed(1,this.value)" oninput="setwfspeed(0,this.value)">', 60,
               w3_div('slider-rate-field class-slider'), 19
            ),

            w3_col_percent('w3-valign/class-slider',
               w3_text(optbar_prefix_color, 'SP parm'), 19,
               w3_slider('slider-spparam', '', 'sp_param', sp_param, 0, 10, 1, 'spec_filter_param_cb'), 60,
               w3_div('slider-spparam-field class-slider'), 19
            ),

            w3_inline('w3-halign-space-around/',
               w3_select('|color:red', '', 'colormap', 'wf.cmap', wf.cmap, wf_cmap_s, 'wf_cmap_cb'),
               //w3_select('|color:red', '', 'contrast', 'wf.contr', W3_SELECT_SHOW_TITLE, wf_contr_s, 'wf_contr_cb'),
               w3_select('|color:red', '', 'spec filter', 'spec_filter', spec_filter, spec_filter_s, 'spec_filter_cb'),
               w3_div('w3-hcenter', w3_button('id-button-spec-peak class-button', 'Peak', 'toggle_or_set_spec_peak'))
            )
         ), 85,
         w3_div('',
            w3_div('w3-hcenter w3-margin-T-2 w3-margin-B-10', w3_button('id-button-wf-autoscale class-button', 'Auto<br>Scale', 'wf_autoscale')),
            w3_div('w3-hcenter w3-margin-B-10', w3_button('id-button-slow-dev class-button', 'Slow<br>Dev', 'toggle_or_set_slow_dev')),
            w3_div('w3-hcenter', w3_button('id-button-wf-gnd class-button w3-momentary', 'GND', 'wf_gnd_cb', 1))
         ), 15
      );
      
   setwfspeed(1, wf_speed);
   toggle_or_set_slow_dev(toggle_e.FROM_COOKIE | toggle_e.SET, 0);
   toggle_or_set_spec_peak(toggle_e.FROM_COOKIE | toggle_e.SET_URL, peak_initially);


   // audio & nb
	de_emphasis = readCookie('last_de_emphasis');
	if (de_emphasis == null) de_emphasis = 0;

	pan = readCookie('last_pan');
	if (pan == null) pan = 0;

	w3_el('id-optbar-audio').innerHTML =
		w3_col_percent('w3-valign/class-slider',
			w3_div('w3-show-inline-block', w3_text(optbar_prefix_color, 'Noise gate')), 20,
			'<input id="input-noise-gate" type="range" min="100" max="5000" value="'+ noiseGate +'" step="100" onchange="setNoiseGate(1,this.value)" oninput="setNoiseGate(0,this.value)">', 50,
			w3_div('field-noise-gate w3-show-inline-block', noiseGate.toString()) +' uS', 15,
			w3_div('w3-hcenter', w3_div('id-button-nb class-button||title="noise blanker"; onclick="toggle_or_set_nb();"', 'NB')), 15
		) +
		w3_col_percent('w3-valign/class-slider',
			w3_div('w3-show-inline-block', w3_text(optbar_prefix_color +' cl-closer-spaced-label-text', 'Noise<br>threshold')), 20,
			'<input id="input-noise-th" type="range" min="0" max="100" value="'+ noiseThresh +'" step="1" onchange="setNoiseThresh(1,this.value)" oninput="setNoiseThresh(0,this.value)">', 50,
			w3_div('field-noise-th w3-show-inline-block', noiseThresh.toString()) +'%', 15,
			//w3_div('w3-hcenter', w3_div('id-button-nb-test class-button||onclick="toggle_nb_test();"', 'Test')), 15
		   w3_button('id-button-compression class-button w3-hcenter||title="compression"', 'Comp', 'toggle_or_set_compression'), 15
		) +
		w3_col_percent('w3-valign w3-margin-TB-4/',
			w3_div('w3-show-inline-block', w3_text(optbar_prefix_color, 'LMS filter')), 25,
			w3_div('w3-valign',
            w3_checkbox('w3-label-inline w3-label-not-bold', 'Denoiser', 'lms.denoise', false, 'lms_denoise_load_cb')
         ), 30,
			w3_div('w3-valign',
            w3_checkbox('w3-label-inline w3-label-not-bold', 'Autonotch', 'lms.autonotch', false, 'lms_autonotch_load_cb')
         ), 30,
			w3_div('w3-hcenter', w3_div('id-button-lms-ext class-button||onclick="extint_open(\'LMS\'); freqset_select();"', 'More')), 15
		) +
		w3_col_percent('w3-valign/class-slider',
			w3_text(optbar_prefix_color, 'Volume'), 20,
			'<input id="id-input-volume" type="range" min="0" max="200" value="'+ volume +'" step="1" onchange="setvolume(1, this.value)" oninput="setvolume(0, this.value)">', 35,
         w3_select('|color:red', '', 'de-emp', 'de_emphasis', de_emphasis, de_emphasis_s, 'de_emp_cb'), 30,
			//w3_div('w3-hcenter', w3_div('id-button-pref class-button|visibility:hidden|onclick="show_pref();"', 'Pref')), 15,
			w3_div('w3-hcenter', w3_div('id-button-mute class-button||onclick="toggle_or_set_mute();"', 'Mute')), 15
			//w3_div('w3-hcenter', w3_div('id-button-test class-button||onclick="toggle_or_set_test();"', 'Test')), 15
		) +
      w3_col_percent('id-squelch w3-valign w3-hide/class-slider',
         //'<span id="id-squelch-label">Squelch </span>', 18,
			w3_text('id-squelch-label', 'Squelch'), 20,
         '<input id="id-squelch-value" type="range" min="0" max="99" value="'+ squelch +'" step="1" onchange="setsquelch(1,this.value)" oninput="setsquelch(1, this.value)">', 50,
         w3_div('id-squelch-field class-slider'), 30
	   ) +
      w3_col_percent('id-pan w3-valign w3-hide/class-slider',
         w3_text(optbar_prefix_color, 'Pan'), 20,
         '<input id="id-pan-value" type="range" min="-1" max="1" value="'+ pan +'" step="0.01" onchange="setpan(1,this.value)" oninput="setpan(0, this.value)">', 50,
         w3_div('id-pan-field'), 15
      );

   w3_color('id-button-mute', muted? 'lime':'white');
   toggle_or_set_test(0);
   //toggle_or_set_audio(toggle_e.FROM_COOKIE | toggle_e.SET, 1);
   toggle_or_set_compression(toggle_e.FROM_COOKIE | toggle_e.SET, 1);
	nb_setup(toggle_e.FROM_COOKIE | toggle_e.SET);
	toggle_or_set_nb(toggle_e.FROM_COOKIE | toggle_e.SET, 0);
	squelch_setup(toggle_e.FROM_COOKIE);

   audio_panner_ui_init();


   // agc
	w3_el('id-optbar-agc').innerHTML =
		w3_col_percent('w3-valign/class-slider',
			'<div id="id-button-agc" class="class-button" onclick="toggle_agc(event)" onmousedown="cancelEvent(event)" onmouseover="agc_over(event)">AGC</div>', 13,
			'<div id="id-button-hang" class="class-button" onclick="toggle_or_set_hang();">Hang</div>', 17,
			w3_divs('w3-show-inline-block/id-label-man-gain cl-closer-spaced-label-text', 'Manual<br>gain'), 15,
			'<input id="input-man-gain" type="range" min="0" max="120" value="'+ manGain +'" step="1" onchange="setManGain(1,this.value)" oninput="setManGain(0,this.value)">', 40,
			w3_div('field-man-gain w3-show-inline-block', manGain.toString()) +' dB', 15
		) +
		w3_div('',
			w3_col_percent('w3-valign/class-slider',
				w3_div('label-threshold w3-show-inline-block', 'Threshold'), 18,
				'<input id="input-threshold" type="range" min="-130" max="0" value="'+ thresh +'" step="1" onchange="setThresh(1,this.value)" oninput="setThresh(0,this.value)">', 52,
				w3_div('field-threshold w3-show-inline-block', thresh.toString()) +' dB', 30
			),
			w3_col_percent('w3-valign/class-slider',
				w3_div('label-slope w3-show-inline-block', 'Slope'), 18,
				'<input id="input-slope" type="range" min="0" max="10" value="'+ slope +'" step="1" onchange="setSlope(1,this.value)" oninput="setSlope(0,this.value)">', 52,
				w3_div('field-slope w3-show-inline-block', slope.toString()) +' dB', 30
			),
			w3_col_percent('w3-valign/class-slider',
				w3_div('label-decay w3-show-inline-block', 'Decay'), 18,
				'<input id="input-decay" type="range" min="20" max="5000" value="'+ decay +'" step="1" onchange="setDecay(1,this.value)" oninput="setDecay(0,this.value)">', 52,
				w3_div('field-decay w3-show-inline-block', decay.toString()) +' mS', 30
			)
		);
	setup_agc(toggle_e.FROM_COOKIE | toggle_e.SET);

	
	// users
	
	
	// status
	
	
	// optbar_setup
	//console.log('optbar_setup');
   w3_click_nav(kiwi_toggle(toggle_e.FROM_COOKIE | toggle_e.SET, 'optbar-wf', 'optbar-wf', 'last_optbar'), 'optbar', 'init');
	

	//jksx XDLS pref button
	//if (dbgUs) w3_el('id-button-pref').style.visibility = 'visible';
	//if (dbgUs) toggle_or_set_pref(toggle_e.SET, 1);

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
		'&nbsp;&nbsp;&nbsp;Project website: <a href="http://kiwisdr.com" target="_blank">kiwisdr.com</a>&nbsp;&nbsp;&nbsp;&nbsp;Here are some tips: \
		<ul style="padding-left: 12px;"> \
		<li> Please <a href="javascript:sendmail(\'pvsslqwChjtjpgq-`ln\');">email us</a> \
			if your browser is having problems with the SDR. </li>\
		<li> Windows: Firefox, Chrome & Edge work; IE does not work. </li>\
		<li> Mac & Linux: Safari, Firefox, Chrome & Opera should work fine. </li>\
		<li> Open and close the panels by using the circled arrows at the top right corner. </li>\
		<li> You can click and/or drag almost anywhere on the page to change settings. </li>\
		<li> Enter a numeric frequency in the box marked "kHz" at right. </li>\
		<li> Or use the "select band" menu to jump to a pre-defined band. </li>\
		<li> Use the zoom icons to control the waterfall span. </li>\
		<li> Tune by clicking on the waterfall, spectrum or the cyan/red-colored station labels. </li>\
		<li> Ctrl-shift or alt-shift click in the waterfall to lookup frequency in online databases. </li>\
		<li> Control or option/alt click to page spectrum down and up in frequency. </li>\
		<li> Adjust the "WF min" slider for best waterfall colors. </li>\
		<li> See the <a href="http://www.kiwisdr.com/quickstart/" target="_blank">Operating information</a> page\
		     and <a href="https://www.dropbox.com/s/i1bjyp1acghnc16/KiwiSDR.design.review.pdf?dl=1" target="_blank">Design review document</a>. </li>\
		</ul> \
		';

		//<li> Noise across the band due to combination of active antenna and noisy location. </li>\
		//<li> Noise floor is high due to the construction method of the prototype ... </li>\
		//<li> ... so if you don\'t hear much on shortwave try \
		//	<a href="javascript:ext_tune(346,\'am\');">LF</a>, <a href="javascript:ext_tune(15.25,\'lsb\');">VLF</a> or \
		//	<a href="javascript:ext_tune(1107,\'am\');">MW</a>. </li>\

		//<li> You must use a modern browser that supports HTML5. Partially working on iPad. </li>\
		//<li> Acknowledging the pioneering web-based SDR work of Pieter-Tjerk de Bohr, PA3FWM author of \
		//	<a href="http://websdr.ewi.utwente.nl:8901/" target="_blank">WebSDR</a>, and \
		//	Andrew Holme\'s <a href="http://www.aholme.co.uk/GPS/Main.htm" target="_blank"> homemade GPS receiver</a>. </li> \


	// id-msgs
	
	var contact_admin = ext_get_cfg_param('contact_admin');
	if (typeof contact_admin == 'undefined') {
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
            //'<a href="javascript:sendmail(\'pvsslqwChjtjpgq-`ln\');">KiwiSDR</a> ' +
            '<a href="http://kiwisdr.com" target="_blank">KiwiSDR</a> ' +
            '| <a href="http://valentfx.com/vanilla/discussions" target="_blank">Forum</a> ' +
            '| <a href="https://kiwiirc.com/client/chat.freenode.net/#kiwisdr" target="_blank">Chat</a> '
         )
		) +
		w3_div('id-status-adc') +
		w3_div('id-status-config') +
		w3_div('id-status-gps') +
		w3_div('',
		   w3_div('id-status-audio w3-show-inline'), ' ',
		   w3_div('id-status-problems w3-show-inline')
		) +
		w3_div('id-status-stats-cpu') +
		w3_div('id-status-stats-xfer');

	setTimeout(function() { setInterval(status_periodic, 5000); }, 1000);

   /*
	w3_el_softfail("id-msgs-inner").innerHTML =
		w3_div('w3-margin-B-5',
		   '<strong> Contacts: </strong>' +
		   (admin_email? '<a href="javascript:sendmail(\''+ admin_email +'\');"><strong>Owner/Admin</strong></a>, ' : '') +
		   '<a href="javascript:sendmail(\'pvsslqwChjtjpgq-`ln\');"><strong>KiwiSDR</strong></a>, ' +
		   '<a href="javascript:sendmail(\'kb4jonCpgq-kv\');"><strong>OpenWebRX</strong></a> ' +
		   '| <a href="https://kiwiirc.com/client/chat.freenode.net/#kiwisdr" target="_blank"><strong>Chat</strong></a> ' +
		   w3_div('id-problems w3-show-inline')
		) +
		w3_div('id-msg-status') +
		w3_div('id-msg-config') +
		w3_div('id-msg-gps') +
		w3_div('id-msg-audio') +
		w3_div('id-debugdiv');
	*/
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
      if (cb_arg != 'init' && prev_optbar == 'optbar-off') toggle_or_set_hide_topbar();
      w3_hide('id-optbar-content');
      h = px(CONTROL_HEIGHT);
   } else {
      w3_show_block('id-optbar-content');
      h = px(CONTROL_HEIGHT + OPTBAR_HEIGHT + OPTBAR_TMARGIN);
   }
   w3_el('id-control').style.height = h;
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
   contr: 0,
   cmap: 0,
};

var wf_contr_s = { 0:'normal', 1:'scheme 1', 2:'scheme 2', 3:'scheme 3', 4:'scheme 4' };

function wf_contr_cb(path, idx, first)
{
   console.log('wf_contr_cb idx='+ idx +' first='+ first);
}

var wf_gnd = 0, wf_gnd_value_base = 150, wf_gnd_value = wf_gnd_value_base;

function wf_gnd_cb(path, idx, first)
{
   //console.log('wf_gnd_cb idx='+ idx +' first='+ first);
   wf_gnd = !idx;
   if (idx == 0) freqset_select();
}


var sp_param = 0;
var spec_filter = 0;
var spec_filter_s = [ 'IIR', 'MMA', 'EMA', 'off' ];
var spec_filter_e = { IIR:0, MMA:1, EMA:2, OFF:3 };
var spec_filter_p = {
   IIR_min:0, IIR_max:1, IIR_step:0.1, IIR_def:0.2, IIR_val:undefined,
   MMA_min:1, MMA_max:64, MMA_step:1, MMA_def:8, MMA_val:undefined,
   EMA_min:1, EMA_max:64, EMA_step:1, EMA_def:8, EMA_val:undefined,
   off_min:0, off_max:0, off_step:0, off_def:0, off_val:undefined
};

function spec_show()
{
   toggle_or_set_spec(toggle_e.SET, 1);
   
   var sp = -1;
   spectrum_param = parseFloat(spectrum_param);
   if (!isNaN(spectrum_param) && spectrum_param != -1) sp = spectrum_param;
   
   var f = spec_filter_s[spec_filter];
   if (sp >= spec_filter_p[f+'_min'] && sp <= spec_filter_p[f+'_max']) {
      console.log('spec_show sp='+ sp);
      spec_filter_param_cb('', sp, /* done */ true, /* first */ false);
   }
}

function spec_filter_cb(path, idx, first)
{
   idx = +idx;
   //console.log('spec_filter_cb idx='+ idx +' first='+ first);
   spec_filter = idx;
   var f = spec_filter_s[spec_filter];
   var val = spec_filter_p[f+'_val'];
   if (val == undefined) {
      val = spec_filter_p[f+'_def'];
      var lsf = parseFloat(readCookie('last_spec_filter'));
      var lsfp = parseFloat(readCookie('last_spec_filter_param'));
      if (lsf == spec_filter && !isNaN(lsfp)) val = lsfp;
   }
   //console.log('spec_filter_cb val='+ val);
	writeCookie('last_spec_filter', spec_filter.toString());
   
   w3_slider_setup('slider-spparam', spec_filter_p[f+'_min'], spec_filter_p[f+'_max'], spec_filter_p[f+'_step'], val);
   spec_filter_param_cb('', val, /* done */ true, /* first */ false);

	need_clear_specavg = true;
   freqset_select();
}

function spectrum_filter(filter)
{
   spec_filter_cb('', filter, false);
}

function spec_filter_param_cb(path, val, done, first)
{
   if (first) return;
	sp_param = +val;
	//console.log('spec_filter_param_cb '+ sp_param +' spec_filter='+ spec_filter +' done='+ done +' first='+ first);
	spec_filter_p[spec_filter_s[spec_filter] +'_val'] = sp_param;
	w3_set_value('slider-spparam', sp_param)
   w3_el('slider-spparam-field').innerHTML = sp_param;
   if (done) {
	   //console.log('spec_filter_param_cb sp_param='+ sp_param +' spec_filter='+ spec_filter +' done='+ done +' first='+ first);
	   writeCookie('last_spec_filter_param', sp_param.toFixed(2));
      freqset_select();
   }
}


var wf_cmap_s = [ 'default', 'CuteSDR', 'greyscale', 'user 1' ];

function wf_cmap_cb(path, idx, first)
{
   if (first) return;
   var idx_map = [ 1, 0, 2, 3 ];
   idx = +idx;
   if (idx < 0 || idx >= idx_map.length) idx = 0;
   colormap_select = idx_map[idx];
   console.log('wf_cmap_cb idx='+ idx +' first='+ first +' colormap_select='+ colormap_select);
   mkcolormap();
   spectrum_dB_bands();
   freqset_select();
}

function setwfspeed(done, str)
{
   if (audioFFT_active) {
      freqset_select();
      return;
   }
   
	wf_speed = +str;
	//console.log('setwfspeed '+ wf_speed +' done='+ done);
	w3_set_value('slider-rate', wf_speed)
   w3_el('slider-rate-field').innerHTML = wf_speeds[wf_speed];
   w3_el('slider-rate-field').style.color = wf_speed? 'white':'orange';
   wf_send('SET wf_speed='+ wf_speed.toFixed(0));
   if (done) freqset_select();
}

function setmaxdb(done, str)
{
	var strdb = parseFloat(str);
	if (strdb <= mindb) {
		maxdb = mindb + 1;
		html('id-input-maxdb').value = maxdb;
		html('id-field-maxdb').innerHTML = maxdb.toFixed(0) + ' dB';
		html('id-field-maxdb').style.color = "red"; 
	} else {
		maxdb = strdb;
		html('id-field-maxdb').innerHTML = strdb.toFixed(0) + ' dB';
		html('id-field-maxdb').style.color = "white"; 
		html('id-field-mindb').style.color = "white";
	}
	
	setmaxmindb(done);
}

function incr_mindb(done, incr)
{
   var incrdb = (+mindb) + incr;
   var val = Math.max(-190, Math.min(-30, incrdb));
   //console.log('incr_mindb mindb='+ mindb +' incrdb='+ incrdb +' val='+ val);
   setmindb(done, val.toFixed(0));
}

function setmindb(done, str)
{
	var strdb = parseFloat(str);
   //console.log('setmindb strdb='+ strdb +' maxdb='+ maxdb +' mindb='+ mindb +' done='+ done);
	if (maxdb <= strdb) {
		mindb = maxdb - 1;
		html('id-input-mindb').value = mindb;
		html('id-field-mindb').innerHTML = mindb.toFixed(0) + ' dB';
		html('id-field-mindb').style.color = "red";
	} else {
		mindb = strdb;
      //console.log('setmindb SET strdb='+ strdb +' maxdb='+ maxdb +' mindb='+ mindb +' done='+ done);
		html('id-input-mindb').value = mindb;
		html('id-field-mindb').innerHTML = strdb.toFixed(0) + ' dB';
		html('id-field-mindb').style.color = "white";
		html('id-field-maxdb').style.color = "white";
	}

	mindb_un = mindb + zoomCorrection();
	setmaxmindb(done);
}

function setmaxmindb(done)
{
	full_scale = maxdb - mindb;
	full_scale = full_scale? full_scale : 1;	// can't be zero
	spectrum_dB_bands();
   wf_send("SET maxdb="+maxdb.toFixed(0)+" mindb="+mindb.toFixed(0));
	need_clear_specavg = true;
   if (done) {
   	freqset_select();
   	writeCookie(audioFFT_active? 'last_AF_max_dB' : 'last_max_dB', maxdb.toFixed(0));
   	writeCookie(audioFFT_active? 'last_AF_min_dB' : 'last_min_dB', mindb_un.toFixed(0));	// need to save the uncorrected (z0) value
	}
}

function update_maxmindb_sliders()
{
	mindb = mindb_un - zoomCorrection();
	full_scale = maxdb - mindb;
	full_scale = full_scale? full_scale : 1;	// can't be zero
	spectrum_dB_bands();
	
   html('id-input-maxdb').value = maxdb;
   html('id-field-maxdb').innerHTML = maxdb.toFixed(0) + ' dB';
	
   html('id-input-mindb').value = mindb;
   html('id-field-mindb').innerHTML = mindb.toFixed(0) + ' dB';
}

var need_wf_autoscale = false;

function wf_autoscale()
{
   //console.log('wf_autoscale');
	//var el = w3_el('id-button-wf-autoscale');
	//if (el) el.style.color = muted? 'lime':'white';
   freqset_select();
   need_wf_autoscale = true;
}

var spectrum_slow_dev = 0;

function toggle_or_set_slow_dev(set, val)
{
	if (typeof set == 'number')
		spectrum_slow_dev = kiwi_toggle(set, val, spectrum_slow_dev, 'last_slow_dev');
	else
		spectrum_slow_dev ^= 1;
	w3_color('id-button-slow-dev', spectrum_slow_dev? 'lime':'white');
	freqset_select();
	writeCookie('last_slow_dev', spectrum_slow_dev.toString());
	if (spectrum_slow_dev && wf_speed == WF_SPEED_FAST)
	   setwfspeed(1, WF_SPEED_MED);
}

var spectrum_peak = 0;
var spectrum_peak_clear = false;

function toggle_or_set_spec_peak(set, val)
{
	if (typeof set == 'number')
		spectrum_peak = kiwi_toggle(set, val, spectrum_peak, 'last_spec_peak');
	else
		spectrum_peak ^= 1;
	if (!spectrum_peak) spectrum_peak_clear = true;
	w3_color('id-button-spec-peak', spectrum_peak? 'lime':'white');
	freqset_select();
	writeCookie('last_spec_peak', spectrum_peak.toString());
}


////////////////////////////////
// audio
////////////////////////////////

var muted_until_freq_set = true;
var muted = false;
var volume = 50;
var f_volume = 0;
var recording = false;

function setvolume(done, str)
{
   volume = +str;
   volume = Math.max(0, Math.min(200, volume));
   f_volume = muted? 0 : volume/100;
   if (done) {
      w3_set_value('id-input-volume', volume);
      freqset_select();
   }
}

function toggle_or_set_mute(set)
{
	if (typeof set == 'number')
      muted = set;
   else
	   muted ^= 1;
   //console.log('toggle_or_set_mute set='+ set +' muted='+ muted);
   w3_show_hide('id-mute-no', !muted);
   w3_show_hide('id-mute-yes', muted);
   w3_color('id-button-mute', muted? 'lime':'white');
   f_volume = muted? 0 : volume/100;
   freqset_select();
}

var de_emphasis = 0;
var de_emphasis_s = [ 'off', '75us', '50us' ];

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
   if (pan > -0.1 && pan < +0.1) pan = 0;
   w3_set_value('id-pan-value', pan);
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
      w3_show_block('id-pan');
      setpan(1, pan);
   }
}

var hide_topbar = 0;
function toggle_or_set_hide_topbar(set)
{
	if (typeof set == 'number')
      hide_topbar = set;
   else {
	   hide_topbar = (hide_topbar + 1) & 3;
	   
	   // there is no top container to hide if data container or spectrum in use
      if ((hide_topbar & 1) && (extint.using_data_container || spectrum_display))
	      hide_topbar = (hide_topbar + 1) & 3;
	}
   //console.log('toggle_or_set_hide_topbar set='+ set +' hide_topbar='+ hide_topbar);
   w3_set_props('id-top-container', 'w3-panel-override-hide', hide_topbar & 1);
   w3_set_props('id-band-container', 'w3-panel-override-hide', hide_topbar & 2);
   openwebrx_resize();
}

var hide_panels = 0;
function toggle_or_set_hide_panels(set)
{
	if (typeof set == 'number')
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
	if (typeof set == 'number')
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
   recording = !recording;
   //console.log('toggle_or_set_rec set=' + set + ' recording=' + recording);
   var el1 = w3_el('id-rec1');
   var el2 = w3_el('id-rec2');
   if (recording) {
      w3_remove_then_add(el1, 'fa-circle', 'fa-circle-o-notch fa-spin');
      w3_remove(el2, 'w3-hide');
   } else {
      w3_remove_then_add(el1, 'fa-circle-o-notch fa-spin', 'fa-circle');
      w3_add(el2, 'w3-hide');
   }
   if (recording) {
      // Start recording. This is a 'window' property, so audio_recv(), where the
      // recording hooks are, can access it.
      window.recording_meta = {
         buffers: [],         // An array of ArrayBuffers with audio samples to concatenate
         data: null,          // DataView for the current buffer
         offset: 0,           // Current offset within the current ArrayBuffer
         total_size: 0,       // Total size of all recorded data in bytes
         filename: window.location.hostname + '_' + new Date().toISOString().replace(/:/g, '_').replace(/\.[0-9]+Z$/, 'Z') + '_' + w3_el('id-freq-input').value + '_' + cur_mode + '.wav'
      };
      window.recording_meta.buffers.push(new ArrayBuffer(65536));
      window.recording_meta.data = new DataView(window.recording_meta.buffers[0]);
   } else {
      // Stop recording. Build a WAV file.
      var wav_header = new ArrayBuffer(44);
      var wav_data = new DataView(wav_header);
      wav_data.setUint32(0, 0x52494646);                                  // ASCII "RIFF"
      wav_data.setUint32(4, window.recording_meta.total_size + 36, true); // Little-endian size of the remainder of the file, excluding this field
      wav_data.setUint32(8, 0x57415645);                                  // ASCII "WAVE"
      wav_data.setUint32(12, 0x666d7420);                                 // ASCII "fmt "
      wav_data.setUint32(16, 16, true);                                   // Length of this section ("fmt ") in bytes
      wav_data.setUint16(20, 1, true);                                    // PCM coding
      var nch = (cur_mode === 'iq')? 2 : 1;                               // Two channels for IQ mode, one channel otherwise
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
var squelch_state = 0;
var squelch = 0;

function squelch_setup(flags)
{
   if (flags & toggle_e.FROM_COOKIE) { 
      var sq = readCookie('last_squelch');
      if (sq != null) {
         squelch = sq;
         w3_set_value('id-squelch-value', sq);
         setsquelch(1, sq);
      }
   }
	if (cur_mode == 'nbfm') {
		w3_el('id-squelch-label').style.color = squelch_state? 'lime':'white';
		w3_el('id-squelch-field').innerHTML = squelch;
		w3_show_block('id-squelch');
   } else {
		w3_hide('id-squelch');
   }
}

function setsquelch(done, str)
{
   squelch = parseFloat(str);
	w3_el('id-squelch-field').innerHTML = str;
   if (done) {
      snd_send("SET squelch="+ squelch.toFixed(0) +' max='+ squelch_threshold.toFixed(0));
      writeCookie('last_squelch', str);
   }
}

// less buffering and compression buttons
var btn_less_buffering = 0;

function toggle_or_set_audio(set, val)
{
	if (typeof set == 'number')
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
	if (typeof set != 'number') {
	   // haven't got audio_init() calls to work yet (other than initial)
	   //audio_init(null, btn_less_buffering, btn_compression);
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

	if (typeof set == 'number')
		btn_compression = kiwi_toggle(set, val, btn_compression, 'last_compression');
	else
		btn_compression ^= 1;

   var el = w3_el('id-button-compression');
   if (el) {
      el.style.color = btn_compression? 'lime':'white';
      el.style.visibility = 'visible';
      freqset_select();
   }
	writeCookie('last_compression', btn_compression.toString());
	//console.log('SET compression='+ btn_compression.toFixed(0));
	snd_send('SET compression='+ btn_compression.toFixed(0));
}

// NB
var btn_nb = 0;

function toggle_or_set_nb(set, val)
{
	if (set != undefined)
		btn_nb = kiwi_toggle(set, val, btn_nb, 'last_nb');
	else
		btn_nb ^= 1;

   var el = w3_el('id-button-nb');
	el.style.color = btn_nb? 'lime':'white';
	el.style.visibility = 'visible';
	freqset_select();
	writeCookie('last_nb', btn_nb.toString());
	nb_send(btn_nb? noiseGate : 0, noiseThresh);
}

function nb_send(gate, thresh)
{
	snd_send('SET nb='+ gate +' th='+ thresh);
	wf_send('SET nb='+ gate +' th='+ thresh);
	//console.log('NB gate='+ gate +' thresh='+ thresh);
}

var default_noiseGate = 100;
var noiseGate = default_noiseGate;

function setNoiseGate(done, str)
{
   noiseGate = parseFloat(str);
   html('input-noise-gate').value = noiseGate;
   html('field-noise-gate').innerHTML = str;
	nb_send(btn_nb? noiseGate : 0, noiseThresh);
	writeCookie('last_noiseGate', noiseGate.toString());
   if (done) freqset_select();
}

var default_noiseThresh = 50;
var noiseThresh = default_noiseThresh;

function setNoiseThresh(done, str)
{
   noiseThresh = parseFloat(str);
   html('input-noise-th').value = noiseThresh;
   html('field-noise-th').innerHTML = str;
	nb_send(btn_nb? noiseGate : 0, noiseThresh);
	writeCookie('last_noiseThresh', noiseThresh.toString());
   if (done) freqset_select();
}

function nb_setup(toggle_flags)
{
	noiseGate = kiwi_toggle(toggle_flags, default_noiseGate, noiseGate, 'last_noiseGate'); setNoiseGate(true, noiseGate);
	noiseThresh = kiwi_toggle(toggle_flags, default_noiseThresh, noiseThresh, 'last_noiseThresh'); setNoiseThresh(true, noiseThresh);
}

var nb_test = 0;

function toggle_nb_test()
{
   nb_test ^= 1;
   var el = w3_el('id-button-nb-test');
	el.style.color = nb_test? 'lime':'white';
	freqset_select();
	nb_send(nb_test? -1 : -2, noiseThresh);
}


// LMS filter

function lms_denoise_load_cb(path, checked, first)
{
   if (first) return;
   kiwi_load_js(['extensions/LMS_filter/LMS_filter.js'],
      function() {
         lms_denoise_cb(path, checked, first);
      }
   );
}

function lms_autonotch_load_cb(path, checked, first)
{
   if (first) return;
   kiwi_load_js(['extensions/LMS_filter/LMS_filter.js'],
      function() {
         lms_autonotch_cb(path, checked, first);
      }
   );
}


////////////////////////////////
// AGC
////////////////////////////////

function set_agc()
{
	snd_send('SET agc='+ agc +' hang='+ hang +' thresh='+ thresh +' slope='+ slope +' decay='+ decay +' manGain='+ manGain);
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
var default_thresh = -130;
var default_slope = 6;
var default_decay = 1000;

function setup_agc(toggle_flags)
{
	agc = kiwi_toggle(toggle_flags, default_agc, agc, 'last_agc'); toggle_or_set_agc(agc);
	hang = kiwi_toggle(toggle_flags, default_hang, hang, 'last_hang'); toggle_or_set_hang(hang);
	manGain = kiwi_toggle(toggle_flags, default_manGain, manGain, 'last_manGain'); setManGain(true, manGain);
	thresh = kiwi_toggle(toggle_flags, default_thresh, thresh, 'last_thresh'); setThresh(true, thresh);
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
		html('id-button-hang').style.borderColor = html('label-threshold').style.color = html('label-slope').style.color = html('label-decay').style.color = 'orange';
	} else {
		html('id-label-man-gain').style.color = 'orange';
		html('id-button-hang').style.borderColor = html('label-threshold').style.color = html('label-slope').style.color = html('label-decay').style.color = 'white';
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
	html('id-optbar-users').innerHTML = w3_div('w3-nowrap', s);
}


////////////////////////////////
// control panel
////////////////////////////////

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
   freqset_select();
}

// icon callbacks
function freq_up_down_cb(path, up)
{
   freq_memory_up_down(+up);
	w3_field_select('id-freq-input', { focus_only:1 });
}

var spectrum_display = false;

function toggle_or_set_spec(set, val)
{
	if (typeof set == 'number' && (set & toggle_e.SET)) {
		spectrum_display = val;
	} else {
		spectrum_display ^= 1;
	}

	// close the extension first if it's using the data container and the spectrum button is pressed
	if (extint.using_data_container && spectrum_display) {
		extint_panel_hide();
	}

	html('id-button-spectrum').style.color = spectrum_display? 'lime':'white';
	w3_show_hide('id-spectrum-container', spectrum_display);
	w3_show_hide('id-top-container', !spectrum_display);
   freqset_select();
}

var band_menu = [];

function setup_band_menu()
{
	var i, op=0, service = null;
	var s="";
	band_menu[op++] = null;		// menu title
	for (i=0; i < bands.length; i++) {
		var b = bands[i];
		if (b.region != "*" && b.region.charAt(0) != '>' && b.region != 'm') continue;

		// FIXME: Add prefix to IBP names to differentiate from ham band names.
		// A software release can't modify bands[] definition in config.js so do this here.
		// At some point config.js will be eliminated when admin page gets equivalent UI.
		if ((b.s == svc.L || b.s == svc.X) && b.region == 'm') {
		   if (!b.name.includes('IBP'))
		      b.name = 'IBP '+ b.name;
		}

		if (service != b.s.name) {
			service = b.s.name; s += '<option value="'+op+'" disabled>'+b.s.name.toUpperCase()+'</option>';
			band_menu[op++] = null;		// section title
		}
		s += '<option value="'+op+'">'+b.name+'</option>';
		//console.log("BAND-MENU"+op+" i="+i+' '+(b.min/1000)+'/'+(b.max/1000));
		band_menu[op++] = b;
	}
	return s;
}

function mode_over(evt, el)
{
   el.title = evt.shiftKey? 'restore passband':'';
}

function any_alternate_click_event(evt)
{
	return (evt.shiftKey || evt.ctrlKey || evt.altKey || evt.button == mouse.middle || evt.button == mouse.right);
}

function restore_passband(mode)
{
   passbands[mode].last_lo = passbands[mode].lo;
   passbands[mode].last_hi = passbands[mode].hi;
   //writeCookie('last_locut', passbands[mode].last_lo.toString());
   //writeCookie('last_hicut', passbands[mode].last_hi.toString());
   //console.log('DEMOD PB reset');
}

function mode_button(evt, el)
{
   var mode = el.innerHTML.toLowerCase();

	// Prevent going between mono and stereo modes while recording
	if ((recording && cur_mode === 'iq') || (recording && cur_mode != 'iq' && mode === 'iq')) {
		return;
	}

	// reset passband to default parameters
	if (any_alternate_click_event(evt)) {
	   restore_passband(mode);
	}
	
	demodulator_analog_replace(mode);
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
	//el.style.backgroundColor = step_9_10? rgb(255, 255*0.8, 255*0.8) : rgb(255*0.8, 255*0.8, 255);
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
		if(actual_order<min_order) 
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

var CONTROL_HEIGHT = 230;
var OPTBAR_HEIGHT = 136;
var OPTBAR_TMARGIN = 8;

// called indirectly by a w3_call()
function panel_setup_control(el)
{
	divControl = el;

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

	   w3_div('id-control-freq1') +
	   w3_div('id-control-freq2') +
	   w3_div('id-control-mode w3-margin-T-6') +
	   w3_div('id-control-zoom w3-margin-T-6') +
	   w3_div('id-control-squelch') +
	   w3_div('id-optbar w3-margin-T-4') +
	   w3_div('id-optbar-content w3-margin-T-8 w3-scroll-y|height:'+ px(OPTBAR_HEIGHT),
	      w3_div('id-optbar-wf w3-hide'),
	      w3_div('id-optbar-audio w3-hide'),
	      w3_div('id-optbar-agc w3-hide'),
	      w3_div('id-optbar-users w3-hide w3-scroll-x'),
	      w3_div('id-optbar-status w3-hide')
	   ) +
	   w3_div('id-control-smeter w3-margin-T-8');

	// make first line of controls full width less vis button
	w3_el('id-control-freq1').style.width = px(el.activeWidth - visIcon - 6);
	
	w3_show_hide('id-mute-no', !muted);
	w3_show_hide('id-mute-yes', muted);
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

function event_dump(evt, id)
{
   console.log('================================');
	console.log('EVENT_DUMP: '+ id +' type='+ evt.type);
	console.log((evt.shiftKey? 'SFT ':'') + (evt.ctrlKey? 'CTL ':'') + (evt.altKey? 'ALT ':'') + (evt.metaKey? 'META ':'') +'key='+evt.key);
	console.log('this.id='+ this.id +' tgt.name='+ evt.target.nodeName +' tgt.id='+ evt.target.id +' ctgt.id='+ evt.currentTarget.id);
	console.log('button='+evt.button+' buttons='+evt.buttons+' detail='+evt.detail+' which='+evt.which);
	console.log('offX='+evt.offsetX+' pageX='+evt.pageX+' clientX='+evt.clientX+' layerX='+evt.layerX );
	console.log('offY='+evt.offsetY+' pageY='+evt.pageY+' clientY='+evt.clientY+' layerY='+evt.layerY );
	console.log('evt, evt.target, evt.currentTarget:');
	console.log(evt);
	console.log(evt.target);
	console.log(evt.currentTarget);
   console.log('----');
}

function arrayBufferToString(buf) {
	//http://stackoverflow.com/questions/6965107/converting-between-strings-and-arraybuffers
	return String.fromCharCode.apply(null, new Uint8Array(buf));
}

function arrayBufferToStringLen(buf, num)
{
	var u8buf=new Uint8Array(buf);
	var output=String();
	num=Math.min(num,u8buf.length);
	for (var i=0;i<num;i++) output+=String.fromCharCode(u8buf[i]);
	return output;
}

// NB: use kiwi_log() instead of console.log() in here
function add_problem(what, append, sticky, el_id)
{
   var id = el_id? el_id : 'id-status-problems'
	//kiwi_log('add_problem '+ what +' sticky='+ sticky +' id='+ id);
	el = w3_el(id);
	if (!el) return;
	if (typeof el.children != "undefined") {
		for (var i=0; i < el.children.length; i++) {
		   if (el.children[i].innerHTML == what) {
		      return;
		   }
		}
		if (append == false) {
		   while (el.children.length != 0) {
		      el.removeChild(el.children.shift());
		   }
		}
	}
	var new_span = w3_appendElement(el, "span", what);
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

function owrx_msg_cb(param, ws)
{
	switch (param[0]) {
		case "wf_setup":
			waterfall_init();
			break;					
		case "extint_list_json":
			extint_list_json(param[1]);
			
			// now that we have the list of extensions see if there is an override
			if (override_ext)
				extint_open(override_ext, 3000);
			else
				if (override_mode == 's4285') extint_open('s4285', 3000);	//jks
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
			break;
		case "wf_fps":
			wf_fps = parseInt(param[1]);
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
			audio_init(parseInt(param[1]), btn_less_buffering, kiwi_toggle(toggle_e.FROM_COOKIE | toggle_e.SET, 1, 1, 'last_compression'));
			break;
		case "audio_rate":
			audio_rate(parseFloat(param[1]));
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
		case "squelch":
			squelch_state = parseInt(param[1]);
			var el = w3_el('id-squelch-label');
			//console.log('SQ '+ squelch_state);
			if (el) el.style.color = squelch_state? 'lime':'white';
			break;
	}
}

function owrx_ws_open_snd(cb, cbp)
{
	ws_snd = open_websocket('SND', cb, cbp, owrx_msg_cb, audio_recv, on_ws_error);
	return ws_snd;
}

function owrx_ws_open_wf(cb, cbp)
{
	ws_wf = open_websocket('W/F', cb, cbp, owrx_msg_cb, waterfall_add_queue, on_ws_error);
	return ws_wf;
}

function on_ws_error()
{
	divlog("WebSocket error.", 1);
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

var need_geo = true;
var need_status = true;

function send_keepalive()
{
	for (var i=0; i<1; i++) {
		if (!ws_snd.up || snd_send("SET keepalive") < 0)
			break;
	
		// these are done here because we know the audio connection is up and can receive messages
		if (need_geo && kiwi_geo() != "") {
			if (msg_send("SET geo="+kiwi_geo()) < 0)
				break;
			if (msg_send("SET geojson="+kiwi_geojson()) < 0)
				break;
			need_geo = false;
		}
		
		if (need_ident) {
			//console.log('need_ident: SET ident_user='+ ident_user);
			if (!ident_user) ident_user = '';
			if (snd_send("SET ident_user="+ encodeURIComponent(ident_user)) < 0)
				break;
			need_ident = false;
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
