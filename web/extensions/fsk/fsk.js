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

var b_5N15 = 0, b_4_7 = 0;
var e_ITA2 = 0, e_CCIR476 = 0;

var fsk_europe = {
   "DDK2":  [],
   "Weather": [ {f:4583, s:450, r:100, b:b_5N15, e:e_ITA2} ],
   "Navtex": [ {f:490, s:170, r:100, b:b_4_7, e:e_CCIR476}, {f:999, s:170, r:100, b:b_4_7, e:e_CCIR476} ],
   "Navtex2": [ {f:518, s:170, r:100, b:b_4_7, e:e_CCIR476} ],
   "Navtex3": [ {f:4209.5, s:170, r:100, b:b_4_7, e:e_CCIR476} ]
};

var fsk_asia_pac = {
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

var fsk_americas = {
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

var fsk_africa = {
   "W Africa":  [],
   "II": [ 490, 500, 518 ],

   "S Africa":  [],
   "VII": [ 518 ],

   "Indian O":  [],
   "VIII": [ 490, 518 ],

   "M East":  [],
   "IX": [ 490, 518 ]
};

var fsk_HF = {
   "World": [],
   "HF":  [ 4209.5, 4210, 6314, 8416.5, 12579, 16806.5, 19680.5, 22376 ],
   "Others": [],
   "not": [],
   "listed": []
};

var fsk_menus = [ fsk_europe, fsk_asia_pac, fsk_americas, fsk_africa, fsk_HF ];

var fsk = {
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
   
   freq: 0,
   baud: 100,
   shift: 170,
   cf: 1000,
   
   invert: 0,
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

function fsk_controls_setup()
{
	fsk.saved_passband = ext_get_passband();

	fsk_jnx = new fsk_JNX();
	fsk.freq = ext_get_freq()/1e3;
	fsk_setup();
	//w3_console_obj(fsk_jnx, 'fsk_JNX');

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
               w3_div('id-fsk-area w3-text-css-yellow', '&nbsp;'), 50,
               w3_div('', 'dxinfocentre.com schedules: ' +
                  '<a href="http://www.dxinfocentre.com/navtex.htm" target="_blank">MF</a>, ' +
                  '<a href="http://www.dxinfocentre.com/maritimesafetyinfo.htm" target="_blank">HF</a>'), 50
            ),

				w3_col_percent('', '',
               w3_select_hier('w3-text-red', 'Europe', 'select', 'fsk.menu0', fsk.menu0, fsk_europe, 'fsk_pre_select_cb'), 20,
               w3_select_hier('w3-text-red', 'Asia/Pacific', 'select', 'fsk.menu1', fsk.menu1, fsk_asia_pac, 'fsk_pre_select_cb'), 20,
               w3_select_hier('w3-text-red', 'Americas', 'select', 'fsk.menu2', fsk.menu2, fsk_americas, 'fsk_pre_select_cb'), 20,
               w3_select_hier('w3-text-red', 'Africa', 'select', 'fsk.menu3', fsk.menu3, fsk_africa, 'fsk_pre_select_cb'), 20,
               w3_select_hier('w3-text-red', 'HF', 'select', 'fsk.menu4', fsk.menu4, fsk_HF, 'fsk_pre_select_cb'), 20
            ),

            w3_div('w3-valign',
               w3_button('|padding:3px 6px', 'Clear', 'fsk_clear_cb', 0),

               w3_select('w3-margin-L-16|color:red', '', 'shift', 'fsk.shift', W3_SELECT_SHOW_TITLE, fsk_shift_s, 'fsk_shift_cb'),

               w3_select('w3-margin-L-16|color:red', '', 'baud', 'fsk.baud', W3_SELECT_SHOW_TITLE, fsk_baud_s, 'fsk_baud_cb'),

               w3_checkbox('w3-margin-L-16 w3-margin-R-5', '', 'fsk.invert', fsk.invert, 'fsk_invert_cb'),
               w3_text('w3-middle', 'invert'),

               w3_checkbox('w3-margin-L-16 w3-margin-R-5', '', 'fsk.scope', fsk.scope, 'fsk_scope_cb'),
               w3_text('w3-middle', 'scope'),
               w3_button('w3-margin-L-16|padding:3px 6px', 'Single', 'fsk_single_cb', 0)
            )
			)
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('fsk');

	fsk_canvas = w3_el('id-fsk-canvas');
	fsk_canvas.ctx = fsk_canvas.getContext("2d");
	fsk_baud_error_init();

   fsk_resize();
	ext_set_controls_width_height(550, 150);
	
	// receive the network-rate, post-decompression, real-mode samples
	ext_register_audio_data_cb(fsk_audio_data_cb);
}

function fsk_setup()
{
	console.log('FSK SETUP baud='+ fsk.baud +' shift='+ fsk.shift +' cf='+ fsk.cf +' format='+ fsk.format +' encoding='+ fsk.encoding);
	fsk_jnx.setup_values(ext_sample_rate(), fsk.baud, fsk.shift, fsk.cf);
	//console.log('fsk_setup ext_get_freq='+ ext_get_freq()/1e3 +' ext_get_carrier_freq='+ ext_get_carrier_freq()/1e3 +' ext_get_mode='+ ext_get_mode())
   ext_tune(fsk.freq, 'cw', ext_zoom.ABS, 12);
   var pb_half = fsk.shift/2 + 50;
   ext_set_passband(fsk.cf - pb_half, fsk.cf + pb_half);
   ext_tune(fsk.freq, 'cw', ext_zoom.ABS, 12);      // set again to get correct freq given new passband
}

function fsk_shift_cb(path, idx, first)
{
   if (first) return;
   var shift = fsk_shift_s[idx];
   console.log('fsk_shift_cb idx='+ idx +' shift='+ shift);
   if (shift != 'custom')
      fsk.shift = shift;
   fsk_setup();
}

function fsk_baud_cb(path, idx, first)
{
   if (first) return;
   var baud = fsk_baud_s[idx];
   console.log('fsk_baud_cb idx='+ idx +' baud='+ baud);
   if (baud != 'custom')
      fsk.baud = baud;
   fsk_setup();
}

function fsk_pre_select_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
	var menu_n = parseInt(path.split('fsk.menu')[1]);
   console.log('fsk_pre_select_cb path='+ path +' idx='+ idx +' menu_n='+ menu_n);

	w3_select_enum(path, function(option) {
	   //console.log('fsk_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	   
	   if (option.disabled) {
	      fsk.prev_disabled = fsk.disabled;
	      fsk.disabled = option;
	   }
	   
	   if (option.value == idx) {
	      var inner = option.innerHTML;
	      //console.log('fsk_pre_select_cb opt.val='+ option.value +' opt.inner='+ inner +' opt.id='+ option.id);
	      var id = option.id.split('id-')[1];
	      var o = w3_obj_seq_el(fsk_menus[menu_n], id);
	      //w3_console_obj(o[0]);
	      fsk.freq = o[0].f;
	      fsk.shift = o[0].s;
	      fsk.baud = o[0].r;
	      fsk.format = o[0].b;
	      fsk.encoding = o[0].e;
         ext_tune(parseFloat(inner), 'cw', ext_zoom.ABS, 12);
         fsk_setup();
         w3_el('id-fsk-area').innerHTML =
            '<b>Area: '+ fsk.prev_disabled.innerHTML +', '+ fsk.disabled.innerHTML +'</b>';
	   }
	});

   // reset other menus
   for (var i = 0; i < fsk.n_menu; i++) {
      if (i != menu_n)
         w3_select_value('fsk.menu'+ i, -1);
   }
}

function fsk_invert_cb(path, checked, first)
{
   checked = checked? 1:0;
   fsk.invert = checked;
   w3_checkbox_value(path, checked);
   fsk_jnx.invert = checked;
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
   console.log('fsk_single_cb single='+ fsk.single +' run='+ fsk.run);
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
