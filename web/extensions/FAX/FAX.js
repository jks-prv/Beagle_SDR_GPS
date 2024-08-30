// Copyright (c) 2017 John Seamons, ZL4VO/KF6VO

var fax = {
   ext_name: 'FAX',     // NB: must match fax.c:fax_ext.name
   first_time: true,
   stop_start_state: 0,
   url_params: null,
   
   // visible window (scroll-back buffer is larger)
   winH:       400,              
   winSBW:     15,   // scrollbar width
   
   freqs: null,
   menu_s: [ ],
   menus: [ ],
   sfmt: 'w3-text-red',

   phasing:    true,
   autostop:   false,
   debug:      0,
   contrast:   1,
   file:       0,
   ch:         0,
   data_canvas:   0,
   copy_canvas:   0,
   
   freq:          0,
// pbL:         800,    // cf - 2200/2
   pbL:        1300,    // cf - 1200/2
   black:      1500,
   cf:         1900,
   white:      2300,
   pbH:        2500,    // cf + 1200/2
// pbH:        3000,    // cf + 2200/2

   lpm: 120,
   lpm_i: 3,
   lpm_s: [ 60, 90, 100, 120, 180, 240 ],

   last_last: 0
};

function FAX_main()
{
	ext_switch_to_client(fax.ext_name, fax.first_time, fax_recv);		// tell server to use us (again)
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
			if (isDefined(param[1]))
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
            var file = kiwi_url_origin() +'/kiwi.data/fax.ch'+ fax.ch +'_'+ param[1];
            var gif = file +'.gif';
            var thumb = file +'.thumb.gif';
            w3_remove_then_add('id-fax-file-icon', 'fa-refresh fa-spin w3-text-aqua', 'fa-repeat w3-text-pink');
            w3_el('id-fax-file-status').innerHTML = w3_link('', gif, w3_img('', thumb));
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
			   var STOP = (param[1] == 1);
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
         w3_inline('w3-halign-space-between|width:85%/',
            w3_div('w3-medium w3-text-aqua', '<b>FAX decoder</b>'),
            w3_div('w3-text-white',
               "From " + w3_link('', 'https://github.com/seandepagnier/weatherfax_pi', 'weatherfax_pi') +
               " by Sean D'Epagnier &copy; 2015"
            )

         ),
         
         w3_div('id-fax-station w3-margin-T-4 w3-text-css-yellow'),

         w3_inline('id-fax-menus w3-tspace-8/'),

         w3_inline('w3-tspace-8/w3-margin-between-16',
            w3_select(fax.sfmt, '', 'LPM', 'fax.lpm_i', fax.lpm_i, fax.lpm_s, 'fax_lpm_cb'),
            w3_button('w3-padding-smaller', 'Next', 'w3_select_next_prev_cb', { dir:w3_MENU_NEXT, id:'fax.menu', func:'fax_pre_select_cb' }),
            w3_button('w3-padding-smaller', 'Prev', 'w3_select_next_prev_cb', { dir:w3_MENU_PREV, id:'fax.menu', func:'fax_pre_select_cb' }),
            w3_button('id-fax-stop-start w3-padding-smaller', 'Stop', 'fax_stop_start_cb'),
            w3_button('w3-padding-smaller', 'Clear', 'fax_clear_cb'),
            w3_inline('',
               w3_div('',
                  w3_div('fa-stack||title="record"',
                     w3_icon('id-fax-file-icon', 'fa-repeat fa-stack-1x w3-text-pink', 22, '', 'fax_file_cb')
                  )
               ),
               w3_div('id-fax-file-status w3-margin-left')
            )
         ),
         w3_inline('w3-tspace-8',
            w3_checkbox('w3-label-inline w3-label-not-bold/', 'auto align', 'fax.phasing', fax.phasing, 'fax_phasing_cb'),
            w3_div('id-fax-phased w3-margin-left w3-padding-small w3-text-black w3-css-lime w3-hidden', 'aligned'),
            w3_checkbox('w3-margin-left/w3-label-inline w3-label-not-bold/', 'auto stop', 'fax.autostop', fax.autostop, 'fax_autostop_cb'),
            w3_div('id-fax-stopped w3-margin-left w3-padding-small w3-text-black w3-css-orange w3-hidden', 'stopped')
         ),
         w3_div('',
            w3_inline('w3-halign-space-between/',
               w3_link('', 'https://www.weather.gov/media/marine/rfax.pdf', 'FAX transmission schedules')
            ),
            w3_div('', 'Shift-click (PC) or touch (mobile) the image to align.')
            //w3_slider('', 'Contrast', 'fax.contrast', fax.contrast, 1, 255, 1, 'fax_contrast_cb')
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
            ext_help_click();
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
   w3_scrollTop('id-fax-data');
   fax_clear_display();
	w3_attribute(fax.data_canvas, 'title', 'shift-click/touch to align horizontally');
   
   // no dynamic resize used because id-fax-data uses left:0 and the canvas begins at the window left edge

   ext_set_controls_width_height(550, 210);
	fax.saved_setup = ext_save_setup();
	ext_set_mode('usb');
   
	w3_do_when_rendered('id-fax-menus',
	   function() {
         fax.ext_url = kiwi_SSL() +'files.kiwisdr.com/fax/FAX_freq_menus.cjson';
         fax.int_url = kiwi_url_origin() +'/extensions/FAX/FAX_freq_menus.cjson';
         fax.using_default = false;
         fax.double_fault = false;
         if (0 && dbgUs) {
            kiwi_ajax(fax.ext_url +'.xxx', 'fax_get_menus_cb', 0, -500);
         } else {
            kiwi_ajax(fax.ext_url, 'fax_get_menus_cb', 0, 10000);
         }
      }
   );
   // REMINDER: w3_do_when_rendered() returns immediately
	
   fax_start_stop(1);
}

function fax_get_menus_cb(freqs)
{
   fax.freqs = freqs;

   ext_get_menus_cb(fax, freqs,
      'fax_get_menus_cb',     // retry_cb

      function(cb_param) {    // done_cb
         ext_render_menus(fax, 'fax');
   
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
      }, null
   );
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
	   
	   if (option.value == idx && !option.disabled) {
	      var inner = option.innerHTML;
	      console.log('fax_pre_select_cb opt.val='+ option.value +' opt.inner='+ inner);
	      var lsb = inner.includes('lsb');
	      var freq = parseFloat(inner);
         ext_tune(freq + (lsb? 1.9 : -1.9), lsb? 'lsb':'usb', ext_zoom.ABS, 11);
         var lo = lsb? -fax.pbH : fax.pbL;
         var hi = lsb? -fax.pbL : fax.pbH;
         ext_set_passband(lo, hi);
         fax.freq = ext_get_freq()/1e3;
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
   fax_clear_menus(menu_n);
}

function fax_clear_menus(except)
{
   // reset frequency menus
   for (var i = 0; i < fax.n_menu; i++) {
      if (isNoArg(except) || i != except)
         w3_select_value('fax.menu'+ i, -1);
   }
}

function FAX_environment_changed(changed)
{
   //w3_console.log(changed, 'FAX_environment_changed');
   if (!changed.freq && !changed.mode) return;

   // reset all frequency menus when frequency etc. is changed by some other means (direct entry, WF click, etc.)
   // but not for changed.zoom, changed.resize etc.
   var dsp_freq = ext_get_freq()/1e3;
   var mode = ext_get_mode();
   //console.log('FAX ENV fax.freq='+ fax.freq +' dsp_freq='+ dsp_freq +' mode='+ mode);
	var m = mode.substr(0,2);
   if (fax.freq != dsp_freq || (m != 'us' && m != 'ls')) {
      fax_clear_menus();
      w3_el('id-fax-station').innerHTML = '&nbsp;';
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
      w3_remove_then_add('id-fax-file-icon', 'fa-repeat fa-refresh fa-spin w3-text-aqua', 'fa-repeat  w3-text-pink');
      w3_el('id-fax-file-status').innerHTML = '';
   }
}

function fax_file_cb(path, param, first)
{
   if (first) return;
   fax.file ^= 1;
   //console.log('flip fax.file='+ fax.file +' param='+ param);
   var el1 = w3_el('id-fax-file-icon');
   
   if (fax.file) {
	   ext_send('SET fax_file_open');
      w3_spin(el1);
      w3_el('id-fax-file-status').innerHTML =
         w3_div('w3-show-inline-block', 'recording<br>line '+ w3_div('id-fax-line w3-show-inline-block'));
	} else {
	   ext_send('SET fax_file_close');
      w3_remove_then_add(el1, 'fa-repeat w3-text-pink', 'fa-refresh fa-spin w3-text-aqua');
      w3_el('id-fax-file-status').innerHTML = 'processing';
	}
}

function FAX_blur()
{
	ext_send('SET fax_stop');
   ext_set_data_height();
	ext_restore_setup(fax.saved_setup);
}

// called to display HTML for configuration parameters in admin interface
function FAX_config_html()
{
   ext_config_html(fax, 'fax', 'FAX', 'FAX configuration');
}

function FAX_help(show)
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
         'First parameter can be a frequency matching an entry in the station menus. <br>' +
         w3_text('|color:orange', 'align<i>[:0|1]</i> &nbsp; stop<i>[:0|1]</i> &nbsp; lpm:<i>value</i>') +
         '<br> So for example this is valid: <i>ext=fax,3855,align,stop</i> <br>' +
         '';
      confirmation_show_content(s, 620, 325);
   }
   return true;
}
