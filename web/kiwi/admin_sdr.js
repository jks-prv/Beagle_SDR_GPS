// Copyright (c) 2016 John Seamons, ZL/KF6VO

// TODO
//		input range validation
//		NTP status?


////////////////////////////////
// config
////////////////////////////////

var ITU_region_i = { 0:'R1: Europe, Africa', 1:'R2: North & South America', 2:'R3: Asia / Pacific' };

var AM_BCB_chan_i = { 0:'9 kHz', 1:'10 kHz' };

var max_freq_i = { 0:'30 MHz', 1:'32 MHz' };

var SPI_clock_i = { 0:'48 MHz', 1:'24 MHz' };

var led_brightness_i = { 0:'brighest', 1:'medium', 2:'dimmer', 3:'dimmest', 4:'off' };

var clone_host = '', clone_pwd = '';
var clone_files_s = [ 'complete config', 'dx labels only' ];

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
			w3_input_get('', 'Initial frequency (kHz)', 'init.freq', 'admin_float_cb'),
			w3_div('w3-center',
				w3_select('', 'Initial mode', '', 'init.mode', init_mode, modes_u, 'admin_select_cb')
			),
			w3_input_get('', 'Initial zoom (0-11)', 'init.zoom', 'admin_int_cb')
		) +

		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_input_get('', 'Initial waterfall min (dBFS, fully zoomed-out)', 'init.min_dB', 'admin_int_cb'),
			w3_input_get('', 'Initial waterfall max (dBFS)', 'init.max_dB', 'admin_int_cb'),
			w3_div('w3-center',
				w3_select('', 'Initial AM BCB channel spacing', '', 'init.AM_BCB_chan', init_AM_BCB_chan, AM_BCB_chan_i, 'admin_select_cb')
			)
		);

   var s2 =
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_div('w3-restart',
				w3_input_get('', 'Frequency scale offset (kHz)', 'freq_offset', 'admin_int_cb'),
				w3_div('w3-text-black',
					'Adds offset to frequency scale. <br> Useful when using a transverter, e.g. set to <br>' +
					'116000 kHz when 144-148 maps to 28-32 MHz.'
				)
			),
			w3_input_get('', 'S-meter calibration (dB)', 'S_meter_cal', 'admin_int_cb'),
			w3_input_get('', 'Waterfall calibration (dB)', 'waterfall_cal', 'admin_int_cb')
		) +
		w3_quarter('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_div('w3-center w3-tspace-8',
				w3_select('', 'ITU region', '', 'init.ITU_region', init_ITU_region, ITU_region_i, 'admin_select_cb'),
				w3_div('w3-text-black',
					'Configures LW/NDB, MW and <br> amateur band allocations, etc.'
				)
			),
			w3_divs('w3-restart/w3-center w3-tspace-8',
				w3_select('', 'Max receiver frequency', '', 'max_freq', max_freq, max_freq_i, 'admin_select_cb'),
				w3_div('w3-text-black')
			),
			w3_divs('w3-restart/w3-center w3-tspace-8',
				w3_select_get_param('', 'SPI clock', '', 'SPI_clock', SPI_clock_i, 'admin_select_cb', 0),
				w3_div('w3-text-black',
					'Set to 24 MHz to reduce interference <br> on 2 meters (144-148 MHz).'
				)
			),
			w3_divs('w3-restart/w3-center w3-tspace-8',
				w3_select_get_param('', 'Status LED brightness', '', 'led_brightness', led_brightness_i, 'admin_select_cb', 0),
				w3_div('w3-text-black',
					'Sets brightness of the 4 LEDs <br> that show status info.'
				)
			)
		);

   var s3 =
		'<hr>' +
      w3_div('w3-valign w3-container w3-section',
         '<header class="w3-container w3-yellow"><h6>' +
         'Clone configuration from another Kiwi. <b>Use with care.</b> Current configuration is <b><i>not</i></b> saved. ' +
         'This Kiwi immediately restarts after cloning.' +
         '</h6></header>'
      ) +
		w3_inline_percent('w3-text-teal/w3-container',
			w3_input('', 'Clone config from Kiwi host', 'clone_host', '', 'w3_string_cb', 'enter hostname (no port number)'), 25,
			w3_input('', 'Kiwi host root password', 'clone_pwd', '', 'w3_string_cb', 'can be blank'), 25,
         w3_select('w3-center//', 'Config to clone', '', 'clone_files', 0, clone_files_s, 'w3_num_cb'), 15,
         w3_button('w3-center//w3-red', 'Clone', 'config_clone_cb'), 10,
         w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', 'Status:') +
         w3_div('id-config-clone-status w3-show-inline-block w3-text-black w3-background-pale-aqua', ''), 25
		) +
		w3_inline_percent('w3-margin-bottom w3-text-teal/w3-container',
		   '', 25,
         w3_div('w3-center w3-text-black',
            'Not the same as Kiwi admin password.<br>Leave blank unless you\'ve explicitly changed host\'s Beagle root password.'
         ), 25
		);

   // FIXME: this should really be in a tab defined by admin.js
   // but don't move it without leaving an explanation since old forum posts may refer to it as being here
   var s4 =
		'<hr>' +
      w3_div('w3-valign w3-container w3-section',
         '<header class="w3-container w3-yellow"><h6>' +
         'If the Kiwi doesn\'t like your external clock you can still connect (user and admin). However the waterfall will be dark and the audio silent.' +
         '</h6></header>'
      ) +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_divs('w3-restart/w3-center w3-tspace-8',
				w3_div('', '<b>External ADC clock?</b>'),
            w3_switch('', 'Yes', 'No', 'ext_ADC_clk', cfg.ext_ADC_clk, 'config_ext_clk_sel_cb'),
				w3_text('w3-text-black w3-center', 'Set when external 66.666600 MHz (nominal) <br> clock connected to J5 connector/pad.')
			),
			w3_divs('w3-restart/w3-tspace-8',
		      w3_input('', 'External clock frequency (enter in MHz or Hz)', 'ext_ADC_freq', cfg.ext_ADC_freq, 'config_ext_freq_cb'),
				w3_text('w3-text-black', 'Set exact clock frequency applied. <br> Input value stored in Hz.')
		   ),
			w3_divs('w3-restart/w3-center w3-tspace-8',
				w3_div('', '<b>Enable GPS correction of ADC clock?</b>'),
            w3_switch('', 'Yes', 'No', 'ADC_clk_corr', cfg.ADC_clk_corr, 'admin_radio_YN_cb'),
				w3_text('w3-text-black w3-center',
				   'Set "no" to keep the Kiwi GPS from correcting for <br>' +
				   'errors in the ADC clock (internal or external).'
				)
			)
		) +
		'<hr>' +
		w3_div('w3-container',
         w3_div('w3-valign',
            '<header class="w3-container w3-yellow"><h6>' +
            'To manually adjust/calibrate the ADC clock (e.g. when there is no GPS signal or GPS correction is disabled) follow these steps:' +
            '</h6></header>'
         ),
         
         w3_label('w3-text-teal',
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
               '<li>Press the <i>40</i> button (i.e. sets mode to AM with 40 Hz passband)</li>' +
               '<li>Set menus: Draw = points, Mode = carrier, PLL = off</li>' +
               '<li>Adjust the gain until you see a point rotating in a circle</li>' +
               '<li>Use the <i>Fcal</i> buttons to slow the rotation as much as possible</li>' +
               '<li>The total accumulated Fcal adjustment is shown</li>' +
               '<li>A full rotation in less than two seconds is good calibration</li>' +
            '</ul>'
         )
      );

   var s5 = '<hr>';
   
   var mode_20kHz = (adm.firmware_sel == firmware_sel.RX_3_WF_3)? 1 : 0;
   console.log('mode_20kHz='+ mode_20kHz);
   var DC_offset_I = 'DC_offset'+ (mode_20kHz? '_20kHz':'') +'_I';
   var DC_offset_Q = 'DC_offset'+ (mode_20kHz? '_20kHz':'') +'_Q';

   if (dbgUs) s5 = s5 +
		w3_div('w3-section w3-text-teal w3-bold', 'Development settings') +
		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_input_get('', 'I balance (DC offset)', DC_offset_I, 'admin_float_cb'),
			w3_input_get('', 'Q balance (DC offset)', DC_offset_Q, 'admin_float_cb'),
			''
		) +
		w3_third('w3-margin-bottom w3-text-teal w3-restart', 'w3-container',
			w3_divs('w3-center w3-tspace-8',
				w3_div('', '<b>Increase web server priority?</b>'),
            w3_switch('', 'Yes', 'No', 'test_webserver_prio', cfg.test_webserver_prio, 'admin_radio_YN_cb'),
				w3_text('w3-text-black w3-center', 'Set \'no\' for standard behavior.')
			),
			w3_divs('w3-center w3-tspace-8',
				w3_div('', '<b>New deadline update scheme?</b>'),
            w3_switch('', 'Yes', 'No', 'test_deadline_update', cfg.test_deadline_update, 'admin_radio_YN_cb'),
				w3_text('w3-text-black w3-center', 'Set \'no\' for standard behavior.')
			),
			''
		) +
		'<hr>';

	return w3_div('id-config w3-hide', s1 + s2 + s3 + s4 + s5);
}

