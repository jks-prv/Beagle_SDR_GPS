// Copyright (c) 2017 John Seamons, ZL/KF6VO

var fax_ext_name = 'fax';		// NB: must match fax.c:fax_ext.name

var fax = {
   first_time: true,
   n_menu:     4,
   menu0:      -1,
   menu1:      -1,
   menu2:      -1,
   menu3:      -1,
   contrast:   1,
   file:       0,
   ch:         0,
   data_canvas:   0,
   copy_canvas:   0,
};

function fax_main()
{
	ext_switch_to_client(fax_ext_name, fax.first_time, fax_recv);		// tell server to use us (again)
	if (!fax.first_time)
		fax_controls_setup();
	fax.first_time = false;
}

var fax_scope_colors = [ 'black', 'red' ];
var fax_image_y = 0;
var fax_w = 1024;
var fax_h = 400;
var fax_startx = 150;
var fax_tw;
//var fax_mkr = 32;
var fax_mkr = 0;

function fax_clear_display()
{
   var ct = fax.data_canvas.ctx;
   ct.fillStyle = 'black';
   ct.fillRect(0,0, fax_startx,fax_h);
   ct.fillStyle = 'lightCyan';
   ct.fillRect(fax_startx,0, fax_w,fax_h);
   fax_image_y = 0;
}

var fax_cmd = { CLEAR:255, DRAW:254 };

