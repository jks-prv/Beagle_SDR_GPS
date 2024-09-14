// Copyright (c) 2024 John Seamons, ZL4VO/KF6VO

var ft8 = {
   ext_name: 'FT8',     // NB: must match FT8.cpp:ft8_ext.name
   first_time: true,
   focus_interval: null,
   
   callsign: '',
   grid: '',
   
   PASSBAND_LO: 100,
   PASSBAND_HI: 3100,

   mode: 0,
   FT8: 0,
   FT4: 1,
   mode_s: ['FT8', 'FT4'],

   // yes, there are really no assigned FT4 freqs for 160m and 60m
   freq_s: {
      'FT8': [ '1840', '3573', '5357', '7074', '10136', '14074', '18100', '21074', '24915', '28074' ],
      'FT4': [ '3575.5', '7047.5', '10140', '14080', '18104', '21140', '24919', '28180' ]
   },

   freq_xvtr_s: {
      'FT8': [ '40680', '50313', '50323', '70154', '70190', '144174', '222065','432174', '1296174', '10489540' ],
      'FT4': [ '50318', '144150' ]
   },

   autorun_u: {
          0: [ 'regular use' ],  // NB: using a numeric key (zero) suppresses the disabled menu entry
      'FT8': [ '1840', '3573', '5357', '7074', '10136', '14074', '18100', '21074', '24915', '28074',
               '40680', '50313', '50323', '70154', '70190', '144174', '222065','432174',
               '1296174', '10489540'
             ],
      'FT4': [ '3575.5', '7047.5', '10140', '14080', '18104', '21140', '24919', '28180',
               '50318', '144150'
             ]
   },

   // translates menu order to cfg value which then has to match order of FT8.cpp:ft8_cfs[]
   // assign new cfg values so as not to disturb existing values stored in cfg
   menu_i_to_cfg_i: [
      0,    //  0 regular use
      
   // +-- cfg value (no renumbering or reuse!)
   // |         +-- menu seq value (use this in FT8.cpp:ft8_autorun_dial)
   // |         |
      -1,   //  1 FT8 label
      1,    //  2 160m
      2,    //  2 80m
      3,    //  3 60m
      4,    //  4 40m
      5,    //  5 30m
      6,    //  6 20m
      7,    //  7 17m
      8,    //  8 15m
      9,    //  9 12m
      10,   // 10 10m

      19,   // 11 8m
      20,   // 12 6m
      21,   // 13 6m
      22,   // 14 4m
      23,   // 15 4m
      24,   // 16 2m
      25,   // 17 220 1.25m
      26,   // 18 440  70cm
      27,   // 19 1296 23cm
      30,   // 20 10G   3cm QO-100
      
      -1,   // 21 FT4 label
      11,   // 22 80m
      12,   // 23 40m
      13,   // 24 30m
      14,   // 25 20m
      15,   // 26 17m
      16,   // 27 15m
      17,   // 28 12m
      18,   // 29 10m
      
      28,   // 30 6m
      29,   // 31 2m
   ],

   PREEMPT_NO: 0,
   PREEMPT_YES: 1,
   preempt_u: ['no', 'yes'],

   sched_u: [ 'continuous',
      '00:00', '01:00', '02:00', '03:00', '04:00', '05:00', '06:00', '07:00', '08:00', '09:00', '10:00', '11:00',
      '12:00', '13:00', '14:00', '15:00', '16:00', '17:00', '18:00', '19:00', '20:00', '21:00', '22:00', '23:00',
   ],

   // must set "remove_returns" so output lines with \r\n (instead of \n alone) don't produce double spacing
   console_status_msg_p: {
      no_decode: true, scroll_only_at_bottom: true, process_return_alone: false, remove_returns: true,
      cols: 135, max_lines: 1024
   },

   log_mins: 0,
   log_interval: null,
   log_txt: '',

   last_last: 0
};

function FT8_main()
{
	ext_switch_to_client(ft8.ext_name, ft8.first_time, ft8_recv);     // tell server to use us (again)
	if (!ft8.first_time)
		ft8_controls_setup();
	ft8.first_time = false;
}