function config_clone_cb(id, idx)
{
   var msg;

   if (clone_host == '') {
      msg = 'please enter host to clone from';
   } else {
      msg = 'cloning from '+ clone_host;
      ext_send('SET config_clone host='+ encodeURIComponent(clone_host) +' pwd=x'+ encodeURIComponent(clone_pwd) +' files='+ clone_files);
   }

   w3_innerHTML('id-config-clone-status', msg);
}

function config_clone_status_cb(status)
{
   var msg;
   
   if (status == 0) {
      msg = 'clone complete, restarting Kiwi';
      ext_send('SET restart');
      admin_wait_then_reload(60, 'Configuration cloned, restarting KiwiSDR server');
   } else
   if (status == 0x500)
      msg = 'wrong password';
   else
   if (status == 256)
      msg = 'host unknown/unresponsive';
   else
      msg = 'clone error #'+ status;
   
   w3_innerHTML('id-config-clone-status', msg);
}

function config_ext_clk_sel_cb(path, idx)
{
   idx = +idx;
   //console.log('config_ext_clk_sel_cb idx='+ idx);
   admin_radio_YN_cb(path, idx);
   
   // force clock adjust to zero when changing external clock select
   w3_num_set_cfg_cb('cfg.clk_adj', 0);
}

function config_ext_freq_cb(path, val, first)
{
   if (first) return;
   var f = parseFloat(val);
   //console.log('config_ext_freq_cb f='+ f);
   if (isNaN(f)) {
      f = null;
   } else {
      if (f < 70) f *= 1e6;      // convert MHz to Hz
      else
      if (f < 70000) f *= 1e3;   // convert kHz to Hz
      f = Math.floor(f);
      if (f < 65000000 || f > 69000000) f = null;
   }
   admin_int_cb(path, f, first);
}