function fax_recv(data)
{
   var canvas = fax.data_canvas;
   var ct = canvas.ctx;
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];

      if (cmd == fax_cmd.CLEAR) {
         fax_clear_display();
      } else
      
      if (cmd == fax_cmd.DRAW) {
         var imd = canvas.imd;
         for (var i = 0; i < fax_w; i++) {
            imd.data[i*4+0] = ba[i+1];
            imd.data[i*4+1] = ba[i+1];
            imd.data[i*4+2] = ba[i+1];
            imd.data[i*4+3] = 0xff;
         }
         if (fax_image_y < fax_h) {
            fax_image_y++;
         } else {
            var w = fax_w + fax_mkr;
            var x = fax_startx - fax_mkr;
            ct.drawImage(canvas, x,1,w,fax_h-1, x,0,w,fax_h-1);   // scroll up including mkr area
            if (fax_mkr) {
               ct.fillStyle = 'black';
               ct.fillRect(fax_startx-fax_mkr,fax_image_y-1, fax_mkr,1);      // clear mkr
            }
         }
         ct.putImageData(imd, fax_startx, fax_image_y-1);
      } else
      
      {  // scope
         var ch = cmd;
         ct.fillStyle = fax_scope_colors[ch];
         for (var i = 0; i < fax_w; i++) {
            ct.fillRect(fax_startx+i,ba[i+1], 1,1);
         }
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
				console.log('fax_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('fax_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				fax_controls_setup();
				fax.ch = parseInt(param[1]);
				break;

			case "fax_download_avail":
            var file = kiwi_url_origin() +'/kiwi.config/fax.ch'+ fax.ch;
            var png = file +'.png';
            var thumb = file +'.thumb.png';
            w3_el('id-fax-file-status').innerHTML =
               w3_link('', png, '<img src='+ dq(thumb) +' />');
				break;

			case "fax_sps_changed":
			   if (fax_mkr) {
               var ct = canvas.ctx;
               ct.fillStyle = 'red';
               ct.fillRect(fax_startx-fax_mkr,fax_image_y-1, fax_mkr,1);
            }
				break;

			case "fax_record_line":
			   var el = w3_el('id-fax-line');
			   if (el) el.innerHTML = param[1];
	         break;
	         
			default:
				console.log('fax_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

// these are usb passband center freqs that will have 1.9 kHz subtracted to give dial (carrier) freq

var fax_europe = {
   // www.dwd.de/DE/fachnutzer/schifffahrt/funkausstrahlung/sendeplan_fax_042017_sommer.pdf
   "Hamburg":  [],
   "DDH/K DE": [ 3855, 7880, 13882.5, 15988 ],
   
   // www.users.zetnet.co.uk/tempusfugit/marine/fwoc.htm
   "Northwood": [],
   "GYA UK":   [ 2618.5, 4610, 6834, 8040, 11086.5 ],
   
   "Athens":   [],
   "SVJ4 GR":  [ 4482.5, 8106.9 ],
   
   // dk8ok.org/2017/06/11/6-3285-khz-murmansk-fax/
   "Murmansk": [],
   "RBW RU":   [ 5336, '6328.5 lsb', 7908.8, 8444.1, 10130 ]
};

var fax_asia_pac = {
   // www.nws.noaa.gov/om/marine/hfreyes_links.htm
   "Pt. Reyes": [],
   "NMC US":   [ 4346, 8682, 12786, 17151.2, 22527 ],
   
   // www.nws.noaa.gov/om/marine/hfhi_links.htm
   "Honolulu": [],
   "KVM70 HI": [ 9982.5, 11090, 16135 ],
   
   // www.nws.noaa.gov/om/marine/hfak_links.htm
   "Kodiak":   [],
   "NOJ AK":   [ 2054, 4298, 8459, 12412.5 ],
   
   // www.metservice.com/marine/radio/zklf-radiofax-schedule
   "Wellington": [],
   "ZKLF NZ":  [ 3247.4, 5807, 9459, 13550.5, 16340.1 ],
   
   // www.bom.gov.au/marine/radio-sat/vmc-technical-guide.shtml
   "Charleville": [],
   "VMC AU":   [ 2628, 5100, 11030, 13920, 20469 ],
   
   // www.bom.gov.au/marine/radio-sat/vmw-technical-guide.shtml
   "Wiluna":   [],
   "VMW AU":   [ 5755, 7535, 10555, 15615, 18060 ],

   // www.jma-net.go.jp/common/177jmh/JMH-ENG.pdf
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

   "Kodiak":   [],
   "NOJ AK":   [ 2054, 4298, 8459, 12412.5 ],
   
   "Nova Scotia": [],
   "VCO CA":   [ 4416, 6915.1 ],
   
   "Iqaluit":  [],
   "VFF CA":   [ 3253, 7710 ],
   
   "Resolute": [],
   "VFR CA":   [ 3253, 7710 ],
   
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

function fax_controls_setup()
{
   if (kiwi_isMobile()) fax_startx = 0;
   fax_tw = fax_w + fax_startx;

   var data_html =
      time_display_html('fax') +

      w3_div('id-fax-data|left:0; width:'+ px(fax_tw) +'; background-color:black; position:relative;',
   		'<canvas id="id-fax-data-canvas" width='+ dq(fax_tw)+' style="position:absolute;"></canvas>',
   		'<canvas id="id-fax-copy-canvas" width='+ dq(fax_tw)+' style="position:absolute;z-index:-1;"></canvas>'
      );

	var controls_html =
		w3_div('id-fax-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
				w3_col_percent('',
               w3_div('w3-medium w3-text-aqua', '<b>HF FAX decoder</b>'), 30,
               w3_div('id-fax-station w3-text-css-yellow'), 60,
               w3_div(), 10
            ),
				w3_col_percent('',
               w3_select_hier('w3-text-red', 'Europe', 'select', 'fax.menu0', fax.menu0, fax_europe, 'fax_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Asia/Pacific', 'select', 'fax.menu1', fax.menu1, fax_asia_pac, 'fax_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Americas', 'select', 'fax.menu2', fax.menu2, fax_americas, 'fax_pre_select_cb'), 25,
               w3_select_hier('w3-text-red', 'Africa', 'select', 'fax.menu3', fax.menu3, fax_africa, 'fax_pre_select_cb'), 25
            ),
				w3_inline('/w3-margin-between-16',
					w3_button('', 'Next', 'fax_next_prev_cb', 1),
					w3_button('', 'Prev', 'fax_next_prev_cb', -1),
					w3_button('', 'Stop', 'fax_stop_start_cb'),
					w3_button('', 'Clear', 'fax_clear_cb'),
					w3_inline('',
                  w3_icon('id-fax-file-play', 'fa-play-circle', 32, 'lime', 'fax_file_cb'),
                  w3_icon('id-fax-file-stop', 'fa-stop-circle-o', 32, 'red', 'fax_file_cb'),
                  w3_inline('id-fax-file-status w3-margin-left/')
               )
            ),
				w3_div('',
               w3_link('', 'www.nws.noaa.gov/os/marine/rfax.pdf', 'FAX transmission schedules'),
               w3_div('', 'Shift-click (PC) or touch (mobile) the image to align.'),
               w3_div('', 'Please <a href="javascript:sendmail(\'pvsslqwChjtjpgq-`ln\');">report</a> corrections/updates to station frequency menus.')
               //w3_slider('Contrast', 'fax.contrast', fax.contrast, 1, 255, 1, 'fax_contrast_cb')
            )
			)
      );

	ext_panel_show(controls_html, data_html, null);
   w3_show_inline('id-fax-file-play');
   w3_hide('id-fax-file-stop');
   time_display_setup('fax');

	fax.data_canvas = w3_el('id-fax-data-canvas');
	fax.copy_canvas = w3_el('id-fax-copy-canvas');
	fax.data_canvas.ctx = fax.data_canvas.getContext("2d");
	fax.copy_canvas.ctx = fax.copy_canvas.getContext("2d");
	fax.data_canvas.imd = fax.data_canvas.ctx.createImageData(fax_w, 1);
	fax.data_canvas.addEventListener("mousedown", fax_mousedown, false);
	if (kiwi_isMobile())
		fax.data_canvas.addEventListener('touchstart', fax_touchstart, false);

   //fax_h = 50;
   fax.data_canvas.height = fax_h.toString();
   fax.copy_canvas.height = fax_h.toString();
   ext_set_data_height(fax_h);
   fax_clear_display();
   
   // no fax_resize() used because id-fax-data uses left:0 and the canvas begins at the window left edge

   ext_set_controls_width_height(550, 200);
   
	var freq = parseFloat(ext_param());
	
   /*
   var f =
      //314 - 0.400;    // white
      //314 + 0.400;    // black
      //346 - 0.400;    // white
      //346 + 0.400;    // black
      0;
   ext_tune(f-1.9, 'usb', ext_zoom.ABS, 11);
   */
	
	var found = false;
	if (freq) {
	   // select matching menu item frequency
      for (var i = 0; i < fax.n_menu; i++) {
         var menu = 'fax.menu'+ i;
         w3_select_enum(menu, function(option) {
            if (parseFloat(option.innerHTML) == freq) {
               fax_pre_select_cb(menu, option.value, false);
               w3_select_value(menu, option.value);
               found = true;
            }
         });
      }
   }
   
   if (!found)
	   ext_set_passband(1400, 2400);    // FAX passband for usb
	
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
	      console.log('fax_pre_select_cb opt.val='+ option.value +' opt.inner='+ inner);
	      var lsb = inner.includes('lsb');
         ext_tune(parseFloat(inner) + (lsb? 1.9 : -1.9), lsb? 'lsb':'usb', ext_zoom.ABS, 11);
         var lo = lsb? -2400 : 1400;
         var hi = lsb? -1400 : 2400;
         ext_set_passband(lo, hi);
         w3_el('id-fax-station').innerHTML =
            '<b>Station: '+ fax_prev_disabled.innerHTML +', '+ fax_disabled.innerHTML +'</b>';
	   }
	});

   // reset other menus
   for (var i = 0; i < fax.n_menu; i++) {
      if (i != menu_n)
         w3_select_value('fax.menu'+ i, -1);
   }
}

