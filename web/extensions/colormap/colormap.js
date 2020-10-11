// Copyright (c) 2017 John Seamons, ZL/KF6VO

var cmap = {
   ext_name: 'colormap',     // NB: must match colormap.cpp:colormap_ext.name
   first_time: true,
   
   which: 0,
   R: 0, G: 1, B: 2,
   color: [ 'red', 'green', 'blue' ],
   colors: [ '#ff0000', '#00ff00', '#2196F3' ],
   rgb: 0,
   draw_r: 0,
   draw_g: 0,
   draw_b: 0,
   draw: 0,
   draw_s: [ 'points', 'lines' ],
   draw_e: { points:0, lines:1 },
   drawing: 0,
   last_x: 0,
   mousedown: 0,
   w: 384,
   h: 128,
   h_ramp: 32,
   button: 0,
   gain_red: 0,
   gain_green: 0,
   gain_blue: 0,
   n_custom: 4,
   save: {},
   aper_algo: 3,
   aper_algo_s: [ 'IIR', 'MMA', 'EMA', 'off' ],
   aper_algo_e: { IIR:0, MMA:1, EMA:2, OFF:3 },
   aper_param: 0,
   aper_param_s: [ 'gain', 'averages', 'averages', '' ],
   aper_params: {
      IIR_min:0, IIR_max:2, IIR_step:0.1, IIR_def:0.8, IIR_val:undefined,
      MMA_min:1, MMA_max:16, MMA_step:1, MMA_def:2, MMA_val:undefined,
      EMA_min:1, EMA_max:16, EMA_step:1, EMA_def:2, EMA_val:undefined,
   },

   ef: 0
};

function colormap_main()
{
	ext_switch_to_client(cmap.ext_name, cmap.first_time, colormap_recv);		// tell server to use us (again)
	if (!cmap.first_time)
		colormap_controls_setup();
	cmap.first_time = false;
}

