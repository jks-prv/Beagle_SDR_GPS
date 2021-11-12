
// Copyright (c) 2021 John Seamons, ZL/KF6VO

var hfdl = {
   ext_name: 'HFDL',    // NB: must match HFDL.cpp:hfdl_ext.name
   first_time: true,
   dataH: 300,
   ctrlW: 600,
   ctrlH: 165,
   freq: 0,
   sfmt: 'w3-text-red w3-ext-retain-input-focus',
   pb: { lo: 300, hi: 2600 },
   
   stations: null,
   url: 'http://kiwisdr.com/hfdl/systable.cjson',
   using_default: false,
   double_fault: false,
   
   ALL: 0,
   DX: 1,
   SQUITTER: 2,
   dsp: 0,
   dsp_s: [ 'all', 'DX', 'squitter' ],
   
   testing: false,
   
   log_mins: 0,
   log_interval: null,
   log_txt: '',
   
   menu_n: 0,
   menu_s: [ ],
   menus: [ ],
   menu_sel: '',

   // must set "remove_returns" so output lines with \r\n (instead of \n alone) don't produce double spacing
   console_status_msg_p: { scroll_only_at_bottom: true, process_return_alone: false, remove_returns: true, ncol: 135 }
};

function HFDL_main()
{
	ext_switch_to_client(hfdl.ext_name, hfdl.first_time, hfdl_recv);		// tell server to use us (again)
	if (!hfdl.first_time)
		hfdl_controls_setup();
	hfdl.first_time = false;
}

