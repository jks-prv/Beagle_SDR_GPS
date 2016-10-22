// Copyright (c) 2016 John Seamons, ZL/KF6VO

var audio_fft_ext_name = 'audio_fft';		// NB: must match audio_fft.c:audio_fft_ext.name

var audio_fft_first_time = 1;

function audio_fft_main()
{
	ext_switch_to_client(audio_fft_ext_name, audio_fft_first_time, audio_fft_recv);		// tell server to use us (again)
	if (!audio_fft_first_time)
		audio_fft_controls_setup();
	audio_fft_first_time = 0;
}

var afft_xo = 200;
var afft_w = 1024;
var afft_th = 200;
var afft_hdr = 12;
var afft_h = afft_th - afft_hdr;
var afft_yo = 0;
var afft_bins, afft_bino = 0;
var afft_draw = false;

function audio_fft_clear()
{
	ext_send('SET clear');
	afft_draw = false;
	var c = audio_fft_data_canvas.ctx;
	c.fillStyle = 'mediumBlue';
	c.fillRect(0, 0, afft_w, afft_th);

	var c = audio_fft_info_canvas.ctx;
	c.fillStyle = '#575757';
	c.fillRect(0, 0, 256, 280);

	switch (audio_fft_preset) {
	
	case 0:
		audio_fft_alpha();
		break;
	
	default:
		var f = ext_get_freq();
		audio_fft_marker((f/1e3).toFixed(2), false, f);
		break;
	}
}

var audio_fft_update_interval;
var afft_last_freq = 0;
var afft_last_offset = 0;

// detect when frequency or offset has changed and adjust display
function audio_fft_update()
{
	var freq = ext_get_freq();
	var offset = freq - ext_get_carrier_freq();
	
	if (freq != afft_last_freq || offset != afft_last_offset) {
		audio_fft_clear();
		//console.log('freq/offset change');
		afft_last_freq = freq;
		afft_last_offset = offset;
	}
}

function audio_fft_marker(txt, left, f)
{
	// draw frequency markers on FFT canvas
	var c = audio_fft_data_canvas.ctx;
	var pxphz = afft_w / audio_fft_sample_rate;
	
	txt = left? (txt + '\u25BC') : ('\u25BC'+ txt);
	c.font = '12px Verdana';
	c.fillStyle = 'white';
	var carrier = ext_get_carrier_freq();
	var car_off = f - carrier;
	var dpx = car_off * pxphz;
	var halfw_tri = c.measureText('\u25BC').width/2 - (left? -1:2);
	var tx = Math.round((afft_w/2-1) + dpx + (left? (-c.measureText(txt).width + halfw_tri) : (-halfw_tri)));
	c.fillText(txt, tx, 10);
	//console.log('MKR='+ txt +' car='+ carrier +' f='+ f +' pxphz='+ pxphz +' coff='+ car_off +' dpx='+ dpx +' tx='+ tx);
}

var audio_fft_sample_rate;
var audio_fft_cmd_e = { FFT:0, CLEAR:1 };
var afft_dbl;
var afft_maxdb, afft_mindb;

function audio_fft_mousedown(evt)
{
	//event_dump(evt, 'FFT');
	var y = (evt.clientY? evt.clientY : (evt.offsetY? evt.offsetY : evt.layerY)) - afft_yo;
	var x = (evt.clientX? evt.clientX : (evt.offsetX? evt.offsetX : evt.layerX)) - afft_xo;
	if (x < 0 || x >= afft_w) return;
	if (y < 0 || y >= afft_h) return;
	var bin = Math.round(y / afft_dbl);
	if (bin < 0 || bin > afft_bins) return;
	afft_bino = Math.round((afft_bino + bin) % afft_bins);
	//console.log('FFT y='+ y +' bino='+ afft_bino +'/'+ afft_bins);
}