function colormap_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('colormap_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('colormap_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('colormap_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				colormap_controls_setup();
				break;

			default:
				console.log('colormap_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function colormap_plot()
{
/*
	var c = cmap.canvas.ctx;
	c.fillStyle = 'mediumBlue';
	c.fillRect(0, 0, 256, 256);
	
	for (var y = 0; y < 255; y++) {
	   var y_0t1 = y/255;
	   var y_1t0 = 1 - y_0t1;
	   var x;
	   
	   if (0) {
         //var emod = 1/7 * Math.exp(cmap.ef * (-2 + 4*y_0t1));
         //var emod = Math.exp(cmap.ef * (-2 + 4*y_0t1));
         var emod = Math.exp(cmap.ef * y_1t0);
         x = emod*y;
         if (x < 0) x = 0;
         if (x > 255) x = 255;
         c.fillStyle = 'white';
         //c.fillRect(x, 255-y, 1, 1);
      }

      //x = y + y * (1 - cmap.gain * Math.exp(cmap.ef * y_1t0));
      var yy = y_1t0;
      x = (255-y) * (1 - cmap.gain * Math.exp(cmap.ef * yy));
      //if (y==200) console.log('yy='+ yy +' eff='+ (cmap.ef * yy) +'|'+ Math.exp(cmap.ef * yy) +' x='+ x);
	   if (x < 0) x = 0;
	   if (x > 255) x = 255;
	   c.fillStyle = 'lime';
	   c.fillRect(x, y, 1, 1);

	   c.fillStyle = 'yellow';
	   c.fillRect(255-y, y, 1, 1);
	}
*/
}

function colormap_controls_setup()
{
	var controls_html =
		w3_div('id-colormap-controls w3-text-white',
		   w3_divs('',
            w3_div('w3-medium w3-text-aqua w3-margin-B-16', '<b>Colormap control</b>'),

            w3_col_percent('w3-valign w3-margin-T-8/',
               w3_div('w3-text-css-orange', '<b>Aperture<br>auto<br>mode</b>'), 17,
               w3_select('id-cmap-aper-algo|color:red', 'Averaging', '', 'cmap.aper_algo', cmap.aper_algo, cmap.aper_algo_s, 'colormap_aper_algo_cb'), 20,
               w3_div('id-cmap-aper-param',
                  w3_slider('id-cmap-aper-param-slider', 'Parameter', 'cmap.aper_param', cmap.aper_param, 0, 10, 1, 'colormap_aper_param_cb')
               ), 40,
               '&nbsp;', 3, w3_div('id-cmap-aper-param-field')
            ),
            
            w3_inline('id-cmap-maxmin w3-margin-T-8 w3-hide w3-text-white w3-small/',
               'Min/max:&nbsp;', w3_div('id-cmap-min'), '/', w3_div('id-cmap-max'), '&nbsp;=&nbsp;',
               w3_div('id-cmap-min-comp'), '/', w3_div('id-cmap-max-comp'), '&nbsp;(computed) +&nbsp;',
               w3_div('id-cmap-min-floor'), '/', w3_div('id-cmap-max-ceil'), '&nbsp;(floor/ceil)'
            ),
            w3_div('id-cmap-maxmin-empty w3-margin-T-8 w3-small', '&nbsp;'),

		      w3_col_percent('w3-margin-T-32 w3-margin-B-16 w3-valign/',
               w3_div('w3-text-css-orange', '<b>Colormap<br>designer</b>'), 20,
               w3_select('|color:red', '', 'colormap', 'wf.cmap', wf.cmap, kiwi.cmap_s, 'wf_cmap_cb'), 27,
               w3_div('w3-text-white', 'select custom colormap<br>then draw in box below'), 40,
               w3_button('w3-btn w3-round-xlarge w3-padding-small w3-aqua', 'clear', 'colormap_clear_button_cb')
            ),

            w3_divs('',
               w3_col_percent('w3-valign/',
                  w3_radio_button('id-cmap-btn-red w3-btn w3-round-xlarge w3-padding-small', 'red', cmap.button, w3_SELECTED, 'colormap_color_button_cb', 0), 17,
                  w3_select('|color:red', '', 'draw', 'cmap.draw_r', cmap.draw_r, cmap.draw_s, 'colormap_draw_mode_cb', 0), 20,
                  w3_slider('id-cmap-gain-red-slider', '', 'cmap.gain_red', cmap.gain_red, -1.04, 1.04, 0.01, 'colormap_color_gain_cb', 0), 40,
                  '&nbsp;', 3, w3_div('id-cmap-gain-red-field')
               ),
               
               w3_col_percent('w3-valign w3-margin-T-8/',
                  w3_radio_button('id-cmap-btn-green w3-btn w3-round-xlarge w3-padding-small', 'green', cmap.button, w3_NOT_SELECTED, 'colormap_color_button_cb', 1), 17,
                  w3_select('|color:red', '', 'draw', 'cmap.draw_g', cmap.draw_g, cmap.draw_s, 'colormap_draw_mode_cb', 1), 20,
                  w3_slider('id-cmap-gain-red-slider', '', 'cmap.gain_green', cmap.gain_green, -1.04, 1.04, 0.01, 'colormap_color_gain_cb', 1), 40,
                  '&nbsp;', 3, w3_div('id-cmap-gain-green-field')
               ),
               
               w3_col_percent('w3-valign w3-margin-T-8 w3-margin-B-16/',
                  w3_radio_button('id-cmap-btn-blue w3-btn w3-round-xlarge w3-padding-small', 'blue', cmap.button, w3_NOT_SELECTED, 'colormap_color_button_cb', 2), 17,
                  w3_select('|color:red', '', 'draw', 'cmap.draw_b', cmap.draw_b, cmap.draw_s, 'colormap_draw_mode_cb', 2), 20,
                  w3_slider('id-cmap-gain-red-slider', '', 'cmap.gain_blue', cmap.gain_blue, -1.04, 1.04, 0.01, 'colormap_color_gain_cb', 2), 40,
                  '&nbsp;', 3, w3_div('id-cmap-gain-blue-field')
               ),
               
               w3_div('|border: 16px solid rgba(255,255,255,0.2); border-radius: 10px; width: 416px',
                  w3_div('|left:0px; width:'+ px(cmap.w) +'; height:'+ px(cmap.h) +'; overflow:hidden; position:relative;',
                     '<canvas id="id-cmap-transfer-canvas" width="'+ cmap.w +'" height="'+ cmap.h +'" style="position:absolute"></canvas>',
                     '<canvas id="id-cmap-overlay-canvas" width="'+ cmap.w +'" height="'+ cmap.h +'" style="position:absolute"></canvas>'
                  )
               ),
               
               w3_div('w3-margin-L-16 w3-margin-T-16|left:0px; width:'+ px(cmap.w) +'; height:'+ px(cmap.h_ramp) +'; background-color:black; overflow:hidden; position:relative;',
                  '<canvas id="id-cmap-ramp-canvas" width="'+ cmap.w +'" height="'+ cmap.h_ramp +'" style="position:absolute"></canvas>'
               )
            )
            
            /*
            w3_divs('w3-container/w3-tspace-8',
               w3_div('w3-text-css-orange', '<b>Colormap definition</b>'),
               w3_slider('', 'Exp', 'cmap.ef', cmap.ef, -10, 10, 0.1, 'colormap_exp_cb'),
               w3_slider('', 'Gain', 'cmap.gain', cmap.gain, 0, 2, 0.01, 'colormap_gain_cb'),
               w3_input('id-colormap-input1||size=10')
            )
            */
         )
		);

	ext_panel_show(controls_html, null);
	ext_set_controls_width_height(440, 530);
	
	if (wf.aper == kiwi.aper_e.auto) {
      w3_show_inline('id-cmap-maxmin');
      colormap_maxmin_cb();
      w3_hide('id-cmap-maxmin-empty');
   }

   w3_set_highlight_color('id-cmap-btn-red', 'w3-red');
   w3_set_highlight_color('id-cmap-btn-green', 'w3-green');
   w3_set_highlight_color('id-cmap-btn-blue', 'w3-blue');
   
	cmap.transfer_canvas = w3_el('id-cmap-transfer-canvas');
	cmap.transfer_canvas.ctx = cmap.transfer_canvas.getContext("2d");
	cmap.overlay_canvas = w3_el('id-cmap-overlay-canvas');
	cmap.overlay_canvas.ctx = cmap.overlay_canvas.getContext("2d");
	
	for (m = 0; m < cmap.n_custom; m++) {
	   var cm = cmap.save['cm'+ m] = {};
      for (var rgb = 0; rgb < 3; rgb++) {
         var cmc = cm['rgb'[rgb]] = {};
         cmc.gain_slider = 0;
         cmc.gain = 1;
         cmc.y = [];
         for (var x = 0; x < cmap.w; x++) {
            cmc.y[x] = 0;
         }
      }
	}
	
	var el = w3_el('id-colormap-controls');
	el.addEventListener("mousedown", colormap_transfer_mousedown_cb, false);
	el.addEventListener("mousemove", colormap_transfer_mousemove_cb, false);
	el.addEventListener("mouseup", colormap_transfer_mouseup_cb, false);

	cmap.ramp_canvas = w3_el('id-cmap-ramp-canvas');
	cmap.ramp_canvas.ctx = cmap.ramp_canvas.getContext("2d");

   colormap_select();
	//colormap_plot();
}

function colormap_init()
{
   // has to be done after spectrum_init() but before audioFFT_setup()
   var init_cmap = +ext_get_cfg_param('init.colormap', -1, EXT_NO_SAVE);
   //console.log('# cmap  init_cmap='+ init_cmap +' cmap_override='+ wf.cmap_override);
   var last_cmap = readCookie('last_cmap', (init_cmap == -1)? 0 : init_cmap);
   if (wf.cmap_override != -1) last_cmap = wf.cmap_override;
   wf_cmap_cb('wf.cmap', last_cmap, false);     // writes 'last_cmap' cookie

   var init_aper = +ext_get_cfg_param('init.aperture', -1, EXT_NO_SAVE);
   //console.log('# cmap init_aper='+ init_aper);
   var last_aper = readCookie('last_aper', (init_aper == -1)? 0 : init_aper);
   wf_aper_cb('wf.aper', last_aper, false);     // writes 'last_aper' cookie
   w3_show('id-aper-data');

   wf.aper_w = parseFloat(w3_el('id-control').style.width);
   wf.aper_h = 16;
   w3_el('id-aper-data').style.width = px(wf.aper_w);
   var aper_canvas = w3_el('id-aper-canvas');
   aper_canvas.width = wf.aper_w;
   wf.aper_ctx = aper_canvas.getContext("2d");
}

function colormap_select()
{
   var which = cmap.which = (wf.cmap >= kiwi.cmap_e.custom_1)? (wf.cmap - kiwi.cmap_e.custom_1) : -1;
   var s = localStorage.getItem('colormap');
   
   if (cmap.transfer_canvas && which == -1) {
      var c = cmap.transfer_canvas.ctx;
      c.fillStyle = 'black';
      c.fillRect(0, 0, cmap.w, cmap.h);
      c.strokeStyle = 'white';
      c.beginPath();
      c.moveTo(0, 0);
      c.lineTo(cmap.w, cmap.h);
      c.stroke();
      c.beginPath();
      c.moveTo(cmap.w, 0);
      c.lineTo(0, cmap.h);
      c.stroke();
      
      c = cmap.ramp_canvas.ctx;
      c.fillStyle = 'black';
      c.fillRect(0, 0, cmap.w, cmap.h_ramp);
   }
   
   if (which != -1) {
      //console.log('LOAD '+ s);
      if (s != null) {
         cmap.save = JSON.parse(s);
         if (cmap.transfer_canvas) {
            var cm = cmap.save['cm'+ which];
            colormap_color_gain_cb('cmap.gain_red', cm.r.gain_slider, 1, false, 0);
            colormap_color_gain_cb('cmap.gain_green', cm.g.gain_slider, 1, false, 1);
            colormap_color_gain_cb('cmap.gain_blue', cm.b.gain_slider, 1, false, 2);
         } else {
            // if called without colormap ext open only update wf colormap
            colormap_update_wf_colormap();
         }
      }
   }
}

function colormap_aper()
{
	if (!wf.aper_ctx) return;
	//var aper_min = Math.max(-120 - zoomCorrection(), -140);
	var aper_min = -140;
	var aper_max = -20;
	var rng = aper_max - aper_min;
	var last_dBm = aper_min;

	for (var x = 0; x < wf.aper_w; x++) {
	   var dBm = -140 + (rng * (x / wf.aper_w));
	   var cmi = color_index_max_min(dBm, maxdb, mindb);
		var color = color_map[cmi];
		if ((mindb >= last_dBm && mindb <= dBm) || (maxdb >= last_dBm && maxdb <= dBm))
		   color ^= 0xffffffff;
		var color_name = '#'+(color >>> 8).toString(16).leadingZeros(6);
	   wf.aper_ctx.fillStyle = color_name;
	   wf.aper_ctx.fillRect(x,0, 1,wf.aper_h);
		if ((dBm >= mindb-3 && dBm <= mindb) || (dBm >= maxdb && dBm <= maxdb+3)) {
		   color = color_map[cmi] ^ 0xffffffff;
		   color_name = '#'+(color >>> 8).toString(16).leadingZeros(6);
	      wf.aper_ctx.fillStyle = color_name;
	      wf.aper_ctx.fillRect(x,wf.aper_h/2, 1,1);
		}
	   last_dBm = dBm;
	}
}

function colormap_aper_algo_cb(path, idx, first)
{
   if (first) {
      idx = +readCookie('last_aper_algo', cmap.aper_algo_e.OFF);
      w3_set_value(path, idx);
   } else {
      idx = +idx;
   }
   //console.log('colormap_aper_algo_cb ENTER path='+ path +' idx='+ idx +' first='+ first);
   //kiwi_trace('colormap_aper_algo_cb');
   wf.aper_algo = cmap.aper_algo = +idx;

   if (cmap.aper_algo == cmap.aper_algo_e.OFF) {
      w3_hide(w3_el('id-cmap-aper-param').parentElement);
      w3_set_innerHTML('id-cmap-aper-param-field', 'aperture auto-scale on <br> waterfall pan/zoom only');
      colormap_update();
   } else {
      var f_a = cmap.aper_algo;
      var f_s = cmap.aper_algo_s[f_a];
      var f_p = cmap.aper_params;
      var val = f_p[f_s +'_val'];
      //console.log('colormap_aper_algo_cb menu='+ f_a +'('+ f_s +') val='+ val);
   
      // update slider to match menu change
      w3_show(w3_el('id-cmap-aper-param').parentElement);
      colormap_aper_param_cb('id-cmap-aper-param-slider', val, /* done */ true, /* first */ false);
      //console.log('colormap_aper_algo_cb EXIT path='+ path +' menu='+ f_a +'('+ f_s +') param='+ cmap.aper_param);
   }
   
	writeCookie('last_aper_algo', cmap.aper_algo.toString());
   freqset_select();
}

function colormap_aper_param_cb(path, val, done, first)
{
   if (first) return;
   val = +val;
   //console.log('colormap_aper_param_cb ENTER path='+ path +' val='+ val +' done='+ done);
   //kiwi_trace('colormap_aper_param_cb');
   var f_a = cmap.aper_algo;
   var f_s = cmap.aper_algo_s[f_a];
   var f_p = cmap.aper_params;

   if (isUndefined(val) || isNaN(val)) {
      val = f_p[f_s +'_def'];
      //console.log('colormap_aper_param_cb using default='+ val +'('+ typeof(val) +')');
      var lsf = parseFloat(readCookie('last_aper_algo'));
      var lsfp = parseFloat(readCookie('last_aper_param'));
      if (lsf == f_a && !isNaN(lsfp)) {
         //console.log('colormap_aper_param_cb USING READ_COOKIE last_aper_param='+ lsfp);
         val = lsfp;
      }
   }

	wf.aper_param = cmap.aper_param = f_p[f_s +'_val'] = val;
	//console.log('colormap_aper_param_cb UPDATE slider='+ val +' menu='+ f_a +' done='+ done +' first='+ first);

   // needed because called by colormap_aper_algo_cb()
   w3_slider_setup('id-cmap-aper-param-slider', f_p[f_s +'_min'], f_p[f_s +'_max'], f_p[f_s +'_step'], val);
   w3_set_innerHTML('id-cmap-aper-param-field', (f_a == cmap.aper_algo_e.OFF)? '' : (val +' '+ cmap.aper_param_s[f_a]));

   if (done) {
	   //console.log('colormap_aper_param_cb DONE WRITE_COOKIE last_aper_param='+ val.toFixed(2));
	   writeCookie('last_aper_param', val.toFixed(2));
      colormap_update();
      freqset_select();
   }

   //console.log('colormap_aper_param_cb EXIT path='+ path);
}

function colormap_maxmin_cb()
{
   w3_set_innerHTML('id-cmap-max', maxdb.toString().positiveWithSign());
   w3_set_innerHTML('id-cmap-max-comp', wf.auto_maxdb.toString().positiveWithSign());
   w3_set_innerHTML('id-cmap-max-ceil', wf.auto_ceil.val.toString().positiveWithSign());
   w3_set_innerHTML('id-cmap-min', mindb.toString().positiveWithSign());
   w3_set_innerHTML('id-cmap-min-comp', wf.auto_mindb.toString().positiveWithSign());
   w3_set_innerHTML('id-cmap-min-floor', wf.auto_floor.val.toString().positiveWithSign());
}

function colormap_color_button_cb(path, idx, first, param)
{
   cmap.rgb = +param;
   cmap.draw = cmap['draw_'+ 'rgb'[cmap.rgb]];
   //console.log('# draw='+ cmap.draw);
}

function colormap_draw_mode_cb(path, idx, first, param)
{
   idx = +idx;
   param = +param;
   if (cmap.rgb == param) {
      cmap.draw = idx;
      //console.log('# draw='+ cmap.draw);
   }
   w3_num_cb(path, idx);
}

function colormap_clear_button_cb(path, idx)
{
   // pass y_net = 0 to colormap_color_gain_cb() to clear both gain and graph values
	colormap_color_gain_cb('', 0, true, false, cmap.R, 0);
	colormap_color_gain_cb('', 0, true, false, cmap.G, 0);
	colormap_color_gain_cb('', 0, true, false, cmap.B, 0);
   localStorage.setItem('colormap', JSON.stringify(cmap.save));
}

function colormap_color_gain_cb(path, val, done, first, param, y_new)
{
   if (first) return;
   var rgb = +param;
   
   // slider val: -1..0..1
   // gain: 0..1..3
   var sv = val = +val;
   
   // snap-to-zero interval
   if (val > 0 && val < 0.05) {
      sv = val = 0;
   } else
   if (val >= 0.05) {
      val -= 0.04;
   }

   if (val < 0 && val > -0.05) {
      sv = val = 0;
   } else
   if (val <= -0.05) {
      val += 0.04;
   }
   
   var gain = (val < 0)? (1+val) : (1+val*2);
   //var gain = (val < 0)? (1+val) : (1 + (9.0 * Math.log10(1+val*9)));
   //console.log('colormap_color_gain_cb: val='+ val +' gain='+ gain +' done='+ done +' rgb='+ rgb);
   var color = cmap.color[rgb];
   w3_set_innerHTML('id-cmap-gain-'+ color +'-field', 'gain '+ gain.toFixed(2));

   if (cmap.which == -1) return;
   var cm = cmap.save['cm'+ cmap.which]['rgb'[rgb]];
   cm.gain_slider = val;
   cm.gain = gain;
   w3_set_value(path, sv);
   w3_color(path, null, (sv < 0)? 'rgba(255,0,0,0.3)' : (sv? 'rgba(0,255,0,0.2)':''));
	colormap_draw_transfer(rgb, 0, cmap.w-1, 1, y_new);
}

// if y_new undefined then current rgb channel only scaled (gain multiplied)
function colormap_draw_transfer(rgb, sx, ex, xinc, y_new)
{
   var which = cmap.which;
   if (which == -1) return;
   
	var c = cmap.transfer_canvas.ctx;
   var cm = cmap.save['cm' + which], cmc;
	var y;

   rgb++; if (rgb >= 3) rgb = 0;
   cmc = cm['rgb'[rgb]];
   var gain1 = cmc.gain;
   var y_color1 = cmc.y;
   var color1 = cmap.colors[rgb];

   rgb++; if (rgb >= 3) rgb = 0;
   cmc = cm['rgb'[rgb]];
   var gain2 = cmc.gain;
   var y_color2 = cmc.y;
   var color2 = cmap.colors[rgb];

   rgb++; if (rgb >= 3) rgb = 0;
   cmc = cm['rgb'[rgb]];
   var gain3 = cmc.gain;
   var y_color3 = cmc.y;
   var color3 = cmap.colors[rgb];


   for (var x = sx; (xinc > 0 && x <= ex) || (xinc < 0 && x >= ex); x += xinc) {
      c.fillStyle = 'black';
      c.fillRect(x, 0, 1, cmap.h);
   
      y = y_color1[x] * gain1;
      y = w3_clamp(Math.round(y), 0, cmap.h-1);
      c.fillStyle = color1;
      c.fillRect(x, y, 2,2);
   
      y = y_color2[x] * gain2;
      y = w3_clamp(Math.round(y), 0, cmap.h-1);
      c.fillStyle = color2;
      c.fillRect(x, y, 2,2);

      if (isArg(y_new))
         y = y_color3[x] = y_new;
      else
         y = y_color3[x];
      y *= gain3;
      y = w3_clamp(Math.round(y), 0, cmap.h-1);
      //w3_innerHTML('id-owner-info', 'rgb='+ rgb +' x='+ x +' y_new='+ y_new);
      c.fillStyle = color3;
      c.fillRect(x, y, 2,2);
   }
   
   var r_gain = cm['r'].gain;
   var g_gain = cm['g'].gain;
   var b_gain = cm['b'].gain;
   var r_y_color = cm['r'].y;
   var g_y_color = cm['g'].y;
   var b_y_color = cm['b'].y;

	var c = cmap.ramp_canvas.ctx;
	var hm1 = cmap.h-1;
   for (x = 0; x < cmap.w; x++) {
      var r = w3_clamp(r_y_color[x] * r_gain, 0, hm1); r = r/hm1 * 255;
      var g = w3_clamp(g_y_color[x] * g_gain, 0, hm1); g = g/hm1 * 255;
      var b = w3_clamp(b_y_color[x] * b_gain, 0, hm1); b = b/hm1 * 255;
      c.fillStyle = kiwi_rgb(r,g,b);
      c.fillRect(x, 0, 1, cmap.h_ramp);
   }
   
   colormap_update_wf_colormap();
}

function colormap_update_wf_colormap()
{
   var which = cmap.which;
   if (which == -1) return;
   var ccm = wf.custom_colormaps[which];
	var hm1 = cmap.h-1;
   
   var cm = cmap.save['cm' + which];
   var r_gain = cm['r'].gain;
   var g_gain = cm['g'].gain;
   var b_gain = cm['b'].gain;
   var r_y_color = cm['r'].y;
   var g_y_color = cm['g'].y;
   var b_y_color = cm['b'].y;

   for (x = 0; x < 256; x++) {
      var i = Math.round(x/255 * (cmap.w-1));
      var r = w3_clamp(r_y_color[i] * r_gain, 0, hm1); r = r/hm1 * 255;
      var g = w3_clamp(g_y_color[i] * g_gain, 0, hm1); g = g/hm1 * 255;
      var b = w3_clamp(b_y_color[i] * b_gain, 0, hm1); b = b/hm1 * 255;
      ccm[x * 3 + 0] = w3_clamp(Math.round(r), 0, 255);
      ccm[x * 3 + 1] = w3_clamp(Math.round(g), 0, 255);
      ccm[x * 3 + 2] = w3_clamp(Math.round(b), 0, 255);
   }

   mkcolormap();
   spectrum_dB_bands();
   colormap_aper();
}

function colormap_mouseXY(el, evt)
{
   var rect = w3_el(el).getBoundingClientRect();
   return { x: Math.round(evt.clientX - rect.left), y: Math.round(evt.clientY - rect.top) };
}

function colormap_transfer_mousedown_cb(evt)
{
   switch (cmap.draw) {
   
   case cmap.draw_e.points:
      cmap.mousedown = 1;
      break;

   case cmap.draw_e.lines:
      if (!cmap.drawing) {
         var xy = colormap_mouseXY('id-cmap-overlay-canvas', evt);
         if (xy.y < -16 || xy.y > (cmap.h + 16) || xy.x < -16 || xy.x > (cmap.w + 16)) return;
         cmap.xy0 = xy;
         cmap.xy1 = null;
         //console.log('DN '+ xy.x +','+ xy.y);
         cmap.drawing = 1;
      }
      break;
   
   default:
      break;
   }
}

function colormap_transfer_mousemove_cb(evt)
{
   if (cmap.which == -1) return;
   
   switch (cmap.draw) {
   
   case cmap.draw_e.points:
      var xy = colormap_mouseXY('id-cmap-transfer-canvas', evt);
      //w3_innerHTML('id-owner-info', xy.x.toFixed(0) +','+ xy.y.toFixed(0) +' b='+ evt.buttons +' rgb='+ cmap.rgb);
      if (!evt.buttons || xy.y < -16 || xy.y > (cmap.h + 16)) return;

      var y_new = w3_clamp(xy.y, 0, cmap.h-1);
      var x = w3_clamp(xy.x, 0, cmap.w-1);
      if (cmap.mousedown) { cmap.last_x = x; cmap.mousedown = 0; }
      var xinc = (x >= cmap.last_x)? 1:-1;
   
      colormap_draw_transfer(cmap.rgb, cmap.last_x, x, xinc, y_new);
      cmap.last_x = x;
      break;
   
   case cmap.draw_e.lines:
      if (!cmap.drawing) return;
      var c = cmap.overlay_canvas.ctx;
      
      if (cmap.xy1)
         c.clearRect(0, 0, cmap.w, cmap.h);
      
      var xy = colormap_mouseXY('id-cmap-overlay-canvas', evt);
      c.strokeStyle = 'white';
      c.beginPath();
      c.moveTo(cmap.xy0.x, cmap.xy0.y);
      c.lineTo(xy.x, xy.y);
      c.stroke();
      cmap.xy1 = xy;
      //console.log('MM '+ xy.x +','+ xy.y);
      return;
   
   default:
      return;
   }
}

function colormap_transfer_mouseup_cb(evt, escape)
{
   switch (cmap.draw) {
   
   case cmap.draw_e.lines:
      if (!cmap.drawing) return;
      
      if (!cmap.xy1) {
         if (escape) cmap.drawing = 0;
         //console.log('UP !MOVE');
         return;
      }
      
      var c = cmap.overlay_canvas.ctx;
      c.clearRect(0, 0, cmap.w, cmap.h);
      
      if (escape) {
         //console.log('UP-ESC');
         cmap.drawing = 0;
      } else {

         //console.log('UP-DRAW '+ cmap.xy1.x +','+ cmap.xy1.y);
         colormap_bresenham(cmap.xy0, cmap.xy1, function(x,y) {
            colormap_draw_transfer(cmap.rgb, x, x, 1, y);
         });

         var xy = cmap.xy1;
         if (xy.y < -16 || xy.y > (cmap.h + 16) || xy.x < -16 || xy.x > (cmap.w + 16)) {
            //console.log('UP-DONE '+ xy.x +','+ xy.y);
            cmap.drawing = 0;
         } else {
            //console.log('UP-GO '+ xy.x +','+ xy.y);
            cmap.xy0 = cmap.xy1;
            cmap.xy1 = null;
         }
      }
      break;
   
   default:
      break;
   }
   
   var s = JSON.stringify(cmap.save);
   //console.log('SAVE '+ s);
   localStorage.setItem('colormap', s);
}

function colormap_escape_key_cb()
{
   if (cmap.draw == cmap.draw_e.lines && cmap.drawing) {
      colormap_transfer_mouseup_cb(null, true);
      return true;
   }
   
   return false;
}

function colormap_bresenham(xy0, xy1, cb)
{
   var
   x0 = xy0.x, y0 = xy0.y,
   x1 = xy1.x, y1 = xy1.y,
   dx = Math.abs(x1 - x0),
   dy = Math.abs(y1 - y0),
   sx = x0 < x1 ? 1 : -1,
   sy = y0 < y1 ? 1 : -1,
   err = dx - dy;

   while (x0 != x1 || y0 != y1) {
      var e2 = 2 * err;
      if (e2 > (dy * -1)) {
          err -= dy;
          x0 += sx;
      }
      if (e2 < dx) {
          err += dx;
          y0 += sy;
      }
      cb(x0, y0);
   }
}

function colormap_update()
{
   //console.log('SET aper='+ wf.aper +' algo='+ wf.aper_algo +' param='+ wf.aper_param.toFixed(2));
   //kiwi_trace();
   wf_send('SET aper='+ wf.aper +' algo='+ wf.aper_algo +' param='+ wf.aper_param.toFixed(2));
}

function colormap_exp_cb(path, val)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Exp '+ val.toFixed(2), path);
	cmap.ef = val;
	colormap_plot();
}

function colormap_gain_cb(path, val)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Gain '+ val.toFixed(2), path);
	//cmap.gain = val;
	colormap_plot();
}

/*
function colormap_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'Colormap Help') +
         '<br><br>' +
         w3_div('w3-text-css-orange', '<b>Aperture auto mode</b>') +
         'xxx' +

         '<br><br>' +
         w3_div('w3-text-css-orange', '<b>Colormap designer</b>') +
         'xxx' +
         '';
      confirmation_show_content(s, 610, 350);
   }
   return true;
}
*/