////////////////////////////////
// channels [not used currently]
////////////////////////////////

function channels_html()
{
	var s =
	w3_div('id-channels w3-hide',
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
		w3_divs('w3-margin-bottom/w3-container',
			w3_input('', 'Top bar title', 'index_html_params.RX_TITLE', '', 'webpage_title_cb')
		) +
		w3_div('w3-container',
			'<label><b>Top bar title HTML preview</b></label>',
			w3_div('id-webpage-title-preview w3-text-black w3-background-pale-aqua', '')
		) +

		w3_divs('w3-margin-top w3-margin-bottom/w3-container',
			w3_input('',
			   'Owner info (appears in center of top bar; can use HTML like &lt;br&gt; for line break if line is too long)',
			   'owner_info', '', 'webpage_owner_info_cb'
			)
		) +
		w3_div('w3-container',
			'<label><b>Owner info HTML preview</b></label>',
			w3_div('id-webpage-owner-info-preview w3-text-black w3-background-pale-aqua', '')
		) +

		w3_divs('w3-margin-top w3-margin-bottom/w3-container',
			w3_input('', 'Status', 'status_msg', '', 'webpage_status_cb')
		) +
		w3_div('w3-container',
			'<label><b>Status HTML preview</b></label>',
			w3_div('id-webpage-status-preview w3-text-black w3-background-pale-aqua', '')
		) +
		
		w3_divs('w3-margin-top/w3-container',
			w3_input('', 'Window/tab title', 'index_html_params.PAGE_TITLE', '', 'webpage_string_cb')
		);
	
	var s2 =
		'<hr>' +
		w3_half('w3-margin-bottom', 'w3-container',
			w3_input('', 'Location', 'index_html_params.RX_LOC', '', 'webpage_string_cb'),
			w3_input('', w3_label('w3-bold', 'Grid square (4 or 6 char) ') +
			   w3_div('id-webpage-grid-check cl-admin-check w3-show-inline-block w3-green w3-btn w3-round-large'),
			   'index_html_params.RX_QRA', '', 'webpage_input_grid'
			)
		) +
		w3_half('', 'w3-container',
			w3_input('', 'Altitude (ASL meters)', 'index_html_params.RX_ASL', '', 'webpage_string_cb'),
         w3_input('', w3_label('w3-bold', 'Map (Google format or lat, lon) ') +
            w3_div('id-webpage-map-check cl-admin-check w3-show-inline-block w3-green w3-btn w3-round-large'),
            'index_html_params.RX_GMAP', '', 'webpage_input_map'
         )
		) +
		
		'<hr>' +
		w3_half('w3-margin-bottom', 'w3-container',
			w3_half('', '',
            w3_div('',
               w3_label('w3-bold', 'Photo file'),
               '<input id="id-photo-file" type="file" accept="image/*" onchange="webpage_photo_file_upload()"/>',
               w3_div('id-photo-error', '')
            ),
            w3_checkbox_get_param('w3-restart w3-label-inline', 'Photo left margin', 'index_html_params.RX_PHOTO_LEFT_MARGIN', 'admin_bool_cb', true)
         ),
			w3_input('', 'Photo maximum height (pixels)', 'index_html_params.RX_PHOTO_HEIGHT', '', 'webpage_string_cb')
		) +
		w3_half('', 'w3-container',
			w3_input('', 'Photo title', 'index_html_params.RX_PHOTO_TITLE', '', 'webpage_string_cb'),
			w3_input('', 'Photo description', 'index_html_params.RX_PHOTO_DESC', '', 'webpage_string_cb')
		);
		
	var s3 =
		'<hr>' +
      w3_div('w3-container',
         w3_textarea_get_param('w3-input-any-change|width:100%',
            w3_label('w3-show-inline-block w3-bold w3-text-teal', 'Additional HTML/Javascript for HTML &lt;head&gt; element (e.g. Google analytics or user customization)'),
            'index_html_params.HTML_HEAD', 10, 100, 'webpage_string_cb', ''
         )
		) +
		
		w3_divs('w3-margin-bottom/w3-container', '');		// bottom gap for better scrolling look

   return w3_div('id-webpage w3-text-teal w3-hide', s1 + s2 + s3);
}

function webpage_input_grid(path, val)
{
	webpage_string_cb(path, val);
	webpage_update_check_grid();
}

function webpage_update_check_grid()
{
	var grid = ext_get_cfg_param('index_html_params.RX_QRA');
	w3_el('webpage-grid-check').innerHTML = '<a href="http://www.levinecentral.com/ham/grid_square.php?Grid='+ grid +'" target="_blank">check grid</a>';
}

function webpage_input_map(path, val)
{
	webpage_string_cb(path, val);
	webpage_update_check_map();
}

function webpage_update_check_map()
{
	var map = ext_get_cfg_param('index_html_params.RX_GMAP');
	w3_el('webpage-map-check').innerHTML = '<a href="https://google.com/maps/place/'+ map +'" target="_blank">check map</a>';
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
	
	var el = w3_el('photo-error');
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
	w3_add(el, (rc == 0)? 'w3-text-green' : 'w3-text-red');
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
	var browse = w3_el('id-photo-file');
	browse.innerHTML = 'Uploading...';
	var file = browse.files[0];
	var fdata = new FormData();
	fdata.append('photo', file, file.name);
	//console.log(file);

	var el = w3_el('photo-error');
	w3_hide(el);
	w3_remove(el, 'w3-text-red');
	w3_remove(el, 'w3-text-green');

	kiwi_ajax_send(fdata, '/PIX?'+ key, 'webpage_photo_uploaded');
}

function webpage_title_cb(path, val)
{
	webpage_string_cb(path, val);
	w3_el('id-webpage-title-preview').innerHTML = admin_preview_status_box(cfg.index_html_params.RX_TITLE);
}

function webpage_owner_info_cb(path, val)
{
	webpage_string_cb(path, val);
	w3_el('id-webpage-owner-info-preview').innerHTML = admin_preview_status_box(cfg.owner_info);
}

function webpage_status_cb(path, val)
{
	w3_string_set_cfg_cb(path, val);
	w3_el('id-webpage-status-preview').innerHTML = admin_preview_status_box(cfg.status_msg);
}

// because of the inline quoting issue, set value dynamically
function webpage_focus()
{
	admin_set_decoded_value('index_html_params.RX_TITLE');
	w3_el('id-webpage-title-preview').innerHTML = admin_preview_status_box(cfg.index_html_params.RX_TITLE);

	admin_set_decoded_value('status_msg');
	w3_el('id-webpage-status-preview').innerHTML = admin_preview_status_box(cfg.status_msg);

	admin_set_decoded_value('index_html_params.PAGE_TITLE');
	admin_set_decoded_value('index_html_params.RX_LOC');
	admin_set_decoded_value('index_html_params.RX_QRA');
	admin_set_decoded_value('index_html_params.RX_ASL');
	admin_set_decoded_value('index_html_params.RX_GMAP');
	admin_set_decoded_value('index_html_params.RX_PHOTO_HEIGHT');
	admin_set_decoded_value('index_html_params.RX_PHOTO_TITLE');
	admin_set_decoded_value('index_html_params.RX_PHOTO_DESC');

	admin_set_decoded_value('owner_info');
	w3_el('id-webpage-owner-info-preview').innerHTML = admin_preview_status_box(cfg.owner_info);

	webpage_update_check_grid();
	webpage_update_check_map();
}

function webpage_string_cb(path, val)
{
	w3_string_set_cfg_cb(path, val);
	ext_send('SET reload_index_params');
}


////////////////////////////////
// sdr.hu
////////////////////////////////

function sdr_hu_html()
{
	var s1 =
		w3_div('w3-tspace-16',
         w3_div('id-need-gps w3-valign w3-hide',
            '<header class="w3-container w3-yellow"><h5>Warning: GPS location field set to the default, please update</h5></header>'
         ),
         
         w3_div('w3-valign',
            '<header class="w3-container w3-yellow"><h5>' +
            'To list your Kiwi on <a href="https://sdr.hu" target="_blank">sdr.hu</a> ' +
            'edit the fields below and ' +
            'obtain an API key from <a href="https://sdr.hu/register" target="_blank">sdr.hu/register</a> ' +
            'and enter it into the <b>API key</b> field.<br>' +
            'Then set the register switch to <b>Yes</b> and look for a status result of "SUCCESS ..." after a few minutes. ' +
            'More information on <a href="http://kiwisdr.com/quickstart/index.html#id-sdr_hu" target="_blank">kiwisdr.com</a>' +
            '</h5></header>'
         )
      ) +

		'<hr>' +
		w3_half('w3-margin-bottom', '',
			w3_div('w3-container',
					'<b>Register your KiwiSDR on <a href="https://sdr.hu/?top=kiwi" target="_blank">sdr.hu</a>?</b> ' +
					w3_switch('', 'Yes', 'No', 'adm.sdr_hu_register', adm.sdr_hu_register, 'sdr_hu_register_cb')
			),
			w3_div('w3-container',
					'<b>Display owner/admin email link on KiwiSDR main page?</b> ' +
					w3_switch('', 'Yes', 'No', 'contact_admin', cfg.contact_admin, 'admin_radio_YN_cb')
			)
		) +

		w3_div('id-sdr_hu-reg-status-container',
         w3_div('w3-container',
            w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', 'sdr.hu registration status:') +
            w3_div('id-sdr_hu-reg-status w3-show-inline-block w3-text-black', '')
         )
      );
      
   var s2 =
		'<hr>' +
		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('', 'Name', 'rx_name', '', 'w3_string_set_cfg_cb'),
			w3_input('', 'Location', 'rx_location', '', 'w3_string_set_cfg_cb')
		) +

		w3_half('w3-margin-bottom w3-restart', 'w3-container',
			//w3_input('', 'Device', 'rx_device', '', 'w3_string_set_cfg_cb'),
			w3_input('', 'Admin email', 'admin_email', '', 'w3_string_set_cfg_cb'),
			w3_input('', 'Antenna', 'rx_antenna', '', 'w3_string_set_cfg_cb')
		) +

		w3_third('w3-margin-bottom w3-restart', 'w3-container',
			w3_input('', w3_label('w3-bold', 'Grid square (4/6 char) ') +
				w3_div('id-sdr_hu-grid-check cl-admin-check w3-show-inline-block w3-green w3-btn w3-round-large') + ' ' +
				w3_div('id-sdr_hu-grid-set cl-admin-check w3-blue w3-btn w3-round-large w3-hide', 'set from GPS'),
				'rx_grid', '', 'sdr_hu_input_grid'
			),
			w3_div('',
            w3_input('', w3_label('w3-bold', 'Location (lat, lon) ') +
               w3_div('id-sdr_hu-gps-check cl-admin-check w3-show-inline-block w3-green w3-btn w3-round-large') + ' ' +
               w3_div('id-sdr_hu-gps-set cl-admin-check w3-blue w3-btn w3-round-large w3-hide', 'set from GPS'),
               'rx_gps', '', 'sdr_hu_check_gps'
            ),
				w3_div('w3-text-black', 'Format: (nn.nnnnnn, nn.nnnnnn)')
			),
			w3_input_get('', 'Altitude (ASL meters)', 'rx_asl', 'admin_int_cb')
		) +

		w3_half('w3-margin-bottom', 'w3-container',
		   w3_div('w3-restart', w3_input('', 'API key', 'adm.api_key', '', 'w3_string_set_cfg_cb', 'enter value returned from sdr.hu/register process')),
		   ''
		) +
		w3_half('w3-margin-bottom', 'w3-container',
		   w3_div('',
            w3_input_get('', 'Coverage frequency low (kHz)', 'sdr_hu_lo_kHz', 'admin_int_cb'),
				w3_div('w3-text-black',
				   'These two settings effect the frequency coverage label displayed on sdr.hu <br>' +
				   'e.g. when set to 0 and 30000 sdr.hu shows "HF". If you\'re using a transverter <br>' +
				   'then appropriate entries will cause "2m" or "70cm" to be shown. Other labels will be <br>' +
				   'shown if you limit the range at HF due to antenna or filtering limitations.'
				)
			),
         w3_input_get('', 'Coverage frequency high (kHz)', 'sdr_hu_hi_kHz', 'admin_int_cb')
      );

	return w3_div('id-sdr_hu w3-text-teal w3-hide', s1 + s2);
}

