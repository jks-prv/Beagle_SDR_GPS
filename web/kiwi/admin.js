// Copyright (c) 2016 John Seamons, ZL/KF6VO

// TODO
//		input range validation
//		NTP status?


////////////////////////////////
// status
////////////////////////////////

function status_html()
{
	var s =
	w3_divs('id-status w3-hide', '',
		'<hr>' +
		w3_divs('id-problems w3-container') +
		w3_divs('id-msg-config w3-container') +
		w3_divs('id-msg-gps w3-container') +
		'<hr>' +
		w3_divs('id-info-1 w3-container') +
		w3_divs('id-info-2 w3-container') +
		'<hr>' +
		w3_divs('id-msg-status w3-container') + 
		w3_divs('id-status-dpump-resets w3-container') + 
		w3_divs('w3-container', '',
		   w3_div('id-status-dpump-hist w3-inline') +
         w3_button('w3-aqua|margin-left:10px', 'Reset', 'status_dpump_hist_reset_cb')
      ) +
      '<hr>' +
		w3_divs('id-debugdiv w3-container')
	);
	return s;
}

function status_dpump_hist_reset_cb(id, idx)
{
	ext_send('SET dpump_hist_reset');
}

function status_user_kick_cb(id, idx)
{
   console.log('status_user_kick_cb='+ idx);
	ext_send('SET user_kick='+ idx);
}


////////////////////////////////
// control
////////////////////////////////

function control_html()
{
	var s1 =
		'<hr>' +
		w3_half('', 'w3-container w3-vcenter',
			w3_div('',
            '<b>Enable user connections?</b> ' +
            w3_switch('Yes', 'No', 'adm.server_enabled', adm.server_enabled, 'server_enabled_cb')
			),
			w3_div('',
				'<b>Close all active user connections </b> ' +
				w3_button('w3-red', 'Kick', 'control_user_kick_cb')
			)
		) +
		w3_divs('w3-container w3-margin-top', '',
			w3_input_get_param('Reason if disabled', 'reason_disabled', 'reason_disabled_cb', '', 'will be shown to users attempting to connect')
		);
	
	var s2 =
		w3_divs('w3-margin-top', 'w3-container',
			'<label><b>Reason HTML preview</b></label>',
			w3_divs('', 'id-reason-disabled-preview w3-text-black w3-background-pale-aqua', '')
		) +
		'<hr>' +
		w3_half('w3-vcenter', '',
         w3_div('',
            w3_div('',
               w3_button('w3-aqua w3-margin', 'KiwiSDR server restart', 'admin_restart_cb'),
               w3_button('w3-blue w3-margin', 'Beagle reboot', 'admin_reboot_cb'),
               w3_button('w3-red w3-margin', 'Beagle power off', 'admin_power_off_cb')
            ),
            w3_divs('id-confirm w3-vcenter w3-hide', '',
               w3_button('w3-bright-yellow w3-margin', 'Confirm', 'admin_confirm_cb')
            )
         ),
			w3_div('w3-container w3-center',
            '<b>Daily restart?</b> ' +
            w3_switch('Yes', 'No', 'adm.daily_restart', adm.daily_restart, 'admin_radio_YN_cb'),
				w3_divs('', 'w3-text-black',
					"Set if you're having problems with the server<br>after it has run for a period of time.<br>" +
					"Restart occurs at the same time as updates (0200 UTC)<br> and will wait until there are no connections."
				)
			)
      );

   return w3_divs('id-control w3-text-teal w3-hide', '', s1 + s2);
}

function control_focus()
{
	w3_el_id('id-reason-disabled-preview').innerHTML = admin_preview_status_box(cfg.reason_disabled);
}

function server_enabled_cb(path, idx, first)
{
	idx = +idx;
	var enabled = (idx == 0);
	
	//console.log('server_enabled_cb: first='+ first +' enabled='+ enabled);

	if (!first) {
		ext_send('SET server_enabled='+ (enabled? 1:0));
	}
	
	admin_bool_cb(path, enabled, first);
}

function control_user_kick_cb(id, idx)
{
	ext_send('SET user_kick=-1');
}

function reason_disabled_cb(path, val)
{
	w3_string_set_cfg_cb(path, val);
	w3_el_id('id-reason-disabled-preview').innerHTML = admin_preview_status_box(cfg.reason_disabled);
}


////////////////////////////////
// config
////////////////////////////////

var ITU_region_i = { 0:'R1: Europe, Africa', 1:'R2: North & South America', 2:'R3: Asia / Pacific' };

var AM_BCB_chan_i = { 0:'9 kHz', 1:'10 kHz' };

var max_freq_i = { 0:'30 MHz', 1:'32 MHz' };

var SPI_clock_i = { 0:'48 MHz', 1:'24 MHz' };

function config_html()
{
	kiwi_get_init_settings();		// make sure defaults exist
	
	var init_mode = ext_get_cfg_param('init.mode', 0);
	var init_AM_BCB_chan = ext_get_cfg_param('init.AM_BCB_chan', 0);
	var init_ITU_region = ext_get_cfg_param('init.ITU_region', 0);
	var max_freq = ext_get_cfg_param('max_freq', 0);
	
	var s1 =
		'<hr>' +
		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_input_get_param('Initial frequency (kHz)', 'init.freq', 'config_float_cb'),
			w3_divs('', 'w3-center',
				w3_select('Initial mode', '', 'init.mode', init_mode, modes_u, 'config_select_cb')
			),
			w3_input_get_param('Initial zoom (0-11)', 'init.zoom', 'config_int_cb')
		) +

		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_input_get_param('Initial waterfall min (dBFS, fully zoomed-out)', 'init.min_dB', 'config_int_cb'),
			w3_input_get_param('Initial waterfall max (dBFS)', 'init.max_dB', 'config_int_cb'),
			w3_divs('', 'w3-center',
				w3_select('Initial AM BCB channel spacing', '', 'init.AM_BCB_chan', init_AM_BCB_chan, AM_BCB_chan_i, 'config_select_cb')
			)
		);

   var s2 =
		'<hr>' +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_divs('w3-restart', '',
				w3_input_get_param('Inactivity timeout (minutes, 0 = no limit)', 'inactivity_timeout_mins', 'config_int_cb')
			),
			w3_input_get_param('S-meter calibration (dB)', 'S_meter_cal', 'config_int_cb'),
			w3_input_get_param('Waterfall calibration (dB)', 'waterfall_cal', 'config_int_cb')
		) +
		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_divs('', 'w3-center w3-tspace-8',
				w3_select('ITU region', '', 'init.ITU_region', init_ITU_region, ITU_region_i, 'config_select_cb'),
				w3_divs('', 'w3-text-black',
					'Configures LW/NDB, MW and <br> amateur band allocations, etc.'
				)
			),
			w3_divs('w3-restart', 'w3-center w3-tspace-8',
				w3_select('Max receiver frequency', '', 'max_freq', max_freq, max_freq_i, 'config_select_cb'),
				w3_divs('', 'w3-text-black')
			),
			w3_divs('w3-restart', 'w3-center w3-tspace-8',
				w3_select_get_param('SPI clock', '', 'SPI_clock', SPI_clock_i, 'config_select_cb', 0),
				w3_divs('', 'w3-text-black',
					'Set to 24 MHz to reduce interference <br> on 2 meters (144-148 MHz).'
				)
			)
		) +
		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_divs('w3-restart', '',
				w3_input_get_param('Frequency scale offset (kHz)', 'freq_offset', 'config_int_cb'),
				w3_divs('', 'w3-text-black',
					'Adds offset to frequency scale. <br> Useful when using a frequency converter, e.g. <br>' +
					'set to 116000 kHz when 144-148 maps to 28-32 MHz.'
				)
			),
			'',
			''
		);

   var s3 =
		'<hr>' +
		w3_div('w3-container',
         w3_label('w3-text-teal',
            'To manually adjust/calibrate the ADC clock (e.g. when there is no GPS signal for automatic calibration) follow these steps:' +
            '<ul>' +
               '<li>Open a normal user connection to the SDR</li>' +
               '<li>Tune to a time station or other accurate signal and zoom all the way in</li>' +
               '<li>Higher frequency shortwave stations are better because they will show more offset than LF/VLF stations</li>' +
               '<li>Click exactly on the signal carrier line in the waterfall</li>' +
               '<li>On the right-click menu select the <i>cal ADC clock (admin)</i> entry</li>' +
               '<li>You may have to give the admin password if not already authenticated</li>' +
               '<li>The adjustment is calculated and the carrier on the waterfall should move to the nearest 1 kHz marker</li>' +
               '<li>Use the fine-tuning controls on the IQ extension panel if necessary</li>' +
            '</ul>'
         ),

         w3_label('w3-text-teal',         
            'You can fine-tune after the above steps as follows:' +
            '<ul>' +
               '<li>Open IQ display extension</li>' +
               '<li>Set the receive frequency to the exact nominal carrier (e.g. 15000 kHz for WWV)</li>' +
               '<li>Press the <i>AM 40 Hz</i> button</li>' +
               '<li>Adjust the gain until you see a point rotating in a circle</li>' +
               '<li>Use the <i>Fcal</i> buttons to slow the rotation as much as possible</li>' +
               '<li>A full rotation in less than two seconds is good calibration</li>' +
            '</ul>'
         )
      ) +
		'<hr>';

	return w3_divs('id-config w3-hide', '', s1 + s2 + s3);
}

function config_int_cb(path, val)
{
	//console.log('config_int '+ path +'='+ val);
	val = parseInt(val);
	if (isNaN(val)) {
		val = 0;
		w3_set_value(path, val);
	}
	admin_int_cb(path, val);
}

function config_float_cb(path, val)
{
	//console.log('config_float '+ path +'='+ val);
	val = parseFloat(val);
	if (isNaN(val)) {
		val = 0;
		w3_set_value(path, val);
	}
	admin_float_cb(path, val);
}

function config_select_cb(path, idx, first)
{
	//console.log('config_select_cb idx='+ idx +' path='+ path);
	idx = +idx;
	if (idx != -1) {
		var save = first? false : true;
		ext_set_cfg_param(path, idx, save);
	}
}


////////////////////////////////
// channels [not used currently]
////////////////////////////////

function channels_html()
{
	var s =
	w3_divs('id-channels w3-hide', '',
		'<hr>' +

		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			'foo',
			'bar',
			'baz'
		)
	);
	return s;
}


