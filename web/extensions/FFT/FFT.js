// Copyright (c) 2016-2021 John Seamons, ZL4VO/KF6VO

var fft_a = { KRAS:1, NOVO:2, KHAB:3, REVD:4, SEYD:5, MULT:6 };

var fft = {
   ext_name: 'FFT',     // NB: must match fft.c:fft_ext.name
   first_time: true,
   update_interval: 0,
   passband_altered: false,
   
   cmd_e: { FFT:0, CLEAR:1 },

   //func: 1,
   //func_e: { OFF:-1, WF:0, SPEC:1, INTEG:2 },
   //func_s: [ 'waterfall', 'spectrum', 'integrate' ],
   func: 0,
   func_e: { OFF:-1, WF:0, SPEC:999, INTEG:1 },
   run: [ -1, 0, 2 ],
   func_s: [ 'waterfall', 'integrate' ],

   pre: -1,
   pre_none: 0,
   pre_ALPHA: 1,
   pre_JJY40: 2,
   pre_RTZ: 3,
   pre_WWVB_MSF_JJY60: 4,
   pre_RBU: 5,
   pre_BPC: 6,
   pre_DCF77: 7,
	pre_s: [
	   'none',
		'Alpha (RSDN-20)',
		'40 JJY',
		'50 RTZ',
		'60 WWVB/MSF/JJY',
		'200/3 RBU',
		'68.5 BPC',
		'77.5 DCF77'
	],

   itime: 10,
   maxdb: -30,
   mindb: -115,
   
   wf_canvas_i: 0,
   wf_canvas: null,
   wf1_canvas: null,
   wf2_canvas: null,
   actual_line: 0,

   integ_canvas: null,
   integ_w: 1024,
   integ_th: 200,
   integ_hdr: 12,
   integ_h: 0,
   integ_yo: 0,
   integ_bins: 0,
   integ_bino: 0,
   integ_draw: false,
   integ_last_freq: 0,
   integ_last_offset: 0,
   integ_dbl: 0,     // vertical line doubling (or more)

   info_canvas: null,

   alpha_stations: [ 'Krasnodar 38\u00B0E', 'Novosibirsk 84\u00B0E', 'Khabarovsk 136\u00B0E', 'Revda 34\u00B0E', 'Seyda 62\u00B0E' ],
   alpha_station_colors: [ 'yellow', 'red', 'lime', 'cyan', 'magenta' ],

   alpha_sched: {
      0: [	'F1',		   'F4/5',	   'F2',		   'F3/p'	   ],
      1: [	11.9,		   12.09,	   12.65,	   14.88		   ],    // F5 is really 12.04 and F3p is slightly different from F3
      2: [	true,		   false,	   false,	   false		   ],
      3: [	fft_a.NOVO,	0,			   fft_a.REVD,	fft_a.KRAS	],
      4: [	fft_a.SEYD,	fft_a.REVD,	fft_a.NOVO,	fft_a.KHAB	],
      5: [	fft_a.KRAS,	fft_a.SEYD,	fft_a.KHAB,	fft_a.NOVO	],
      6: [	fft_a.KHAB,	0,			   fft_a.KRAS,	fft_a.MULT	],
      7: [	fft_a.REVD,	0,			   0,			   fft_a.SEYD	],
      8: [	0,			   0,			   fft_a.SEYD,	fft_a.REVD	]
   },
};

function FFT_main()
{
	ext_switch_to_client(fft.ext_name, fft.first_time, fft_recv);		// tell server to use us (again)
	if (!fft.first_time) {
		fft_controls_setup();
	} else {
      fft.integ_h = fft.integ_th - fft.integ_hdr;
	}
	fft.first_time = false;
}