function sdr_hu_register_cb(path, idx)
{
   idx = +idx;
   //console.log('sdr_hu_register_cb idx='+ idx);
   
   var text, color;
   if (idx == w3_SWITCH_YES_IDX && cfg.server_url == '') {
      text = 'Error, you must first setup a valid Kiwi connection URL on the admin "connect" tab';
      color = '#ffeb3b';
      w3_switch_set_value(path, w3_SWITCH_NO_IDX);    // force back to 'no'
      idx = w3_SWITCH_NO_IDX;
   } else
   if (idx == w3_SWITCH_YES_IDX) {
      text = '(waiting for sdr.hu response, can take several minutes in some cases)';
      color = 'hsl(180, 100%, 95%)';
   } else {    // w3_SWITCH_NO_IDX
      text = '(registration not enabled)';
      color = 'hsl(180, 100%, 95%)';
   }
   
   w3_innerHTML('id-sdr_hu-reg-status', text);
   w3_color('id-sdr_hu-reg-status', null, color);
   admin_radio_YN_cb(path, idx);
   //console.log('sdr_hu_register_cb adm.sdr_hu_register='+ adm.sdr_hu_register);
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
	
	w3_el('sdr_hu-grid-set').onclick = function() {
		var val = sdr_hu_status.grid;
		w3_set_value('rx_grid', val);
		w3_input_change('rx_grid', 'sdr_hu_input_grid');
	};

	w3_el('sdr_hu-gps-set').onclick = function() {
		var val = '('+ sdr_hu_status.lat +', '+ sdr_hu_status.lon +')';
		w3_set_value('rx_gps', val);
		w3_input_change('rx_gps', 'sdr_hu_check_gps');
	};

	// only get updates while the sdr_hu tab is selected
	ext_send("SET sdr_hu_update");
	sdr_hu_interval = setInterval(function() {ext_send("SET sdr_hu_update");}, 5000);
	
	sdr_hu_register_cb('adm.sdr_hu_register', adm.sdr_hu_register? w3_SWITCH_YES_IDX : w3_SWITCH_NO_IDX);   // display initial message
}