function fax_next_prev_cb(path, np, first)
{
	np = +np;
	//console.log('fax_next_prev_cb np='+ np);
	
   // if any menu has a selection value then select next/prev (if any)
   var prev = 0, capture_next = 0, captured_next = 0, captured_prev = 0;
   var menu;
   
   for (var i = 0; i < fax.n_menu; i++) {
      menu = 'fax.menu'+ i;
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
      fax_pre_select_cb(menu, val, false);
      w3_select_value(menu, val);
   }
}

function fax_mousedown(evt)
{
   fax_shift(evt, true);
}

function fax_touchstart(evt)
{
   fax_shift(evt, false);
}

function fax_shift(evt, requireShiftKey)
{
	//event_dump(evt, 'FFT');
	var offset = (evt.clientX? evt.clientX : (evt.offsetX? evt.offsetX : evt.layerX));
	//if (!requireShiftKey) alert('off='+ offset +' fax_startx='+ fax_startx +' fax_tw='+ fax_tw);
	if ((requireShiftKey && !evt.shiftKey) || offset < fax_startx || offset >= fax_tw) return;
	offset -= fax_startx;
	var norm = (offset / fax_w).toFixed(6);     // normalize
	console.log('FAX shift='+ norm);

   // shift existing part of image
   var data_canvas = fax.data_canvas;
   var copy_canvas = fax.copy_canvas;
   var dct = data_canvas.ctx;
   var cct = copy_canvas.ctx;
   var sx = fax_startx;
   var w = fax_w;
   var h = fax_h;
   var w0 = offset;
   var w1 = w - w0;
   cct.drawImage(data_canvas, sx+w0,0,w1,h, sx,0,w1,h);
   dct.drawImage(data_canvas, sx,0,w0,h, sx+w1,0,w0,h);
   dct.drawImage(copy_canvas, sx,0,w1,h, sx,0,w1,h);

	ext_send('SET fax_shift='+ norm);
}