function ft8_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('ft8_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (isDefined(param[1]))
				console.log('ft8_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('ft8_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				ft8_controls_setup();
				break;

			case "chars":
				ft8_output_chars(param[1]);
				break;

			case "debug":
            if (dbgUs) console.log(kiwi_decodeURIComponent('FT8', param[1]));
				break;

			default:
				console.log('ft8_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function ft8_output_chars(c)
{
   c = kiwi_decodeURIComponent('FT8', c);    // NB: already encoded on C-side
   //ft8.log_txt += kiwi_remove_escape_sequences(c);

   var a = c.split('');
   a.forEach(function(ch, i) {
      if (ch == '<') a[i] = kiwi.esc_lt;
      else
      if (ch == '>') a[i] = kiwi.esc_gt;
   });
   ft8.console_status_msg_p.s = a.join('');
   //console.log(ft8.console_status_msg_p);
   kiwi_output_msg('id-ft8-console-msgs', 'id-ft8-console-msg', ft8.console_status_msg_p);
}

function ft8_controls_setup()
{
	ft8.saved_setup = ext_save_setup();
   ext_tune(null, 'usb', ext_zoom.ABS, 11, ft8.PASSBAND_LO, ft8.PASSBAND_HI);

   var data_html =
      time_display_html('ft8') +
      
      w3_div('id-ft8-data|left:150px; width:1044px; height:300px; overflow:hidden; position:relative; background-color:mediumBlue;',
			w3_div('id-ft8-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|width:1024px; height:300px; position:absolute; overflow-x:hidden;',
			   w3_code('id-ft8-console-msgs w3-text-output-striped/')
			)
      );
   
   var callsign = ext_get_cfg_param_string('ft8.callsign') || '';
   ft8.callsign = callsign;
   if (callsign == '') callsign = '(not set)';

   var grid = ext_get_cfg_param_string('ft8.grid') || '';
   ft8.grid = grid;
   if (grid == '') grid = '(not set)';
   
   // re-define band menu if downconverter in use
   var r = ext_get_freq_range();
   if (r.lo_kHz > 32000 && r.hi_kHz > 32000) {
      new_s = {};
      w3_obj_enum(ft8.freq_xvtr_s,
         function(key, i, o) {
            var a = kiwi_deep_copy(o);
            for (var j = 0; j < a.length; j++) {
               var f_kHz = +a[j];
               if (f_kHz < r.lo_kHz || f_kHz > r.hi_kHz) {
                  a[j] = undefined;
               }
            }
            console.log(a);
            new_s[key] = kiwi_array_remove_undefined(a);
         }
      );
      ft8.freq_s = new_s;
   }

   var url =
      'https://pskreporter.info/pskmap.html?preset'+ ((ft8.callsign != '')? ('&callsign='+ ft8.callsign) : '') +
      '&txrx=rx&mode=FT8&timerange=86400&blankifnone=1&hidepink=1&hidelight=1&showlines=1&mapCenter=35,13,2';

	var controls_html =
		w3_div('id-ft8-controls w3-text-white',
			w3_divs('',
            w3_col_percent('w3-valign/',
               w3_div('',
				      w3_div('w3-medium w3-text-aqua', '<b>FT8/FT4 decoder</b>')
				   ), 40,
					w3_div('', 'From <b><a href="https://github.com/kgoba/ft8_lib/tree/update_to_0_2" target="_blank">ft8_lib</a></b> Karlis Goba &copy; 2018'), 45
				),
				w3_div('id-ft8-err w3-margin-T-10 w3-padding-small w3-css-yellow w3-width-fit w3-hide'),
				w3_inline('id-ft8-container w3-margin-T-6/w3-margin-between-8',

               w3_div('',
                  w3_inline('/w3-margin-between-16',
                     w3_select_hier('id-ft8-freq w3-text-red w3-width-auto', '', 'freq', 'ft8.freq_idx', -1, ft8.freq_s, 'ft8_freq_cb'),
                     w3_select('w3-text-red', '', 'mode', 'ft8.mode', ft8.FT8, ft8.mode_s, 'ft8_mode_cb')
                  ),
                  w3_div('w3-margin-T-4',
                     w3_link('w3-bold', url, 'pskreporter.info')
                  )
               ),

               w3_div('cl-ft8-text', 'reporter call '+ callsign),
               w3_div('id-ft8-rgrid cl-ft8-text', 'reporter grid '+ grid + (cfg.ft8.GPS_update_grid? ' (GPS)':'')),
               w3_button('w3-padding-smaller w3-css-yellow', 'Clear', 'ft8_clear_button_cb'),
               (dbgUs? w3_button('w3-padding-smaller w3-aqua', 'Test', 'ft8_test_cb') : '')
            )
			)
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('ft8');

   ext_set_data_height(300);
	ext_set_controls_width_height(525, 90);
   ft8_clear_button_cb();

	if (ext_nom_sample_rate() != 12000) {
	   w3_hide2('id-ft8-container', true);
	   w3_hide2('id-ft8-err', false);
	   w3_innerHTML('id-ft8-err',
	      'FT8 extension only works on Kiwis configured for 12 kHz wide channels');
	} else {
	   ext_send('SET ft8_start='+ ft8.FT8 +' debug='+ (dbgUs? 1:0));
	}

	ft8.url_params = ext_param();
	var p = ft8.url_params;
   if (p) {
      p = p.split(',');
      var do_test = 0;
      p.forEach(function(a, i) {
         if (w3_ext_param('test', a).match) {
            do_test = 1;
         }
         if (w3_ext_param('help', a).match) {
            ext_help_click();
         }
      });

      // check first URL param matching entry in the freq menu
      var found = false;
      var freq = parseFloat(ft8.url_params);
      if (!isNaN(freq)) {
         w3_select_enum('id-ft8-freq', function(option) {
            //console.log('CONSIDER '+ parseFloat(option.innerHTML));
            if (!found && parseFloat(option.innerHTML) == freq) {
               ft8_freq_cb('id-ft8-freq', option.value, false);
               found = true;
            }
         });
      }

      if (dbgUs && do_test) ft8_test_cb();
   }
}

function FT8_focus()
{
   ft8_check_GPS_update_grid();
   ft8.focus_interval = setInterval(function() { ft8_check_GPS_update_grid(); }, 10000);
}

function FT8_blur()
{
   kiwi_clearInterval(ft8.focus_interval);
}

function ft8_freq_cb(path, idx, first)
{
	if (first) return;
   idx = +idx;
	var menu_item = w3_select_get_value('id-ft8-freq', idx);
	var freq = +(menu_item.option);
	var r = ext_get_freq_range();
	var fo_kHz = freq - r.offset_kHz;
   ext_tune(fo_kHz, 'usb', ext_zoom.ABS, 11);
   var mode = menu_item.last_disabled;
   w3_select_value(path, idx);   // for benefit of direct callers
	//console.log('ft8_freq_cb: path='+ path +' idx='+ idx +' fo_kHz='+ fo_kHz +' mode='+ mode);
	
	if (mode != ft8.mode_s[ft8.mode]) {
	   ft8.mode ^= 1;
	   ft8_mode_cb('ft8.mode', ft8.mode);
	   //console.log('ft8_freq_cb: changing mode to '+ ft8.mode);
	}
}

function ft8_mode_cb(path, idx, first)
{
	if (first) return;
	//console.log('ft8_mode_cb: idx='+ idx);
   idx = +idx;
	w3_set_value(path, idx);
   ft8.mode = idx? ft8.FT4 : ft8.FT8;
	ext_send('SET ft8_protocol='+ ft8.mode);
   //console.log('ft8_mode_cb: changing mode to '+ ft8.mode);
}

function ft8_clear_button_cb(path, idx, first)
{
   if (first) return;
   ft8.console_status_msg_p.s = '\f';
   kiwi_output_msg('id-ft8-console-msgs', 'id-ft8-console-msg', ft8.console_status_msg_p);
   ft8.log_txt = '';
}

function ft8_test_cb()
{
   ext_send('SET ft8_test');
}


////////////////////////////////
// admin
////////////////////////////////

function ft8_input_grid_cb(path, val, first, cb_a)
{
   val = val.trim();
   var from_gps = (cb_a[1] && cb_a[1] == 'gps')? true : false;
   //console.log('ft8_input_grid_cb val='+ val +' from_gps='+ from_gps);
   
	// don't update cfg from a continuous GPS update -- only show in field
   if (!from_gps)
	   w3_string_set_cfg_cb(path, val);

	w3_set_value(path, val);

	// need this because ft8_check_GPS_update_grid() runs asynch of server sending updated value via 10s status
	ft8.grid = val;
}

// called to display HTML for configuration parameters in admin interface
function FT8_config_html()
{
   var s =
      w3_div('w3-show-inline-block w3-width-full',
         w3_col_percent('w3-container/w3-margin-bottom',
            w3_input_get('w3-restart', 'Reporter callsign', 'ft8.callsign', 'w3_string_set_cfg_cb', ''), 32,
            '', 3,
            w3_div('',
               w3_inline('w3-halign-space-between/',
                  w3_label('w3-bold', 'Reporter grid square '),
                  w3_button('id-ft8-grid-set cl-admin-check w3-blue w3-btn w3-round-large w3-margin-B-2 w3-hide', 'set from GPS')
               ),
               w3_input_get('', '', 'ft8.grid', 'ft8_input_grid_cb', '', '6-character grid square location'
               )
            ), 30,
            '', 3,
            w3_input_get('', 'SNR correction', 'ft8.SNR_adj', 'w3_num_set_cfg_cb', ''), 12,
            '', 3,
            w3_input_get('', 'dT correction', 'ft8.dT_adj', 'w3_num_set_cfg_cb', ''), 12
         ),

         w3_col_percent('w3-container w3-margin-T-8 w3-margin-B-16/',
            w3_divs('w3-center w3-tspace-8',
               w3_switch_label('w3-center', 'Update grid continuously<br>from GPS?', 'Yes', 'No', 'cfg.ft8.GPS_update_grid', cfg.ft8.GPS_update_grid, 'ft8_GPS_update_grid_cb'),
               w3_text('w3-text-black w3-center',
                  'Useful for Kiwis in motion <br> (e.g. marine mobile)'
               )
            ), 23,
            '&nbsp;', 3,
            w3_divs('w3-center w3-tspace-8',
               w3_switch_label('w3-center', 'Log decodes to<br>syslog?', 'Yes', 'No', 'ft8.syslog', cfg.ft8.syslog, 'admin_radio_YN_cb'),
               w3_text('w3-text-black w3-center',
                  'Use with care as over time <br> filesystem can fill up.'
               )
            ), 23
         ),

         '<hr>',
         w3_div('w3-container',
            w3_div('', '<b>Autorun</b>'),
            w3_div('w3-container',
               w3_div('w3-text-black', 'On startup automatically begins running the FT8 decoder on the selected band(s).<br>' +
                  'Channels available for regular use are reduced by one for each FT8 autorun enabled.<br>' +
                  'If Kiwi has been configured for a mix of channels with and without waterfalls then channels without waterfalls will be used first.<br><br>' +
                  
                  'Spot decodes are available in the Kiwi log (use "Log" tab above) and are listed on <a href="https://pskreporter.info/pskmap.html" target="_blank">pskreporter.info</a><br>' +
                  'The "Reporter" fields above must be set to valid values for proper spot entry into the <a href="https://pskreporter.info/pskmap.html" target="_blank">pskreporter.info</a> database.'),
               
               w3_div('w3-margin-T-10 w3-valign',
                  '<header class="id-ft8-warn-full w3-container w3-yellow"><h6>' +
                  'If your Kiwi is publicly listed you must <b>not</b> configure all the channels to use FT8-autorun!<br>' +
                  '(unless at least one is set to preemptable) This defeats the purpose of making your Kiwi <br>' +
                  'public and public registration will be disabled until you make at least one channel available <br>' +
                  'for connection. Check the Admin Public tab.' +
                  '</h6></header>'
               ),
               
               w3_inline('w3-margin-top/',
                  w3_button('w3-blue', 'set all to regular use', 'ft8_autorun_all_regular_cb'),
                  w3_button('id-ft8-restart w3-margin-left w3-aqua w3-hide', 'autorun restart', 'ft8_autorun_restart_cb')
               ),
               
               w3_div('id-ft8-admin-autorun w3-margin-T-16')
            )
         )
      );

   ext_config_html(ft8, 'ft8', 'FT8', 'FT8/FT4 configuration', s);

	s = '';
	for (var i=0; i < rx_chans;) {
	   var s2 = '';
      var f1 = 'w3-margin-right w3-defer';
      var f2 = f1 +' w3-margin-T-8';
	   for (var j=0; j < 8 && i < rx_chans; j++, i++) {
	      var arun = w3_array_el_seq(ft8.menu_i_to_cfg_i, +cfg.ft8['autorun'+ i]);
	      //console.log('ft8.autorun'+ i +'='+ cfg.ft8['autorun'+ i] +' arun='+ arun);
	      s2 +=
	         w3_div('',
	            w3_select_hier(f1, '', 'freq', 'ft8.autorun'+ i, arun, ft8.autorun_u, 'ft8_autorun_select_cb'),
	            w3_select_get_param(f2, '', 'preemptable?', 'ft8.preempt'+ i, ft8.preempt_u, 'ft8_preempt_select_cb')
	            //w3_select_get_param(f2, '', 'start UTC', 'ft8.start'+ i, ft8.sched_u, 'ft8_autorun_sched_cb', 0, 0),
	            //w3_select_get_param(f2, '', 'stop UTC', 'ft8.stop'+ i, ft8.sched_u, 'ft8_autorun_sched_cb', 0, 1)
	         );
	   }
	   s += w3_inline('w3-margin-bottom/', s2);
	}
	w3_innerHTML('id-ft8-admin-autorun', s);
}

function ft8_GPS_update_grid_cb(path, idx, first)
{
	var enabled = w3_switch_idx2val(+idx);
	//console.log('ft8_GPS_update_grid_cb: first='+ first +' enabled='+ enabled);

	if (!first) {
	   ext_send("ADM get_gps_info");    // NB: must be sent as ADM command
	}
	
	admin_bool_cb(path, enabled, first);
}

function ft8_autorun_public_check()
{
   var num_autorun = 0;
	for (var i=0; i < rx_chans; i++) {
	   if (cfg.ft8['autorun'+ i] != 0 && cfg.ft8['preempt'+ i] == ft8.PREEMPT_NO)
	      num_autorun++;
	}
	if (cfg.ft8.autorun != num_autorun)
	   ext_set_cfg_param('ft8.autorun', num_autorun, EXT_SAVE);
	
	var full = (adm.kiwisdr_com_register && num_autorun >= rx_chans);
   w3_remove_then_add_cond('id-ft8-warn-full', full, 'w3-red', 'w3-yellow');
	if (full) {
      kiwisdr_com_register_cb('adm.kiwisdr_com_register', w3_SWITCH_NO_IDX);
	}
}

function ft8_autorun_restart_cb()
{
   //console.log('ft8_autorun_restart_cb');
   ft8_autorun_public_check();
   w3_hide('id-ft8-restart');
   ext_send("ADM ft8_autorun_restart");  // NB: must be sent as ADM command
}

function ft8_autorun_select_cb(path, idx, first)
{
   //console.log('ft8_autorun_select_cb path='+ path +' idx='+ idx +' cfg_i='+ ft8.menu_i_to_cfg_i[+idx]);
   admin_select_cb(path, ft8.menu_i_to_cfg_i[+idx], first);
   w3_select_value(path, +idx);
   if (first) return;
   w3_show('id-ft8-restart');
	var el = w3_el('id-kiwi-container');
	w3_scrollDown(el);   // keep menus visible
}

function ft8_preempt_select_cb(path, idx, first)
{
   //console.log('ft8_preempt_select_cb: path='+ path +' idx='+ idx +' first='+ first);
   admin_select_cb(path, idx, first);
   if (first) return;
   w3_show('id-ft8-restart');
	var el = w3_el('id-kiwi-container');
	w3_scrollDown(el);   // keep menus visible
}

function ft8_autorun_sched_cb(path, idx, first, cbp)
{
   //console.log('ft8_autorun_sched_cb path='+ path +' idx='+ idx +' cbp='+ cbp +' first='+ first);
}

function ft8_autorun_all_regular_cb(path, idx, first)
{
   if (first) return;
   for (var i = 0; i < rx_chans; i++) {
      path = 'ft8.autorun'+ i;
      w3_select_value(path, 0);
      admin_select_cb(path, 0, /* first: true => no save */ true);
   }
   ext_set_cfg_param('ft8.autorun', 0, EXT_SAVE);
   w3_show('id-ft8-restart');
	var el = w3_el('id-kiwi-container');
	w3_scrollDown(el);   // keep menus visible
}

function FT8_config_focus()
{
   //console.log('ft8_config_focus');
   ft8_autorun_public_check();
   ft8_check_GPS_update_grid();
   ft8.focus_interval = setInterval(function() { ft8_check_GPS_update_grid(); }, 10000);

   var el = w3_el('id-ft8-grid-set');
	if (el) el.onclick = function() {
	   ft8.single_shot_update = true;
	   ext_send("ADM get_gps_info");    // NB: must be sent as ADM command
	};
}

function FT8_config_blur()
{
   //console.log('FT8_config_blur');
   kiwi_clearInterval(ft8.focus_interval);
}

// should be okay that this is called via a timeout from both admin and user contexts
function ft8_check_GPS_update_grid()
{
   var auto = kiwi.GPS_auto_grid;
   var ok = isNonEmptyString(auto);
   //console.log('ft8_check_GPS_update_grid '+ (isAdmin()? 'ADMIN' : 'USER') +' GPS_update_grid='+ cfg.ft8.GPS_update_grid +' kiwi.GPS_auto_grid='+ auto +' w3_get_value(ft8.grid)='+ w3_get_value('ft8.grid') +' cfg.ft8.grid='+ cfg.ft8.grid);

   if (cfg.ft8.GPS_update_grid && ok && ft8.grid != auto) {
      ft8.grid = auto;
      if (isAdmin()) {
         w3_set_value('id-ft8.grid', ft8.grid);
         w3_input_change('ft8.grid', 'ft8_input_grid_cb', 'gps');
      } else {
         w3_innerHTML('id-ft8-rgrid', 'reporter grid<br>'+ ft8.grid + (auto? ' (GPS)':''));
      }
      //console.log('ft8_check_GPS_update_grid SET ft8.grid='+ ft8.grid);
   }

   if (isAdmin()) {
      if (kiwi.GPS_fixes) w3_show_inline_block('id-ft8-grid-set');
      ft8.single_shot_update = false;
   }
}

// called from receipt of an "ADM get_gps_info_cb" in admin_recv()
// which is the callback to us sending an on-demand "ADM get_gps_info" above
function FT8_gps_info_cb()
{
   //console.log('FT8_gps_info_cb');
   if (!cfg.ft8.GPS_update_grid && !ft8.single_shot_update) return;
   ft8.grid = kiwi.GPS_auto_grid;
   w3_set_value('id-ft8.grid', ft8.grid);
   w3_input_change('ft8.grid', 'ft8_input_grid_cb', 'gps');    // for w3-restart
   ft8.single_shot_update = false;
}

function FT8_blur()
{
	ext_set_data_height();     // restore default height
	ext_restore_setup(ft8.saved_setup);
	ext_send('SET ft8_close');
   kiwi_clearInterval(ft8.log_interval);
}

function FT8_help(show)
{
   if (show) {
      var s =
         w3_text('w3-medium w3-bold w3-text-aqua', 'FT8/FT4 decoder help') +
         '<br>Spots are uploaded to pskreporter.info if the <i>reporter call</i> and <i>reporter grid</i> ' +
         'fields on the admin page, extensions tab, FT8 subtab have valid entries. ' +
         'Leave the callsign field blank if you do not want any uploads to pskreporter.info ' +
         'But consider leaving the grid field set so the km distance from the Kiwi to the ' +
         'spot will be shown.<br><br>' +
         
         'Uploaded spots are highlighted in green. Spots are only uploaded once every 60 minutes. ' +
         'The <i>age</i> column shows, in minutes, how long it has been since the last upload. ' +
         '<br>SNR information is currently not uploaded as it is not accurate.<br><br>' +
         
         'Clicking the <i>pskreporter.info</i> link will take you directly to the map with the ' +
         'reporter callsign of the Kiwi preset.<br><br>' +
         
         'URL parameters:<br>' +
         'The first parameters can select one of the entries in the <i>freq</i> menu<br>' +
         'e.g. <i>my_kiwi:8073/?ext=ft8,10136</i>' +
         '';
      confirmation_show_content(s, 610, 300);
   }
   return true;
}
