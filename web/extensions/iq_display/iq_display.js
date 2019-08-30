// Copyright (c) 2016 John Seamons, ZL/KF6VO

var iq_display_ext_name = 'iq_display';		// NB: must match iq_display.c:iq_display_ext.name

var iq = {
   cmd_e: { IQ_POINTS:0, IQ_DENSITY:1, IQ_CLEAR:2 },
   draw: 1,
   mode: 0,
   cmaI: 0,
   cmaQ: 0,
   df: 0,
   pll: 1,
   pll_bw: 5,
   update_interval: 0,
   size: Math.round(Math.log2(1024)),
   offset: 0,
   gain: 0,
   update_interval: 0,
   den_max: 1,
   maxdb: 255,
   mindb: 0,
   gps_correcting: 0,
   gps_correcting_initial: 1,
   adc_clock_Hz: 0,
   phase: 0
};

var iq_display_first_time = true;

function iq_display_main()
{
	ext_switch_to_client(iq_display_ext_name, iq_display_first_time, iq_display_recv);		// tell server to use us (again)
	if (!iq_display_first_time)
		iq_display_controls_setup();
	iq_display_first_time = false;
}

var iq_display_map = new Uint32Array(256*256);

function iq_display_clear()
{
	var c = iq_display_canvas.ctx;
	
	if (iq.draw == iq.cmd_e.IQ_POINTS) {
      c.fillStyle = 'mediumBlue';
      c.fillRect(0, 0, 256, 256);
      c.fillStyle = 'white';
      c.fillRect(0, 128, 256, 1);
      c.fillRect(128, 0, 1, 256);
   }
   
	ext_send('SET clear');
	
	for (var q=0; q < 256; q++)
		for (var i=0; i < 256; i++)
			iq_display_map[q*256 + i] = 0;
	iq.den_max = 1;
}

function iq_display_sched_update()
{
	kiwi_clearInterval(iq.update_interval);
	iq.update_interval = setInterval(iq_display_update, 250);
}

var iq_display_imageData;
var iq_display_upd_cnt = 0;

function iq_display_update()
{
	//console.log('iq_display_update '+ iq.den_max);
	var c = iq_display_canvas.ctx;

	if (iq.draw == iq.cmd_e.IQ_DENSITY) {
		var y=0;
		for (var q=0; q < (256*256); q += 256) {
			for (var i=0; i < 256; i++) {
				var color = iq_display_map[q + i] / iq.den_max * 0xff;
				color = color_index_max_min(color, iq.maxdb, iq.mindb);
				iq_display_imageData.data[i*4+0] = color_map_r[color];
				iq_display_imageData.data[i*4+1] = color_map_g[color];
				iq_display_imageData.data[i*4+2] = color_map_b[color];
				iq_display_imageData.data[i*4+3] = 0xff;
			}
			c.putImageData(iq_display_imageData, 0, y);
			y++;
		}
		/*
      c.fillStyle = 'white';
      c.fillRect(0, 128, 256, 1);
      c.fillRect(128, 0, 1, 256);
      */
	}
	
	if (iq_display_upd_cnt == 3) {
      w3_el('iq_display-cma').innerHTML =
         //'I='+ iq.cmaI.toExponential(1).withSign() +' Q='+ iq.cmaQ.toExponential(1).withSign() +' df='+ iq.df.toExponential(1).withSign();
         'PLL df: '+ iq.df.toFixed(1).withSign() + ' Hz<br>PLL phase: '+ iq.phase.toFixed(3).withSign() + ' rad';
      w3_el('iq_display-adc').innerHTML =
         'ADC clock: '+ (iq.adc_clock_Hz/1e6).toFixed(6) +' MHz';

      var gps_correcting = (cfg.ADC_clk_corr && ext_adc_gps_clock_corr() > 3)? 1:0;
      w3_innerHTML('iq_display-gps', gps_correcting? ('GPS corrections: '+ ext_adc_gps_clock_corr()) : '');
      if (gps_correcting != iq.gps_correcting || iq.gps_correcting_initial) {
         if (!gps_correcting) {
            w3_innerHTML('iq-fcal-p',
               w3_button('w3-css-yellow|margin-left:12px; padding:6px 10px;', 'Fcal '+ w3_icon('', 'fa-repeat'), 'iq_display_IQ_cal_jog_cb', 1)
            )
            w3_innerHTML('iq-fcal-m',
               w3_button('w3-css-yellow|margin-left:12px; padding:6px 10px;', 'Fcal '+ w3_icon('', 'fa-undo'), 'iq_display_IQ_cal_jog_cb', -1)
            )
         } else {
            w3_innerHTML('iq-fcal-p', 'GPS is correcting');
            w3_innerHTML('iq-fcal-m', '');
         }
	      w3_show_hide('id-iq-fcal', !gps_correcting);
         iq.gps_correcting = gps_correcting;
         iq.gps_correcting_initial = 0;
      }

      iq_display_upd_cnt = 0;
   }
   iq_display_upd_cnt++;
}

