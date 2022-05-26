// Copyright (c) 2017 John Seamons, ZL/KF6VO

var nt = {
   ext_name: 'NAVTEX',     // NB: must match navtex.c:navtex_ext.name
   first_time: true,
   
   dataH: 300,
   ctrlW: 550,
   ctrlH: 175,

   lhs: 150,
   tw: 1024,
   x: 0,
   last_y: [],
   n_menu:     3,
   menu0:      -1,
   menu1:      -1,
   menu2:      -1,
   
   mode: 0,
   mode_s: [ 'normal', 'DX' ],
   dsc_mode: 0,
   dsc_mode_s: [ 'normal', 'show errs' ],
   MODE_DECODE: 0,
   MODE_DX: 1,
   MODE_SHOW_ERRS: 1,
   show_errs: 0,

   type: 0,
   TYPE_NAVTEX: 0,
   TYPE_DSC: 1,
   decode: 1,
   freq: 0,
   freq_s: '',
   cf: 500,
   shift: 170,
   baud: 100,
   framing: '4/7',
   inverted: 0,
   encoding: 'CCIR476',

   dx: 0,
   dxn: 80,
   fifo: [],

   run: 0,
   single: 0,
   decim: 4,
   sample_count: 0,
   edge: 0,

   log_mins: 0,
   log_interval: null,
   log_txt: '',

   last_last: 0
};

function NAVTEX_main()
{
	ext_switch_to_client(nt.ext_name, nt.first_time, navtex_recv);		// tell server to use us (again)
	if (!nt.first_time)
		navtex_controls_setup();
	nt.first_time = false;
}