function audio_fft_recv(data)
{
	var firstChars = getFirstChars(data, 3);
	
	// process data sent from server/C by ext_send_data_msg()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0] >> 1;
		var ch = ba[0] & 1;
		var o = 1;
		var len = ba.length-1;

		if (cmd == audio_fft_cmd_e.FFT) {
			if (!afft_draw) return;
			
			var c = audio_fft_data_canvas.ctx;
			var im = audio_fft_data_canvas.im;
			var bin = ba[o];
			o++; len--;

			bin -= afft_bino;
			if (bin < 0)
				bin += afft_bins;

			for (var x=0; x < afft_w; x++) {
				var color = waterfall_color_index_max_min(ba[x+o], afft_maxdb, afft_mindb);
				im.data[x*4+0] = color_map_r[color];
				im.data[x*4+1] = color_map_g[color];
				im.data[x*4+2] = color_map_b[color];
				im.data[x*4+3] = 0xff;
			}
			
			for (var y=0; y < afft_dbl; y++) {
				c.putImageData(im, 0, afft_yo + (bin*afft_dbl + y));
			}
		} else
		
		if (cmd == audio_fft_cmd_e.CLEAR) {	// not currently used
			audio_fft_clear();
		} else {
			console.log('audio_fft_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		}
		
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_encoded_msg()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('audio_fft_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('audio_fft_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				audio_fft_controls_setup();
				break;

			case "srate":
				audio_fft_sample_rate = parseFloat(param[1]);
				break;

			case "bins":
				afft_bins = parseInt(param[1]);
				afft_bino = 0;
				afft_dbl = Math.floor(afft_h / afft_bins);
				if (afft_dbl < 1) afft_dbl = 1;
				afft_yo = (afft_h - afft_dbl * afft_bins) / 2;
				if (afft_yo < 0) afft_yo = 0;
				console.log('audio_fft_recv bins='+ afft_bins +' dbl='+ afft_dbl +' yo='+ afft_yo);
				afft_yo += afft_hdr;
				afft_draw = true;
				
				break;

			default:
				console.log('audio_fft_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var audio_fft_itime_init = 10.0;
var audio_fft_maxdb_init = -10;
var audio_fft_mindb_init = -134;

var audio_fft = {
	'itime':audio_fft_itime_init, 'pre':0, 'maxdb':audio_fft_maxdb_init, 'mindb':audio_fft_mindb_init
};

var audio_fft_data_canvas, audio_fft_info_canvas;

function audio_fft_controls_setup()
{
   var data_html =
      '<div id="id-audio_fft-data" style="left:200px; width:1024px; height:200px; background-color:mediumBlue; position:relative; display:none" title="audio_fft">' +
   		'<canvas id="id-audio_fft-data-canvas" width="1024" height="200" style="position:absolute"></canvas>'+
      '</div>';

   var info_html =
      '<div id="id-audio_fft-info" style="left:0px; width:256px; height:280px; overflow:hidden; position:relative;" title="audio_fft">' +
   		'<canvas id="id-audio_fft-info-canvas" width="256" height="280" style="position:absolute"></canvas>'+
      '</div>';

	//jks debug
	if (dbgUs) audio_fft.pre = 1;

	var pre_s = {
		0:'Alpha',
		1:'40 JJY',
		2:'60 WWVB/MSF/JJY',
		3:'68.5 BPC',
		4:'77.5 DCF77' };

	var controls_html =
		w3_divs('id-audio_fft-controls w3-text-white', '',
			w3_half('', '',
				info_html,
				w3_divs('w3-container', 'w3-tspace-8',
					w3_divs('', 'w3-medium w3-text-aqua', '<b>Audio FFT display</b>'),
					w3_input('Integrate time (secs)', 'audio_fft.itime', audio_fft.itime, 'audio_fft_itime_cb', '', 'w3-width-64'),
					w3_select('Presets', 'select', 'audio_fft.pre', audio_fft.pre, pre_s, 'audio_fft_pre_select_cb'),
					w3_slider('WF max', 'audio_fft.maxdb', audio_fft.maxdb, -100, 20, 'audio_fft_maxdb_cb'),
					w3_slider('WF min', 'audio_fft.mindb', audio_fft.mindb, -190, -30, 'audio_fft_mindb_cb'),
					w3_btn('Clear', 'audio_fft_clear_cb')
				)
			)
		);

	ext_panel_show(controls_html, data_html, null);

	audio_fft_data_canvas = html_id('id-audio_fft-data-canvas');
	audio_fft_data_canvas.ctx = audio_fft_data_canvas.getContext("2d");
	audio_fft_data_canvas.im = audio_fft_data_canvas.ctx.createImageData(afft_w, 1);
	audio_fft_data_canvas.addEventListener("mousedown", audio_fft_mousedown, false);

	audio_fft_info_canvas = html_id('id-audio_fft-info-canvas');
	audio_fft_info_canvas.ctx = audio_fft_info_canvas.getContext("2d");

	audio_fft_visible(1);

	audio_fft_itime_cb('audio_fft.itime', audio_fft.itime);
	audio_fft_maxdb_cb('audio_fft.maxdb', audio_fft.maxdb);
	audio_fft_mindb_cb('audio_fft.mindb', audio_fft.mindb);
	
	ext_send('SET run=1');
	audio_fft_clear();

	audio_fft_update_interval = setInterval('audio_fft_update()', 1000);
}

var s = { KRAS:1, NOVO:2, KHAB:3, REVD:4, SEYD:5, MULT:6 };
var alpha_stations = [ 'Krasnodar 38\u00B0E', 'Novosibirsk 84\u00B0E', 'Khabarovsk 136\u00B0E', 'Revda 34\u00B0E', 'Seyda 62\u00B0E' ];
var alpha_station_colors = [ 'yellow', 'red', 'lime', 'cyan', 'magenta' ];
var alpha_freqs = [  ];

var alpha_sched = {
	0: [	'F1',		'F4/5',	'F2',		'F3/p'	],
	1: [	11.9,		12.09,	12.65,	14.88		],		// F5 is really 12.04
	2: [	true,		false,	false,	false		],
	3: [	s.NOVO,	0,			s.REVD,	s.KRAS	],
	4: [	s.SEYD,	s.REVD,	s.NOVO,	s.KHAB	],
	5: [	s.KRAS,	s.SEYD,	s.KHAB,	s.NOVO	],
	6: [	s.KHAB,	0,			s.KRAS,	s.MULT	],
	7: [	s.REVD,	0,			0,			s.SEYD	],
	8: [	0,			0,			s.SEYD,	s.REVD	]
};

function audio_fft_alpha()
{
	var c = audio_fft_info_canvas.ctx;
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
			if (station == s.MULT) {
				c.fillStyle = alpha_station_colors[s.NOVO-1];
				c.fillRect(xo + xi*freq + (2-2-nbarw), y, nbarw, yh);
				c.fillStyle = alpha_station_colors[s.REVD-1];
				c.fillRect(xo + xi*freq + 2, y, nbarw, yh);
				c.fillStyle = alpha_station_colors[s.SEYD-1];
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
	var c = audio_fft_data_canvas.ctx;
	var pxphz = afft_w / audio_fft_sample_rate;
	
	c.font = '12px Verdana';
	c.fillStyle = 'white';
	for (var freq = 0; freq < 4; freq++) {
		var f_s = alpha_sched[1][freq].toFixed(2);
		var f = parseFloat(f_s) * 1e3;
		audio_fft_marker(f_s, alpha_sched[2][freq], f);
	}
}

// FIXME: allow zero to indicate continue acquire mode
function audio_fft_itime_cb(path, val)
{
	var itime = parseFloat(val);
	if (itime < 1) itime = 1;
	itime = itime.toFixed(3);
	w3_num_cb(path, itime);
	ext_send('SET itime='+ itime);
	//console.log('itime='+ itime);
	audio_fft_clear();
}

function audio_fft_set_itime(itime)
{
	w3_set_value('audio_fft.itime', itime);
	audio_fft_itime_cb('audio_fft.itime', itime);
}

var audio_fft_preset = -1;

function audio_fft_pre_select_cb(path, idx)
{
	if (idx) afft_draw = false;
	audio_fft_preset = idx-1;
	
	switch (audio_fft_preset) {
	
	case 0:
		ext_tune(11.5, 'usb', ext_zoom.MAX_IN);
		ext_set_passband(300, 3470);
		audio_fft_set_itime(3.6);
		break;
	
	case 1:
		ext_tune(40, 'cw', ext_zoom.MAX_IN);
		audio_fft_set_itime(1.0);
		break;
	
	case 2:
		ext_tune(60, 'cw', ext_zoom.MAX_IN);
		audio_fft_set_itime(1.0);
		break;
	
	case 3:
		ext_tune(68.5, 'cw', ext_zoom.MAX_IN);
		audio_fft_set_itime(1.0);
		break;
	
	case 4:
		ext_tune(77.5, 'cw', ext_zoom.MAX_IN);
		audio_fft_set_itime(1.0);
		break;
	
	default:
		break;
	}
}

function audio_fft_maxdb_cb(path, val)
{
   afft_maxdb = parseFloat(val);
	w3_num_cb(path, val);
	w3_set_label('WF max '+ val +' dBFS', path);
}

function audio_fft_mindb_cb(path, val)
{
   afft_mindb = parseFloat(val);
	w3_num_cb(path, val);
	w3_set_label('WF min '+ val +' dBFS', path);
}

function audio_fft_clear_cb(path, val)
{
	audio_fft_clear();
	setTimeout('w3_radio_unhighlight('+ q(path) +')', w3_highlight_time);
}

function audio_fft_blur()
{
	//console.log('### audio_fft_blur');
	audio_fft_visible(0);		// hook to be called when controls panel is closed
	kiwi_clearInterval(audio_fft_update_interval);
}

// called to display HTML for configuration parameters in admin interface
function audio_fft_config_html()
{
	ext_admin_config(audio_fft_ext_name, 'FFT',
		w3_divs('id-audio_fft w3-text-teal w3-hide', '',
			'<b>Audio FFT display configuration</b>' +
			'<hr>' +
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					admin_input('int1', 'audio_fft.int1', 'admin_num_cb'),
					admin_input('int2', 'audio_fft.int2', 'admin_num_cb')
				), '', ''
			)
		)
	);
}

function audio_fft_visible(v)
{
	visible_block('id-audio_fft-data', v);
}
