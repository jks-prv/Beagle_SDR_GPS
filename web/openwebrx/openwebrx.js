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

// Copyright (c) 2015 - 2016 John Seamons, ZL/KF6VO

is_firefox=navigator.userAgent.indexOf("Firefox")!=-1;

// key freq concepts:
//		all freqs in Hz
//		center_freq is center of entire sampled bandwidth (e.g. 15 MHz)
//		offset_freq (-bandwidth/2 .. bandwidth/2) = center_freq - freq
//		freq = center_freq + offset_freq

// constants, passed from server
var bandwidth;
var center_freq;
var fft_size;
var rx_chans, rx_chan;
var client_ip;

var cur_mode;
var fft_fps;

var ws_aud, ws_fft;

var comp_override = -1;
var inactivity_timeout = -1;

function kiwi_interface()
{
	var pageURL = window.location.href;
	console.log("URL: "+pageURL);
	var tune = new RegExp("[?&]f=([0-9.]*)([^&#z]*)?z?([0-9]*)"+
		"?(?:$|&abuf=([0-9]*))?(?:$|&ctaps=([0-9]*))?(?:$|&cdiv=([0-9]*))?(?:$|&acomp=([0-9]*))"+
		"?(?:$|&timeout=([0-9]*))"
		).exec(pageURL);
	if (tune) {
		console.log("ARG f="+tune[1]+" m="+tune[2]+" z="+tune[3]+" abuf="+tune[4]+" ctaps="+tune[5]+" cdiv="+tune[6]+" acomp="+tune[7]+" timeout="+tune[8]);
		init_frequency = parseFloat(tune[1]);
		if (tune[2]) {
			init_mode = tune[2];
		}
		if (tune[3]) {
			init_zoom = tune[3];
		}
		if (tune[4]) {
			console.log("ARG audio_buffer_size="+tune[4]+"/"+audio_buffer_size);
			audio_buffer_size = parseFloat(tune[4]);
		}
		if (tune[5]) {
			console.log("ARG comp_lpf_taps_length="+tune[5]+"/"+comp_lpf_taps_length);
			comp_lpf_taps_length = parseFloat(tune[5]);
		}
		if (tune[6]) {
			console.log("ARG comp_off_div="+tune[6]+"/"+comp_off_div);
			comp_off_div = parseFloat(tune[6]);
		}
		if (tune[7]) {
			console.log("ARG comp_override="+tune[7]+"/"+comp_override);
			comp_override = parseFloat(tune[7]);
		}
		if (tune[8]) {
			console.log("ARG inactivity_timeout="+tune[8]+"/"+inactivity_timeout);
			inactivity_timeout = parseFloat(tune[8]);
		}
	}
	
	kiwi_geolocate();
	init_rx_photo();
	place_panels();
	dx_init_panel();
	init_panels();
	smeter_init();
	
	window.setTimeout(function(){window.setInterval(send_keepalive,5000);},5000);
	window.setTimeout(function(){window.setInterval(update_TOD,1000);},1000);
	window.addEventListener("resize",openwebrx_resize);

	ws_aud = open_websocket("AUD", timestamp, audio_recv);	
	if (audio_init()) {
		ws_aud.close();
		return;
	}
	ws_fft = open_websocket("FFT", timestamp, waterfall_add_queue);
}

var panel_shown = { readme:1, msgs:1, news:1 };
var ptype = { HIDE:0, POPUP:1 };
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
	var readme = updateCookie('readme', 'seen2');
	init_panel_toggle(ptype.TOGGLE, 'readme', dbgUs? popt.CLOSE : (readme? popt.PERSIST : 7000), readme_color);
	init_panel_toggle(ptype.TOGGLE, 'msgs');
	init_panel_toggle(ptype.POPUP, 'news', show_news? ((readCookie('news', 'seen') == null)? popt.PERSIST : popt.CLOSE) : popt.CLOSE);
	init_panel_toggle(ptype.POPUP, 'dx', popt.CLOSE);
}

function init_panel_toggle(type, panel)
{
	var divPanel = html('id-'+panel);
	divPanel.ptype = type;
	var divVis = html('id-'+panel+'-vis');
	if (type == ptype.TOGGLE) {
		divVis.innerHTML =
			'<a id="id-'+panel+'-hide" onclick="toggle_panel('+ q(panel) +');"><img src="icons/hideleft.24.png" width="24" height="24" /></a>' +
			'<a id="id-'+panel+'-show" class="class-vis-show" onclick="toggle_panel('+ q(panel) +');"><img src="icons/hideright.24.png" width="24" height="24" /></a>';
	} else {		// ptype.POPUP
		divVis.innerHTML =
			'<a id="id-'+panel+'-close" onclick="toggle_panel('+ q(panel) +');"><img src="icons/close.24.png" width="24" height="24" /></a>';
	}

	var rightSide = (divPanel.getAttribute('data-panel-pos') == "right");
	var visOffset = divPanel.activeWidth - (visIcon - visBorder);
	console.log("init_panel_toggle "+panel+" right="+rightSide+" off="+visOffset);
	if (rightSide) {
		divVis.style.right = visBorder.toString()+'px';
		console.log("RS2");
	} else {
		divVis.style.left = visOffset.toString()+'px';
	}
	divVis.style.top = visBorder+'px';
	console.log("ARROW l="+divVis.style.left+" r="+divVis.style.right+' t='+divVis.style.top);

	if (arguments.length > 2) {
		var timeo = arguments[2];
		if (timeo) {
			setTimeout('toggle_panel('+ q(panel) +')', timeo);
		} else
		if (timeo == popt.CLOSE) {
			toggle_panel(panel);		// make it go away immediately
		} else
		if (type == ptype.POPUP) {
			divPanel.style.visibility = "visible";
		}
	}
	if (arguments.length > 3) {
		divShow = html('id-'+panel+'-show');
		if (typeof divShow.style != 'undefined') divShow.style.backgroundColor = divShow.style.borderColor = arguments[3];
	}
}

function toggle_panel(panel)
{
	var divPanel = html('id-'+panel);
	var divVis = html('id-'+panel+'-vis');

	if (divPanel.ptype == ptype.POPUP) {
		divPanel.style.visibility = 'hidden';
		updateCookie(panel, 'seen');
		return;
	}
	
	var arrow_width = 12, hideWidth = divPanel.activeWidth + 2*15;
	var rightSide = (divPanel.getAttribute('data-panel-pos') == "right");
	var from, to;

	hideWidth = rightSide? -hideWidth : -hideWidth;
	if (panel_shown[panel]) {
		from = 0; to = hideWidth;
	} else {
		from = hideWidth; to = 0;
	}
	animate(divPanel, rightSide? 'right':'left', "px", from, to, 0.93, 1000, 60, 0);
	html('id-'+panel+'-'+(panel_shown[panel]? 'hide':'show')).style.display = "none";
	html('id-'+panel+'-'+(panel_shown[panel]? 'show':'hide')).style.display = "block";
	panel_shown[panel] ^= 1;

	var visOffset = divPanel.activeWidth - visIcon;
	//console.log("toggle_panel "+panel+" right="+rightSide+" shown="+panel_shown[panel]);
	if (rightSide)
		divVis.style.right = (panel_shown[panel]? visBorder : (visOffset + visIcon + visBorder*2)).toString()+'px';
	else
		divVis.style.left = (visOffset + (panel_shown[panel]? visBorder : (visIcon + visBorder*2))).toString()+'px';
}

function openwebrx_resize() 
{
	resize_canvases();
	resize_waterfall_container(true);
	resize_scale();
	check_top_bar_congestion();
}

function check_top_bar_congestion()
{
	/*
	var wt=html("id-rx-title");
	var tl=html("id-ha5kfu-top-logo");
	if(wt.offsetLeft+wt.offsetWidth>tl.offsetLeft-20) tl.style.display="none";
	else tl.style.display="block";
	*/
}

var rx_photo_spacer_height = 67;

function init_rx_photo()
{
	rx_photo_height += rx_photo_spacer_height;
	html("id-top-photo-clip").style.maxHeight=rx_photo_height.toString()+"px";
	//window.setTimeout(function() { animate(html("id-rx-photo-title"),"opacity","",1,0,1,500,30); },1000);
	//window.setTimeout(function() { animate(html("id-rx-photo-desc"),"opacity","",1,0,1,500,30); },1500);
	if (dbgUs) {
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
	animate_to(html("id-top-photo-clip"),"maxHeight","px",rx_photo_height,0.93,1000,60,function(){resize_waterfall_container(true);});
	html("id-rx-details-arrow-down").style.display="none";
	html("id-rx-details-arrow-up").style.display="block";
}

function close_rx_photo()
{
	rx_photo_state=0;
	animate_to(html("id-top-photo-clip"),"maxHeight","px",rx_photo_spacer_height,0.93,1000,60,function(){resize_waterfall_container(true);});
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
	if(dont_toggle_rx_photo_flag) { dont_toggle_rx_photo_flag=0; return; }
	if(rx_photo_state) close_rx_photo();
	else open_rx_photo()
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
	env_h2=5;                //   |||env_att_line_w                |_env_lineplus
	env_lineplus=1;          //   ||env_bounding_line_w
	env_line_click_area=6;
	//range=get_visible_freq_range();
	from_px=scale_px_from_freq(from,range);
	to_px=scale_px_from_freq(to,range);
	if(to_px<from_px) /* swap'em */ { temp_px=to_px; to_px=from_px; from_px=temp_px; }
	
	/*from_px-=env_bounding_line_w/2;
	to_px+=env_bounding_line_w/2;*/
	from_px-=(env_att_w+env_bounding_line_w);
	to_px+=(env_att_w+env_bounding_line_w); 
	// do drawing:
	scale_ctx.lineWidth=3;
	scale_ctx.strokeStyle=color;
	scale_ctx.fillStyle = color;
	var drag_ranges={ envelope_on_screen: false, line_on_screen: false };
	if(!(to_px<0||from_px>window.innerWidth)) // out of screen?
	{
		drag_ranges.beginning={x1:from_px, x2: from_px+env_bounding_line_w+env_att_w};
		drag_ranges.ending={x1:to_px-env_bounding_line_w-env_att_w, x2: to_px};
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
		}
	}
	return drag_ranges;
}

