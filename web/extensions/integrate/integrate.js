// Copyright (c) 2016 John Seamons, ZL/KF6VO

var integrate_ext_name = 'integrate';		// NB: must match integrate.c:integrate_ext.name

var integrate_first_time = true;

function integrate_main()
{
	ext_switch_to_client(integrate_ext_name, integrate_first_time, integrate_recv);		// tell server to use us (again)
	if (!integrate_first_time)
		integrate_controls_setup();
	integrate_first_time = false;
}

var integ_w = 1024;
var integ_th = 200;
var integ_hdr = 12;
var integ_h = integ_th - integ_hdr;
var integ_yo = 0;
var integ_bins, integ_bino = 0;
var integ_draw = false;

function integrate_clear()
{
	ext_send('SET clear');
	integ_draw = false;
	var c = integrate_data_canvas.ctx;
	c.fillStyle = 'mediumBlue';
	c.fillRect(0, 0, integ_w, integ_th);

	var c = integrate_info_canvas.ctx;
	c.fillStyle = '#575757';
	c.fillRect(0, 0, 256, 280);
	
	var left = w3_el('integrate-controls-left');
	var right = w3_el('integrate-controls-right');

	switch (integrate_preset) {
	
	case 0:		// Alpha has sub-display on left
		ext_set_controls_width_height(undefined, 290);		// default width
		left.style.width = '49.9%';
		right.style.width = '49.9%';
		integrate_alpha();
		break;
	
	default:
		ext_set_controls_width_height(275, 290);
		left.style.width = '0%';
		right.style.width = '100%';
		var f = ext_get_freq();
		integrate_marker((f/1e3).toFixed(2), false, f);
		break;
	}
}

var integrate_update_interval;
var integ_last_freq = 0;
var integ_last_offset = 0;

// detect when frequency or offset has changed and adjust display
function integrate_update()
{
	var freq = ext_get_freq();
	var offset = freq - ext_get_carrier_freq();
	
	if (freq != integ_last_freq || offset != integ_last_offset) {
		integrate_clear();
		//console.log('freq/offset change');
		integ_last_freq = freq;
		integ_last_offset = offset;
	}
}

function integrate_marker(txt, left, f)
{
	// draw frequency markers on FFT canvas
	var c = integrate_data_canvas.ctx;
	var pxphz = integ_w / ext_sample_rate();
	
	txt = left? (txt + '\u25BC') : ('\u25BC'+ txt);
	c.font = '12px Verdana';
	c.fillStyle = 'white';
	var carrier = ext_get_carrier_freq();
	var car_off = f - carrier;
	var dpx = car_off * pxphz;
	var halfw_tri = c.measureText('\u25BC').width/2 - (left? -1:2);
	var tx = Math.round((integ_w/2-1) + dpx + (left? (-c.measureText(txt).width + halfw_tri) : (-halfw_tri)));
	c.fillText(txt, tx, 10);
	//console.log('MKR='+ txt +' car='+ carrier +' f='+ f +' pxphz='+ pxphz +' coff='+ car_off +' dpx='+ dpx +' tx='+ tx);
}

var integrate_cmd_e = { FFT:0, CLEAR:1 };
var integ_dbl;    // vertical line doubling (or more)
var integ_maxdb, integ_mindb;

function integrate_mousedown(evt)
{
	//event_dump(evt, 'FFT');
	var y = (evt.clientY? evt.clientY : (evt.offsetY? evt.offsetY : evt.layerY)) - integ_yo;
	if (y < 0 || y >= integ_h) return;
	var bin = Math.round(y / integ_dbl);
	if (bin < 0 || bin > integ_bins) return;
	integ_bino = Math.round((integ_bino + bin) % integ_bins);
	//console.log('FFT y='+ y +' bino='+ integ_bino +'/'+ integ_bins);
}