fax_stop_start_state = 0;

function fax_stop_start_cb(path, idx, first)
{
	ext_send('SET '+ (fax_stop_start_state? 'fax_start' : 'fax_stop'));
   fax_stop_start_state ^= 1;
   w3_button_text(fax_stop_start_state? 'Start' : 'Stop', path);
	//fax_file_cb(0, 0, 0);
}

function fax_clear_cb()
{
   fax_clear_display();
   if (!fax.file)
      w3_el('id-fax-file-status').innerHTML = '';
}

function fax_file_cb(path, param, first)
{
   fax.file ^= 1;
   //console.log('flip fax.file='+ fax.file);
   
   if (fax.file) {
	   ext_send('SET fax_file_open');
	   w3_hide('id-fax-file-play');
	   w3_show_inline('id-fax-file-stop');
      w3_el('id-fax-file-status').innerHTML =
         w3_div('w3-show-inline-block', 'recording<br>line '+ w3_div('id-fax-line w3-show-inline-block')) +
         w3_icon('|margin-left:8px;', 'fa-refresh fa-spin', 20, 'lime');
	} else {
	   ext_send('SET fax_file_close');
	   w3_show_inline('id-fax-file-play');
	   w3_hide('id-fax-file-stop');
      w3_el('id-fax-file-status').innerHTML = 'processing '+
         w3_icon('|margin-left:8px;', 'fa-refresh fa-spin', 20, 'cyan');
	}
}

function fax_blur()
{
	ext_send('SET fax_stop');
   ext_set_data_height();
}

// called to display HTML for configuration parameters in admin interface
function fax_config_html()
{
	ext_admin_config(fax_ext_name, 'fax',
		w3_div('id-fax w3-text-teal w3-hide',
			'<b>FAX configuration</b>' +
			'<hr>' +
			w3_third('', 'w3-container',
				w3_divs('w3-margin-bottom',
					w3_input_get('', 'int1', 'fax.int1', 'w3_num_cb'),
					w3_input_get('', 'int2', 'fax.int2', 'w3_num_cb')
				), '', ''
			)
		)
	);
}
