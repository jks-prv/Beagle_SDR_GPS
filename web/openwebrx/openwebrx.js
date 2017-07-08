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

// Copyright (c) 2015 - 2017 John Seamons, ZL/KF6VO


// key freq concepts:
//		all freqs in Hz
//		center_freq is center of entire sampled bandwidth (e.g. 15 MHz)
//		offset_freq (-bandwidth/2 .. bandwidth/2) = center_freq - freq
//		freq = center_freq + offset_freq

// constants, passed from server
var bandwidth;
var center_freq;
var wf_fft_size;

// UI geometry
var height_top_bar_parts = 67;
var height_spectrum_canvas = 200;

var cur_mode;
var wf_fps;

var ws_snd, ws_wf;

var inactivity_timeout_override = -1, inactivity_timeout_msg = false;

var spectrum_show = 0;
var gen_freq = 0, gen_attn = 0;
var squelch_threshold = 0;
var wf_compression = 1;
var debug_v = 0;		// a general value settable from the URI to be used during debugging
var sb_trace = 0;
var kiwi_gc = 1;
var kiwi_gc_snd = 0;
var kiwi_gc_wf = -1;
var kiwi_gc_recv = -1;
var kiwi_gc_wspr = -1;
var override_ext = null, extint_param = null;
var muted_initially = false;
var param_nocache = false;
var nocache = false;
var param_ctrace = false;
var ctrace = false;
var no_clk_corr = false;

function kiwi_main()
{
	override_freq = parseFloat(readCookie('last_freq'));
	override_mode = readCookie('last_mode');
	override_zoom = parseFloat(readCookie('last_zoom'));
	override_9_10 = parseFloat(readCookie('last_9_10'));
	override_max_dB = parseFloat(readCookie('last_max_dB'));
	override_min_dB = parseFloat(readCookie('last_min_dB'));
	
	console.log('LAST f='+ override_freq +' m='+ override_mode +' z='+ override_zoom
		+' 9_10='+ override_9_10 +' min='+ override_min_dB +' max='+ override_max_dB);

	// process URL query string parameters
	var num_win = 4;
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
	// x?			matches x 0 or 1 time
	// (x)		capturing parens, stores in array
	// (?:x)y	non-capturing parens, allows y to apply to multi-char x
	//	x|y		alternative (or)
	// x? x* x+	0/1, >=0, >=1 occurrences of x

	var q = qd[1];
	//console.log('this window/tab:');
	//console.log(q);
	
	s = 'f'; if (q[s]) {
		var p = new RegExp('([0-9.,]*)([^&#z]*)?z?([0-9]*)').exec(q[s]);
		if (p[1]) override_freq = parseFloat(p[1].replace(',', '.'));
		if (p[2]) override_mode = p[2];
		if (p[3]) override_zoom = p[3];
	}

	s = 'ext'; if (q[s]) {
		override_ext = q[s].split(',')[0];
		extint_param = q[s].split(',')[1];
		console.log('URL: ext='+ override_ext +' ext_p='+ extint_param);
	}

	s = 'sp'; if (q[s]) spectrum_show = parseInt(q[s]);
	s = 'sq'; if (q[s]) squelch_threshold = parseFloat(q[s]);
	s = 'blen'; if (q[s]) audio_buffer_min_length_sec = parseFloat(q[s])/1000;
	s = 'wfdly'; if (q[s]) waterfall_delay = parseFloat(q[s]);
	s = 'audio'; if (q[s]) audio_better_delay = parseFloat(q[s]);
	s = 'mute'; if (q[s]) muted_initially = parseInt(q[s]);
	s = 'timeout'; if (q[s]) OFF_inactivity_timeout_override = parseFloat(q[s]);
	s = 'gen'; if (q[s]) gen_freq = parseFloat(q[s]);
	s = 'attn'; if (q[s]) gen_attn = parseInt(q[s]);
	s = 'cmap'; if (q[s]) colormap_select = parseInt(q[s]);
	s = 'sqrt'; if (q[s]) colormap_sqrt = parseInt(q[s]);
	s = 'wf_comp'; if (q[s]) wf_compression = parseInt(q[s]);
	s = 'gc'; if (q[s]) kiwi_gc = parseInt(q[s]);
	s = 'gc_snd'; if (q[s]) kiwi_gc_snd = parseInt(q[s]);
	s = 'gc_wf'; if (q[s]) kiwi_gc_wf = parseInt(q[s]);
	s = 'gc_recv'; if (q[s]) kiwi_gc_recv = parseInt(q[s]);
	s = 'gc_wspr'; if (q[s]) kiwi_gc_wspr = parseInt(q[s]);
	s = 'nocache'; if (q[s]) { param_nocache = true; nocache = parseInt(q[s]); }
	s = 'ctrace'; if (q[s]) { param_ctrace = true; ctrace = parseInt(q[s]); }
	s = 'ncc'; if (q[s]) no_clk_corr = parseInt(q[s]);
	s = 'v'; if (q[s]) console.log('URL: debug_v = '+ (debug_v = q[s]));

	if (muted_initially) toggle_mute();

	if (kiwi_gc_snd == -1) kiwi_gc_snd = kiwi_gc;
	if (kiwi_gc_wf == -1) kiwi_gc_wf = kiwi_gc;
	if (kiwi_gc_recv == -1) kiwi_gc_recv = kiwi_gc;
	if (kiwi_gc_wspr == -1) kiwi_gc_wspr = kiwi_gc;
	console.log('GC: snd='+ kiwi_gc_snd +' wf='+ kiwi_gc_wf +' recv='+ kiwi_gc_recv +' wspr='+ kiwi_gc_wspr);

	kiwi_xdLocalStorage_init();
	kiwi_get_init_settings();
	kiwi_geolocate();
	init_rx_photo();
	right_click_menu_init();
	confirmation_panel_init();
	ext_panel_init();
	place_panels();
	init_panels();
	smeter_init();
	extint_init();
	time_display_setup('id-topbar-right-container');
	
	window.setTimeout(function() {window.setInterval(send_keepalive, 5000);}, 5000);
	window.setTimeout(function() {window.setInterval(status_periodic, 1000);}, 1000);
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
	snd_send("SET autonotch=0");
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
	wf_send("SET slow=2");
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
	init_panel_toggle(ptype.TOGGLE, 'id-readme', false, (dbgUs || kiwi_isMobile())? popt.CLOSE : (readme_firsttime? popt.PERSIST : 7000), readme_color);

	init_panel_toggle(ptype.TOGGLE, 'id-msgs', true, kiwi_isMobile()? popt.CLOSE : popt.PERSIST);

	var news_firsttime = (readCookie('news', 'seen') == null);
	init_panel_toggle(ptype.POPUP, 'id-news', false, show_news? (news_firsttime? popt.PERSIST : popt.CLOSE) : popt.CLOSE);

	init_panel_toggle(ptype.POPUP, 'id-ext-controls', false, popt.CLOSE);

	init_panel_toggle(ptype.POPUP, 'id-confirmation', false, popt.CLOSE);
}

function init_panel_toggle(type, panel, scrollable, timeo, color)
{
	var divPanel = w3_el_id(panel);
	divPanel.ptype = type;
	console.log('init_panel_toggle '+ panel +' ptype='+ divPanel.ptype +' '+ type);
	var divVis = w3_el_id(panel+'-vis');
	divPanel.scrollable = (scrollable == true)? true:false;
	var visHoffset = (divPanel.scrollable)? -kiwi_scrollbar_width() : visBorder;
	var rightSide = (divPanel.getAttribute('data-panel-pos') == "right");
   divPanel.panelShown = 1;
	
	if (type == ptype.TOGGLE) {
		var hide = rightSide? 'right':'left';
		var show = rightSide? 'left':'right';
		divVis.innerHTML =
			'<a id="'+panel+'-hide" onclick="toggle_panel('+ q(panel) +');"><img src="icons/hide'+ hide +'.24.png" width="24" height="24" /></a>' +
			'<a id="'+panel+'-show" class="class-vis-show" onclick="toggle_panel('+ q(panel) +');"><img src="icons/hide'+ show +'.24.png" width="24" height="24" /></a>';
	} else {		// ptype.POPUP or ptype:HIDE
		divVis.innerHTML =
			'<a id="'+panel+'-close" onclick="toggle_panel('+ q(panel) +');"><img src="icons/close.24.png" width="24" height="24" /></a>';
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
		var divShow = w3_el_id(panel+'-show');
		if (divShow != undefined) divShow.style.backgroundColor = divShow.style.borderColor = color;
	}
}

function toggle_panel(panel)
{
	var divPanel = w3_el_id(panel);
	var divVis = w3_el_id(panel+'-vis');
	console.log('toggle_panel '+ panel +' ptype='+ divPanel.ptype);

	if (divPanel.ptype == ptype.POPUP) {
		divPanel.style.visibility = divPanel.panelShown? 'hidden' : 'visible';
		divPanel.panelShown ^= 1;
		updateCookie(panel, 'seen');
		return;
	}
	
	var arrow_width = 12, hideWidth = divPanel.activeWidth + 2*15;
	var rightSide = (divPanel.getAttribute('data-panel-pos') == "right");
	var from, to;

	hideWidth = rightSide? -hideWidth : -hideWidth;
	if (divPanel.panelShown) {
		from = 0; to = hideWidth;
	} else {
		from = hideWidth; to = 0;
	}
	
	animate(divPanel, rightSide? 'right':'left', "px", from, to, 0.93, kiwi_isMobile()? 1:1000, 60, 0);
	
	w3_el_id(panel+'-'+(divPanel.panelShown? 'hide':'show')).style.display = "none";
	w3_el_id(panel+'-'+(divPanel.panelShown? 'show':'hide')).style.display = "block";
	divPanel.panelShown ^= 1;

	var visOffset = divPanel.activeWidth - visIcon;
	var visHoffset = (divPanel.scrollable)? -kiwi_scrollbar_width() : visBorder;
	//console.log("toggle_panel "+panel+" right="+rightSide+" shown="+divPanel.panelShown);
	if (rightSide)
		divVis.style.right = px(divPanel.panelShown? 0 : (visOffset + visIcon + visBorder*2));
	else
		divVis.style.left = px(visOffset + (divPanel.panelShown? visHoffset : (visIcon + visBorder*2)));
	freqset_select();
}

function openwebrx_resize()
{
	resize_canvases();
	resize_waterfall_container(true);
	resize_scale();
	//check_top_bar_congestion();
}

function check_top_bar_congestion()
{
	var left = w3_boundingBox_children('id-left-info-container');
	var owner = w3_boundingBox_children('id-mid-owner-container');
	var mid = w3_boundingBox_children('id-mid-info-container');
	var right = w3_boundingBox_children('id-topbar-right-container');
	
	console.log('LEFT offL='+ left.offsetLeft +' offR='+ left.offsetRight +' width='+ left.offsetWidth);
	console.log('OWNER offL='+ owner.offsetLeft +' offR='+ owner.offsetRight +' width='+ owner.offsetWidth);
	console.log('MID offL='+ mid.offsetLeft +' offR='+ mid.offsetRight +' width='+ mid.offsetWidth);
	console.log('RIGHT offL='+ right.offsetLeft +' offR='+ right.offsetRight +' width='+ right.offsetWidth);

/*
	var total = left.offsetRight + mid.offsetWidth + 30 + 200;
	console.log('CHECK offR='+ left.offsetRight +' width='+ mid.offsetWidth +' tot='+ total +' iw='+ window.innerWidth);
	
	if (total > window.innerWidth) {
		visible_block('id-topbar-right-container', false);
		w3_iterate_children('id-mid-info-container', function(el) {
			console.log(el.id +' HIDE '+ css_style(el, 'right'));
			el.style.right = '15px';
		});
	} else {
		visible_block('id-topbar-right-container', true);
		w3_iterate_children('id-mid-info-container', function(el) {
			console.log(el.id +' SHOW '+ css_style(el, 'right'));
			el.style.right = '230px';
		});
	}
*/
}

