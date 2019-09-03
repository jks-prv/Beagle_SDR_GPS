// Copyright (c) 2017 John Seamons, ZL/KF6VO

var fax_ext_name = 'fax';		// NB: must match fax.c:fax_ext.name

var fax = {
   first_time: true,
   stop_start_state: 0,
   url_params: null,
   
   // visible window (scroll-back buffer is larger)
   winH:       400,              
   winSBW:     15,   // scrollbar width
   
   n_menu:     4,
   menu0:      -1,
   menu1:      -1,
   menu2:      -1,
   menu3:      -1,
   phasing:    true,
   autostop:   false,
   debug:      0,
   contrast:   1,
   file:       0,
   ch:         0,
   data_canvas:   0,
   copy_canvas:   0,
   
   pbL:         800,    // cf - 1100
   black:      1500,
   cf:         1900,
   white:      2300,
   pbH:        3000,    // cf + 1100

   lpm: 120,
   lpm_i: 3,
   lpm_s: [ 60, 90, 100, 120, 180, 240 ],
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
var fax_h = 2048;
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
            /*
            if (i == 0) {
               imd.data[i*4+0] = 255;
               imd.data[i*4+1] = 0;
               imd.data[i*4+2] = 0;
            } else
            if (i == 500) {
               imd.data[i*4+0] = 255;
               imd.data[i*4+1] = 255;
               imd.data[i*4+2] = 0;
            } else
            if (i == fax_w-1) {
               imd.data[i*4+0] = 0;
               imd.data[i*4+1] = 255;
               imd.data[i*4+2] = 0;
            } else
            */
            {
               imd.data[i*4+0] = ba[i+1];
               imd.data[i*4+1] = ba[i+1];
               imd.data[i*4+2] = ba[i+1];
            }
            imd.data[i*4+3] = 0xff;
         }
         
         // If updating line is within visible window then adjust scrollbar to track.
         // Otherwise assume user is adjusting scrollbar and we shouldn't disturb.
         // When scroll-back buffer is full it shifts up so this feature doesn't matter at that point.
         var s_topT = w3_el('id-fax-data').scrollTop;
         var s_topB = s_topT + fax.winH;
         if (fax_image_y >= s_topT && fax_image_y <= s_topB) {
            var adj = (fax_image_y >= s_topB);
            //console.log('Y='+ fax_image_y +' st='+ s_topT +'/'+ s_topB + (adj? ' ADJ':' TRACK'));
            if (adj) w3_el('id-fax-data').scrollTop = (fax_image_y+1) - fax.winH;
         } else {
            //console.log('Y='+ fax_image_y +' st='+ s_topT +'/'+ s_topB +' NO-TRACK');
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
            var file = kiwi_url_origin() +'/kiwi.config/fax.ch'+ fax.ch +'_'+ param[1];
            var png = file +'.png';
            var thumb = file +'.thumb.png';
            w3_remove_then_add('id-fax-file-icon1', 'fa-circle-o-notch fa-refresh fa-spin w3-text-aqua', 'fa-circle w3-text-pink');
            w3_add('id-fax-file-icon2', 'w3-hide');
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
	         
			case "fax_phased":
			   w3_visible('id-fax-phased', true);
			   setTimeout(function() { w3_visible('id-fax-phased', false); }, 5000);
	         break;
	         
			case "fax_autostopped":
			   var STOP  = (param[1] == 1);
            w3_button_text('id-fax-stop-start', STOP? 'Start' : 'Stop');
            w3_visible('id-fax-stopped', STOP);
            fax.stop_start_state ^= 1;
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
   "SVJ4 GR":  [ 4482.9, 8106.9 ],
   
   // dk8ok.org/2017/06/11/6-3285-khz-murmansk-fax/
   "Murmansk": [],
   "RBW RU":   [ 5336, '6328.5 lsb', 7908.8, 8444.1, 10130 ]

   // not heard
   // mt-utility.blogspot.com/2009/12/so-who-is-gm-11f.html
   //"Sevastopol": [],
   //"GM-11F UR":   [ 5103, 7090 ]
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
   
   // mt-utility.blogspot.com/2010/02/1800-utc-8658-utc-fax-confirmed-jfx.html
   "JFC/JFW/JFX": [],
   "JP":       [ 6414.5, 8658, 13074, 16907.5, 22559.6 ],
   
   // mt-utility.blogspot.com/2009/11/afternoon-us-kyodo-news-is-jsc-not-jjc.html
   "Kyodo":    [],
   "JJC/JSC JP": [ '4316/60', '8467.5/60', '12745.5/60', '16971/60', '17069.6/60', '22542/60' ],

   // terminated 10/2013
   //"Taipei":   [],
   //"BMF TW":   [ 4616, 8140, 13900, 18560 ],
   
   "Seoul":    [],
   "HLL2 KR":  [ 3585, 5857.5, 7433.5, 9165, 13570 ],
   
   "Shanghai": [],
   "XSG, CN":  [ 4170, 8302, 12382, 16559 ],
   
   "Bangkok":  [],
   "HSW64 TH": [ 7395+1.9 ],
   
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
   
   "Honolulu": [],
   "KVM70 HI": [ 9982.5, 11090, 16135 ],
   
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
   fax_tw = fax_startx + fax_w + fax.winSBW;
   fax.debug= 0;

   // URL params that need to be setup before controls instantiated
	var p = fax.url_params = ext_param();
	if (p) {
      p = p.split(',');
      p.forEach(function(a, i) {
         console.log('FAX param1 <'+ a +'>');
         var a1 = a.split(':');
         a1 = a1[a1.length-1].toLowerCase();
         var r;
         if ((r = w3_ext_param('lpm', a)).match) {
            w3_ext_param_array_match_num(fax.lpm_s, r.num, function(i, n) { fax.lpm_i = i; fax.lpm = n; });
         } else
         if ((r = w3_ext_param('align', a)).match) {
            fax.phasing = (r.num == 0)? 0:1;
         } else
         if ((r = w3_ext_param('stop', a)).match) {
            fax.autostop = (r.num == 0)? 0:1;
         } else
         if ((r = w3_ext_param('debug', a)).match) {
            fax.debug = (r.num == 0)? 0:1;
         }
      });
   }

   var data_html =
      time_display_html('fax') +

      w3_div('id-fax-data|left:0; width:'+ px(fax_tw) +'; height:'+ px(fax.winH) +
         '; background-color:black; position:relative; overflow-y:scroll; overflow-x:hidden',
   		'<canvas id="id-fax-data-canvas" width='+ dq(fax_tw)+' style="left:'+ px(0) +'; position:absolute;"></canvas>',
   		'<canvas id="id-fax-copy-canvas" width='+ dq(fax_tw)+' style="left:'+ px(0) +'; position:absolute;z-index:-1;"></canvas>'
      );

	var controls_html =
		w3_div('id-fax-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
				w3_col_percent('',
               w3_div('w3-medium w3-text-aqua', '<b>HF FAX decoder</b>'), 30,
               w3_div('id-fax-station w3-text-css-yellow'), 60,
               w3_div(), 10
            ),
				w3_inline('w3-halign-space-between/',
               w3_select_hier('w3-text-red', 'Europe', 'select', 'fax.menu0', fax.menu0, fax_europe, 'fax_pre_select_cb'),
               w3_select_hier('w3-text-red', 'Asia/Pacific', 'select', 'fax.menu1', fax.menu1, fax_asia_pac, 'fax_pre_select_cb'),
               w3_select_hier('w3-text-red', 'Americas', 'select', 'fax.menu2', fax.menu2, fax_americas, 'fax_pre_select_cb'),
               w3_select_hier('w3-text-red', 'Africa', 'select', 'fax.menu3', fax.menu3, fax_africa, 'fax_pre_select_cb')
            ),
				w3_inline('/w3-margin-between-16',
               w3_select('|color:red', '', 'LPM', 'fax.lpm_i', fax.lpm_i, fax.lpm_s, 'fax_lpm_cb'),
					w3_button('w3-padding-smaller', 'Next', 'fax_next_prev_cb', 1),
					w3_button('w3-padding-smaller', 'Prev', 'fax_next_prev_cb', -1),
					w3_button('id-fax-stop-start w3-padding-smaller', 'Stop', 'fax_stop_start_cb'),
					w3_button('w3-padding-smaller', 'Clear', 'fax_clear_cb'),
					w3_inline('',
                  w3_div('',
                     w3_div('fa-stack||title="record"',
                        w3_icon('id-fax-file-icon1', 'fa-circle fa-nudge-down fa-stack-2x w3-text-pink', 22, '', 'fax_file_cb'),
                        w3_icon('id-fax-file-icon2', 'fa-stop fa-stack-1x w3-text-pink w3-hide', 10, '', 'fax_file_cb')
                     )
                  ),
                  w3_div('id-fax-file-status w3-margin-left')
               )
            ),
            w3_inline('',
               w3_checkbox('w3-label-inline w3-label-not-bold/', 'auto align', 'fax.phasing', fax.phasing, 'fax_phasing_cb'),
               w3_div('id-fax-phased w3-margin-left w3-padding-small w3-text-black w3-css-lime w3-hidden', 'aligned'),
               w3_checkbox('w3-margin-left/w3-label-inline w3-label-not-bold/', 'auto stop', 'fax.autostop', fax.autostop, 'fax_autostop_cb'),
               w3_div('id-fax-stopped w3-margin-left w3-padding-small w3-text-black w3-css-orange w3-hidden', 'stopped'),
            ),
				w3_div('',
				   w3_inline('w3-halign-space-between/',
                  w3_link('', 'www.nws.noaa.gov/os/marine/rfax.pdf', 'FAX transmission schedules'),
                  w3_div('', 'Please <a href="javascript:sendmail(\'pvsslqwChjtjpgq-`ln\');">report</a> station corrections/updates.')
               ),
               w3_div('', 'Shift-click (PC) or touch (mobile) the image to align.')
               //w3_slider('', 'Contrast', 'fax.contrast', fax.contrast, 1, 255, 1, 'fax_contrast_cb')
            )
			)
      );

	ext_panel_show(controls_html, data_html, null);
   time_display_setup('fax');

   // URL params that need to be setup after controls instantiated
	if (fax.url_params) {
      p = fax.url_params.split(',');
      p.forEach(function(a, i) {
         //console.log('FAX param2 <'+ a +'>');
         if (w3_ext_param('help', a).match) {
            extint_help_click();
         }
      });
   }

	fax.data_canvas = w3_el('id-fax-data-canvas');
	fax.copy_canvas = w3_el('id-fax-copy-canvas');
	fax.data_canvas.ctx = fax.data_canvas.getContext("2d");
	fax.copy_canvas.ctx = fax.copy_canvas.getContext("2d");
	fax.data_canvas.imd = fax.data_canvas.ctx.createImageData(fax_w, 1);
	fax.data_canvas.addEventListener("mousedown", fax_mousedown, false);
	if (kiwi_isMobile())
		fax.data_canvas.addEventListener('touchstart', fax_touchstart, false);

   fax.data_canvas.height = fax_h.toString();
   fax.copy_canvas.height = fax_h.toString();
   ext_set_data_height(fax.winH);
   w3_el('id-fax-data').scrollTop = 0;
   fax_clear_display();
   
   // no dynamic resize used because id-fax-data uses left:0 and the canvas begins at the window left edge

   ext_set_controls_width_height(550, 200);
   
	// first URL param can be a match in the preset menus
   var found = false;
	if (fax.url_params) {
      var freq = parseFloat(fax.url_params);
      if (!isNaN(freq)) {
         // select matching menu item frequency
         for (var i = 0; i < fax.n_menu; i++) {
            var menu = 'fax.menu'+ i;
            w3_select_enum(menu, function(option) {
               //console.log('CONSIDER '+ parseFloat(option.innerHTML));
               if (!found && parseFloat(option.innerHTML) == freq) {
                  fax_pre_select_cb(menu, option.value, false);
                  found = true;
               }
            });
            if (found) break;
         }
      }
   }

   if (!found)
	   ext_set_passband(fax.pbL, fax.pbH);    // FAX passband for usb
	
   fax_start_stop(1);
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
         var lo = lsb? -fax.pbH : fax.pbL;
         var hi = lsb? -fax.pbL : fax.pbH;
         ext_set_passband(lo, hi);
         w3_el('id-fax-station').innerHTML =
            '<b>Station: '+ fax_prev_disabled.innerHTML +', '+ fax_disabled.innerHTML +'</b>';
         var s = inner.split('/');
         var lpm = 120, t;
         if (s.length > 1 && !isNaN(t = parseInt(s[1]))) lpm = t;
         fax_lpm(lpm);
         w3_select_set_if_includes('fax.lpm_i', '\\b'+ lpm +'\\b');
   
         // if called directly instead of from menu callback, select menu item
         w3_select_value(path, idx);
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
	var sx = fax_startx;
	//if (!requireShiftKey) alert('off='+ offset +' sx='+ sx +' fax_tw='+ fax_tw);
	if ((requireShiftKey && !evt.shiftKey) || offset < sx || offset >= (sx + fax_w)) return;
	offset -= sx;
	var norm = (offset / fax_w).toFixed(6);     // normalize
	console.log('FAX offset='+ offset +' shift='+ norm);

   // shift existing part of image
   var data_canvas = fax.data_canvas;
   var copy_canvas = fax.copy_canvas;
   var dct = data_canvas.ctx;
   var cct = copy_canvas.ctx;
   var sx = sx;
   var w = fax_w;
   var h = fax_h;
   var w0 = offset;
   var w1 = w - w0;
   cct.drawImage(data_canvas, sx+w0,0,w1,h, sx,0,w1,h);
   dct.drawImage(data_canvas, sx,0,w0,h, sx+w1,0,w0,h);
   dct.drawImage(copy_canvas, sx,0,w1,h, sx,0,w1,h);

	ext_send('SET fax_shift='+ norm);
}