function sdr_hu_input_grid(path, val)
{
	w3_string_set_cfg_cb(path, val);
	sdr_hu_update_check_grid();
}

function sdr_hu_update_check_grid()
{
	var grid = ext_get_cfg_param('rx_grid');
	w3_el('sdr_hu-grid-check').innerHTML = '<a href="http://www.levinecentral.com/ham/grid_square.php?Grid='+ grid +'" target="_blank">check grid</a>';
}

function sdr_hu_check_gps(path, val, first)
{
   var lat = 0, lon = 0;
   var re = /([-]?\d*\.?\d+)/g;
   for (var i = 0; i < 2; i++) {
      var p = re.exec(val);
      //console.log(p);
      if (p) {
         if (i) lon = parseFloat(p[0]); else lat = parseFloat(p[0]);
      }
   }
   
   val = '('+ lat.toFixed(6) +', '+ lon.toFixed(6) +')';

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
	w3_el('sdr_hu-gps-check').innerHTML = '<a href="https://google.com/maps/place/'+ gps +'" target="_blank">check map</a>';
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
	if (adm.sdr_hu_register && sdr_hu_status.reg != undefined && sdr_hu_status.reg != '') {
	   var el = w3_el('id-sdr_hu-reg-status');
	   el.innerHTML = sdr_hu_status.reg;
	}
	
	// GPS has had a solution, show buttons
	if (sdr_hu_status.lat != undefined) {
		w3_show_inline_block('id-sdr_hu-grid-set');
		w3_show_inline_block('id-sdr_hu-gps-set');
	}
}