var rx_photo_spacer_height = height_top_bar_parts;

function init_rx_photo()
{
	RX_PHOTO_HEIGHT += rx_photo_spacer_height;
	html("id-top-photo-clip").style.maxHeight = px(RX_PHOTO_HEIGHT);
	//window.setTimeout(function() { animate(html("id-rx-photo-title"),"opacity","",1,0,1,500,30); },1000);
	//window.setTimeout(function() { animate(html("id-rx-photo-desc"),"opacity","",1,0,1,500,30); },1500);
	if (dbgUs || kiwi_isMobile()) {
		close_rx_photo();
	} else {
		window.setTimeout(function() { close_rx_photo() },5000);
	}
}

var rx_photo_state=1;

function open_rx_photo()
{
	rx_photo_state=1;
	html("id-rx-photo-desc").style.opacity=1;
	html("id-rx-photo-title").style.opacity=1;
	animate_to(html("id-top-photo-clip"),"maxHeight","px",RX_PHOTO_HEIGHT,0.93,kiwi_isMobile()? 1:1000,60,function(){resize_waterfall_container(true);});
	html("id-rx-details-arrow-down").style.display="none";
	html("id-rx-details-arrow-up").style.display="block";
}

function close_rx_photo()
{
	rx_photo_state=0;
	animate_to(html("id-top-photo-clip"),"maxHeight","px",rx_photo_spacer_height,0.93,kiwi_isMobile()? 1:1000,60,function(){resize_waterfall_container(true);});
	html("id-rx-details-arrow-down").style.display="block";
	html("id-rx-details-arrow-up").style.display="none";
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


// ========================================================
// =================  >ANIMATION ROUTINES  ================
// ========================================================

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


// ========================================================
// ================  >DEMODULATOR ROUTINES  ===============
// ========================================================

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
	scale_ctx.lineWidth=3;
	scale_ctx.strokeStyle=color;
	scale_ctx.fillStyle = color;
	var drag_ranges = { envelope_on_screen: false, line_on_screen: false };
	
	if(!(to_px<0 || from_px>window.innerWidth)) // out of screen?
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
	
	if(typeof line != "undefined") // out of screen? 
	{
		line_px=scale_px_from_freq(line,range);
		if(!(line_px<0||line_px>window.innerWidth))
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
	this.has_audio_output=true;
	this.has_text_output=false;
	this.envelope={};
	this.color=demodulators_get_next_color();
	this.stop=function(){};
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
	am:		{ lo: -5000,	hi:  5000 },
	amn:		{ lo: -2500,	hi:  2500 },
	lsb:		{ lo: -2700,	hi:  -300 },	// cf = 1500 Hz, bw = 2400 Hz
	usb:		{ lo:   300,	hi:  2700 },	// cf = 1500 Hz, bw = 2400 Hz
	cw:		{ lo:   300,	hi:   700 },	// cf = 500 Hz, bw = 400 Hz
	cwn:		{ lo:   470,	hi:   530 },	// cf = 500 Hz, bw = 60 Hz
	nbfm:		{ lo: -4600,	hi:  4600 },	// FIXME: set based on current srate?
	s4285:	{ lo:   600,	hi:  3000 },	// cf = 1800 Hz, bw = 2400 Hz, s4285 compatible
//	s4285:	{ lo:   400,	hi:  3200 },	// cf = 1800 Hz, bw = 2800 Hz, made things a little worse?
};

function demodulator_default_analog(offset_frequency, subtype, locut, hicut)
{
	//http://stackoverflow.com/questions/4152931/javascript-inheritance-call-super-constructor-or-use-prototype-chain
	demodulator.call(this, offset_frequency);

	this.subtype=subtype;
	this.envelope.dragged_range = demodulator.draggable_ranges.none;
	var sampleRateDiv2 = audio_input_rate? audio_input_rate/2 : 5000;
	this.filter={
		min_passband: 50,
		high_cut_limit: sampleRateDiv2,
		low_cut_limit: -sampleRateDiv2
	};

	//Subtypes only define some filter parameters and the mod string sent to server, 
	//so you may set these parameters in your custom child class.
	//Why? As the demodulation is done on the server, difference is mainly on the server side.
	this.server_mode = subtype;

	var lo = isNaN(locut)? passbands[subtype].last_lo : locut;
	var hi = isNaN(hicut)? passbands[subtype].last_hi : hicut;
	if (lo == 'undefined' || lo == null) {
		lo = passbands[subtype].last_lo = passbands[subtype].lo;
	}
	if (hi == 'undefined' || hi == null) {
		hi = passbands[subtype].last_hi = passbands[subtype].hi;
	}
	this.low_cut = lo;
	this.high_cut = hi;
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
		snd_send("SET mod="+this.server_mode+
			" low_cut="+this.low_cut.toString()+" high_cut="+this.high_cut.toString()+
			" freq="+(freq_car_Hz/1000).toFixed(3));
	}
	// this.set(); //we set parameters on object creation

	//******* envelope object *******
   // for drawing the filter envelope above scale
	this.envelope.parent = this;

	this.envelope.draw = function(visible_range) 
	{
		this.visible_range = visible_range;
		this.drag_ranges = demod_envelope_draw(g_range,
				center_freq+this.parent.offset_frequency + this.parent.low_cut,
				center_freq+this.parent.offset_frequency + this.parent.high_cut,
				this.color, center_freq+this.parent.offset_frequency);
		var bw = Math.abs(this.parent.high_cut - this.parent.low_cut);
		pb_adj_lo_ttip.innerHTML = 'lo '+ this.parent.low_cut.toString() +', bw '+ bw.toString();
		pb_adj_hi_ttip.innerHTML = 'hi '+ this.parent.high_cut.toString() +', bw '+ bw.toString();
		pb_adj_cf_ttip.innerHTML = 'cf '+ (this.parent.low_cut + Math.abs(this.parent.high_cut - this.parent.low_cut)/2).toString();
		pb_adj_car_ttip.innerHTML = ((center_freq + this.parent.offset_frequency)/1000).toFixed(2) +' kHz';
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
		//html("id-control-1").innerHTML=format_frequency("{x} MHz",center_freq+this.parent.offset_frequency,1e6,4);
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
	var offset = 0, prev_pbo = 0, low_cut = NaN, high_cut = NaN;
	var wasCW = false, toCW = false, fromCW = false;
	
	if(demodulators.length) {
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
	} else {
		// Freq not changing, just mode. Do correct thing for switch to/from cw modes: keep display freq constant
		var pbo = 0;
		if (toCW) pbo = -passband_offset();
		if (fromCW) pbo = prev_pbo;	// passband offset calculated _before_ demod was changed
		offset += pbo;
		//console.log('DEMOD-replace SAME freq='+ (offset + center_freq) +' PBO='+ pbo +' toCW='+ toCW +' fromCW='+ fromCW);
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
	if (offset>bandwidth/2 || offset<-bandwidth/2) return;
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


// ========================================================
// ===================  >SCALE  ===========================
// ========================================================

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
	
	resize_scale();
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
	var bins = bins_at_cur_zoom();
	out.start = bin_to_freq(x_bin);
	out.center = bin_to_freq(x_bin + bins/2);
	out.end = bin_to_freq(x_bin + bins);
	out.bw = out.end-out.start;
	out.hpp = out.bw / scale_canvas.clientWidth;
	//console.log("GVFR z="+zoom_level+" xb="+x_bin+" BACZ="+bins+" s="+out.start+" c="+out.center+" e="+out.end+" bw="+out.bw+" hpp="+out.hpp+" cw="+scale_canvas.clientWidth);
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
	fcalc = function(freq) 
	{ 
		out.numlarge = (range.bw/freq);
		out.pxlarge = canvas_container.clientWidth/out.numlarge; 	//distance between large markers (these have text)
		out.ratio = 5; 															//(ratio-1) small markers exist per large marker
		out.pxsmall = out.pxlarge/out.ratio; 								//distance between small markers
		if (out.pxsmall < scale_min_space_btwn_small_markers) return false; 
		if (out.pxsmall/2 >= scale_min_space_btwn_small_markers && freq.toString()[0] != "5") { out.pxsmall/=2; out.ratio*=2; }
		out.smallbw = freq/out.ratio;
		return true;
	}
	
	for (var i=scale_markers_levels.length-1; i>=0; i--)
	{
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
	scale_ctx.font = "bold 11px sans-serif";
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
		if (f < 1e6) {
			pre_divide /= 1000;
			decimals = 0;
		}
		text_to_draw = format_frequency(spacing.params.format+((f < 1e6)? 'kHz':'MHz'), f, pre_divide, decimals);
	}
	var last_large;

var conv_ct=0;
	for(;;)
	{
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
				//if this is the first overall marker when zoomed out
				//console.log('case 1');
				if (x < text_measured.width/2)
				{ //and if it would be clipped off the screen
					if (scale_px_from_freq(marker_hz+spacing.smallbw*spacing.ratio,g_range)-text_measured.width >= scale_min_space_btwn_texts)
					{ //and if we have enough space to draw it correctly without clipping
						scale_ctx.textAlign = "left";
						scale_ctx.fillText(text_to_draw, 0, text_y_pos); 
					}
				}
			} else
			if (zoom_level==0 && g_range.end-spacing.smallbw*spacing.ratio < marker_hz)  
			{ //if this is the last overall marker when zoomed out
				//console.log('case 2');
				if (x > window.innerWidth-text_measured.width/2) 
				{ //and if it would be clipped off the screen
					if (window.innerWidth-text_measured.width-scale_px_from_freq(marker_hz-spacing.smallbw*spacing.ratio,g_range) >= scale_min_space_btwn_texts)
					{ //and if we have enough space to draw it correctly without clipping
						scale_ctx.textAlign = "right";
						scale_ctx.fillText(text_to_draw, window.innerWidth, text_y_pos); 
					}	
				} else {
					// last large marker is not the last marker, so draw normally
					scale_ctx.fillText(text_to_draw, x, text_y_pos);
				}
			} else {
				//console.log('case 3');
				scale_ctx.fillText(text_to_draw, x, text_y_pos); //draw text normally
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

function resize_scale()
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


// ========================================================
// =================== >CONVERSIONS =======================
// ========================================================

// A "bin" is the wf_fft_size multiplied by the maximum zoom factor.
// So there are approx 1024 * 2^11 = 2M bins.
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


// ========================================================
// ======================== >CANVAS =======================
// ========================================================

function canvas_contextmenu(evt)
{
	//console.log('## CMENU tgt='+ evt.target.id +' Ctgt='+ evt.currentTarget.id);
	
	if (evt.target.id == 'id-wf-canvas') {
		// TBD: popup menu with database lookup, etc.
	}
	
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
	var norm = relativeX/waterfall_width;
	var bin = x_bin + norm * bins_at_cur_zoom();
	var freq = bin_to_freq(bin);
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
      '<hr>',
      'restore passband',
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
		freq_database_lookup(canvas_get_dspfreq(x), (idx == 1));
      break;
   
   case 2:  // restore passband
      restore_passband(cur_mode);
      demodulator_analog_replace(cur_mode);
      break;
   
   case 3:  // cal ADC clock
      admin_pwd_query(function() {
         var r1k_kHz = Math.round(freq_displayed_Hz / 1e3);     // 1kHz windows on 1 kHz boundaries
         var r1k_Hz = r1k_kHz * 1e3;
         var clk_diff = r1k_Hz - freq_displayed_Hz;
         var clk_adj = Math.round(clk_diff * ext_adc_clock_Hz() / r1k_Hz);   // clock adjustment normalized to ADC clock frequency
         console.log('cal ADC clock dsp='+ freq_displayed_Hz +' car='+ freq_car_Hz +' r1k_kHz='+ r1k_kHz +' clk_diff='+ clk_diff +' clk_adj='+ clk_adj);
         var new_adj = cfg.clk_adj + clk_adj;
         var ppm = new_adj * 1e6 / ext_adc_clock_Hz();
         console.log('cal ADC clock prev_adj='+ cfg.clk_adj +' new_adj='+ new_adj +' ppm='+ ppm.toFixed(1));
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

   f = Math.floor(Hz/100) / 10000;	// round down to nearest 100 Hz, and express in MHz, for GlobalTuners
   var globaltuners = "qrg.globaltuners.com/?q="+f.toFixed(4);
   
   if (utility) {
      url += globaltuners;
   } else {
      if (kHz >= b.NDB_lo && kHz < b.NDB_hi) {
         f = kHz_r1k.toFixed(0);		// 1kHz windows on 1 kHz boundaries for NDBs
         url += "www.classaxe.com/dx/ndb/rww/signal_list/?mode=signal_list&submode=&targetID=&sort_by=khz&limit=-1&offset=0&show=list&"+
         "type_DGPS=1&type_NAVTEX=1&type_NDB=1&filter_id=&filter_khz_1="+ f +"&filter_khz_2="+ f +
         "&filter_channels=&filter_sp=&filter_sp_itu_clause=AND&filter_itu=&filter_continent=&filter_dx_gsq=&region=&"+
         "filter_listener%5B%5D=&filter_heard_in=%28All+States+and+Countries%29&filter_date_1=&filter_date_2=&offsets=&sort_by_column=khz";
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
   
   console.log('LOOKUP '+ kHz +' -> '+ f +' '+ url);
   var win = window.open(url, '_blank');
   if (win) win.focus();
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
		html("id-mouse-unit").innerHTML = format_frequency("{x}", canvas_get_dspfreq(relativeX), 1e3, 2);
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
		ignore_next_keyup_event = true;
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

function canvas_mousewheel(evt)
{
	if (!waterfall_setup_done) return;
	//var i = Math.abs(evt.wheelDelta);
	//var out = ((i / evt.wheelDelta) < 0);
	//console.log(evt);
	var relativeX = evt.pageX;
	var out = ((evt.deltaY / Math.abs(evt.deltaY)) > 0);
	//console.log(dir);
	//i/=120;
	var dir = out? ext_zoom.OUT : ext_zoom.IN;
	/*while (i--)*/
		zoom_step(dir, relativeX);
	evt.preventDefault();	
	//evt.returnValue = false; //disable scrollbar move
}


var ZOOM_NOMINAL = 10, ZOOM_BAND = 6;
var zoom_nom = 0, zoom_old_nom = 0;
var zoom_levels_max = 0;
var zoom_level = 0;
var zoom_freq = 0;
var zoom_maxin_s = ['id-maxin', 'id-maxin-nom', 'id-maxin-max'];

var x_bin = 0;				// left edge of waterfall in units of bin number

// called from mouse wheel and zoom button pushes
// x_rel: 0 .. waterfall_width, position of mouse cursor on window, -1 called from button push

function zoom_step(dir, arg2)
{
	var out = (dir == ext_zoom.OUT);
	var dir_in = (dir == ext_zoom.IN);
	var not_band_and_not_abs = (dir != ext_zoom.TO_BAND && dir != ext_zoom.ABS);
	var ozoom = zoom_level;
	var x_obin = x_bin;
	var x_norm;

	//console.log('zoom_step dir='+ dir +' arg2='+ arg2);
	if (dir == ext_zoom.MAX_OUT) {		// max out
		out = true;
		zoom_level = 0;
		x_bin = 0;
	} else {			// in/out, nom/max in, abs, band
	
		// clamp
		if (not_band_and_not_abs && ((out && zoom_level == 0) || (dir_in && zoom_level >= zoom_levels_max))) { freqset_select(); return; }

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
			if (arg2 == undefined) { freqset_select(); return; }		// no abs zoom value specified
			var znew = arg2;
			//console.log('zoom_step ABS znew='+ znew +' zmax='+ zoom_levels_max +' zcur='+ zoom_level);
			if (znew < 0 || znew > zoom_levels_max || znew == zoom_level) { freqset_select(); return; }
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
	
	//console.log("ZStep z"+zoom_level.toString()+" zlevel_val="+zoom_levels[zoom_level].toString()+" fLEFT="+canvas_get_dspfreq(0));
	
	var nom = (zoom_level == zoom_levels_max)? 2 : ((zoom_level >= zoom_nom)? 1:0);
	if (nom != zoom_old_nom) {
		visible_table_cell(zoom_maxin_s[zoom_old_nom], false);
		visible_table_cell(zoom_maxin_s[nom], true);
		zoom_old_nom = nom;
	}
	
	if (zoom_level == 0 || ozoom == 0) {
		visible_table_cell('id-maxout', zoom_level != 0);
		visible_table_cell('id-maxout-max', zoom_level == 0);
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
   freqset_select();
	writeCookie('last_zoom', zoom_level);
	freq_link_update();
}

function zoom_click(evt, dir, arg2)
{
	if (any_alternate_click_event(evt)) {
		// currentTarget.parent=<span> currentTarget=<div> target=<img>
		var div = evt.currentTarget;
		var span = div.parentNode;		
		var zin = (span.id == 'id-zoom-in');
		var zout = (span.id == 'id-zoom-out');
		if (!zin && !zout) return;
		
		var pb = ext_get_passband();
		var pb_width = Math.abs(pb.high - pb.low);
		var pb_inc;
		if (zin)
			pb_inc = ((pb_width * (1/0.80)) - pb_width) / 2;		// wider
		else
			pb_inc = (pb_width - (pb_width * 0.80)) / 2;				// narrower

		// round to 10 Hz
		// FIXME: this doesn't allow us to reach ultimate min or max passband width
		pb_inc = Math.round(pb_inc/10) * 10;
		//console.log('PB w='+ pb_width +' inc='+ pb_inc +' lo='+ pb.low +' hi='+ pb.high);
		pb.low += zin? -pb_inc : pb_inc;
		pb.high += zin? pb_inc : -pb_inc;
		ext_set_passband(pb.low, pb.high, true);
		//console.log('PB lo='+ pb.low +' hi='+ pb.high);
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
	var span = div.parentNode;
	
	var zin = (span.id == 'id-zoom-in');
	
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
	var dbins = norm_to_bins(norm_dir);
	waterfall_pan_canvases(dbins);		// < 0 = pan left (toward lower freqs)
   freqset_select();
}


var window_width;
var waterfall_width;
var waterfall_scrollable_height;

var canvas_container;
var canvas_phantom;

var wf_canvases = [];
var wf_cur_canvas = null;
var wf_canvas_default_height = 200;
var wf_canvas_actual_line;
var wf_canvas_id_seq = 1;

// NB: canvas data width is wf_fft_size, but displayed style width is waterfall_width (likely different),
// so image is stretched to fit when rendered by browser.

function add_canvas()
{	
	var new_canvas = document.createElement("canvas");
	new_canvas.id = 'id-wf-canvas';
	new_canvas.id_seq = wf_canvas_id_seq++;
	new_canvas.width = wf_fft_size;
	new_canvas.height = wf_canvas_default_height;
	wf_canvas_actual_line = wf_canvas_default_height-1;
	new_canvas.style.width = px(waterfall_width);	
	new_canvas.style.left = 0;
	new_canvas.openwebrx_height = wf_canvas_default_height;	
	new_canvas.style.height = px(new_canvas.openwebrx_height);

	// initially the canvas is one line "above" the top of the container
	new_canvas.openwebrx_top = (-wf_canvas_default_height+1);	
	new_canvas.style.top = px(new_canvas.openwebrx_top);

	new_canvas.ctx = new_canvas.getContext("2d");
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

	// a phantom one at the end
	canvas_phantom = html("id-phantom-canvas");
	add_canvas_listner(canvas_phantom);
	canvas_phantom.style.width = canvas_container.clientWidth+"px";

	// the first one to get started
	add_canvas();

	spectrum_canvas = document.createElement("canvas");	
	spectrum_canvas.id = 'id-spectrum-canvas';
	spectrum_canvas.width = wf_fft_size;
	spectrum_canvas.height = height_spectrum_canvas;
	spectrum_canvas.style.width = px(waterfall_width);
	spectrum_canvas.style.height = px(spectrum_canvas.height);
	html("id-spectrum-container").appendChild(spectrum_canvas);
	spectrum_ctx = spectrum_canvas.getContext("2d");
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
		canvas_phantom.style.display = "block";
	} else
		canvas_phantom.style.display = "none";
	
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
	canvas_phantom.style.width = new_width;
	canvas_phantom.style.left = zoom_value;
	spectrum_canvas.style.width = new_width;
}


// ========================================================
// =======================  >WATERFALL  ===================
// ========================================================

var waterfall_setup_done=0;
var waterfall_timer;
var waterfall_ms;

function waterfall_init()
{
	init_canvas_container();
	resize_waterfall_container(false); /* then */ resize_canvases();
	panels_setup();
	ident_init();
	scale_setup();
	mkcolormap();
	bands_init();
	spectrum_init();
	dx_schedule_update();
	users_init(false);
	stats_init();
	if (spectrum_show) toggle_or_set_spec(1);

	waterfall_ms = 900/wf_fps;
	waterfall_timer = window.setInterval(waterfall_dequeue, waterfall_ms);
	console.log('waterfall_dequeue @ '+ waterfall_ms +' msec');

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
var spectrum_update_rate_Hz = 10;	// limit update rate since rendering spectrum is currently expensive
var spectrum_update = 0, spectrum_last_update = 0;
var spectrum_filtered = 1;

function spectrum_init()
{
	spectrum_colormap = spectrum_ctx.createImageData(1, spectrum_canvas.height);
	spectrum_colormap_transparent = spectrum_ctx.createImageData(1, spectrum_canvas.height);
	update_maxmindb_sliders();
	spectrum_dB_bands();
	setInterval(function() { spectrum_update++ }, 1000 / spectrum_update_rate_Hz);
}

function spectrum_filter(filter)
{
	spectrum_filtered = filter;
	need_clear_specavg = true;
}

// based on WF max/min range, compute color banding each 10 dB for spectrum display
function spectrum_dB_bands()
{
	dB_bands = [];
	var i=0;
	var color_shift_dB = -8;	// give a little headroom to the spectrum colormap
	var barmax = maxdb, barmin = mindb + color_shift_dB;
	var rng = barmax - barmin;
	rng = rng? rng : 1;	// can't be zero
	//console.log("DB barmax="+barmax+" barmin="+barmin+" maxdb="+maxdb+" mindb="+mindb);
	var last_norm = 0;
	for (var dB = Math.floor(maxdb/10)*10; (mindb-dB) < 10; dB -= 10) {
		var norm = 1 - ((dB - mindb) / full_scale);
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
var specavg = [];

function waterfall_add(data_raw)
{
	if (data_raw == null) return;
	//var canvas = wf_canvases[0];
	var canvas = wf_cur_canvas;
	if (canvas == null) return;
	
	var u32View = new Uint32Array(data_raw, 4, 3);
	var x_bin_server = u32View[0];		// bin & zoom from server at time data was queued
	var u32 = u32View[1];
	if (kiwi_gc_wf) u32View = null;	// gc
	var x_zoom_server = u32 & 0xffff;
	var flags = (u32 >> 16) & 0xffff;
	var wf_flags = { COMPRESSED:1 };

	var data_arr_u8 = new Uint8Array(data_raw, 16);	// unsigned dBm values, converted to signed later on
	var bytes = data_arr_u8.length;
	var w = wf_fft_size;

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
	
	var data, decomp_data = null;
	if (flags & wf_flags.COMPRESSED) {
		decomp_data = new Uint8Array(bytes*2);
		var wf_adpcm = { index:0, previousValue:0 };
		decode_ima_adpcm_e8_u8(data_arr_u8, decomp_data, bytes, wf_adpcm);
		var ADPCM_PAD = 10;
		data = decomp_data.subarray(ADPCM_PAD);
	} else {
		data = data_arr_u8;
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
			var y = Math.round(band.norm * sh);
			spectrum_ctx.fillRect(0,y,sw,1);
		}

		if (clear_specavg) {
			for (var x=0; x<sw; x++) {
				specavg[x] = spectrum_color_index(data[x]);
			}
			clear_specavg = false;
		}
		
		// if necessary, draw scale on right side
		if (redraw_spectrum_dB_scale) {
		
			// set sidebar background where the dB text will go
			/*
			spectrum_ctx.fillStyle = "black";
			spectrum_ctx.fillRect(sw,0,tw,sh);
			*/
			for (var x = sw; x < spectrum_canvas.width; x++) {
				spectrum_ctx.putImageData(spectrum_colormap_transparent, x, 0, 0, 0, 1, sh);
			}
			
			// the dB scale text
			spectrum_ctx.fillStyle = "white";
			for (var i=0; i < dB_bands.length; i++) {
				var band = dB_bands[i];
				var y = Math.round(band.norm * sh);
				spectrum_ctx.fillText(band.dB, sw+3, y-4);
				//console.log("SP x="+sw+" y="+y+' '+dB);
			}
			redraw_spectrum_dB_scale = false;
		}
	}
	
	// Add line to waterfall image			
	
	var oneline_image = canvas.oneline_image;
	var zwf, color;
	
	if (spectrum_display && need_spectrum_update) {
		for (var x=0; x<w; x++) {
			// wf
			zwf = waterfall_color_index(data[x]);
			color = color_map[zwf];
			for (var i=0; i<4; i++)
				oneline_image.data[x*4+i] = ((color>>>0) >> ((3-i)*8)) & 0xff;
			
			// spectrum
			if (x < sw) {			
				var ysp = spectrum_color_index(data[x]);

				if (spectrum_filtered) {
					// non-linear spectrum filter from Rocky (Alex, VE3NEA)
					// see http://www.dxatlas.com/rocky/advanced.asp
					var iir_gain = 1 - Math.exp(-0.2 * ysp/255)
					var z = specavg[x];
					z = z + iir_gain * (ysp - z);
					if (z > 255) z = 255; if (z < 0) z = 0;
					specavg[x] = z;
				} else {
					z = ysp;
					if (z > 255) z = 255; if (z < 0) z = 0;
				}

				// draw the spectrum based on the spectrum colormap which should
				// color the 10 dB bands appropriately
				var y = Math.round((1 - z/255) * sh);

				// fixme: could optimize amount of data moved like smeter
				spectrum_ctx.putImageData(spectrum_colormap, x, 0, 0, y, 1, sh-y+1);
			}
		}
	} else {
		for (var x=0; x<w; x++) {
			// wf
			zwf = waterfall_color_index(data[x]);
			color = color_map[zwf];
	
			for (var i=0; i<4; i++) {
				oneline_image.data[x*4+i] = ((color>>>0) >> ((3-i)*8)) & 0xff;
			}
		}
	}
	
	canvas.ctx.putImageData(oneline_image, 0, wf_canvas_actual_line);
	
	// If data from server hasn't caught up to our panning or zooming then fix it.
	// This code is tricky and full of corner-cases.
	
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

	// reset "select band" menu if freq is no longer inside band
	//console.log('page_scroll PBV='+ passband_visible() +' freq_car_Hz='+ freq_car_Hz);
	if (passband_visible() < 0)
		check_band(0);
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

	waterfall_queue.push({ data:what, seq:seq, spacing:spacing });
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
		console.log('WF Q'+ waterfall_queue.length);
		wf_dq_onesec = 0;
	}
	*/

	// demodulator must have been initialized before calling zoom_step()
	if (init_zoom && !init_zoom_set && demodulators[0]) {
		init_zoom = parseInt(init_zoom);
		if (init_zoom < 0 || init_zoom > zoom_levels_max) init_zoom = 0;
		//console.log("### init_zoom="+init_zoom);
		zoom_step(ext_zoom.ABS, init_zoom);
		init_zoom_set = true;
	}

	if (!waterfall_setup_done || waterfall_queue.length == 0) return;
	
	while (waterfall_queue.length != 0) {

		var seq = waterfall_queue[0].seq;
		var target = audio_sequence + waterfall_delay;
		if (seq > target) {
			//console.log('SEQ too soon: '+ seq +' > '+ target +' ('+ (seq - target) +')');
			return;		// too soon
		}

		var now = Date.now();
		if (seq == audio_sequence && now < (waterfall_last_out + waterfall_queue[0].spacing)) {
			//console.log('SEQ need spacing');
			return;		// need spacing
		}
	
		// seq < audio_sequence or seq == audio_sequence and spacing is okay
		waterfall_last_out = now;
		
		var data = waterfall_queue[0].data;
		if (kiwi_gc_wf) waterfall_queue[0].data = null;	// gc
		waterfall_queue.shift();
		waterfall_add(data);
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

function do_waterfall_index(db_value, sqrt)
{
	// What is transmitted over the network are unsigned 55..255 values (compressed) which
	// correspond to -200..0 dBm. Convert here to back to <= 0 signed dBm.
	// Done this way because -127 is the smallest value in an int8 which isn't enough
	// to hold our smallest dBm value and also we don't expect dBm values > 0 to be needed.
	//
	// If we map it the reverse way, (u1_t) 0..255 => 0..-255 dBm (which is more natural), then we get
	// noise in the bottom bins due to funny interaction of the reversed values with the
	// ADPCM compression for reasons we don't understand.
	
	if (db_value < 0) db_value = 0;
	if (db_value > 255) db_value = 255;
	db_value = -(255 - db_value);
	db_value += cfg.waterfall_cal;

	if (db_value < mindb) db_value = mindb;
	if (db_value > maxdb) db_value = maxdb;
	var relative_value = db_value - mindb;

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
	
/*
window.setInterval(function(){ 
	sum=0;
	for (var i=0;i<audio_received.length;i++)
		sum+=audio_received[i].length;
	divlog("audio buffer bytes: "+sum);
}, 2000);*/


// ========================================================
// =======================  >UI  ==========================
// ========================================================

function line_stroke(ctx, vert, linew, color, x1,y1,x2,y2)
{
	/*
	ctx.lineWidth = linew;
	ctx.strokeStyle = color;
	ctx.beginPath();
	ctx.moveTo(x1, y1);
	ctx.lineTo(x2, y2);
	ctx.stroke();
	*/
	var w = vert? linew : x2 - x1 + 1;
	var h = vert? y2 - y1 + 1 : linew;
	var x = x1 - (vert? linew/2 : 0);
	var y = y1 - (vert? 0 : linew/2);
	ctx.fillStyle = color;
	ctx.fillRect(x,y,w,h);
}

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

function freqmode_set_dsp_kHz(fdsp, mode)
{
	fdsp *= 1000;
	//console.log("freqmode_set_dsp_kHz: fdsp="+fdsp+' mode='+mode);

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
	//console.log("freqset_car_Hz: fcar="+fcar);
	if (isNaN(fcar)) kiwi_trace();
	freq_car_Hz = fcar;
	//console.log("freqset_car_Hz: NEW freq_car_Hz="+fcar);
}

function freqset_update_ui()
{
	//console.log("FUPD-UI demod-ofreq="+(center_freq + demodulators[0].offset_frequency));
	//kiwi_trace();
	freq_displayed_Hz = freq_car_to_dsp(freq_car_Hz);
	freq_displayed_kHz_str = (freq_displayed_Hz/1000).toFixed(2);
	//console.log("FUPD-UI freq_car_Hz="+freq_car_Hz+' NEW freq_displayed_Hz='+freq_displayed_Hz);
	
	if (!waterfall_setup_done) return;
	
	var obj = w3_el_id('id-freq-input');
	if (typeof obj == "undefined" || obj == null) return;		// can happen if SND comes up long before W/F
	obj.value = freq_displayed_kHz_str;
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
	check_band(freq_car_Hz);
	
	writeCookie('last_freq', freq_displayed_kHz_str);
	freq_step_update_ui();
	freq_link_update();
}

function freqset_select(id)
{
	w3_field_select('id-freq-input', kiwi_isMobile()? false : true);
}

var last_mode_obj = null;

function modeset_update_ui(mode)
{
	if (last_mode_obj != null) last_mode_obj.style.color = "white";
	
	// if sound comes up before waterfall then the button won't be there
	var obj = w3_el_id('button-'+mode);
	if (obj && obj.style) obj.style.color = "lime";
	last_mode_obj = obj;
	setup_slider_one();
	writeCookie('last_mode', mode);
	freq_link_update();
}

// delay the UI updates called from the audio path until the waterfall UI setup is done
function try_freqset_update_ui()
{
	if (waterfall_setup_done) {
		freqset_update_ui();
		mkenvelopes(get_visible_freq_range());
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
	var el = w3_el_id('id-freq-link');
	el.innerHTML =
	   '<a href="'+ url +'" target="_blank" title="'+ url +'">' +
		   w3_icon('', 'fa-external-link-square') +
		'</a>';
}

function freqset_complete(timeout)
{
	kiwi_clearTimeout(freqset_tout);
	var obj = w3_el_id('id-freq-input');
	//console.log("FCMPL t/o="+ timeout +" obj="+(typeof obj)+" val="+(obj.value).toString());
	if (typeof obj == "undefined" || obj == null) return;		// can happen if SND comes up long before W/F
	var f = parseFloat(obj.value.replace(',', '.'));		// Thanks Petri, OH1BDF
	//console.log("FCMPL2 obj="+(typeof obj)+" val="+(obj.value).toString());
	if (f > 0 && !isNaN(f)) {
		freqmode_set_dsp_kHz(f, null);
		w3_field_select(obj, kiwi_isMobile()? false : true);
	}
}

var ignore_next_keyup_event = false;

// freqset_keyup is called on each key-up while the frequency box is selected so that if a numeric
// entry is made, without a terminating <return> key, a setTimeout(freqset_complete()) can be done to
// arrange automatic completion.

function freqset_keyup(obj, evt)
{
	kiwi_clearTimeout(freqset_tout);
	//console.log("FKU obj="+(typeof obj)+" val="+obj.value); console.log(obj); console.log(evt);
	
	// Ignore modifier-key key-up events triggered because the frequency box is selected while
	// modifier-key-including mouse event occurs somewhere else.
	
	// But this is tricky. Key-up of a shift/ctrl/alt/cmd key can only be detected by looking for a
	// evt.key string length != 1, i.e. evt.shiftKey et al don't seem to be valid for the key-up event!
	// But also have to check for them with any_alternate_click_event() in case a modifier key of a
	// normal key is pressed (e.g. shift-$).
	//if (ignore_next_keyup_event) {
	if (evt != undefined && evt.key != undefined) {
		var klen = evt.key.length;
		if (any_alternate_click_event(evt) || klen != 1) {
	
			// An escape while the the freq box has focus causes the browser to put input value back to the
			// last entered value directly by keyboard. This value is likely different than what was set by
			// the last "element.value = ..." assigned from a waterfall click. So we have to restore the value.
			if (evt.key == 'Escape') {
				//console.log('** restore freq box');
				freqset_update_ui();
			}
	
			//console.log('FKU IGNORE ign='+ ignore_next_keyup_event +' klen='+ klen);
			ignore_next_keyup_event = false;
			return;
		}
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
	console.log(s);
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
		console.log("STEP -400/1000 kHz="+kHz+" trunc="+trunc+" fnew="+fnew);
	} else
	if (freq_displayed_Hz != trunc) {
		fnew = trunc;
		took = 'TRUNC';
	} else {
		fnew += step_Hz;
		took = 'INC';
	}
	console.log('STEP '+sel+' '+cur_mode+' fold='+freq_displayed_Hz+' inc='+incHz+' trunc='+trunc+' fnew='+fnew+' '+took);
	freqmode_set_dsp_kHz(fnew/1000, null);
}

var freq_step_last_mode, freq_step_last_band;

function freq_step_update_ui(force)
{
	if (typeof cur_mode == "undefined") return;
	var b = find_band(freq_displayed_Hz);
	
	//console.log("freq_step_update_ui: lm="+freq_step_last_mode+' cm='+cur_mode);
	if (!force && freq_step_last_mode == cur_mode && freq_step_last_band == b) {
		//console.log("freq_step_update_ui: return "+freq_step_last_mode+' '+cur_mode);
		return;
	}

	var show_9_10 = (b != null && (b.name == 'LW' || b.name == 'MW') &&
		(cur_mode == 'am' || cur_mode == 'amn' || cur_mode == 'lsb' || cur_mode == 'usb'))? true:false;
	visible_inline('id-9-10-cell', show_9_10);

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
				band_ctx.font = "bold 11px sans-serif";
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

function select_band(op)
{
	b = band_menu[op];
	if (b == null) {
		w3_select_value('select-band', 0);
		last_selected_band = 0;
		return;
	}
	var freq, mode = null;
	if (typeof b.sel != "undefined") {
		freq = parseFloat(b.sel);
		if (typeof b.sel == "string") {
			mode = b.sel.search(/[a-z]/i);
			mode = (mode == -1)? null : b.sel.slice(mode);
		}
	} else {
		freq = b.cf/1000;
	}

	//console.log("SEL BAND"+op+" "+b.name+" freq="+freq+((mode != null)? " mode="+mode:""));
	last_selected_band = op;
	if (dbgUs) {
		console.log("SET BAND cur z="+zoom_level+" xb="+x_bin);
		sb_trace=0;
	}
	freqmode_set_dsp_kHz(freq, mode);
	zoom_step(ext_zoom.TO_BAND, b);		// pass band to disambiguate nested bands in band menu
	if (sb_trace) {
		console.log("SET BAND after z="+zoom_level+" xb="+x_bin);
	}
}

function check_band(freq)
{
	// reset "select band" menu if freq is no longer inside band
	if (last_selected_band) {
		band = band_menu[last_selected_band];
		//console.log("check_band "+last_selected_band+" f="+freq+' '+band.min+'/'+band.max);
		if (freq < band.min || freq > band.max) {
			w3_select_value('select-band', 0);
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

var muted = false;
var volume = 50;
var f_volume = muted? 0 : volume/100;


////////////////////////////////
// confirmation panel
////////////////////////////////

function confirmation_panel_init()
{
   w3_el_id('id-panels-container').innerHTML +=
      '<div id="id-confirmation" class="class-panel" data-panel-name="confirmation" data-panel-pos="center" data-panel-order="0" data-panel-size="600,100"></div>';

	var el = w3_el_id('id-confirmation');
	el.innerHTML =
		w3_divs('id-confirmation-container', 'class-panel-inner', '') +
		w3_divs('id-confirmation-vis class-vis', '');
}

function confirmation_panel_resize(w, h)
{
	var el = w3_el_id('id-confirmation');
	panel_set_width_height('id-confirmation', w, h);
   el.style.left = px(window.innerWidth/2 - el.activeWidth/2);
   el.style.bottom = px(window.innerHeight/2 - el.uiHeight/2);
}

function confirmation_hook_close(id, func)
{
	w3_el_id('id-confirmation-close').onclick = func;     // hook the close icon
	
   var el = w3_el_id(id);
	el.addEventListener('keyup', function(evt) {
		//event_dump(evt, 'EXT');
		if (evt.key == 'Escape' && evt.target.nodeName == 'INPUT') {
	      func();
	   }
	}, false);
}


////////////////////////////////
// cal ADC clock confirmation panel
////////////////////////////////

var cal_adc_new_adj;

function cal_adc_dialog(new_adj, clk_diff, r1k, ppm)
{
   //console.log('cal_adc_confirm');
   cal_adc_new_adj = new_adj;
   
	w3_el_id('id-confirmation-container').innerHTML =
		w3_col_percent('', 'w3-vcenter',
		   w3_div('w3-inline', 'ADC clock will be adjusted by<br>'+ clk_diff +' Hz to '+ r1k +' kHz<br>' +
		      '(ADC clock '+ ppm.toFixed(1) +' ppm)') +
         w3_button('w3-green|margin-left:16px', 'Confirm', 'cal_adc_confirm') +
         w3_button('w3-red|margin-left:16px', 'Cancel', 'cal_adc_cancel'),
		   80
		);
	
	confirmation_hook_close('id-confirmation', cal_adc_cancel);
	
	var el = w3_el_id('id-confirmation');
	el.style.zIndex = 1020;
	confirmation_panel_resize(525, 70);
   toggle_panel('id-confirmation');
}

function cal_adc_confirm()
{
   ext_send('SET clk_adj='+ cal_adc_new_adj);
   ext_set_cfg_param('cfg.clk_adj', cal_adc_new_adj, true);
   toggle_panel('id-confirmation');
}

function cal_adc_cancel()
{
	var el = w3_el_id('id-confirmation');
   toggle_panel('id-confirmation');
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

	html('id-confirmation-container').innerHTML =
		w3_col_percent('', 'w3-text-aqua',
			w3_input('Admin password', 'admin.pwd', '', 'admin_pwd_cb2', 'admin password required'), 80
		);

	confirmation_hook_close('id-confirmation', admin_pwd_cancel);

	var el = html('id-confirmation');
	el.style.zIndex = 1020;
	confirmation_panel_resize(525, 80);
   toggle_panel('id-confirmation');

	// put the cursor in (i.e. select) the password field
	w3_field_select('id-admin.pwd', kiwi_isMobile()? false : true);
}

function admin_pwd_cancel()
{
   toggle_panel('id-confirmation');
}

function admin_pwd_cb2(el, val)
{
   w3_string_cb(el, val);
   toggle_panel('id-confirmation');
	ext_valpwd('admin', val, ws_wf);
}


////////////////////////////////
// ext panel
////////////////////////////////

function ext_panel_init()
{
   w3_el_id('id-panels-container').innerHTML +=
      '<div id="id-ext-controls" class="class-panel" data-panel-name="ext-controls" data-panel-pos="bottom-left" data-panel-order="0" data-panel-size="525,300"></div>';

	var el = html('id-ext-data-container');
	el.style.zIndex = 100;

	el = html('id-ext-controls');
	el.innerHTML =
		w3_divs('id-ext-controls-container', 'class-panel-inner', '') +
		w3_divs('id-ext-controls-vis class-vis', '');
	
	// close ext panel if escape key while input field has focus
	el.addEventListener("keyup", function(evt) {
		//event_dump(evt, 'EXT');
		if (evt.key == 'Escape' && evt.target.nodeName == 'INPUT')
			extint_panel_hide();
	}, false);
}

var extint_using_data_container = false;

function extint_panel_show(controls_html, data_html, show_func)
{
	extint_using_data_container = (data_html != null);
	//console.log('extint_panel_show using_data_container='+ extint_using_data_container);

	if (extint_using_data_container) {
		toggle_or_set_spec(0);
		html('id-ext-data-container').innerHTML = data_html;
		html('id-ext-data-container').style.display = 'block';
		html('id-top-container').style.display = 'none';
	} else {
		html('id-ext-data-container').style.display = 'none';
		if (!spectrum_display)
			html('id-top-container').style.display = 'block';
	}

	// hook the close icon to call extint_panel_hide()
	var el = html('id-ext-controls-close');
	el.onclick = function() { toggle_panel("ext-controls"); extint_panel_hide(); };
	//console.log('extint_panel_show onclick='+ el.onclick);
	
	var el = html('id-ext-controls-container');
	el.innerHTML = controls_html;
	//console.log(controls_html);
	
	if (show_func) show_func();
	
	el = html('id-ext-controls');
	el.style.zIndex = 150;
	//el.style.top = px((extint_using_data_container? height_spectrum_canvas : height_top_bar_parts) +157+10);
	el.style.visibility = 'visible';
	html('id-msgs').style.visibility = 'hidden';
}

function extint_panel_hide()
{
	//console.log('extint_panel_hide using_data_container='+ extint_using_data_container);

	if (extint_using_data_container) {
		html('id-ext-data-container').style.display = 'none';
		html('id-top-container').style.display = 'block';
		extint_using_data_container = false;
	}
	
	html('id-ext-controls').style.visibility = 'hidden';
	html('id-msgs').style.visibility = 'visible';
	
	extint_blur_prev();
	
	// on close, reset extension menu
	w3_select_value('select-ext', -1);
	
   freqset_select();
}


////////////////////////////////
// dx (markers)
////////////////////////////////

var dx_update_delay = 350;
var dx_update_timeout, dx_seq=0, dx_idx, dx_z;

function dx_schedule_update()
{
	kiwi_clearTimeout(dx_update_timeout);
	dx_div.innerHTML = "";
	dx_update_timeout = setTimeout(dx_update, dx_update_delay);
}

function dx_update()
{
	dx_seq++;
	dx_idx = 0; dx_z = 120;
	//g_range = get_visible_freq_range();
	//console.log("DX min="+(g_range.start/1000)+" max="+(g_range.end/1000));
	wf_send('SET MKR min='+ (g_range.start/1000).toFixed(3) +' max='+ (g_range.end/1000).toFixed(3) +' zoom='+ zoom_level +' width='+ waterfall_width);
}

// Why doesn't using addEventListener() to ignore mousedown et al not seem to work for
// div elements created appending to innerHTML?

var DX_MODE = 0xf;

var DX_TYPE = 0xf0;
var DX_TYPE_SFT = 4;
var types = { 0:'active', 1:'watch-list', 2:'sub-band', 3:'DGPS', 4:'NoN' , 5:'interference' };
var types_s = { active:0, watch_list:1, sub_band:2, DGPS:3, NoN:4 , interference:5 };
var type_colors = { 0:'cyan', 0x10:'lightPink', 0x20:'aquamarine', 0x30:'lavender', 0x40:'violet' , 0x50:'violet' };

var DX_FLAG = 0xff00;

var dx_ibp_list, dx_ibp_interval, dx_ibp_server_time_ms, dx_ibp_local_time_epoch_ms = 0;
var dx_ibp_freqs = { 14:0, 18:1, 21:2, 24:3, 28:4 };

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
var dx_ibp_lastsec = 0;

function dx(arr)
{
	var i;
	
	var obj = arr[0];
	dx_idx = 0; dx_z = 120;
	dx_div.innerHTML = '';
	kiwi_clearInterval(dx_ibp_interval);
	dx_ibp_list = [];
	dx_ibp_server_time_ms = obj.t * 1000;
	dx_ibp_local_time_epoch_ms = Date.now();
	
	
	for (i = 1; i < arr.length; i++) {
		obj = arr[i];
		var gid = obj.g;
		var freq = obj.f;
		var moff = obj.o;
		var flags = obj.b;
		var ident = obj.i;
		var notes = (typeof obj.n != 'undefined')? obj.n : '';

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
		dx_list[gid] = { "gid":gid, "carrier":carrier, "freq":freq, "moff":moff, "flags":flags, "ident":ident, "notes":notes };
		//console.log(dx_list[gid]);
		
		var s =
			'<div id="'+dx_idx+'-id-dx-label" class="class-dx-label '+ gid +'-id-dx-gid" style="left:'+(x-10)+'px; top:'+t+'px; z-index:'+dx_z+'; ' +
				'background-color:'+ color +';" ' +
				
				// overrides underlying canvas listeners for the dx labels
				'onmouseenter="dx_enter(this,'+ cmkr_x +')" onmouseleave="dx_leave(this,'+ cmkr_x +')" ' +
				'onmousedown="ignore(event)" onmousemove="ignore(event)" onmouseup="ignore(event)" ontouchmove="ignore(event)" ontouchend="ignore(event)" ' +
				'onclick="dx_click(event,'+ gid +')" ontouchstart="dx_click(event,'+ gid +')">' +
			'</div>' +
			'<div class="class-dx-line" id="'+dx_idx+'-id-dx-line" style="left:'+x+'px; top:'+t+'px; height:'+h+'px; z-index:110"></div>';
		//console.log(s);
	
		dx_div.innerHTML += s;
		var el = w3_el_id(dx_idx +'-id-dx-label');
		
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
						var el = w3_el_id(dx_ibp_list[i].idx +'-id-dx-label');
						if (el) el.innerHTML = 'IBP: '+ dx_ibp[s*2] +' '+ dx_ibp[s*2+1];
					}
				}
				dx_ibp_lastsec = rsec;
			}, 500);
		}
		
		dx_idx++; dx_z++;
	}
}

var dxo = { };
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
		dxo.o = 0;
		dxo.m = modes_s[cur_mode];
		dxo.y = types_s.active;
		dxo.i = dxo.n = '';
	} else {
		//console.log('DX EDIT entry #'+ gid +' prev: f='+ dx_list[gid].freq +' flags='+ dx_list[gid].flags.toHex() +' i='+ dx_list[gid].ident +' n='+ dx_list[gid].notes);
		dxo.f = dx_list[gid].carrier.toFixed(2);		// starts as a string, validated to be a number
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
		wf_send('SET DX_UPD g='+ dxo.gid +' f='+ dxo.f +' o='+ dxo.o +' m='+ mode +
			' i='+ encodeURIComponent(dxo.i +'x') +' n='+ encodeURIComponent(dxo.n +'x'));
		return;
	}

	extint_panel_hide();		// committed to displaying edit panel, so remove any ext panel
	resize_waterfall_container(true);	// necessary if an ext was present so wf canvas size stays correct

	var s =
		w3_divs('w3-rest w3-text-aqua', 'w3-margin-top',
			w3_col_percent('', 'w3-hspace-8',
				w3_input('Freq', 'dxo.f', dxo.f, 'dx_num_cb'), 30,
				w3_select('Mode', '', 'dxo.m', dxo.m, modes_u, 'dx_sel_cb'), 15,
				w3_select('Type', '', 'dxo.y', dxo.y, types, 'dx_sel_cb'), 25,
				w3_input('Offset', 'dxo.o', dxo.o, 'dx_num_cb'), 25	// wraps if 30% used (100% total), why?
			),
		
			w3_input('Ident', 'dxo.i', '', 'dx_string_cb'),
			w3_input('Notes', 'dxo.n', '', 'dx_string_cb'),
		
			w3_divs('', 'w3-show-inline-block w3-hspace-16',
				w3_button('', 'Modify', 'dx_modify_cb'),
				w3_button('', 'Add', 'dx_add_cb'),
				w3_button('', 'Delete', 'dx_delete_cb')
			)
		);
	
	// can't do this as initial val passed to w3_input above when string contains quoting
	ext_panel_show(s, null, function() {
		var el = w3_el_id('dxo.i');
		el.value = dxo.i;
		w3_el_id('dxo.n').value = dxo.n;
		
		// change focus to input field
		// FIXME: why does this work after pwd panel, but not otherwise?
		//console.log('el.value='+ el.value);
		w3_field_select(el, kiwi_isMobile()? false : true);
	});
}

/*
	FIXME input validation issues:
		data out-of-range
		data missing
		what should it mean? delete button, but params have been changed (e.g. freq)
		SECURITY: can eval arbitrary code input?
*/

function dx_num_cb(el, val)
{
	w3_num_cb(el, val);
}

function dx_sel_cb(el, val)
{
	w3_string_cb(el, val);
}

function dx_string_cb(el, val)
{
	w3_string_cb(el, val);
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
	var mode = dxo.m;
	var type = dxo.y << DX_TYPE_SFT;
	mode |= type;
	wf_send('SET DX_UPD g='+ dxo.gid +' f='+ dxo.f +' o='+ dxo.o +' m='+ mode +
		' i='+ encodeURIComponent(dxo.i +'x') +' n='+ encodeURIComponent(dxo.n +'x'));
	setTimeout(function() {dx_close_edit_panel(id);}, 250);
}

function dx_add_cb(id, val)
{
	//console.log('DX COMMIT new entry');
	//console.log(dxo);
	var mode = dxo.m;
	var type = dxo.y << DX_TYPE_SFT;
	mode |= type;
	wf_send('SET DX_UPD g=-1 f='+ dxo.f +' o='+ dxo.o +' m='+ mode +
		' i='+ encodeURIComponent(dxo.i +'x') +' n='+ encodeURIComponent(dxo.n +'x'));
	setTimeout(function() {dx_close_edit_panel(id);}, 250);
}

function dx_delete_cb(id, val)
{
	//console.log('DX COMMIT delete entry #'+ dxo.gid);
	//console.log(dxo);
	wf_send('SET DX_UPD g='+ dxo.gid +' f=-1');
	setTimeout(function() {dx_close_edit_panel(id);}, 250);
}

function dx_click(ev, gid)
{
	if (ev.shiftKey) {
		dx_show_edit_panel(ev, gid);
	} else {
		mode = modes_l[dx_list[gid].flags & DX_MODE];
		//console.log("DX-click f="+ dx_list[gid].freq +" mode="+ mode +" cur_mode="+ cur_mode);
		freqmode_set_dsp_kHz(dx_list[gid].freq, mode);
	}
	return cancelEvent(ev);		// keep underlying canvas from getting event
}

// Any var we add to the div in dx() is undefined in the div that appears here,
// so use the number embedded in id="" to create reference.
// Even the "data-" thing doesn't work.

var dx_z_save, dx_bg_color_save;

function dx_enter(dx, cmkr_x)
{
	dx_z_save = dx.style.zIndex;
	dx.style.zIndex = 999;
	dx_bg_color_save = dx.style.backgroundColor;
	dx.style.backgroundColor = 'yellow';
	var dx_line = html(parseInt(dx.id)+'-id-dx-line');
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
	var dx_line = html(parseInt(dx.id)+'-id-dx-line');
	dx_line.zIndex = 110;
	dx_line.style.width = '1px';
	
	if (cmkr_x) {
		dx_canvas.style.zIndex = 99;
	}
}


////////////////////////////////
// smeter
////////////////////////////////

var smeter_width;
var SMETER_SCALE_HEIGHT = 29;
var SMETER_BIAS = 127;
var SMETER_MAX = 3.4;
var sMeter_dBm_biased = 0;
var sMeter_ctx;
var smeter_ovfl;

// 6 dB / S-unit
var bars = {
	dBm: [	-121,	-109,	-97,	-85,	-73,	-63,		-53,		-33,		-13],
	text: [	'S1',	'S3',	'S5',	'S7',	'S9',	'+10',	'+20',	'+40',	'+60']
};

function smeter_dBm_biased_to_x(dBm_biased)
{
	return Math.round(dBm_biased / (SMETER_MAX + SMETER_BIAS) * smeter_width);
}

function smeter_init()
{
	html('id-control-smeter').innerHTML =
		'<canvas id="id-smeter-scale" class="class-smeter-scale" width="0" height="0"></canvas>' +
		w3_divs('id-smeter-ovfl', '', 'OV');
	var sMeter_canvas = html('id-smeter-scale');
	
	smeter_ovfl = w3_el_id('smeter-ovfl');

	smeter_width = divControl.activeWidth - html_LR_border_pad(sMeter_canvas);		// less our own border/padding
	
	var w = smeter_width, h = SMETER_SCALE_HEIGHT, y=h-8;
	sMeter_ctx = sMeter_canvas.getContext("2d");
	sMeter_ctx.canvas.width = w;
	sMeter_ctx.canvas.height = h;
	sMeter_ctx.fillStyle = "gray";
	sMeter_ctx.globalAlpha = 1;
	sMeter_ctx.fillRect(0,0,w,h);

	sMeter_ctx.font = "10px sans-serif";
	sMeter_ctx.textBaseline = "middle";
	sMeter_ctx.textAlign = "center";
	
	sMeter_ctx.fillStyle = "white";
	for (var i=0; i < bars.text.length; i++) {
		var x = smeter_dBm_biased_to_x(bars.dBm[i] + SMETER_BIAS);
		line_stroke(sMeter_ctx, 1, 3, "white", x,y-8,x,y+8);
		sMeter_ctx.fillText(bars.text[i], x, y-14);
		//console.log("SM x="+x+' dBm='+bars.dBm[i]+' '+bars.text[i]);
	}

	line_stroke(sMeter_ctx, 0, 5, "black", 0,y,w,y);
	setInterval(update_smeter, 100);
}

var sm_px = 0, sm_timeout = 0, sm_interval = 10;

function update_smeter()
{
	var x = smeter_dBm_biased_to_x(sMeter_dBm_biased);
	var y = SMETER_SCALE_HEIGHT-8;
	sMeter_ctx.globalAlpha = 1;
	line_stroke(sMeter_ctx, 0, 5, "lime", 0,y,x,y);
	
	if (sm_timeout-- == 0) {
		sm_timeout = sm_interval;
		if (x < sm_px) line_stroke(sMeter_ctx, 0, 5, "black", x,y,sm_px,y);
		//if (x < sm_px) line_stroke(sMeter_ctx, 0, 5, "black", x,y,smeter_width,y);
		sm_px = x;
	} else {
		if (x < sm_px) {
			line_stroke(sMeter_ctx, 0, 5, "red", x,y,sm_px,y);
		} else {
			sm_px = x;
			sm_timeout = sm_interval;
		}
	}
	
	smeter_ovfl.style.visibility = audio_adc_ovfl? 'visible':'hidden';
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
	html('input-ident').value = ident;
	ident_user = ident;
	need_ident = true;
	//console.log('ident_init: SET ident='+ ident_user);
}

function ident_complete()
{
	kiwi_clearTimeout(ident_tout);
	var obj = html('input-ident');
	var ident = obj.value;
	ident = kiwi_strip_tags(ident, '');
	obj.value = ident;
	console.log('ICMPL obj='+ (typeof obj) +' ident_user=<'+ ident +'>');
	// okay for ident='' to erase it
	// SECURITY: size limited by <input size=...> but guard against binary data injection?
	w3_field_select(obj, kiwi_isMobile()? false : true);
	writeCookie('ident', ident);
	ident_user = ident;
	need_ident = true;
	//console.log('ident_complete: SET ident_user='+ ident_user);
}

function ident_keyup(obj, evt)
{
	kiwi_clearTimeout(ident_tout);
	//console.log("IKU obj="+(typeof obj)+" val="+obj.value);
	
	// ignore modifier keys used with mouse events that also appear here
	if (ignore_next_keyup_event) {
		//console.log("ignore shift-key ident_keyup");
		ignore_next_keyup_event = false;
		return;
	}
	
	ident_tout = setTimeout(ident_complete, 5000);
}


// ========================================================
// =======================  >PANELS  ======================
// ========================================================

var panel_margin = 10;

// called from waterfall_init()
function panels_setup()
{
	var td = function(inner, _id) {
		var id = (typeof _id != "undefined")? 'id="'+ _id +'"' : '';
		var td = '<div '+ id +' class="class-td">'+ inner +'</div>';
		return td;
	}

	// id-control
	
	html("id-ident").innerHTML =
		'<form name="form_ident" action="#" onsubmit="ident_complete(); return false;">' +
			'Your name or callsign: <input id="input-ident" type="text" size=32 onkeyup="ident_keyup(this, event);">' +
		'</form>';
	
	html("id-control-1").innerHTML =
		td('<form id="id-freq-form" name="form_freq" action="#" onsubmit="freqset_complete(0); return false;">' +
			'<input id="id-freq-input" type="text" size=8 onkeyup="freqset_keyup(this, event);">' +
			'</form>', 'id-freq-cell') +

		td('<select id="select-band" onchange="select_band(this.value)">' +
				'<option value="0" selected disabled>select band</option>' +
				setup_band_menu() +
			'</select>', 'select-band-cell') +

		td('<select id="select-ext" onchange="freqset_select(); extint_select(this.value)">' +
				'<option value="-1" selected disabled>extension</option>' +
				extint_select_menu() +
			'</select>', 'select-ext-cell');

	if (kiwi_is_iOS()) {
	//if (true) {
		html('id-iOS-container').style.display = "table-cell";
		var el = html('id-iOS-play-button');
		el.style.marginTop = px(w3_center_in_window(el));
	}
	
	html("id-control-2").innerHTML =
		'<div id="id-mouse-freq" class="class-td"><span id="id-mouse-unit">-----.--</span></div>' +
		td('<div id="id-freq-link"></div>', 'id-link-cell') +
		td('<div id="id-button-9-10" class="class-button-small" title="LW/MW 9/10 kHz tuning step" onclick="button_9_10()">10</div>', 'id-9-10-cell') +
		'<div id="id-step-freq" class="class-td">' +
			'<img id="id-step-0" src="icons/stepdn.20.png" onclick="freqstep(0)" />' +
			'<img id="id-step-1" src="icons/stepdn.18.png" onclick="freqstep(1)" style="padding-bottom:1px" />' +
			'<img id="id-step-2" src="icons/stepdn.16.png" onclick="freqstep(2)" style="padding-bottom:2px" />' +
			'<img id="id-step-3" src="icons/stepup.16.png" onclick="freqstep(3)" style="padding-bottom:2px" />' +
			'<img id="id-step-4" src="icons/stepup.18.png" onclick="freqstep(4)" style="padding-bottom:1px" />' +
			'<img id="id-step-5" src="icons/stepup.20.png" onclick="freqstep(5)" />' +
		'</div>' +
		td('<div id="button-spectrum" class="class-button" onclick="toggle_or_set_spec();">Spectrum</div>');
	
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

	html("id-control-3").innerHTML =
		td('<div id="button-am" class="class-button" onclick="mode_button(event, \'am\')" onmousedown="cancelEvent(event)" onmouseover="mode_over(event)">AM</div>') +
		td('<div id="button-amn" class="class-button" onclick="mode_button(event, \'amn\')" onmousedown="cancelEvent(event)" onmouseover="mode_over(event)">AMN</div>') +
		td('<div id="button-lsb" class="class-button" onclick="mode_button(event, \'lsb\')" onmousedown="cancelEvent(event)" onmouseover="mode_over(event)">LSB</div>') +
		td('<div id="button-usb" class="class-button" onclick="mode_button(event, \'usb\')" onmousedown="cancelEvent(event)" onmouseover="mode_over(event)">USB</div>') +
		td('<div id="button-cw" class="class-button" onclick="mode_button(event, \'cw\')" onmousedown="cancelEvent(event)" onmouseover="mode_over(event)">CW</div>') +
		td('<div id="button-cwn" class="class-button" onclick="mode_button(event, \'cwn\')" onmousedown="cancelEvent(event)" onmouseover="mode_over(event)">CWN</div>') +
		td('<div id="button-nbfm" class="class-button" onclick="mode_button(event, \'nbfm\')" onmousedown="cancelEvent(event)" onmouseover="mode_over(event)">NBFM</div>');

	html("id-control-4").innerHTML =
		td('<div class="class-icon" onclick="zoom_click(event,1)" onmouseover="zoom_over(event)" title="zoom in"><img src="icons/zoomin.png" width="32" height="32" /></div>', 'id-zoom-in') +
		td('<div class="class-icon" onclick="zoom_click(event,-1)" onmouseover="zoom_over(event)" title="zoom out"><img src="icons/zoomout.png" width="32" height="32" /></div>', 'id-zoom-out') +
		td('<div class="class-icon" onclick="zoom_click(event,8)" title="max in"><img src="icons/maxin.png" width="32" height="32" /></div>', 'id-maxin') +
		td('<div class="class-icon" onclick="zoom_click(event,8)" title="max in"><img src="icons/maxin.nom.png" width="32" height="32" /></div>', 'id-maxin-nom') +
		td('<div class="class-icon" onclick="zoom_click(event,8)" title="max in"><img src="icons/maxin.max.png" width="32" height="32" /></div>', 'id-maxin-max') +
		td('<div class="class-icon" onclick="zoom_click(event,-9)" title="max out"><img src="icons/maxout.png" width="32" height="32" /></div>', 'id-maxout') +
		td('<div class="class-icon" onclick="zoom_click(event,-9)" title="max out"><img src="icons/maxout.max.png" width="32" height="32" /></div>', 'id-maxout-max') +
		td('<div class="class-icon" onclick="zoom_click(event,0)" title="zoom to band">' +
			'<img src="icons/zoomband.png" width="32" height="16" style="padding-top:13px; padding-bottom:13px;"/>' +
		'</div>') +
		td('<div class="class-icon" onclick="page_scroll(-'+page_scroll_amount+')" title="page down"><img src="icons/pageleft.png" width="32" height="32" /></div>') +
		td('<div class="class-icon" onclick="page_scroll('+page_scroll_amount+')" title="page up"><img src="icons/pageright.png" width="32" height="32" /></div>');

	html("id-control-sliders").innerHTML =
		w3_col_percent('w3-vcenter', '',
			w3_divs('slider-one class-slider', ''), 70,
			w3_divs('slider-one-field class-slider', ''), 15,
			'<div id="id-button-pref" class="class-button" onclick="show_pref();" style="visibility:hidden">Pref</div>', 15
		) +
		w3_col_percent('w3-vcenter', '',
			w3_divs('slider-mindb class-slider', ''), 70,
			w3_divs('field-mindb class-slider', ''), 15,
			'<div id="id-button-func" class="class-button" onclick="toggle_or_set_func();" style="visibility:hidden">Func</div>', 15
		) +
		w3_col_percent('w3-vcenter', '',
			w3_divs('slider-volume class-slider', ''), 70,
			'<div id="id-button-mute" class="class-button" onclick="toggle_mute();">Mute</div>', 15,
			'<div id="id-button-more" class="class-button" onclick="toggle_or_set_more();">More</div>', 15
		);

	html('id-button-mute').style.color = muted? 'lime':'white';

	html('id-control-more').innerHTML =
		w3_col_percent('w3-vcenter', 'class-slider',
			'<div id="id-button-agc" class="class-button" onclick="toggle_agc(event)" onmousedown="cancelEvent(event)" onmouseover="agc_over(event)">AGC</div>', 13,
			'<div id="id-button-hang" class="class-button" onclick="toggle_or_set_hang();">Hang</div>', 17,
			w3_divs('w3-show-inline-block', 'label-man-gain', 'Manual<br>Gain '), 15,
			'<input id="input-man-gain" type="range" min="0" max="120" value="'+ manGain +'" step="1" onchange="setManGain(1,this.value)" oninput="setManGain(0,this.value)">', 40,
			w3_divs('field-man-gain w3-show-inline-block', '', manGain.toString()) +' dB', 15
		) +
		w3_divs('', '',
			w3_col_percent('w3-vcenter', 'class-slider',
				w3_divs('label-threshold w3-show-inline-block', '', 'Thresh '), 18,
				'<input id="input-threshold" type="range" min="-130" max="0" value="'+ thresh +'" step="1" onchange="setThresh(1,this.value)" oninput="setThresh(0,this.value)">', 52,
				w3_divs('field-threshold w3-show-inline-block', '', thresh.toString()) +' dB', 30
			),
			w3_col_percent('w3-vcenter', 'class-slider',
				w3_divs('label-slope w3-show-inline-block', '', 'Slope '), 18,
				'<input id="input-slope" type="range" min="0" max="10" value="'+ slope +'" step="1" onchange="setSlope(1,this.value)" oninput="setSlope(0,this.value)">', 52,
				w3_divs('field-slope w3-show-inline-block', '', slope.toString()) +' dB', 30
			),
			w3_col_percent('w3-vcenter', 'class-slider',
				w3_divs('label-decay w3-show-inline-block', '', 'Decay '), 18,
				'<input id="input-decay" type="range" min="20" max="5000" value="'+ decay +'" step="1" onchange="setDecay(1,this.value)" oninput="setDecay(0,this.value)">', 52,
				w3_divs('field-decay w3-show-inline-block', '', decay.toString()) +' msec', 30
			)
		);

	html('slider-mindb').innerHTML =
		w3_col_percent('w3-vcenter', '',
			'WF min', 25,
			'<input id="input-mindb" type="range" min="-190" max="-30" value="'+mindb+'" step="1" onchange="setmindb(1,this.value)" oninput="setmindb(0, this.value)">', 75
		);

	html('slider-volume').innerHTML =
		w3_col_percent('w3-vcenter', '',
			'Volume', 25,
			'<input id="input-volume" type="range" min="0" max="200" value="'+volume+'" step="1" onchange="setvolume(1, this.value)" oninput="setvolume(0, this.value)">', 75
		);

	setup_agc(toggle_e.FROM_COOKIES | toggle_e.SET);
	setup_slider_one();
	toggle_or_set_more(toggle_e.FROM_COOKIES | toggle_e.SET, 0);
	
	//jksx XDLS pref button
	if (dbgUs) w3_el_id('id-button-pref').style.visibility = 'visible';
	if (dbgUs) w3_el_id('id-button-func').style.visibility = 'visible';

	// id-news
	html('id-news').style.backgroundColor = news_color;
	html("id-news-inner").innerHTML =
		'<span style="font-size: 14pt; font-weight: bold;">' +
			'KiwiSDR now available on ' +
			'<a href="https://www.kickstarter.com/projects/1575992013/kiwisdr-beaglebone-software-defined-radio-sdr-with" target="_blank">' +
				'<img class="class-KS" src="icons/kickstarter-logo-light.150x18.png" />' +
			'</a>' +
		'</span>' +
		'';


	// id-readme
	
	html('id-readme').style.backgroundColor = readme_color;
	html("id-readme-inner").innerHTML =
		'<span style="font-size: 15pt; font-weight: bold;">Welcome! </span>' +
		'&nbsp&nbsp&nbsp Here are some tips: \
		<ul style="padding-left: 12px;"> \
		<li> Please <a href="javascript:sendmail(\'pvsslqwChjtjpgq-`ln\');">email us</a> \
			if your browser is having problems with the SDR. </li>\
		<li> Windows: Firefox & Chrome work; IE is still completely broken. </li>\
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

	html("id-msgs-inner").innerHTML =
		'<div id="id-client-log-title"><strong> Contacts: </strong>' +
		(admin_email? '<a href="javascript:sendmail(\''+ admin_email +'\');"><strong>Owner/Admin</strong></a>, ' : '') +
		'<a href="javascript:sendmail(\'pvsslqwChjtjpgq-`ln\');"><strong>KiwiSDR</strong></a>, ' +
		'<a href="javascript:sendmail(\'kb4jonCpgq-kv\');"><strong>OpenWebRX</strong></a> ' +
		'| <a href="https://kiwiirc.com/client/chat.freenode.net/#kiwisdr" target="_blank"><strong>Chat</strong></a> ' +
		'<span id="id-problems"></span></div>' +
		'<div id="id-status-msg"></div>' +
		'<span id="id-msg-config"></span><br/>' +
		'<span id="id-msg-gps"></span><br/>' +
		'<span id="id-msg-audio"></span><br/>' +
		'<div id="id-debugdiv"></div>';
}

var squelch_state = 0;

function setup_slider_one()
{
	var el = html('slider-one')
	if (cur_mode == 'nbfm') {
		if (el) el.innerHTML = 
			w3_col_percent('w3-vcenter', '',
				'<span id="id-squelch">Squelch </span>', 25,
				'<input id="slider-one-value" type="range" min="0" max="99" value="'+squelch+'" step="1" onchange="setsquelch(1,this.value)" oninput="setsquelch(1, this.value)">', 75
			);
		html('id-squelch').style.color = squelch_state? 'lime':'white';
		html('slider-one-field').innerHTML = squelch;
	} else {
		if (el) el.innerHTML =
			w3_col_percent('w3-vcenter', '',
				'WF max', 25,
				'<input id="slider-one-value" type="range" min="-100" max="20" value="'+maxdb+'" step="1" onchange="setmaxdb(1,this.value)" oninput="setmaxdb(0, this.value)">', 75
			);
		html('slider-one-field').innerHTML = maxdb + ' dB';
	}
}

var squelch = 0;

function setsquelch(done, str)
{
   squelch = parseFloat(str);
	html('slider-one-field').innerHTML = str;
   if (done) snd_send("SET squelch="+ squelch.toFixed(0) +' max='+ squelch_threshold.toFixed(0));
}

// Safari on iOS only plays webaudio after it has been started by clicking a button
function iOS_play_button()
{
	try {
		var actx = audio_context;
		var bufsrc = actx.createBufferSource();
   	bufsrc.connect(actx.destination);
   	try { bufsrc.start(0); } catch(ex) { bufsrc.noteOn(0); }
   } catch(ex) { add_problem("audio start"); }

	//visible_block('id-iOS-container', false);
	html('id-iOS-container').style.opacity = 0;
	setTimeout(function() { visible_block('id-iOS-container', false); }, 1100);
   freqset_select();
}

function zoomCorrection()
{
	return 3/*dB*/ * zoom_level;
	//return 0 * zoom_level;		// gives constant noise floor when using USE_GEN
}

function setmaxdb(done, str)
{
	var strdb = parseFloat(str);
	if (strdb <= mindb) {
		maxdb = mindb + 1;
		html('slider-one-value').value = maxdb;
		html('slider-one-field').innerHTML = maxdb.toFixed(0) + ' dB';
		html('slider-one-field').style.color = "red"; 
	} else {
		maxdb = strdb;
		if (cur_mode != 'nbfm') html('slider-one-field').innerHTML = strdb.toFixed(0) + ' dB';
		html('slider-one-field').style.color = "white"; 
		html('field-mindb').style.color = "white";
	}
	
	setmaxmindb(done);
}

function setmindb(done, str)
{
	var strdb = parseFloat(str);
	if (maxdb <= strdb) {
		mindb = maxdb - 1;
		html('input-mindb').value = mindb;
		html('field-mindb').innerHTML = mindb.toFixed(0) + ' dB';
		html('field-mindb').style.color = "red";
	} else {
		mindb = strdb;
		html('field-mindb').innerHTML = strdb.toFixed(0) + ' dB';
		html('field-mindb').style.color = "white";
		html('slider-one-field').style.color = "white"; 
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
   	writeCookie('last_max_dB', maxdb.toFixed(0));
   	writeCookie('last_min_dB', mindb_un.toFixed(0));	// need to save the uncorrected (z0) value
	}
}

function update_maxmindb_sliders()
{
	mindb = mindb_un - zoomCorrection();
	full_scale = maxdb - mindb;
	full_scale = full_scale? full_scale : 1;	// can't be zero
	spectrum_dB_bands();
	
	if (cur_mode != 'nbfm') {
		html('slider-one-value').value = maxdb;
		html('slider-one-field').innerHTML = maxdb.toFixed(0) + ' dB';
	}
	
   html('input-mindb').value = mindb;
   html('field-mindb').innerHTML = mindb.toFixed(0) + ' dB';
}

function setvolume(done, str)
{
   volume = parseFloat(str);
   f_volume = muted? 0 : volume/100;
   if (done) freqset_select();
}

function toggle_mute()
{
	muted ^= 1;
	var el = w3_el_id('id-button-mute');
	if (el) el.style.color = muted? 'lime':'white';
   f_volume = muted? 0 : volume/100;
   freqset_select();
}

var btn_func = 0;

function toggle_or_set_func(set, val)
{
	if (set != undefined)
		btn_func = kiwi_toggle(set, val, btn_func);
	else
		btn_func ^= 1;

	html('id-button-func').style.color = btn_func? 'lime':'white';
	freqset_select();
	
	if (dbgUs) {
	   snd_send('SET debug_v='+ btn_func);
	}
}

var more = 0;

function toggle_or_set_more(set, val)
{
	if (set != undefined)
		more = kiwi_toggle(set, val, more, 'last_more');
	else
		more ^= 1;

	html('id-button-more').style.color = more? 'lime':'white';
	if (more) {
		//divControl.style.top = '224px';
		//divControl.style.height = '490px';		// max
		divControl.style.height = '390px';
		visible_block('id-control-more', true);
	} else {
		visible_block('id-control-more', false);
		//divControl.style.top = 'auto';
		//divControl.style.bottom = 0;
		divControl.style.height = divControl.uiHeight +'px';
	}
	writeCookie('last_more', more.toString());
	freqset_select();
}

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
	w3_el_id('id-button-agc').title = any_alternate_click_event(evt)? 'restore AGC params':'';
}

var default_agc = 1;
var default_hang = 0;
var default_manGain = 50;
var default_thresh = -100;
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
		html('label-man-gain').style.color = 'white';
		html('id-button-hang').style.borderColor = html('label-threshold').style.color = html('label-slope').style.color = html('label-decay').style.color = 'orange';
	} else {
		html('label-man-gain').style.color = 'orange';
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

var spectrum_display = false;

function toggle_or_set_spec(set)
{
	// close the extension first if it's using the data container and the spectrum button is pressed
	if (extint_using_data_container && (set == 1 || (set == undefined && spectrum_display == 0))) {
		extint_panel_hide();
	}

	if (set != undefined) {
		spectrum_display = set;
	} else {
		spectrum_display ^= 1;
	}

	html('button-spectrum').style.color = spectrum_display? 'lime':'white';
	html('id-spectrum-container').style.display = spectrum_display? 'block':'none';
	html('id-top-container').style.display = spectrum_display? 'none':'block';
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

function mode_over(evt)
{
	for (m in modes_l) {
		w3_el_id('button-'+modes_l[m]).title = evt.shiftKey? 'restore passband':'';
	}
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

function mode_button(evt, mode)
{
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
	var el = html('id-button-9-10');
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
		if (c.className == "class-panel") {
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

function panel_setup_control(el)
{
	divControl = el;

	// un-more-expand control panel if escape key while input field has focus
	el.addEventListener("keyup", function(evt) {
		//event_dump(evt, 'PAR');
		if (evt.key == 'Escape' && evt.target.nodeName == 'INPUT')
			toggle_or_set_more(toggle_e.SET, 0);
	}, false);
	
	// make first line of controls full width less vis button
	w3_el_id('id-control-1').style.width = px(el.activeWidth - visIcon - 5);
}

function panel_set_vis_button(id)
{
	var el = w3_el_id(id);
	var vis = w3_el_id(id +'-vis');
	var visOffset = el.activeWidth - visIcon;
	//console.log('left='+ visOffset +' id='+ (id +'-vis') +' '+ el.activeWidth +' '+ visIcon +' '+ visBorder);
	vis.style.left = px(visOffset + visBorder);
}

function panel_set_width_height(id, width, height)
{
	var panel = w3_el_id(id);

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

// ========================================================
// =======================  >MISC  ========================
// ========================================================

function event_dump(evt, id)
{
	console.log('EVENT_DUMP: '+ id +' type='+ evt.type +' ----');
	console.log('id='+ this.id +' name='+ evt.target.nodeName +' tgtID='+ evt.target.id +' ctgtID='+ evt.currentTarget.id);
	console.log('sft='+evt.shiftKey+' alt='+evt.altKey+' ctrl='+evt.ctrlKey+' meta='+evt.metaKey+' key='+evt.key);
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

function add_problem(what, sticky)
{
	problems_span = html('id-problems');
	if (!problems_span) return;
	if (typeof problems_span.children != "undefined")
		for(var i=0;i<problems_span.children.length;i++) if(problems_span.children[i].innerHTML==what) return;
	new_span = document.createElement("span");
	new_span.innerHTML=what;
	problems_span.appendChild(new_span);
	if (sticky == undefined)
		window.setTimeout(function(ps,ns) { ps.removeChild(ns); }, 1000, problems_span, new_span);
}

function set_gen(freq, attn)
{
	snd_send("SET genattn="+ attn.toFixed(0));
	snd_send("SET gen="+ freq +" mix=-1");
}


// ========================================================
// =======================  >WEBSOCKET  ===================
// ========================================================

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
				extint_override(override_ext);
			else
				if (override_mode == 's4285') extint_override('s4285');	//jks
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
		case "audio_rate":
			audio_init();
			audio_rate(parseFloat(param[1]));
			break;
		case "kiwi_up":
			kiwi_up(parseInt(param[1]));
			break;
		case "too_busy":
			kiwi_too_busy(parseInt(param[1]));
			break;
		case "gps":
			toggle_or_set_spec(1);
			break;
		case "fft_mode":
			kiwi_fft_mode();
			break;
		case "inactivity_timeout_msg":
			add_problem('inactivity timeout '+ param[1] +' minutes', true);
			inactivity_timeout_msg = true;
			break;
		case "squelch":
			squelch_state = parseInt(param[1]);
			var el = w3_el_id('id-squelch');
			console.log('SQ '+ squelch_state);
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
			
			// FIXME temporary until dedicated experiment mechanism is implemented
			if (Sha256.hash(ident_user) == 'd51c59f101ec361eac5a77ac2414214481e5d7e330a854d299fbf4e3fc3ae314') {
				inactivity_timeout_override = 0;
				need_status = true;
			}
			need_ident = false;
		}
	
		if (need_status) {
			if (snd_send("SET need_status=1") < 0)
				break;
			if (inactivity_timeout_override == 0) {
				if (snd_send("SET OVERRIDE inactivity_timeout=0") < 0)
					break;
			}
			need_status = false;
		}
	}

	if (!ws_wf.up || wf_send("SET keepalive") < 0)
		return;
}