function fax_start_stop(start)
{
   if (start)
      ext_send('SET fax_start lpm='+ fax.lpm +' phasing='+ (fax.phasing? 1:0) +' autostop='+ (fax.autostop? 1:0) +' debug='+ (fax.debug? 1:0));
   else
      ext_send('SET fax_stop');
}

function fax_stop_start_cb(path, idx, first)
{
	fax_start_stop(fax.stop_start_state);
   fax.stop_start_state ^= 1;
   w3_button_text(path, fax.stop_start_state? 'Start' : 'Stop');
   w3_visible('id-fax-stopped', false);
	//fax_file_cb(0, 0, 0);
}

function fax_phasing_cb(path, checked, first)
{
   if (first) return;
   //console.log('fax_phasing_cb checked='+ checked);
   fax.phasing = checked;

   if (!fax.stop_start_state) {
      //console.log('fax_phasing_cb RESTART phasing='+ fax.phasing);
      fax_start_stop(0);
      fax_start_stop(1);
   }
}

function fax_autostop_cb(path, checked, first)
{
   if (first) return;
   //console.log('fax_autostop_cb checked='+ checked);
   fax.autostop = checked;

   if (!fax.stop_start_state) {
      //console.log('fax_autostop_cb RESTART autostop='+ fax.autostop);
      fax_start_stop(0);
      fax_start_stop(1);
   }
}