////////////////////////////////
// webpage
////////////////////////////////

function webpage_html()
{
	var s1 =
		'<hr>' +
		w3_divs('w3-margin-bottom', 'w3-container',
			w3_input('Top bar title', 'index_html_params.RX_TITLE', '', 'webpage_title_cb')
		) +
		w3_divs('', 'w3-container',
			'<label><b>Top bar title HTML preview</b></label>',
			w3_divs('', 'id-webpage-title-preview w3-text-black w3-background-pale-aqua', '')
		) +

		w3_divs('w3-margin-top w3-margin-bottom', 'w3-container',
			w3_input('Owner info (appears in center of top bar)', 'owner_info', '', 'webpage_owner_info_cb')
		) +
		w3_divs('', 'w3-container',
			'<label><b>Owner info HTML preview</b></label>',
			w3_divs('', 'id-webpage-owner-info-preview w3-text-black w3-background-pale-aqua', '')
		) +

		w3_divs('w3-margin-top w3-margin-bottom', 'w3-container',
			w3_input('Status', 'status_msg', '', 'webpage_status_cb')
		) +
		w3_divs('', 'w3-container',
			'<label><b>Status HTML preview</b></label>',
			w3_divs('', 'id-webpage-status-preview w3-text-black w3-background-pale-aqua', '')
		) +
		
		w3_divs('w3-margin-top', 'w3-container',
			w3_input('Window/tab title', 'index_html_params.PAGE_TITLE', '', 'webpage_string_cb')
		);
	
	var s2 =
		'<hr>' +
		w3_half('w3-margin-bottom', 'w3-container',
			w3_input('Location', 'index_html_params.RX_LOC', '', 'webpage_string_cb'),
			w3_input('Grid square (4 or 6 char) ', 'index_html_params.RX_QRA', '', 'webpage_input_grid', null, null,
			   w3_div('id-webpage-grid-check cl-admin-check w3-inline w3-green w3-btn w3-round-large')
			)
		) +
		w3_half('', 'w3-container',
			w3_input('Altitude (ASL meters)', 'index_html_params.RX_ASL', '', 'webpage_string_cb'),
			w3_input('Map (Google format) ', 'index_html_params.RX_GMAP', '', 'webpage_input_map', null, null,
			   w3_div('id-webpage-map-check cl-admin-check w3-inline w3-green w3-btn w3-round-large')
			)
		) +
		
		'<hr>' +
		w3_half('w3-margin-bottom', 'w3-container',
			w3_half('', '',
            w3_divs('', '',
               w3_label('', 'Photo file'),
               '<input id="id-photo-file" type="file" accept="image/*" onchange="webpage_photo_file_upload()"/>',
               w3_divs('', 'id-photo-error', '')
            ),
            w3_checkbox_get_param('w3-restart', 'Photo left margin', 'index_html_params.RX_PHOTO_LEFT_MARGIN', 'admin_bool_cb', true)
         ),
			w3_input('Photo maximum height (pixels)', 'index_html_params.RX_PHOTO_HEIGHT', '', 'webpage_string_cb')
		) +
		w3_half('', 'w3-container',
			w3_input('Photo title', 'index_html_params.RX_PHOTO_TITLE', '', 'webpage_string_cb'),
			w3_input('Photo description', 'index_html_params.RX_PHOTO_DESC', '', 'webpage_string_cb')
		) +
		
		w3_divs('w3-margin-bottom', 'w3-container', '');		// bottom gap for better scrolling look

   return w3_divs('id-webpage w3-text-teal w3-hide', '', s1 + s2);
}

function webpage_input_grid(path, val)
{
	webpage_string_cb(path, val);
	webpage_update_check_grid();
}

function webpage_update_check_grid()
{
	var grid = ext_get_cfg_param('index_html_params.RX_QRA');
	w3_el_id('webpage-grid-check').innerHTML = '<a href="http://www.levinecentral.com/ham/grid_square.php?Grid='+ grid +'" target="_blank">check grid</a>';
}

function webpage_input_map(path, val)
{
	webpage_string_cb(path, val);
	webpage_update_check_map();
}

function webpage_update_check_map()
{
	var map = ext_get_cfg_param('index_html_params.RX_GMAP');
	w3_el_id('webpage-map-check').innerHTML = '<a href="http://google.com/maps/place/'+ map +'" target="_blank">check map</a>';
}

function webpage_photo_uploaded(obj)
{
	var rc;
	
	if (obj.AJAX_error != undefined)
		rc = -1;
	else
		rc = obj.r;
	
	//console.log('## webpage_photo_uploaded rc='+ rc);
	if (rc == 0) {
		// server restart not needed, effect is immediate on reload or next connection
		webpage_string_cb('index_html_params.RX_PHOTO_FILE', 'kiwi.config/photo.upload');
	}
	
	var el = w3_el_id('photo-error');
	var e;
	
	switch (rc) {
	case -1:
		e = 'Communication error';
		break;
	case 0:
		e = 'Upload successful';
		break;
	case 1:
		e = 'Authentication failed';
		break;
	case 2:
		e = 'Not an image file?';
		break;
	case 3:
		e = 'Unable to determine file type';
		break;
	case 4:
		e = 'File too large';
		break;
	default:
		e = 'Undefined error?';
		break;
	}
	
	el.innerHTML = e;
	w3_class(el, (rc == 0)? 'w3-text-green' : 'w3-text-red');
	w3_show_block(el);
}

function webpage_photo_file_upload()
{
	ext_get_authkey(function(key) {
		webpage_photo_file_upload2(key);
	});
}

function webpage_photo_file_upload2(key)
{
	var browse = w3_el_id('id-photo-file');
	browse.innerHTML = 'Uploading...';
	var file = browse.files[0];
	var fdata = new FormData();
	fdata.append('photo', file, file.name);
	//console.log(file);

	var el = w3_el_id('photo-error');
	w3_hide(el);
	w3_unclass(el, 'w3-text-red');
	w3_unclass(el, 'w3-text-green');

	kiwi_ajax_send(fdata, '/PIX?'+ key, 'webpage_photo_uploaded');
}

function webpage_title_cb(path, val)
{
	webpage_string_cb(path, val);
	w3_el_id('id-webpage-title-preview').innerHTML = admin_preview_status_box(cfg.index_html_params.RX_TITLE);
}

function webpage_owner_info_cb(path, val)
{
	webpage_string_cb(path, val);
	w3_el_id('id-webpage-owner-info-preview').innerHTML = admin_preview_status_box(cfg.owner_info);
}

function webpage_status_cb(path, val)
{
	w3_string_set_cfg_cb(path, val);
	w3_el_id('id-webpage-status-preview').innerHTML = admin_preview_status_box(cfg.status_msg);
}

// because of the inline quoting issue, set value dynamically
function webpage_focus()
{
	admin_set_decoded_value('index_html_params.RX_TITLE');
	w3_el_id('id-webpage-title-preview').innerHTML = admin_preview_status_box(cfg.index_html_params.RX_TITLE);

	admin_set_decoded_value('status_msg');
	w3_el_id('id-webpage-status-preview').innerHTML = admin_preview_status_box(cfg.status_msg);

	admin_set_decoded_value('index_html_params.PAGE_TITLE');
	admin_set_decoded_value('index_html_params.RX_LOC');
	admin_set_decoded_value('index_html_params.RX_QRA');
	admin_set_decoded_value('index_html_params.RX_ASL');
	admin_set_decoded_value('index_html_params.RX_GMAP');
	admin_set_decoded_value('index_html_params.RX_PHOTO_HEIGHT');
	admin_set_decoded_value('index_html_params.RX_PHOTO_TITLE');
	admin_set_decoded_value('index_html_params.RX_PHOTO_DESC');

	admin_set_decoded_value('owner_info');
	w3_el_id('id-webpage-owner-info-preview').innerHTML = admin_preview_status_box(cfg.owner_info);

	webpage_update_check_grid();
	webpage_update_check_map();
}

function webpage_string_cb(path, val)
{
	w3_string_set_cfg_cb(path, val);
	ext_send('SET reload_index_params');
}


////////////////////////////////
// connect
////////////////////////////////

// cfg.{sdr_hu_dom_sel, sdr_hu_dom_name, sdr_hu_dom_ip, server_url}
// for now server_url and sdr_hu_dom_name are kept equal when changed

var connect_dom_sel = { NAM:0, DUC:1, PUB:2, SIP:3, REV:4 };
var duc_update_i = { 0:'5 min', 1:'10 min', 2:'15 min', 3:'30 min', 4:'60 min' };
var duc_update_v = { 0:5, 1:10, 2:15, 3:30, 4:60 };