function fft_clear()
{
	ext_send('SET clear');
	fft.integ_draw = false;
	var w = fft.integ_w, h = fft.integ_h, th = fft.integ_th;

	fft.actual_line = h - 1;
	fft.wf_canvas_i = 0;
	fft.wf_canvas = fft.wf1_canvas;
	var top = -h+1 + fft.integ_hdr;

   fft.wf1_canvas.ctx.clearRect(0, 0, w, h);
	fft.wf1_canvas.top = top;
	fft.wf1_canvas.style.top = px(top);

   fft.wf2_canvas.ctx.clearRect(0, 0, w, h);
	fft.wf2_canvas.top = top;
	fft.wf2_canvas.style.top = px(top);

	var c = fft.integ_canvas.ctx;
	if (fft.func == fft.func_e.WF) {
      c.clearRect(0, 0, w, th);
	   th = fft.integ_hdr;
	}
   c.fillStyle = 'mediumBlue';
   c.fillRect(0, 0, w, th);
   
	if (fft.func == fft.func_e.SPEC) {
      spec.clear_avg = true;
   }

	c = fft.info_canvas.ctx;
	c.fillStyle = '#575757';
	c.fillRect(0, 0, 256, 280);
	
	var left = w3_el('fft-controls-left');
	var right = w3_el('fft-controls-right');

	if (fft.pre == fft.pre_ALPHA) {
		ext_set_controls_width_height(525, 300);
		left.style.width = '49.9%';
		right.style.width = '49.9%';
		fft_alpha();
	} else {
		ext_set_controls_width_height(275, 275);
		left.style.width = '0%';
		right.style.width = '100%';
		var f = ext_get_freq();
		fft_marker((f/1e3).toFixed(2), false, f);
	}

   ext_send('SET run='+ fft.run[fft.func+1]);
}

// detect when frequency or offset has changed and adjust display
function fft_update()
{
	var freq = ext_get_freq();
	var offset = freq - ext_get_carrier_freq();
	
	if (freq != fft.integ_last_freq || offset != fft.integ_last_offset) {
		fft_clear();
		//console.log('freq/offset change');
		fft.integ_last_freq = freq;
		fft.integ_last_offset = offset;
	}
}

function fft_marker(txt, left, f)
{
	// draw frequency markers on FFT canvas
	var c = fft.integ_canvas.ctx;
	var pxphz = fft.integ_w / ext_sample_rate();
	
	txt = left? (txt + '\u25BC') : ('\u25BC'+ txt);
	c.font = '12px Verdana';
	c.fillStyle = 'white';
	var carrier = ext_get_carrier_freq();
	var car_off = f - carrier;
	var dpx = car_off * pxphz;
	var halfw_tri = c.measureText('\u25BC').width/2 - (left? -1:2);
	var tx = Math.round((fft.integ_w/2-1) + dpx + (left? (-c.measureText(txt).width + halfw_tri) : (-halfw_tri)));
	c.fillText(txt, tx, 10);
	//console.log('MKR='+ txt +' car='+ carrier +' f='+ f +' pxphz='+ pxphz +' coff='+ car_off +' dpx='+ dpx +' tx='+ tx);
}

function fft_mousedown(evt)
{
	//event_dump(evt, 'FFT');
	var y = (evt.clientY? evt.clientY : (evt.offsetY? evt.offsetY : evt.layerY)) - fft.integ_yo;
	if (y < 0 || y >= fft.integ_h) return;
	var bin = Math.round(y / fft.integ_dbl);
	if (bin < 0 || bin > fft.integ_bins) return;
	fft.integ_bino = Math.round((fft.integ_bino + bin) % fft.integ_bins);
	//console.log('FFT y='+ y +' bino='+ fft.integ_bino +'/'+ fft.integ_bins);
}

