// Copyright (c) 2016 John Seamons, ZL/KF6VO

var S_meter_ext_name = 'S_meter';		// NB: must match S_meter.c:S_meter_ext.name

var S_meter_first_time = true;

function S_meter_main()
{
	ext_switch_to_client(S_meter_ext_name, S_meter_first_time, S_meter_recv);		// tell server to use us (again)
	if (!S_meter_first_time)
		S_meter_controls_setup();
	S_meter_first_time = false;
}

var sm_xo = 200;
var sm_w = 1024;
var sm_padding = 10;
var sm_tw = sm_w + sm_padding*2;
var sm_th = 200;

function S_meter_recv(data)
{
	var firstChars = getFirstChars(data, 3);
	
	// process data sent from server/C by ext_send_data_msg()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0] >> 1;
		var ch = ba[0] & 1;
		var o = 1;
		var len = ba.length-1;

		console.log('S_meter_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_encoded_msg()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('S_meter_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('S_meter_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				S_meter_controls_setup();
				break;

			case "smeter":
				S_meter_plot(parseFloat(param[1]));
				break;

			default:
				console.log('S_meter_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var S_meter_maxdb_init = -30;
var S_meter_mindb_init = -130;
var S_meter_speed_max = 10;

var S_meter = {
	'range':0, 'maxdb':S_meter_maxdb_init, 'mindb':S_meter_mindb_init, 'speed':S_meter_speed_max, 'marker':0
};

var S_meter_data_canvas;

function S_meter_controls_setup()
{
   var data_html =
      '<div id="id-S_meter-data" style="left:200px; width:1044px; height:200px; background-color:mediumBlue; position:relative; display:none" title="S_meter">' +
   		'<canvas id="id-S_meter-data-canvas" width="1024" height="180" style="position:absolute; padding: 10px 10px 10px 10px;"></canvas>'+
      '</div>';

	var range_s = {
		0:'manual',
		1:'auto'
	};

	var marker_s = {
		0:'1 sec',
		1:'5 sec',
		2:'10 sec',
		3:'15 sec',
		4:'30 sec',
		5:'1 min'
	};

	var controls_html =
		w3_divs('id-S_meter-controls w3-text-white', '',
			w3_divs('w3-container', 'w3-tspace-8',
				w3_divs('', 'w3-medium w3-text-aqua', '<b>S-meter graph</b>'),
				w3_select('Range', '', 'S_meter.range', S_meter.range, range_s, 'S_meter_range_select_cb'),
				w3_slider('Scale max', 'S_meter.maxdb', S_meter.maxdb, -160, 0, 10, 'S_meter_maxdb_cb'),
				w3_slider('Scale min', 'S_meter.mindb', S_meter.mindb, -160, 0, 10, 'S_meter_mindb_cb'),
				w3_slider('Speed', 'S_meter.speed', S_meter.speed, 1, S_meter_speed_max, 1, 'S_meter_speed_cb'),
				w3_divs('', 'w3-show-inline w3-hspace-16',
					w3_select('Marker rate', '', 'S_meter.marker', S_meter.marker, marker_s, 'S_meter_marker_select_cb'),
					w3_btn('Clear', 'S_meter_clear_cb')
				)
			)
		);

	ext_panel_show(controls_html, data_html, null);

	S_meter_data_canvas = html_id('id-S_meter-data-canvas');
	S_meter_data_canvas.ctx = S_meter_data_canvas.getContext("2d");
	S_meter_data_canvas.im = S_meter_data_canvas.ctx.createImageData(sm_w, 1);

	S_meter_resize();
	ext_set_controls_width(225);

	S_meter_visible(1);

	S_meter_maxdb_cb('S_meter.maxdb', S_meter.maxdb);
	S_meter_mindb_cb('S_meter.mindb', S_meter.mindb);
	S_meter_speed_cb('S_meter.speed', S_meter.speed);
	
	ext_send('SET run=1');
	S_meter_clear();

	S_meter_update_interval = setInterval('S_meter_update()', 1000);
}

function S_meter_resize()
{
	var el = html_idname('S_meter-data');
	var left = (window.innerWidth - sm_tw) / 2;
	el.style.left = px(left);
}

var S_meter_range = 0;
var S_meter_range_e = { MANUAL:0, AUTO:1 };

function S_meter_range_select_cb(path, idx, first)
{
	// ignore the auto instantiation callback because we don't want to rescale at this point
	if (first) return;

	S_meter_range = +idx;
	if (S_meter_range == S_meter_range_e.MANUAL)
		S_meter_rescale();
}

function S_meter_maxdb_cb(path, val, complete)
{
   var maxdb = +val;
   maxdb = Math.max(S_meter.mindb, maxdb);		// don't let min & max cross
	w3_num_cb(path, maxdb.toString());
	w3_set_label('Scale max '+ maxdb.toString() +' dBm', path);

	if (complete) S_meter_rescale();
}

function S_meter_mindb_cb(path, val, complete)
{
   var mindb = +val;
   mindb = Math.min(mindb, S_meter.maxdb);		// don't let min & max cross
	w3_num_cb(path, mindb.toString());
	w3_set_label('Scale min '+ mindb.toString() +' dBm', path);

	if (complete) S_meter_rescale();
}

var S_meter_speed;	// not the same as S_meter.speed

function S_meter_speed_cb(path, val, complete)
{
	var val_i = +val;
   S_meter_speed = S_meter_speed_max - val_i + 1;
	w3_num_cb(path, val_i.toString());
	w3_set_label('Speed 1'+ ((S_meter_speed != 1)? ('/'+S_meter_speed.toString()) : ''), path);
}

var S_meter_marker_e = { SEC_1:0, SEC_5:1, SEC_10:2, SEC_15:3, SEC_30:4, MIN_1:5 };
var S_meter_marker_sec = [ 1, 5, 10, 15, 30, 60 ];
var S_meter_marker = S_meter_marker_sec[S_meter.marker];

function S_meter_marker_select_cb(path, idx)
{
	S_meter_marker = S_meter_marker_sec[+idx];
}

function S_meter_clear_cb(path, val)
{
	S_meter_clear();
	setTimeout('w3_radio_unhighlight('+ q(path) +')', w3_highlight_time);
}

var S_meter_update_interval;
var sm_last_freq = 0;
var sm_last_offset = 0;

// detect when frequency or offset has changed and restart graph
function S_meter_update()
{
	var freq = ext_get_freq();
	var offset = freq - ext_get_carrier_freq();
	
	// don't restart on mode(offset) change so user can switch and see difference
	if (freq != sm_last_freq /*|| offset != sm_last_offset*/) {
		S_meter_clear();
		//console.log('freq/offset change');
		sm_last_freq = freq;
		sm_last_offset = offset;
	}
}

var sm = {
   hi: S_meter_maxdb_init,
   lo: S_meter_mindb_init,
   goalH: 0,
   goalL: 0,
   scroll: 0,
   scaleWidth: 60,
   hysteresis: 15,
   window_size: 20,
   xi: 0,					// initial x value as grid scrolls left until panel full
   secs_last: 0,
   redraw_scale: false,
   clr_color: 'mediumBlue',
   bg_color: 'ghostWhite',
   grid_color: 'lightGrey',
   scale_color: 'white',
   plot_color: 'black'
};

// FIXME: handle window resize

function S_meter_clear()
{
	ext_send('SET clear');
	var c = S_meter_data_canvas.ctx;
	c.fillStyle = sm.clr_color;
	c.fillRect(0,0, sm_w,sm_th);
	sm.xi = S_meter_data_canvas.width - sm.scaleWidth;
	S_meter_rescale();
}

function S_meter_rescale()
{
	if (S_meter_range == S_meter_range_e.MANUAL)
   	sm.redraw_scale = true;
}

function S_meter_plot(dBm)
{
   var cv = S_meter_data_canvas;
   var ct = S_meter_data_canvas.ctx;
   var w = cv.width - sm.scaleWidth, h = cv.height;
   var wi = w-sm.xi;
	var range = sm.hi - sm.lo;

	var y_dBm = function(dBm) {
		var norm = (dBm - sm.lo) / range;
		return h - (norm * h);
	};

	var gridC_10dB = function(dB) { return 10 * Math.ceil(dB/10); };
	var gridF_10dB = function(dB) { return 10 * Math.floor(dB/10); };

	// re-scale existing part of plot if manual range changed or in auto-range mode
	var force_recomp = false;
   if (sm.redraw_scale || S_meter_range == S_meter_range_e.AUTO) {
   	if (S_meter_range == S_meter_range_e.MANUAL) {
			sm.goalH = S_meter.maxdb;
			sm.goalL = S_meter.mindb;
			force_recomp = true;
		} else {
			var converge_rate_dBm = 0.25 / S_meter_speed;
			sm.goalH = (sm.goalH > (dBm+sm.window_size))? (sm.goalH - converge_rate_dBm) : (dBm+sm.window_size);
			sm.goalL = (sm.goalL < (dBm-sm.window_size))? (sm.goalL + converge_rate_dBm) : (dBm-sm.window_size);
		}

		if (sm.goalL > sm.lo+sm.hysteresis || sm.goalL < sm.lo || force_recomp) { 
			var new_lo = gridF_10dB(sm.goalL)-5;
			var new_range = sm.hi - new_lo;

			if (new_lo <= sm.lo) {
				// shrink previous grid above with new background below
				var shrink = range / new_range;
				if (wi) {
					ct.drawImage(cv, sm.xi,0, wi,h, sm.xi,0, wi,h*shrink);
					ct.fillStyle = sm.bg_color;
					ct.fillRect(sm.xi,Math.floor(shrink*h), wi,h*(1-shrink)+1);
				}
			} else {
				// upper portion of previous grid expands to fill entire space
				var expand = new_range / range;
				if (wi) ct.drawImage(cv, sm.xi,0, wi,h*expand, sm.xi,0, wi,h);
			}

			sm.lo = new_lo;
			range = new_range;
			if (S_meter_range == S_meter_range_e.AUTO)
				sm.redraw_scale = true;
		}
		
		if (sm.goalH > sm.hi || sm.goalH < sm.hi-sm.hysteresis || force_recomp) {
			var new_hi = gridC_10dB(sm.goalH)+5;
			var new_range = new_hi - sm.lo;
			if (new_hi >= sm.hi) {
				// shrink previous grid below with new background above
				var shrink = range / new_range;
				if (wi) {
					ct.drawImage(cv, sm.xi,0, wi,h, sm.xi,h*(1-shrink), wi,h*shrink);
					ct.fillStyle = sm.bg_color;
					ct.fillRect(sm.xi,0, wi,Math.ceil(h*(1-shrink)));
				}
			} else {
				// lower portion of previous grid expands to fill entire space
				var expand = new_range / range;
				if (wi) ct.drawImage(cv, sm.xi,h*(1-expand), wi,h*expand, sm.xi,0, wi,h);
			}
			sm.hi = new_hi;
			range = new_hi - sm.lo;
			if (S_meter_range == S_meter_range_e.AUTO)
				sm.redraw_scale = true;
		}
	}

   if (sm.redraw_scale) {
		ct.fillStyle = sm.clr_color;
		ct.fillRect(w,0, sm.scaleWidth,h);
      ct.fillStyle = sm.scale_color;
      ct.font = '10px Verdana';
      for (var dB = sm.lo; (dB = gridC_10dB(dB)) <= sm.hi; dB++) {
         ct.fillText(dB +' dBm', w+2,y_dBm(dB)+4, sm.scaleWidth);
      }
      sm.redraw_scale = false;
   }

   if (++sm.scroll >= S_meter_speed) {
      sm.scroll = 0;
      if (sm.xi > 0) sm.xi--;
      
      // shift entire window left 1px to make room for new line
      ct.drawImage(cv, 1,0,w-1,h, 0,0,w-1,h);

      var secs = new Date().getTime() / 1000;
      var now = Math.floor(secs / S_meter_marker);
      var then = Math.floor(sm.secs_last / S_meter_marker);

      if (now == then) {
			// draw dB level lines by default
         ct.fillStyle = sm.bg_color;
         ct.fillRect(w-1,0, 1,h);
         ct.fillStyle = sm.grid_color;
         for (var dB = sm.lo; (dB = gridC_10dB(dB)) <= sm.hi; dB++) {
            ct.fillRect(w-1,y_dBm(dB), 1,1);
         }
      } else {
         // time to draw time marker
         sm.secs_last = secs;
         ct.fillStyle = sm.grid_color;
         ct.fillRect(w-1,0, 1,h);
      }
   }

	// always plot, even if not shifting display
   ct.fillStyle = sm.plot_color;
   ct.fillRect(w-1,y_dBm(dBm), 1,1);
}

// hook that is called when controls panel is closed
function S_meter_blur()
{
	//console.log('### S_meter_blur');
	ext_send('SET run=0');
	S_meter_visible(0);		
	kiwi_clearInterval(S_meter_update_interval);
	ext_set_controls_width(-1);	// restore width in case it is currently not the default
}

// called to display HTML for configuration parameters in admin interface
function S_meter_config_html()
{
	ext_admin_config(S_meter_ext_name, 'S Meter',
		w3_divs('id-S_meter w3-text-teal w3-hide', '',
			'<b>S-meter graph configuration</b>' +
			'<hr>' +
			''
			/*
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					admin_input('int1', 'S_meter.int1', 'admin_num_cb'),
					admin_input('int2', 'S_meter.int2', 'admin_num_cb')
				), '', ''
			)
			*/
		)
	);
}

function S_meter_visible(v)
{
	visible_block('id-S_meter-data', v);
}