function fax_lpm(lpm)
{
   console.log('fax_lpm lpm='+ lpm +' fax.lpm='+ fax.lpm);
   if (isNaN(lpm)) return;
   if (fax.lpm == lpm) return;
   fax.lpm = lpm;
   
   if (!fax.stop_start_state) {
      //console.log('fax_lpm_cb RESTART lpm='+ fax.lpm);
      fax_start_stop(0);
      fax_start_stop(1);
   }
}

function fax_lpm_cb(path, idx, first)
{
   if (first) return;
   idx = +idx;
   lpm = fax.lpm_s[idx];
   //console.log('fax_lpm_cb idx='+ idx +' first='+ first +' lpm='+ lpm);
   fax_lpm(lpm);
}

function fax_clear_cb()
{
   fax_clear_display();
   if (!fax.file) {
      w3_remove_then_add('id-fax-file-icon1', 'fa-circle-o-notch fa-refresh fa-spin w3-text-aqua', 'fa-circle  w3-text-pink');
      w3_add('id-fax-file-icon2', 'w3-hide');
      w3_el('id-fax-file-status').innerHTML = '';
   }
}

function fax_file_cb(path, param, first)
{
   fax.file ^= 1;
   //console.log('flip fax.file='+ fax.file);
   var el1 = w3_el('id-fax-file-icon1');
   var el2 = w3_el('id-fax-file-icon2');
   
   if (fax.file) {
	   ext_send('SET fax_file_open');
      w3_remove_then_add(el1, 'fa-circle', 'fa-circle-o-notch fa-spin');
      w3_remove(el2, 'w3-hide');
      w3_el('id-fax-file-status').innerHTML =
         w3_div('w3-show-inline-block', 'recording<br>line '+ w3_div('id-fax-line w3-show-inline-block'));
	} else {
	   ext_send('SET fax_file_close');
      w3_remove_then_add(el1, 'fa-circle-o-notch w3-text-pink', 'fa-refresh fa-spin w3-text-aqua');
      w3_add(el2, 'w3-hide');
      w3_el('id-fax-file-status').innerHTML = 'processing';
	}
}

