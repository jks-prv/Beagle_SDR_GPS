// Copyright (c) 2017 John Seamons, ZL/KF6VO

var navtex_ext_name = 'navtex';		// NB: must match navtex.c:navtex_ext.name

var navtex_first_time = true;

function navtex_main()
{
	ext_switch_to_client(navtex_ext_name, navtex_first_time, navtex_recv);		// tell server to use us (again)
	if (!navtex_first_time)
		navtex_controls_setup();
	navtex_first_time = false;
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
			if (typeof param[1] != "undefined")
				console.log('navtex_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('navtex_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
            kiwi_load_js_dir('extensions/fsk/', ['JNX.js', 'BiQuadraticFilter.js', 'CCIR476.js', 'FSK_async.js'], 'navtex_controls_setup');
				break;

			default:
				console.log('navtex_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function navtex_scope(dv, edge)
{
   if (!nt.scope || !nt.run) return;
   nt.sample_count++;
   nt.edge |= edge;
   if ((nt.sample_count & (nt.decim-1)) != 0) return;
   
   var cv = navtex_canvas;
   var ct = navtex_canvas.ctx;
   var w = cv.width;
   var h = cv.height;
   var x = nt.lhs + nt.x;
   var y;

   if (nt.edge) {
      ct.fillStyle = 'red';
      ct.fillRect(x,0, 1,h);
   } else {
      y = nt.last_y[nt.x];
      ct.fillStyle = 'black';
      if (y == -1) {
         ct.fillRect(x,0, 1,h);
      } else {
         ct.fillRect(x,y, 1,1);
      }
      ct.fillStyle = 'yellow';
      ct.fillRect(x,h/2, 1,1);
   }

   //dv /= 5;
   if (dv > 1) dv = 1;
   if (dv < -1) dv = -1;
   y = Math.floor(h/2 + dv*h/4);
   ct.fillStyle = 'lime';
   ct.fillRect(x,y, 1,1);
   nt.last_y[nt.x] = nt.edge? -1:y;
   nt.edge = 0;

   nt.x++;
   if (nt.x >= w) {
      nt.x = 0;
      if (nt.single) {
         nt.run = 0;
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
   var h = Math.round(nt.th*0.8/2 * err/max);
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

// must set "remove_returns" since pty output lines are terminated with \r\n instead of \n alone
// otherwise the \r overwrite logic in kiwi_output_msg() will be triggered
var navtex_console_status_msg_p = { scroll_only_at_bottom: true, process_return_nexttime: false, remove_returns: true, ncol: 135 };

function navtex_output_char(c)
{
   if (nt.dx) {    // ZCZC_STnn
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
   kiwi_output_msg('id-navtex-console-msgs', 'id-navtex-console-msg', navtex_console_status_msg_p);
}

var navtex_jnx;

function navtex_audio_data_cb(samps, nsamps)
{
   navtex_jnx.process_data(samps, nsamps);
}

var navtex_canvas;

// www.dxinfocentre.com/navtex.htm
// www.dxinfocentre.com/maritimesafetyinfo.htm

var navtex_europe = {
   "NE Atl":  [],
   "I XIX-S": [ 490, 518 ],
   
   "Med": [],
   "III":   [ 490, 518 ]
};

var navtex_asia_pac = {
   "Asia S":  [],
   "XI-S": [ 490, 518 ],

   "Asia N":  [],
   "XI-N": [ 424, 490, 518 ],

   "Bering":  [],
   "XIII": [ 518, 3165 ],

   "Aus/NZ":  [],
   "X XIV": [ ],
   "None": [ ]
};

var navtex_americas = {
   "NW Atl":  [],
   "IV": [ 472, 490, 518 ],

   "W N.Am":  [],
   "XII-N": [ 426, 518 ],

   "HI, C.Am":  [],
   "XII-S XVI": [ 490, 518 ],

   "W S.Am":  [],
   "XV": [ 490, 518 ],

   "E. S.Am":  [],
   "V VI": [ 490, 518 ]
};

var navtex_africa = {
   "W Africa":  [],
   "II": [ 490, 500, 518 ],

   "S Africa":  [],
   "VII": [ 518 ],

   "Indian O":  [],
   "VIII": [ 490, 518 ],

   "M East":  [],
   "IX": [ 490, 518 ]
};

var navtex_HF = {
   "Navtex":  [],
   "Main":  [ 4209.5 ],
   "Navtex":  [],
   "Aux":  [ 4212.5, 4215, 4228, 4241, 4255, 4323, 4560,
         6326, 6328, 6360.5, 6405, 6425, 6448, 6460,
         8417.5, 8424, 8425.5, 8431, 8431.5, 8433, 8451, 8454, 8473, 8580, 8595, 8643,
         12581.5, 12599.5, 12603, 12631, 12637.5, 12654, 12709.9, 12729, 12799.5, 12825, 12877.5, 13050,
         16886, 16898.5, 16927, 16974, 17045, 17175.2, 17155
      ]
};

var nt = {
   lhs: 150,
   tw: 1024,
   th: 200,
   x: 0,
   last_y: [],
   n_menu:     5,
   menu0:      -1,
   menu1:      -1,
   menu2:      -1,
   menu3:      -1,
   menu4:      -1,
   prev_disabled: 0,
   disabled: 0,

   decode: 1,
   freq_s: '',
   cf: 500,
   shift: 170,
   baud: 100,
   framing: '4/7',
   inverted: 0,
   encoding: 'CCIR476',
   invert: 0,

   dx: 0,
   dxn: 80,
   fifo: [],

   scope: 0,
   run: 0,
   single: 0,
   decim: 4,
   sample_count: 0,
   edge: 0,

   last_last: 0
};

var navtex_mode_s = [ 'decode', 'DX', 'scope' ];

function navtex_controls_setup()
{
	nt.saved_passband = ext_get_passband();

	navtex_jnx = new JNX();
	navtex_jnx.setup_values(ext_sample_rate(), nt.cf, nt.shift, nt.baud, nt.framing, nt.inverted, nt.encoding);
	//w3_console_obj(navtex_jnx, 'JNX');
	navtex_jnx.set_baud_error_cb(navtex_baud_error);
	navtex_jnx.set_output_char_cb(navtex_output_char);

   var data_html =
      time_display_html('navtex') +
      
      w3_div('id-navtex-data|width:'+ px(nt.lhs+1024) +'; height:200px; overflow:hidden; position:relative; background-color:black;',
         '<canvas id="id-navtex-canvas" width="'+ (nt.lhs+1024) +'" height="200" style="left:0; position:absolute"></canvas>',
			w3_div('id-navtex-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|left:'+ px(nt.lhs) +'; width:1024px; height:200px; position:relative; overflow-x:hidden;',
			   '<pre><code id="id-navtex-console-msgs"></code></pre>'
			)
      );

	var controls_html =
		w3_div('id-navtex-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
            w3_col_percent('',
				   w3_div('w3-medium w3-text-aqua', '<b><a href="https://en.wikipedia.org/wiki/Navtex" target="_blank">Navtex</a> decoder</b>'), 45,
					w3_div('', 'From <b><a href="https://arachnoid.com/JNX/index.html" target="_blank">JNX</a></b> by P. Lutus &copy; 2011'), 55
				),
				
            w3_col_percent('',
               w3_div('id-navtex-area w3-text-css-yellow', '&nbsp;'), 45,
               w3_div('', 'dxinfocentre.com schedules: ' +
                  '<a href="http://www.dxinfocentre.com/navtex.htm" target="_blank">MF</a>, ' +
                  '<a href="http://www.dxinfocentre.com/maritimesafetyinfo.htm" target="_blank">HF</a>'), 55
            ),

				w3_col_percent('',
               w3_select_hier('w3-text-red', 'Europe', 'select', 'nt.menu0', nt.menu0, navtex_europe, 'navtex_pre_select_cb'), 20,
               w3_select_hier('w3-text-red', 'Asia/Pacific', 'select', 'nt.menu1', nt.menu1, navtex_asia_pac, 'navtex_pre_select_cb'), 20,
               w3_select_hier('w3-text-red', 'Americas', 'select', 'nt.menu2', nt.menu2, navtex_americas, 'navtex_pre_select_cb'), 20,
               w3_select_hier('w3-text-red', 'Africa', 'select', 'nt.menu3', nt.menu3, navtex_africa, 'navtex_pre_select_cb'), 20,
               w3_select_hier('w3-text-red', 'HF', 'select', 'nt.menu4', nt.menu4, navtex_HF, 'navtex_pre_select_cb'), 20
            ),

            w3_inline('',
					w3_button('w3-padding-smaller', 'Next', 'navtex_next_prev_cb', 1),
					w3_button('w3-margin-left w3-padding-smaller', 'Prev', 'navtex_next_prev_cb', -1),

               w3_select('w3-margin-left|color:red', '', 'mode', 'nt.mode', 0, navtex_mode_s, 'navtex_mode_cb'),

               w3_inline('id-navtex-decode/',
                  w3_button('w3-margin-left w3-padding-smaller', 'Clear', 'navtex_clear_cb', 0),
                  w3_checkbox('w3-margin-left/w3-label-inline w3-label-not-bold/', 'invert', 'nt.invert', nt.invert, 'navtex_invert_cb')
               ),

               w3_inline('id-navtex-scope w3-hide/',
                  w3_button('w3-margin-left w3-padding-smaller', 'Single', 'navtex_single_cb', 0)
               )
            )
			)
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('navtex');

	navtex_canvas = w3_el('id-navtex-canvas');
	navtex_canvas.ctx = navtex_canvas.getContext("2d");
	navtex_baud_error_init();

	ext_set_controls_width_height(550, 150);
	
	var p = ext_param();
	console.log('Navtex p='+ p);
	var freq = parseFloat(p);
	if (freq) {
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
	
	// receive the network-rate, post-decompression, real-mode samples
	ext_register_audio_data_cb(navtex_audio_data_cb);
}

function navtex_pre_select_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
	var menu_n = parseInt(path.split('nt.menu')[1]);
   //console.log('navtex_pre_select_cb path='+ path +' idx='+ idx +' menu_n='+ menu_n);

	w3_select_enum(path, function(option) {
	   //console.log('navtex_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	   
	   if (option.disabled) {
	      nt.prev_disabled = nt.disabled;
	      nt.disabled = option;
	   }
	   
	   if (option.value == idx) {
	      nt.freq_s = option.innerHTML;
	      //console.log('navtex_pre_select_cb opt.val='+ option.value +' freq_s='+ nt.freq_s);
         nt.freq = parseFloat(nt.freq_s);
         ext_tune(nt.freq, 'cw', ext_zoom.ABS, 12);

         var pb_half = nt.shift/2 + 50;
	      //console.log('navtex_pre_select_cb cf='+ nt.cf +' pb_half='+ pb_half);
         ext_set_passband(nt.cf - pb_half, nt.cf + pb_half);
         ext_tune(nt.freq, 'cw', ext_zoom.ABS, 12);      // set again to get correct freq given new passband

         // if called directly instead of from menu callback, select menu item
         w3_select_value(path, idx);

         w3_el('id-navtex-area').innerHTML =
            '<b>Area: '+ nt.prev_disabled.innerHTML +', '+ nt.disabled.innerHTML +'</b>';
	   }
	});

   // reset other menus
   for (var i = 0; i < nt.n_menu; i++) {
      if (i != menu_n)
         w3_select_value('nt.menu'+ i, -1);
   }
}

function navtex_environment_changed(changed)
{
   // reset all frequency menus when frequency etc. is changed by some other means (direct entry, WF click, etc.)
   // but not for changed.zoom, changed.resize etc.
   var dsp_freq = ext_get_freq()/1e3;
   var mode = ext_get_mode();
   //console.log('Navtex ENV nt.freq='+ nt.freq +' dsp_freq='+ dsp_freq);
   if (nt.freq != dsp_freq || mode != 'cw') {
      for (var i = 0; i < nt.n_menu; i++) {
         w3_select_value('nt.menu'+ i, -1);
      }
      nt.menu_sel = '';
      w3_el('id-navtex-area').innerHTML = '&nbsp;';
   }
}

function navtex_next_prev_cb(path, np, first)
{
	np = +np;
	//console.log('navtex_next_prev_cb np='+ np);
	
   // if any menu has a selection value then select next/prev (if any)
   var prev = 0, capture_next = 0, captured_next = 0, captured_prev = 0;
   var menu;
   
   for (var i = 0; i < nt.n_menu; i++) {
      menu = 'nt.menu'+ i;
      var el = w3_el(menu);
      var val = el.value;
      //console.log('menu='+ menu +' value='+ val);
      if (val == -1) continue;
      
      w3_select_enum(menu, function(option) {
	      if (option.disabled) return;
         if (capture_next) {
            captured_next = option.value;
            capture_next = 0;
         }
         if (option.value === val) {
            captured_prev = prev;
            capture_next = 1;
         }
         prev = option.value;
      });
      break;
   }

	//console.log('i='+ i +' captured_prev='+ captured_prev +' captured_next='+ captured_next);
	val = 0;
	if (np == 1 && captured_next) val = captured_next;
	if (np == -1 && captured_prev) val = captured_prev;
	if (val) {
      navtex_pre_select_cb(menu, val, false);
   }
}

function navtex_mode_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
   nt.decode = nt.dx = nt.scope = 0;

   switch (idx) {
   
   case 0:  // decode
   default:
      nt.decode = 1;
      break;

   case 1:  // DX
      nt.dx = 1;
      break;

   case 2:  // scope
      nt.scope = 1;
      nt.run = 1;
      break;
   }

   navtex_jnx.set_scope_cb(nt.scope? navtex_scope : null);

   w3_show_hide_inline('id-navtex-decode', !nt.scope);
   w3_show_hide('id-navtex-console-msg', !nt.scope);
   w3_show_hide_inline('id-navtex-scope', nt.scope);
}

function navtex_clear_cb(path, idx, first)
{
   if (first) return;
   navtex_console_status_msg_p.s = encodeURIComponent('\f');
   kiwi_output_msg('id-navtex-console-msgs', 'id-navtex-console-msg', navtex_console_status_msg_p);
}

function navtex_invert_cb(path, checked, first)
{
   checked = checked? 1:0;
   nt.invert = checked;
   w3_checkbox_set(path, checked);
   navtex_jnx.invert = checked;
}

function navtex_single_cb(path, idx, first)
{
   if (first) return;
   //console.log('navtex_single_cb single='+ nt.single +' run='+ nt.run);
   if (nt.single) nt.run = 1;
   nt.single ^= 1;
   w3_innerHTML(path, nt.single? 'Run' : 'Single');
}

function navtex_blur()
{
	ext_unregister_audio_data_cb();
   ext_set_passband(nt.saved_passband.low, nt.saved_passband.high);
}
