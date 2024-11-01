
// Copyright (c) 2021 John Seamons, ZL4VO/KF6VO

var ale = {
   ext_name: 'ALE_2G',     // NB: must match ALE_2G.cpp:ale_2g_ext.name
   first_time: true,
   dataH: 300,
   ctrlW: 600,
   //ctrlH: 215,
   ctrlH: 185,
   freq: 0,
   freq_next: 0,
   tune_ack_expecting: 0,
   tune_ack_got: 0,
   sfmt: 'w3-text-red',
   scan: ['scan', 'w3-bold w3-text-blue'],
   scan_lsb: ['scan', 'id-ale_2g-lsb w3-bold w3-text-blue'],
   pb: { lo: 600, hi: 2650 },
   
   nets: null,
   url: kiwi_SSL() +'files.kiwisdr.com/ale/ALE_nets.cjson',
   using_default: false,
   double_fault: false,
   
   SHOW_SCAN: 0,
   SHOW_STOP: 1,
   action_s: [ 'SCAN', 'STOP' ],
   
   SET: -2,
   RESUME: -1,
   STOP: 0,
   START: 1,
   scan_s: [ 'SET', 'RESUME', 'STOP', 'START' ],
   
   SHOW_FORMAT_EXPAND:       0x0,
   SHOW_FORMAT_SORT_HI_LO:   0x1,
   SHOW_FORMAT_COLLAPSE:     0x2,
   format_m: 0,
   format_s: [ 'expand', 'exp H>L', 'collapse', 'col H>L' ],
   
   dsp: 0,
   dsp_s: [ 'DX', '+cmds', 'all', '+debug' ],
   
   scan_t_ms: 1000,
   scan_t_m: 1,
   scan_t_custom_i: 5,
   scan_t_s: [ '0.75s', '1s', '2s', '3s', '5s', 'custom' ],
   scan_t_f: '',
   scan_list: null,
   scan_cur_idx: 0,
   scan_cur_val: 0,
   scanning: false,
   scan_timeout: null,
   
   have_user_scan_list: false,
   user_scan_list: [],
   
   isActive: false,
   ignore_resume: false,
   testing: false,
   periodic_self_test: false,
   
   REC: 0x1,
   LOG: 0x2,
   record_m: 0,
   record_s: [ 'off', 'rec', 'log', 'r+l' ],
   recording: false,
   record_secs: 30,
   record_timeout: null,

   log_mins: 10,
   log_interval: null,
   log_txt: '',
   
   f_limit: 0,
   f_sign: 1,
   f_limit_m: 0,
   f_limit_le_custom_i: 5,
   f_limit_ge_custom_i: 10,
   f_limit_s: [ 'none', '&le; 6 MHz', '&le; 8 MHz', '&le; 10 MHz', '&le; 12 MHz', '&le; custom',
                        '&ge; 6 MHz', '&ge; 8 MHz', '&ge; 10 MHz', '&ge; 12 MHz', '&ge; custom' ],
   f_limit_v: [ 0, 6, 8, 10, 12, null, 6, 8, 10, 12, null ],
   f_limit_f: '',
   
   resamp_m: 1,
   resamp_s: [ 'ok fast', 'better slow' ],

   menu_n: 0,
   menu_s: [ ],
   menus: [ ],
   menu_sel: '',

   // must set "remove_returns" so output lines with \r\n (instead of \n alone) don't produce double spacing
   console_status_msg_p: { scroll_only_at_bottom: true, process_return_alone: false, remove_returns: true, cols: 135 }
};

function ALE_2G_main()
{
	ext_switch_to_client(ale.ext_name, ale.first_time, ale_2g_recv);		// tell server to use us (again)
	if (!ale.first_time)
		ale_2g_controls_setup();
	ale.first_time = false;
}