function connect_html()
{
   // transition code: default cfg.sdr_hu_dom_name with previous value of cfg.server_url
   if (ext_get_cfg_param('sdr_hu_dom_sel') == null) {
      ext_set_cfg_param('sdr_hu_dom_sel', 0);
      var server_url = ext_get_cfg_param('server_url');
      if (server_url == 'kiwisdr.example.com') {
         server_url = '';
	      ext_set_cfg_param('cfg.server_url', server_url, true);
      }
      ext_set_cfg_param('sdr_hu_dom_name', server_url, true);
   }

   var ci = 0;
   var s1 =
		w3_divs('w3-vcenter', '',
			'<header class="w3-container w3-yellow"><h5>' +
			'If you are not able to make an incoming connection from the Internet to your Kiwi because ' +
			'of problems <br> with your router or Internet Service Provider (ISP) then please consider using the KiwiSDR ' +
			'<a href="http://proxy.kiwisdr.com" target="_blank">reverse proxy service</a>.' +
			'</h5></header>'
		) +
		
      '<hr>' +
		w3_divs('id-warn-ip w3-vcenter w3-margin-B-8 w3-hide', '', '<header class="w3-container w3-yellow"><h5>' +
			'Warning: Using an IP address in the Kiwi connect name will work, but if you switch to using a domain name later on<br>' +
			'this will cause duplicate entries on <a href="http://sdr.hu/?top=kiwi" target="_blank">sdr.hu</a> ' +
			'See <a href="http://kiwisdr.com/quickstart#id-sdr_hu-dup" target="_blank">kiwisdr.com/quickstart</a> for more information.' +
			'</h5></header>'
		) +
		
      w3_divs('w3-container', 'w3-tspace-8',
         w3_label('', 'What domain name or IP address will people use to connect to your KiwiSDR?<br>' +
            'If you are listing on sdr.hu this information will be part of your entry.<br>' +
            'Click one of the five options below and enter any additional information:<br><br>'),
         
         // (n/a anymore) w3-static because w3-sidenav does a position:fixed which is no good here at the bottom of the page
         // w3-left to get float:left to put the input fields on the right side
         // w3-sidenav-full-height to match the height of the w3_input() on the right side
		   '<nav class="id-admin-nav-dom w3-sidenav w3-static w3-left w3-border w3-sidenav-full-height w3-margin-R-16 w3-light-grey">' +
		      w3_sidenav('connect_dom_nam', 'Domain Name', admin_colors[ci++], (cfg.sdr_hu_dom_sel == connect_dom_sel.NAM)) +
		      w3_sidenav('connect_dom_duc', 'DUC Domain', admin_colors[ci++], (cfg.sdr_hu_dom_sel == connect_dom_sel.DUC)) +
		      w3_sidenav('connect_dom_rev', 'Reverse Proxy', admin_colors[ci++], (cfg.sdr_hu_dom_sel == connect_dom_sel.REV)) +
		      w3_sidenav('connect_dom_pub', 'Public IP', admin_colors[ci++], (cfg.sdr_hu_dom_sel == connect_dom_sel.PUB)) +
		      w3_sidenav('connect_dom_sip', 'Specified IP', admin_colors[ci++], (cfg.sdr_hu_dom_sel == connect_dom_sel.SIP)) +
		   '</nav>',
		   
		   w3_divs('', 'w3-padding-L-16',
            w3_div('w3-inline|width:70%;', w3_input_get_param('', 'sdr_hu_dom_name', 'connect_dom_name_cb', '',
               'Enter domain name that you will point to Kiwi public IP address, e.g. kiwisdr.my_domain.com '+
               '(no port number)')),
            w3_div('id-connect-duc-dom w3-padding-TB-8'),
            w3_div('id-connect-rev-dom w3-padding-TB-8'),
            w3_div('id-connect-pub-ip w3-padding-TB-8'),
            w3_div('w3-inline|width:70%;', w3_input_get_param('', 'sdr_hu_dom_ip', 'connect_dom_ip_cb', '',
               'Enter known public IP address of the Kiwi (no port number)'))
         ),
         
		   w3_div('w3-margin-T-16', 
            w3_label('w3-inline w3-margin-R-16 w3-text-teal', '', 'connect-url-text') +
			   w3_div('id-connect-url w3-inline w3-text-black w3-background-pale-aqua')
         )
      );

   var s2 =
		'<hr>' +
		w3_divs('', '',
			w3_col_percent('w3-text-teal', 'w3-container',
			   w3_div('w3-text-teal w3-bold', 'Dynamic DNS update client (DUC) configuration'), 50,
				w3_div('w3-text-teal w3-bold w3-center w3-light-grey', 'Account at noip.com'), 50
			),
			
			w3_col_percent('w3-text-teal', 'w3-container',
				w3_div(), 50,
				w3_input_get_param('Username or email', 'adm.duc_user', 'w3_string_set_cfg_cb', '', 'required'), 25,
				w3_input_get_param('Password', 'adm.duc_pass', 'w3_string_set_cfg_cb', '', 'required'), 25
			),
			
			w3_col_percent('w3-text-teal', 'w3-container',
				w3_div('w3-center',
					'<b>Enable DUC at startup?</b><br>' +
					w3_divs('', '',
						w3_switch('Yes', 'No', 'adm.duc_enable', adm.duc_enable, 'connect_DUC_enabled_cb')
					)
				), 20,
				
				w3_div('w3-center',
				   w3_select('Update', '', 'adm.duc_update', adm.duc_update, duc_update_i, 'config_select_cb')
				), 10,
				
				w3_divs('', 'w3-center w3-tspace-8',
					w3_button('w3-aqua', 'Click to (re)start DUC', 'connect_DUC_start_cb'),
					w3_divs('', 'w3-text-black',
						'After changing username or password click to test changes.'
					)
				), 20,
				
				w3_input_get_param('Host', 'adm.duc_host', 'connect_DUC_host_cb', '', 'required'), 50
			),
			
			w3_div('w3-container',
            w3_label('w3-inline w3-margin-R-16 w3-text-teal', 'Status:') +
				w3_div('id-net-duc-status w3-inline w3-text-black w3-background-pale-aqua', '')
			)
		);

   var s3 =
		'<hr>' +
		w3_divs('', '',
			w3_col_percent('w3-text-teal', 'w3-container',
			   w3_div('w3-text-teal w3-bold', 'Reverse proxy configuration'), 50,
				w3_div('w3-text-teal w3-bold w3-center w3-light-grey', 'Account at kiwisdr.com'), 50
			),
			
			w3_col_percent('w3-text-teal', 'w3-container',
				w3_div(), 50,
				w3_input_get_param('User key', 'adm.rev_user', 'w3_string_set_cfg_cb', '', 'required'), 50
			),
			
			w3_col_percent('w3-text-teal', 'w3-container',
				w3_divs('', 'w3-center w3-tspace-8',
					w3_button('w3-aqua', 'Click to (re)register', 'connect_rev_register_cb'),
					w3_divs('', 'w3-text-black',
						'After changing user key or<br>host name click to register proxy.'
					)
				), 50,
				
				w3_div('',
               w3_div('w3-inline|width:40%;', w3_input_get_param('Host name', 'adm.rev_host', 'connect_rev_host_cb', '', 'required')) +
               w3_div('id-connect-rev-url w3-inline', '.proxy.kiwisdr.com')
            ), 50
			),
			
			w3_div('w3-container',
            w3_label('w3-inline w3-margin-R-16 w3-text-teal', 'Status:') +
				w3_div('id-connect-rev-status w3-inline w3-text-black w3-background-pale-aqua', '')
			)
		) +
		'<hr>';

	return w3_divs('id-connect w3-text-teal w3-hide', '', s1 + s2 + s3);
}

function connect_focus()
{
   connect_update_url();
	ext_send('SET DUC_status_query');
	ext_send('SET rev_status_query');
}

var connect_rev_server = -1;

function connect_update_url()
{
	w3_el_id('id-connect-duc-dom').innerHTML = 'Use domain name from DUC configuration below: ' +
	   w3_div('w3-inline w3-text-black w3-background-pale-aqua',
	      ((adm.duc_host && adm.duc_host != '')? adm.duc_host : '(none currently set)')
	   );

   var server = (connect_rev_server == -1)? '' : connect_rev_server;
   var server_url = '.proxy'+ server +'.kiwisdr.com';
	w3_el_id('id-connect-rev-dom').innerHTML = 'Use domain name from reverse proxy configuration below: ' +
	   w3_div('w3-inline w3-text-black w3-background-pale-aqua',
	      ((adm.rev_host && adm.rev_host != '')? (adm.rev_host + server_url) : '(none currently set)')
	   );
	w3_el_id('id-connect-rev-url').innerHTML = server_url;

	w3_el_id('id-connect-pub-ip').innerHTML = 'Public IP address detected by Kiwi: ' +
	   w3_div('w3-inline w3-text-black w3-background-pale-aqua',
	      (config_net.pub_ip? config_net.pub_ip : '(no public IP address detected)')
	   );

   var host = decodeURIComponent(cfg.server_url);
   var host_and_port = host;
   
   if (cfg.sdr_hu_dom_sel != connect_dom_sel.REV) {
      host_and_port += ':'+ config_net.pub_port;
      w3_set_label('Based on above selection, and external port from Network tab, the URL to connect to your Kiwi is:', 'connect-url-text');
   } else {
      host_and_port += ':8073';
      w3_set_label('Based on above selection the URL to connect to your Kiwi is:', 'connect-url-text');
   }
   
   w3_el_id('id-connect-url').innerHTML =
      (host == '')? '(incomplete information)' : ('http://'+ host_and_port);
}

function connect_dom_nam_focus()
{
   console.log('connect_dom_nam_focus server_url='+ cfg.sdr_hu_dom_name);
	ext_set_cfg_param('cfg.server_url', cfg.sdr_hu_dom_name, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.NAM, true);
	connect_update_url();
   w3_hide('id-warn-ip');
}

function connect_dom_duc_focus()
{
   console.log('connect_dom_duc_focus server_url='+ adm.duc_host);
	ext_set_cfg_param('cfg.server_url', adm.duc_host, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.DUC, true);
	connect_update_url();
   w3_hide('id-warn-ip');
}

function connect_dom_rev_focus()
{
   var server = (connect_rev_server == -1)? '' : connect_rev_server;
   var dom = (adm.rev_host == '')? '' : adm.rev_host + '.proxy'+ server +'.kiwisdr.com';
   console.log('connect_dom_rev_focus server_url='+ dom);
	ext_set_cfg_param('cfg.server_url', dom, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.REV, true);
	connect_update_url();
   w3_hide('id-warn-ip');
}

function connect_dom_pub_focus()
{
   console.log('connect_dom_pub_focus server_url='+ config_net.pub_ip);
	ext_set_cfg_param('cfg.server_url', config_net.pub_ip, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.PUB, true);
	connect_update_url();
	//w3_show_block('id-warn-ip');
}

function connect_dom_sip_focus()
{
   console.log('connect_dom_sip_focus server_url='+ cfg.sdr_hu_dom_ip);
	ext_set_cfg_param('cfg.server_url', cfg.sdr_hu_dom_ip, true);
	ext_set_cfg_param('cfg.sdr_hu_dom_sel', connect_dom_sel.SIP, true);
	connect_update_url();
	//w3_show_block('id-warn-ip');
}

function connect_dom_name_cb(path, val, first)
{
	connect_remove_port(path, val, first);
   if (cfg.sdr_hu_dom_sel == connect_dom_sel.NAM)     // if currently selected option update the value
      connect_dom_nam_focus();
}

function connect_dom_ip_cb(path, val, first)
{
	connect_remove_port(path, val, first);
   if (cfg.sdr_hu_dom_sel == connect_dom_sel.SIP)     // if currently selected option update the value
      connect_dom_sip_focus();
}