function integrate_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		if (cmd == integrate_cmd_e.FFT) {
			if (!integ_draw) return;
			
			var c = integrate_data_canvas.ctx;
			var im = integrate_data_canvas.im;
			var bin = ba[o];
			o++; len--;

			bin -= integ_bino;
			if (bin < 0)
				bin += integ_bins;

			for (var x=0; x < integ_w; x++) {
				var color = waterfall_color_index_max_min(ba[x+o], integ_maxdb, integ_mindb);
				im.data[x*4+0] = color_map_r[color];
				im.data[x*4+1] = color_map_g[color];
				im.data[x*4+2] = color_map_b[color];
				im.data[x*4+3] = 0xff;
			}
			
			for (var y=0; y < integ_dbl; y++) {
				c.putImageData(im, 0, integ_yo + (bin*integ_dbl + y));
			}
		} else
		
		if (cmd == integrate_cmd_e.CLEAR) {	// not currently used
			integrate_clear();
		} else {
			console.log('integrate_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		}
		
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('integrate_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('integrate_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				integrate_controls_setup();
				break;

			case "bins":
				integ_bins = parseInt(param[1]);
				integ_bino = 0;
				integ_dbl = Math.floor(integ_h / integ_bins);
				if (integ_dbl < 1) integ_dbl = 1;
				integ_yo = (integ_h - integ_dbl * integ_bins) / 2;
				if (integ_yo < 0) integ_yo = 0;
				//console.log('integrate_recv bins='+ integ_bins +' dbl='+ integ_dbl +' yo='+ integ_yo);
				integ_yo += integ_hdr;
				integ_draw = true;
				
				break;

			default:
				console.log('integrate_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var integrate_itime_init = 10.0;
var integrate_maxdb_init = -10;
var integrate_mindb_init = -134;

var integrate = {
	'itime':integrate_itime_init, 'pre':0, 'maxdb':integrate_maxdb_init, 'mindb':integrate_mindb_init
};

var integrate_data_canvas, integrate_info_canvas;

function integrate_controls_setup()
{
	integrate_preset = -1;
	
   var data_html =
      time_display_html('integrate') +

      w3_div('id-integrate-data|left:150px; width:1024px; height:200px; background-color:mediumBlue; position:relative;',
   		'<canvas id="id-integrate-data-canvas" width="1024" height="200" style="position:absolute"></canvas>'
      );

   var info_html =
      w3_div('id-integrate-info|left:0px; height:280px; overflow:hidden; position:relative;',
   		'<canvas id="id-integrate-info-canvas" width="256" height="280" style="position:absolute"></canvas>'
      );

	//jks debug
	if (dbgUs) integrate.pre = 1;

	var pre_s = {
		0:'Alpha (RSDN-20)',
		1:'40 JJY',
		2:'60 WWVB/MSF/JJY',
		3:'200/3 RBU',
		4:'68.5 BPC',
		5:'77.5 DCF77'
	};

	var controls_html =
		w3_div('id-integrate-controls w3-text-white',
			w3_half('', '',
				info_html,
				w3_divs('w3-container/w3-tspace-8',
					w3_div('w3-medium w3-text-aqua', '<b>Audio integration</b>'),
					w3_input('w3-width-64 w3-padding-smaller', 'Integrate time (secs)', 'integrate.itime', integrate.itime, 'integrate_itime_cb'),
					w3_select('', 'Presets', 'select', 'integrate.pre', -1, pre_s, 'integrate_pre_select_cb'),
					w3_slider('', 'WF max', 'integrate.maxdb', integrate.maxdb, -100, 20, 1, 'integrate_maxdb_cb'),
					w3_slider('', 'WF min', 'integrate.mindb', integrate.mindb, -190, -30, 1, 'integrate_mindb_cb'),
					w3_button('', 'Clear', 'integrate_clear_cb')
				), 'id-integrate-controls-left', 'id-integrate-controls-right'
			)
		);

	ext_panel_show(controls_html, data_html, null);
	time_display_setup('integrate');

	integrate_data_canvas = w3_el('id-integrate-data-canvas');
	integrate_data_canvas.ctx = integrate_data_canvas.getContext("2d");
	integrate_data_canvas.im = integrate_data_canvas.ctx.createImageData(integ_w, 1);
	integrate_data_canvas.addEventListener("mousedown", integrate_mousedown, false);

	integrate_info_canvas = w3_el('id-integrate-info-canvas');
	integrate_info_canvas.ctx = integrate_info_canvas.getContext("2d");

	integrate_environment_changed( {resize:1} );

	integrate_itime_cb('integrate.itime', integrate.itime);
	
	ext_send('SET run=1');
	integrate_clear();

	integrate_update_interval = setInterval(integrate_update, 1000);
}

function integrate_environment_changed(changed)
{
   if (!changed.resize) return;
	var el = w3_el('id-integrate-data');
	var left = (window.innerWidth - integ_w - time_display_width()) / 2;
	el.style.left = px(left);
}

var alpha = { KRAS:1, NOVO:2, KHAB:3, REVD:4, SEYD:5, MULT:6 };
var alpha_stations = [ 'Krasnodar 38\u00B0E', 'Novosibirsk 84\u00B0E', 'Khabarovsk 136\u00B0E', 'Revda 34\u00B0E', 'Seyda 62\u00B0E' ];
var alpha_station_colors = [ 'yellow', 'red', 'lime', 'cyan', 'magenta' ];
var alpha_freqs = [  ];

var alpha_sched = {
	0: [	'F1',		   'F4/5',	   'F2',		   'F3/p'	   ],
	1: [	11.9,		   12.09,	   12.65,	   14.88		   ],    // F5 is really 12.04 and F3p is slightly different from F3
	2: [	true,		   false,	   false,	   false		   ],
	3: [	alpha.NOVO,	0,			   alpha.REVD,	alpha.KRAS	],
	4: [	alpha.SEYD,	alpha.REVD,	alpha.NOVO,	alpha.KHAB	],
	5: [	alpha.KRAS,	alpha.SEYD,	alpha.KHAB,	alpha.NOVO	],
	6: [	alpha.KHAB,	0,			   alpha.KRAS,	alpha.MULT	],
	7: [	alpha.REVD,	0,			   0,			   alpha.SEYD	],
	8: [	0,			   0,			   alpha.SEYD,	alpha.REVD	]
};

function integrate_alpha()
{
	var c = integrate_info_canvas.ctx;
	var xo = 64, xi = 48, y = 16, yh = 20, yi = 24, barw = 9, nbarw = 5;

	c.font = '12px Verdana';
	var ty = yh/2 + 10/2;

	for (var freq = 0; freq < 4; freq++) {
		c.fillStyle = 'white';
		var txt = alpha_sched[0][freq];
		var tx = barw/2 - c.measureText(txt).width/2;
		c.fillText(txt, xo + xi*freq + tx, 10);
		var txt = alpha_sched[1][freq].toFixed(2);
		var tx = barw/2 - c.measureText(txt).width/2;
		c.fillText(txt, xo + xi*freq + tx, 26);

		y = 32;
		for (var tslot = 1; tslot <= 6; tslot++) {
			if (freq == 0) {
				c.fillStyle = 'white';
				c.fillText('slot '+ tslot, 16, y + ty);
			}
			
			var station = alpha_sched[tslot+2][freq];
			if (station == alpha.MULT) {
				c.fillStyle = alpha_station_colors[alpha.NOVO-1];
				c.fillRect(xo + xi*freq + (2-2-nbarw), y, nbarw, yh);
				c.fillStyle = alpha_station_colors[alpha.REVD-1];
				c.fillRect(xo + xi*freq + 2, y, nbarw, yh);
				c.fillStyle = alpha_station_colors[alpha.SEYD-1];
				c.fillRect(xo + xi*freq + (2+nbarw+2), y, nbarw, yh);
			} else
			if (station) {
				c.fillStyle = alpha_station_colors[station-1];
				c.fillRect(xo + xi*freq, y, barw, yh);
			}
			y += yi;
		}
	}

	y = 192;
	c.font = '12px Verdana';
	for (var station = 0; station <= 4; station++) {
		c.fillStyle = alpha_station_colors[station];
		c.fillRect(xo, y, yh, barw);
		c.fillStyle = 'white';
		c.fillText(alpha_stations[station], xo + yh + 4, y + barw);
		y += 16;
	}

	// draw frequency markers on FFT canvas
	var c = integrate_data_canvas.ctx;
	var pxphz = integ_w / ext_sample_rate();
	
	c.font = '12px Verdana';
	c.fillStyle = 'white';
	for (var freq = 0; freq < 4; freq++) {
		var f_s = alpha_sched[1][freq].toFixed(2);
		var f = parseFloat(f_s) * 1e3;
		integrate_marker(f_s, alpha_sched[2][freq], f);
	}
}

// FIXME: allow zero to indicate continuous acquire mode
function integrate_itime_cb(path, val)
{
	var itime = parseFloat(val);
	if (itime < 1) itime = 1;
	itime = itime.toFixed(3);
	w3_num_cb(path, itime);
	ext_send('SET itime='+ itime);
	//console.log('itime='+ itime);
	integrate_clear();
}

function integrate_set_itime(itime)
{
	w3_set_value('integrate.itime', itime);
	integrate_itime_cb('integrate.itime', itime);
}

var integrate_preset = -1;

function integrate_pre_select_cb(path, idx)
{
	idx = +idx;
	integ_draw = false;
	integrate_preset = idx;
	
	switch (integrate_preset) {
	
	case 0:
		ext_tune(11.5, 'usb', ext_zoom.NOM_IN);
		ext_set_passband(300, 3470);
		integrate_set_itime(3.6);
		break;
	
	case 1:
		ext_tune(40, 'cw', ext_zoom.NOM_IN);
		integrate_set_itime(1.0);
		break;
	
	case 2:
		ext_tune(60, 'cw', ext_zoom.NOM_IN);
		integrate_set_itime(1.0);
		break;
	
	case 3:
		ext_tune(66.35, 'cw', ext_zoom.NOM_IN);
		integrate_set_itime(1.0);
		break;
	
	case 4:
		ext_tune(68.5, 'cw', ext_zoom.NOM_IN);
		integrate_set_itime(1.0);
		break;
	
	case 5:
		ext_tune(77.5, 'cw', ext_zoom.NOM_IN);
		integrate_set_itime(1.0);
		break;
	
	default:
		break;
	}
}

function integrate_maxdb_cb(path, val, complete, first)
{
   integ_maxdb = parseFloat(val);
	w3_num_cb(path, val);
	w3_set_label('WF max '+ val +' dBFS', path);
}

function integrate_mindb_cb(path, val, complete, first)
{
   integ_mindb = parseFloat(val);
	w3_num_cb(path, val);
	w3_set_label('WF min '+ val +' dBFS', path);
}

function integrate_clear_cb(path, val)
{
	integrate_clear();
	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

// hook that is called when controls panel is closed
function integrate_blur()
{
	//console.log('### integrate_blur');
	kiwi_clearInterval(integrate_update_interval);
}

// called to display HTML for configuration parameters in admin interface
function integrate_config_html()
{
	ext_admin_config(integrate_ext_name, 'Integrate',
		w3_div('id-integrate w3-text-teal w3-hide',
			'<b>Audio integration configuration</b>' +
			'<hr>' +
			''
			/*
			w3_third('', 'w3-container',
				w3_divs('w3-margin-bottom',
					w3_input_get('', 'int1', 'integrate.int1', 'w3_num_cb'),
					w3_input_get('', 'int2', 'integrate.int2', 'w3_num_cb')
				), '', ''
			)
			*/
		)
	);
}