function navtex_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('navtex_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('navtex_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('navtex_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
            kiwi_load_js_dir('extensions/FSK/', ['JNX.js', 'BiQuadraticFilter.js', 'CCIR476.js', 'DSC.js'], 'navtex_controls_setup');
				break;

			case "test_done":
				break;

			default:
				console.log('navtex_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function navtex_baud_error_init()
{
   var hh = nt.th/2;
   var cv = navtex_canvas;
   var ct = navtex_canvas.ctx;

   ct.fillStyle = 'white';
   ct.font = '14px Verdana';
   ct.fillText('Baud', nt.lhs/2-15, hh);
   ct.fillText('Error', nt.lhs/2-15, hh+14);
}

function navtex_baud_error(err)
{
   var max = 8;
   if (err > max) err = max;
   if (err < -max) err = -max;
   var h = Math.round(nt.th*0.4/2 * err/max);
   //console.log('err='+ err +' h='+ h);

   var bw = 20;
   var bx = nt.lhs - bw*2;
   var hh = nt.th/2;
   var cv = navtex_canvas;
   var ct = navtex_canvas.ctx;
   
   ct.fillStyle = 'black';
   ct.fillRect(bx,0, bw,nt.th);

   if (h > 0) {
      ct.fillStyle = 'lime';
      ct.fillRect(bx,hh-h, bw,h);
   } else {
      ct.fillStyle = 'red';
      ct.fillRect(bx,hh, bw,-h);
   }
}

// must set "remove_returns" so output lines with \r\n (instead of \n alone) don't produce double spacing
var navtex_console_status_msg_p = { scroll_only_at_bottom: true, process_return_alone: false, remove_returns: true, ncol: 135 };

function navtex_output_char(c)
{
   if (nt.type == nt.TYPE_NAVTEX && nt.dx) {    // ZCZC_STnn
      if (c == '\r' || c == '\n') c = ' ';
      nt.fifo.push(c);
      var s = nt.fifo.join('');
      //console.log('DX ['+ s +']');
      if (!s.startsWith('ZCZC')) {
         while (nt.fifo.length > 9) nt.fifo.shift();
      
         if (nt.dx_tail) {
            if (nt.dx_tail == nt.dxn && c == ' ') return;
            nt.dx_tail--;
            if (nt.dx_tail == 0) c += ' ...\n';
         } else {
            return;
         }
      } else {
         c = '';
         if (nt.dx_tail) c += ' ...\n';
         c += (new Date()).toUTCString().substr(5,20) +' UTC | ';
         if (nt.freq_s != '') c += nt.freq_s + ' | ';
         c += s.substr(0,9) +' | ';
         nt.fifo = [];
         nt.dx_tail = nt.dxn;
      }
   }
   
   navtex_console_status_msg_p.s = encodeURIComponent(c);
   nt.log_txt += kiwi_remove_escape_sequences(kiwi_decodeURIComponent('NAVTEX', c));

   // kiwi_output_msg() does decodeURIComponent()
   kiwi_output_msg('id-navtex-console-msgs', 'id-navtex-console-msg', navtex_console_status_msg_p);
}

function navtex_audio_data_cb(samps, nsamps)
{
   nt.jnx.process_data(samps, nsamps);
}

var navtex_canvas;

// www.dxinfocentre.com/navtex.htm
// www.dxinfocentre.com/maritimesafetyinfo.htm

var navtex_menu_s = [ 'NAVTEX MF', 'NAVTEX HF', 'DSC HF' ];

var navtex_MF = {
   "International": [ 518 ],
   "National": [ 490 ],
   "Other": [ 424, 426, 472, 500 ]
};

var navtex_HF = {
   "Main":  [ 4209.5 ],
   "NBDP": [ 4210, 6314, 8416.5, 12779, 16806.5, 19680.5, 22376 ],
   "Aux":  [ 3165, 4212.5, 4215, 4228, 4241, 4255, 4323, 4560,
         6326, 6328, 6360.5, 6405, 6425, 6448, 6460,
         8417.5, 8424, 8425.5, 8431, 8431.5, 8433, 8451, 8454, 8473, 8580, 8595, 8643,
         12581.5, 12599.5, 12603, 12631, 12637.5, 12654, 12709.9, 12729, 12799.5, 12825, 12877.5, 13050,
         16886, 16898.5, 16927, 16974, 17045, 17175.2, 17155
      ]
};

var DSC_HF = {
   "Distress &amp;_Urgency": [ 2187.5, 4207.5, 6312, 8414.5, 12577, 16804.5 ],
   "Ship/ship_calling": [ 2177 ]
};

function navtex_controls_setup()
{
   nt.th = nt.dataH;
	nt.saved_mode = ext_get_mode();

	nt.jnx = new JNX();
	nt.freq = ext_get_freq()/1e3;
	//w3_console.log(nt.jnx, 'nt.jnx');
	nt.jnx.set_baud_error_cb(navtex_baud_error);
	nt.jnx.set_output_char_cb(navtex_output_char);

   var data_html =
      time_display_html('navtex') +
      
      w3_div('id-navtex-data|width:'+ px(nt.lhs+1024) +'; height:'+ px(nt.dataH) +'; overflow:hidden; position:relative; background-color:black;',
         '<canvas id="id-navtex-canvas" width='+ dq(nt.lhs+1024) +' height='+ dq(nt.dataH) +' style="left:0; position:absolute"></canvas>',
			w3_div('id-navtex-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|left:'+ px(nt.lhs) +'; width:1024px; position:relative; overflow-x:hidden;',
			   '<pre><code id="id-navtex-console-msgs"></code></pre>'
			)
      );

	var controls_html =
		w3_div('id-navtex-controls w3-text-white',
			w3_divs('/w3-tspace-8',
            w3_col_percent('',
				   w3_div('w3-medium w3-text-aqua', '<b><a href="https://en.wikipedia.org/wiki/Navtex" target="_blank">NAVTEX</a> / ' +
				      '<a href="https://en.wikipedia.org/wiki/Digital_selective_calling" target="_blank">DSC</a> decoder</b>'), 45,
					w3_div('', 'From <b><a href="https://arachnoid.com/JNX/index.html" target="_blank">JNX</a></b> by P. Lutus &copy; 2011'), 55
				),
				
            w3_col_percent('',
               w3_div('id-navtex-station w3-text-css-yellow', '&nbsp;'), 50,
               w3_div('', 'NAVTEX schedules: ' +
                  '<a href="http://www.dxinfocentre.com/navtex.htm" target="_blank">MF</a>, ' +
                  '<a href="http://www.dxinfocentre.com/maritimesafetyinfo.htm" target="_blank">HF</a>'), 50
            ),

				w3_inline('/w3-margin-between-16',
               w3_select_hier('w3-text-red', 'NAVTEX MF', 'select', 'nt.menu0', nt.menu0, navtex_MF, 'navtex_pre_select_cb'),
               w3_select_hier('w3-text-red', 'NAVTEX HF', 'select', 'nt.menu1', nt.menu1, navtex_HF, 'navtex_pre_select_cb'),
               w3_select_hier('w3-text-red', 'DSC HF', 'select', 'nt.menu2', nt.menu2, DSC_HF, 'navtex_pre_select_cb')
            ),

            w3_inline('/w3-margin-between-16',
               w3_button('w3-padding-smaller', 'Next', 'w3_select_next_prev_cb', { dir:w3_MENU_NEXT, id:'nt.menu', func:'navtex_pre_select_cb' }),
               w3_button('w3-padding-smaller', 'Prev', 'w3_select_next_prev_cb', { dir:w3_MENU_PREV, id:'nt.menu', func:'navtex_pre_select_cb' }),

               w3_select('w3-text-red', '', 'NAVTEX', 'nt.mode', 0, nt.mode_s, 'navtex_mode_cb'),
               w3_select('w3-text-red', '', 'DSC', 'nt.dsc_mode', 0, nt.dsc_mode_s, 'navtex_dsc_mode_cb'),

               w3_button('w3-padding-smaller w3-css-yellow', 'Clear', 'navtex_clear_button_cb', 0),
               w3_button('id-navtex-log w3-padding-smaller w3-purple', 'Log', 'navtex_log_cb'),

               cfg.navtex.test_file? w3_button('w3-padding-smaller w3-aqua', 'Test', 'navtex_test_cb') : '',

               w3_input('id-navtex-log-mins/w3-label-not-bold/w3-ext-retain-input-focus|padding:0;width:auto|size=4',
                  'log min', 'nt.log_mins', nt.log_mins, 'navtex_log_mins_cb')
            )
			)
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('navtex');
	navtex_canvas = w3_el('id-navtex-canvas');
	navtex_canvas.ctx = navtex_canvas.getContext("2d");

   // URL params that need to be setup after controls instantiated
   var p = nt.url_params = ext_param();
	console.log('NAVTEX url_params='+ p);
	if (p) {
      p = p.split(',');
      p.forEach(function(a, i) {
         //console.log('NAVTEX param2 <'+ a +'>');
         var r;
         if (w3_ext_param('dx', a).match) {
            navtex_mode_cb('id-nt.mode', nt.MODE_DX);
         } else
         if (w3_ext_param('errors', a).match) {
            navtex_dsc_mode_cb('id-nt.dsc_mode', nt.MODE_SHOW_ERRS);
         } else
         if ((r = w3_ext_param('log_time', a)).match) {
            if (isNumber(r.num)) {
               navtex_log_mins_cb('id-nt.log_mins', r.num);
            }
         } else
         if (cfg.navtex.test_file && w3_ext_param('test', a).match) {
            ext_send('SET test');
         } else
         if (w3_ext_param('help', a).match) {
            extint_help_click();
         }
      });
   }

	navtex_setup();
	navtex_baud_error_init();

   ext_set_data_height(nt.dataH);
	ext_set_controls_width_height(nt.ctrlW, nt.ctrlH);
	
	// first URL param can be a match in the preset menus
	if (nt.url_params) {
      var freq = parseFloat(nt.url_params);
      if (!isNaN(freq)) {
         // select matching menu item frequency
         var found = false;
         for (var i = 0; i < nt.n_menu; i++) {
            var menu = 'nt.menu'+ i;
            w3_select_enum(menu, function(option) {
               //console.log('CONSIDER '+ parseFloat(option.innerHTML));
               if (parseFloat(option.innerHTML) == freq) {
                  navtex_pre_select_cb(menu, option.value, false);
                  found = true;
               }
            });
            if (found) break;
         }
      }
   }
	
	// receive the network-rate, post-decompression, real-mode samples
	ext_register_audio_data_cb(navtex_audio_data_cb);
}

function navtex_setup()
{
	nt.freq = ext_get_freq()/1e3;
   console.log('NAVTEX/DSC SETUP freq='+ nt.freq +' cf='+ nt.cf +' shift='+ nt.shift  +' baud='+ nt.baud +' framing='+ nt.framing +' enc='+ nt.encoding +' inv='+ nt.inverted +' show_errs='+ nt.show_errs);
	nt.jnx.setup_values(ext_sample_rate(), nt.cf, nt.shift, nt.baud, nt.framing, nt.inverted, nt.encoding, nt.show_errs);
   navtex_crosshairs(1);
}

function navtex_crosshairs(vis)
{
   var ct = canvas_annotation.ctx;
   ct.clearRect(0,0, window.innerWidth,24);
   
   if (vis && ext_get_zoom() >= 10) {
      var f = ext_get_freq();
      var f_range = get_visible_freq_range();
      //console.log(f_range);
      var Lpx = scale_px_from_freq(f - nt.shift/2, f_range);
      var Rpx = scale_px_from_freq(f + nt.shift/2, f_range);
      //console.log('NAVTEX crosshairs f='+ f +' Lpx='+ Lpx +' Rpx='+ Rpx);
      var d = 3;
      for (var i = 0; i < 6; i++) {
         var y = i*d;
         ct.fillStyle = (i&1)? 'black' : 'white';
         ct.fillRect(Lpx-d,y, d,d);
         ct.fillRect(Rpx-d,y, d,d);
         ct.fillStyle = (i&1)? 'white' : 'black';
         ct.fillRect(Lpx,y, d,d);
         ct.fillRect(Rpx,y, d,d);
      }
   }
}

function navtex_pre_select_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
	var menu_n = parseInt(path.split('nt.menu')[1]);
   //console.log('navtex_pre_select_cb path='+ path +' idx='+ idx +' menu_n='+ menu_n);
   var header;
   var cont = 0;
   var found = false;

	w3_select_enum(path, function(option) {
	   if (found) return;
	   //console.log('navtex_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	   
	   if (option.disabled && option.value != -1) {
	      if (cont)
	         header = header +' '+ option.innerHTML;
	      else
	         header = option.innerHTML;
	      cont = 1;
	   } else {
	      cont = 0;
	   }

	   if (option.value != idx || option.disabled) return;
	   found = true;
      nt.freq_s = option.innerHTML;
      //console.log('navtex_pre_select_cb opt.val='+ option.value +' freq_s='+ nt.freq_s);
      nt.freq = parseFloat(nt.freq_s);
      ext_tune(nt.freq, 'cw', ext_zoom.ABS, 13);
      if (navtex_menu_s[menu_n].includes('DSC')) {
         nt.type = nt.TYPE_DSC;
         nt.framing = '7/3';
         nt.encoding = 'DSC';
         nt.inverted = 1;
      } else {
         nt.type = nt.TYPE_NAVTEX;
         nt.framing = '4/7';
         nt.encoding = 'CCIR476';
         nt.inverted = 0;
      }
      navtex_setup();

      var pb_half = nt.shift/2 + 50;
      //console.log('navtex_pre_select_cb cf='+ nt.cf +' pb_half='+ pb_half);
      ext_set_passband(nt.cf - pb_half, nt.cf + pb_half);
      ext_tune(nt.freq, 'cw', ext_zoom.ABS, 13);      // set again to get correct freq given new passband

      // if called directly instead of from menu callback, select menu item
      w3_select_value(path, idx);

      w3_el('id-navtex-station').innerHTML =
         '<b>'+ navtex_menu_s[menu_n] +', '+ header +'</b>';
	});

   // reset other menus
   navtex_clear_menus(menu_n);
}

function navtex_clear_menus(except)
{
   // reset frequency menus
   for (var i = 0; i < nt.n_menu; i++) {
      if (!isArg(except) || i != except)
         w3_select_value('nt.menu'+ i, -1);
   }
}

function navtex_log_mins_cb(path, val)
{
   nt.log_mins = w3_clamp(+val, 0, 24*60, 0);
   console.log('navtex_log_mins_cb path='+ path +' val='+ val +' log_mins='+ nt.log_mins);
	w3_set_value(path, nt.log_mins);

   kiwi_clearInterval(nt.log_interval);
   if (nt.log_mins != 0) {
      console.log('NAVTEX logging..');
      nt.log_interval = setInterval(function() { navtex_log_cb(); }, nt.log_mins * 60000);
   }
}

function navtex_log_cb()
{
   var ts = kiwi_host() +'_'+ new Date().toISOString().replace(/:/g, '_').replace(/\.[0-9]+Z$/, 'Z') +'_'+ w3_el('id-freq-input').value +'_'+ cur_mode;
   var txt = new Blob([nt.log_txt], { type: 'text/plain' });
   var a = document.createElement('a');
   a.style = 'display: none';
   a.href = window.URL.createObjectURL(txt);
   a.download = 'NAVTEX.'+ ts +'.log.txt';
   document.body.appendChild(a);
   console.log('navtex_log: '+ a.download);
   a.click();
   window.URL.revokeObjectURL(a.href);
   document.body.removeChild(a);
}

function NAVTEX_environment_changed(changed)
{
   //w3_console.log(changed, 'NAVTEX_environment_changed');
   if (!changed.passband_screen_location) return;
   navtex_crosshairs(1);

   if (!changed.freq && !changed.mode) return;

   // reset all frequency menus when frequency etc. is changed by some other means (direct entry, WF click, etc.)
   // but not for changed.zoom, changed.resize etc.
   var dsp_freq = ext_get_freq()/1e3;
   var mode = ext_get_mode();
   //console.log('Navtex ENV nt.freq='+ nt.freq +' dsp_freq='+ dsp_freq);
   if (nt.freq != dsp_freq || mode != 'cw') {
      navtex_clear_menus();
      w3_el('id-navtex-station').innerHTML = '&nbsp;';
   }
}

function navtex_mode_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
   w3_select_value(path, idx);
   nt.decode = nt.dx = 0;

   switch (idx) {
   
   case nt.MODE_DECODE:
   default:
      nt.decode = 1;
      break;

   case nt.MODE_DX:
      nt.dx = 1;
      break;
   }
}

function navtex_dsc_mode_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
   w3_select_value(path, idx);
   nt.decode = nt.show_errs = 0;

   switch (idx) {
   
   case nt.MODE_DECODE:
   default:
      nt.decode = 1;
      break;

   case nt.MODE_SHOW_ERRS:
      nt.show_errs = 1;
      break;
   }
   
   navtex_setup();
}

function navtex_clear_button_cb(path, idx, first)
{
   if (first) return;
   navtex_console_status_msg_p.s = encodeURIComponent('\f');
   kiwi_output_msg('id-navtex-console-msgs', 'id-navtex-console-msg', navtex_console_status_msg_p);
   nt.log_txt = '';
}

function navtex_single_cb(path, idx, first)
{
   if (first) return;
   //console.log('navtex_single_cb single='+ nt.single +' run='+ nt.run);
   if (nt.single) nt.run = 1;
   nt.single ^= 1;
   w3_innerHTML(path, nt.single? 'Run' : 'Single');
}

function navtex_test_cb(path, idx, first)
{
   ext_send('SET test');
}

function NAVTEX_blur()
{
	ext_unregister_audio_data_cb();
	ext_set_mode(nt.saved_mode);
   navtex_crosshairs(0);
   kiwi_clearInterval(nt.log_interval);
}

// called to display HTML for configuration parameters in admin interface
function NAVTEX_config_html()
{
   var s =
      w3_inline_percent('w3-container',
         w3_div('w3-restart',
            w3_input_get('', 'Test filename', 'navtex.test_file', 'w3_string_set_cfg_cb', 'NAVTEX.test.12k.au')
         ), 40
      );

   ext_config_html(nt, 'navtex', 'NAVTEX', 'NAVTEX configuration', s);
}
