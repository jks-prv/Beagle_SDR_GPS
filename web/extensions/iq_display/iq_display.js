// Copyright (c) 2016 John Seamons, ZL/KF6VO

var iq_display_ext_name = 'iq_display';		// NB: must match iq_display.c:iq_display_ext.name

var iq = {
   cmd_e: { IQ_POINTS:0, IQ_DENSITY:1, IQ_S4285_P:2, IQ_S4285_D:3, IQ_CLEAR:4 },
   draw: 0,
   mode: 0,
   cmaI: 0,
   cmaQ: 0,
   df: 0,
   pll: 1,
   pll_bw: 10,
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
var iq_display_max = 1;

function iq_display_clear()
{
	var c = iq_display_canvas.ctx;
	c.fillStyle = 'mediumBlue';
	c.fillRect(0, 0, 256, 256);
	c.fillStyle = 'white';
	c.fillRect(0, 128, 256, 1);
	c.fillRect(128, 0, 1, 256);
	ext_send('SET clear');
	
	for (var q=0; q < 256; q++)
		for (var i=0; i < 256; i++)
			iq_display_map[q*256 + i] = 0;
	iq_display_max = 1;
}

var iq_display_imageData;
var iq_display_upd_cnt = 0;

function iq_display_update()
{
	//console.log('iq_display_update '+ iq_display_max);
	var c = iq_display_canvas.ctx;

	if (iq.draw == iq.cmd_e.IQ_DENSITY || iq.draw == iq.cmd_e.IQ_S4285_D) {
		var y=0;
		for (var q=0; q < (256*256); q += 256) {
			for (var i=0; i < 256; i++) {
				var color = Math.round(iq_display_map[q + i] / iq_display_max * 0xff);
				iq_display_imageData.data[i*4+0] = color_map_r[color];
				iq_display_imageData.data[i*4+1] = color_map_g[color];
				iq_display_imageData.data[i*4+2] = color_map_b[color];
				iq_display_imageData.data[i*4+3] = 0xff;
			}
			c.putImageData(iq_display_imageData, 0, y);
			y++;
		}
	}
	
	if (iq_display_upd_cnt == 3) {
      w3_el('iq_display-cma').innerHTML =
         'I='+ iq.cmaI.toExponential(1).withSign() +' Q='+ iq.cmaQ.toExponential(1).withSign() +' df='+ iq.df.toExponential(1).withSign();
      w3_el('iq_display-adc').innerHTML =
         'ADC clock: '+ (ext_adc_clock_Hz()/1e6).toFixed(6) +' MHz';
      w3_el('iq_display-gps').innerHTML =
         'GPS corrections: '+ ext_adc_gps_clock_corr();
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

		if (cmd == iq.cmd_e.IQ_POINTS || cmd == iq.cmd_e.IQ_S4285_P) {
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
		
		if (cmd == iq.cmd_e.IQ_DENSITY || cmd == iq.cmd_e.IQ_S4285_D) {
			//console.log('IQ_DENSITY '+ len);
			var c = iq_display_canvas.ctx;
			var i, q;

			for (var j=1; j < len; j += 2) {
				i = ba[j+0];
				q = ba[j+1];
				var m = iq_display_map[q*256 + i];
				m++;
				if (m > iq_display_max) iq_display_max = m;
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

			default:
				console.log('iq_display_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var iq_display_gain_init = 85;
var iq_display_points_init = 10;    // 1 << value

var iq_display = {
	'gain':iq_display_gain_init, 'draw':0, 'points':iq_display_points_init, 'offset':0
};

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

	// FIXME
	//var draw_s = dbgUs? { 0:'points', 1:'density', 2:'s4285 pts', 3:'s4285 den' } : { 0:'points', 1:'density' };
	var draw_s = { 0:'points', 1:'density' };
	var mode_s = { 0:'IQ', 1:'carrier' };
	var pll_s = { 0:'off', 1:'on', 2:'BPSK', 3:'QPSK', 4:'8PSK' };

   var p = ext_param();
   if (p) {
      p = p.split(',');
      for (var i=0, len = p.length; i < len; i++) {
         var a = p[i];
         //console.log('iq_display: param <'+ a +'>');
         w3_obj_enum_data(draw_s, a.toString(), function(i, key) { iq.draw = key; });
         w3_obj_enum_data(mode_s, a.toString(), function(i, key) { iq.mode = key; });
         w3_obj_enum_data(pll_s, a.toString(), function(i, key) { iq.pll = key; });
         if (a.startsWith('pbw:')) {
            iq.pll_bw = parseInt(a.substring(4));
         }
      }
   }
   //console.log('iq_display: iq.pll='+ iq.pll +' iq.pll_bw='+ iq.pll_bw);
   
   /*
   FIXME
   pgain = (pgain != null)? parseInt(pgain) : -1;
   if (pgain >= 0) {
      iq_display_gain_init = pgain;
	   iq_display_gain_cb('iq_display.gain', iq_display_gain_init);
      w3_set_value('iq_display.gain', iq_display_gain_init);
   }
   */
	
	var controls_html =
		w3_divs('id-iq_display-controls w3-text-white', '',
			w3_half('', '',
				w3_divs('', '',
				   data_html,
			      w3_div('id-iq_display-cma w3-margin-T-8'),
			      w3_div('id-iq_display-adc'),
			      w3_div('id-iq_display-gps')
			   ),
				w3_divs('w3-margin-L-8', 'w3-tspace-8',
					w3_div('w3-medium w3-text-aqua', '<b>IQ display</b>'),
					w3_slider('Gain', 'iq_display.gain', iq_display.gain, 0, 100, 1, 'iq_display_gain_cb'),
					w3_col_percent('', '',
					   w3_select('', 'Draw', '', 'iq_display.draw', iq_display.draw, draw_s, 'iq_display_draw_select_cb'), 36,
					   w3_select('', 'Mode', '', 'iq.mode', iq.mode, mode_s, 'iq_display_mode_select_cb'), 36,
					   w3_select('', 'PLL', '', 'iq.pll', iq.pll, pll_s, 'iq_display_pll_select_cb'), 27
					),
					w3_slider('Points', 'iq_display.points', iq_display.points, 4, 14, 1, 'iq_display_points_cb'),
					w3_slider('PLL b/w', 'iq.pll_bw', iq.pll_bw, 1, 300, 1, 'iq_display_pll_bw_cb'),
					w3_col_percent('', '',
					   //w3_input('Clock offset', 'iq_display.offset', iq_display.offset, 'iq_display_offset_cb', '', 'w3-width-128'),
						w3_button('', 'Clear', 'iq_display_clear_cb'), 50,
						w3_button('', 'AM 40Hz', 'iq_display_AM_40Hz_cb'), 50
					),
					'<hr '+ w3_psa('|margin:16px 0') +'>',
					w3_col_percent('', '',
					   w3_button('w3-css-yellow', 'IQ bal', 'iq_display_IQ_balance_cb'), 33,
					   w3_button('w3-css-yellow|margin-left:12px; padding:6px 10px;', 'Fcal '+ w3_icon('', 'fa-repeat'), 'iq_display_IQ_cal_jog_cb', 1), 33,
					   w3_button('w3-css-yellow|margin-left:12px; padding:6px 10px;', 'Fcal '+ w3_icon('', 'fa-undo'), 'iq_display_IQ_cal_jog_cb', -1), 33
					)
				)
			)
		);

	ext_panel_show(controls_html, null, null);
	ext_set_controls_width_height(540, 340);

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

function iq_display_points_cb(path, val, complete, first)
{
   val = +val;
	var points = 1 << val;
	w3_num_cb(path, val);
	w3_set_label('Points '+ points, path);
	ext_send('SET points='+ points);
	iq_display_clear();
}

function iq_display_pll_bw_cb(path, val, complete, first)
{
   val = +val;
   //console.log('iq_display_pll_bw_cb val='+ val);
	w3_num_cb(path, val);
	w3_set_label('PLL b/w '+ val +' Hz', path);
	ext_send('SET pll_bandwidth='+ val);
	iq_display_clear();
}

var iq_display_update_interval;

function iq_display_draw_select_cb(path, idx)
{
	iq.draw = +idx;
	ext_send('SET draw='+ iq.draw);
	kiwi_clearInterval(iq_display_update_interval);
	iq_display_update_interval = setInterval(iq_display_update, 250);
	iq_display_clear();
}

function iq_display_mode_select_cb(path, idx)
{
	iq.mode = +idx;
	ext_send('SET mode='+ iq.mode);
	kiwi_clearInterval(iq_display_update_interval);
	iq_display_update_interval = setInterval(iq_display_update, 250);
	iq_display_clear();
}

function iq_display_pll_select_cb(path, idx)
{
   var exp = [0, 1, 2, 4, 8];
	iq.pll = +idx;
   console.log('iq_display_pll_select_cb iq.pll='+ iq.pll);
	ext_send('SET exponent='+ exp[iq.pll]);
	kiwi_clearInterval(iq_display_update_interval);
	iq_display_update_interval = setInterval(iq_display_update, 250);
	iq_display_clear();
}

function iq_display_offset_cb(path, val)
{
	w3_num_cb(path, val);
	ext_send('SET offset='+ val);
}

function iq_display_AM_40Hz_cb(path, val)
{
   ext_set_mode('am');
   ext_set_passband(-20, 20);
	setTimeout(function() { iq_display_clear() }, 500);
}

function iq_display_clear_cb(path, val)
{
	iq_display_clear();
	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

function iq_display_IQ_balance_cb(path, val)
{
	admin_pwd_query(function() {
      //console.log('iq_display_IQ_balance_cb');
      
      w3_el('id-confirmation-container').innerHTML =
         w3_col_percent('', 'w3-vcenter',
            w3_div('w3-show-inline-block',
               'CAUTION: Only IQ balance with the<br>' +
               'antenna disconnected. Zoom in and<br>' +
               'tune to a frequency with no signals.<br>' +
               'I = '+ (-iq.cmaI).toFixed(6) +'&nbsp; &nbsp; Q = '+ (-iq.cmaQ).toFixed(6)
            ) +
            w3_button('w3-green|margin-left:16px;', 'Confirm', 'iq_balance_confirm') +
            w3_button('w3-red|margin-left:16px;', 'Cancel', 'confirmation_panel_cancel'),
            90
         );
      
      confirmation_hook_close('id-confirmation', confirmation_panel_cancel);
      
      var el = w3_el('id-confirmation');
      el.style.zIndex = 1020;
      confirmation_panel_resize(525, 85);
      toggle_panel('id-confirmation');

	});
	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

function iq_balance_confirm()
{
   console.log('iq_balance_confirm: INCR ADJ I='+ (-iq.cmaI) +' Q='+ (-iq.cmaQ));
   cfg.DC_offset_I += -iq.cmaI;
   ext_set_cfg_param('cfg.DC_offset_I', cfg.DC_offset_I, false);
   cfg.DC_offset_Q += -iq.cmaQ;
   ext_set_cfg_param('cfg.DC_offset_Q', cfg.DC_offset_Q, true);
   console.log('iq_balance_confirm: NEW I='+ cfg.DC_offset_I.toFixed(6) +' Q='+ cfg.DC_offset_Q.toFixed(6));
   toggle_panel('id-confirmation');
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
      }
	});
	setTimeout(function() {w3_radio_unhighlight(path);}, w3_highlight_time);
}

function iq_display_blur()
{
	//console.log('### iq_display_blur');
	ext_send('SET run=0');
	kiwi_clearInterval(iq_display_update_interval);
}

// called to display HTML for configuration parameters in admin interface
function iq_display_config_html()
{
	ext_admin_config(iq_display_ext_name, 'IQ',
		w3_divs('id-iq_display w3-text-teal w3-hide', '',
			'<b>IQ display configuration</b>' +
			'<hr>' +
			''
			/*
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					w3_input_get_param('int1', 'iq_display.int1', 'w3_num_cb'),
					w3_input_get_param('int2', 'iq_display.int2', 'w3_num_cb')
				), '', ''
			)
			*/
		)
	);
}