function connect_remove_port(el, s, first)
{
	var sl = s.length;
	var state = { bad:0, number:1, alpha:2, remove:3 };
	var st = state.bad;
	
	s = s.replace('http://', '');
	
	for (var i = sl-1; i >= 0; i--) {
		var c = s.charAt(i);
		if (c >= '0' && c <= '9') {
			st = state.number;
			continue;
		}
		if (c == ':') {
			if (st == state.number)
				st = state.remove;
			break;
		}
		st = state.alpha;
		if (c == ']')
		   break;      // start of escaped ipv6 with embedded ':'s
	}
	
	if (st == state.remove) {
		s = s.substr(0,i);
	}
	
	w3_string_set_cfg_cb(el, s, first);
	admin_set_decoded_value(el);
}


// DUC

function connect_DUC_enabled_cb(path, idx, first)
{
	idx = +idx;
	var enabled = (idx == 0);
	//console.log('connect_DUC_enabled_cb: first='+ first +' enabled='+ enabled);

	if (!first) {
		//?(enabled? 1:0);
	}
	
	admin_bool_cb(path, enabled, first);
}

function connect_DUC_start_cb(id, idx)
{
	// decode stored json values because we recode below to encode spaces of composite string
	var s = '-u '+ q(decodeURIComponent(adm.duc_user)) +' -p '+ q(decodeURIComponent(adm.duc_pass)) +
	   ' -H '+ q(decodeURIComponent(adm.duc_host)) +' -U '+ duc_update_v[adm.duc_update];
	console.log('start DUC: '+ s);
	ext_send('SET DUC_start args='+ encodeURIComponent(s));
}

function connect_DUC_host_cb(path, val, first)
{
   w3_string_set_cfg_cb(path, val, first);
   if (cfg.sdr_hu_dom_sel == connect_dom_sel.DUC)     // if currently selected option update the value
      connect_dom_duc_focus();
   else
      connect_update_url();
}

function connect_DUC_status_cb(status)
{
	status = +status;
	console.log('DUC_status='+ status);
	var s;
	
	switch (status) {
		case 0:   s = 'DUC started successfully'; break;
		case 100: s = 'Incorrect username or password'; break;
		case 101: s = 'No hosts defined on your account at noip.com; please correct and retry'; break;
		case 102: s = 'Please specify a host'; break;
		case 103: s = 'Host given isn\'t defined on your account at noip.com; please correct and retry'; break;
		case 300: s = 'DUC start failed'; break;
		case 301: s = 'DUC enabled and running'; break;
		default:  s = 'DUC internal error: '+ status; break;
	}
	
	w3_el_id('id-net-duc-status').innerHTML = s;
}


// reverse proxy

function connect_rev_register_cb(id, idx)
{
   if (adm.rev_user == '' || adm.rev_host == '')
      return connect_rev_status_cb(100);
   
	w3_el_id('id-connect-rev-status').innerHTML = '';
	var s = 'user='+ adm.rev_user +' host='+ adm.rev_host;
	console.log('start rev: '+ s);
	ext_send('SET rev_register '+ s);
}

function connect_rev_host_cb(path, val, first)
{
   w3_string_set_cfg_cb(path, val, first);
   if (cfg.sdr_hu_dom_sel == connect_dom_sel.REV)     // if currently selected option update the value
      connect_dom_rev_focus();
   else
      connect_update_url();
}

function connect_rev_status_cb(status)
{
	status = +status;
	console.log('rev_status='+ status);
	var s;
	
	if (status >= 0 && status <= 99) {
	   // jks-proxy
      //connect_rev_server = status & 0xf;
      //status = status >> 4;
      connect_dom_rev_focus();
   }
	
	switch (status) {
		case   0: s = 'Existing account, registration successful'; break;
		case   1: s = 'New account, registration successful'; break;
		case   2: s = 'Updating host name, registration successful'; break;
		case 100: s = 'User key or host name field blank'; break;
		case 101: s = 'User key invalid; please contact support@kiwisdr.com'; break;
		case 102: s = 'Host name already in use; please choose another and retry'; break;
		case 103: s = 'Invalid characters in user key or host name field (use a-z, A-Z, 0-9, -, _)'; break;
		case 200: s = 'Reverse proxy enabled and running'; break;
		case 900: s = 'Problem contacting proxy.kiwisdr.com; please check Internet connection'; break;
		default:  s = 'Reverse proxy internal error: '+ status; break;
	}
	
	w3_el_id('id-connect-rev-status').innerHTML = s;
}


////////////////////////////////
// sdr.hu
////////////////////////////////

function sdr_hu_html()
{
	var s1 =
		w3_divs('id-need-gps w3-vcenter w3-hide', '',
			'<header class="w3-container w3-yellow"><h5>Warning: GPS location field set to the default, please update</h5></header>'
		) +
		
		'<hr>' +
		w3_half('w3-margin-bottom', '',
			w3_divs('w3-container', '',
					'<b>Display your KiwiSDR on <a href="http://sdr.hu/?top=kiwi" target="_blank">sdr.hu</a>?</b> ' +
					w3_switch('Yes', 'No', 'adm.sdr_hu_register', adm.sdr_hu_register, 'admin_radio_YN_cb')
			),
			w3_divs('w3-container', '',
					'<b>Display owner/admin email link on KiwiSDR main page?</b> ' +
					w3_switch('Yes', 'No', 'contact_admin', cfg.contact_admin, 'admin_radio_YN_cb')
			)
		) +

		w3_div('id-sdr_hu-reg-status-container w3-hide',
         w3_div('w3-container',
            w3_label('w3-inline w3-margin-R-16 w3-text-teal', 'sdr.hu registration status:') +
            w3_div('id-sdr_hu-reg-status w3-inline w3-text-black w3-background-pale-aqua', '')
         )
      );
      
   var s2 =
		'<hr>' +
		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Name', 'rx_name', '', 'w3_string_set_cfg_cb'),
			w3_input('Location', 'rx_location', '', 'w3_string_set_cfg_cb')
		) +

		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			//w3_input('Device', 'rx_device', '', 'w3_string_set_cfg_cb'),
			w3_input('Admin email', 'admin_email', '', 'w3_string_set_cfg_cb'),
			w3_input('Antenna', 'rx_antenna', '', 'w3_string_set_cfg_cb')
		) +

		w3_third('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('Grid square (4/6 char) ', 'rx_grid', '', 'sdr_hu_input_grid', null, null,
				w3_div('id-sdr_hu-grid-check cl-admin-check w3-inline w3-green w3-btn w3-round-large') + ' ' +
				w3_div('id-sdr_hu-grid-set cl-admin-check w3-blue w3-btn w3-round-large w3-hide', 'set from GPS')
			),
			w3_input('Location (lat, lon) ', 'rx_gps', '', 'sdr_hu_check_gps', null, null,
				w3_div('id-sdr_hu-gps-check cl-admin-check w3-inline w3-green w3-btn w3-round-large') + ' ' +
				w3_div('id-sdr_hu-gps-set cl-admin-check w3-blue w3-btn w3-round-large w3-hide', 'set from GPS')
			),
			w3_input_get_param('Altitude (ASL meters)', 'rx_asl', 'config_int_cb')
		) +

		w3_half('w3-margin-bottom w3-restart', 'w3-container',
		   w3_div('w3-restart', w3_input('API key', 'adm.api_key', '', 'w3_string_set_cfg_cb', 'from sdr.hu/register process')),
		   ''
		) +
		w3_half('w3-margin-bottom', 'w3-container',
         w3_input_get_param('Coverage frequency low (kHz)', 'sdr_hu_lo_kHz', 'config_int_cb'),
         w3_input_get_param('Coverage frequency high (kHz)', 'sdr_hu_hi_kHz', 'config_int_cb')
      );

	return w3_divs('id-sdr_hu w3-text-teal w3-hide', '', s1 + s2);
}

var sdr_hu_interval;

// because of the inline quoting issue, set value dynamically
function sdr_hu_focus()
{
	admin_set_decoded_value('rx_name');
	admin_set_decoded_value('rx_location');
	//admin_set_decoded_value('rx_device');
	admin_set_decoded_value('rx_antenna');
	admin_set_decoded_value('rx_grid');
	admin_set_decoded_value('rx_gps');
	admin_set_decoded_value('admin_email');
	admin_set_decoded_value('adm.api_key');

	// The default in the factory-distributed kiwi.json is the kiwisdr.com NZ location.
	// Detect this and ask user to change it so sdr.hu/map doesn't end up with multiple SDRs
	// defined at the kiwisdr.com location.
	var gps = decodeURIComponent(ext_get_cfg_param('rx_gps'));
	sdr_hu_check_gps('rx_gps', gps, /* first */ true);
	
	sdr_hu_update_check_grid();
	sdr_hu_update_check_map();
	
	w3_el_id('sdr_hu-grid-set').onclick = function() {
		var val = sdr_hu_status.grid;
		w3_set_value('rx_grid', val);
		w3_input_change('rx_grid', 'sdr_hu_input_grid');
	};

	w3_el_id('sdr_hu-gps-set').onclick = function() {
		var val = '('+ sdr_hu_status.lat +', '+ sdr_hu_status.lon +')';
		w3_set_value('rx_gps', val);
		w3_input_change('rx_gps', 'sdr_hu_check_gps');
	};

	// only get updates while the sdr_hu tab is selected
	ext_send("SET sdr_hu_update");
	sdr_hu_interval = setInterval(function() {ext_send("SET sdr_hu_update");}, 5000);
}

function sdr_hu_input_grid(path, val)
{
	w3_string_set_cfg_cb(path, val);
	sdr_hu_update_check_grid();
}

function sdr_hu_update_check_grid()
{
	var grid = ext_get_cfg_param('rx_grid');
	w3_el_id('sdr_hu-grid-check').innerHTML = '<a href="http://www.levinecentral.com/ham/grid_square.php?Grid='+ grid +'" target="_blank">check grid</a>';
}

function sdr_hu_check_gps(path, val, first)
{
	if (val.charAt(0) != '(')
		val = '('+ val;
	if (val.charAt(val.length-1) != ')')
		val = val +')';

	if (val == '(-37.631120, 176.172210)' || val == '(-37.631120%2C%20176.172210)') {
		w3_show_block('id-need-gps');
		w3_flag('rx_gps');
	} else {
		w3_hide('id-need-gps');
		w3_unflag('rx_gps');
	}
	
	w3_string_set_cfg_cb(path, val, first);
	w3_set_value(path, val);
	sdr_hu_update_check_map();
}