////////////////////////////////
// dx
////////////////////////////////

function dx_html()
{
	var s =
	w3_div('id-dx w3-hide',
	   w3_div('w3-container w3-margin-top', 'TODO..')
	   /*
		w3_inline('w3-halign-space-between/w3-margin-top',
			w3_inline('/w3-margin-between-16',
				w3_button('w3-yellow', 'Modify', 'dx_modify_cb'),
				w3_button('w3-green', 'Add', 'dx_add_cb'),
				w3_button('w3-red', 'Delete', 'dx_delete_cb')
			),
		   w3_input('w3-text-teal/w3-label-inline/w3-padding-small|width:300px', 'Filter', 'dxo.filter', '', 'dx_filter_cb')
		),
		
		w3_div('w3-container w3-margin-top w3-margin-bottom w3-card-8 w3-round-xlarge w3-pale-blue',
         w3_div('id-dx-list-legend'),
         
         // reminder: "70vh" means 70% of the viewport (browser window) height
         w3_div('id-dx-list w3-margin-bottom|height:70vh;overflow-x:hidden;overflow-y:hidden')
      )
      */
	);
	return s;
}

function dx_focus()
{
   /*
   console.log('### dx_focus: SET GET_DX_JSON');
   w3_innerHTML('id-dx-list-legend', '');
   w3_el('id-dx-list').style.overflowY = 'hidden';
   w3_innerHTML('id-dx-list',
      w3_div('w3-show-inline-block w3-relative|top:45%;left:45%',
         w3_icon('', 'fa-refresh fa-spin', 48, 'teal'),
         w3_div('id-dx-list-count w3_text_black')
      )
   );
   
	ext_send('SET GET_DX_JSON');
   */
}

