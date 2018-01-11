// Copyright (c) 2016 John Seamons, ZL/KF6VO

var iq_display_ext_name = 'iq_display';		// NB: must match iq_display.c:iq_display_ext.name

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
var iq_display_draw;
var iq_display_cmaI, iq_display_cmaQ;
var iq_display_upd_cnt = 0;

function iq_display_update()
{
	//console.log('iq_display_update '+ iq_display_max);
	var c = iq_display_canvas.ctx;

	if (iq_display_draw == iq_display_cmd_e.IQ_DENSITY || iq_display_draw == iq_display_cmd_e.IQ_S4285_D) {
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
         'CMA I: '+ iq_display_cmaI.toExponential(2) +'&nbsp; &nbsp; CMA Q: '+ iq_display_cmaQ.toExponential(2);
      w3_el('iq_display-adc').innerHTML =
         'ADC clock: '+ (ext_adc_clock_Hz()/1e6).toFixed(6) +' MHz';
      w3_el('iq_display-gps').innerHTML =
         'GPS corrections: '+ ext_adc_gps_clock_corr();
      iq_display_upd_cnt = 0;
   }
   iq_display_upd_cnt++;
}

var iq_display_cmd_e = { IQ_POINTS:0, IQ_DENSITY:1, IQ_S4285_P:2, IQ_S4285_D:3, IQ_CLEAR:4 };

function iq_display_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0] >> 1;
		var ch = ba[0] & 1;
		var len = ba.length-1;

		if (cmd == iq_display_cmd_e.IQ_POINTS || cmd == iq_display_cmd_e.IQ_S4285_P) {
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
		
		if (cmd == iq_display_cmd_e.IQ_DENSITY || cmd == iq_display_cmd_e.IQ_S4285_D) {
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
		
		if (cmd == iq_display_cmd_e.IQ_CLEAR) {	// not currently used
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
				iq_display_cmaI = parseFloat(param[1]);
				break;

			case "cmaQ":
				iq_display_cmaQ = parseFloat(param[1]);
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
	var draw_s = dbgUs? { 0:'points', 1:'density', 2:'s4285 pts', 3:'s4285 den' } : { 0:'points', 1:'density' };

	var controls_html =
		w3_divs('id-iq_display-controls w3-text-white', '',
			w3_half('', '',
				w3_divs('', '',
				   data_html,
			      w3_div('id-iq_display-cma w3-margin-T-8'),
			      w3_div('id-iq_display-adc'),
			      w3_div('id-iq_display-gps')
			   ),
				w3_divs('w3-container', 'w3-tspace-8',
					w3_div('w3-medium w3-text-aqua', '<b>IQ display</b>'),
					w3_slider('Gain', 'iq_display.gain', iq_display.gain, 0, 100, 1, 'iq_display_gain_cb'),
					w3_div('w3-show-inline-block',
					   w3_select('', 'Draw', '', 'iq_display.draw', iq_display.draw, draw_s, 'iq_display_draw_select_cb'),
						w3_button('|margin-left:16px', 'Clear', 'iq_display_clear_cb')
					),
					w3_slider('Points', 'iq_display.points', iq_display.points, 4, 14, 1, 'iq_display_points_cb'),
               w3_div('w3-show-inline-block',
					   //w3_input('Clock offset', 'iq_display.offset', iq_display.offset, 'iq_display_offset_cb', '', 'w3-width-128'),
						w3_button('', 'AM 40Hz', 'iq_display_AM_40Hz_cb')
					),
					'<hr '+ w3_psa('|margin:16px 0') +'>',
               w3_div('w3-show-inline-block',
					   w3_button('w3-css-yellow', 'IQ bal', 'iq_display_IQ_balance_cb'),
					   w3_button('w3-css-yellow|margin-left:12px; padding:6px 10px;', 'Fcal '+ w3_icon('', 'fa-repeat'), 'iq_display_IQ_cal_jog_cb', 1),
					   w3_button('w3-css-yellow|margin-left:12px; padding:6px 10px;', 'Fcal '+ w3_icon('', 'fa-undo'), 'iq_display_IQ_cal_jog_cb', -1)
					)
				)
			)
		);

	ext_panel_show(controls_html, null, null);
	ext_set_controls_width_height(null, 330);

	iq_display_canvas = w3_el('id-iq_display-canvas');
	iq_display_canvas.ctx = iq_display_canvas.getContext("2d");
	iq_display_imageData = iq_display_canvas.ctx.createImageData(256, 1);

   var pgain = ext_param();
   pgain = (pgain != null)? parseInt(pgain) : -1;
   if (pgain >= 0) {
      iq_display_gain_init = pgain;
	   iq_display_gain_cb('iq_display.gain', iq_display_gain_init);
      w3_set_value('iq_display.gain', iq_display_gain_init);
   }
	
	iq_display_clear();
	ext_send('SET run=1');
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

var iq_display_update_interval;

function iq_display_draw_select_cb(path, idx)
{
	iq_display_draw = +idx;
	ext_send('SET draw='+ iq_display_draw);
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
               'I = '+ (-iq_display_cmaI).toFixed(6) +'&nbsp; &nbsp; Q = '+ (-iq_display_cmaQ).toFixed(6)
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
   console.log('iq_balance_confirm: INCR ADJ I='+ (-iq_display_cmaI) +' Q='+ (-iq_display_cmaQ));
   cfg.DC_offset_I += -iq_display_cmaI;
   ext_set_cfg_param('cfg.DC_offset_I', cfg.DC_offset_I, false);
   cfg.DC_offset_Q += -iq_display_cmaQ;
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