function sdr_hu_update_check_map()
{
	var gps = decodeURIComponent(ext_get_cfg_param('rx_gps'));
	gps = gps.substring(1, gps.length-1);		// remove parens
	w3_el_id('sdr_hu-gps-check').innerHTML = '<a href="http://google.com/maps/place/'+ gps +'" target="_blank">check map</a>';
}

function sdr_hu_blur(id)
{
	kiwi_clearInterval(sdr_hu_interval);
}

var sdr_hu_status = { };

function sdr_hu_update(p)
{
	var i;
	var sdr_hu_json = decodeURIComponent(p);
	//console.log('sdr_hu_json='+ sdr_hu_json);
	sdr_hu_status = JSON.parse(sdr_hu_json);
	
	// sdr.hu registration status
	if (sdr_hu_status.reg != undefined && sdr_hu_status.reg != '') {
	   var el = w3_el_id('id-sdr_hu-reg-status');
	   el.innerHTML = sdr_hu_status.reg;
		w3_show_inline('id-sdr_hu-reg-status-container');
	}
	
	// GPS has had a solution, show buttons
	if (sdr_hu_status.lat != undefined) {
		w3_show_inline('id-sdr_hu-grid-set');
		w3_show_inline('id-sdr_hu-gps-set');
	}
}


////////////////////////////////
// dx
////////////////////////////////

function dx_html()
{
	var s =
	w3_divs('id-dx w3-hide', '',
		'<hr>' +
		w3_divs('w3-container', '', 'TODO: dx list editing...') +
		'<hr>'
	);
	return s;
}


////////////////////////////////
// network
////////////////////////////////

function network_html()
{
	var s1 =
		w3_divs('id-net-auto-nat w3-vcenter w3-hide', '',
			'<header class="w3-container"><h5 id="id-net-auto-nat-msg">Automatic add of NAT rule on firewall / router: </h5></header>'
		) +

		w3_divs('id-net-need-update w3-vcenter w3-hide', '',
			w3_button('w3-aqua', 'Are you sure? Click to update interface DHCP/static IP configuration', 'network_dhcp_static_update_cb')
		) +

		'<hr>' +
		w3_divs('id-net-reboot', '',
			w3_col_percent('w3-container w3-margin-bottom w3-text-teal', 'w3-hspace-16',
				w3_divs('', 'w3-restart',
					w3_input_get_param('Internal port', 'adm.port', 'admin_int_cb')
				), 24,
				w3_divs('', 'w3-restart',
					w3_input_get_param('External port', 'adm.port_ext', 'admin_int_cb')
				), 24,
				w3_divs('w3-center', 'w3-restart',
					'<b>Auto add NAT rule<br>on firewall / router?</b><br>',
					w3_divs('', '',
						w3_switch('Yes', 'No', 'adm.auto_add_nat', adm.auto_add_nat, 'admin_radio_YN_cb')
					)
				), 24,
				w3_divs('w3-center', '',
						'<b>IP address<br>(only static IPv4 for now)</b><br> ' +
						w3_radio_btn_get_param('DHCP', 'adm.ip_address.use_static', 0, false, 'network_use_static_cb') +
						w3_radio_btn_get_param('Static', 'adm.ip_address.use_static', 1, false, 'network_use_static_cb')
				), 24
			),
			w3_divs('id-net-static w3-hide', '',
				w3_third('w3-margin-B-8 w3-text-teal', 'w3-container',
					w3_input_get_param('IP address (n.n.n.n where n = 0..255)', 'adm.ip_address.ip', 'network_ip_address_cb', ''),
					w3_input_get_param('Netmask (n.n.n.n where n = 0..255)', 'adm.ip_address.netmask', 'network_netmask_cb', ''),
					w3_input_get_param('Gateway (n.n.n.n where n = 0..255)', 'adm.ip_address.gateway', 'network_gw_address_cb', '')
				),
				w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
					w3_divs('id-network-check-ip w3-green', ''),
					w3_divs('id-network-check-nm w3-green', ''),
					w3_divs('id-network-check-gw w3-green', '')
				)
			)
		);
	
	var s2 =
		'<hr>' +
		w3_divs('id-net-config w3-container', '') +

		'<hr>' +
		w3_divs('', 'w3-container',
		   w3_div('', 
            w3_label('w3-show-inline w3-text-teal', 'Check if your external router port is open:') +
            w3_button('w3-show-inline w3-aqua|margin-left:10px', 'Check port open', 'net_port_open_cb')
         ),
         //'Does kiwisdr.com receive a correct reply when checking these URLs used to reach your Kiwi:',
         'Does kiwisdr.com successfully connect to your Kiwi using these URLs?<br>' +
         'If both respond "NO" then check the NAT port mapping on your router.<br>' +
         'If first responds "NO" and second "YES" then domain name of first isn\'t resolving to the ip address of the second. Check DNS.',
		   w3_div('', 
            w3_label('id-net-check-port-dom-q w3-inline w3-margin-LR-16 w3-text-teal') +
			   w3_div('id-net-check-port-dom-s w3-inline w3-text-black w3-background-pale-aqua')
         ),
		   w3_div('', 
            w3_label('id-net-check-port-ip-q w3-inline w3-margin-LR-16 w3-text-teal') +
			   w3_div('id-net-check-port-ip-s w3-inline w3-text-black w3-background-pale-aqua')
         )
      );

   var s3 =
		'<hr>' +
		w3_divs('w3-container', '', 'TODO: throttle #chan MB/dy GB/mo, hostname') +
		'<hr>';

	// FIXME replace this with general instantiation call from w3_input()
	setTimeout(function() {
		var use_static = ext_get_cfg_param('adm.ip_address.use_static', false);
		network_use_static_cb('adm.ip_address.use_static', use_static, /* first */ true);
	}, 500);
	
	return w3_divs('id-network w3-hide', '', s1 + s2 + s3);
}

function network_focus()
{
	w3_el_id('id-net-check-port-dom-q').innerHTML =
	   (cfg.server_url != '')?
	      'http://'+ cfg.server_url +':'+ config_net.pub_port +' :' :
	      '(incomplete information) :';
	w3_el_id('id-net-check-port-ip-q').innerHTML =
	   'http://'+ config_net.pvt_ip +':'+ config_net.pub_port +' :';
   w3_el_id('id-net-check-port-dom-s').innerHTML = '';
   w3_el_id('id-net-check-port-ip-s').innerHTML = '';
}

function network_check_port_status_cb(status)
{
   console.log('network_check_port_status_cb status='+ status.toHex());
   if (status < 0) {
      w3_el_id('id-net-check-port-dom-s').innerHTML = 'Error checking port status';
      w3_el_id('id-net-check-port-ip-s').innerHTML = 'Error checking port status';
   } else {
      var dom_status = status & 0xf0;
      var ip_status = status & 0x0f;
      w3_el_id('id-net-check-port-dom-s').innerHTML = dom_status? 'NO' : 'YES';
      w3_el_id('id-net-check-port-ip-s').innerHTML = ip_status? 'NO' : 'YES';
   }
}

function net_port_open_cb()
{
   w3_el_id('id-net-check-port-dom-s').innerHTML = w3_icon('', 'fa-refresh fa-spin', '', 20);
   w3_el_id('id-net-check-port-ip-s').innerHTML = w3_icon('', 'fa-refresh fa-spin', '', 20);
	ext_send('SET check_port_open');
}

function network_dhcp_static_update_cb(path, idx)
{
	if (adm.ip_address.use_static)
		ext_send('SET static_ip='+ kiwi_ip_str(network_ip) +' static_nm='+ kiwi_ip_str(network_nm) +' static_gw='+ kiwi_ip_str(network_gw));
	else
		ext_send('SET use_DHCP');

	w3_hide('id-net-need-update');
	w3_reboot_cb();		// show reboot button after confirm button pressed
}

function network_use_static_cb(path, idx, first)
{
	idx = +idx;
	//console.log('network_use_static_cb idx='+ idx);
	var dhcp = (idx == 0);
	
	// only show IP fields if in static mode
	if (!dhcp) {
		w3_show_block('id-net-static');
	} else {
		w3_hide('id-net-static');
	}

	//console.log('network_use_static_cb: first='+ first +' dhcp='+ dhcp);

	// when mode is changed decide if update button needs to appear
	if (!first) {
		if (dhcp) {
			w3_show_block('id-net-need-update');
		} else {
			network_show_update(false);	// show based on prior static info (if any)
		}
	} else {
		// first time, fill-in the fields with the configured values
		network_ip_address_cb('adm.ip_address.ip', adm.ip_address.ip, true);
		network_netmask_cb('adm.ip_address.netmask', adm.ip_address.netmask, true);
		network_gw_address_cb('adm.ip_address.gateway', adm.ip_address.gateway, true);
	}
	
	admin_bool_cb(path, dhcp? 0:1, first);
}

function network_ip_nm_check(val, ip)
{
	var rexp = '^([0-9]*)\.([0-9]*)\.([0-9]*)\.([0-9]*)$';
	var p = new RegExp(rexp).exec(val);
	var a, b, c, d;
	
	if (p != null) {
		//console.log('regexp p='+ p);
		a = parseInt(p[1]);
		a = (a > 255)? Math.NaN : a;
		b = parseInt(p[2]);
		b = (b > 255)? Math.NaN : b;
		c = parseInt(p[3]);
		c = (c > 255)? Math.NaN : c;
		d = parseInt(p[4]);
		d = (d > 255)? Math.NaN : d;
	}
	
	if (p == null || isNaN(a) || isNaN(b) || isNaN(c) || isNaN(d)) {
		ip.ok = false;
	} else {
		ip.ok = true; ip.a = a; ip.b = b; ip.c = c; ip.d = d;
	}
	
	return ip.ok;
}

network_ip = { ok:false, a:null, b:null, c:null, d:null };
network_nm = { ok:false, a:null, b:null, c:null, d:null };
network_gw = { ok:false, a:null, b:null, c:null, d:null };

function network_show_update(first)
{
	//console.log('network_show_update: first='+ first);

	if (!first && network_ip.ok && network_nm.ok && network_gw.ok) {
		//console.log('network_show_update: SHOW');
		w3_show_block('id-net-need-update');
	} else {
		w3_hide('id-net-need-update');
	}
}