function ale_2g_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		console.log('ale_2g_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('ale_2g_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('ale_2g_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				ale_2g_controls_setup();
				break;

			case "bar_pct":
			   if (ale.testing) {
               var pct = w3_clamp(parseInt(param[1]), 0, 100);
               //if (pct > 0 && pct < 3) pct = 3;    // 0% and 1% look weird on bar
               var el = w3_el('id-ale_2g-bar');
               if (el) el.style.width = pct +'%';
               //console.log('bar_pct='+ pct);
            }
			   break;

			case "test_start":
			   ale_2g_test_cb('', 1);
			   break;

			case "test_done":
            if (ale.testing) {
               w3_hide('id-ale_2g-bar-container');
               w3_show('id-ale_2g-record');
               ale.testing = ale.periodic_self_test = false;
            }
			   break;

			case "chars":
			   if (!ale.periodic_self_test) ale_2g_decoder_output_chars(param[1]);
				break;

			case "tune_ack":
			   ale.tune_ack_got++;
				ale_2g_tune_ack(param[1]);
				break;

			case "active":
			   var freq = +param[1];
			   if (ale.scanning && freq != 0) {
			      if (dbgUs) console.log('#### $active STOP freq='+ freq.toFixed(2) +' ale.freq='+ ale.freq +' scanning='+ ale.scanning +' ignore_resume='+ ale.ignore_resume);
			      ale.isActive = true;
               ale_2g_scanner(ale.STOP);     // pause scanning without removing scanlist
               
               //if (freq != ale.freq) {
               if (freq != ale.freq_next) {
			         if (dbgUs) console.log('#### $GO BACK #### freq='+ freq.toFixed(2) +' != ale.freq='+ ale.freq +' != ale.freq_next='+ ale.freq_next);
			         ale_2g_tune_ack(freq, true);
               }
			   } else
			   
			   if (!ale.scanning && ale.scan_list && freq == 0) {
			      if (dbgUs) console.log('#### $active RESUME ignore_resume='+ ale.ignore_resume +' freq='+ freq.toFixed(2) +' scanning='+ ale.scanning);
			      if (ale.recording) {
			         if (dbgUs) console.log('#### $active RESUME: delayed due to recording');
                  ale_2g_msg('w3-text-css-lime', ale.menu_s[ale.cur_menu] +', '+ ale.cur_header +': scanning paused, waiting for recording');
			         break;
			      }
		 	      ale.isActive = false;
			      if (!ale.ignore_resume)
                  ale_2g_scanner(ale.RESUME);   // resume scanning where we left off
               ale.ignore_resume = false;
			   } else
			   
			   {
			      ale.ignore_resume = true;
			      if (dbgUs) console.log('#### $active NOP freq='+ freq.toFixed(2) +' scanning='+ ale.scanning +' scanlist='+ ale.scan_list);
			   }
				break;

			case "call_est_test":
			   if (!dbgUs) break;
			   // fall through ...
			   
			case "call_est":
			   var rtime = ale.record_secs;
			   console.log(param[0] +' rec_log='+ ale.record_m +' rtime='+ rtime);
			   if ((ale.record_m & ale.REC) && rtime) {
			      if (ale.recording) {
				      console.log('call_est: already recording');
			      } else {
				      console.log('call_est: rec_log='+ ale.record_m +' rtime '+ rtime);
				      ale_2g_decoder_output_chars('--- call established --- recording ---\n');
                  ale.recording = true;
                  toggle_or_set_rec(1);
                  ale.record_timeout = setTimeout(function() { ale_2g_recording_stop(); }, rtime * 1000);
               }
			   }
				break;

			case "event":
				break;

			default:
				console.log('ale_2g_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function ale_2g_toggle_recording_cb()
{
   toggle_or_set_rec();
   if (ale.recording) ale_2g_recording_stop();
}

function ale_2g_recording_stop()
{
   toggle_or_set_rec(0);
   kiwi_clearTimeout(ale.record_timeout);
   ale.recording = false;
   
   if (ale.isActive) {
      if (dbgUs) console.log('#### $active RESUME: after recording');
      ale_2g_scanner(ale.RESUME);   // resume scanning where we left off
      ale.isActive = false;
   }
}

function ale_2g_decoder_output_chars(c)
{
   ale.console_status_msg_p.s = c;     // NB: already encoded on C-side
   ale.log_txt += kiwi_remove_escape_sequences(kiwi_decodeURIComponent('ALE_2G', c));

   // kiwi_output_msg() does decodeURIComponent()
   kiwi_output_msg('id-ale_2g-console-msgs', 'id-ale_2g-console-msg', ale.console_status_msg_p);
}

function ale_2g_controls_setup()
{
   var data_html =
      time_display_html('ale_2g') +

      w3_div('id-ale_2g-data|left:150px; width:1044px; height:'+ px(ale.dataH) +'; overflow:hidden; position:relative; background-color:mediumBlue;',
			w3_div('id-ale_2g-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|left: 10px; width:1024px; position:absolute; overflow-x:hidden;',
			   '<pre><code id="id-ale_2g-console-msgs"></code></pre>'
			)
      );

	var controls_html =
		w3_div('id-ale_2g-controls w3-text-white',
         w3_col_percent('w3-tspace-8 w3-valign/',
            w3_div('w3-medium w3-text-aqua', '<b>ALE 2G decoder</b>'), 30,
            w3_div('', 'Based on <b><a href="https://github.com/gat3way/gr-ale" target="_blank">gr-ale</a></b> by Milen Rangelov et al.</b>')
         ),

         w3_inline('w3-tspace-4 w3-halign-space-between/',
            w3_div('id-ale_2g-msg')
         ),
         
         w3_inline('id-ale_2g-menus w3-tspace-6 w3-halign-space-between/'),

         w3_inline('w3-tspace-8/w3-margin-between-16',
            w3_select(ale.sfmt, '', 'format', 'ale.format_m', ale.format_m, ale.format_s, 'ale_2g_format_cb'),
            w3_select(ale.sfmt, '', 'display', 'ale.dsp', ale.dsp, ale.dsp_s, 'ale_2g_display_cb'),
            w3_inline('',
               w3_select('id-ale_2g-scan-t '+ ale.sfmt, '', 'scan T', 'ale.scan_t_m', ale.scan_t_m, ale.scan_t_s, 'ale_2g_scan_t_cb'),
               w3_input('id-ale_2g-scan-t-custom w3-margin-left w3-hide|padding:0;width:auto|size=4',
                  '', 'ale.scan_t_f', ale.scan_t_f, 'ale_2g_scan_t_custom_cb')
            ),
            w3_inline('',
               w3_select('id-ale_2g-f-limit '+ ale.sfmt, '', 'F limit', 'ale.f_limit_m', ale.f_limit_m, ale.f_limit_s, 'ale_2g_f_limit_cb'),
               w3_input('id-ale_2g-f-limit-custom w3-margin-left w3-hide|padding:0;width:auto|size=8',
                  '', 'ale.f_limit_f', ale.f_limit_f, 'ale_2g_f_limit_custom_cb')
            )
         ),

         w3_inline('w3-tspace-4 w3-valign/w3-margin-between-12',
            w3_button('w3-padding-smaller', 'Next', 'w3_select_next_prev_cb', { dir:w3_MENU_NEXT, id:'ale.menu', isNumeric:true, func:'ale_2g_np_pre_select_cb' }),
            w3_button('w3-padding-smaller', 'Prev', 'w3_select_next_prev_cb', { dir:w3_MENU_PREV, id:'ale.menu', isNumeric:true, func:'ale_2g_np_pre_select_cb' }),
            w3_button('id-ale_2g-scan-button w3-padding-smaller w3-green', 'Scan', 'ale_2g_scan_button_cb'),
            w3_button('id-ale_2g-clear-button w3-padding-smaller w3-css-yellow', 'Clear', 'ale_2g_clear_button_cb'),
            w3_button('id-ale_2g-log w3-padding-smaller w3-purple', 'Log', 'ale_2g_log_cb'),
            w3_button('id-ale_2g-test w3-padding-smaller w3-aqua', 'Test', 'ale_2g_test_cb', 1),
            w3_div('id-ale_2g-bar-container w3-progress-container w3-round-large w3-white w3-hide|width:70px; height:16px',
               w3_div('id-ale_2g-bar w3-progressbar w3-round-large w3-light-green|width:0%', '&nbsp;')
            ),
            
            w3_select('id-ale_2g-record '+ ale.sfmt, '', 'record', 'ale.record_m', ale.record_m, ale.record_s, 'ale_2g_record_cb'),
            w3_input('id-ale_2g-record-secs/w3-label-not-bold/|padding:0;width:auto|size=4',
               'rec sec', 'ale.record_secs', ale.record_secs, 'ale_2g_record_secs_cb'),
            w3_div('fa-stack||title="record"',
               w3_icon('id-rec2', 'fa-repeat fa-stack-1x w3-text-pink', 22, '', 'ale_2g_toggle_recording_cb')
            ),
            w3_input('id-ale_2g-log-mins/w3-label-not-bold/|padding:0;width:auto|size=4',
               'log min', 'ale.log_mins', ale.log_mins, 'ale_2g_log_mins_cb'),

            (0 && dbgUs)? w3_select(ale.sfmt, '', 'resampler', 'ale.resamp_m', ale.resamp_m, ale.resamp_s, 'ale_2g_resamp_cb') : ''
         )
      );

	ext_panel_show(controls_html, data_html, null);
   ext_set_data_height(ale.dataH);
	ext_set_controls_width_height(ale.ctrlW, ale.ctrlH);
	time_display_setup('ale_2g');
	ale_2g_msg('w3-text-css-yellow', '&nbsp;');
	ale_2g_set_scan(ale.SHOW_SCAN);
	ALE_2G_environment_changed( {resize:1, freq:1} );
	ext_send('SET start');
	ext_send('SET use_new_resampler='+ ale.resamp_m);
	
	ale.saved_mode = ext_get_mode();
	ext_set_mode('usb');
   ext_set_passband(ale.pb.lo, ale.pb.hi);   // minimum passband for ALE
	
	// our sample file is 12k only
	if (ext_nom_sample_rate() != 12000)
	   w3_add('id-ale_2g-test', 'w3-disabled');
	
	w3_do_when_rendered('id-ale_2g-menus',
	   function() {
         ext_send('SET reset');
         ale.double_fault = false;
         if (0 && dbgUs) {
            kiwi_ajax(ale.url +'.xxx', 'ale_2g_get_nets_done_cb', 0, -500);
         } else {
            kiwi_ajax(ale.url, 'ale_2g_get_nets_done_cb', 0, 10000);
         }
      
         //ale.watchdog = setInterval(function() { ale_2g_watchdog(); }, 1000);
      }
   );
   // REMINDER: w3_do_when_rendered() returns immediately
}

function ale_2g_watchdog()
{
   //console.log('ale tick');
}

function ale_2g_get_nets_done_cb(nets)
{
   var url = ale.url;
   var fault = false;
   
   if (!nets) {
      console.log('ale_2g_get_nets_done_cb: nets='+ nets);
      fault = true;
   } else
   
   if (nets.AJAX_error && nets.AJAX_error == 'timeout') {
      console.log('ale_2g_get_nets_done_cb: TIMEOUT');
      ale.using_default = true;
      fault = true;
   } else
   if (nets.AJAX_error && nets.AJAX_error == 'status') {
      console.log('ale_2g_get_nets_done_cb: status='+ nets.status);
      ale.using_default = true;
      fault = true;
   } else
   if (!isObject(nets)) {
      console.log('ale_2g_get_nets_done_cb: not array');
      fault = true;
   }
   
   if (fault) {
      if (ale.double_fault) {
         console.log('ale_2g_get_nets_done_cb: default station list fetch FAILED');
         console.log(nets);
         return;
      }
      console.log(nets);
      
      // load the default station list from a file embedded with the extension (should always succeed)
      var url = kiwi_url_origin() + '/extensions/ALE_2G/ALE_nets.cjson';
      console.log('ale_2g_get_nets_done_cb: using default station list '+ url);
      ale.using_default = true;
      ale.double_fault = true;
      kiwi_ajax(url, 'ale_2g_get_nets_done_cb', 0, /* timeout */ 10000);
      return;
   }
   
   console.log('ale_2g_get_nets_done_cb: from '+ url);
   if (isDefined(nets.AJAX_error)) {
      console.log(nets);
      return;
   }
   ale.nets = nets;
   ale_2g_render_menus();

   ale.url_params = ext_param();
   if (dbgUs) console.log('url_params='+ ale.url_params);
   
	if (ale.url_params) {
      var p = ale.url_params.split(',');

	   // first URL param can be a match in the preset menus
      var m_freq = p[0].parseFloatWithUnits('kM', 1e-3);
      var m_str = kiwi_decodeURIComponent('ale', p[0]).toLowerCase();
      if (dbgUs) console.log('URL freq='+ m_freq);
      var found_scan_param = false;
      var found_menu_match = false;
      var menu_match_numeric = false;
      var match_menu, match_val;
      var do_test = 0;
      
      // look for user scan list
      if (isNumber(m_freq) && p.length >= 2) {
         var stop = false;
         p.forEach(function(a, i) {
            var f = a.parseFloatWithUnits('kM', 1e-3);
            if (!isNumber(f)) stop = true;
            if (stop) return;
            ale.user_scan_list.push(f);
         });
      }
      
      // found user scan list
      if (ale.user_scan_list.length >= 2) {
         ale.have_user_scan_list = true;

         var lsb = false;
         p.forEach(function(a, i) {
            if (w3_ext_param('lsb', a).match)
               lsb = true;
         });

         if (lsb) ale.user_scan_list.unshift("lsb");
         ale.user_scan_list.unshift(0);   // add 'scan' entry to front of list
         if (dbgUs) console.log('user_scan_list=...');
         if (dbgUs) console.log(ale.user_scan_list);
         ale_2g_render_menus();     // must render again before ale_2g_pre_select_cb() below
         // -1 below because ale_2g_render_menus() has added +1 to array for 'scan' entry at front
         for (var i = 0; i < ale.user_scan_list.length-1; i++)
            p.shift();
      }

      // process any format:N param before matching menu items in case menu re-sorted or collapsed
      p.forEach(function(a, i) {
         var r;
         if ((r = w3_ext_param('format', a)).match) {
            if (isNumber(r.num)) {
               var idx = w3_clamp(r.num, 0, ale.format_s.length-1, 0);
               console.log('menus '+ r.num +' '+ idx);
               ale_2g_format_cb('id-ale.format_m', idx);      // will call ale_2g_render_menus()
            }
         }
      });

      // select matching menu item frequency if there was no user scan list already specified
      if (!ale.have_user_scan_list) {

         for (var i = 0; i < ale.menu_n; i++) {
            var menu = 'ale.menu'+ i;
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
                  // find first 'scan' or single frequency entry by
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
                  // delay call to ale_2g_pre_select_cb(), which might start scanning,
                  // until other params processed below
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
      }

      p.forEach(function(a, i) {
         if (dbgUs) console.log('ALE param2 <'+ a +'>');
         var r;
         if (w3_ext_param('help', a).match) {
            ext_help_click();
         } else
         if ((r = w3_ext_param('display', a)).match) {
            if (isNumber(r.num)) {
               var idx = w3_clamp(r.num, 0, ale.dsp_s.length-1, 0);
               console.log('display '+ r.num +' '+ idx);
               ale_2g_display_cb('id-ale.dsp', idx);
            }
         } else
         if ((r = w3_ext_param('scan', a)).match) {
            // 'scan' or 'scan:(val)' which always considers val a custom value
            found_scan_param = true;
            if (isNumber(r.num)) {
               ale_2g_scan_t_custom_cb('id-ale_2g-scan-t-custom', r.num);
               ale_2g_scan_t_cb('id-ale_2g-scan-t', ale.scan_t_custom_i);
            }
         } else
         if ((r = w3_ext_param('limit_le', a)).match) {
            if (r.has_value) {
               ale_2g_f_limit_custom_cb('id-ale_2g-f-limit-custom', r.string_case);
               ale_2g_f_limit_cb('id-ale_2g-f-limit', ale.f_limit_le_custom_i);
            }
         } else
         if ((r = w3_ext_param('limit_ge', a)).match) {
            if (r.has_value) {
               ale_2g_f_limit_custom_cb('id-ale_2g-f-limit-custom', r.string_case);
               ale_2g_f_limit_cb('id-ale_2g-f-limit', ale.f_limit_ge_custom_i);
            }
         } else
         if ((r = w3_ext_param('rec', a)).match) {
            var num = r.has_value? r.num : 1;
            if (isNumber(num)) {
               var idx = w3_clamp(num, 0, ale.record_s.length-1, 0);
               console.log('record '+ num +' '+ idx);
               ale_2g_record_cb('id-ale.record_m', idx);
            }
         } else
         if ((r = w3_ext_param('rec_time', a)).match) {
            if (isNumber(r.num)) {
               ale_2g_record_secs_cb('id-ale.record_secs', r.num);
            }
         } else
         if ((r = w3_ext_param('log_time', a)).match) {
            if (isNumber(r.num)) {
               ale_2g_log_mins_cb('id-ale.log_mins', r.num);
            }
         } else
         if (w3_ext_param('test', a).match) {
            do_test = 1;
         } else
         if (w3_ext_param('format', a).match) {
            ;     // processed above
         } else
            console.log('ALE 2G: unknown URL param "'+ a +'"');
      });
      
      if (found_menu_match) {
         ale_2g_pre_select_cb(match_menu, match_val, false);
      }

      if (found_scan_param && found_menu_match) {
         console.log('found_scan_param && found_menu_match');
         if (look_for_first_entry)
            ale_2g_scanner(ale.SET, undefined, 1);
         if (!menu_match_numeric)
            ale_2g_scanner(ale.RESUME);
      }
   }

   // if a user scan list is present always begin scanning just as for net name matches
   if (ale.have_user_scan_list) {
      console.log('have_user_scan_list');

      // since first entry in local menu, value should always be = 1 to match 'scan' entry
      ale_2g_pre_select_cb('ale.menu'+ (ale.menu_n-1), 1, false);
      ale_2g_scanner(ale.SET, undefined, 1);
      ale_2g_scanner(ale.RESUME);
   }
   
   if (do_test) ale_2g_test_cb('', 1);
   if (dbgUs) console.log(ale.nets);
}

function ale_2g_render_menus()
{
   var new_menu = function(el, i, oo, menu_s) {
      var o = kiwi_deep_copy(oo);
      ale.menu_n++;
      ale['menu'+i] = -1;
      ale.menus[i] = o;
      ale.menu_s[i] = menu_s;

      w3_obj_enum(o, function(net_s, j, o2) {
         //console.log(menu_s +'.'+ i +'.'+ net_s +'.'+ j +':');
         //console.log(o2);
         if (ale.format_m & ale.SHOW_FORMAT_SORT_HI_LO) {
            // sort high to low except zero which is always first
            o2.sort(function(a,b) {
               if (a == 0) return -1e20;
               if (b == 0) return +1e20;
               return b-a;
            });
         }
         //console.log(menu_s +'.'+ i +'.'+ net_s +'.'+ j +' NEW:');
         //console.log(o2);

         if (isArray(o2) && o2[0] == 0 && o2.length >= 2) {
            o2[0] = (isString(o2[1]) && o2[1].includes('lsb'))? ale.scan_lsb : ale.scan;  // sets o[0] to signal 'scan' entry
         }
         if (isArray(o2) && o2.length >= 2 && isString(o2[1])) {
            o2.splice(1,1);
         }
      });

      var collapse = (ale.format_m & ale.SHOW_FORMAT_COLLAPSE)? 1:0;
      var s = w3_select_hier_collapse(ale.sfmt, menu_s, 'select', 'ale.menu'+ i, -1, collapse, o, 'ale_2g_pre_select_cb');
      var el2 = w3_create_appendElement(el, 'div', s);
      w3_add(el2, 'id-'+ menu_s);
   };
   
   var s;
   var el = w3_el('id-ale_2g-menus');
   el.innerHTML = '';
   ale.menu_n = 0;

   w3_obj_enum(ale.nets, function(menu_s, i, o2) {   // each object
      //console.log(menu_s +'.'+ i +':');
      //console.log(o2);
      
      //console.log(menu_s +'.'+ i +' NEW:');
      //console.log(o2);
      new_menu(el, i, o2, menu_s);
   });
   
   var pvt_menu = {};
   var have_pvt_menu = false;

   if (ale.have_user_scan_list) {
      pvt_menu['URL list'] = ale.user_scan_list;
      have_pvt_menu = true;
   }
   
   if (isDefined(cfg.ale_2g) && isDefined(cfg.ale_2g.admin_menu)) {
      //console.log('ale_2g_render_menus admin_menu:');
      s = kiwi_remove_cjson_comments(decodeURIComponent(cfg.ale_2g.admin_menu));
      //console.log(s);
      var o;
      try {
         o = JSON.parse(s);
         w3_obj_enum(o, function(key, i, o2) {
            pvt_menu[key] = o2;
            have_pvt_menu = true;
         });
      } catch(ex) {
         console.log('JSON parse error');
         console.log(ex);
         console.log(cfg.ale_2g.admin_menu);
         o = {};
      }
   }
   
   if (have_pvt_menu) {
      if (dbgUs) console.log(pvt_menu);
      new_menu(el, ale.menu_n, pvt_menu, 'Local');
   }
   //console.log('ale_2g_render_menus ale.menu_n='+ ale.menu_n);
}

function ale_2g_format_cb(path, idx, first)
{
   if (first) return;
   ale.format_m = +idx;
   w3_set_value(path, +idx);     // for benefit of direct callers
   ale_2g_render_menus();
}

function ale_2g_msg(color, s)
{
   var el = w3_el('id-ale_2g-msg');
   w3_remove_then_add(el, ale.msg_last_color, color);
   ale.msg_last_color = color;
   el.innerHTML = '<b>'+ s +'</b>';
   //console.log('MSG '+ s);
}

function ale_2g_clear_menus(except)
{
   // reset frequency menus
   for (var i = 0; i < ale.menu_n; i++) {
      if (isNoArg(except) || i != except)
         w3_select_value('ale.menu'+ i, -1);
   }
}

function ale_2g_np_pre_select_cb(path, val, first)
{
   var resume_scanning = (ale.scanning && ale.scan_list);
   if (resume_scanning) {
      if (dbgUs) console.log('ale_2g_pre_select_cb path='+ path +' $$$$ BUSY $$$$');
      return;
   }

   ale_2g_pre_select_cb(path, val, first);
}

function ale_2g_pre_select_cb(path, val, first)
{
   if (first) return;
   val = +val;
   
   if (dbgUs) console.log('ale_2g_pre_select_cb path='+ path +' scanning='+ ale.scanning +' isActive='+ ale.isActive +' scan_list='+ ale.scan_list);
   
   if (val == 0) {
      if (dbgUs) console.log('ale_2g_pre_select_cb path='+ path +' $$$$ val=0 $$$$ cur_idx='+ ale.scan_cur_idx);
      return;
   }

	var menu_n = parseInt(path.split('ale.menu')[1]);
   if (dbgUs) console.log('ale_2g_pre_select_cb path='+ path +' val='+ val +' menu_n='+ menu_n);

   // find matching object entry in ale.menus[] hierarchy and set ale.* parameters from it
   var lsb = false;
   var header;
   var cont = 0;
   var found = false;

	w3_select_enum(path, function(option) {
	   if (found) return;
	   if (dbgUs) console.log('ale_2g_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	   
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
	   
	   if (w3_contains(option, 'id-ale_2g-lsb')) {
	      //console.log('lsb = true');
	      lsb = true;
	   }
	   if (option.value != val) return;
	   found = true;
      ale.cur_menu = menu_n;
      ale.cur_header = header;
      ale.lsb = lsb;
      //console.log('ale.lsb='+ lsb);
	   
      ale.menu_sel = option.innerHTML +' ';
      if (dbgUs) console.log('ale_2g_pre_select_cb opt.val='+ option.value +' menu_sel='+ ale.menu_sel +' opt.id='+ option.id);

      var id = option.id.split('id-')[1];
      if (isUndefined(id)) {
	      console.log('ale_2g_pre_select_cb: option.id isUndefined');
	      console.log('ale_2g_pre_select_cb opt.val='+ option.value +' opt.disabled='+ option.disabled +' opt.inner='+ option.innerHTML);
	      console.log(option);
	      return;
      }
      
      id = id.split('-');
      var i = +id[0];
      var j = +id[1];
      if (dbgUs) console.log('ale_2g_pre_select_cb i='+ i +' j='+ j);
      var o1 = w3_obj_seq_el(ale.menus[menu_n], i);
      if (dbgUs) w3_console.log(o1, 'o1');
      o2 = w3_obj_seq_el(o1, j);
      if (dbgUs) w3_console.log(o2, 'o2');
   
      var s, show_msg = 0;
      if (isNumber(o2)) {
         if (dbgUs) console.log(o2);

         // selecting numeric menu entry resets inactivity timer
         ale_2g_tune(o2, true);
         s = (+o2).toFixedNZ(1) +' kHz';
         ale_2g_scanner(ale.SET, o1, j);  // specify scanlist so we can resume from here by clicking scan button
         show_msg = 1;
      } else
      if (isArray(o2)) {
         if (dbgUs) console.log(o2[0]);   // ["scan", "props"]
         s = o2[0];
         if (dbgUs) console.log(o1);      // [["scan", "props"], freq, ...]
         
         // selecting "scan" menu entry resets inactivity timer
         ale_2g_scanner(ale.START, o1, undefined, /* from_menu */ true);
      } else
      if (isString(o2)) {
         console.log('$str freq');
         console.log(o2);
         ale_2g_scanner(ale.STOP, null);
      }
   
      if (show_msg) {
         ale_2g_msg('w3-text-css-yellow', ale.menu_s[menu_n] +', '+ ale.cur_header +': '+ s);
      }

      // if called directly instead of from menu callback, select menu item
      w3_select_value(path, val);
	});

   // reset other frequency menus
   ale_2g_clear_menus(menu_n);
}

function ale_2g_tune(f_kHz, from_menu)
{
   ale.freq_next = f_kHz;
   ale.tune_ack_expecting++;
   if (dbgUs) console.log('ale_2g_tune tx f='+ f_kHz.toFixed(2));
   ext_send('SET tune='+ f_kHz.toFixed(2) + (from_menu? ',0' : ',1'));
}

function ale_2g_tune_ack(f_kHz, go_back_override)
{
   if (dbgUs) console.log('ale_2g_tune_ack rx f='+ f_kHz +' isActive='+ ale.isActive +' go_back_override='+ go_back_override);
   if (ale.isActive && go_back_override != true) return;
   ale.freq = f_kHz;
   //ext_tune(f_kHz, 'usb', ext_zoom.CUR, undefined, undefined, undefined, {no_set_freq:1});
   ext_tune(f_kHz, ale.lsb? 'lsb':'usb', ext_zoom.CUR);
}

function ale_2g_set_scan(val)
{
   //console.log('ale_2g_set_scan SET action='+ ale.action_s[val]); 

   var button_set = function(text, color) {
      var el = w3_el('id-ale_2g-scan-button');
      w3_button_text(el, text);
      w3_remove_then_add(el, ale.scan_last_color, color);
      ale.scan_last_color = color;
   };

   ale.action = val;

   switch (ale.action) {
   
   case ale.SHOW_STOP:
      button_set('Stop', 'w3-red');
      break;
   
   case ale.SHOW_SCAN:
      button_set('Scan', 'w3-green');
      break;
   }
}

function ale_2g_scan_button_cb(path, val, first)
{
   //console.log('ale_2g_scan_button_cb CB action='+ ale.action_s[ale.action]); 

   if (first) return;
   
   switch (ale.action) {
   
   case ale.SHOW_SCAN:
      ale_2g_scanner(ale.RESUME);
      break;
   
   case ale.SHOW_STOP:
      ale_2g_scanner(ale.STOP);
      break;
   }
}

function ale_2g_scan(scan)
{
   if (dbgUs) console.log('ale_2g_scan scan='+ TF(scan));
   ale.scanning = scan;
   ext_set_scanning(scan? 1:0);
}


// called as:
// ale_2g_scanner(ale.SET);               [-2] stop scanning, allows ale.{scan_list,scan_cur_idx} to be set | SHOW_SCAN
// ale_2g_scanner(ale.RESUME);            [-1] resume scanning where we left off | SHOW_STOP
// ale_2g_scanner(ale.STOP, null);        [0] stop scanning, remove scanlist | SHOW_SCAN
// ale_2g_scanner(ale.STOP);              [0] if scanning: pause, if not: stop scanning; without removing scanlist | always SHOW_SCAN
// ale_2g_scanner(ale.START, scan_list);  [1] start, if scanlist: SHOW_STOP else SHOW_SCAN

function ale_2g_scanner(idx, scan_list, new_idx, from_menu)
{
   if (dbgUs) console.log('ale_2g_scanner idx='+ ((idx <= 1)? ale.scan_s[idx+2] : idx) +' scan_cur_idx='+ ale.scan_cur_idx
      +' new_idx(PARAM)='+ new_idx +' scan_list(PARAM)='+ kiwi_typeof(scan_list) +' ale.scan_list='+ kiwi_typeof(ale.scan_list));
   var s, resume_scanning;
   
   if (isDefined(scan_list)) ale.scan_list = scan_list;
   if (isDefined(new_idx)) ale.scan_cur_idx = +new_idx;
   
   if (idx == ale.SET) {
      if (dbgUs) console.log('SET scan_list='+ ale.scan_list +' cur_idx='+ ale.scan_cur_idx);
      kiwi_clearTimeout(ale.scan_timeout);
      ale_2g_set_scan(ale.SHOW_SCAN);
      ale_2g_scan(0);
      return;
   }
   
   if (idx == ale.RESUME) {
      if (dbgUs) console.log('RESUME scan_list='+ ale.scan_list +' cur_idx='+ ale.scan_cur_idx);
      if (ale.scan_list) {
         idx = ale.scan_cur_idx;
         ale_2g_set_scan(ale.SHOW_STOP);
         ale_2g_scan(1);
         from_menu = true;    // make resume reset inactivity timer
      } else {
         if (dbgUs) console.log('RESUME but no scan_list');
         ale.scan_cur_idx = 0;
         kiwi_clearTimeout(ale.scan_timeout);
         ale_2g_set_scan(ale.SHOW_SCAN);
         ale_2g_scan(0);
         return;
      }
   }

   if (idx == ale.STOP) {
      kiwi_clearTimeout(ale.scan_timeout);
      resume_scanning = (ale.scanning && ale.scan_list);
      //ale_2g_set_action(resume_scanning? ale.SHOW_CONTINUE : ale.SHOW_CLEAR);
      ale_2g_set_scan(ale.SHOW_SCAN);
      //if (!resume_scanning) ale.scan_cur_idx = 0;
      ale_2g_scan(0);
      s = ale.isActive? ', signal detected' : '';
      if (resume_scanning)
         ale_2g_msg('w3-text-css-lime', resume_scanning? (ale.menu_s[ale.cur_menu] +', '+ ale.cur_header +': scanning paused'+ s) : '&nbsp;');
      return;
   }
   
   if (idx == ale.START) {
      if (ale.scan_list) {
         ale_2g_scan(1);
      }
      //ale_2g_set_action(ale.scan_list? ale.SHOW_PAUSE : ale.SHOW_CLEAR);
      ale_2g_set_scan(ale.scan_list? ale.SHOW_STOP : ale.SHOW_SCAN);
   }
   
   resume_scanning = (ale.scanning && ale.scan_list);
   if (!resume_scanning) {
      ale.scan_cur_idx = idx;
      if (dbgUs) console.log('ale_2g_scanner: IMMEDIATE CHANGE scan_cur_idx='+ idx);
      return;
   }
   
   var f, f_s;
   var lt = (ale.f_limit < 0)? true:false;
   var lim = Math.abs(ale.f_limit);
   var len = ale.scan_list.length;
   var looping = 0;
   var idx_f = -1;
   do {
      idx_f = idx;
      f_s = ale.scan_list[idx];
      f = +f_s;
      //ale.prev_idx = idx;
      idx++;
      if (idx >= len) idx = 1;
      if (looping++ > len) {
         //console.log('looping');
         ale_2g_msg('w3-text-orange', 'settings give empty scanlist');
         
         // poll for change that could clear empty scanlist condition
         kiwi_clearTimeout(ale.scan_timeout);
         ale.scan_timeout = setTimeout(function() { ale_2g_scanner(idx); }, ale.scan_t_ms);
         return;
      }
      //console.log('f='+ f +' lim='+ lim +' idx='+ idx +' len='+ len +' looping='+ looping);
   } while (lim && ((lt && f > lim) || (!lt && f < lim)));
   
   ale_2g_msg('w3-text-css-lime', ale.menu_s[ale.cur_menu] +', '+ ale.cur_header +': scanning '+ f.toFixedNZ(1) +' kHz');
   //console.log('ale_2g_scanner: '+ f_s +' idx_f='+ idx_f);
   ale_2g_tune(f, from_menu);

   kiwi_clearTimeout(ale.scan_timeout);
   ale.scan_timeout = setTimeout(function() {
      /*
      if (ale.tune_ack_got != ale.tune_ack_expecting) {
         console.log('tune_ack 1: got='+ ale.tune_ack_got +' expecting='+ ale.tune_ack_expecting);
         ale.scan_timeout = setTimeout(function() {
            console.log('tune_ack +: got='+ ale.tune_ack_got +' expecting='+ ale.tune_ack_expecting);
         }, ale.scan_t_ms);
      } else {
         ale.scan_cur_idx = idx;
         ale_2g_scanner(idx);
      }
      */
      ale.scan_cur_idx = idx;
      ale_2g_scanner(idx);
   }, ale.scan_t_ms);
}

function ale_2g_clear_button_cb(path, val, first)
{
   if (first) return;
   //console.log('ale_2g_clear_button_cb'); 
   ale.console_status_msg_p.s = encodeURIComponent('\f');
   kiwi_output_msg('id-ale_2g-console-msgs', 'id-ale_2g-console-msg', ale.console_status_msg_p);
   ale.log_txt = '';
   ale_2g_test_cb('', 0);
}

function ale_2g_scan_t_cb(path, idx, first)
{
   if (first) return;
   var scan_t_s = ale.scan_t_s[+idx];
   w3_set_value(path, idx);   // for benefit of direct callers
   var isCustom = (scan_t_s == 'custom');
   if (dbgUs) console.log('ale_2g_scan_t_cb idx='+ idx +' scan_t_s='+ scan_t_s +' isCustom='+ isCustom);
   var el_custom = w3_el('id-ale_2g-scan-t-custom');
   w3_show_hide(el_custom, isCustom);

   if (isCustom) {
	   ale_2g_scan_t_custom_cb(el_custom, w3_get_value(el_custom));
   } else {
      ale.scan_t_ms = parseFloat(scan_t_s) * 1000;
   }   
}

function ale_2g_scan_t_custom_cb(path, val)
{
	var scan_t = w3_clamp(parseFloat(val), 0.75, 60, 1);
   console.log('ale_2g_scan_t_custom_cb path='+ path +' val='+ val +' scan_t='+ scan_t);
	w3_set_value(path, scan_t);
	ale.scan_t_ms = scan_t * 1000;
	w3_show_block(path);
}

function ale_2g_f_limit_cb(path, idx, first)
{
   if (first) return;
   var f_limit_s = ale.f_limit_s[+idx];
   ale.f_sign = f_limit_s.includes('&le;')? -1:1;
   var isCustom = f_limit_s.includes('custom');
   console.log('ale_2g_f_limit_cb idx='+ idx +' f_limit_s='+ f_limit_s +' isCustom='+ isCustom);
	w3_set_value(path, +idx);
   var el_custom = w3_el('id-ale_2g-f-limit-custom');
   w3_show_hide(el_custom, isCustom);

   if (isCustom) {
	   ale_2g_f_limit_custom_cb(el_custom, w3_get_value(el_custom));
   } else {
      ale.f_limit = ale.f_limit_v[+idx] * 1000 * ale.f_sign;
   }
   console.log('MENU f_limit='+ ale.f_limit);
}

function ale_2g_f_limit_custom_cb(path, val)
{
   var f_limit_kHz = val.toString().parseFloatWithUnits('kM', 1e-3);
	f_limit_kHz = w3_clamp(f_limit_kHz, 1, 32000, 1);
   console.log('ale_2g_f_limit_custom_cb path='+ path +' val='+ val +' f_limit_kHz='+ f_limit_kHz);
	w3_set_value(path, f_limit_kHz);
	ale.f_limit = f_limit_kHz * ale.f_sign;
	w3_show_block(path);
   console.log('CUSTOM f_limit='+ ale.f_limit);
}

function ale_2g_display_cb(path, idx, first)
{
   if (first) return;
   idx = +idx;
   //console.log('ale_2g_display_cb path='+ path +' idx='+ idx);
   ale.dsp = idx;
	ext_send('SET display='+ idx);
	w3_set_value(path, idx);
}

function ale_2g_resamp_cb(path, idx, first)
{
   idx = +idx;
	ext_send('SET use_new_resampler='+ idx);
}

function ale_2g_test_cb(path, val, first)
{
   //if (+val == 0) kiwi_trace();
   if (first) return;
   if (ext_nom_sample_rate() != 12000) {     // our sample file is 12k only
      ale.testing = ale.periodic_self_test = false;
      return;
   }
   val = +val;
   if (dbgUs) console.log('ale_2g_test_cb: val='+ val);
	w3_el('id-ale_2g-bar').style.width = '0%';
   ale.testing = val;
   if (!val) ale.periodic_self_test = false;
   w3_show_hide('id-ale_2g-bar-container', ale.testing);
   w3_show_hide('id-ale_2g-record', !ale.testing);
   
   // pushing the test button in the middle of a scan activity stop
   // won't cause it to resume scanning immediately like it should
   if (val && !ale.scanning && ale.isActive && ale.scan_list) {
      ale.isActive = false;
      if (dbgUs) console.log('#### $active FORCED RESUME ignore_resume='+ ale.ignore_resume);
      if (!ale.ignore_resume) {
         ale_2g_scanner(ale.RESUME);   // resume scanning where we left off
         ext_send('SET reset');
      }
      ale.ignore_resume = false;
   }

   // positive value gates test audio to a specific freq
   // negative value applies test audio at all times
   //var f = '24000.60';
   var f = -(+ext_get_freq_kHz()).toFixed(2);
   if (dbgUs) console.log('ale_2g_test_cb='+ (val? f : 0));
	ext_send('SET test='+ (val? f : 0));
}

function ale_2g_record_cb(path, idx, first)
{
   if (first) return;
   idx = +idx;
   console.log('ale_2g_record_cb idx='+ idx);
   ale.record_m = idx;
	w3_set_value(path, idx);
	ale_2g_log_mins_cb('', ale.log_mins);
}

function ale_2g_record_secs_cb(path, val)
{
   ale.record_secs = w3_clamp(+val, 1, 24*60*60, 1);
   console.log('ale_2g_record_secs_cb path='+ path +' val='+ val +' record_secs='+ ale.record_secs);
	w3_set_value(path, ale.record_secs);
}

function ale_2g_log_mins_cb(path, val)
{
   ale.log_mins = w3_clamp(+val, 1, 24*60, 1);
   console.log('ale_2g_log_mins_cb path='+ path +' val='+ val +' log_mins='+ ale.log_mins);
	w3_set_value(path, ale.log_mins);

   kiwi_clearInterval(ale.log_interval);
   if (ale.record_m & ale.LOG) ale.log_interval = setInterval(function() { ale_2g_log_cb(); }, ale.log_mins * 60000);
}

function ale_2g_log_cb()
{
   var txt = new Blob([ale.log_txt], { type: 'text/plain' });
   var a = document.createElement('a');
   a.style = 'display: none';
   a.href = window.URL.createObjectURL(txt);
   a.download = kiwi_timestamp_filename('ALE_2G.', '.log.txt');
   document.body.appendChild(a);
   console.log('ale_2g_log: '+ a.download);
   a.click();
   window.URL.revokeObjectURL(a.href);
   document.body.removeChild(a);
}

// automatically called on changes in the environment
function ALE_2G_environment_changed(changed)
{
   //w3_console.log(changed, 'ALE_2G_environment_changed');

   if (changed.freq || changed.mode) {
      var f_kHz = ext_get_freq_kHz();
      var mode = ext_get_mode();
      //console.log('ALE_2G_environment_changed freq='+ ale.freq +' f_kHz='+ f_kHz);
	   if (ale.freq != f_kHz || (mode != 'lsb' && mode != 'usb')) {
	      //console.log('ale_2g_clear_menus()');
	      ale_2g_clear_menus();
	   }
   }

   if (changed.resize) {
      var el = w3_el('id-ale_2g-data');
      var left = (window.innerWidth - 1024 - time_display_width()) / 2;
      el.style.left = px(left);
   }
}

function ALE_2G_focus()
{
   ale.perodic_test = setInterval(
      function() {
         console.log('ale_2g: periodic test');
         ale_2g_decoder_output_chars('[periodic self test]\n');
         ale.periodic_self_test = true;
         w3_el('id-ale_2g-test').click();
      }, 4*60*60*1000   // every 4 hours
      //}, 45*1000,
   );
}

function ALE_2G_blur()
{
   // anything that needs to be done when extension blurred (closed)
   //kiwi_clearInterval(ale.watchdog);
   ale_2g_scanner(ale.STOP, null);
	ext_send('SET stop');
	console.log('ALE_2G_blur saved_mode='+ ale.saved_mode);
	ext_set_mode(ale.saved_mode);
   kiwi_clearInterval(ale.log_interval);
   kiwi_clearInterval(ale.perodic_test);
}

function ALE_2G_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'ALE 2G decoder help') +
         w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
            w3_div('w3-margin-R-8',
               'The menu content is fetched from ' +
               '<a href="http://kiwisdr.com/ale/ALE_nets.cjson" target="_blank">kiwisdr.com</a> each time the extension is opened.' +
               '<br><br>' +
               'Because the menus can be long (many frequency entries) the <i>format</i> setting can be used to collapse them ' +
               'to show just their net names followed by the <i>scan</i> entry (or single frequency). ' +
               'Menus can also be sorted by high-to-low frequency since this is optimal for some types of ALE monitoring.' +
               '<br><br>' +
               'The <i>display</i> setting controls the ALE message detail shown. ' +
               'The <i>DX</i> setting limits the information displayed per ALE transaction. ' +
               'The <i>scan time</i> setting has preset entries and supports custom scan rates from ' +
               ' 0.75 to 60 secs.' +
               '<br><br>' +
               'The frequencies of a scan list can be limited by the <i>frequency limit</i> setting. ' +
               'This is useful when the scan list covers a wide range of HF frequencies but propagation ' +
               'makes scanning some of them pointless (e.g. > 12 MHz at night).' +
               '<br><br>' +
               '<i>record</i> settings: Automatic audio recording can be setup when an ALE call between two stations is established. ' +
               'Also periodic downloading of the ALE message log to a file. Adjust your browser settings so these files are downloaded ' +
               'and saved automatically without causing a browser popup window for each download.' +
               '<br><br>' +
               'The Kiwi owner/admin can define the contents of the <i>Local</i> menu on the <br>' +
               '<i>Admin > Extensions > ALE_2G</i> page. JSON format is used. Users cannot currently define '+
               'their own menus (except via URL parameters, see below) but suggestions for the downloaded menus on extension startup can be made on the Kiwi forum.' +
         
               '<br><br>URL parameters: <br>' +
               w3_text('|color:orange', '(menu match or frequency list) &nbsp; lsb &nbsp; format:[<i>0123</i>] &nbsp; display:[<i>0123</i>] &nbsp; scan[:<i>secs</i>] &nbsp; <br>' +
               'limit_le:<i>freq</i> &nbsp; limit_ge:<i>freq</i> &nbsp; rec:[<i>0123</i>] &nbsp; rec_time:<i>secs</i> &nbsp; log_time:<i>mins</i> &nbsp; test') +
               '<br><br>' +
               'The first URL parameter can be a frequency entry from one of the menus (e.g. "3596") ' +
               'or the name of a menu scan list (e.g. "MARS" in the Amateur menu). ' +
               'Or it can be a list of frequencies separated by commas. Such a list will appear as the first entry in the ' +
               '<i>Local</i> menu for subsequent selection. The parameter <i>lsb</i> will make this list use lsb mode.' +
               '<br><br>' +
               'Frequencies can use the suffixes \'k\' and \'M\'. [0123] refers to the order of selections in the corresponding menu.' +
               '<br><br>' +
               'Keywords are case-insensitive and can be abbreviated. So for example these are valid: <br>' +
               '<i>ext=ale,3596</i> &nbsp;&nbsp; ' +
               '<i>ext=ale,mars,scan</i> &nbsp;&nbsp; <i>ext=ale,cothen,s:0.75</i> &nbsp;&nbsp; <i>ext=ale,4M,5M,6M,lsb</i> <br>' +
               '<i>ext=ale,7102,14.1M,18106,s</i> &nbsp;&nbsp; <i>ext=ale,ham,s,disp:1,limit_le:10M,rec,rec_t:10</i><br>' +
               '<i>ext=ale,mars,scan,rec:3,rec_t:60,log_t:10</i> &nbsp; (i.e. <i>rec:3</i> is the "r+l" record menu entry)<br>' +
               ''
            )
         );

      confirmation_show_content(s, 610, 375);
      w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
   }
   return true;
}

function ale_2g_admin_menu_save_cb(path)
{
   //console.log('ale_2g_admin_menu_save_cb');
   var el = w3_el('id-cfg.ale_2g.admin_menu');
   //console.log('val='+ el.value);
   ale_2g_admin_menu_cb('cfg.ale_2g.admin_menu', el.value);
   w3_schedule_highlight(el);
}

function ale_2g_admin_menu_cb(path, val)
{
   //console.log('ale_2g_admin_menu_cb path='+ path +' val='+ JSON.stringify(val));
   var el = w3_el('id-ale_2g-admin-textarea');
   w3_set_value(path, val);
   
   var s, o, json_err, json_msg;
   try {
      s = kiwi_remove_cjson_comments(val);
      o = JSON.parse(s);
      json_err = false;
   } catch(ex) {
      console.log('JSON parse error');
      json_msg = ex.message;
      console.log(ex.message);
      //console.log(ex);
      console.log(s);
      json_err = true;
   }
   w3_remove_then_add(el, 'w3-css-pink', json_err? 'w3-css-pink' : '');
   w3_innerHTML('id-ale_2g-admin-textarea-status', json_err? json_msg : '');
   
   if (!json_err) {
      //console.log(o);
      w3_json_set_cfg_cb(path, val);
   }
}

// called to display HTML for configuration parameters in admin interface
function ALE_2G_config_html()
{
   var s =
      w3_div('w3-container',
         w3_textarea_get_param('id-ale_2g-admin-textarea w3-input-any-change|width:100%',
            w3_inline('',
               w3_text('w3-bold w3-text-teal', 'Admin menu'),
               w3_text('w3-text-black w3-margin-left', 'Data in JSON format.'),
               w3_button('w3-margin-left w3-aqua', 'Save', 'ale_2g_admin_menu_save_cb')
            ),
            'cfg.ale_2g.admin_menu', 16, 100, 'ale_2g_admin_menu_cb',
         
            '{\n' +
            '// Data in JSON format.\n' +
            '// Double-slash comments MUST start in column one!\n' +
            '// Underscores in net/station names are converted to line breaks in menu entries.\n' +
            '// A zero as the first element of the frequency array converts to a "scan" menu entry.\n' +
            '// I.e. if a net consists of a single frequency the zero would be left out as there is nothing to scan.\n' +
            '// "lsb" as the second element of the frequency array selects LSB mode for all entries.\n' +
            '\n' +
            '    "Admin-1": [1111],\n' +
            '    "Admin-2": [0, 2222, 3333, 4444],\n' +
            '    "Admin-3_Test": [0, 5555.5, 6666.6],\n' +
            '    "Admin-LSB": [0, "lsb", 7777.7, 8888.8]\n' +
            '}\n'
         ),
         
         w3_inline('w3-valign w3-margin-top/', 
            w3_text('w3-text-teal', 'Status:'),
            w3_div('id-ale_2g-admin-textarea-status w3-margin-left w3-text-black w3-background-pale-aqua', '')
         ),
         w3_text('w3-margin-top w3-text-black', 'Note: Line numbers in JSON.parse error messages do not include comment lines. Adjust accordingly.')
      );

   ext_config_html(ale, 'ale_2g', 'ALE_2G', 'ALE_2G configuration', s);
}