function fax_blur()
{
	ext_send('SET fax_stop');
	ext_send('SET fax_close');
   ext_set_data_height();
}

// called to display HTML for configuration parameters in admin interface
function fax_config_html()
{
	ext_admin_config(fax_ext_name, 'fax',
		w3_div('id-fax w3-text-teal w3-hide',
			'<b>FAX configuration</b>' +
			'<hr>'
		)
	);
}

function fax_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'FAX decoder help') +
         '<br>When "auto align" is checked the phasing information sent at the beginning of most <br>' +
         'FAXes is used to horizontally align the image. This may not work if the signal is not strong.<br><br>' +

         'When "auto stop" is checked the start and stop tones sent at the beginning and end of most <br>' +
         'FAXes is used to stop the window scrolling. This may not work if the signal is not strong.<br><br>' +

         'Most stations use an LPM (lines per minute) value of 120. The exception is JJC/JSC which <br>' +
         'mostly uses 60. <br><br>' +
         
         'You can manually align the image by shift-clicking (touch on mobile devices) at the location <br>' +
         'you want moved to the left edge. <br><br>' +

         'URL parameters: <br>' +
         'First parameter can be a frequency matching an entry in station menus. <br>' +
         'align<i>[:0|1]</i> &nbsp; stop<i>[:0|1]</i> &nbsp; lpm:<i>value</i> <br>' +
         'So for example this is valid: <i>&ext=fax,3855,auto,stop</i> <br>' +
         '';
      confirmation_show_content(s, 620, 325);
   }
   return true;
}