function network_show_check(id, name, path, val_str, ip, first, check_func)
{
	if (val_str != '') {
		var el = w3_el_id(id);
		var check = network_ip_nm_check(val_str, ip), check2 = true;
		
		if (check == true && check_func != undefined) {
			check2 = check_func(val_str, ip);
		}
	
		if (check == false || check2 == false) {
			el.innerHTML = 'bad '+ name +' entered';
			w3_unclass(el, 'w3-green');
			w3_class(el, 'w3-red');
		} else {
			el.innerHTML = name +' okay, check: '+ ip.a +'.'+ ip.b +'.'+ ip.c +'.'+ ip.d;
			w3_unclass(el, 'w3-red');
			w3_class(el, 'w3-green');
			w3_string_set_cfg_cb(path, val_str, first);
		}

		network_show_update(first);		// when a field is made good decide if update button needs to be shown
	}
}

function network_ip_address_cb(path, val, first)
{
	network_show_check('network-check-ip', 'IP address', path, val, network_ip, first);
}

function network_netmask_cb(path, val, first)
{
	network_nm.nm = -1;
	network_show_check('network-check-nm', 'netmask', path, val, network_nm, first,
		function(val_str, ip) {
			var ip_host = kiwi_inet4_d2h(val_str);
			ip.nm = 0;		// degenerate case: no one-bits in netmask at all
			for (var i = 0; i < 32; i++) {
				if (ip_host & (1<<i)) {		// first one-bit hit
					ip.nm = 32-i;
					for (; i < 32; i++) {
						if ((ip_host & (1<<i)) == 0) {
							ip.nm = -1;		// rest of bits weren't ones like they're supposed to be
							ip.ok = false;
							return false;
						}
					}
				}
			}
			ip.ok = true;
			return true;
		});

	if (network_nm.nm != -1)
		w3_el_id('network-check-nm').innerHTML += ' (/'+ network_nm.nm +')';
}

function network_gw_address_cb(path, val, first)
{
	network_show_check('network-check-gw', 'gateway', path, val, network_gw, first);
}


////////////////////////////////
// update
//		auto reload page when build finished?
////////////////////////////////

function update_html()
{
	var s =
	w3_divs('id-update w3-hide', '',
		'<hr>' +
		w3_divs('id-msg-update w3-container', '') +
		'<hr>' +
		w3_divs('', 'w3-margin-bottom',
         w3_half('w3-container', 'w3-text-teal',
				w3_div('',
                  '<b>Automatically check for software updates?</b> ' +
                  w3_switch('Yes', 'No', 'adm.update_check', adm.update_check, 'admin_radio_YN_cb')
            ),
				w3_div('',
                  '<b>Automatically install software updates?</b> ' +
                  w3_switch('Yes', 'No', 'adm.update_install', adm.update_install, 'admin_radio_YN_cb')
            )
         ),
         w3_half('w3-container', 'w3-text-teal',
            w3_div('',
               w3_select_psa('w3-label-inline', 'After update', '', 'adm.update_restart', adm.update_restart, update_restart_u, 'config_select_cb')
            ),
            ''
         )
		) +
		'<hr>' +
		w3_half('w3-container', 'w3-text-teal',
			w3_divs('w3-vcenter', '',
				'<b>Check for software update </b> ' +
				w3_button('w3-aqua w3-margin', 'Check now', 'update_check_now_cb')
			),
			w3_divs('w3-vcenter', '',
				'<b>Force software build </b> ' +
				w3_button('w3-aqua w3-margin', 'Build now', 'update_build_now_cb')
			)
		) +
		'<hr>' +
		w3_divs('w3-container', '', 'TODO: alt github name') +
		'<hr>'
	);
	return s;
}

var update_restart_u = { 0: 'restart server', 1: 'reboot Beagle' };

function update_check_now_cb(id, idx)
{
	ext_send('SET force_check=1 force_build=0');
	w3_el_id('msg-update').innerHTML = w3_icon('', 'fa-refresh fa-spin', '', 20);
}

function update_build_now_cb(id, idx)
{
	ext_send('SET force_check=1 force_build=1');
	if (adm.update_restart == 0)
	   w3_show_block('id-build-restart');
	else
	   w3_show_block('id-build-reboot');
}


////////////////////////////////
// backup
////////////////////////////////

function backup_html()
{
	var s =
	w3_divs('id-backup w3-hide', '',
		'<hr>',
		w3_div('w3-section w3-text-teal w3-bold', 'Backup complete contents of KiwiSDR by writing Beagle filesystem onto a user provided micro-SD card'),
		w3_text('w3-container w3-red', 'WARNING: after SD card is written immediately remove from Beagle.<br>Otherwise on next reboot Beagle will be re-flashed from SD card.'),
		'<hr>',
		w3_third('w3-container', 'w3-vcenter',
			w3_button('w3-aqua w3-margin', 'Click to write micro-SD card', 'backup_sd_write'),

			w3_divs('', '',
				w3_divs('id-progress-container w3-progress-container w3-round-large w3-gray w3-show-inline-block', '',
					w3_divs('id-progress w3-progressbar w3-round-large w3-light-green w3-width-zero', '',
						w3_div('id-progress-text w3-container')
					)
				),
				w3_div('id-progress-time w3-inline') +
				w3_div('id-progress-icon w3-inline w3-margin-left')
			),

			w3_div('id-sd-status class-sd-status')
		),
		'<hr>',
		'<div id="id-status-msg" class="w3-container w3-text-output w3-scroll-down w3-small w3-margin-B-16"></div>'
	);
	return s;
}

function backup_focus()
{
	w3_el_id('id-progress-container').style.width = px(300);
	w3_el_id('id-status-msg').style.height = px(300);
}

var sd_progress, sd_progress_max = 4*60;		// measured estimate -- in secs (varies with SD card write speed)
var backup_sd_interval;
var backup_refresh_icon = w3_icon('', 'fa-refresh fa-spin', '', 20);

function backup_sd_write(id, idx)
{
	var el = w3_el_id('id-sd-status');
	el.innerHTML = "writing the micro-SD card...";

	w3_el_id('id-progress-text').innerHTML = w3_el_id('id-progress').style.width = '0%';

	sd_progress = -1;
	backup_sd_progress();
	backup_sd_interval = setInterval(backup_sd_progress, 1000);

	w3_el_id('id-progress-icon').innerHTML = backup_refresh_icon;

	ext_send("SET microSD_write");
}

function backup_sd_progress()
{
	sd_progress++;
	var pct = ((sd_progress / sd_progress_max) * 100).toFixed(0);
	if (pct <= 95) {	// stall updates until we actually finish in case SD is writing slowly
		w3_el_id('id-progress-text').innerHTML = w3_el_id('id-progress').style.width = pct +'%';
	}
	w3_el_id('id-progress-time').innerHTML =
		((sd_progress / 60) % 60).toFixed(0) +':'+ (sd_progress % 60).toFixed(0).leadingZeros(2);
}

function backup_sd_write_done(err)
{
	var el = w3_el_id('id-sd-status');
	var msg = err? ('FAILED error '+ err.toString()) : 'WORKED';
	if (err == 1) msg += '<br>No SD card inserted?';
	if (err == 15) msg += '<br>rsync I/O error';
	el.innerHTML = msg;
	el.style.color = err? 'red':'lime';

	if (!err) {
		// force to max in case we never made it during updates
		w3_el_id('id-progress-text').innerHTML = w3_el_id('id-progress').style.width = '100%';
	}
	kiwi_clearInterval(backup_sd_interval);
	w3_el_id('id-progress-icon').innerHTML = '';
}


////////////////////////////////
// GPS
//		tracking tasks aren't stopped when !enabled
////////////////////////////////

function gps_html()
{
	var s =
	w3_divs('id-gps w3-hide', '',
		w3_divs('w3-section w3-container w3-text-teal', '',
				'<b>Enable GPS?</b> ' +
				w3_switch('Yes', 'No', 'adm.enable_gps', adm.enable_gps, 'admin_radio_YN_cb')
		) +

		w3_divs('w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue', '',
			w3_table('id-gps-ch w3-table w3-striped')
		) +

		w3_divs('w3-container w3-section w3-card-8 w3-round-xlarge w3-pale-blue', '',
			w3_table('id-gps-info w3-table')
		)
	);
	return s;
}

var gps_interval;

function gps_focus(id)
{
	// only get updates while the gps tab is selected
	ext_send("SET gps_update");
	gps_interval = setInterval(function() {ext_send("SET gps_update");}, 1000);
}

function gps_blur(id)
{
	kiwi_clearInterval(gps_interval);
}

var SUBFRAMES = 5;
var max_rssi = 1;

var refresh_icon = w3_icon('', 'fa-refresh', '', 20);

var sub_colors = [ 'w3-red', 'w3-green', 'w3-blue', 'w3-yellow', 'w3-orange' ];

function gps_update_admin_cb()
{
	var i, s;

	s =
		w3_table_row('',
			w3_table_heads('', 'ch', 'acq', 'PRN', 'SNR', 'gain', 'hold', 'wdog'),
			w3_table_heads('w3-center', 'err', 'subframe'),
			w3_table_heads('', 'novfl'),
			w3_table_heads('|width:50%', 'RSSI')
		);
	
      for (var cn=0; cn < gps.ch.length; cn++) {
         s += w3_table_row('id-gps-ch-'+ cn, '');
      }

	w3_el_id("id-gps-ch").innerHTML = s;

	for (var cn=0; cn < gps.ch.length; cn++) {
		var ch = gps.ch[cn];

		if (ch.rssi > max_rssi)
			max_rssi = ch.rssi;
	
		var cells =
			w3_table_cells('w3-right-align', cn) +
			w3_table_cells('w3-center', (cn == gps.FFTch)? refresh_icon:'') +
			w3_table_cells('w3-right-align',
				ch.prn? ch.prn:'',
				ch.snr? ch.snr:'',
				ch.rssi? ch.gain:'',
				ch.hold? ch.hold:'',
				ch.rssi? ch.wdog:''
			) +
			w3_table_cells('',
				'<span class="w3-tag '+ (ch.unlock? 'w3-red':'w3-white') +'">U</span>' +
				'<span class="w3-tag '+ (ch.parity? 'w3-red':'w3-white') +'">P</span>'
			);
	
		var sub = '';
		for (i = SUBFRAMES-1; i >= 0; i--) {
			var sub_color;
			if (ch.sub_renew & (1<<i)) {
				sub_color = 'w3-grey';
			} else {
				sub_color = (ch.sub & (1<<i))? sub_colors[i]:'w3-white';
			}
			sub += '<span class="w3-tag '+ sub_color +'">'+ (i+1) +'</span>';
		}
		cells +=
			w3_table_cells('w3-right-align', sub);
	
		var pct = ((ch.rssi / max_rssi) * 100).toFixed(0);
		cells +=
			w3_table_cells('w3-right-align', ch.novfl? ch.novfl:'') +
			
			w3_table_cells('',
				'<div class="w3-progress-container w3-round-xlarge w3-white">' +
					'<div class="w3-progressbar w3-round-xlarge w3-light-green" style="width:'+ pct +'%">' +
						'<div class="w3-container w3-text-white">'+ ch.rssi +'</div>' +
					'</div>' +
				'</div>'
			);
		
		w3_el_id('id-gps-ch-'+ cn).innerHTML = cells;
	}

	el = w3_el_id("id-gps-info");
	s =
		w3_table_row('',
			w3_table_heads('', 'acq', 'tracking', 'good', 'fixes', 'run', 'TTFF', 'UTC offset',
				'ADC clock', 'lat', 'lon', 'alt', 'map')
		) +
		
		w3_table_row('',
			w3_table_cells('',
				gps.acq? 'yes':'paused',
				gps.track? gps.track:'',
				gps.good? gps.good:'',
				gps.fixes? gps.fixes.toUnits():'',
				gps.run,
				gps.ttff? gps.ttff:'',
			//	gps.gpstime? gps.gpstime:'',
				gps.utc_offset? gps.utc_offset:'',
				gps.adc_clk.toFixed(6) +' ('+ gps.adc_corr.toUnits() +')',
				gps.lat? gps.lat:'',
				gps.lat? gps.lon:'',
				gps.lat? gps.alt:'',
				gps.lat? gps.map:''
			)
		);
	el.innerHTML = s;
}


