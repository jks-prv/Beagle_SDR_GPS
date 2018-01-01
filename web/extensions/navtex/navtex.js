// Copyright (c) 2016 John Seamons, ZL/KF6VO

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
		var cmd = ba[0] >> 1;
		var ch = ba[0] & 1;
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
				navtex_controls_setup();
				break;

			default:
				console.log('navtex_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function navtex_plot(dv, edge)
{
   var cv = navtex_canvas;
   var ct = navtex_canvas.ctx;
   var w = cv.width, h = cv.height;
   var y;
   //edge = false;

   if (edge) {
      ct.fillStyle = 'red';
      ct.fillRect(nt.x,0, 1,h);
   } else {
      y = nt.last_y[nt.x];
      ct.fillStyle = 'mediumBlue';
      if (y == -1)
         ct.fillRect(nt.x,0, 1,h);
      else
         ct.fillRect(nt.x,y, 1,1);
   }

   //dv /= 5;
   if (dv > 1) dv = 1;
   if (dv < -1) dv = -1;
   y = Math.floor(h/2 + dv*h/4);
   ct.fillStyle = 'lime';
   ct.fillRect(nt.x,y, 1,1);
   nt.last_y[nt.x] = edge? -1:y;

   nt.x++;
   if (nt.x >= w) nt.x = 0;
}

// must set "remove_returns" since pty output lines are terminated with \r\n instead of \n alone
// otherwise the \r overwrite logic in kiwi_output_msg() will be triggered
var navtex_console_status_msg_p = { scroll_only_at_bottom: true, process_return_nexttime: false, remove_returns: true, ncol: 135 };

function navtex_output_char(s)
{
   navtex_console_status_msg_p.s = s;
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
   "NE Atlantic":  [],
   "I XIX-S": [ 490, 518 ],
   
   "Med": [],
   "III":   [ 490, 518 ]
};

var navtex_asia_pac = {
   "Asia S":  [],
   "XI-S": [ 490, 518 ],

   "Asia N":  [],
   "XI-N": [ 424, 490, 518 ],

   "Bering Sea":  [],
   "XIII": [ 518, 3165 ],

   "Aus/NZ":  [],
   "X XIV": [ ],
   "None": [ ]
};

var navtex_americas = {
   "NW Atlantic":  [],
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

   "Indian Ocn":  [],
   "VIII": [ 490, 518 ],

   "Mdl East":  [],
   "IX": [ 490, 518 ]
};

var nt = {
   x: 0,
   last_y: [],
   n_menu:     4,
   menu0:      -1,
   menu1:      -1,
   menu2:      -1,
   menu3:      -1,
   prev_disabled: 0,
   disabled: 0,
   last_last: 0
};

function navtex_controls_setup()
{
   var data_html =
      time_display_html('navtex') +
      
      w3_div('id-navtex-data|left:150px; width:1024px; height:200px; overflow:hidden; position:relative; background-color:mediumBlue;',
			w3_div('id-navtex-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|height:200px; position:relative;',
			   '<pre><code id="id-navtex-console-msgs"></code></pre>'
			),
         '<canvas id="id-navtex-canvas" width="1024" height="200" style="position:absolute"></canvas>'
      );

	var controls_html =
		w3_div('id-navtex-controls w3-text-white',
			w3_divs('w3-container', 'w3-tspace-8',
            w3_col_percent('', '',
				   w3_div('w3-medium w3-text-aqua', '<b><a href="https://en.wikipedia.org/wiki/Navtex" target="_blank">Navtex</a> decoder</b>'), 50,
					w3_div('', 'From <b><a href="https://arachnoid.com/JNX/index.html" target="_blank">JNX</a></b> by P. Lutus &copy; 2011'), 50
				),
				
            w3_col_percent('', '',
               w3_div('id-navtex-area w3-text-css-yellow', '&nbsp;'), 50,
               w3_div('', 'dxinfocentre.com schedules: ' +
                  '<a href="http://www.dxinfocentre.com/navtex.htm" target="_blank">MF</a>, ' +
                  '<a href="http://www.dxinfocentre.com/maritimesafetyinfo.htm" target="_blank">HF</a>'), 50
            ),

				w3_col_percent('', '',
               w3_select_hier('w3-text-red', 'Europe', 'select', 'nt.menu0', nt.menu0, navtex_europe, 'navtex_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Asia/Pacific', 'select', 'nt.menu1', nt.menu1, navtex_asia_pac, 'navtex_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Americas', 'select', 'nt.menu2', nt.menu2, navtex_americas, 'navtex_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Africa', 'select', 'nt.menu3', nt.menu3, navtex_africa, 'navtex_pre_select_cb'), 25
            )
			)
		);
	
	nt.saved_passband = ext_get_passband();
	nt.passband_changed = false;

	ext_panel_show(controls_html, data_html, null);
	time_display_setup('navtex');

	navtex_canvas = w3_el('id-navtex-canvas');
	navtex_canvas.ctx = navtex_canvas.getContext("2d");

   navtex_resize();
	ext_set_controls_width_height(null, 125);
	
	navtex_jnx = new JNX();
	navtex_jnx.setup_values(ext_sample_rate());
	//w3_console_obj(navtex_jnx, 'JNX');

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
	      var inner = option.innerHTML;
	      console.log('navtex_pre_select_cb opt.val='+ option.value +' opt.inner='+ inner);
         ext_tune(parseFloat(inner), 'cw', ext_zoom.ABS, 11);
         var lo = 500 - 100;
         var hi = 500 + 100;
         ext_set_passband(lo, hi);
         nt.passband_changed = true;
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

function navtex_resize()
{
	var el = w3_el('id-navtex-data');
	var left = (window.innerWidth - sm_tw - time_display_width()) / 2;
	el.style.left = px(left);
}

function navtex_blur()
{
	ext_unregister_audio_data_cb();
	if (nt.passband_changed)
      ext_set_passband(nt.saved_passband.low, nt.saved_passband.high);
}