function hfdl_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		console.log('hfdl_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('hfdl_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('hfdl_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				hfdl_controls_setup();
				break;

			case "bar_pct":
			   if (hfdl.testing) {
               var pct = w3_clamp(parseInt(param[1]), 0, 100);
               //if (pct > 0 && pct < 3) pct = 3;    // 0% and 1% look weird on bar
               var el = w3_el('id-hfdl-bar');
               if (el) el.style.width = pct +'%';
               //console.log('bar_pct='+ pct);
            }
			   break;

			case "test_start":
			   hfdl_test_cb('', 1);
			   break;

			case "test_done":
            if (hfdl.testing) {
               w3_hide('id-hfdl-bar-container');
               w3_show('id-hfdl-record');
            }
			   break;

			case "chars":
			   var s = param[1];

			   if (hfdl.dsp != hfdl.ALL) {
               var x = kiwi_decodeURIComponent('', param[1]).split('\n');
               x.forEach(function(a, i) { x[i] = a.trim(); });
               //console.log(x);

               switch (hfdl.dsp) {
               
                  case hfdl.DX:
                     s = x[0] +'\n'+ x[1].slice(0,-1) +' | '+ x[2] +' | '+ x[3];
                     if (!x[3].startsWith('Squitter')) {
                        s += ' | '+ x[4];
                     }
                     s += '\n';
                     break;
            
                  case hfdl.SQUITTER:
                     if (!x[3].startsWith('Squitter')) {
                        s = '';
                        break;
                     }
                     s = x[0] +'\n'+ x[2] +'\n';
                     x.forEach(function(a, i) {
                        if (a.startsWith('ID:') || a.startsWith('Frequencies in use:'))
                           s += x[i] +'\n';
                     });
                     break;
               }
            }

				if (s != '') hfdl_decoder_output_chars(s);
				break;

			default:
				console.log('hfdl_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function hfdl_decoder_output_chars(c)
{
   hfdl.console_status_msg_p.s = c;     // NB: already encoded on C-side
   hfdl.log_txt += kiwi_remove_ANSI_escape_sequences(kiwi_decodeURIComponent('HFDL', c));

   // kiwi_output_msg() does decodeURIComponent()
   kiwi_output_msg('id-hfdl-console-msgs', 'id-hfdl-console-msg', hfdl.console_status_msg_p);
}

function hfdl_controls_setup()
{
   var data_html =
      time_display_html('hfdl') +

      w3_div('id-hfdl-data|left:150px; width:1044px; height:'+ px(hfdl.dataH) +'; overflow:hidden; position:relative; background-color:mediumBlue;',
			w3_div('id-hfdl-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|left: 10px; width:1024px; position:absolute; overflow-x:hidden;',
			   '<pre><code id="id-hfdl-console-msgs"></code></pre>'
			)
      );

	var controls_html =
		w3_div('id-hfdl-controls w3-text-white',
         w3_col_percent('w3-tspace-8 w3-valign/',
            w3_div('w3-medium w3-text-aqua', '<b>HFDL decoder</b>'), 25,
            w3_div('', 'From <b><a href="https://github.com/szpajder/dumphfdl" target="_blank">dumphfdl</a></b> by Tomasz Lemiech &copy;2021 GPL-3.0</b>')
         ),

         w3_inline('w3-tspace-4 w3-halign-space-between/',
            w3_div('id-hfdl-msg')
         ),
         
         w3_inline('w3-tspace-8/w3-margin-between-16',
            w3_select(hfdl.sfmt, 'Display', '', 'hfdl.dsp', hfdl.dsp, hfdl.dsp_s, 'hfdl_display_cb'),
            w3_inline('id-hfdl-menus w3-tspace-6 w3-gap-16/')
         ),

         w3_inline('w3-tspace-8 w3-valign/w3-margin-between-12',
            w3_button('w3-padding-smaller', 'Next', 'w3_select_next_prev_cb', { dir:w3_MENU_NEXT, id:'hfdl.menu', isNumeric:true, func:'hfdl_np_pre_select_cb' }),
            w3_button('w3-padding-smaller', 'Prev', 'w3_select_next_prev_cb', { dir:w3_MENU_PREV, id:'hfdl.menu', isNumeric:true, func:'hfdl_np_pre_select_cb' }),
            w3_button('id-hfdl-clear-button w3-padding-smaller w3-css-yellow', 'Clear', 'hfdl_clear_button_cb'),
            w3_button('id-button-test w3-padding-smaller w3-aqua', 'Test', 'hfdl_test_cb', 1),
            w3_div('id-hfdl-bar-container w3-progress-container w3-round-large w3-white w3-hide|width:70px; height:16px',
               w3_div('id-hfdl-bar w3-progressbar w3-round-large w3-light-green|width:0%', '&nbsp;')
            ),
            
            w3_input('id-hfdl-log-mins/w3-label-not-bold/w3-ext-retain-input-focus|padding:0;width:auto|size=4',
               'log min', 'hfdl.log_mins', hfdl.log_mins, 'hfdl_log_mins_cb')
         )
      );

	ext_panel_show(controls_html, data_html, null);
   ext_set_data_height(hfdl.dataH);
	ext_set_controls_width_height(hfdl.ctrlW, hfdl.ctrlH);
	time_display_setup('hfdl');
	hfdl_msg('w3-text-css-yellow', '&nbsp;');
	HFDL_environment_changed( {resize:1, freq:1} );
   hfdl.save_agc_delay = ext_agc_delay(100);
	ext_send('SET start');
	
	hfdl.saved_mode = ext_get_mode();
	ext_set_mode('iq');
   ext_set_passband(hfdl.pb.lo, hfdl.pb.hi);    // optimal passband for HFDL
	
	// our sample file is 12k only
	if (ext_nom_sample_rate() != 12000)
	   w3_add('id-button-test', 'w3-disabled');
	
	w3_do_when_rendered('id-hfdl-menus', function() {
      ext_send('SET reset');
	   hfdl.double_fault = false;
	   if (1 && dbgUs) {
         kiwi_ajax(hfdl.url +'.xxx', 'hfdl_get_systable_done_cb', 0, -500);
	   } else {
         kiwi_ajax(hfdl.url, 'hfdl_get_systable_done_cb', 0, 10000);
      }
      
      //hfdl.watchdog = setInterval(function() { hfdl_watchdog(); }, 1000);
   });
}

function hfdl_watchdog()
{
   //console.log('hfdl tick');
}

function hfdl_get_systable_done_cb(stations)
{
   var url = hfdl.url;
   var fault = false;
   
   if (!stations) {
      console.log('hfdl_get_systable_done_cb: stations='+ stations);
      fault = true;
   } else
   
   if (stations.AJAX_error && stations.AJAX_error == 'timeout') {
      console.log('hfdl_get_systable_done_cb: TIMEOUT');
      hfdl.using_default = true;
      fault = true;
   } else
   if (!isObject(stations)) {
      console.log('hfdl_get_systable_done_cb: not array');
      fault = true;
   }
   
   if (fault) {
      if (hfdl.double_fault) {
         console.log('hfdl_get_systable_done_cb: default station list fetch FAILED');
         console.log(stations);
         return;
      }
      console.log(stations);
      
      // load the default station list from a file embedded with the extension (should always succeed)
      var url = kiwi_url_origin() + '/extensions/HFDL/systable.cjson';
      console.log('hfdl_get_systable_done_cb: using default station list '+ url);
      hfdl.using_default = true;
      hfdl.double_fault = true;
      kiwi_ajax(url, 'hfdl_get_systable_done_cb', 0, /* timeout */ 10000);
      return;
   }
   
   console.log('hfdl_get_systable_done_cb: from '+ url);
   if (isDefined(stations.AJAX_error)) {
      console.log(stations);
      return;
   }
   hfdl.stations = stations;
   hfdl_render_menus();

   hfdl.url_params = ext_param();
   if (dbgUs) console.log('url_params='+ hfdl.url_params);
   
	if (hfdl.url_params) {
      var p = hfdl.url_params.split(',');

	   // first URL param can be a match in the preset menus
      var m_freq = p[0].parseFloatWithUnits('kM', 1e-3);
      var m_str = kiwi_decodeURIComponent('hfdl', p[0]).toLowerCase();
      if (dbgUs) console.log('URL freq='+ m_freq);
      var found_menu_match = false;
      var menu_match_numeric = false;
      var match_menu, match_val;
      var do_test = 0;

      // select matching menu item frequency
      for (var i = 1; i < 2; i++) {    // limit matching to "Bands" menu
         var menu = 'hfdl.menu'+ i;
         var look_for_first_entry = false;
         var match = false;
   
         if (dbgUs) console.log('CONSIDER '+ menu +' -----------------------------------------');
         w3_select_enum(menu, function(option, j) {
            var val = +option.value;
            if (found_menu_match || val == -1) return;
            var menu_f = parseFloat(option.innerHTML);
            var menu_s = option.innerHTML.toLowerCase();
            if (dbgUs) console.log('CONSIDER '+ val +' '+ option.innerHTML);
      
            if (look_for_first_entry) {
               // find first single frequency entry by
               // skipping disabled entries from multi-line net name
               if (option.disabled) return;
               match = true;
               if (isNumber(menu_f))
                  menu_match_numeric = true;
            } else {
               if (isNumber(menu_f)) {
                  if (menu_f == m_freq) {
                     if (dbgUs) console.log('MATCH num: '+ menu_s +'['+ j +']');
                     match = true;
                     menu_match_numeric = true;
                  }
               } else {
                  if (menu_s.includes(m_str)) {
                     look_for_first_entry = true;
                     if (dbgUs) console.log('MATCH str: '+ menu_s +'['+ j +'] BEGIN look_for_first_entry');
                     return;
                  }
               }
            }
      
            if (match) {
               if (dbgUs) console.log('MATCH '+ val +' '+ option.innerHTML);
               // delay call to hfdl_pre_select_cb() until other params processed below
               match_menu = menu;
               match_val = val;
               found_menu_match = true;
            }
         });
      
         if (found_menu_match) {
            p.shift();
            break;
         }
      }

      p.forEach(function(a, i) {
         if (dbgUs) console.log('HFDL param2 <'+ a +'>');
         var r;
         if (w3_ext_param('help', a).match) {
            extint_help_click();
         } else
         if ((r = w3_ext_param('display', a)).match) {
            if (isNumber(r.num)) {
               var idx = w3_clamp(r.num, 0, hfdl.dsp_s.length-1, 0);
               console.log('display '+ r.num +' '+ idx);
               hfdl_display_cb('id-hfdl.dsp', idx);
            }
         } else
         if ((r = w3_ext_param('log_time', a)).match) {
            if (isNumber(r.num)) {
               hfdl_log_mins_cb('id-hfdl.log_mins', r.num);
            }
         } else
         if (w3_ext_param('test', a).match) {
            do_test = 1;
         } else
            console.log('HFDL: unknown URL param "'+ a +'"');
      });
      
      if (found_menu_match) {
         hfdl_pre_select_cb(match_menu, match_val, false);
      }
   }

   if (do_test) hfdl_test_cb('', 1);
   if (dbgUs) console.log(hfdl.stations);
}

function hfdl_render_menus()
{
   var i;
   var bf = [ 4, 5, 6, 8, 10, 11, 13, 15, 17, 21 ];
   var bf_s = [ '3', '4.6', '5.5', '6.5', '8.9', '10', '11.3', '13.3', '15', '18', '22' ];
   var b = [];
   for (i = 0; i < bf.length; i++) b[i] = [];
   
   var add_bands_menu = function(f) {
      var f_n = parseInt(f);
      for (i = 0; i < bf.length; i++) {
         if (f_n < bf[i] * 1e3) {
            b[i].push(f);
            break;
         }
      }
   };

   var new_menu = function(el, i, oo, menu_s) {
      var o = kiwi_deep_copy(oo);
      hfdl.menu_n++;
      hfdl['menu'+i] = -1;
      hfdl.menus[i] = o;
      hfdl.menu_s[i] = menu_s;
      var m = [];

      w3_obj_enum(o, function(key_s, j, o2) {
         //console.log(menu_s +'.'+ i +'.'+ key_s +'.'+ j +':');
         //console.log(o2);
         var name_s = o2.name.split(', ')[1];
         o2.frequencies.sort(function(a,b) { return a - b; });
         var freq_a = [];
         o2.frequencies.forEach(function(f) {
            freq_a.push(f);
            add_bands_menu(f +' '+ name_s);
         });
         m[name_s] = freq_a;
      });

      //console.log('m=...');
      //console.log(m);
      var s = w3_select_hier(hfdl.sfmt, menu_s, 'select', 'hfdl.menu'+ i, -1, m, 'hfdl_pre_select_cb');
      var el2 = w3_create_appendElement(el, 'div', s);
      w3_add(el2, 'id-'+ menu_s);
   };
   
   var s;
   var el = w3_el('id-hfdl-menus');
   el.innerHTML = '';
   hfdl.menu_n = 0;

   w3_obj_enum(hfdl.stations, function(key_s, i, o2) {
      //console.log(key_s +'.'+ i +':');
      //console.log(o2);
      if (key_s != 'stations') return;
      new_menu(el, 0, o2, 'Stations');
   });

   var b2 = [];
   for (i = 0; i < bf.length; i++) {
      b[i].sort(function(a,b) { return parseInt(b) - parseInt(a); });
      var o = {};
      o.name = 'x, '+ bf_s[i] + ' MHz';
      o.frequencies = [];
      b[i].forEach(function(f) { o.frequencies.push(f); });
      b2[i.toString()] = o;
   }
   //console.log(b2);
   new_menu(el, 1, b2, 'Bands');
   //console.log('hfdl_render_menus hfdl.menu_n='+ hfdl.menu_n);
}

function hfdl_format_cb(path, idx, first)
{
   if (first) return;
   hfdl.format_m = +idx;
   w3_set_value(path, +idx);     // for benefit of direct callers
   hfdl_render_menus();
}

function hfdl_msg(color, s)
{
   var el = w3_el('id-hfdl-msg');
   w3_remove_then_add(el, hfdl.msg_last_color, color);
   hfdl.msg_last_color = color;
   el.innerHTML = '<b>'+ s +'</b>';
   //console.log('MSG '+ s);
}

function hfdl_clear_menus(except)
{
   // reset frequency menus
   for (var i = 0; i < hfdl.menu_n; i++) {
      if (!isArg(except) || i != except)
         w3_select_value('hfdl.menu'+ i, -1);
   }
}

function hfdl_np_pre_select_cb(path, val, first)
{
   hfdl_pre_select_cb(path, val, first);
}

function hfdl_pre_select_cb(path, val, first)
{
   if (first) return;
   val = +val;
   
   if (dbgUs) console.log('hfdl_pre_select_cb path='+ path);
   
   if (val == 0) {
      if (dbgUs) console.log('hfdl_pre_select_cb path='+ path +' $$$$ val=0 $$$$');
      return;
   }

	var menu_n = parseInt(path.split('hfdl.menu')[1]);
   if (dbgUs) console.log('hfdl_pre_select_cb path='+ path +' val='+ val +' menu_n='+ menu_n);

   // find matching object entry in hfdl.menus[] hierarchy and set hfdl.* parameters from it
   var lsb = false;
   var header;
   var cont = 0;
   var found = false;

	w3_select_enum(path, function(option) {
	   if (found) return;
	   if (dbgUs) console.log('hfdl_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	   
	   if (option.disabled && option.value != -1) {
	      if (cont)
	         header = header +' '+ option.innerHTML;
	      else
	         header = option.innerHTML;
	      cont = 1;
	      lsb = false;
	      //console.log('lsb = false');
	   } else {
	      cont = 0;
	   }
	   
	   if (w3_contains(option, 'id-hfdl-lsb')) {
	      //console.log('lsb = true');
	      lsb = true;
	   }
	   if (option.value != val) return;
	   found = true;
      hfdl.cur_menu = menu_n;
      hfdl.cur_header = header;
      hfdl.lsb = lsb;
      //console.log('hfdl.lsb='+ lsb);
	   
      hfdl.menu_sel = option.innerHTML +' ';
      if (dbgUs) console.log('hfdl_pre_select_cb opt.val='+ option.value +' menu_sel='+ hfdl.menu_sel +' opt.id='+ option.id);

      var id = option.id.split('id-')[1];
      if (isUndefined(id)) {
	      console.log('hfdl_pre_select_cb: option.id isUndefined');
	      console.log('hfdl_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	      console.log(option);
	      return;
      }
      
      id = id.split('-');
      var i = id[0];
      var j = id[1];
      if (dbgUs) console.log('hfdl_pre_select_cb i='+ i +' j='+ j);
      var o1 = w3_obj_seq_el(hfdl.menus[menu_n], i);
      //console.log('o1=...');
      //console.log(o1);
      if (dbgUs) w3_console.log(o1, 'o1');
      o2 = w3_obj_seq_el(o1.frequencies, j);
      //console.log('o2=...');
      //console.log(o2);
      if (dbgUs) w3_console.log(o2, 'o2');
   
      var s, show_msg = 0;
      o2 = parseInt(o2);
      if (isNumber(o2)) {
         if (dbgUs) console.log(o2);
         hfdl_tune(o2);
         s = (+o2).toFixedNZ(1) +' kHz';
         show_msg = 1;
      }

      if (show_msg) {
         hfdl_msg('w3-text-css-yellow', hfdl.menu_s[menu_n] +', '+ hfdl.cur_header +': '+ s);
      }

      // if called directly instead of from menu callback, select menu item
      w3_select_value(path, val);
	});

   // reset other frequency menus
   hfdl_clear_menus(menu_n);
}

function hfdl_tune(f_kHz)
{
   if (dbgUs) console.log('hfdl_tune tx f='+ f_kHz.toFixed(2));
   hfdl.freq = f_kHz;
   ext_tune(f_kHz, 'iq', ext_zoom.CUR);
   ext_set_passband(hfdl.pb.lo, hfdl.pb.hi);    // optimal passband for HFDL
   ext_send('SET display_freq='+ f_kHz.toFixed(2));
}

function hfdl_clear_button_cb(path, val, first)
{
   if (first) return;
   //console.log('hfdl_clear_button_cb'); 
   hfdl.console_status_msg_p.s = encodeURIComponent('\f');
   kiwi_output_msg('id-hfdl-console-msgs', 'id-hfdl-console-msg', hfdl.console_status_msg_p);
   hfdl.log_txt = '';
   hfdl_test_cb('', 0);
}

function hfdl_display_cb(path, idx, first)
{
	//console.log('hfdl_display_cb: idx='+ idx +' first='+ first);
	if (first) return;
   idx = +idx;
   hfdl.dsp = idx;
	w3_set_value(path, idx);
}

function hfdl_test_cb(path, val, first)
{
   if (first) return;
   val = +val;
   if (dbgUs) console.log('hfdl_test_cb: val='+ val);
   hfdl.testing = val;
	w3_el('id-hfdl-bar').style.width = '0%';
   w3_show_hide('id-hfdl-bar-container', hfdl.testing);
   
   // positive value gates test audio to a specific freq
   // negative value applies test audio at all times
   //var f = '24000.60';
   var f = -(+ext_get_freq_kHz()).toFixed(2);
   if (dbgUs) console.log('hfdl_test_cb='+ (val? f : 0));
	ext_send('SET test='+ (val? f : 0));
}

function hfdl_log_mins_cb(path, val)
{
   hfdl.log_mins = w3_clamp(+val, 0, 24*60, 0);
   console.log('hfdl_log_mins_cb path='+ path +' val='+ val +' log_mins='+ hfdl.log_mins);
	w3_set_value(path, hfdl.log_mins);

   kiwi_clearInterval(hfdl.log_interval);
   if (hfdl.log_mins) hfdl.log_interval = setInterval(function() { hfdl_log_cb(); }, hfdl.log_mins * 60000);
}

function hfdl_log_cb()
{
   var ts = kiwi_host() +'_'+ new Date().toISOString().replace(/:/g, '_').replace(/\.[0-9]+Z$/, 'Z') +'_'+ w3_el('id-freq-input').value +'_'+ cur_mode;
   var txt = new Blob([hfdl.log_txt], { type: 'text/plain' });
   var a = document.createElement('a');
   a.style = 'display: none';
   a.href = window.URL.createObjectURL(txt);
   a.download = 'HFDL.'+ ts +'.log.txt';
   document.body.appendChild(a);
   console.log('hfdl_log: '+ a.download);
   a.click();
   window.URL.revokeObjectURL(a.href);
   document.body.removeChild(a);
}

// automatically called on changes in the environment
function HFDL_environment_changed(changed)
{
   //w3_console.log(changed, 'HFDL_environment_changed');

   if (changed.freq || changed.mode) {
      var f_kHz = ext_get_freq_kHz();
      var mode = ext_get_mode();
      //console.log('HFDL_environment_changed freq='+ hfdl.freq +' f_kHz='+ f_kHz);
	   if (hfdl.freq != f_kHz || mode != 'iq') {
	      console.log('hfdl_clear_menus()');
	      hfdl_clear_menus();
	   }
   }

   if (changed.resize) {
      var el = w3_el('id-hfdl-data');
      var left = (window.innerWidth - 1024 - time_display_width()) / 2;
      el.style.left = px(left);
   }
}

function HFDL_blur()
{
   // anything that needs to be done when extension blurred (closed)
	ext_set_mode(hfdl.saved_mode);
   ext_agc_delay(hfdl.save_agc_delay);
}

function HFDL_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'HFDL decoder help') +
         w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
            w3_div('w3-margin-R-8',
               'Periodic downloading of the HFDL message log to a file can be specified. Adjust your browser settings so these files are downloaded ' +
               'and saved automatically without causing a browser popup window for each download.' +

               '<br><br>URL parameters: <br>' +
               '<i>(menu match)</i> &nbsp; display:[012] &nbsp; scan[:<i>secs</i>] &nbsp; ' +
               'log_time:<i>mins</i> &nbsp; test' +
               '<br><br>' +
               'The first URL parameter can be a frequency entry from the "Bands" menu (e.g. "8977"). <br>' +
               '[012] refers to the order of selections in the corresponding menu.' +
               '<br><br>' +
               'Keywords are case-insensitive and can be abbreviated. So for example these are valid: <br>' +
               '<i>ext=hfdl,8977</i> &nbsp;&nbsp; ' +
               '<i>ext=hfdl,8977,d:1</i> &nbsp;&nbsp; <i>ext=hfdl,8977,l:10</i><br>' +
               ''
            )
         );

      confirmation_show_content(s, 610, 280);
      w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
   }
   return true;
}

// called to display HTML for configuration parameters in admin interface
function HFDL_config_html()
{
   ext_config_html(hfdl, 'hfdl', 'HFDL', 'HFDL configuration');
}
