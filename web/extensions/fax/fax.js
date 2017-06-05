// Copyright (c) 2017 John Seamons, ZL/KF6VO

var fax_ext_name = 'fax';		// NB: must match fax.c:fax_ext.name

var fax_first_time = true;

function fax_main()
{
	ext_switch_to_client(fax_ext_name, fax_first_time, fax_recv);		// tell server to use us (again)
	if (!fax_first_time)
		fax_controls_setup();
	fax_first_time = false;
}

var fax_scope_colors = [ 'black', 'red' ];
var fax_image_y = 0;
var fax_w = 1024;
var fax_h = 200;
var fax_startx = 150;

function fax_clear_display()
{
   var ct = fax_data_canvas.ctx;
   ct.fillStyle = 'lightCyan';
   ct.fillRect(0,0, fax_w,fax_h);
   fax_image_y = 0;
}

var fax_cmd = { CLEAR:255, DRAW:254 };

function fax_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
      var ct = fax_data_canvas.ctx;

      if (cmd == fax_cmd.CLEAR) {
         fax_clear_display();
      } else
      
      if (cmd == fax_cmd.DRAW) {
         var imd = fax_data_canvas.imd;
         for (var i = 0; i < fax_w; i++) {
            if (i < 0) {
               imd.data[i*4+0] = 0;
               imd.data[i*4+1] = 0xff;
               imd.data[i*4+2] = 0;
            } else {
               imd.data[i*4+0] = ba[i+1];
               imd.data[i*4+1] = ba[i+1];
               imd.data[i*4+2] = ba[i+1];
            }
            imd.data[i*4+3] = 0xff;
         }
         if (fax_image_y < fax_h) {
            fax_image_y++;
         } else {
            ct.drawImage(fax_data_canvas, 0,1,fax_w,fax_h-1, 0,0,fax_w,fax_h-1);     // scroll up
         }
         ct.putImageData(imd, 0, fax_image_y-1);
      } else
      
      {     // scope
         var ch = cmd;
         ct.fillStyle = fax_scope_colors[ch];
         for (var i = 0; i < fax_w; i++) {
            ct.fillRect(i,ba[i+1], 1,1);
         }
      }

		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (1 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				//console.log('fax_recv: '+ param[0] +'='+ param[1]);
			else
				//console.log('fax_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				fax_controls_setup();
				fax.ch = parseInt(param[1]);
				break;

			case "fax_status":
	         w3_el_id('fax-file-status').innerHTML = decodeURIComponent(param[1]);
				break;

			default:
				console.log('fax_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var fax_europe = {
   "Hamburg":  [],
   "DDH/K DE": [ 3855, 7880, 13882.5 ],
   
   "Northwood": [],
   "GYA UK":   [ 2618.5, 4610, 6834, 8040, 11886.5, 12390, 18261 ],
   
   "Athens":   [],
   "SVJ4 GR":  [ 4481, 8105 ],
   
   "Murmansk": [],
   "RBW RU":   [ 5336, 6445.5, 7908.8, 10130 ]
};

var fax_asia_pac = {
   "Pt. Reyes": [],
   "NMC US":   [ 4346, 8682, 12786, 17151.2, 22527 ],
   
   "Honolulu": [],
   "KVM70 HI": [ 9982.5, 11090, 16135 ],
   
   "Kodiak":   [],
   "NOJ AK":   [ 2054, 4298, 8459, 12410.6 ],
   
   "Wellington": [],
   "ZKLF NZ":  [ 3247.4, 5807, 9459, 13550.5, 16340.1 ],
   
   "Charleville": [],
   "VMC AU":   [ 2628, 5100, 11030, 13920, 20469 ],
   
   "Wiluna":   [],
   "VMW AU":   [ 5755, 7535, 10555, 15615, 18060 ],

   "Tokyo":    [],
   "JMH JP":   [ 3622.5, 7795, 13988.5 ],
   
   "Taipei":   [],
   "BMF TW":   [ 4616, 8140, 13900, 18560 ],
   
   "Seoul":    [],
   "HLL2 KR":  [ 3585, 5857.5, 7433.5, 9165, 13570 ],
   
   "Bangkok":  [],
   "HSW64 TH": [ 7395 ],
   
   "Kyodo":    [],
   "JJC JP":   [ 4316, 8467.5, 12745.5, 16971, 17069.6, 22542 ],

   "Singapore": [],
   "9VF SG":   [ 16035, 17430 ]
};

var fax_americas = {
   "Pt. Reyes": [],
   "NMC US":   [ 4346, 8682, 12786, 17151.2, 22527 ],
   
   "New Orleans": [],
   "NMG US":   [ 4317.9, 8503.9, 12789.9, 17146.4 ],
   
   "Boston":   [],
   "NMF US":   [ 4235, 6340.5, 9110, 12750 ],

   "Iqaluit":  [],
   "VFF CA":   [ 3253, 7710 ],
   
   "Resolute": [],
   "VFR CA":   [ 3253, 7710 ],
   
   "Nova Scotia": [],
   "VCO CA":   [ 4416, 6915.1 ],
   
   "Inuvik":   [],
   "VFA CA":   [ 4292, 8456 ],
   
   "Brazil":   [],
   "PWZ33 BR": [ 12665, 16978 ],
   
   "Chile":    [],
   "CBV/M CL": [ 4228, 4322, 8677, 8696, 17146.4 ]
};

var fax_africa = {
   "S. Africa": [],
   "ZSJ SA":   [ 4014, 7508, 13538, 18238 ]
};

var fax_data_canvas;

var fax = {
   menu0:      -1,
   menu1:      -1,
   menu2:      -1,
   menu3:      -1,
   contrast:   1,
   file:       0,
   ch:         0
};

function fax_controls_setup()
{
   var data_html =
      '<div id="id-integrate-time-display" style="top:50px; background-color:black; position:relative;"></div>' +

      '<div id="id-fax-data" class="scale" style="left:'+ px(fax_startx) +'; width:1024px; background-color:white; position:relative; display:none" title="fax">' +
   		'<canvas id="id-fax-data-canvas" width="1024" style="position:absolute;"></canvas>' +
      '</div>';

	var controls_html =
		w3_divs('id-fax-controls w3-text-white', '',
			w3_divs('w3-container', 'w3-tspace-8',
				w3_col_percent('', '',
               w3_div('w3-medium w3-text-aqua', '<b>HF FAX decoder</b>'), 30,
               w3_div('id-fax-station w3-text-bright-yellow'), 60,
               w3_div(), 10
            ),
				w3_col_percent('', '',
               w3_select_hier('w3-text-red', 'Europe', 'select', 'fax.menu0', fax.menu0, fax_europe, 'fax_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Asia/Pac', 'select', 'fax.menu1', fax.menu1, fax_asia_pac, 'fax_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Americas', 'select', 'fax.menu2', fax.menu2, fax_americas, 'fax_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Africa', 'select', 'fax.menu3', fax.menu3, fax_africa, 'fax_pre_select_cb'), 25
            ),
				w3_divs('w3-vcenter', 'w3-show-inline w3-hspace-16',
					w3_btn('Stop', 'fax_stop_cb'),
					w3_btn('Start', 'fax_start_cb'),
					w3_btn('Clear', 'fax_clear_cb'),
					w3_button('id-fax-file-btn', 'Recording<br>Start', 'fax_file_cb'),
					w3_inline('fax-file-status')
            ),
				w3_divs('', '', 'Demo. Missing features. Click on FAX to align. No shear adjustment yet.')
				//w3_slider('Contrast', 'fax.contrast', fax.contrast, 1, 255, 1, 'fax_contrast_cb')
			)
      );

	ext_panel_show(controls_html, data_html, null);
   time_display_setup('id-integrate-time-display');

	fax_data_canvas = w3_el_id('id-fax-data-canvas');
	fax_data_canvas.ctx = fax_data_canvas.getContext("2d");
	fax_data_canvas.imd = fax_data_canvas.ctx.createImageData(fax_w, 1);
	fax_data_canvas.addEventListener("mousedown", fax_mousedown, false);

   fax_h = 400;
   //fax_h = 200;
   fax_data_canvas.height = fax_h.toString();
   ext_set_data_height(fax_h);
   fax_clear_display();
   ext_set_controls_width_height(600, 200);
   visible_block('id-fax-data', 1);
   
	var freq = parseFloat(ext_param());
	if (!freq) freq = 7880;
	
   /* jksx
   var f =
      //314 - 0.400;      // white
      //314 + 0.400;    // black
      //346 - 0.400;      // white
      //346 + 0.400;    // black
      0;
   ext_tune(f-1.9, 'usb', ext_zoom.ABS, 11);
   */
	
	for (var i = 0; i < 4; i++) {
      var menu = 'fax.menu'+ i;
      w3_select_enum(menu, function(option) {
         if (parseFloat(option.innerHTML) == freq) {
            fax_pre_select_cb(menu, option.value, false);
            w3_select_value(menu, option.value);
         }
      });
   }
	
   //ext_set_passband(1200, 2600);
   ext_set_passband(1400, 2400);

   ext_send('SET fax_start');
}

var fax_disabled, fax_prev_disabled;

function fax_pre_select_cb(path, idx, first)
{
   if (first) return;
	idx = +idx;
	var menu_n = parseInt(path.split('fax.menu')[1]);
   //console.log('fax_pre_select_cb path='+ path +' idx='+ idx +' menu_n='+ menu_n);

	w3_select_enum(path, function(option) {
	   //console.log('fax_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	   
	   if (option.disabled) {
	      fax_prev_disabled = fax_disabled;
	      fax_disabled = option;
	   }
	   
	   if (option.value == idx) {
	      var inner = option.innerHTML;
	      //console.log('fax_pre_select_cb opt.val='+ option.value +' opt.inner='+ inner);
         ext_tune(parseFloat(inner) - 1.9, 'usb', ext_zoom.ABS, 11);
         w3_el_id('id-fax-station').innerHTML =
            '<b>Station: '+ fax_prev_disabled.innerHTML +', '+ fax_disabled.innerHTML +'</b>';
	   }
	});

   // reset other menus
   for (var i = 0; i < 4; i++) {
      if (i != menu_n)
         w3_select_value('fax.menu'+ i, -1);
   }
}

function fax_mousedown(evt)
{
	//event_dump(evt, 'FFT');
	var offset = (evt.clientX? evt.clientX : (evt.offsetX? evt.offsetX : evt.layerX)) - fax_startx;
	if (offset < 0 || offset >= fax_w) return;
	offset = (offset / fax_w).toFixed(6);
	ext_send('SET fax_shift='+ offset);
	console.log('FAX shift='+ offset);
}

function fax_stop_cb()
{
	ext_send('SET fax_stop');
	//fax_file_cb(0, 0, 0);
}

function fax_start_cb()
{
	ext_send('SET fax_start');
}

function fax_clear_cb()
{
   fax_clear_display();
   if (!fax.file)
      w3_el_id('fax-file-status').innerHTML = '';
}

function fax_file_cb(path, first, idx, force)
{
   if (typeof force != 'undefined') {
      fax.file = force;
      //console.log('force fax.file='+ fax.file);
   } else {
      fax.file ^= 1;
      //console.log('flip fax.file='+ fax.file);
   }
   
   if (fax.file) {
	   ext_send('SET fax_file_open');
	   w3_el_id('id-fax-file-btn').innerHTML = 'Recording<br>Stop';
      w3_el_id('fax-file-status').innerHTML =
         'recording <i class="fa fa-refresh fa-spin" style="margin-left:8px; font-size:18px"></i>';
	} else {
	   ext_send('SET fax_file_close');
	   w3_el_id('id-fax-file-btn').innerHTML = 'Recording<br>Start';
	   var png = kiwi_url_origin() +'/kiwi.config/fax.ch'+ fax.ch +'.png';
	   w3_el_id('fax-file-status').innerHTML =
         '<a href='+ dq(png) +' target="_blank">' +
            '<img src='+ dq(png) +' width=64 height=64 />' +
         '</a>';
	}
}

function fax_blur()
{
	ext_send('SET fax_stop');
	visible_block('id-fax-data', 0);
   ext_set_data_height();
}

// called to display HTML for configuration parameters in admin interface
function fax_config_html()
{
	ext_admin_config(fax_ext_name, 'fax',
		w3_divs('id-fax w3-text-teal w3-hide', '',
			'<b>fax configuration</b>' +
			'<hr>' +
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					w3_input_get_param('int1', 'fax.int1', 'w3_num_cb'),
					w3_input_get_param('int2', 'fax.int2', 'w3_num_cb')
				), '', ''
			)
		)
	);
}