function iq_display_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0] >> 1;
		var ch = ba[0] & 1;
		var len = ba.length-1;

		if (cmd == iq.cmd_e.IQ_POINTS) {
			var c = iq_display_canvas.ctx;
			var i, q;

			for (var j=1; j < len; j += 4) {
				i = ba[j+0];
				q = ba[j+1];
				c.fillStyle = 'black';
				c.fillRect(i, q, 2, 2);
	
				i = ba[j+2];
				q = ba[j+3];
				c.fillStyle = ch? 'lime':'cyan';
				c.fillRect(i, q, 2, 2);
			}
		} else
		
		if (cmd == iq.cmd_e.IQ_DENSITY) {
			//console.log('IQ_DENSITY '+ len);
			var c = iq_display_canvas.ctx;
			var i, q;

			for (var j=1; j < len; j += 2) {
				i = ba[j+0];
				q = ba[j+1];
				var m = iq_display_map[q*256 + i];
				m++;
				if (m > iq.den_max) iq.den_max = m;
				iq_display_map[q*256 + i] = m;
			}
		} else
		
		if (cmd == iq.cmd_e.IQ_CLEAR) {	// not currently used
			iq_display_clear();
		} else {
			console.log('iq_display_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
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
				console.log('iq_display_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('iq_display_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				iq_display_controls_setup();
				break;

			case "cmaI":
				iq.cmaI = parseFloat(param[1]);
				break;

			case "cmaQ":
				iq.cmaQ = parseFloat(param[1]);
				break;

			case "df":
				iq.df = parseFloat(param[1]);
				break;

			case "phase":
				iq.phase = parseFloat(param[1]);
				break;

			case "adc_clock":
				iq.adc_clock_Hz = parseFloat(param[1]);
				break;

			default:
				console.log('iq_display_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var iq_display_canvas;

function iq_display_controls_setup()
{
   var scope_html =
      w3_div('id-iq_display-scope|left:150px; width:1024px; height:200px; background-color:mediumBlue; position:relative;', 
   		'<canvas id="id-iq_display-scope-canvas" width="1024" height="200" style="position:absolute"></canvas>'
      );

   var data_html =
      w3_div('id-iq_display-data|left:0px; width:256px; height:256px; background-color:mediumBlue; overflow:hidden; position:relative;',
   		'<canvas id="id-iq_display-canvas" width="256" height="256" style="position:absolute"></canvas>'
      );

	var draw_s = [ 'points', 'density' ];
	var mode_s = [ 'IQ', 'carrier' ];
	var pll_s = [ 'off', 'on', 'BPSK', 'QPSK', '8PSK', 'MSK100', 'MSK200' ];

   var p = ext_param();
   if (p) {
      p = p.split(',');
      p.forEach(function(a, i) {
         console.log('IQ param <'+ a +'>');
         w3_ext_param_array_match_str(draw_s, a, function(i) { iq.draw = i; });
         w3_ext_param_array_match_str(mode_s, a, function(i) { iq.mode = i; });
         w3_ext_param_array_match_str(pll_s, a, function(i) { iq.pll = i; });
         var r;
         if ((r = w3_ext_param('size', a)).match) {
            iq.size = w3_clamp(Math.round(Math.log2(r.num)), 4, 14);
         } else
         if ((r = w3_ext_param('pll_bw', a)).match) {
            iq.pll_bw = r.num;
         } else
         if ((r = w3_ext_param('gain', a)).match) {
            iq.gain = w3_clamp(r.num, 0, 100);
         } else
         if ((r = w3_ext_param('cmax', a)).match) {
            iq.maxdb = w3_clamp(r.num, 0, 255);
         } else
         if ((r = w3_ext_param('cmin', a)).match) {
            iq.mindb = w3_clamp(r.num, 0, 255);
         }
      });
   }
   //console.log('IQ iq.pll='+ iq.pll +' iq.pll_bw='+ iq.pll_bw);
   
	var controls_html =
		w3_div('id-iq_display-controls w3-text-white',
			w3_half('', '',
				w3_div('',
				   data_html,
			      w3_div('id-iq_display-cma w3-margin-T-8'),
			      w3_div('id-iq_display-adc'),
			      w3_div('id-iq_display-gps'),
			      w3_div('id-iq-fcal w3-hide')
			   ),
				w3_div('w3-margin-L-8',
					w3_div('w3-medium w3-text-aqua', '<b>IQ display</b>'),
					w3_slider('w3-tspace-8//', 'Gain', 'iq.gain', iq.gain, 0, 100, 1, 'iq_display_gain_cb'),
					w3_inline('w3-tspace-8/w3-margin-between-6',
					   w3_select('', 'Draw', '', 'iq.draw', iq.draw, draw_s, 'iq_display_draw_select_cb'),
					   w3_select('', 'Mode', '', 'iq.mode', iq.mode, mode_s, 'iq_display_mode_select_cb'),
					   w3_select('', 'PLL', '', 'iq.pll', iq.pll, pll_s, 'iq_display_pll_select_cb')
					),
					w3_slider('w3-tspace-8 id-iq-size//', 'Size', 'iq.size', iq.size, 4, 14, 1, 'iq_display_size_cb'),
					w3_slider('w3-tspace-8 id-iq-maxdb w3-hide//', 'Colormap max', 'iq.maxdb', iq.maxdb, 0, 255, 1, 'iq_display_maxdb_cb'),
					w3_slider('id-iq-mindb w3-hide//', 'Colormap min', 'iq.mindb', iq.mindb, 0, 255, 1, 'iq_display_mindb_cb'),
					w3_inline('w3-margin-B-16 w3-tspace-8',
					   w3_input('w3-label-inline w3-padding-smaller|width:auto|size=3', 'PLL bandwidth', 'iq.pll_bw', iq.pll_bw, 'iq_display_pll_bw_cb'),
					   w3_label('w3-margin-L-8', ' Hz')
					),
					w3_inline('w3-tspace-8/w3-margin-between-16',
					   //w3_input('w3-width-128', 'Clock offset', 'iq.offset', iq.offset, 'iq_display_offset_cb'),
						w3_button('w3-padding-small', 'Clear', 'iq_display_clear_cb'),
						w3_button('w3-padding-small', '2.4k', 'iq_display_AM_bw_cb', 2400),
						w3_button('w3-padding-small', '160', 'iq_display_AM_bw_cb', 160),
						w3_button('w3-padding-small', '40', 'iq_display_AM_bw_cb', 40)
					),
					'<hr style="margin:10px 0">',
					w3_col_percent('w3-tspace-8/',
					   w3_button('w3-css-yellow', 'IQ bal', 'iq_display_IQ_balance_cb'), 33,
					   w3_div('id-iq-fcal-p'), 33,
					   w3_div('id-iq-fcal-m'), 33
					)
				)
			)
		);

	ext_panel_show(controls_html, null, null);
	ext_set_controls_width_height(550, 350);
	iq_display_clk_adj();

	iq_display_canvas = w3_el('id-iq_display-canvas');
	iq_display_canvas.ctx = iq_display_canvas.getContext("2d");
	iq_display_imageData = iq_display_canvas.ctx.createImageData(256, 1);

	ext_send('SET run=1');
	
	// give the PLL time to settle on startup
	setTimeout(function() { iq_display_clear() }, 500);
	setTimeout(function() { iq_display_clear() }, 2000);
}

function iq_display_gain_cb(path, val, complete, first)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Gain '+ ((val == 0)? '(auto-scale)' : val +' dB'), path);
	ext_send('SET gain='+ val);
	iq_display_clear();
}

function iq_display_draw_select_cb(path, idx)
{
	iq.draw = +idx;
	ext_set_controls_width_height(550, (iq.draw == iq.cmd_e.IQ_POINTS)? 350:360);
	ext_send('SET draw='+ iq.draw);
	w3_show_hide('id-iq-size', iq.draw == iq.cmd_e.IQ_POINTS);
	w3_show_hide('id-iq-maxdb', iq.draw == iq.cmd_e.IQ_DENSITY);
	w3_show_hide('id-iq-mindb', iq.draw == iq.cmd_e.IQ_DENSITY);
	iq_display_sched_update();
	iq_display_clear();
}

function iq_display_mode_select_cb(path, idx)
{
	iq.mode = +idx;
	ext_send('SET display_mode='+ iq.mode);
	iq_display_sched_update();
	iq_display_clear();
}

function iq_display_pll_select_cb(path, idx)
{
	var mod = [0, 1, 1, 1, 1,   2,   2]; // PLL mode: 0 -> no PLL, 1 -> single PLL, 2->MSK using two PLLs
	var arg = [0, 1, 2, 4, 8, 100, 200]; // argument: exponent for single PLL, baud for MSK
	iq.pll = +idx;
	console.log('iq_display_pll_select_cb iq.pll='+ iq.pll);
	ext_send('SET pll_mode='+ mod[iq.pll]+' arg='+arg[iq.pll]);
	iq_display_sched_update();
	iq_display_clear();
}

function iq_display_size_cb(path, val, complete, first)
{
   val = +val;
	var size = 1 << val;
	w3_num_cb(path, val);
	w3_set_label('Size '+ size, path);
	ext_send('SET points='+ size);
	iq_display_clear();
}

function iq_display_maxdb_cb(path, val, complete, first)
{
   val = +val;
	w3_set_label('Colormap max '+ val, path);
   iq.maxdb = val;
   //console.log('iq_display_maxdb_cb val='+ val);
	w3_num_cb(path, val);
}

function iq_display_mindb_cb(path, val, complete, first)
{
   val = +val;
	w3_set_label('Colormap min '+ val, path);
   iq.mindb = val;
   //console.log('iq_display_mindb_cb val='+ val);
	w3_num_cb(path, val);
}

function iq_display_pll_bw_cb(path, val, complete, first)
{
   val = +val;
   //console.log('iq_display_pll_bw_cb val='+ val);
	w3_num_cb(path, val);
	ext_send('SET pll_bandwidth='+ val);
}

function iq_display_offset_cb(path, val)
{
	w3_num_cb(path, val);
	ext_send('SET offset='+ val);
}

function iq_display_clear_cb(path, val)
{
	iq_display_clear();
	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

function iq_display_AM_bw_cb(path, val)
{
   var hbw = +val/2;
   ext_set_mode('am');
   ext_set_passband(-hbw, hbw);
	setTimeout(function() { iq_display_clear() }, 500);
}

function iq_display_IQ_balance_cb(path, val)
{
   iq.mode_20kHz = (rx_chans == 3 && wf_chans == 3)? 1:0;
   iq.DC_offset_I = 'DC_offset'+ (iq.mode_20kHz? '_20kHz':'') +'_I';
   iq.DC_offset_Q = 'DC_offset'+ (iq.mode_20kHz? '_20kHz':'') +'_Q';
   console.log('mode_20kHz='+ iq.mode_20kHz +' '+ iq.DC_offset_I +' '+ iq.DC_offset_Q);
   iq.DC_offset_default = [ -0.02, -0.034 ];
   
	admin_pwd_query(function() {
      //console.log('iq_display_IQ_balance_cb');
      
      var s =
         w3_inline('',
            w3_div('',
                w3_text('w3-medium w3-bold w3-text-css-yellow', 'CAUTION') +
               '<br>Only IQ balance under the following conditions:<br>' +
               'PLL mode is set to OFF <br>' +
               'Receive mode is set to AM <br>' +
               'Antenna is disconnected <br>' +
               'Tuned to a frequency with no signals <br>' +
               'Zoom-in and verify no spurs in passband <br>' +
               '<br>Iprev = '+ cfg[iq.DC_offset_I].toFixed(6) +'&nbsp; &nbsp; Qprev = '+ cfg[iq.DC_offset_Q].toFixed(6) +
               '<br>Iincr = '+ (-iq.cmaI).toFixed(6) +'&nbsp; &nbsp; Qincr = '+ (-iq.cmaQ).toFixed(6)
            ),
            w3_button('w3-green w3-margin-left', 'Confirm', 'iq_balance_confirm'),
            w3_button('w3-red w3-margin-left', 'Cancel', 'confirmation_panel_close')
         ) +
         w3_button('w3-css-yellow w3-margin-left w3-tspace-8', 'Reset to default values', 'iq_balance_default')
      
      confirmation_show_content(s, 550, 230);

	});
	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

function iq_balance_default()
{
   console.log('mode_20kHz='+ iq.mode_20kHz +' iq_balance_default='+ iq.DC_offset_default[iq.mode_20kHz]);
   cfg[iq.DC_offset_I] = cfg[iq.DC_offset_Q] = iq.DC_offset_default[iq.mode_20kHz];
   ext_set_cfg_param('cfg.'+ iq.DC_offset_I, cfg[iq.DC_offset_I], false);
   ext_set_cfg_param('cfg.'+ iq.DC_offset_Q, cfg[iq.DC_offset_Q], true);
   confirmation_panel_close();
}

function iq_balance_confirm()
{
   console.log('iq_balance_confirm: PREV I='+ cfg[iq.DC_offset_I].toFixed(6) +' Q='+ cfg[iq.DC_offset_Q].toFixed(6));
   console.log('iq_balance_confirm: INCR ADJ I='+ (-iq.cmaI) +' Q='+ (-iq.cmaQ));
   cfg[iq.DC_offset_I] += -iq.cmaI;
   ext_set_cfg_param('cfg.'+ iq.DC_offset_I, cfg[iq.DC_offset_I], false);
   cfg[iq.DC_offset_Q] += -iq.cmaQ;
   ext_set_cfg_param('cfg.'+ iq.DC_offset_Q, cfg[iq.DC_offset_Q], true);
   console.log('iq_balance_confirm: NEW I='+ cfg[iq.DC_offset_I].toFixed(6) +' Q='+ cfg[iq.DC_offset_Q].toFixed(6));
   confirmation_panel_close();
}

function iq_display_IQ_cal_jog_cb(path, val)
{
	admin_pwd_query(function() {
	   var jog = +val;
      var new_adj = cfg.clk_adj + jog;
      //console.log('jog ADC clock: prev='+ cfg.clk_adj +' jog='+ jog +' new='+ new_adj);
      var adc_clock_ppm_limit = 100;
      var hz_limit = ext_adc_clock_nom_Hz() * adc_clock_ppm_limit / 1e6;

      if (new_adj < -hz_limit || new_adj > hz_limit) {
         console.log('jog ADC clock: ADJ TOO LARGE');
      } else {
         ext_send('SET clk_adj='+ new_adj);
         ext_set_cfg_param('cfg.clk_adj', new_adj, true);
         iq_display_clk_adj();
      }
	});
	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

function iq_display_clk_adj()
{
   w3_innerHTML('id-iq-fcal', 'Total Fcal adjustment: '+ cfg.clk_adj.toString().withSign() +' Hz');
}

function iq_display_blur()
{
	//console.log('### iq_display_blur');
	ext_send('SET run=0');
	kiwi_clearInterval(iq.update_interval);
}

// called to display HTML for configuration parameters in admin interface
function iq_display_config_html()
{
	ext_admin_config(iq_display_ext_name, 'IQ',
		w3_div('id-iq_display w3-text-teal w3-hide',
			'<b>IQ display configuration</b>' +
			'<hr>' +
			''
			/*
			w3_third('', 'w3-container',
				w3_divs('w3-margin-bottom',
					w3_input_get('', 'int1', 'iq_display.int1', 'w3_num_cb'),
					w3_input_get('', 'int2', 'iq_display.int2', 'w3_num_cb')
				), '', ''
			)
			*/
		)
	);
}

function iq_display_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'IQ display help') +
         '<br>URL parameters: <br>' +
         'gain:<i>num</i> &nbsp; density|points &nbsp; IQ|carrier &nbsp; off|on|BPSK|QPSK|8PSK|MSK100|MSK200 <br>' +
         'cmax:<i>num</i> &nbsp; cmin:<i>num</i> &nbsp; pll_bw:<i>num</i> <br>' +
         'Non-numeric values are those appearing in their respective menus. <br>' +
         'Keywords are case-insensitive and can be abbreviated. <br>' +
         'So for example this is valid: <i>&ext=iq,g:75,poi,car,q,pll:8</i> <br>' +
         '';
      confirmation_show_content(s, 610, 175);
   }
   return true;
}