function dx_hide()
{
}

var dxo = {
};

function dx_json(dx)
{
   /*
   var i, len = dx.dx.length;
   console.log('### dx_json: entries='+ len);
   w3_innerHTML('id-dx-list-count', 'loading '+ len +' entries');
   
   // if this isn't delayed the above innerHTML set of id-dx-list-count doesn't render
   setTimeout(function() { dx_json2(dx); }, 100);
   */
}

function dx_json2(dx)
{
   var i, len = dx.dx.length;
   var s = '';
   
   dxo.tags = [];

   //for (i = -1; i < len; i++) {
   for (i = -1; i < 4; i++) {
      var d = null;
      var fr = '', mo = 0, id = '', no = '';
      var pb = '', ty = 0, os = '', ext = '';
      var ts = 0, tag = '';
      var hide = (i == -1)? 'w3-hide ':'';
      
      // this is so all the s_new code can be reused to construct the legend
      var h = function(psa) { return (i == -1)? 'w3-hide' : psa; }
      var l = function(label) { return (i == -1)? label : ''; }
      if (i != -1) {
         d = dx.dx[i];
         fr = d[0];
         mo = modes_s[d[1].toLowerCase()];
         id = decodeURIComponent(d[2]);
         no = decodeURIComponent(d[3]);
         ts = d[4];
         tag = d[5];
         dxo.tags[i] = tag;
         
         var lo = 0, hi = 0;
         var opt = d[6];
         if (opt) {
            if (opt.WL == 1) ty = types_s.watch_list; else
            if (opt.SB == 1) ty = types_s.sub_band; else
            if (opt.DG == 1) ty = types_s.DGPS; else
            if (opt.NoN == 1) ty = types_s.special_event; else    // deprecated
            if (opt.SE == 1) ty = types_s.special_event; else
            if (opt.XX == 1) ty = types_s.interference; else
            if (opt.MK == 1) ty = types_s.masked; else
            ty = 0;

            if (opt.lo) lo = +opt.lo;
            if (opt.hi) hi = +opt.hi;
            if (opt.o) os = opt.o;
            if (opt.p) ext = opt.p;
         }

         if (lo || hi) {
            if (lo == -hi) {
               pb = (Math.abs(hi)*2).toFixed(0);
            } else {
               pb = lo.toFixed(0) +', '+ hi.toFixed(0);
            }
         }

      }
      
      // 'path'+i so path id is unique for field highlight
      console.log('i='+ i +' mo='+ mo +' ty='+ ty);
      console.log(d);
      var s_new =
         w3_divs('w3-text-teal/w3-margin-T-8',
            w3_col_percent('',
               w3_col_percent('w3-valign/w3-hspace-16',
                  //w3_text('w3-text-black w3-tiny', tag), 5,
                  (i == -1)? '' : w3_button('w3-font-fixed w3-padding-tiny w3-selection-green', '+', 'dx_add_cb', i), 1,
                  (i == -1)? '' : w3_button('w3-font-fixed w3-padding-tiny w3-red', '-', 'dx_rem_cb', i), 1,
                  w3_input(h('w3-padding-small||size=8'), l('Freq'), 'dxo.f_'+i, fr, 'dx_num_cb'), 19,
                  w3_select(h(''), l('Mode'), '', 'dxo.m_'+i, mo, modes_u, 'dx_sel_cb'), 19,
                  w3_input(h('w3-padding-small||size=4'), l('Passband'), 'dxo.pb_'+i, pb, 'dx_passband_cb'), 19,
                  w3_select(h(''), l('Type'), '', 'dxo.y_'+i, ty, types, 'dx_sel_cb'), 19,
                  w3_input(h('w3-padding-small||size=2'), l('Offset'), 'dxo.o_'+i, os, 'dx_num_cb'), 19
               ), 45,
               w3_col_percent('w3-valign/w3-margin-left',
                  w3_input(h('w3-padding-small'), l('Ident'), 'dxo.i_'+i, id, 'dx_string_cb'), 40,
                  w3_input(h('w3-padding-small'), l('Notes'), 'dxo.n_'+i, no, 'dx_string_cb'), 40,
                  w3_input(h('w3-padding-small'), l('Extension'), 'dxo.p_'+i, ext, 'dx_string_cb'), 20
               ), 54
            )
         );
      
      if (i == -1) {
         w3_innerHTML('id-dx-list-legend', s_new);
      } else {
         s += s_new;
      }
   }
   w3_el('id-dx-list').style.overflowY = 'scroll';
   //console.log('render =====================');
   w3_innerHTML('id-dx-list', s);
}