function fft_recv(data_raw)
{
	var firstChars = arrayBufferToStringLen(data_raw, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data_raw, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;
      var bin = ba[o];
      o++; len--;

		if (cmd == fft.cmd_e.FFT) {
			if (spec.source == spec.AUDIO) {
			   var data = new Uint8Array(data_raw, 6);
			   spectrum_update(data);
			} else {
			   if (!fft.integ_draw) return;
			   var fc, c, im;
			   
			   if (fft.func == fft.func_e.WF) {
			      fc = fft.wf_canvas;
               c = fc.ctx;
               im = fc.im;
			   } else {
               c = fft.integ_canvas.ctx;
               im = fft.integ_canvas.im;
               bin -= fft.integ_bino;
               if (bin < 0)
                  bin += fft.integ_bins;
			   }

            for (var x=0; x < fft.integ_w; x++) {
               var color = waterfall_color_index_max_min(ba[x+o], fft.maxdb, fft.mindb);
               im.data[x*4+0] = color_map_r[color];
               im.data[x*4+1] = color_map_g[color];
               im.data[x*4+2] = color_map_b[color];
               im.data[x*4+3] = 0xff;
            }
         
			   if (fft.func == fft.func_e.WF) {
               c.putImageData(im, 0, fft.actual_line);
               fft.actual_line--;
		         fft.wf1_canvas.style.top = px(fft.wf1_canvas.top++);
		         fft.wf2_canvas.style.top = px(fft.wf2_canvas.top++);

               if (fft.actual_line < 0) {
                  var h = fft.integ_h;
		            fft.wf_canvas_i ^= 1;
		            fft.wf_canvas = fft.wf_canvas_i? fft.wf2_canvas : fft.wf1_canvas;
		            fc = fft.wf_canvas;
                  fc.top = -h+1 + fft.integ_hdr;
                  fc.style.top = px(fc.top);
                  fft.actual_line = h - 1;
               }
			   } else {
               for (var y=0; y < fft.integ_dbl; y++) {
                  c.putImageData(im, 0, fft.integ_yo + (bin * fft.integ_dbl + y));
               }
            }
         }
		} else
		
		if (cmd == fft.cmd_e.CLEAR) {	// not currently used
			fft_clear();
		} else {
			console.log('fft_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		}
		
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data_raw);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('fft_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('fft_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				fft_controls_setup();
				break;

			case "bins":
				fft.integ_bins = parseInt(param[1]);
				fft.integ_bino = 0;
				fft.integ_dbl = Math.floor(fft.integ_h / fft.integ_bins);
				if (fft.integ_dbl < 1) fft.integ_dbl = 1;
				fft.integ_yo = (fft.integ_h - fft.integ_dbl * fft.integ_bins) / 2;
				if (fft.integ_yo < 0) fft.integ_yo = 0;
				//console.log('fft_recv bins='+ fft.integ_bins +' dbl='+ fft.integ_dbl +' yo='+ fft.integ_yo);
				fft.integ_yo += fft.integ_hdr;
				fft.integ_draw = true;
				
				break;

			default:
				console.log('fft_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function fft_controls_setup()
{
   var data_html =
      time_display_html('fft') +

      w3_div('id-fft-data|left:150px; width:1024px; height:200px; background-color:mediumBlue; position:relative;',
   		'<canvas id="id-fft-wf1-canvas" width="1024" height="188" style="position:absolute"></canvas>',
   		'<canvas id="id-fft-wf2-canvas" width="1024" height="188" style="position:absolute"></canvas>',
   		// after fft canvases so on top
   		'<canvas id="id-fft-integ-canvas" width="1024" height="200" style="position:absolute"></canvas>'
      );

   var info_html =
      w3_div('id-fft-info|left:0px; height:280px; overflow:hidden; position:relative;',
   		'<canvas id="id-fft-info-canvas" width="256" height="280" style="position:absolute"></canvas>'
      );

   var p = ext_param();
   if (p) {
      p = p.split(',');
      p.forEach(function(a, i) {
         console.log('FFT param <'+ a +'>');
         w3_ext_param_array_match_str(fft.func_s, a, function(i) { fft.func = i; });
         w3_ext_param_array_match_str(fft.pre_s, a, function(i) { fft.pre = i; });
         var r;
         if ((r = w3_ext_param('itime', a)).match) {
            fft.itime = w3_clamp(r.num, 1, 60);
         } else
         if ((r = w3_ext_param('maxdb', a)).match) {
            fft.maxdb = w3_clamp(r.num, -170, -10);
         } else
         if ((r = w3_ext_param('mindb', a)).match) {
            fft.mindb = w3_clamp(r.num, -190, -30);
         } else
         if (w3_ext_param('help', a).match) {
            // delay needed due to interference with spectrum display render
            ext_help_click(true);
         }
      });
   }

	var controls_html =
		w3_div('id-fft-controls w3-text-white',
			w3_half('', '',
				info_html,
				w3_divs('',
					w3_div('w3-medium w3-text-aqua', '<b>Audio FFT</b>'),
					w3_inline('w3-margin-T-8 w3-halign-space-between/',
                  w3_select('w3-text-red', 'Function', '', 'fft.func', fft.func, fft.func_s, 'fft_func_cb'),
                  w3_input('w3-width-64 w3-padding-smaller', 'Integrate time', 'fft.itime', fft.itime, 'fft_itime_cb')
               ),
					w3_select('w3-margin-T-8//w3-text-red w3-width-auto', 'Integrate presets', 'select', 'fft.pre', W3_SELECT_SHOW_TITLE, fft.pre_s, 'fft_pre_select_cb'),
					w3_div('id-fft-msg-fft w3-margin-T-8',
                  w3_slider('', 'max', 'fft.maxdb', fft.maxdb, -170, -10, 1, 'fft_maxdb_cb'),
                  w3_slider('', 'min', 'fft.mindb', fft.mindb, -190, -30, 1, 'fft_mindb_cb')
               ),
               w3_text('id-fft-msg-spec  w3-margin-T-8 w3-hide', 'Spectrum uses main control panel<br>WF tab sliders and settings'),
					w3_button('w3-margin-T-8 w3-padding-small', 'Clear', 'fft_clear_cb')
				), 'id-fft-controls-left', 'id-fft-controls-right'
			)
		);

	ext_panel_show(controls_html, data_html, null);
	fft.saved_mode = ext_get_mode();
	time_display_setup('fft');

	fft.integ_canvas = w3_el('id-fft-integ-canvas');
	fft.integ_canvas.ctx = fft.integ_canvas.getContext("2d");
	fft.integ_canvas.im = fft.integ_canvas.ctx.createImageData(fft.integ_w, 1);
	fft.integ_canvas.addEventListener("mousedown", fft_mousedown, false);

	fft.wf1_canvas = w3_el('id-fft-wf1-canvas');
	fft.wf1_canvas.ctx = fft.wf1_canvas.getContext("2d");
	fft.wf1_canvas.im = fft.wf1_canvas.ctx.createImageData(fft.integ_w, 1);

	fft.wf2_canvas = w3_el('id-fft-wf2-canvas');
	fft.wf2_canvas.ctx = fft.wf2_canvas.getContext("2d");
	fft.wf2_canvas.im = fft.wf2_canvas.ctx.createImageData(fft.integ_w, 1);

	fft.info_canvas = w3_el('id-fft-info-canvas');
	fft.info_canvas.ctx = fft.info_canvas.getContext("2d");

	FFT_environment_changed( {resize:1} );

	fft_itime_cb('fft.itime', fft.itime);
	
	fft_clear();

	fft.update_interval = setInterval(fft_update, 1000);
	fft_func_cb('fft.func', fft.func, false);
	if (fft.pre != -1) fft_pre_select_cb('fft.pre', fft.pre, false);
}

function FFT_environment_changed(changed)
{
   if (changed.mode) {
      //console.log('FFT_environment_changed run='+ fft.func);
      ext_send('SET run='+ fft.run[fft.func+1]);
   }
   
   if (changed.resize) {
      var el = w3_el('id-fft-data');
      var left = (window.innerWidth - fft.integ_w - time_display_width()) / 2;
      el.style.left = px(left);
   }
}

function fft_alpha()
{
	var c = fft.info_canvas.ctx;
	var xo = 64, xi = 48, y = 16, yh = 20, yi = 24, barw = 9, nbarw = 5;

	c.font = '12px Verdana';
	var ty = yh/2 + 10/2;

	for (var freq = 0; freq < 4; freq++) {
		c.fillStyle = 'white';
		var txt = fft.alpha_sched[0][freq];
		var tx = barw/2 - c.measureText(txt).width/2;
		c.fillText(txt, xo + xi*freq + tx, 10);
		var txt = fft.alpha_sched[1][freq].toFixed(2);
		var tx = barw/2 - c.measureText(txt).width/2;
		c.fillText(txt, xo + xi*freq + tx, 26);

		y = 32;
		for (var tslot = 1; tslot <= 6; tslot++) {
			if (freq == 0) {
				c.fillStyle = 'white';
				c.fillText('slot '+ tslot, 16, y + ty);
			}
			
			var station = fft.alpha_sched[tslot+2][freq];
			if (station == fft_a.MULT) {
				c.fillStyle = fft.alpha_station_colors[fft_a.NOVO-1];
				c.fillRect(xo + xi*freq + (2-2-nbarw), y, nbarw, yh);
				c.fillStyle = fft.alpha_station_colors[fft_a.REVD-1];
				c.fillRect(xo + xi*freq + 2, y, nbarw, yh);
				c.fillStyle = fft.alpha_station_colors[fft_a.SEYD-1];
				c.fillRect(xo + xi*freq + (2+nbarw+2), y, nbarw, yh);
			} else
			if (station) {
				c.fillStyle = fft.alpha_station_colors[station-1];
				c.fillRect(xo + xi*freq, y, barw, yh);
			}
			y += yi;
		}
	}

	y = 192;
	c.font = '12px Verdana';
	for (var station = 0; station <= 4; station++) {
		c.fillStyle = fft.alpha_station_colors[station];
		c.fillRect(xo, y, yh, barw);
		c.fillStyle = 'white';
		c.fillText(fft.alpha_stations[station], xo + yh + 4, y + barw);
		y += 16;
	}

	// draw frequency markers on FFT canvas
	var c = fft.integ_canvas.ctx;
	var pxphz = fft.integ_w / ext_sample_rate();
	
	c.font = '12px Verdana';
	c.fillStyle = 'white';
	for (var freq = 0; freq < 4; freq++) {
		var f_s = fft.alpha_sched[1][freq].toFixed(2);
		var f = parseFloat(f_s) * 1e3;
		fft_marker(f_s, fft.alpha_sched[2][freq], f);
	}
}

// FIXME: allow zero to indicate continuous acquire mode
function fft_itime_cb(path, val)
{
	var itime = parseFloat(val);
	if (itime < 1) itime = 1;
	itime = itime.toFixed(3);
	w3_num_cb(path, itime);
	ext_send('SET itime='+ itime);
	//console.log('itime='+ itime);
	fft_clear();
}

function fft_set_itime(itime)
{
	w3_set_value('fft.itime', itime);
	fft_itime_cb('fft.itime', itime);
}

function fft_func_cb(path, idx, first)
{
   console.log('fft_func_cb new='+ idx +' prev='+ fft.func +' first='+ first);
   if (first) return;
	idx = +idx;
	
	switch (idx) {
	
      case fft.func_e.SPEC:
         w3_hide('id-ext-data-container');
         ext_show_spectrum(spec.RF);
         // id-top-container already hidden because we use data_html
         break;
   
      default:
         if (fft.func != fft.func_e.SPEC) break;
         ext_hide_spectrum();
         w3_show_block('id-ext-data-container');
         break;
	}
	
	w3_select_value(path, idx);      // for benefit of direct calls
	fft.func = idx;
	w3_show_hide('id-fft-msg-fft',  fft.func != fft.func_e.SPEC);
	w3_show_hide('id-fft-msg-spec', fft.func == fft.func_e.SPEC);
	fft_clear();
	w3_attribute(fft.integ_canvas, 'title', 'click to align vertically', fft.func == fft.func_e.INTEG);
}

function fft_pre_select_cb(path, idx, first)
{
   //console.log('fft_pre_select_cb path='+ path +' idx='+ idx +' first='+ first);
   if (first) return;
	idx = +idx;
	fft.integ_draw = false;
	fft.pre = idx;
	var func = fft.func_e.INTEG;
	
	switch (fft.pre) {
	
	case fft.pre_none:
		fft_set_itime(10.0);
		break;
	
	case fft.pre_ALPHA:
		ext_tune(11.5, 'usb', ext_zoom.NOM_IN);
		ext_set_passband(300, 3470);
		fft.passband_altered = true;
		fft_set_itime(3.6);
		break;
	
	case fft.pre_JJY40:
		ext_tune(40, 'cw', ext_zoom.NOM_IN);
		fft_set_itime(1.0);
		break;
	
	case fft.pre_RTZ:
		ext_tune(50.32, 'cwn', ext_zoom.NOM_IN);
		fft_set_itime(1.0);
		break;
	
	case fft.pre_WWVB_MSF_JJY60:
		ext_tune(60, 'cw', ext_zoom.NOM_IN);
		fft_set_itime(1.0);
		break;
	
	case fft.pre_RBU:
		ext_tune(66.35, 'cwn', ext_zoom.NOM_IN);
		fft_set_itime(1.0);
		break;
	
	case fft.pre_BPC:
		ext_tune(68.5, 'cw', ext_zoom.NOM_IN);
		fft_set_itime(1.0);
		break;
	
	case fft.pre_DCF77:
		ext_tune(77.5, 'cw', ext_zoom.NOM_IN);
		fft_set_itime(1.0);
		break;
	
	default:
	   func = fft.func_e.SPEC;
		break;
	}

	w3_select_value(path, idx);      // for benefit of direct calls
	//console.log('fft_pre_select_cb SET func='+ func);
	fft_func_cb('fft.func', func, false);
}

function fft_maxdb_cb(path, val, complete, first)
{
   fft.maxdb = parseFloat(val);
	w3_num_cb(path, val);
	w3_set_label('WF/integrate max '+ val +' dB', path);
}

function fft_mindb_cb(path, val, complete, first)
{
   fft.mindb = parseFloat(val);
	w3_num_cb(path, val);
	w3_set_label('WF/integrate min '+ val +' dB', path);
}

function fft_clear_cb(path, val)
{
	fft_clear();
	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

// hook that is called when controls panel is closed
function FFT_blur()
{
   console.log('FFT_blur');
	kiwi_clearInterval(fft.update_interval);
   ext_send('SET run='+ fft.func_e.OFF);
	spec.need_clear_avg = true;   // remove our spectrum data from averaging buffers
   
   if (fft.passband_altered)
	   ext_set_mode(fft.saved_mode);
}

// called to display HTML for configuration parameters in admin interface
function FFT_config_html()
{
   ext_config_html(fft, 'fft', 'FFT', 'FFT configuration');
}

function FFT_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'Audio FFT help') +
         '<br>Remember that on KiwiSDR the audio and waterfall channels are completely separate. <br>' +
         'For example you can pan the waterfall frequency without effecting the audio. <br>' +
         'By contrast this extension allows visualization of the <i>audio</i> channel itself by using<br>' +
         'an FFT, waterfall and integrator (summing waterfall) for weak signals. <br>' +
         
         '<br>URL parameters: <br>' +
         w3_text('|color:orange', 'itime:<i>num</i> &nbsp; maxdb:<i>num</i> &nbsp; mindb:<i>num</i>') +
         '<br> Non-numeric values are those appearing in their respective menus. <br>' +
         'Keywords are case-insensitive and can be abbreviated. <br>' +
         'So for example these are valid: <br>' +
         '<i>ext=fft,integ,itime:5</i> &nbsp;&nbsp; ' +
         '<i>ext=fft,water,min:-130,max:-40</i> &nbsp;&nbsp; <i>ext=fft,alpha</i> <br>' +
         '<br>' +
         'Clicking on integrate display will restart it such that the click-point is <br>' +
         'moved to top of the display (i.e. vertical timing can be realigned).' +
         '';
      confirmation_show_content(s, 610, 300);
   }
   return true;
}