function demod_envelope_where_clicked(x, drag_ranges, key_modifiers)
{  // Check exactly what the user has clicked based on ranges returned by demod_envelope_draw().
	in_range=function(x,g_range) { return g_range.x1<=x&&g_range.x2>=x; }
	dr=demodulator.draggable_ranges;

	if(key_modifiers.shiftKey)
	{
		//Check first: shift + center drag emulates BFO knob
		if(drag_ranges.line_on_screen&&in_range(x,drag_ranges.line)) return dr.bfo;
		//Check second: shift + envelope drag emulates PBF knob
		if(drag_ranges.envelope_on_screen&&in_range(x,drag_ranges.whole_envelope)) return dr.pbs;
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
demodulator.draggable_ranges={none: 0, beginning:1 /*from*/, ending: 2 /*to*/, anything_else: 3, bfo: 4 /*line (while holding shift)*/, pbs: 5 } //to which parameter these correspond in demod_envelope_draw()

//******* class demodulator_default_analog *******
// This can be used as a base for basic audio demodulators.
// It already supports most basic modulations used for ham radio and commercial services: AM/FM/LSB/USB

demodulator_response_time=100; 
//in ms; if we don't limit the number of SETs sent to the server, audio will underrun (possibly output buffer is cleared on SETs in GNU Radio

var passbands = {
	lsb:	{ lo: -2700,	hi:  -300 },	// cf = 1500 Hz, bw = 2400 Hz
	usb:	{ lo:   300,	hi:  2700 },	// cf = 1500 Hz, bw = 2400 Hz
	cw:	{ lo:   300,	hi:   700 },	// cf = 500 Hz, bw = 400 Hz
	cwn:	{ lo:   470,	hi:   530 },	// cf = 500 Hz, bw = 60 Hz
	am:	{ lo: -4000,	hi:  4000 },
	amn:	{ lo: -2500,	hi:  2500 },
};

function demodulator_default_analog(offset_frequency, subtype)
{
	//http://stackoverflow.com/questions/4152931/javascript-inheritance-call-super-constructor-or-use-prototype-chain
	demodulator.call(this, offset_frequency);

	this.subtype=subtype;
	this.envelope.dragged_range = demodulator.draggable_ranges.none;
	this.filter={
		min_passband: 100,
		high_cut_limit: audio_context.sampleRate/2,
		low_cut_limit: -audio_context.sampleRate/2
	};

	//Subtypes only define some filter parameters and the mod string sent to server, 
	//so you may set these parameters in your custom child class.
	//Why? As the demodulation is done on the server, difference is mainly on the server side.
	this.server_mode = subtype;
	this.low_cut =  passbands[subtype].lo;
	this.high_cut = passbands[subtype].hi;
	this.usePBCenter = false;
	this.usePBCenterDX = false;
	this.isCW = false;

	if(subtype=="lsb")
	{
		this.usePBCenter=true;
		this.usePBCenterDX=true;
	}
	else if(subtype=="usb")
	{
		this.usePBCenter=true;
		this.usePBCenterDX=true;
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
	else if(subtype=="am")
	{
	}	
	else if(subtype=="amn")
	{
	}
	else console.log("DEMOD-new: unknown subtype="+subtype);

	this.wait_for_timer=false;
	this.set_after=false;

	this.set=function()
	{ //set() is a wrapper to call doset(), but it ensures that doset won't execute more frequently than demodulator_response_time.

		if(!this.wait_for_timer) 
		{
			this.doset();
			this.set_after=false;
			this.wait_for_timer=true;
			timeout_this=this; //http://stackoverflow.com/a/2130411
			window.setTimeout(function() {
				timeout_this.wait_for_timer=false;
				if(timeout_this.set_after) timeout_this.set();
			},demodulator_response_time);
		}
		else
		{
			this.set_after=true;
		}
	}

	this.doset = function()
	{  //this function sends demodulator parameters to the server
		//console.log('DOSET fcar='+freq_car_Hz);
		//foo if (dbgUs && dbgUsFirst) { dbgUsFirst = false; console.trace(); }
		ws_aud_send("SET mod="+this.server_mode+
			" low_cut="+this.low_cut.toString()+" high_cut="+this.high_cut.toString()+
			" freq="+(freq_car_Hz/1000).toFixed(3));
	}
	//this.set(); //we set parameters on object creation

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
	};

	// event handlers
	this.envelope.drag_start = function(x, key_modifiers)
	{
		this.key_modifiers = key_modifiers;
		this.dragged_range = demod_envelope_where_clicked(x, this.drag_ranges, key_modifiers);
		//console.log("dragged_range: "+this.dragged_range.toString());
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
		dr=demodulator.draggable_ranges;
		//console.log("DRAG-MOVE "+this.dragged_range+' '+dr.none);
		if(this.dragged_range==dr.none) return false; // we return if user is not dragging (us) at all

		freq_change = Math.round(this.visible_range.hpp * (x-this.drag_origin.x));
		//console.log("DRAG fch="+freq_change);
		/*if(this.dragged_range==dr.beginning||this.dragged_range==dr.ending)
		{
			//we don't let the passband be too small
			if (this.parent.low_cut+new_freq_change <= this.parent.high_cut-this.parent.filter.min_passband) this.freq_change=new_freq_change;
			else return;
		}
		var new_value;*/

		//dragging the line in the middle of the filter envelope while holding Shift does emulate
		//the BFO knob on radio equipment: moving offset frequency, while passband remains unchanged
		//Filter passband moves in the opposite direction than dragged, hence the minus below.
		minus=(this.dragged_range==dr.bfo)?-1:1;
		//dragging any other parts of the filter envelope while holding Shift does emulate the PBS knob
		//(PassBand Shift) on radio equipment: PBS does move the whole passband without moving the offset
		//frequency.
		if(this.dragged_range==dr.beginning||this.dragged_range==dr.bfo||this.dragged_range==dr.pbs) 
		{
			//we don't let low_cut go beyond its limits
			if((new_value=this.drag_origin.low_cut+minus*freq_change)<this.parent.filter.low_cut_limit) return true;
			//nor the filter passband be too small
			if(this.parent.high_cut-new_value<this.parent.filter.min_passband) return true; 
			//sanity check to prevent GNU Radio "firdes check failed: fa <= fb"
			if(new_value>=this.parent.high_cut) return true;
			this.parent.low_cut=new_value;
		}
		if(this.dragged_range==dr.ending||this.dragged_range==dr.bfo||this.dragged_range==dr.pbs) 
		{
			//we don't let high_cut go beyond its limits
			if((new_value=this.drag_origin.high_cut+minus*freq_change)>this.parent.filter.high_cut_limit) return true;
			//nor the filter passband be too small
			if(new_value-this.parent.low_cut<this.parent.filter.min_passband) return true; 
			//sanity check to prevent GNU Radio "firdes check failed: fa <= fb"
			if(new_value<=this.parent.low_cut) return true;
			this.parent.high_cut=new_value;
		}
		if(this.dragged_range==dr.anything_else || this.dragged_range==dr.bfo)
		{
			//when any other part of the envelope is dragged, the offset frequency is changed (whole passband also moves with it)
			new_value = this.drag_origin.offset_frequency + freq_change;
			if(new_value>bandwidth/2||new_value<-bandwidth/2) return true; //we don't allow tuning above Nyquist frequency :-)
			this.parent.offset_frequency = new_value;
		}
		//now do the actual modifications:
		mkenvelopes(this.visible_range);
		freqset_car_Hz(this.parent.offset_frequency + center_freq);
		this.parent.set();
		freqset_update_ui();

		//will have to change this when changing to multi-demodulator mode:
		//html("id-params-1").innerHTML=format_frequency("{x} MHz",center_freq+this.parent.offset_frequency,1e6,4);
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

function demodulator_analog_replace(subtype)
{ //this function should only exist until the multi-demodulator capability is added	
	var offset = 0;
	
	if(demodulators.length) {
		offset = demodulators[0].offset_frequency;
		demodulator_remove(0);
	} else {
		console.log("init_freq="+init_frequency+" init_mode="+init_mode);
		offset = (init_frequency*1000).toFixed(0) - center_freq;
		subtype = init_mode;
	}
	
	// initial offset, but doesn't consider demod.isCW since it isn't valid yet
	if (arguments.length > 1) {
		offset = arguments[1] - center_freq;
	}
	
	//console.log("DEMOD-replace calling add: INITIAL offset="+(offset+center_freq));
	demodulator_add(new demodulator_default_analog(offset, subtype));
	
	// determine actual offset now that demod.isCW is valid
	if (arguments.length > 1) {
		freq_car_Hz = freq_dsp_to_car(arguments[1]);
		//console.log("DEMOD-replace SPECIFIED freq="+arguments[1]+' car='+freq_car_Hz);
		offset = freq_car_Hz - center_freq;
	}
	
	cur_mode = subtype;
	//console.log("demodulator_analog_replace: cur_mode="+cur_mode);

	// must be done here after demod is added, so demod.isCW is available after demodulator_add()
	// done even if freq unchanged in case mode is changing
	//console.log("DEMOD-replace calling set: FINAL offset="+(offset+center_freq));
	demodulator_set_offset_frequency(0, offset);
	
	modeset_update_ui(subtype);
}

function demodulator_set_offset_frequency(which, offset)
{
	if(offset>bandwidth/2 || offset<-bandwidth/2) return;
	offset = Math.round(offset);
	//console.log("demodulator_set_offset_frequency: offset="+(offset + center_freq));
	
	// set carrier freq before demodulators[0].set() below
	// requires demodulators[0].isCW to be valid
	freqset_car_Hz(offset + center_freq);
	
	var demod = demodulators[0];
	demod.offset_frequency = offset;
	demod.set();
	freqset_update_ui();
	if (waterfall_setup_done) mkenvelopes(get_visible_freq_range());
}


// ========================================================
// ===================  >SCALE  ===========================
// ========================================================

var scale_ctx, band_ctx, dx_ctx;
var scale_canvas, band_canvas, dx_div, dx_canvas;

function scale_setup()
{
	scale_canvas = html("id-scale-canvas");	
	scale_ctx = scale_canvas.getContext("2d");
	scale_canvas.addEventListener("mousedown", scale_canvas_mousedown, false);
	scale_canvas.addEventListener("mousemove", scale_canvas_mousemove, false);
	scale_canvas.addEventListener("mouseup", scale_canvas_mouseup, false);

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

var scale_canvas_drag_params={
	mouse_down: false,
	drag: false,
	start_x: 0,
	key_modifiers: {shiftKey:false, altKey: false, ctrlKey: false}
};

function scale_canvas_mousedown(evt)
{
	//console.log("MDN");
	with (scale_canvas_drag_params) {
		mouse_down = true;
		drag = false;
		start_x = evt.pageX;
		key_modifiers.shiftKey = evt.shiftKey;
		key_modifiers.altKey = evt.altKey;
		key_modifiers.ctrlKey = evt.ctrlKey;
	}
	evt.preventDefault();
}

function scale_offset_carfreq_from_px(x, visible_range)
{
	if (typeof visible_range === "undefined") visible_range = get_visible_freq_range();
	var offset = passband_offset();
	var f = visible_range.start + visible_range.bw*(x/canvas_container.clientWidth);
	//console.log("SOCFFPX f="+f+" off="+offset+" f-o="+(f-offset)+" rtn="+(f - center_freq - offset));
	return f - center_freq - offset;
}

function scale_canvas_mousemove(evt)
{
	var event_handled = 0;
	if (scale_canvas_drag_params.mouse_down && !scale_canvas_drag_params.drag && Math.abs(evt.pageX - scale_canvas_drag_params.start_x) > canvas_drag_min_delta) 
	//we can use the main drag_min_delta thing of the main canvas
	{
		scale_canvas_drag_params.drag=true;
		//call the drag_start for all demodulators (and they will decide if they're dragged, based on X coordinate)
		for (var i=0;i<demodulators.length;i++) event_handled |= demodulators[i].envelope.drag_start(evt.pageX, scale_canvas_drag_params.key_modifiers);
		//console.log("MOV1 evh? "+event_handled);
		scale_canvas.style.cursor="move";
	}
	else if (scale_canvas_drag_params.drag)
	{
		//call the drag_move for all demodulators (and they will decide if they're dragged)
		for (var i=0;i<demodulators.length;i++) event_handled |= demodulators[i].envelope.drag_move(evt.pageX);
		//console.log("MOV2 evh? "+event_handled);
		if (!event_handled) demodulator_set_offset_frequency(0, scale_offset_carfreq_from_px(evt.pageX));
	}
	
}

function scale_canvas_end_drag(x)
{
	canvas_container.style.cursor="default";
	scale_canvas_drag_params.drag=false;
	scale_canvas_drag_params.mouse_down=false;
	var event_handled=false;
	for (var i=0;i<demodulators.length;i++) event_handled |= demodulators[i].envelope.drag_end(x);
	//console.log("MED evh? "+event_handled);
	if (!event_handled) demodulator_set_offset_frequency(0, scale_offset_carfreq_from_px(x));
}

function scale_canvas_mouseup(evt)
{
	//console.log("MUP");
	scale_canvas_end_drag(evt.pageX);
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

var scale_markers_levels=[
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
		"decimals":1
	}
];

var scale_markers_levels_dbug=[
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
		"decimals":1
	}
];

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
	text_y_pos = 22+10+((is_firefox)?3:0);
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
			if (zoom_level==0 && g_range.start+spacing.smallbw*spacing.ratio > marker_hz) {
				//if this is the first overall marker when zoomed out
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
				if (x > window.innerWidth-text_measured.width/2) 
				{ //and if it would be clipped off the screen
					if (window.innerWidth-text_measured.width-scale_px_from_freq(marker_hz-spacing.smallbw*spacing.ratio,g_range) >= scale_min_space_btwn_texts)
					{ //and if we have enough space to draw it correctly without clipping
						scale_ctx.textAlign = "right";
						scale_ctx.fillText(text_to_draw, window.innerWidth, text_y_pos); 
					}	
				}		
			} else
				scale_ctx.fillText(text_to_draw, x, text_y_pos); //draw text normally
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
	dx_canvas.style.left = '0px';
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
	mk_spurs();
}


// ========================================================
// =================== >CONVERSIONS =======================
// ========================================================

// A "bin" is the fft_size multiplied by the maximum zoom factor.
// So there are approx 1024 * 2^11 = 2M bins.
// The left edge of the waterfall is specified with a bin number.
// The higher precision of having a large number of bins makes the code simpler.
// Remember that the actual displayed waterfall_width is typically larger than the
// fft_size data in the canvas due to stretching of the canvas to fit the screen.

function bins_at_zoom(zoom)
{
	var bins = fft_size << (zoom_levels_max - zoom);
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
	var max_bins = fft_size << zoom_levels_max;
	return Math.round((bin / max_bins) * bandwidth);
}

function freq_to_bin(freq) {
	var max_bins = fft_size << zoom_levels_max;
	return Math.round(freq/bandwidth * max_bins);
}

function bins_to_pixels_frac(cf, bins, zoom) {
	var bin_ratio = bins / bins_at_zoom(zoom);
	if (bin_ratio > 1) bin_ratio = 1;
	if (bin_ratio < -1) bin_ratio = -1;
	var f_pixels = fft_size * bin_ratio;
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
	var max_bins = fft_size << zoom_levels_max;
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
	var f = center_freq + demodulators[0].offset_frequency;
	var pb_bin = freq_to_bin(f);
	//console.log("PBV f="+f+" x_bin="+x_bin+" BACZ="+bins_at_cur_zoom()+" max_bin="+(x_bin+bins_at_cur_zoom())+" pb_bin="+pb_bin);
	pb_bin = (pb_bin >= x_bin && pb_bin < (x_bin+bins_at_cur_zoom()))? pb_bin : -pb_bin-1;
	//console.log("PBV ="+pb_bin+' '+((pb_bin<0)? 'outside':'inside'));
	return pb_bin;
}


// ========================================================
// ======================== >CANVAS =======================
// ========================================================

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

canvas_drag=false;
canvas_drag_min_delta=1;
canvas_mouse_down = false;
canvas_ignore_mouse_event = false;

function canvas_mousedown(evt)
{
	//console.log("MDN "+this.id+" ign="+canvas_ignore_mouse_event);
	//console.log("MDN sft="+evt.shiftKey+" alt="+evt.altKey+" ctrl="+evt.ctrlKey+" meta="+evt.metaKey);
	//console.log("MDN button="+evt.button+" buttons="+evt.buttons+" detail="+evt.detail+" which="+evt.which);

	if (evt.shiftKey) {
		canvas_ignore_mouse_event = true;

		if (evt.target.id == 'id-dx-container') {
			dx_show_edit_panel(-1);
		} else {
			// lookup mouse pointer in online resources
			var fo = (canvas_get_dspfreq((evt.offsetX)? evt.offsetX : evt.layerX) / 1000).toFixed(2);
			var f;
			var url = "http://";
			if (evt.ctrlKey) {
				f = Math.round(fo/5) * 5;	// 5kHz windows on 5 kHz boundaries -- intended for SWBC
				url += "www.short-wave.info/index.php?freq="+f+"&timbus=NOW&ip="+client_ip+"&porm=4";
			} else {
				f = Math.floor(fo/10) / 100;	// round down to nearest 100 Hz, and express in MHz, for GlobalTuners
				url += "qrg.globaltuners.com/?q="+f.toFixed(2).toString();
			}
			//console.log("LOOKUP "+fo+" -> "+f+" "+url);
			var win = window.open(url, '_blank');
			if (win != "undefined") win.focus();
		}
	} else
	if (evt.altKey) {
		canvas_ignore_mouse_event = true;
		page_scroll(-page_scroll_amount);
	} else
	if (evt.metaKey) {
		canvas_ignore_mouse_event = true;
		page_scroll(page_scroll_amount);
	}
	
	canvas_mouse_down = true;
	canvas_drag=false;
	canvas_drag_last_x=canvas_drag_start_x=evt.pageX;
	canvas_drag_last_y=canvas_drag_start_y=evt.pageY;
	evt.preventDefault(); //don't show text selection mouse pointer
}

function canvas_mousemove(evt)
{
	if(!waterfall_setup_done) return;
	//console.log("MOV "+this.id+" ign="+canvas_ignore_mouse_event);
	//element=html("id-freq-show");
	relativeX=(evt.offsetX)? evt.offsetX:evt.layerX;

	/*
	realX=(relativeX-element.clientWidth/2);
	maxX=(waterfall_width-element.clientWidth);
	if(realX>maxX) realX=maxX;
	if(realX<0) realX=0;
	element.style.left=realX.toString()+"px";
	*/

	if(canvas_mouse_down && !canvas_ignore_mouse_event)
	{
		if(!canvas_drag && Math.abs(evt.pageX-canvas_drag_start_x) > canvas_drag_min_delta) 
		{
			canvas_drag=true;
			canvas_container.style.cursor="move";
		}
		if(canvas_drag) 
		{
			var deltaX=canvas_drag_last_x-evt.pageX;
			var deltaY=canvas_drag_last_y-evt.pageY;

			var dbins = norm_to_bins(deltaX / waterfall_width);
			waterfall_pan_canvases(dbins);

			canvas_drag_last_x=evt.pageX;
			canvas_drag_last_y=evt.pageY;
		}
	} else {
		html("id-mouse-unit").innerHTML=format_frequency("{x}", canvas_get_dspfreq(relativeX), 1e3, 2);
		//console.log("MOU rX="+relativeX.toFixed(1)+" f="+canvas_get_dspfreq(relativeX).toFixed(1));
	}
}

function canvas_container_mouseout(evt)
{
	canvas_end_drag();
}

//function body_mouseup() { canvas_end_drag(); console.log("body_mouseup"); }
//function window_mouseout() { canvas_end_drag(); console.log("document_mouseout"); }

function canvas_mouseup(evt)
{
	if(!waterfall_setup_done) return;
	//console.log("MUP "+this.id+" ign="+canvas_ignore_mouse_event);
	relativeX=(evt.offsetX)?evt.offsetX:evt.layerX;

	if (canvas_ignore_mouse_event) {
		ignore_next_keyup_event = true;
	} else {
		if (!canvas_drag) {
			 demodulator_set_offset_frequency(0, canvas_get_carfreq_offset(relativeX, true));		
		} else {
			canvas_end_drag();
		}
	}
	
	canvas_mouse_down = false;
	canvas_ignore_mouse_event = false;
}

function canvas_end_drag()
{
	canvas_container.style.cursor = "crosshair";
	canvas_mouse_down = false;
	canvas_ignore_mouse_event = false;
}

function canvas_mousewheel(evt)
{
	if (!waterfall_setup_done) return;
	//var i=Math.abs(evt.wheelDelta);
	//var out=(i/evt.wheelDelta)<0;
	//console.log(evt);
	var relativeX = (evt.offsetX)? evt.offsetX:evt.layerX;
	var out = (evt.deltaY / Math.abs(evt.deltaY)) > 0;
	//console.log(dir);
	//i/=120;
	var dir = out? zoom.out:zoom.in;
	/*while (i--)*/
		zoom_step(dir, relativeX);
	evt.preventDefault();	
	//evt.returnValue = false; //disable scrollbar move
}


zoom_levels_max=0;
zoom_level=0;
zoom_freq=0;

var x_bin = 0;				// left edge of waterfall in units of bin number

// called from mouse wheel and zoom button pushes
// dir: 1 = zoom in, -1 = zoom out, 9 = max in, -9 max out, 0 = zoom to band
// x_rel: 0 .. waterfall_width, position of mouse cursor on window, -1 called from button push

var zoom = { to_band:0, in:1, out:-1, abs:2, max_in:9, max_out:-9 };

function zoom_step(dir)
{
	var out = (dir == zoom.out);
	var ozoom = zoom_level;
	var x_obin = x_bin;
	var x_norm;

	if (dir == zoom.max_out) {		// max out
		out = true;
		zoom_level = 0;
		x_bin = 0;
	} else {					// in/out, max in, band
		if (dir != zoom.to_band && ((out && zoom_level == 0) || (!out && zoom_level >= zoom_levels_max))) { freqset_select(); return; }

		if (dir == zoom.to_band) {
			// zoom to band
			var f = center_freq + demodulators[0].offset_frequency;
			var cf;
			var b = null;
			if (arguments.length > 1) b = arguments[1];	// band specified by caller
			if (b != null) {
					zoom_level = b.zoom_level;
					cf = b.cf;
//foo
if(sb_trace)
console.log("ZTB-user f="+f+" cf="+cf+" b="+b.name+" z="+b.zoom_level);
			} else {
				for (i=0; i < bands.length; i++) {		// search for first containing band
					b = bands[i];
					if (f >= b.min && f <= b.max)
						break;
				}
				if (i != bands.length) {
					//console.log("ZTB-calc f="+f+" cf="+cf+" b="+b.name+" z="+b.zoom_level);
					zoom_level = b.zoom_level;
					cf = b.cf;
				} else {
					zoom_level = 6;	// not in a band -- pick a reasonable span
					cf = f;
					//console.log("ZTB-outside f="+f+" cf="+cf);
				}
			}
			out = (zoom_level < ozoom);
			x_bin = freq_to_bin(cf);		// center waterfall at middle of band
			x_bin -= norm_to_bins(0.5);
		} else
		
		if (dir == zoom.abs) {
			if (arguments.length <= 1) { freqset_select(); return; }
			var znew = arguments[1];
			if (znew < 0 || znew > zoom_levels_max || znew == zoom_level) { freqset_select(); return; }
			out = (znew < zoom_level);
			zoom_level = znew;
			// center waterfall at middle of passband
			x_bin = freq_to_bin(center_freq + demodulators[0].offset_frequency);
			x_bin -= norm_to_bins(0.5);
			//console.log("ZOOM ABS z="+znew+" out="+out+" b="+x_bin);
		} else
		
		if (dir == zoom.max_in) {	// max in
			out = false;
			zoom_level = zoom_levels_max;
			// center max zoomed waterfall at middle of passband
			x_bin = freq_to_bin(center_freq + demodulators[0].offset_frequency);
			x_bin -= norm_to_bins(0.5);
		} else {
		
			// in, out
			if (arguments.length > 1) {
				var x_rel = arguments[1];
				
				// zoom in or out centered on cursor position, not passband
				x_norm = x_rel / waterfall_width;	// normalized position (0..1) on waterfall
			} else {
				// zoom in or out centered on passband, if visible, else middle of current waterfall
				var pb_bin = passband_visible();
				if (pb_bin >= 0)		// visible
					x_norm = (pb_bin - x_bin) / bins_at_cur_zoom();
				else
					x_norm = 0.5;
			}
			x_bin += norm_to_bins(x_norm);	// remove offset bin relative to current zoom
			zoom_level += dir;
			x_bin -= norm_to_bins(x_norm);	// add offset bin relative to new zoom
		}
	}
	
	//console.log("ZStep z"+zoom_level.toString()+" zlevel_val="+zoom_levels[zoom_level].toString()+" fLEFT="+canvas_get_dspfreq(0));
	
	x_bin = clamp_xbin(x_bin);
	var dbins = Math.abs(x_obin - x_bin);
	var pixel_dx = bins_to_pixels(1, dbins, out? zoom_level:ozoom);
	//console.log("Zs z"+ozoom+'>'+zoom_level+' b='+x_bin+'/'+x_obin+'/'+dbins+' bz='+bins_at_zoom(ozoom)+' r='+(dbins / bins_at_zoom(ozoom))+' px='+pixel_dx);
	var dz = zoom_level - ozoom;
	waterfall_zoom_canvases(dz, pixel_dx);
	mkscale();
	dx_schedule_update();
	//console.log("Z"+zoom_level+" xb="+x_bin);
	ws_fft_send("SET zoom="+ zoom_level +" start="+ x_bin);
	need_maxmindb_update = true;
   freqset_select();
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
var waterfall_scrollable_height = 1000;
var canvas_context;
var canvases = [];
var canvas_default_height = 200;
var canvas_container;
var canvas_phantom;
var canvas_actual_line;
var canvas_id=0;

// NB: canvas data width is fft_size, but displayed style width is waterfall_width (likely different),
// so image is stretched to fit when rendered!

function add_canvas()
{	
	var new_canvas = document.createElement("canvas");
	new_canvas.width=fft_size;
	new_canvas.height=canvas_default_height;
	canvas_actual_line=canvas_default_height-1;
	new_canvas.style.width=waterfall_width.toString()+"px";	
	new_canvas.style.left="0px";
	new_canvas.openwebrx_height=canvas_default_height;	
	new_canvas.style.height=new_canvas.openwebrx_height.toString()+"px";
	new_canvas.openwebrx_top=(-canvas_default_height+1);	
	new_canvas.style.top=new_canvas.openwebrx_top.toString()+"px";

	canvas_context = new_canvas.getContext("2d");
	canvas_container.appendChild(new_canvas);
	add_canvas_listner(new_canvas);
	new_canvas.openwebrx_id=canvas_id++;
	canvases.unshift(new_canvas);
}

var spectrum_canvas, spectrum_ctx;

function init_canvas_container()
{
	window_width = window.innerWidth;		// window width minus any scrollbar
	canvas_container=html("id-waterfall-container");
	waterfall_width = canvas_container.clientWidth;
	//console.log("init_canvas_container ww="+waterfall_width);
	canvas_container.addEventListener("mouseout", canvas_container_mouseout, false);
	//window.addEventListener("mouseout",window_mouseout,false);
	//document.body.addEventListener("mouseup",body_mouseup,false);

	canvas_phantom=html("id-phantom-canvas");
	add_canvas_listner(canvas_phantom);
	canvas_phantom.style.width=canvas_container.clientWidth+"px";
	add_canvas();

	spectrum_canvas = document.createElement("canvas");	
	spectrum_canvas.width = fft_size;
	spectrum_canvas.height = 200;
	spectrum_canvas.style.width = waterfall_width.toString()+"px";	
	spectrum_canvas.style.height = spectrum_canvas.height.toString()+"px";	
	html("id-spectrum-container").appendChild(spectrum_canvas);
	spectrum_ctx = spectrum_canvas.getContext("2d");
	add_canvas_listner(spectrum_canvas);
	spectrum_ctx.font = "10px sans-serif";
	spectrum_ctx.textBaseline = "middle";
	spectrum_ctx.textAlign = "left";
}

function add_canvas_listner(obj)
{
	obj.addEventListener("mouseover", canvas_mouseover, false);
	obj.addEventListener("mouseout", canvas_mouseout, false);
	obj.addEventListener("mousemove", canvas_mousemove, false);
	obj.addEventListener("mouseup", canvas_mouseup, false);
	obj.addEventListener("mousedown", canvas_mousedown, false);
	obj.addEventListener("wheel", canvas_mousewheel, false);
}

function mouse_ignore(ev) { console.log("MIGN"); return cancelEvent(ev) }

function mouse_listner_ignore(obj)
{
	obj.addEventListener("mouseover", mouse_ignore, false);
	obj.addEventListener("mouseout", mouse_ignore, false);
	obj.addEventListener("mousemove", mouse_ignore, false);
	obj.addEventListener("mouseup", mouse_ignore, false);
	obj.addEventListener("mousedown", mouse_ignore, false);
	obj.addEventListener("wheel", mouse_ignore, false);
}

canvas_maxshift=0;

function shift_canvases()
{
	canvases.forEach(function(p) 
	{
		p.style.top=(p.openwebrx_top++).toString()+"px";
	});
	
	// retire canvases beyond bottom of scroll-back window
	var p = canvases[canvases.length-1];
	if (p.openwebrx_top >= waterfall_scrollable_height) {
		//p.style.display="none";
		canvas_container.removeChild(p);		// fixme: is this right?
		canvases.pop();		// fixme: this alone makes scrollbar jerky
		
		/* doesn't work as expected
		p.openwebrx_height--;
		if (p.openwebrx_height <= 0) {
			//console.log("XX "+p.openwebrx_top+' '+canvas_container.clientHeight);
			p.style.display="none";
			//var ctx = p.getContext("2d"); ctx.fillStyle = "Red"; ctx.fillRect(0,0,p.width,p.height);
			canvases.pop();
		} else {
			//p.style.height=p.openwebrx_height.toString()+"px";
			p.beginPath();
			p.rect(0, 0, p.openwebrx_width, p.openwebrx_height);
			p.clip();
		}
		*/
	}
	
	canvas_maxshift++;
	if(canvas_maxshift < canvas_container.clientHeight)
	{
		canvas_phantom.style.top=canvas_maxshift.toString()+"px";
		canvas_phantom.style.height=(canvas_container.clientHeight-canvas_maxshift).toString()+"px";
		canvas_phantom.style.display="block";
	}
	else
		canvas_phantom.style.display="none";
	
	
	//canvas_container.style.height=(((canvases.length-1)*canvas_default_height)+(canvas_default_height-canvas_actual_line)).toString()+"px";
	//canvas_container.style.height="100%";
}

function resize_canvases(zoom)
{
	window_width = canvas_container.innerWidth;
	waterfall_width = canvas_container.clientWidth;
	//console.log("RESIZE winW="+window_width+" wfW="+waterfall_width);

	new_width = waterfall_width.toString()+"px";
	var zoom_value = "0px";
	//console.log("RESIZE z"+zoom_level+" nw="+new_width+" zv="+zoom_value);

	canvases.forEach(function(p) 
	{
		p.style.width=new_width;
		p.style.left=zoom_value;
	});
	canvas_phantom.style.width=new_width;
	canvas_phantom.style.left=zoom_value;
}


// ========================================================
// =======================  >WATERFALL  ===================
// ========================================================

var waterfall_setup_done=0;
var waterfall_queue = [];
var waterfall_timer;

function waterfall_init()
{
	init_canvas_container();
	waterfall_timer = window.setInterval(waterfall_dequeue,900/Math.abs(fft_fps));
	resize_waterfall_container(false); /* then */ resize_canvases();
	panels_setup();
	ident_init();
	scale_setup();
	mkcolormap();
	bands_init();
	spectrum_init();
	dx_schedule_update();
	users_init();
	waterfall_setup_done=1;
}

var dB_bands = [];

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

var redraw_spectrum_dB_scale = false;
var spectrum_data;
var spectrum_update_rate_Hz = 10;	// limit update rate since rendering spectrum is currently expensive
var spectrum_update = 0, spectrum_last_update = 0;

function spectrum_init()
{
	spectrum_data = spectrum_ctx.createImageData(1, spectrum_canvas.height);
	update_maxmindb_sliders();
	mk_dB_bands();
	setInterval(function() { spectrum_update++ }, 1000 / spectrum_update_rate_Hz);
}

function mk_dB_bands()
{
	dB_bands = [];
	var i=0;
	var rng = barmax-barmin;
	//console.log("DB barmax="+barmax+" barmin="+barmin+" maxdb="+maxdb+" mindb="+mindb);
	var last_norm = 0;
	for (var dB = Math.floor(maxdb/10)*10; (mindb-dB) < 10; dB -= 10) {
		var norm = 1 - ((dB - mindb) / full_scale);
		var cmi = Math.round((dB - barmin) / rng * 255);
		var color = color_map[cmi];
		var color_name = '#'+(color >>> 8).toString(16).leadingZeros(6);
		dB_bands[i] = { dB:dB, norm:norm, color:color_name };
		
		var ypos = function(norm) { return Math.round(norm * spectrum_canvas.height) }
		for (var y = ypos(last_norm); y < ypos(norm); y++) {
			for (var j=0; j<4; j++) {
				spectrum_data.data[y*4+j] = ((color>>>0) >> ((3-j)*8)) & 0xff;
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
var iir_gain = [];

var seq=0;
function waterfall_add(dat)
{
	if(!waterfall_setup_done) return;
	var u32View = new Uint32Array(dat, 4, 2);
	var x_bin_server = u32View[0];		// bin & zoom from server at time data was queued
	var x_zoom_server = u32View[1];
	var data = new Uint8Array(dat, 12);	// unsigned dBm values, converted to signed later on
	var bytes = data.length;
	var w = fft_size;

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
	
	if (waterfall_compression) {
		var ndata = new Uint8Array(bytes*2);
		var wf_adpcm = { index:0, previousValue:0 };
		decode_ima_adpcm_u8_i16(data, ndata, bytes, wf_adpcm);
		data = ndata.subarray(10);
	}
	
	var sw, sh, tw=25;
	var need_update = false;
	if (spectrum_display && spectrum_update != spectrum_last_update) {
		spectrum_last_update = spectrum_update;
		need_update = true;

		sw = spectrum_canvas.width-tw;
		sh = spectrum_canvas.height;
		spectrum_ctx.fillStyle = "black";
		spectrum_ctx.fillRect(0,0,sw,sh);
		
		spectrum_ctx.fillStyle = "lightGray";
		for (var i=0; i < dB_bands.length; i++) {
			var band = dB_bands[i];
			var y = Math.round(band.norm * sh);
			spectrum_ctx.fillRect(0,y,sw,1);
		}

		if (clear_specavg) {
			for (var x=0; x<sw; x++) {
				specavg[x] = waterfall_color_index(data[x]);
			}
			clear_specavg = false;
		}
		
		if (redraw_spectrum_dB_scale) {
			spectrum_ctx.fillStyle = "black";
			spectrum_ctx.fillRect(sw,0,tw,sh);
			spectrum_ctx.fillStyle = "white";
			for (var i=0; i < dB_bands.length; i++) {
				var band = dB_bands[i];
				var y = Math.round(band.norm * sh);
				spectrum_ctx.fillText(band.dB, sw+3, y);
				//console.log("SP x="+sw+" y="+y+' '+dB);
			}
			redraw_spectrum_dB_scale = false;
		}
	}
	
	// Add line to waterfall image			
	oneline_image = canvas_context.createImageData(w,1);
/*var sum=0;
for (var x=w-200; x<w-10; x++) {
	sum += data[x];
}
if (sum == 0) console.log("zero sum!");*/
	
	var zwf, color;
	if (spectrum_display && need_update) {
		for (var x=0; x<w; x++) {
			// wf
			zwf = waterfall_color_index(data[x]);
			color = color_map[zwf];
			for (var i=0; i<4; i++)
				oneline_image.data[x*4+i] = ((color>>>0) >> ((3-i)*8)) & 0xff;
			
			// spectrum
			if (x < sw) {
			
				// non-linear spectrum filter from Rocky (Alex, VE3NEA)
				// see http://www.dxatlas.com/rocky/advanced.asp
				iir_gain[x] = 1 - Math.exp(-0.2 * zwf/255)
				var z = specavg[x];
				z = z + iir_gain[x] * (zwf - z);
				if (z > 255) z = 255; if (z < 0) z = 0;
				specavg[x] = z;

				var y = Math.round((1 - z/255) * sh), sy2;
				// fixme: could optimize amount of data moved like smeter
				spectrum_ctx.putImageData(spectrum_data, x, 0, 0, y, 1, sh-y+1);
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
	
	canvas_context.putImageData(oneline_image, 0, canvas_actual_line);
	
	// if data from server hasn't caught up to our panning or zooming then fix it
	var pixel_dx;
//foo
if (sb_trace) console.log('WF bin='+x_bin+'/'+x_bin_server+' z='+zoom_level+'/'+x_zoom_server);
	if (zoom_level != x_zoom_server) {
		var dz = zoom_level - x_zoom_server;
		var out = dz < 0;
		var dbins = Math.abs(x_bin_server - x_bin);
		var pixel_dx = bins_to_pixels(2, dbins, out? zoom_level:x_zoom_server);
if (sb_trace)
		console.log("Zcatchup z"+x_zoom_server+'>'+zoom_level+' b='+x_bin+'/'+x_bin_server+'/'+dbins+" px="+pixel_dx);
		waterfall_zoom(canvases[0], canvas_context, dz, canvas_actual_line, pixel_dx);
if (sb_trace) console.log('WF z fix');
	} else
	
	if (x_bin != x_bin_server) {
		pixel_dx = bins_to_pixels(3, x_bin - x_bin_server, zoom_level);
		waterfall_pan(canvases[0], canvas_context, canvas_actual_line, pixel_dx);
		//console.log("FIX L="+canvas_actual_line+" xb="+x_bin+" xbs="+x_bin_server+" pdx="+pixel_dx));
if (sb_trace) console.log('WF bin fix');
	}
if (sb_trace && x_bin == x_bin_server && zoom_level == x_zoom_server) sb_trace=0;
	
	canvas_actual_line--;
	shift_canvases();
	if(canvas_actual_line<0) add_canvas();
}

function waterfall_pan(cv, ctx, line, dx)
{
	var y, w, h;
	if (line == -1) {
		y = 0;
		h = cv.height;
	} else {
		y = line;
		h = 1;
	}

	if (dx < 0) {		// pan right (toward higher freqs)
		dx = -dx;
		w = cv.width-dx;
		try {
			if (w != 0) ctx.drawImage(cv, 0,y,w,h, dx,y,w,h);
		} catch(ex) {
		   console.log("EX dx="+dx+" y="+y+" w="+w+" h="+h);		// fixme remove
		}
		ctx.fillStyle = "Black";
		//ctx.fillStyle = (line==-1)? "Green":"Red";
		ctx.fillRect(0,y,dx,h);
	} else {				// pan left (toward lower freqs)
		w = cv.width-dx;
		if (w != 0) ctx.drawImage(cv, dx,y,w,h, 0,y,w,h);
		ctx.fillStyle = "Black";
		//ctx.fillStyle = (line==-1)? "Green":"Red";
		ctx.fillRect(w,y,cv.width,h);
	}
}

// gave to keep track of fractional pixels while panning
var last_pixels_frac = 0;

function waterfall_pan_canvases(bins)
{
	var cv, ctx;
	if (!bins) return;
	
	var x_obin = x_bin;
	x_bin += bins;
	x_bin = clamp_xbin(x_bin);
	//console.log("PAN lpf="+last_pixels_frac);
	var f_dx = bins_to_pixels_frac(4, x_bin - x_obin, zoom_level) + last_pixels_frac;		// actual movement allowed due to clamping
	var i_dx = Math.round(f_dx);
	last_pixels_frac = f_dx - i_dx;
	//console.log("PAN z="+zoom_level+" xb="+x_bin+" db="+(x_bin - x_obin)+" f_dx="+f_dx+" i_dx="+i_dx+" lpf="+last_pixels_frac);
	if (!i_dx) return;

	for (var i=0; i<canvases.length; i++) {
		cv = canvases[i];
		ctx = cv.getContext("2d");
		waterfall_pan(cv, ctx, -1, i_dx);
	}
	
	ws_fft_send("SET zoom="+ zoom_level +" start="+ x_bin);
	
	mkscale();
	need_clear_specavg = true;
	dx_schedule_update();
}

function waterfall_zoom(cv, ctx, dz, line, x)
{
	var w = cv.width;
	var nw = w / (1 << Math.abs(dz));
	var y, h;
	if (line == -1) {
		y = 0;
		h = cv.height;
	} else {
		y = line;
		h = 1;
	}

	if (dz < 0) {		// zoom out
		if (w != 0) ctx.drawImage(cv, 0,y,w,h, x,y,nw,h);
		ctx.fillStyle = "Black";
		ctx.fillRect(0,y,x,y+h);
		ctx.fillRect(x+nw,y,w,y+h);
	} else {			// zoom in
		try {
			if (w != 0) {
				if (sb_trace) console.log('WFZ x='+x+' y='+y+' nw='+nw+' w='+w+' h='+h+' dz='+dz);
				ctx.drawImage(cv, x,y,nw,h, 0,y,w,h);
				ctx.fillStyle = "Black";
				ctx.fillRect(0,y,x,y+h);
				ctx.fillRect(x+nw,y,w,y+h);
			}
		} catch(ex) {
		   console.log("EX2 dz="+dz+" x="+x+" y="+y+" w="+w+" h="+h);		// fixme remove
		}
	}
	
	accum_pixels_frac = 0;
}

function waterfall_zoom_canvases(dz, x)
{
	var cv, ctx;

	for (var i=0; i<canvases.length; i++) {
		cv = canvases[i];
		ctx = cv.getContext("2d");
		waterfall_zoom(cv, ctx, dz, -1, x);
	}
	
	need_clear_specavg = true;
	//console.log("ZOOM z"+zoom_level+" xb="+x_bin+" x="+x);
}

function resize_waterfall_container(check_init)
{
	if(check_init && !waterfall_setup_done) return;
	canvas_container.style.height = (window.innerHeight - html("id-top-container").clientHeight - html("id-scale-container").clientHeight).toString()+"px";
}

function waterfall_add_queue(what)
{
	waterfall_queue.push(what);
}

var init_zoom_set = false;

function waterfall_dequeue()
{
	if(waterfall_queue.length) waterfall_add(waterfall_queue.shift());

	if(fft_fps < 0 || waterfall_queue.length > Math.max(fft_fps/2,8)) //in case of emergency 
	{
		if (fft_fps > 0) {
		   console.log("w/f qov "+waterfall_queue.length);
			add_problem("fft overflow");
		}
		while(waterfall_queue.length) waterfall_add(waterfall_queue.shift());
	}
	
	// demodulator must have been initialized before calling zoom_step()
	if (init_zoom && !init_zoom_set && audio_started) {
		init_zoom = parseInt(init_zoom);
		if (init_zoom < 0 || init_zoom > zoom_levels_max) init_zoom = 0;
		console.log("init_zoom="+init_zoom);
		zoom_step(zoom.abs, init_zoom);
		init_zoom_set = true;
	}
}

//var color_scale=[0xFFFFFFFF, 0x000000FF];
//var color_scale=[0x000000FF, 0x000000FF, 0x3a0090ff, 0x10c400ff, 0xffef00ff, 0xff5656ff];
//var color_scale=[0x000000FF, 0x000000FF, 0x534b37ff, 0xcedffaff, 0x8899a9ff,  0xfff775ff, 0xff8a8aff, 0xb20000ff];

//var color_scale=[ 0x000000FF, 0xff5656ff, 0xffffffff];

//2014-04-22
var color_scale=[0x2e6893ff, 0x69a5d0ff, 0x214b69ff, 0x9dc4e0ff,  0xfff775ff, 0xff8a8aff, 0xb20000ff];

var orig_colors = 0;
var color_map = new Uint32Array(256);

function mkcolormap()
{
	for (var i=0; i<256; i++) {
		var r, g, b;
		
		if (orig_colors) {
			if (i<64)	{ r = 0;				g = 0;									b = i*2;			} else
			if (i<128)	{ r = i*3-192;		g = 0;									b = i*2;			} else
			if (i<192)	{ r = i+64;			g = Math.sqrt((i-128)/64)*256;	b = 511-i*2;	} else
							{ r = 255;			g = 255;									b = i-256*2;	}
		} else {		// CuteSDR
			if (i<43)	{ r = 0;						g = 0;							b = i*255/43;				} else
			if (i<87)	{ r = 0;						g = (i-43)*255/43;			b = 255;						} else
			if (i<120)	{ r = 0;						g = 255;							b = 255-(i-87)*255/32;	} else
			if (i<154)	{ r = (i-120)*255/33;	g = 255;							b = 0;						} else
			if (i<217)	{ r = 255;					g = 255-(i-154)*255/62;		b = 0;						} else
							{ r = 255;					g = 0;							b = (i-217)*128/38;		}
		}

		color_map[i] = (r<<24) | (g<<16) | (b<<8) | 0xff;
	}
}

function waterfall_color_index(db_value)
{
	// convert to negative-only signed dBm (i.e. -256 to -1 dBm)
	// done this way because -127 is the smallest value in an int8 which isn't enough
	db_value = -(255 - db_value);		
	
	if (db_value < mindb) db_value = mindb;
	if (db_value > maxdb) db_value = maxdb;
	relative_value = db_value - mindb;
	value_percent = relative_value/full_scale;
	
	var i = value_percent*255;
	i = Math.round(i);
	if (i<0) i=0;
	if (i>255) i=255;
	return i;
}

function waterfall_mkcolor(db_value)
{
	if (db_value < mindb) db_value = mindb;
	if (db_value > maxdb) db_value = maxdb;
	relative_value = db_value - mindb;
	value_percent = relative_value/full_scale;
	
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

var freq_displayed_Hz, freq_car_Hz;
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

function passband_offset_dxlabel(mode)
{
	var offset=0;
	var usePBCenter = false;
	var pb = passbands[mode];
	var usePBCenter = (mode == 'usb' || mode == 'lsb');
	offset = usePBCenter? pb.lo + (pb.hi - pb.lo)/2 : 0;
	//console.log("passband_offset: m="+mode+" use="+usePBCenter+' o='+offset);
	return offset;
}

function freqmode_set_dsp_kHz(fdsp, mode)
{
	fdsp *= 1000;
	//console.log("freqmode_set_dsp_kHz: fdsp="+fdsp+' mode='+mode);

	if (mode != null && mode != cur_mode) {
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
	//console.log("FUPD-UI freq_car_Hz="+freq_car_Hz+' NEW freq_displayed_Hz='+freq_displayed_Hz);
	var obj = html('id-freq-input');
	if (typeof obj == "undefined" || obj == null) return;		// can happen if SND comes up long before W/F
	obj.value = (freq_displayed_Hz/1000).toFixed(2);
	//console.log("FUPD obj="+(typeof obj)+" val="+obj.value);
	//obj.focus();
	if (!kiwi_is_iOS()) obj.select();
	
	// re-center if the new passband is outside the current waterfall 
	var pb_bin = -passband_visible() - 1;
	//console.log("RECEN pb_bin="+pb_bin+" x_bin="+x_bin+" f(x_bin)="+bin_to_freq(x_bin));
	if (pb_bin >= 0) {
		var wf_middle_bin = x_bin + bins_at_cur_zoom()/2;
		//console.log("RECEN YES pb_bin="+pb_bin+" wfm="+wf_middle_bin+" dbins="+(pb_bin - wf_middle_bin));
		waterfall_pan_canvases(pb_bin - wf_middle_bin);		// < 0 = pan left (toward lower freqs)
	}
	
	// reset "select band" menu if freq is no longer inside band
	if (last_selected_band) {
		band = band_menu[last_selected_band];
		if (freq_car_Hz < band.min || freq_car_Hz > band.max) {
			//console.log("SB-out"+last_selected_band+" f="+freq_car_Hz+' '+band.min+'/'+band.max);
			html('select-band').value = 0;
			last_selected_band = 0;
		}
	}
	
	freq_step_update_ui();
}

function freqset_select()
{
	var obj = html('id-freq-input');
	//obj.focus();
	//console.log("FS iOS="+kiwi_is_iOS());
	if (!kiwi_is_iOS()) obj.select();
}

var last_mode_obj = null;

function modeset_update_ui(mode)
{
	if (last_mode_obj != null) last_mode_obj.style.color = "white";
	var obj = html('button-'+mode);
	//obj.style.color = "#FFFF50";
	obj.style.color = "lime";
	last_mode_obj = obj;
}

function freqset_complete()
{
	kiwi_clearTimeout(freqset_tout);
	var obj = html('id-freq-input');
	//console.log("FCMPL obj="+(typeof obj)+" val="+(obj.value).toString());
	var f = parseFloat(obj.value);
	//console.log("FCMPL2 obj="+(typeof obj)+" val="+(obj.value).toString());
	if (f > 0 && !isNaN(f)) {
		freqmode_set_dsp_kHz(f, null);
		obj.select();
	}
}

var ignore_next_keyup_event = false;

function freqset_keyup(obj, evt)
{
	kiwi_clearTimeout(freqset_tout);
	//console.log("FKU obj="+(typeof obj)+" val="+obj.value);
	
	// ignore modifier keys used with mouse events that also appear here
	if (ignore_next_keyup_event) {
		//console.log("ignore shift-key freqset_keyup");
		ignore_next_keyup_event = false;
		return;
	}
	
	freqset_tout = setTimeout('freqset_complete()', 3000);
}

var num_step_buttons = 6;

var up_down = {
	am: [0, -1, -0.1, 0.1, 1, 0 ],
	amn: [0, -1, -0.1, 0.1, 1, 0 ],
	usb: [-5, -1, -0.1, 0.1, 1, 5 ],
	lsb: [-5, -1, -0.1, 0.1, 1, 5 ],
	cw: [0, -0.1, -0.01, 0.01, 0.1, 0 ],
	cwn: [0, -0.1, -0.01, 0.01, 0.1, 0 ]
};

var step_default_AM = 10000, step_default_CW = 1000;
var NDB_400_1000_mode = 1;		// special 400/1000 step mode for NDB band

function freqstep(sel)
{
	if (0 && dbgUs && (sel == 2 || sel == 3)) {
		ws_aud_send("SET spi_delay="+((sel == 2)? -1:1));
		return;
	}

	var step_Hz = up_down[cur_mode][sel]*1000;
	
	// set step size from band channel spacing
	if (step_Hz == 0) {
		var b = find_band(freq_displayed_Hz);
		if (cur_mode == 'cw' || cur_mode == 'cwn') {
			if (b != null && b.name == 'NDB') {
				step_Hz = NDB_400_1000_mode;
			} else {
				step_Hz = step_default_CW;
			}
		} else
		if (b != null && b.chan != 0) {
			step_Hz = b.chan;
			//console.log('STEP '+step_Hz+' band='+b.name);
		} else {
			step_Hz = step_default_AM;
			//console.log('STEP '+step_Hz+' no band chan found');
		}
		if (sel < num_step_buttons/2) step_Hz = -step_Hz;
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
		console.log("STEP-400/1000 kHz="+kHz+" trunc="+trunc+" fnew="+fnew);
	} else
	if (freq_displayed_Hz != trunc) {
		fnew = trunc;
		took = 'TRUNC';
	} else {
		fnew += step_Hz;
		took = 'INC';
	}
	console.log('STEP '+cur_mode+' fold='+freq_displayed_Hz+' inc='+incHz+' trunc='+trunc+' fnew='+fnew+' '+took);
	freqmode_set_dsp_kHz(fnew/1000, null);
}

var freq_step_last_mode, freq_step_last_band;

function freq_step_update_ui()
{
	if (typeof cur_mode == "undefined") return;
	var b = find_band(freq_displayed_Hz);
	
	//console.log("freq_step_update_ui: lm="+freq_step_last_mode+' cm='+cur_mode);
	if (freq_step_last_mode == cur_mode && freq_step_last_band == b) {
		//console.log("freq_step_update_ui: return "+freq_step_last_mode+' '+cur_mode);
		return;
	}
	
	for (var i=0; i < num_step_buttons; i++) {
		var step_Hz = up_down[cur_mode][i]*1000;
		if (step_Hz == 0) {
			if (cur_mode == 'cw' || cur_mode == 'cwn') {
				if (b != null && b.name == 'NDB') {
					step_Hz = NDB_400_1000_mode;
				} else {
					step_Hz = step_default_CW;
				}
			} else
			if (b != null && b.chan != 0) {
				step_Hz = b.chan;
			} else {
				step_Hz = step_default_AM;
			}
			if (i < num_step_buttons/2) step_Hz = -step_Hz;
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

function bands_init()
{
	var i, z;
	for (i=0; i < bands.length; i++) {
		var b = bands[i];
		bands[i].chan = (typeof b.chan == "undefined")? 0 : b.chan;
		b.min -= b.chan/2; b.max += b.chan/2;
		b.min *= 1000; b.max *= 1000; b.chan *= 1000;
		var bw = b.max - b.min;
		for (z=zoom_levels_max; z >= 0; z--) {
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
			break;
	}
	if (i == bands.length) return null; else return b;
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
	//console.log("BB fftw="+fft_size+" tw="+tw+" rs="+r.start+" re="+r.end+" bw="+(r.end-r.start));
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
		html('select-band').value = 0;
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
//foo
	//console.log("SEL BAND"+op+" "+b.name+" freq="+freq+((mode != null)? " mode="+mode:""));
	last_selected_band = op;
//if (dbgUs) sb_trace=1;
	freqmode_set_dsp_kHz(freq, mode);
	zoom_step(zoom.to_band, b);		// pass band to disambiguate nested bands in band menu
}
var sb_trace=0;

function tune(freq, mode) { freqmode_set_dsp_kHz(freq, mode); zoom_step(zoom.to_band); }

var maxdb = init_max_dB;
var mindb_un = init_min_dB;
var mindb = mindb_un;
var full_scale = maxdb - mindb;
var barmax = 0, barmin = -170		// mindb @ z0 - zoom_correction * zoom_levels_max

function init_scale_dB()
{
	maxdb = init_max_dB;
	mindb_un = init_min_dB;
	mindb = mindb_un;
	full_scale = maxdb - mindb;

}

init_scale_dB();

var muted = false;
//var muted = true;		// beta: remove once working
var volume = 50;
var f_volume = muted? 0 : volume/100;


////////////////////////////////
// dx (markers)
////////////////////////////////

var dx_update_delay = 350;
var dx_update_timeout, dx_seq=0, dx_idx, dx_z;

function dx_schedule_update()
{
	kiwi_clearTimeout(dx_update_timeout);
	dx_div.innerHTML = "";
	dx_update_timeout = setTimeout('dx_update()', dx_update_delay);
}

function dx_update()
{
	dx_seq++;
	dx_idx = 0; dx_z = 120;
	//g_range = get_visible_freq_range();
	//console.log("DX min="+(g_range.start/1000)+" max="+(g_range.end/1000));
	kiwi_ajax("/MKR?min="+(g_range.start/1000).toFixed(3)+"&max="+(g_range.end/1000).toFixed(3)+"&zoom="+zoom_level+"&width="+waterfall_width, true);
}

// Why doesn't using addEventListener() to ignore mousedown et al not seem to work for
// div elements created appending to innerHTML?

var DX_MODE = 0xf;
var modes_i = { 0:'AM', 1:'AMN', 2:'USB', 3:'LSB', 4:'CW', 5:'CWN', 6:'DATA' };
var modes_s = { 'am':0, 'amn':1, 'usb':2, 'lsb':3, cw:4, 'cwn':5, 'data':6 };

var DX_TYPE = 0xf0;
var DX_TYPE_SFT = 4;
var types = { 0:'normal', 1:'watch-list', 2:'sub-band', 3:'DGPS', 4:'NoN' , 5:'interference' };
var type_colors = { 0:'cyan', 0x10:'lightPink', 0x20:'aquamarine', 0x30:'lavender', 0x40:'violet' , 0x50:'violet' };

var dx_list = [];

function dx(gid, freq, moff, flags, ident)
{
	if (gid == -1) {
		dx_idx = 0; dx_z = 120;
		dx_div.innerHTML = '';
		return;
	}
	
	var notes = (arguments.length > 5)? arguments[5] : "";
	var freqHz = freq * 1000;
	var loff = passband_offset_dxlabel(modes_i[flags & DX_MODE].toLowerCase());	// always place label at center of passband
	var x = scale_px_from_freq(freqHz + loff, g_range) - 1;	// fixme: why are we off by 1?
	var cmkr_x = 0;		// optional carrier marker for NDBs
	if (moff) cmkr_x = scale_px_from_freq(freqHz - moff, g_range);

	var t = dx_label_top + (30 * (dx_idx&1));		// stagger the labels vertically
	var h = dx_container_h - t;
	//console.log("DX "+dx_seq+':'+dx_idx+" f="+freq+" o="+loff+" k="+moff+" F="+flags+" m="+modes_i[flags & DX_MODE]+" <"+ident+"> <"+notes+'>');
	
	dx_list[gid] = { "gid":gid, "freq":freq, "moff":moff, "flags":flags, "ident":ident, "notes":notes };
	//console.log(dx_list[gid]);
	
	var s =
		'<div id="'+dx_idx+'-id-dx-label" class="class-dx-label '+ gid +'-id-dx-gid" style="left:'+(x-10)+'px; top:'+t+'px; z-index:'+dx_z+'; ' +
			'background-color:'+ type_colors[flags & DX_TYPE] +';" ' +
			'onmouseenter="dx_enter(this,'+ cmkr_x +')" onmouseleave="dx_leave(this,'+ cmkr_x +')" ' +
			'onmousedown="ignore(event)" onmousemove="ignore(event)" onmouseup="ignore(event)" onclick="dx_click(event,'+ gid +')">' +
		'</div>' +
		'<div class="class-dx-line" id="'+dx_idx+'-id-dx-line" style="left:'+x+'px; top:'+t+'px; height:'+h+'px; z-index:110"></div>';
	//console.log(s);
	dx_div.innerHTML += s;
	var el = html_id(dx_idx+'-id-dx-label');
	el.innerHTML = decodeURIComponent(ident);
	el.title = decodeURIComponent(notes);
	dx_idx++; dx_z++;
}

function dx_init_panel()
{
	html('id-dx').innerHTML =
		w3_div('id-dx-edit', 'class-panel-inner') +
		w3_div('id-dx-vis class-vis', '');
}

var dxo = { };
var dx_admin = false;

// note that an entry can be cloned by selecting it, but then using the "add" button instead of "modify"
function dx_show_edit_panel(gid)
{
	dxo.gid = gid;
	if (!dx_admin) {
		// try with null password in case local subnet login option is set
		var pwd = '';
		kiwi_ajax("/PWD?cb=dx_admin_cb&type=admin&pwd=x"+ pwd, true);	// prefix pwd with 'x' in case empty
		return;
	}
	dx_show_edit_panel2();
}

function dx_admin_cb(badp)
{
	if (!badp) {
		dx_admin = true;
		dx_show_edit_panel2();
		return;
	}

	var el = html('id-dx-edit');
	var s =
		w3_input('Password', 'dxo.p', '', 64, 'dx_pwd_cb', 'admin password required to edit marker list') +
		'';
	el.innerHTML = s;
	//console.log(s);
	
	el.innerHTML = s;
	el = html('id-dx');
	el.style.zIndex = 150;
	el.style.visibility = 'visible';
}

function dx_pwd_cb(el, val)
{
	dx_string_cb(el, val);
	kiwi_ajax("/PWD?cb=dx_admin_cb&type=admin&pwd=x"+ val, true);	// prefix pwd with 'x' in case empty
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
		dxo.f = (freq_displayed_Hz/1000).toFixed(2);
		dxo.o = 0;
		dxo.m = modes_s[cur_mode]+1;
		dxo.y = 0;		// force menu title to be shown
		dxo.i = dxo.n = '';
	} else {
		//console.log('DX EDIT entry #'+ gid +' prev: f='+ dx_list[gid].freq +' flags='+ dx_list[gid].flags.toHex() +' i='+ dx_list[gid].ident +' n='+ dx_list[gid].notes);
		dxo.f = dx_list[gid].freq.toFixed(2);		// starts as a string, validated to be a number
		dxo.o = dx_list[gid].moff;
		dxo.m = (dx_list[gid].flags & DX_MODE) +1;		// account for menu title
		dxo.y = ((dx_list[gid].flags & DX_TYPE) >> DX_TYPE_SFT) +1;
		dxo.i = decodeURIComponent(dx_list[gid].ident);
		dxo.n = decodeURIComponent(dx_list[gid].notes);
		//console.log('dxo.i='+ dxo.i);
	}
	//console.log(dxo);

	var el = html('id-dx-edit');
	var s =
		w3_div('',
			w3_col_percent('w3-vcenter', 'w3-margin-8',
				w3_input('Freq', 'dxo.f', dxo.f, 16, 'dx_int_cb'), 30,
				w3_select('Mode', 'dxo.m', dxo.m, modes_i, 'dx_sel_cb'), 15,
				w3_select('Type', 'dxo.y', dxo.y, types, 'dx_sel_cb'), 25,
				w3_input('Offset', 'dxo.o', dxo.o, 16, 'dx_int_cb'), 30
			)
		) +
		
		w3_input('Ident', 'dxo.i', '', 64, 'dx_string_cb') +
		w3_input('Notes', 'dxo.n', '', 64, 'dx_string_cb') +
		
		w3_div('',
			w3_btn('Modify', 'dx_modify_cb', 'w3-margin') +
			w3_btn('Add', 'dx_add_cb', 'w3-margin') +
			w3_btn('Delete', 'dx_delete_cb', 'w3-margin')
		) +
		'';
	
	el.innerHTML = s;
	//console.log(s);
	
	// can't do this as initial val passed to w3_input above when string contains quoting
	html_idname('dxo.i').value = dxo.i;
	html_idname('dxo.n').value = dxo.n;
	
	el = html('id-dx');
	el.style.zIndex = 150;
	el.style.visibility = 'visible';
}

/*
	FIXME input validation issues:
		data out-of-range
		data missing
		what should it mean? delete button, but params have been changed (e.g. freq)
		SECURITY: can eval arbitrary code input?
*/

function dx_int_cb(el, val)
{
	var v = parseFloat(val);
	if (isNaN(v)) v = 0;
	//console.log('dx_int_cb: el='+ el +' val='+ val +' v='+ v);
	eval(el +' = '+ v);
}

function dx_sel_cb(el, val)
{
	//console.log('dx_sel_cb: el='+ el +' val='+ val);
	eval(el +' = '+ val.toString());
}

function dx_string_cb(el, val)
{
	//console.log('dx_string_cb: el='+ el +' val='+ val);
	eval(el +' = \"'+ val +'\"');
}

function dx_close_edit_panel(id)
{
	w3_radio_unhighlight(id);
	html('id-dx').style.visibility = 'hidden';
	
	// NB: Can't simply do a dx_schedule_update() here as there is a race for the server to
	// update the dx list before we can pull it again. Instead, the add/modify/delete ajax
	// response will call dx_update() directly when the server has updated.
}

function dx_modify_cb(id, val)
{
	//console.log('DX COMMIT modify entry #'+ dxo.gid +' f='+ dxo.f);
	//console.log(dxo);
	if (dxo.m == 0) dxo.m = 1;
	var mode = dxo.m - 1;		// account for menu title
	if (dxo.y == 0) dxo.y = 1;
	var type = (dxo.y - 1) << DX_TYPE_SFT;
	mode |= type;
	kiwi_ajax('/UPD?g='+ dxo.gid +'&f='+ dxo.f +'&o='+ dxo.o +'&m='+ mode +
		'&i='+ encodeURIComponent(dxo.i) +'&n='+ encodeURIComponent(dxo.n), true);
	setTimeout('dx_close_edit_panel('+ q(id) +')', 250);
}

function dx_add_cb(id, val)
{
	//console.log('DX COMMIT new entry');
	//console.log(dxo);
	if (dxo.m == 0) dxo.m = 1;
	var mode = dxo.m - 1;
	if (dxo.y == 0) dxo.y = 1;
	var type = (dxo.y - 1) << DX_TYPE_SFT;
	mode |= type;
	var s = '/UPD?g=-1&f='+ dxo.f +'&o='+ dxo.o +'&m='+ mode +
		'&i='+ encodeURIComponent(dxo.i) +'&n='+ encodeURIComponent(dxo.n);
	//console.log(s);
	kiwi_ajax(s, true);
	setTimeout('dx_close_edit_panel('+ q(id) +')', 250);
}

function dx_delete_cb(id, val)
{
	//console.log('DX COMMIT delete entry #'+ dxo.gid);
	//console.log(dxo);
	kiwi_ajax('/UPD?g='+ dxo.gid +'&f=-1', true);
	setTimeout('dx_close_edit_panel('+ q(id) +')', 250);
}

function dx_click(ev, gid)
{
	if (ev.shiftKey) {
		dx_show_edit_panel(gid);
	} else {
		mode = modes_i[dx_list[gid].flags & DX_MODE].toLowerCase();
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
var SMETER_SCALE_HEIGHT = 27;
var SMETER_BIAS = 127;
var SMETER_MAX = 3.4;
var sMeter_dBm_biased = 0;
var sMeter_ctx;

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
	html('id-params-smeter').innerHTML =
		'<canvas id="id-smeter-scale" class="class-smeter-scale" width="0" height="0"></canvas>';
	var sMeter_canvas = html('id-smeter-scale');

	smeter_width = divClientParams.activeWidth - html_LR_border_pad(sMeter_canvas);		// less our own border/padding
	
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
	setInterval('update_smeter()', 100);
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
}


////////////////////////////////
// user list
////////////////////////////////

var users_interval = 2500, stats_interval = 5000;
var stats_check = stats_interval / users_interval, stats_seq = 0;
var user_init = false;

function users_init()
{
	console.log("users_init #rx="+rx_chans);
	for (var i=0; i < rx_chans; i++) divlog('RX'+i+': <span id="id-user-'+i+'"></span>');
	setTimeout('users_update()', users_interval);
	user_init = true;
}

var need_config = 1, need_update = 1;

function users_need_ver_update()
{
	setTimeout(function() { need_update = 1; }, 3000);
}

function users_update()
{
	//console.log('users_update: need_config='+ need_config +' need_update='+ need_update);
	var qs = 'stats='+ (((stats_seq % stats_check) == 0)? 1:0) +'&config='+ need_config +'&update='+ need_update +'&ch='+ rx_chan;
	need_config = need_update = 0;
	kiwi_ajax('/USR?'+qs, true); stats_seq++;
	setTimeout('users_update()', users_interval);
}

function user(i, name, geoloc, freq, mode, zoom, connected)
{
	//console.log('user'+i+' n='+name);
   var f = (freq/1000).toFixed(2);
	var s = '', g = '';
	if (geoloc != '(null)') g = ' ('+geoloc+')';
   if (name) s = '"'+name+'"'+g+' '+f+' kHz'+' '+mode+' z'+zoom+' '+connected;
   s = kiwi_strip_tags(s, '');
   //console.log('user innerHTML = '+s);
   if (user_init) html('id-user-'+i).innerHTML = s;
}


////////////////////////////////
// user ident
////////////////////////////////

var ident_tout;
var ident_name = null;
var need_name = false;

function ident_init()
{
	var name = initCookie('ident', "");
	name = kiwi_strip_tags(name, '');
	//console.log("IINIT name=<"+name+'>');
	html('input-ident').value = name;
	ident_name = name;
	need_name = true;
}

function ident_complete()
{
	kiwi_clearTimeout(ident_tout);
	var obj = html('input-ident');
	var name = obj.value;
	name = kiwi_strip_tags(name, '');
	obj.value = name;
	console.log("ICMPL obj="+(typeof obj)+" name=<"+name+'>');
	// okay for name="" to erase it
	// fixme: size limited by <input size=...> but guard against binary data injection?
	obj.select();
	writeCookie('ident', name);
	ident_name = name;
	need_name = true;
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
	
	ident_tout = setTimeout('ident_complete()', 5000);
}


////////////////////////////////
// stats update
////////////////////////////////

function update_TOD()
{
	//console.log('update_TOD');
	var i1 = html('id-info-1');
	var i2 = html('id-info-2');
	var d = new Date();
	var s = d.getSeconds() % 30;
	//if (s >= 16 && s <= 25) {
	if (true) {
		i1.style.fontSize = i2.style.fontSize = (conn_type == 'admin')? '100%':'60%';
		i1.innerHTML = kiwi_cpu_stats_str;
		i2.innerHTML = kiwi_audio_stats_str;
	} else {
		i1.style.fontSize = i2.style.fontSize = '85%';
		i1.innerHTML = d.toString();
		i2.innerHTML = d.toUTCString();
	}
}


// ========================================================
// =======================  >PANELS  ======================
// ========================================================

var panel_margin = 10;

// called from waterfall_init()
function panels_setup()
{
	var td = function(inner, _id) {
		var id = (typeof _id != "undefined")? 'id="'+_id+'"' : '';
		var td = '<span '+id+' class="class-td">'+inner+'</span>';
		return td;
	}

	// id-client-params
	
	html("id-ident").innerHTML =
		'<div><form name="form_ident" action="#" onsubmit="ident_complete(); return false;">' +
			'Your name or callsign: <input id="input-ident" type="text" size=32 onkeyup="ident_keyup(this, event);">' +
		'</form></div>';

	html("id-params-1").innerHTML =
		td('<form id="id-freq-form" name="form_freq" action="#" onsubmit="freqset_complete(); return false;">' +
			'<input id="id-freq-input" type="text" size=9 onkeyup="freqset_keyup(this, event);"> kHz' +
		'</form>', 'id-freq-cell') +
		td('<select id="select-band" onchange="select_band(this.value)">' +
				'<option value="0" selected disabled>select band</option>' +
				setup_band_menu() +
		'</select>', 'select-band-cell') +
		td('<span id="id-iOS-button" class="class-button"><span id="id-iOS-text" onclick="iOS_audio_start();">press</span></span>', 'id-iOS-cell');
	
	if (kiwi_is_iOS()) {
	//if (true) {		// beta: remove once working
		var iOS = html('id-iOS-text');
		iOS.state = 1;
		iOS.texts = [ 'press', 'for', 'iOS', 'audio' ];
		iOS.button_timer =
			setInterval('var iOS = html("id-iOS-text"); animate(iOS, "opacity", "", iOS.state&1^1, iOS.state&1, 1, 500, 30, \
				function() { iOS.innerHTML = iOS.texts[iOS.state>>1&3]; }); iOS.state+=1;', 750);
		html('id-iOS-cell').style.display = "table-cell";
	}
	
	html("id-params-2").innerHTML =
		'<div id="id-mouse-freq" class="class-td"><span id="id-mouse-unit">-----.--</span><span id="id-mouse-suffix">kHz</span></div>' +
		'<div id="id-step-freq" class="class-td">' +
			'<img id="id-step-0" src="icons/stepdn.20.png" onclick="freqstep(0)" />' +
			'<img id="id-step-1" src="icons/stepdn.18.png" onclick="freqstep(1)" style="padding-bottom:1px" />' +
			'<img id="id-step-2" src="icons/stepdn.16.png" onclick="freqstep(2)" style="padding-bottom:2px" />' +
			'<img id="id-step-3" src="icons/stepup.16.png" onclick="freqstep(3)" style="padding-bottom:2px" />' +
			'<img id="id-step-4" src="icons/stepup.18.png" onclick="freqstep(4)" style="padding-bottom:1px" />' +
			'<img id="id-step-5" src="icons/stepup.20.png" onclick="freqstep(5)" />' +
		'</div>' +
		td('<div id="button-spectrum" class="class-button" onclick="toggle_or_set_spec();">Spectrum</div>');

	html("id-params-3").innerHTML =
		td('<div id="button-am" class="class-button" onclick="demodulator_analog_replace(\'am\');">AM</div>') +
		td('<div id="button-amn" class="class-button" onclick="demodulator_analog_replace(\'amn\');">AMN</div>') +
		td('<div id="button-lsb" class="class-button" onclick="demodulator_analog_replace(\'lsb\');">LSB</div>') +
		td('<div id="button-usb" class="class-button" onclick="demodulator_analog_replace(\'usb\');">USB</div>') +
		td('<div id="button-cw" class="class-button" onclick="demodulator_analog_replace(\'cw\');">CW</div>') +
		td('<div id="button-cwn" class="class-button" onclick="demodulator_analog_replace(\'cwn\');">CWN</div>') +
		td('<div id="button-cwn" class="class-button" onclick="demodulator_analog_replace(\'cwn\');">Data</div>');

	html("id-params-4").innerHTML =
		td('<div class="class-icon" onclick="zoom_step(1);" title="zoom in"><img src="icons/zoomin.png" width="32" height="32" /></div>') +
		td('<div class="class-icon" onclick="zoom_step(-1);" title="zoom out"><img src="icons/zoomout.png" width="32" height="32" /></div>') +
		td('<div class="class-icon" onclick="zoom_step(9);" title="max in"><img src="icons/maxin.png" width="32" height="32" /></div>') +
		td('<div class="class-icon" onclick="zoom_step(-9);" title="max out"><img src="icons/maxout.png" width="32" height="32" /></div>') +
		td('<div class="class-icon" onclick="zoom_step(0);" title="zoom to band">' +
			'<img src="icons/zoomband.png" width="32" height="16" style="padding-top:13px; padding-bottom:13px;"/>' +
		'</div>') +
		td('<div class="class-icon" onclick="page_scroll(-'+page_scroll_amount+')" title="page down"><img src="icons/pageleft.png" width="32" height="32" /></div>') +
		td('<div class="class-icon" onclick="page_scroll('+page_scroll_amount+')" title="page up"><img src="icons/pageright.png" width="32" height="32" /></div>');

	html("id-params-sliders").innerHTML =
		w3_col_percent('w3-vcenter', '',
			w3_div('slider-maxdb class-slider', ''), 70,
			w3_div('field-maxdb class-slider', ''), 30
		) +
		w3_col_percent('w3-vcenter', '',
			w3_div('slider-mindb class-slider', ''), 70,
			w3_div('field-mindb class-slider', ''), 30
		) +
		w3_col_percent('w3-vcenter', '',
			w3_div('slider-volume class-slider', ''), 70,
			'<div id="button-mute" class="class-button" onclick="toggle_mute();">Mute</div>', 30
		);

	html('button-mute').style.color = muted? 'lime':'white';

	html('slider-maxdb').innerHTML =
		'WF max: <input id="input-maxdb" type="range" min="-100" max="20" value="'+maxdb+'" step="1" onchange="setmaxdb(1,this.value)" oninput="setmaxdb(0, this.value)">';

	html('slider-mindb').innerHTML =
		'WF min: <input id="input-mindb" type="range" min="-190" max="-30" value="'+mindb+'" step="1" onchange="setmindb(1,this.value)" oninput="setmindb(0, this.value)">';

	html('slider-volume').innerHTML =
		'Volume: <input id="input-volume" type="range" min="0" max="200" value="'+volume+'" step="1" onchange="setvolume(1, this.value)" oninput="setvolume(0, this.value)">';


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
		<li> Please <a href="javascript:sendmail(\'ihpCihp-`ln\');">email me</a> \
			if your browser is having problems with the SDR. </li>\
		<li> Windows: Firefox & Chrome work; IE is still completely broken. </li>\
		<li> Mac & Linux: Safari, Firefox, Chrome & Opera should work fine. </li>\
		<li> Open and close the panels by using the circled arrows at the top right corner. </li>\
		<li> You can click and/or drag almost anywhere on the page to change settings. </li>\
		<li> Enter a numeric frequency in the box marked "kHz" at right. </li>\
		<li> Or use the "select band" menu to jump to a pre-defined band. </li>\
		<li> Use the zoom icons to control the waterfall span. </li>\
		<li> Tune by clicking on the waterfall, spectrum or those cyan-colored station labels. </li>\
		<li> Shift or ctrl-shift click in the waterfall to lookup frequency in online databases. </li>\
		<li> Alt or meta/command click to page spectrum up and down in frequency. </li>\
		<li> Adjust the "WF max/min" sliders for best waterfall colors. </li>\
		<li> For more information see the <a href="http://www.kiwisdr.com/KiwiSDR/" target="_blank">webpage</a> \
		     and <a href="https://dl.dropboxusercontent.com/u/68809050/KiwiSDR/KiwiSDR.design.review.pdf" target="_blank">Design review document</a>. </li>\
		</ul> \
		';

		//<li> Noise across the band due to combination of active antenna and noisy location. </li>\
		//<li> Noise floor is high due to the construction method of the prototype ... </li>\
		//<li> ... so if you don\'t hear much on shortwave try \
		//	<a href="javascript:tune(346,\'am\');">LF</a>, <a href="javascript:tune(15.25,\'lsb\');">VLF</a> or \
		//	<a href="javascript:tune(1107,\'am\');">MW</a>. </li>\

		//<li> You must use a modern browser that supports HTML5. Partially working on iPad. </li>\
		//<li> Acknowledging the pioneering web-based SDR work of Pieter-Tjerk de Bohr, PA3FWM author of \
		//	<a href="http://websdr.ewi.utwente.nl:8901/" target="_blank">WebSDR</a>, and \
		//	Andrew Holme\'s <a href="http://www.aholme.co.uk/GPS/Main.htm" target="_blank"> homemade GPS receiver</a>. </li> \


	// id-msgs
	
	html("id-msgs-inner").innerHTML =
		'<div id="id-client-log-title"><strong> KiwiSDR </strong>' +
		'<a href="javascript:sendmail(\'ihpCihp-`ln\');">(ZL/KF6VO)</a>' +
		'<strong> and OpenWebRX </strong>' +
		'<a href="javascript:sendmail(\'kb4jonCpgq-kv\');">(HA7ILM)</a>' +
		'<strong> client log </strong><span id="id-problems"></span></div>' +
//		'Please send us bug reports and suggestions.<br/>' +
		'<div id="id-status-msg"></div>' +
		'<span id="id-msg-config"></span><br/>' +
		'<span id="id-msg-gps"></span><br/>' +
		'<span id="id-msg-audio"></span><br/>' +
		'<div id="id-debugdiv"></div>';
}

var kiwi_audio_stats_str = "";

function ajax_audio_stats(audio_kbps, waterfall_kbps, waterfall_fps, waterfall_total_fps, http_kbps, sum_kbps)
{
	kiwi_audio_stats_str = 'audio '+audio_kbps.toFixed(0)+' kB/s, waterfall '+waterfall_kbps.toFixed(0)+
		' kB/s ('+waterfall_fps.toFixed(0)+'/'+waterfall_total_fps.toFixed(0)+' fps)' +
		', http '+http_kbps.toFixed(0)+' kB/s, total '+sum_kbps.toFixed(0)+' kB/s ('+(sum_kbps*8).toFixed(0)+' kb/s)';
}

var kiwi_cpu_stats_str = "";
var kiwi_config_str = "";

function ajax_cpu_stats(uptime_secs, user, sys, idle, ecpu)
{
	kiwi_cpu_stats_str = 'Beagle CPU '+user+'% usr / '+sys+'% sys / '+idle+'% idle, FPGA eCPU '+ecpu.toFixed(0)+'%';
	var t = uptime_secs;
	var sec = Math.trunc(t % 60); t = Math.trunc(t/60);
	var min = Math.trunc(t % 60); t = Math.trunc(t/60);
	var hr  = Math.trunc(t % 24); t = Math.trunc(t/24);
	var days = t;
	var kiwi_uptime_str = ' | Uptime: ';
	if (days) kiwi_uptime_str += days +' '+ ((days > 1)? 'days':'day') +' ';
	kiwi_uptime_str += hr +':'+ min.leadingZeros(2) +':'+ sec.leadingZeros(2);
	html("id-msg-config").innerHTML = kiwi_config_str + kiwi_uptime_str;
}

function ajax_msg_config(rx_chans, gps_chans, serno, pub, port, pvt, nm, mac, vmaj, vmin)
{
	var s;
	kiwi_config_str = 'Config: v'+ vmaj +'.'+ vmin +', '+ rx_chans +' SDR channels, '+ gps_chans +' GPS channels';
	html("id-msg-config").innerHTML = kiwi_config_str;

	html("id-msg-config2").innerHTML =
		'KiwiSDR serial number: '+ serno;

	html("id-net-config").innerHTML =
		"Public IP address (outside your firewall/router): "+ pub +"<br>\n" +
		"Private IP address (inside your firewall/router): "+ pvt +"<br>\n" +
		"Netmask: /"+ nm +"<br>\n" +
		"KiwiSDR listening on TCP port number: "+ port +"<br>\n" +
		"Ethernet MAC address: "+ mac.toUpperCase();
}

function ajax_msg_update(pending, in_progress, rx_chans, gps_chans, vmaj, vmin, pmaj, pmin, build_date, build_time)
{
	kiwi_config_str = 'Config: v'+ vmaj +'.'+ vmin +', '+ rx_chans +' SDR channels, '+ gps_chans +' GPS channels';
	html("id-msg-config").innerHTML = kiwi_config_str;

	var s;
	s = 'Installed version: v'+ vmaj +'.'+ vmin +', built '+ build_date +' '+ build_time;
	if (in_progress) {
		s += '<br>Update to version v'+ + pmaj +'.'+ pmin +' in progress';
	} else
	if (pending) {
		s += '<br>Update to version v'+ + pmaj +'.'+ pmin +' pending';
	} else
	if (pmaj == -1) {
		s += '<br>Available version: unknown until checked';
	} else {
		if (vmaj == pmaj && vmin == pmin)
			s += '<br>Running most current version';
		else
			s += '<br>Available version: v'+ pmaj +'.'+ pmin;
	}
	html("id-msg-update").innerHTML = s;
}

// Safari on iOS only plays webaudio after it has been started by clicking a button
function iOS_audio_start()
{
	try {
		var actx = audio_context;
		var bufsrc = actx.createBufferSource();
   	bufsrc.connect(actx.destination);
   	var iOS = html('id-iOS-text');
   	kiwi_clearInterval(iOS.anim_timer);
   	kiwi_clearInterval(iOS.button_timer);
		iOS.style.opacity = 1;
   	try { bufsrc.start(0); } catch(ex) { bufsrc.noteOn(0); }
   	iOS.innerHTML = 'iOS';
   	iOS.style.color = 'lime';
   } catch(ex) { add_problem("audio start"); }
   freqset_select();
}

function zoomCorrection()
{
	return 3 * zoom_level;
	//return 0 * zoom_level;		// gives constant noise floor when using USE_GEN
}

function setmaxdb(done, str)
{
   maxdb = parseFloat(str);
	full_scale = maxdb - mindb;
	mk_dB_bands();
   html('field-maxdb').innerHTML = str + " dBFS";
   ws_fft_send("SET maxdb="+maxdb.toFixed(0)+" mindb="+mindb.toFixed(0));
	need_clear_specavg = true;
   if (done) freqset_select();
}

function setmindb(done, str)
{
   mindb = parseFloat(str);
   mindb_un = mindb + zoomCorrection();
	full_scale = maxdb - mindb;
	mk_dB_bands();
   html('field-mindb').innerHTML = str + " dBFS";
   ws_fft_send("SET maxdb="+maxdb.toFixed(0)+" mindb="+mindb.toFixed(0));
	need_clear_specavg = true;
   if (done) freqset_select();
}

function update_maxmindb_sliders()
{
	mindb = mindb_un - zoomCorrection();
	full_scale = maxdb - mindb;
	mk_dB_bands();
   html('input-maxdb').value = maxdb;
   html('field-maxdb').innerHTML = maxdb.toFixed(0) + " dBFS";
   html('input-mindb').value = mindb;
   html('field-mindb').innerHTML = mindb.toFixed(0) + " dBFS";
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
	html('button-mute').style.color = muted? 'lime':'white';
   f_volume = muted? 0 : volume/100;
   freqset_select();
}

var spectrum_display = false;

function toggle_or_set_spec()
{
	if (arguments.length > 0)
		spectrum_display = arguments[0];
	else
		spectrum_display ^= 1;
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

var divClientParams;

function place_panels()
{
	var left_col = [];
	var right_col = [];
	var plist = html("id-panels-container").children;
	for (var i=0; i<plist.length; i++)
	{
		var c = plist[i];
		if(c.className=="class-panel")
		{
			//var newSize = c.dataset.panelSize.split(",");		// doesn't work with IE10
			var newSize = c.getAttribute('data-panel-size').split(",");
			if (c.getAttribute('data-panel-pos')=="left") { left_col.push(c); }
			else if(c.getAttribute('data-panel-pos')=="right") { right_col.push(c); }
			
			c.style.width = newSize[0]+"px";
			c.style.height = newSize[1]+"px";
			c.style.margin = panel_margin.toString()+"px";
			c.uiWidth = parseInt(newSize[0]);
			c.uiHeight = parseInt(newSize[1]);
			
			// Since W3.CSS includes the normalized use of "box-sizing: border-box"
			// style.width no longer represents the active content area.
			// So compute it ourselves for those who need it later on.
			var border_pad = html_LR_border_pad(c);
			c.activeWidth = c.uiWidth - border_pad;
			console.log('place_panels: id='+ c.id +' uiW='+ c.uiWidth +' bp='+ border_pad + 'active='+ c.activeWidth);
			
			if (c.getAttribute('data-panel-pos') == 'center') {
				console.log("L/B "+(window.innerHeight).toString()+"px "+(c.uiHeight).toString()+"px");
				c.style.left = (window.innerWidth/2 - c.activeWidth/2).toString()+"px";
console.log('### WIH '+ window.innerHeight);	//jks
				//c.style.bottom = (window.innerHeight/2 - c.uiHeight/2).toString()+"px";
				c.style.top = (67+157+20).toString()+"px";
				c.style.visibility = "hidden";
			}
		}
	}
	y=0;
	while(left_col.length>0)
	{
		p = pop_bottommost_panel(left_col);
		p.style.left = "0px";
		p.style.bottom = y.toString()+"px";
		p.style.visibility = "visible";
		y += p.uiHeight+3*panel_margin;
	}
	y=0;
	while(right_col.length>0)
	{
		p = pop_bottommost_panel(right_col);
		p.style.right = "10px";
		p.style.bottom = y.toString()+"px";
		p.style.visibility = "visible";
		y += p.uiHeight+3*panel_margin;
	}

	divClientParams = html('id-client-params');
}


// ========================================================
// =======================  >MISC  ========================
// ========================================================

function arrayBufferToString(buf) {
	//http://stackoverflow.com/questions/6965107/converting-between-strings-and-arraybuffers
	return String.fromCharCode.apply(null, new Uint8Array(buf));
}

function getFirstChars(buf, num)
{
	var u8buf=new Uint8Array(buf);
	var output=String();
	num=Math.min(num,u8buf.length);
	for (var i=0;i<num;i++) output+=String.fromCharCode(u8buf[i]);
	return output;
}

function add_problem(what)
{
	problems_span = html("id-problems");
	if (!problems_span) return;
	if (typeof problems_span.children != "undefined")
		for(var i=0;i<problems_span.children.length;i++) if(problems_span.children[i].innerHTML==what) return;
	new_span = document.createElement("span");
	new_span.innerHTML=what;
	problems_span.appendChild(new_span);
	window.setTimeout(function(ps,ns) { ps.removeChild(ns); }, 1000, problems_span, new_span);
}

function divlog(what, is_error)
{
	if (typeof is_error !== "undefined" && is_error) what='<span class="class-error">'+what+"</span>";
	html('id-debugdiv').innerHTML += what+"<br />";
}

String.prototype.startswith=function(str){ return this.indexOf(str) == 0; }; //http://stackoverflow.com/questions/646628/how-to-check-if-a-string-startswith-another-string

/*function email(what)
{
	//| http://stackoverflow.com/questions/617647/where-is-my-one-line-implementation-of-rot13-in-javascript-going-wrong
	what=what.replace(/[a-zA-Z]/g,function(c){return String.fromCharCode((c<="Z"?90:122)>=(c=c.charCodeAt(0)+13)?c:c-26);});
	window.location.href="mailto:"+what;
}*/

var enc = function(s) { return s.replace(/./gi, function(c) { return String.fromCharCode(c.charCodeAt(0) ^ 3); }); }

var sendmail = function (to, subject) {
	var s = "mailto:"+enc(to)+((typeof subject != "undefined")? ('?subject='+subject):'');
	console.log(s);
	window.location.href = s;
}

function kiwi_too_busy_ui() {}


// ========================================================
// =======================  >WEBSOCKET  ===================
// ========================================================

var wsockets = [];

function open_websocket(stream, tstamp, cb_recv)
{
	if (!("WebSocket" in window) || !("CLOSING" in WebSocket)) {
		console.log('WEBSOCKET TEST');
		kiwi_serious_error("Your browser does not support WebSocket, which is required for WebRX to run. <br> Please use an HTML5 compatible browser.");
		return null;
	}

	var ws_url;			// replace http:// with ws:// on the URL that includes the port number
	try {
		ws_url = window.location.origin.split("://")[1];
	} catch(ex) {
		ws_url = this.location.href.split("://")[1];
	}
	ws_url = 'ws://'+ ws_url +'/'+ tstamp +'/'+ stream;
	
	//console.log("WS_URL "+ws_url);
	var ws = new WebSocket(ws_url);
	wsockets.push( { 'ws':ws, 'name':stream } );
	ws.up = false;
	ws.stream = stream;

	// There is a delay between a "new WebSocket()" and it being able to perform a ws.send(),
	// so must wait for the ws.onopen() to occur before sending any init commands.
	ws.onopen = function() {
		ws.up = true;
		ws.send("SERVER DE CLIENT openwebrx.js "+ws.stream);
		//divlog("WebSocket opened to "+ws_url);
		if (ws.stream == "AUD") {		// fixme: remove these eventually
			
			ws.send("SET squelch=0");
			ws.send("SET autonotch=0");
			//ws.send("SET genattn=131071");	// 0x1ffff
			ws.send("SET genattn=4095");	// 0xfff
			var gen_freq = 0;
			if (dbgUs && initCookie('ident', "").search('gen') != -1) gen_freq = (init_frequency*1000).toFixed(0);
			ws.send("SET gen="+(gen_freq/1000).toFixed(3)+" mix=-1");
			ws.send("SET mod="+init_mode+" low_cut=-4000 high_cut=4000 freq="+init_frequency.toFixed(3));
			ws.send("SET agc=1 hang=0 thresh=-120 slope=0 decay=200 manGain=0");
			ws.send("SET browser="+navigator.userAgent);
		} else
		if (ws.stream == "FFT") {
			ws.send("SET send_dB=1");
			// fixme: okay to remove this now?
			ws.send("SET zoom=0 start=0");
			ws.send("SET maxdb="+maxdb+" mindb="+mindb);
			ws.send("SET slow=2");
		}
	};

	ws.onmessage = function(evt) {
		on_ws_recv(evt, ws);
	};
	ws.cb_recv = cb_recv;

	ws.onclose = function(ws) {
		console.log("WS-CLOSE: "+ws.stream);
	};
	
	ws.binaryType = "arraybuffer";

	ws.onerror = on_ws_error;
	return ws;
}

// http://stackoverflow.com/questions/4812686/closing-websocket-correctly-html5-javascript
// FIXME: but doesn't work as expected due to problems with onbeforeunload on different browsers
window.onbeforeunload = function() {
	var i;
	
	//alert('onbeforeunload '+ wsockets.length);
	for (i=0; i < wsockets.length; i++) {
		var ws = wsockets[i].ws;
		if (ws) {
			//alert('window.onbeforeunload: ws close '+ wsockets[i].name);
			ws.onclose = function () {};
			ws.close();
		}
	}
};

function on_ws_error(event)
{
	divlog("WebSocket error.",1);
}

var bin_server = 0;
var zoom_server = 0;
var process_return_nexttime = false;

function on_ws_recv(evt, ws)
{
	var data = evt.data;
	
	if (!(data instanceof ArrayBuffer)) {
		divlog("on_ws_recv(): Not ArrayBuffer received...",1);
		return;
	}

	//var s = arrayBufferToString(data);
	//console.log('on_ws_recv: <'+ s +'>');

	firstChars = getFirstChars(data,3);
	//divlog("on_ws_recv: "+firstChars);

	if (firstChars == "CLI") {
		var stringData = arrayBufferToString(data);
		if (stringData.substring(0,16) == "CLIENT DE SERVER")
			divlog("Acknowledged WebSocket connection: "+stringData);
	} else
	
	if (firstChars == "MSG") {
		var s='';
		var stringData = arrayBufferToString(data);
		params = stringData.substring(4).split(" ");
		for (var i=0; i < params.length; i++) {
			s += ' '+params[i];
			param = params[i].split("=");
			switch (param[0]) {
				case "fft_setup":
					waterfall_init();
					break;					
				case "bandwidth":
					bandwidth = parseInt(param[1]);
					break;		
				case "center_freq":
					center_freq = parseInt(param[1]);
					break;
				case "fft_size":
					fft_size = parseInt(param[1]);
					break;
				case "fft_fps":
					fft_fps = parseInt(param[1]);
					break;
				case "start":
					bin_server = parseInt(param[1]);
					break;
				case "zoom":
					zoom_server = parseInt(param[1]);
					break;
				case "zoom_max":
					zoom_levels_max = parseInt(param[1]);
					break;
				case "wf_comp":
					waterfall_compression = parseInt(param[1]);
					break;
				case "rx_chans":
					rx_chans = parseInt(param[1]);
					break;
				case "rx_chan":
					rx_chan = parseInt(param[1]);
					break;
				case "audio_rate":
					audio_rate(parseFloat(param[1]));
					break;
				case "audio_comp":
					audio_compression = parseInt(param[1]);
					if (comp_override != -1) {
						audio_compression = comp_override? true:false;
						ws_aud_send("SET OVERRIDE comp="+ (audio_compression? 1:0));
						console.log("COMP override audio_compression="+ audio_compression);
					}
					console.log("COMP audio_compression="+audio_compression);
					break;

				case "kiwi_up":
					kiwi_up(parseInt(param[1]));
					break;
				case "too_busy":
					kiwi_too_busy(parseInt(param[1]));
					break;
				case "down":
					kiwi_down(param[1]);
					break;
				case "gps":
					toggle_or_set_spec(1);
					break;
				case "fft":
					kiwi_fft();
					break;
				case "client_ip":
					client_ip = param[1];
					console.log("client IP: "+client_ip);
					break;
				case "status_msg":
					var el = html_id('id-status-msg');
					if (!el) break;
					var s = decodeURIComponent(param[1]);
					s = s.replace(/\n/g, '<br>');
					//console.log('status_msg: <'+ s +'>');
					
					var o = el.innerHTML;
					for (var i=0; i < s.length; i++) {
						if (process_return_nexttime) {
							var ci = o.lastIndexOf('<br>');
							if (ci == -1) {
								o = '';
							} else {
								o = o.substring(0, ci+4);
							}
							process_return_nexttime = false;
						}
						var c = s.charAt(i);
						if (c == '\r') {
							process_return_nexttime = true;
						} else
						if (c == '\f') {		// form-feed is how we clear element from appending
							o = '';
						} else {
							o += c;
						}
					}
					el.innerHTML = o;

					if (typeof el.getAttribute != "undefined" && el.getAttribute('data-scroll-down') == 'true')
						el.scrollTop = el.scrollHeight;
					break;
				case "request_dx_update":
					dx_update();
					break;
				default:
					s += " ???";
					break;
			}
		}
		//console.log('cmd-'+ws.stream+':'+s);
	} else {
		ws.cb_recv(data);
	}
}

function ws_aud_send(s)
{
	try {
		ws_aud.send(s);
		return 0;
	} catch(ex) {
		console.log("CATCH ws_aud_send('"+s+"') ex="+ex);
		kiwi_trace();
		return -1;
	}
}

function ws_fft_send(s)
{
	try {
		ws_fft.send(s);
		return 0;
	} catch(ex) {
		console.log("CATCH ws_fft_send('"+s+"') ex="+ex);
		kiwi_trace();
		return -1;
	}
}

var need_geo = true;
var need_status = true;

function send_keepalive()
{
	for (var i=0; i<1; i++) {
		if (!ws_aud.up || ws_aud_send("SET keepalive=1") < 0)
			break;
	
		// these are done here because we know the audio connection is up and can receive messages
		if (need_geo && kiwi_geo() != "") {
			if (ws_aud_send("SET geo="+kiwi_geo()) < 0)
				break;
			if (ws_aud_send("SET geojson="+kiwi_geojson()) < 0)
				break;
			need_geo = false;
		}
		
		if (need_name) {
			if (ws_aud_send("SET name="+ident_name) < 0)
				break;
			need_name = false;
		}
	
		if (need_status) {
			if (ws_aud_send("SET need_status=1") < 0)
				break;
			if (inactivity_timeout == 0) {
				if (ws_aud_send("SET OVERRIDE inactivity_timeout=0") < 0)
					break;
			}
			need_status = false;
		}
	}

	if (!ws_fft.up || ws_fft_send("SET keepalive=1") < 0)
		return;
}