function dx_filter_cb(path, p)
{
   console.log('dx_filter_cb p='+ p);
}

function dx_add_cb(path, p)
{
   console.log('dx_add p='+ p);
}

function dx_rem_cb(path, p)
{
   console.log('dx_rem p='+ p);
}


////////////////////////////////
// extensions
////////////////////////////////

function extensions_html()
{
	var s =
	w3_div('id-extensions w3-hide w3-section',
      w3_sidenav('id-extensions-nav'),
		w3_div('id-extensions-config')
	);
	return s;
}

function extensions_focus()
{
   //console.log('extensions_focus');
   
   // first time after page load ext_admin_config() hasn't been called yet from all extensions
   if (w3_el('id-nav-wspr')) {
      w3_click_nav(kiwi_toggle(toggle_e.FROM_COOKIE | toggle_e.SET, 'wspr', 'wspr', 'last_admin_ext_nav'), 'extensions_nav');
   }
}

var ext_cur_nav;

function extensions_blur()
{
   //console.log('extensions_blur');
   if (ext_cur_nav) w3_call(ext_cur_nav +'_config_blur');
}

function extensions_nav_focus(id, cb_arg)
{
   //console.log('extensions_nav_focus id='+ id +' cb_arg='+ cb_arg);
   writeCookie('last_admin_ext_nav', id);
   w3_call(id +'_config_focus');
   ext_cur_nav = id;
}

function extensions_nav_blur(id, cb_arg)
{
   //console.log('extensions_nav_blur id='+ id);
   w3_call(id +'_config_blur');
}

var ext_seq = 0;

// called by extensions to register extension admin configuration
function ext_admin_config(id, nav_name, ext_html, focus_blur_cb)
{
   //console.log('ext_admin_config id='+ id +' nav_name='+ nav_name);
   // indicate we don't want a callback unless explicitly requested
   if (focus_blur_cb == undefined) focus_blur_cb = null;

	var ci = ext_seq % admin_colors.length;
	w3_el('id-extensions-nav').innerHTML +=
		w3_nav(admin_colors[ci] + ' w3-border', nav_name, id, 'extensions_nav');
	ext_seq++;
	w3_el('id-extensions-config').innerHTML += w3_div('w3-show-inline-block', ext_html);
}