////////////////////////////////
// log
////////////////////////////////

var log = { };
var nlog = 256;

function log_html()
{
	var s =
	w3_divs('id-log w3-text-teal w3-hide', '',
		'<hr>'+
		w3_divs('', 'w3-container',
		   w3_div('',
            w3_label('w3-show-inline', 'KiwiSDR server log (scrollable list, first and last set of messages)') +
            w3_button('w3-aqua|margin-left:10px', 'Dump', 'log_dump_cb') +
            w3_button('w3-blue|margin-left:10px', 'Clear Histogram', 'log_clear_hist_cb')
         ),
			w3_divs('', 'id-log-msg w3-margin-T-8 w3-text-output w3-small w3-text-black', '')
		)
	);
	return s;
}

function log_setup()
{
	var el = w3_el_id('id-log-msg');
	var s = '<pre>';
		for (var i = 0; i < nlog; i++) {
			if (i == nlog/2) s += '<code id="id-log-not-shown"></code>';
			s += '<code id="id-log-'+ i +'"></code>';
		}
	s += '</pre>';
	el.innerHTML = s;

	ext_send('SET log_update=1');
}

function log_dump_cb(id, idx)
{
	ext_send('SET log_dump');
}

function log_clear_hist_cb(id, idx)
{
	ext_send('SET log_clear_hist');
}


function log_resize()
{
	var el = w3_el_id('id-log-msg');
	if (!el) return;
	var log_height = window.innerHeight - w3_el_id("id-admin-header-container").clientHeight - 100;
	el.style.height = px(log_height);
}

var log_interval;

function log_focus(id)
{
	log_resize();
	log_update();
	log_interval = setInterval(log_update, 3000);
}

function log_blur(id)
{
	kiwi_clearInterval(log_interval);
}

function log_update()
{
	ext_send('SET log_update=0');
}


////////////////////////////////
// console
////////////////////////////////

// must set "remove_returns" since pty output lines are terminated with \r\n instead of \n alone
// otherwise the \r overwrite logic in kiwi_status_msg() will be triggered
var console_status_msg_p = { process_return_nexttime: false, remove_returns: true, ncol: 160 };

function console_html()
{
	var s =
	w3_divs('id-console w3-section w3-text-teal w3-hide', '',
		w3_divs('', 'w3-container',
		   w3_div('',
            w3_label('w3-show-inline', 'Beagle Debian console') +
            w3_button('w3-aqua|margin-left:10px', 'Connect', 'console_connect_cb')
         ),
			w3_divs('', 'id-console-msg w3-margin-T-8 w3-text-output w3-scroll-down w3-small w3-text-black',
			   '<pre><code id="id-console-msgs"></code></pre>'
			),
         w3_divs('w3-margin-top', '',
            w3_input('', 'console_input', '', 'console_input_cb', 'enter shell command')
         ),
		   w3_div('w3-margin-top',
            w3_button('w3-yellow', 'Send ^C', 'console_ctrl_C_cb') +
            w3_button('w3-red|margin-left:10px', 'Send ^\\', 'console_ctrl_backslash_cb')
         )
		)
	);
	return s;
}

function console_input_cb(path, val)
{
	//console.log('console_w2c='+ val);
	ext_send('SET console_w2c='+ encodeURIComponent(val +'\n'));
   w3_set_value(path, '');    // erase input field
}

function console_connect_cb()
{
	ext_send('SET console_open');
}

function console_ctrl_C_cb()
{
	ext_send('SET console_ctrl_C');
}

function console_ctrl_backslash_cb()
{
	ext_send('SET console_ctrl_backslash');
}

function console_setup()
{
}

function console_resize()
{
	var el = w3_el_id('id-console-msg');
	if (!el) return;
	var console_height = window.innerHeight - w3_el_id("id-admin-header-container").clientHeight - 200;
	el.style.height = px(console_height);
}

function console_focus(id)
{
	console_resize();
}

function console_blur(id)
{
}


////////////////////////////////
// extensions
////////////////////////////////

function extensions_html()
{
	var s =
	w3_divs('id-admin-ext w3-hide w3-section', '',
		'<nav class="id-admin-ext-nav w3-sidenav w3-light-grey"></nav>' +
		w3_divs('id-admin-ext-config', '')
	);
	return s;
}

var ext_seq = 0;
var admin_colors = [
	'w3-hover-red',
	'w3-hover-blue',
	'w3-hover-purple',
	'w3-hover-black',
	'w3-hover-aqua',
	'w3-hover-pink',
	'w3-hover-yellow',
	'w3-hover-khaki',
	'w3-hover-green',
	'w3-hover-orange',
	'w3-hover-grey',
	'w3-hover-lime',
	'w3-hover-indigo',
	'w3-hover-brown',
	'w3-hover-teal'
];

function ext_admin_config(id, nav_name, ext_html)
{
	var ci = ext_seq % admin_colors.length;
	w3_el_id('id-admin-ext-nav').innerHTML +=
		w3_sidenav(id, nav_name, admin_colors[ci] + ((ci&1)? ' w3-ext-lightGray':''));
	ext_seq++;
	w3_el_id('id-admin-ext-config').innerHTML += ext_html;
}


////////////////////////////////
// security
////////////////////////////////

// FIXME: breaks if we someday add more channels
var chan_no_pwd_u = { 0:'none', 1:'1', 2:'2', 3:'3' };

function security_html()
{
	var chan_no_pwd = ext_get_cfg_param('chan_no_pwd', 0);

	var s =
	w3_divs('id-security w3-hide', '',
		'<hr>' +
		w3_col_percent('w3-container w3-vcenter', 'w3-hspace-16 w3-text-teal',
			w3_divs('', '',
				w3_divs('', '',
					'<b>User auto-login from local net<br>even if password set?</b><br>',
					w3_divs('w3-margin-T-8', '',
						w3_switch('Yes', 'No', 'adm.user_auto_login', adm.user_auto_login, 'admin_radio_YN_cb')
					)
				)
			), 25,

			w3_divs('', 'w3-center',
				w3_select('Number of channels<br>not requiring a password<br>even if password set',
					'', 'chan_no_pwd', chan_no_pwd, chan_no_pwd_u, 'config_select_cb'),
				w3_divs('', 'w3-margin-T-8 w3-text-black',
					'Set this and a password to create a two sets of channels, ' +
					'some that have open-access requiring no password and some that are password protected.'
				)
			), 25,

			w3_divs('', '',
				w3_input('User password', 'adm.user_password', '', 'w3_string_set_cfg_cb',
					'No password set: unrestricted Internet access to SDR')
			), 50
		) +
		'<hr>' +
		w3_col_percent('w3-container w3-vcenter', 'w3-hspace-16 w3-text-teal',
			w3_divs('', '',
				w3_divs('', '',
					'<b>Admin auto-login from local net<br>even if password set?</b><br>',
					w3_divs('w3-margin-T-8', '',
						w3_switch('Yes', 'No', 'adm.admin_auto_login', adm.admin_auto_login, 'admin_radio_YN_cb')
					)
				)
			), 25,

			w3_divs('w3-text-teal', '',
				''
			), 25,

			w3_divs('', '',
				w3_input('Admin password', 'adm.admin_password', '', 'w3_string_set_cfg_cb',
					'No password set: no admin access from Internet allowed')
			), 50
		) +
		'<hr>' +
		//w3_divs('id-security-json w3-section w3-border', '')
		''
	);
	return s;
}

function security_focus(id)
{
	admin_set_decoded_value('adm.user_password');
	admin_set_decoded_value('adm.admin_password');
	//w3_el_id('id-security-json').innerHTML = w3_divs('w3-padding w3-scroll', '', JSON.stringify(cfg));
}


////////////////////////////////
// admin
////////////////////////////////

function admin_main()
{
	window.addEventListener("resize", admin_resize);
}

function admin_resize()
{
	log_resize();
	console_resize();
}

function kiwi_ws_open(conn_type, cb, cbp)
{
	return open_websocket(conn_type, cb, cbp, null, admin_recv);
}

