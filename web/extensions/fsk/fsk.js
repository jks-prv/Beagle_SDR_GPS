// Copyright (c) 2017 John Seamons, ZL/KF6VO

var fsk_ext_name = 'fsk';		// NB: must match fsk.c:fsk_ext.name

var fsk_first_time = true;

function fsk_main()
{
	ext_switch_to_client(fsk_ext_name, fsk_first_time, fsk_recv);		// tell server to use us (again)
	if (!fsk_first_time)
		fsk_controls_setup();
	fsk_first_time = false;
}

function fsk_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0] >> 1;
		var ch = ba[0] & 1;
		var o = 1;
		var len = ba.length-1;

		console.log('fsk_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('fsk_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('fsk_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				fsk_controls_setup();
				break;

			default:
				console.log('fsk_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function fsk_scope(dv, edge)
{
   if (!fsk.scope || !fsk.run) return;
   fsk.sample_count++;
   fsk.edge |= edge;
   if ((fsk.sample_count & (fsk.decim-1)) != 0) return;
   
   var cv = fsk_canvas;
   var ct = fsk_canvas.ctx;
   var w = cv.width;
   var h = cv.height;
   var x = fsk.lhs + fsk.x;
   var y;

   if (fsk.edge) {
      ct.fillStyle = 'red';
      ct.fillRect(x,0, 1,h);
   } else {
      y = fsk.last_y[fsk.x];
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
   fsk.last_y[fsk.x] = fsk.edge? -1:y;
   fsk.edge = 0;

   fsk.x++;
   if (fsk.x >= w) {
      fsk.x = 0;
      if (fsk.single) {
         fsk.run = 0;
      }
   }
}

function fsk_baud_error_init()
{
   var hh = fsk.th/2;
   var cv = fsk_canvas;
   var ct = fsk_canvas.ctx;

   ct.fillStyle = 'white';
   ct.font = '14px Verdana';
   ct.fillText('Baud', fsk.lhs/2-15, hh);
   ct.fillText('Error', fsk.lhs/2-15, hh+14);
}

function fsk_baud_error(err)
{
   var max = 8;
   if (err > max) err = max;
   if (err < -max) err = -max;
   var h = Math.round(fsk.th*0.8/2 * err/max);
   //console.log('err='+ err +' h='+ h);

   var bw = 20;
   var bx = fsk.lhs - bw*2;
   var hh = fsk.th/2;
   var cv = fsk_canvas;
   var ct = fsk_canvas.ctx;
   
   ct.fillStyle = 'black';
   ct.fillRect(bx,0, bw,fsk.th);

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
var fsk_console_status_msg_p = { scroll_only_at_bottom: true, process_return_nexttime: false, remove_returns: true, ncol: 135 };

function fsk_output_char(s)
{
   fsk_console_status_msg_p.s = s;
   kiwi_output_msg('id-fsk-console-msgs', 'id-fsk-console-msg', fsk_console_status_msg_p);
}

var fsk_jnx;

function fsk_audio_data_cb(samps, nsamps)
{
   fsk_jnx.process_data(samps, nsamps);
}

var fsk_canvas;

var fsk_europe = {
   '_Weather_': [],
      'DDH/K DE': [
         {f:4583, s:450, b:100, fr:'5N1.5', i:1, e:'ITA2'},
         {f:7646, s:450, b:100, fr:'5N1.5', i:1, e:'ITA2'},
         {f:10100.8, s:450, b:100, fr:'5N1.5', i:1, e:'ITA2'}
      ],
   '_Maritime_': [],
      'SVO Athens': [
         {f:12603.5, s:450, b:100, fr:'5N1.5', i:1, e:'ITA2'}
      ],
      'UDK2 Murmansk': [
         {f:6322.5, s:450, b:100, fr:'5N1.5', i:1, e:'ITA2'}
      ],
   '_Navtex_': [
      {f:490, cf:500, s:170, b:100, fr:'4/7', i:0, e:'CCIR476'},
      {f:518, cf:500, s:170, b:100, fr:'4/7', i:0, e:'CCIR476'},
      {f:4209.5, cf:500, s:170, b:100, fr:'4/7', i:0, e:'CCIR476'}
   ]
};

var fsk_asia_pac = {
   "Maritime":  [],
      'XSQ China': [
         {f:8425.5, s:450, b:100, fr:'5N1.5', i:1, e:'ITA2'},
         {f:12622.5, s:450, b:100, fr:'5N1.5', i:1, e:'ITA2'}
      ]
};

var fsk_americas = {
   "TBD":  []
};

var fsk_africa = {
   "TBD":  []
};

var fsk_menus = [ fsk_europe, fsk_asia_pac, fsk_americas, fsk_africa ];

var fsk = {
   lhs: 150,
   tw: 1024,
   th: 200,
   x: 0,
   last_y: [],
   
   n_menu:     4,
   menu0:      -1,
   menu1:      -1,
   menu2:      -1,
   menu3:      -1,
   prev_header: 0,
   header: 0,
   
   freq: 0,
   cf: 1000,
   shift: 170,
   baud: 100,
   framing: '5N1.5',
   inverted: 1,
   encoding: 'ITA2',
   
   scope: 0,
   run: 0,
   single: 0,
   decim: 8,
   
   sample_count: 0,
   edge: 0,
   
   last_last: 0
};

var fsk_shift_s = [ 85, 170, 425, 450, 850, 'custom' ];
var fsk_baud_s = [ 45.45, 50, 75, 100, 150, 300, 'custom' ];
var fsk_framing_s = [ '5N1.5', '4/7', 'custom' ];
var fsk_encoding_s = [ 'ITA2', 'CCIR476', 'custom' ];

function fsk_controls_setup()
{
	fsk.saved_passband = ext_get_passband();

	fsk_jnx = new JNX();
	fsk.freq = ext_get_freq()/1e3;
	//w3_console_obj(fsk_jnx, 'fsk_JNX');
	fsk_jnx.set_baud_error_cb(fsk_baud_error);
	fsk_jnx.set_scope_cb(fsk_scope);
	fsk_jnx.set_output_char_cb(fsk_output_char);

   var data_html =
      time_display_html('fsk') +
      
      w3_div('id-fsk-data|width:'+ px(fsk.lhs+1024) +'; height:200px; overflow:hidden; position:relative; background-color:black;',
         '<canvas id="id-fsk-canvas" width="'+ (fsk.lhs+1024) +'" height="200" style="left:0; position:absolute"></canvas>',
			w3_div('id-fsk-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|left:'+ px(fsk.lhs) +'; width:1024px; height:200px; position:relative; overflow-x:hidden;',
			   '<pre><code id="id-fsk-console-msgs"></code></pre>'
			)
      );

	var controls_html =
		w3_div('id-fsk-controls w3-text-white',
			w3_divs('w3-container', 'w3-tspace-8',
            w3_col_percent('', '',
				   w3_div('w3-medium w3-text-aqua', '<b><a href="https://en.wikipedia.org/wiki/Navtex" target="_blank">FSK</a> decoder</b>'), 50,
					w3_div('', 'From <b><a href="https://arachnoid.com/JNX/index.html" target="_blank">JNX</a></b> by P. Lutus &copy; 2011'), 50
				),
				
            w3_col_percent('', '',
               w3_div('id-fsk-station w3-text-css-yellow', '&nbsp;'), 50,
               w3_div('', '&nbsp;'), 50
            ),

				w3_col_percent('', '',
               w3_select_hier('w3-text-red', 'Europe', 'select', 'fsk.menu0', fsk.menu0, fsk_europe, 'fsk_pre_select_cb'), 30,
               w3_select_hier('w3-text-red', 'Asia/Pacific', 'select', 'fsk.menu1', fsk.menu1, fsk_asia_pac, 'fsk_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Americas', 'select', 'fsk.menu2', fsk.menu2, fsk_americas, 'fsk_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Africa', 'select', 'fsk.menu3', fsk.menu3, fsk_africa, 'fsk_pre_select_cb'), 20,
            ),

            w3_div('w3-valign',
               w3_select('|color:red', '', 'shift', 'fsk.shift', W3_SELECT_SHOW_TITLE, fsk_shift_s, 'fsk_shift_cb'),

               w3_select('w3-margin-L-16|color:red', '', 'baud', 'fsk.baud', W3_SELECT_SHOW_TITLE, fsk_baud_s, 'fsk_baud_cb'),

               w3_select('w3-margin-L-16|color:red', '', 'framing', 'fsk.framing', W3_SELECT_SHOW_TITLE, fsk_framing_s, 'fsk_framing_cb'),

               w3_select('w3-margin-L-16|color:red', '', 'encoding', 'fsk.encoding', W3_SELECT_SHOW_TITLE, fsk_encoding_s, 'fsk_encoding_cb'),

               w3_checkbox('w3-margin-L-16 w3-margin-R-5', '', 'fsk.inverted', fsk.inverted, 'fsk_inverted_cb'),
               w3_text('w3-middle', 'inverted')
            ),

            w3_div('w3-valign',
               w3_button('|padding:3px 6px', 'Clear', 'fsk_clear_cb', 0),

               w3_checkbox('w3-margin-L-16 w3-margin-R-5', '', 'fsk.scope', fsk.scope, 'fsk_scope_cb'),
               w3_text('w3-middle', 'scope'),
               w3_button('w3-margin-L-16|padding:3px 6px', 'Single', 'fsk_single_cb', 0)
            )
			)
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('fsk');

	fsk_setup();

	fsk_canvas = w3_el('id-fsk-canvas');
	fsk_canvas.ctx = fsk_canvas.getContext("2d");
	fsk_baud_error_init();

   fsk_resize();
	ext_set_controls_width_height(650, 200);
	
	// receive the network-rate, post-decompression, real-mode samples
	ext_register_audio_data_cb(fsk_audio_data_cb);
}

function fsk_setup()
{
   console.log('FSK SETUP freq='+ fsk.freq +' cf='+ fsk.cf);
	console.log('FSK SETUP shift='+ fsk.shift +' baud='+ fsk.baud+' framing='+ fsk.framing +' encoding='+ fsk.encoding);
	fsk_jnx.setup_values(ext_sample_rate(), fsk.cf, fsk.shift, fsk.baud, fsk.framing, fsk.inverted, fsk.encoding);
	//console.log('fsk_setup ext_get_freq='+ ext_get_freq()/1e3 +' ext_get_carrier_freq='+ ext_get_carrier_freq()/1e3 +' ext_get_mode='+ ext_get_mode())
   ext_tune(fsk.freq, 'cw', ext_zoom.ABS, 12);
   var pb_half = fsk.shift/2 + 50;
   ext_set_passband(fsk.cf - pb_half, fsk.cf + pb_half);
   ext_tune(fsk.freq, 'cw', ext_zoom.ABS, 12);      // set again to get correct freq given new passband
   
   // set menus
   w3_select_set_if_includes('fsk.shift', '\\b'+ fsk.shift +'\\b');
   w3_select_set_if_includes('fsk.baud', '\\b'+ fsk.baud +'\\b');
   w3_select_set_if_includes('fsk.framing', '\\b'+ fsk.framing +'\\b');
   w3_select_set_if_includes('fsk.encoding', '\\b'+ fsk.encoding +'\\b');
   w3_checkbox_value('fsk.inverted', fsk.inverted);
}

function fsk_pre_select_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
	var menu_n = parseInt(path.split('fsk.menu')[1]);
   //console.log('fsk_pre_select_cb path='+ path +' idx='+ idx +' menu_n='+ menu_n);

	w3_select_enum(path, function(option) {
	   //console.log('fsk_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	   
	   if (option.disabled) {
	      if (option.innerHTML.startsWith('_')) {
	         fsk.prev_header = option.innerHTML.replace(/_/g, '');
	         fsk.header = '';
	      } else {
	         fsk.header = option.innerHTML;
	      }
	   }
	   
	   if (option.value == idx) {
	      var inner = option.innerHTML;
	      //console.log('fsk_pre_select_cb opt.val='+ option.value +' opt.inner='+ inner +' opt.id='+ option.id);
	      var id = option.id.split('id-')[1];
	      id = id.split('-');
	      var i = id[0];
	      var j = id[1];
	      //console.log('fsk_pre_select_cb i='+ i +' j='+ j);
	      var o = w3_obj_seq_el(fsk_menus[menu_n], i);
	      //w3_console_obj(o);
	      o = w3_obj_seq_el(o, j);
	      //w3_console_obj(o);

	      fsk.freq = o.f;
	      fsk.cf = o.hasOwnProperty('cf')? o.cf : 1000;
	      fsk.shift = o.s;
	      fsk.baud = o.b;
	      fsk.framing = o.fr;
	      fsk.inverted = o.i;
	      fsk.encoding = o.e;

         ext_tune(parseFloat(inner), 'cw', ext_zoom.ABS, 12);
         fsk_setup();
         w3_el('id-fsk-station').innerHTML =
            '<b>Station: '+ fsk.prev_header + ((fsk.header != '')? (', '+ fsk.header) : '') +'</b>';
	   }
	});

   // reset other menus
   for (var i = 0; i < fsk.n_menu; i++) {
      if (i != menu_n)
         w3_select_value('fsk.menu'+ i, -1);
   }
}

function fsk_shift_cb(path, idx, first)
{
   if (first) return;
   var shift = fsk_shift_s[idx];
   //console.log('fsk_shift_cb idx='+ idx +' shift='+ shift);
   if (shift != 'custom')
      fsk.shift = shift;
   fsk_setup();
}

function fsk_baud_cb(path, idx, first)
{
   if (first) return;
   var baud = fsk_baud_s[idx];
   //console.log('fsk_baud_cb idx='+ idx +' baud='+ baud);
   if (baud != 'custom')
      fsk.baud = baud;
   fsk_setup();
}

function fsk_framing_cb(path, idx, first)
{
   if (first) return;
   var framing = fsk_framing_s[idx];
   console.log('fsk_framing_cb idx='+ idx +' framing='+ framing);
   if (framing != 'custom')
      fsk.framing = framing;
   fsk_setup();
}

function fsk_encoding_cb(path, idx, first)
{
   if (first) return;
   var encoding = fsk_encoding_s[idx];
   console.log('fsk_encoding_cb idx='+ idx +' encoding='+ encoding);
   if (encoding != 'custom')
      fsk.encoding = encoding;
   fsk_setup();
}

function fsk_inverted_cb(path, checked, first)
{
   checked = checked? 1:0;
   fsk.inverted = checked;
   w3_checkbox_value(path, checked);
   fsk_setup();
}

function fsk_scope_cb(path, checked, first)
{
   checked = checked? 1:0;
   fsk.scope = checked;
   fsk.run = 1;
   w3_checkbox_value(path, checked);
   w3_show_hide('id-fsk-console-msg', !checked);
}

function fsk_single_cb(path, idx, first)
{
   if (first) return;
   //console.log('fsk_single_cb single='+ fsk.single +' run='+ fsk.run);
   if (fsk.single) fsk.run = 1;
   fsk.single ^= 1;
   w3_innerHTML(path, fsk.single? 'Run' : 'Single');
}

function fsk_clear_cb(path, idx, first)
{
   if (!first)
      fsk_output_char('\f');
}

function fsk_resize()
{
   if (0) {
      var el = w3_el('id-fsk-data');
      var left = (window.innerWidth - sm_tw - time_display_width()) / 2;
      el.style.left = px(left);
	}
}

function fsk_blur()
{
	ext_unregister_audio_data_cb();
   ext_set_passband(fsk.saved_passband.low, fsk.saved_passband.high);
}