function admin_draw()
{
	var admin = w3_el_id("id-admin");
	var ci = 0;
	admin.innerHTML =
		w3_divs('id-admin-header-container', '',
			'<header class="w3-container w3-teal"><h5>Admin interface</h5></header>' +
			'<nav class="w3-navbar w3-border w3-light-grey">' +
				w3_navdef('status', 'Status', admin_colors[ci++]) +
				w3_nav('control', 'Control', admin_colors[ci++]) +
				w3_nav('config', 'Config', admin_colors[ci++]) +
				//w3_nav('channels', 'Channels', admin_colors[ci++]) +
				w3_nav('webpage', 'Webpage', admin_colors[ci++]) +
				w3_nav('connect', 'Connect', admin_colors[ci++]) +
				w3_nav('sdr_hu', 'sdr.hu', admin_colors[ci++]) +
				w3_nav('dx', 'DX list', admin_colors[ci++]) +
				w3_nav('update', 'Update', admin_colors[ci++]) +
				w3_nav('backup', 'Backup', admin_colors[ci++]) +
				w3_nav('network', 'Network', admin_colors[ci++]) +
				w3_nav('gps', 'GPS', admin_colors[ci++]) +
				w3_nav('log', 'Log', admin_colors[ci++]) +
				w3_nav('console', 'Console', admin_colors[ci++]) +
				w3_nav('admin-ext', 'Extensions', admin_colors[ci++]) +
				w3_nav('security', 'Security', admin_colors[ci++]) +
			'</nav>' +
	
			w3_divs('id-restart w3-hide', 'w3-vcenter',
				'<header class="w3-show-inline-block w3-container w3-red"><h5>Restart required for changes to take effect</h5></header>' +
				w3_div('w3-inline', w3_button('w3-aqua w3-margin', 'KiwiSDR server restart', 'admin_restart_now_cb'))
			) +
			
			w3_divs('id-reboot w3-hide', 'w3-vcenter',
				'<header class="w3-show-inline-block w3-container w3-red"><h5>Reboot required for changes to take effect</h5></header>' +
				w3_div('w3-inline', w3_button('w3-blue w3-margin', 'Beagle reboot', 'admin_reboot_now_cb'))
			) +
			
			w3_divs('id-build-restart w3-vcenter w3-hide', '',
				'<header class="w3-container w3-blue"><h5>Server will restart after build</h5></header>'
			) +

			w3_divs('id-build-reboot w3-vcenter w3-hide', '',
				'<header class="w3-container w3-red"><h5>Beagle will reboot after build</h5></header>'
			)
		);
		
	admin.innerHTML +=
		status_html() +
		control_html() +
		config_html() +
		//channels_html() +
		webpage_html() +
		connect_html() +
		sdr_hu_html() +
		dx_html() +
		network_html() +
		update_html() +
		backup_html() +
		gps_html() +
		log_html() +
		console_html() +
		extensions_html() +
		security_html();

	log_setup();
	console_setup();
	users_init(true);
	stats_init();

	//admin.style.top = admin.style.left = '10px';
	var i1 = html('id-info-1');
	var i2 = html('id-info-2');
	//i1.style.position = i2.style.position = 'static';
	//i1.style.fontSize = i2.style.fontSize = '100%';
	//i1.style.color = i2.style.color = 'white';
	visible_block('id-admin', 1);
	
	setTimeout(function() { setInterval(status_periodic, 1000); }, 1000);
}

var log_msg_idx, log_msg_not_shown = 0;

// after calling admin_main(), server will download cfg and adm state to us, then send 'init' message
function admin_recv(data)
{
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	//console.log('admin_recv: '+ stringData);

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		//console.log('admin_recv: '+ param[0]);
		switch (param[0]) {

			case "init":
				rx_chans = rx_chan = param[1];
				//console.log("ADMIN init rx_chans="+rx_chans);
				
				if (rx_chans == -1) {
					var admin = w3_el_id("id-admin");
					admin.innerHTML =
						'<header class="w3-container w3-red"><h5>Admin interface</h5></header>' +
						'<p>To use the new admin interface you must edit the configuration ' +
						'parameters from your current kiwi.config/kiwi.cfg into kiwi.config/kiwi.json<br>' +
						'Use the file kiwi.config/kiwi.template.json as a guide.</p>';
				} else {
					admin_draw();
					ext_send('SET extint_load_extension_configs');
				}
				break;

			case "ext_config_html":
				var ext_name = decodeURIComponent(param[1]);
				//console.log('ext_config_html name='+ ext_name);
				w3_call(ext_name +'_config_html');
				break;

			case "sdr_hu_update":
				sdr_hu_update(param[1]);
				break;

			case "auto_nat":
				var p = +param[1];
				var el = w3_el_id('id-net-auto-nat-msg');
				var msg, color;
				
				switch (p) {
					case 0: break;
					case 1: msg = 'succeeded'; color = 'w3-green'; break;
					case 2: msg = 'no device found'; color = 'w3-orange'; break;
					case 3: msg = 'rule already exists'; color = 'w3-yellow'; break;
					case 4: msg = 'command failed'; color = 'w3-red'; break;
					default: break;
				}
				
				if (p) {
					el.innerHTML += msg;
					el = w3_el_id('id-net-auto-nat');
					w3_class(el, color);
					w3_show_block(el);
				}
				break;

			case "log_msg_not_shown":
				log_msg_not_shown = parseInt(param[1]);
				if (log_msg_not_shown) {
					var el = w3_el_id('id-log-not-shown');
					el.innerHTML = '---- '+ log_msg_not_shown.toString() +' lines not shown ----\n'
				}
				break;

			case "log_msg_idx":
				log_msg_idx = parseInt(param[1]);
				break;

			case "log_msg_save":
				var el = w3_el_id('id-log-'+ log_msg_idx);
				if (!el) break;
				var el2 = w3_el_id('id-log-msg');
				var wasScrolledDown = kiwi_isScrolledDown(el2);
				var s = decodeURIComponent(param[1]).replace(/</g, '&lt;').replace(/>/g, '&gt;');
				el.innerHTML = s;

				// only jump to bottom of updated list if it was already sitting at the bottom
				if (wasScrolledDown) el2.scrollTop = el2.scrollHeight;
				break;

			case "log_update":
				log_update(param[1]);
				break;

			case "microSD_done":
				backup_sd_write_done(parseFloat(param[1]));
				break;

			case "DUC_status":
				connect_DUC_status_cb(parseFloat(param[1]));
				break;

			case "rev_status":
				connect_rev_status_cb(parseFloat(param[1]));
				break;

			case "check_port_status":
				network_check_port_status_cb(parseInt(param[1]));
				break;
				
			case "console_c2w":
		      console_status_msg_p.s = param[1];
		      //console.log('console_c2w:');
		      //console.log(console_status_msg_p);
				kiwi_status_msg('id-console-msgs', 'id-console-msg', console_status_msg_p);
				break;

			case "console_done":
			   console.log('## console_done');
				break;

			default:
				console.log('ADMIN UNKNOWN: '+ param[0] +'='+ param[1]);
				break;
		}
	}
}

// callback when a control has w3-restart property
function w3_restart_cb()
{
	w3_show_block('id-restart');
}

// callback when a control has w3-reboot property
function w3_reboot_cb()
{
	w3_show_block('id-reboot');
}

var pending_restart = false;
var pending_reboot = false;
var pending_power_off = false;

function admin_restart_cb()
{
	pending_restart = true;
	w3_show_block('id-confirm');
}

function admin_reboot_cb()
{
	pending_reboot = true;
	w3_show_block('id-confirm');
}

function admin_power_off_cb()
{
	pending_power_off = true;
	w3_show_block('id-confirm');
}

var admin_pie_size = 25;
var admin_reload_secs, admin_reload_rem;

function admin_draw_pie() {
	w3_el_id('id-admin-reload-secs').innerHTML = 'Admin page reload in '+ admin_reload_rem + ' secs';
	if (admin_reload_rem > 0) {
		admin_reload_rem--;
		kiwi_draw_pie('id-admin-pie', admin_pie_size, (admin_reload_secs - admin_reload_rem) / admin_reload_secs);
	} else {
		window.location.reload(true);
	}
};

function admin_wait_then_reload(secs, msg)
{
	var admin = w3_el_id("id-admin");
	var s2;
	
	if (secs) {
		s2 =
			w3_divs('w3-vcenter w3-margin-T-8', 'w3-container',
				w3_div('w3-inline', kiwi_pie('id-admin-pie', admin_pie_size, '#eeeeee', 'deepSkyBlue')),
				w3_div('w3-inline',
					w3_div('id-admin-reload-msg'),
					w3_div('id-admin-reload-secs')
				)
			);
	} else {
		s2 =
			w3_divs('w3-vcenter w3-margin-T-8', 'w3-container',
				w3_div('id-admin-reload-msg')
			);
	}
	
	var s = '<header class="w3-container w3-teal"><h5>Admin interface</h5></header>'+ s2;
	//console.log('s='+ s);
	admin.innerHTML = s;
	
	if (msg) w3_el_id("id-admin-reload-msg").innerHTML = msg;
	
	if (secs) {
		admin_reload_rem = admin_reload_secs = secs;
		setInterval(admin_draw_pie, 1000);
		admin_draw_pie();
	}
}

function admin_restart_now_cb()
{
	ext_send('SET restart');
	admin_wait_then_reload(60, 'Restarting KiwiSDR server');
}

function admin_reboot_now_cb()
{
	ext_send('SET reboot');
	admin_wait_then_reload(90, 'Rebooting Beagle');
}

function admin_confirm_cb()
{
	if (pending_restart) {
		admin_restart_now_cb();
	} else
	if (pending_reboot) {
		admin_reboot_now_cb();
	} else
	if (pending_power_off) {
		ext_send('SET power_off');
		admin_wait_then_reload(0, 'Powering off Beagle');
	}
}

function admin_int_cb(path, val, first)
{
	var v = parseInt(val);
	//console.log('admin_int_cb '+ typeof val +' "'+ val +'" '+ v);
	if (isNaN(v)) v = null;

	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, v, save);
}

function admin_float_cb(path, val, first)
{
	var v = parseFloat(val);
	//console.log('admin_float_cb '+ typeof val +' "'+ val +'" '+ v);
	if (isNaN(v)) v = null;

	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, val, save);
}

function admin_bool_cb(path, val, first)
{
	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	//console.log('admin_bool_cb path='+ path +' val='+ val +' save='+ save);
	ext_set_cfg_param(path, val? true:false, save);
}

function admin_set_decoded_value(path)
{
	var val = ext_get_cfg_param(path);
	//console.log('admin_set_decoded_value: path='+ path +' val='+ val);
	w3_set_decoded_value(path, val);
}

// translate radio button yes/no index to bool value
function admin_radio_YN_cb(id, idx)
{
	admin_bool_cb(id, idx? 0:1);     // idx: 0 = 'yes', 1 = 'no'
}

function admin_preview_status_box(val)
{
	var s = decodeURIComponent(val);
	if (!s || s == '') s = '&nbsp;';
	return s;
}
