// Copyright (c) 2016 John Seamons, ZL/KF6VO

// TODO
//		input range validation
//		NTP status?


var admin_sdr = {
   pmi: 0,
   pbm: 'am',
   pbl: 0,
   pbh: 0,
   pbc: 0,
   pbw: 0,
   pbc_lock: false,
   
   // defaults for reset button
   pb: {
      am:   { c:     0, w:  9800 },
      amn:  { c:     0, w:  5000 },
      usb:  { c:  1500, w:  2400 },
      usn:  { c:  1350, w:  2100 },
      lsb:  { c: -1500, w:  2400 },
      lsn:  { c: -1350, w:  2100 },
      cw:   { c:   500, w:   400 },
      cwn:  { c:   500, w:    60 },
      nbfm: { c:     0, w: 12000 },
      iq:   { c:     0, w: 10000 },
      drm:  { c:     0, w: 10000 },
      sam:  { c:     0, w:  9800 },
      sau:  { c:  2450, w:  9800 },
      sal:  { c: -2450, w:  9800 },
      sas:  { c:     0, w:  9800 },
      qam:  { c:     0, w:  9800 }
   },
   
   _last_: 0
};

////////////////////////////////
// config
////////////////////////////////

var ITU_region_i = [ 'R1: Europe, Africa', 'R2: North & South America', 'R3: Asia / Pacific' ];

var AM_BCB_chan_i = [ '9 kHz', '10 kHz' ];

var max_freq_i = [ '30 MHz', '32 MHz' ];

var SPI_clock_i = [ '48 MHz', '24 MHz' ];

var led_brightness_i = [ 'brighest', 'medium', 'dimmer', 'dimmest', 'off' ];

var clone_host = '', clone_pwd = '';
var clone_files_s = [ 'complete config', 'dx labels only' ];

function config_html()
{
	kiwi_get_init_settings();		// make sure defaults exist
	
	var init_mode = ext_get_cfg_param('init.mode', 0);
	var init_colormap = ext_get_cfg_param('init.colormap', 0);
	var init_aperture = ext_get_cfg_param('init.aperture', 1);
	var init_AM_BCB_chan = ext_get_cfg_param('init.AM_BCB_chan', 0);
	var init_ITU_region = ext_get_cfg_param('init.ITU_region', 0);
	var max_freq = ext_get_cfg_param('max_freq', 0);
	var max_freq = ext_get_cfg_param('max_freq', 0);

	var s1 =
		'<hr>' +
		w3_text('w3-margin-B-8 w3-text-teal w3-bold', 'Initial values for:') +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_input_get('', 'Frequency (kHz)', 'init.freq', 'admin_float_cb|3'),
			w3_inline('/w3-halign-space-around/w3-center',
				w3_select('', 'Mode', '', 'init.mode', init_mode, kiwi.modes_u, 'admin_select_cb'),
				w3_select('', 'Colormap', '', 'init.colormap', init_colormap, kiwi.cmap_s, 'admin_select_cb'),
				w3_select('', 'Aperture', '', 'init.aperture', init_aperture, kiwi.aper_s, 'config_aperture_cb')
			),
			w3_div('w3-center w3-tspace-8',
				w3_select('w3-width-auto', 'AM BCB channel spacing', '', 'init.AM_BCB_chan', init_AM_BCB_chan, AM_BCB_chan_i, 'admin_select_cb')
			)
		) +

		w3_third('w3-text-teal', 'w3-container',
		   w3_div('',
            w3_input_get('id-wf-show-min//', 'Waterfall min (dBFS, fully zoomed-out)', 'init.min_dB', 'config_wfmin_cb'),
            w3_input_get('id-wf-show-floor//', 'Waterfall floor (dB)', 'init.floor_dB', 'admin_int_cb')
         ),
		   w3_div('',
            w3_input_get('id-wf-show-max//', 'Waterfall max (dBFS)', 'init.max_dB', 'config_wfmax_cb'),
            w3_input_get('id-wf-show-ceil//', 'Waterfall ceil (dB)', 'init.ceil_dB', 'admin_int_cb')
         ),
			w3_input_get('', 'Zoom (0-14)', 'init.zoom', 'config_zoom_cb')
		) +
		
      w3_third('', 'w3-container',
         w3_div('id-wfmin-error w3-margin-T-8 w3-red w3-hide', 'Waterfall min must be < max'),
         w3_div('id-wfmax-error w3-margin-T-8 w3-red w3-hide', 'Waterfall max must be > min'),
         w3_div('id-zoom-error w3-margin-T-8 w3-red w3-hide', 'Zoom must be 0 to 14')
      ) +
      
      w3_div('w3-margin-bottom');

	var s2 =
		'<hr>' +
		w3_text('w3-text-teal w3-bold', 'Default passbands:') +
		w3_third('w3-text-teal w3-valign', 'w3-container',
			w3_inline('w3-halign-space-around w3-tspace-8/w3-center',
            w3_button('w3-aqua', 'Reset', 'config_pb_reset'),
            w3_select('w3-label-inline', 'Mode', '', 'admin_sdr.pbm', admin_sdr.pbm, kiwi.modes_u, 'config_pb_mode')
			),
			w3_input('', 'Passband low', 'admin_sdr.pbl', admin_sdr.pbl, 'config_pb_val'),
			w3_input('', 'Passband high', 'admin_sdr.pbh', admin_sdr.pbh, 'config_pb_val')
		) +
      w3_third('', 'w3-container',
         '',
         w3_div('id-pbl-error w3-margin-T-8 w3-yellow w3-hide', 'Value creates an invalid passband'),
         w3_div('id-pbh-error w3-margin-T-8 w3-yellow w3-hide', 'Value creates an invalid passband')
      ) +
		w3_third('w3-margin-T-16 w3-text-teal', 'w3-container',
			w3_divs('/w3-center w3-tspace-8',
				w3_div('w3-text-black',
				   'As each field is changed the others are <br>' +
				   'automatically adjusted. Define CW offset by <br>' +
				   'setting appropriate passband center frequency <br>' +
				   'for CW/CWN modes (typ 500, 800 or 1000 Hz).'
				)
			),
			w3_half('w3-valign', '',
			   w3_input('', 'Passband center', 'admin_sdr.pbc', admin_sdr.pbc, 'config_pb_val'),
            w3_checkbox('w3-halign-center//w3-label-inline', 'Lock', 'admin_sdr.pbc_lock', admin_sdr.pbc_lock, 'config_pbc_lock')
			),
			w3_input('', 'Passband width', 'admin_sdr.pbw', admin_sdr.pbw, 'config_pb_val')
		) +
      w3_third('', 'w3-container',
         '',
         w3_div('id-pbc-error w3-margin-T-8 w3-yellow w3-hide', 'Value creates an invalid passband'),
         w3_div('id-pbw-error w3-margin-T-8 w3-yellow w3-hide', 'Value creates an invalid passband')
      ) +
      
      w3_div('w3-margin-bottom');

   var s3 =
		'<hr>' +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_div('',
				w3_input_get('w3-restart', 'Frequency scale offset (kHz, 1 Hz resolution)', 'freq_offset', 'admin_float_cb|3'),
				w3_div('w3-text-black',
					'Adds offset to frequency scale. <br> Useful when using a downconverter, e.g. set to <br>' +
					'116000 kHz when 144-148 maps to 28-32 MHz.'
				)
			),
			w3_divs('w3-restart/w3-center w3-tspace-8',
				w3_select('w3-width-auto', 'Max receiver frequency', '', 'max_freq', max_freq, max_freq_i, 'admin_select_cb'),
				w3_div('w3-text-black',
				   '32 MHz necessary for some downconverters. But note <br>' +
				   'there will be more spurs in the 30-32 MHz range.'
				)
			),
			w3_div('',
            w3_checkbox_get_param('//w3-restart w3-label-inline', 'Show AGC threshold on S-meter', 'agc_thresh_smeter', 'admin_bool_cb', true),
            w3_checkbox_get_param('w3-margin-T-8// w3-label-inline', 'Show user geolocation info', 'show_geo', 'admin_bool_cb', true)
         )
		) +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_input_get('', 'S-meter calibration (dB)', 'S_meter_cal', 'admin_int_cb'),
			w3_divs('/w3-center',
            w3_slider('', 'S-meter OV', 'cfg.S_meter_OV_counts', cfg.S_meter_OV_counts, 0, 15, 1, 'config_OV_counts_cb'),
            w3_text('w3-text-black',
               'Increase if S-meter OV is flashing excessively.'
            )
         ),
			w3_divs('/w3-center',
            w3_slider('', 'Passband overload mute', 'cfg.overload_mute', cfg.overload_mute, -33, 0, 1, 'overload_mute_cb'),
            w3_text('w3-text-black',
               'When the signal level in the passband exceeds this level <br>' +
               'the audio will be muted. The icon '+ w3_icon('|padding:0 3px;', 'fa-exclamation-triangle', 20, 'yellow|#575757') +' will replace the <br>' +
               'mute icon in the control panel. Useful for muting when <br>' +
               'a strong nearby transmitter is active.'
            )
         )
		) +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_input_get('', 'Waterfall calibration (dB)', 'waterfall_cal', 'admin_int_cb'),
			w3_div('w3-center w3-tspace-8',
				w3_select('w3-width-auto', 'ITU region', '', 'init.ITU_region', init_ITU_region, ITU_region_i, 'admin_select_cb'),
				w3_div('w3-text-black',
					'Configures LW/NDB, MW and <br> amateur band allocations, etc.'
				)
			),
			w3_div('w3-center w3-tspace-8',
			   w3_input_get('', 'Name/callsign input field max length (16-64)', 'ident_len', 'config_ident_len_cb'),
				w3_div('w3-text-black',
					'Used to limit the number of characters a user can enter into the name/callsign field at the top-right of the page.'
				)
			)
		);

	var s4 =
		'<hr>' +
		w3_third('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_divs('w3-restart/w3-center w3-tspace-8',
				w3_select_get_param('w3-width-auto', 'SPI clock', '', 'SPI_clock', SPI_clock_i, 'admin_select_cb', 0),
				w3_div('w3-text-black',
					'Set to 24 MHz to reduce interference <br> on 2 meters (144-148 MHz).'
				)
			),
			w3_divs('w3-restart/w3-center w3-tspace-8',
				w3_select_get_param('w3-width-auto', 'Status LED brightness', '', 'led_brightness', led_brightness_i, 'admin_select_cb', 0),
				w3_div('w3-text-black',
					'Sets brightness of the 4 LEDs <br> that show status info.'
				)
			),
			''
		);

   var s5 =
		'<hr>' +
      w3_div('w3-valign w3-container w3-section',
         '<header class="w3-container w3-yellow"><h6>' +
         'Clone configuration from another Kiwi. <b>Use with care.</b> Current configuration is <b><i>not</i></b> saved. ' +
         'This Kiwi immediately restarts after cloning.' +
         '</h6></header>'
      ) +
		w3_inline_percent('w3-text-teal/w3-container',
			w3_input('', 'Clone config from Kiwi host', 'clone_host', '', 'w3_string_cb', 'enter hostname (no port number)'), 25,
			w3_input('', 'Kiwi host root password', 'clone_pwd', '', 'w3_string_cb', 'required'), 25,
         w3_select('w3-center//', 'Config to clone', '', 'clone_files', 0, clone_files_s, 'w3_num_cb'), 15,
         w3_button('w3-center//w3-red', 'Clone', 'config_clone_cb'), 10,
         w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', 'Status:') +
         w3_div('id-config-clone-status w3-show-inline-block w3-text-black w3-background-pale-aqua', ''), 25
		) +
		w3_inline_percent('w3-margin-bottom w3-text-teal/w3-container',
		   '', 25,
         w3_div('w3-center w3-text-black',
            'Either the root password you\'ve explicitly set or the Kiwi device serial number.'
         ), 25
		);

   // FIXME: this should really be in a tab defined by admin.js
   // but don't move it without leaving an explanation since old forum posts may refer to it as being here
   var s6 =
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
		);
		
   var s7 =
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
      ) +
      '<hr>';
   
   var mode_20kHz = (adm.firmware_sel == kiwi.RX3_WF3)? 1 : 0;
   console.log('mode_20kHz='+ mode_20kHz);
   var DC_offset_I = 'DC_offset'+ (mode_20kHz? '_20kHz':'') +'_I';
   var DC_offset_Q = 'DC_offset'+ (mode_20kHz? '_20kHz':'') +'_Q';

   if (dbgUs) s7 = s7 +
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

	return w3_div('id-config w3-hide', s1 + s2 + s3 + s4 + s5 + s6 + s7);
}

function config_focus()
{
   console.log('config_focus');
   config_pb_mode('', admin_sdr.pmi, false);
   config_wf_show();
}

function config_wf_show()
{
   w3_hide2('id-wf-show-min', cfg.init.aperture != 0);
   w3_hide2('id-wf-show-max', cfg.init.aperture != 0);
   w3_hide2('id-wf-show-floor', cfg.init.aperture != 1);
   w3_hide2('id-wf-show-ceil', cfg.init.aperture != 1);
}

function config_aperture_cb(path, idx, first)
{
   if (first) return;
   i = +idx;
   admin_select_cb(path, idx);
   config_wf_show();
}

function config_pb_reset(id, idx)
{
   var mode = admin_sdr.pbm;
   console.log('config_pb_reset mode='+ mode);
	admin_sdr.pbc = admin_sdr.pb[mode].c;
	admin_sdr.pbw = admin_sdr.pb[mode].w;
	var hbw = admin_sdr.pbw / 2;
	admin_sdr.pbh = admin_sdr.pbc + hbw;
	admin_sdr.pbl = admin_sdr.pbc - hbw;
	admin_sdr.pbc_lock = false;
	config_pb_val('pbc', admin_sdr.pbc, false);
}

function config_pb_mode(path, idx, first)
{
   if (first) return;      // cfg.passbands not available yet
   idx = +idx;
   admin_sdr.pmi = idx;
   var mode = kiwi.modes_l[idx];
   admin_sdr.pbm = mode;
   console.log('config_pb_mode idx='+ idx +' mode='+ mode);

	admin_sdr.pbl = cfg.passbands[mode].lo;
	admin_sdr.pbh = cfg.passbands[mode].hi;
	var pbc_default = (admin_sdr.pbl + admin_sdr.pbh) / 2;
	admin_sdr.pbc = ext_get_cfg_param('cfg.passbands.'+ mode +'.c', pbc_default, EXT_SAVE);
	admin_sdr.pbw = admin_sdr.pbh - admin_sdr.pbl;
	admin_sdr.pbc_lock = ext_get_cfg_param('cfg.passbands.'+ mode +'.lock', false, EXT_SAVE);
	console.log('config_pb_mode: admin_sdr.pbc_lock='+ admin_sdr.pbc_lock);
	
	// Handle case of switch from higher b/w mode (e.g. 3-ch 20.25 kHz mode) to lower b/w
	// mode where values might define an invalid pb. Show invalid values and let call to
	// config_pb_val() below display error. Clamping in openwebrx will compensate.
   w3_set_value('admin_sdr.pbl', admin_sdr.pbl);
   w3_set_value('admin_sdr.pbh', admin_sdr.pbh);
   w3_set_value('admin_sdr.pbc', admin_sdr.pbc);
   w3_set_value('admin_sdr.pbw', admin_sdr.pbw);
   w3_set_value('admin_sdr.pbc_lock', admin_sdr.pbc_lock);

	config_pb_val('pbl', admin_sdr.pbl, false);
}
	
function config_pb_val(path, val, first, cb)
{
   if (first) return;      // cfg.passbands not available yet
   var which = path.slice(-3);
   //console.log('config_pb_val path='+ path);
   val = +val;
   var srate = ext_nom_sample_rate();
   //console.log('SR='+ srate);
	var half_srate = srate? srate/2 : 6000;
   var min = -half_srate, max = half_srate;
   var pbl, pbh, pbc, pbw, hbw;
   var locked = admin_sdr.pbc_lock;
   var ok;
   
   // reset error indicators
   w3_show_hide('id-pbl-error', false);
   w3_show_hide('id-pbh-error', false);
   w3_show_hide('id-pbc-error', false);
   w3_show_hide('id-pbw-error', false);

   switch (which) {
   
   case 'pbl':
      pbl = val;
      if (locked) {     // if pbc locked, leave pdh unchanged to allow asymmetrical filter definition
         pbc = admin_sdr.pbc;
         pbw = admin_sdr.pbh - pbl;
         ok = (pbl <= (admin_sdr.pbc-1) && pbl >= min);
      } else {          // if not locked adjust pbc/pbw to create a symmetrical pb
         pbc = (pbl + admin_sdr.pbh) / 2;
         pbw = admin_sdr.pbh - pbl;
         ok = (pbl <= (admin_sdr.pbh-2) && pbl >= min);
      }

      if (ok) {
         admin_sdr.pbl = pbl;
         admin_sdr.pbc = pbc;
         admin_sdr.pbw = pbw;
      }
      console.log('pbl='+ pbl +' ok='+ ok);
      w3_show_hide('id-pbl-error', !ok);
      break;
   
   case 'pbh':
      pbh = val;
      if (locked) {     // if pbc locked, leave pdl unchanged to allow asymmetrical filter definition
         pbc = admin_sdr.pbc;
         pbw = pbh - admin_sdr.pbl;
         ok = (pbh >= (admin_sdr.pbc+1) && pbh <= max);
      } else {          // if not locked adjust pbc/pbw to create a symmetrical pb
         pbc = (admin_sdr.pbl + pbh) / 2;
         pbw = pbh - admin_sdr.pbl;
         ok = (pbh >= (admin_sdr.pbl+2) && pbh <= max);
      }

      if (ok) {
         admin_sdr.pbh = pbh;
         admin_sdr.pbc = pbc;
         admin_sdr.pbw = pbw;
      }
      console.log('pbh='+ pbh +' ok='+ ok);
      w3_show_hide('id-pbh-error', !ok);
      break;
   
   case 'pbc':
      pbc = val;
      if (locked) {  // if locked maintain possible pbl/pbh asymmetry
         var delta_l = admin_sdr.pbc - admin_sdr.pbl;
         var delta_h = admin_sdr.pbh - admin_sdr.pbc;
         console.log('delta_l='+ delta_l +' delta_h='+ delta_h);
         pbl = pbc - delta_l;
         pbh = pbc + delta_h;
         console.log(admin_sdr.pbl +'|'+ admin_sdr.pbc +'|'+ admin_sdr.pbh +' => '+ pbl +'|'+ pbc +'|'+ pbh);
         ok = (pbc > pbl && pbc < pbh);
      } else {    // adjust pbl/pbh to maintain pbc symmetry (same pbw)
         hbw = admin_sdr.pbw / 2;
         pbl = pbc - hbw;
         pbh = pbc + hbw;
         ok = (pbl >= min && pbh <= max);
      }

      if (ok) {
         admin_sdr.pbl = pbl;
         admin_sdr.pbh = pbh;
         admin_sdr.pbc = pbc;
      }
      console.log('pbc='+ pbc +' ok='+ ok);
      w3_show_hide('id-pbc-error', !ok);
      break;
   
   case 'pbw':
      pbw = val;
      if (locked) {     // if locked apply change in pbw to possible pbl/pbh asymmetry
         var delta_w = pbw - admin_sdr.pbw;
         var ratio_pbh = (admin_sdr.pbh - admin_sdr.pbc) / admin_sdr.pbw;
         var delta_l = Math.round(delta_w * (1 - ratio_pbh));
         var delta_h = Math.round(delta_w * ratio_pbh);
         console.log('delta_w='+ delta_w +' ratio_pbh='+ ratio_pbh.toFixed(4) +' delta_l='+ delta_l +' delta_h='+ delta_h);
         pbl = admin_sdr.pbl - delta_l;
         pbh = admin_sdr.pbh + delta_h;
         console.log(admin_sdr.pbl +'|'+ admin_sdr.pbc +'|'+ admin_sdr.pbh +' => '+ pbl +'|'+ admin_sdr.pbc +'|'+ pbh);
         ok = (pbw >= 2 && pbl < pbh && pbl < admin_sdr.pbc && pbh > admin_sdr.pbc && pbl >= min && pbh <= max);
      } else {    // adjust pbl/pbh to maintain pbc symmetry (same pbc)
         hbw = pbw / 2;
         pbl = admin_sdr.pbc - hbw;
         pbh = admin_sdr.pbc + hbw;
         ok = (pbw >= 2 && pbl >= min && pbh <= max);
      }

      if (ok) {
         admin_sdr.pbl = pbl;
         admin_sdr.pbh = pbh;
         admin_sdr.pbw = pbw;
      }
      console.log('pbw='+ pbw +' ok='+ ok);
      w3_show_hide('id-pbw-error', !ok);
      break;
   }
   
   // leave invalid values in fields, but will revert when mode is changed or tab switched etc.
   if (!ok) return;
   if (locked) console.log('LOCKED');
   
   w3_set_value('admin_sdr.pbl', admin_sdr.pbl);
   w3_set_value('admin_sdr.pbh', admin_sdr.pbh);
   w3_set_value('admin_sdr.pbc', admin_sdr.pbc);
   w3_set_value('admin_sdr.pbw', admin_sdr.pbw);
   w3_checkbox_set('admin_sdr.pbc_lock', admin_sdr.pbc_lock);
   
   ext_set_cfg_param('cfg.passbands.'+ admin_sdr.pbm +'.lo', admin_sdr.pbl, EXT_NO_SAVE);
   ext_set_cfg_param('cfg.passbands.'+ admin_sdr.pbm +'.hi', admin_sdr.pbh, EXT_NO_SAVE);
   ext_set_cfg_param('cfg.passbands.'+ admin_sdr.pbm +'.c', admin_sdr.pbc, EXT_NO_SAVE);
   ext_set_cfg_param('cfg.passbands.'+ admin_sdr.pbm +'.lock', admin_sdr.pbc_lock, EXT_SAVE);
}

function config_pbc_lock(path, val, first)
{
   if (first) return;
   admin_sdr.pbc_lock = val? true:false;
   console.log('config_pbc_lock: cfg.passbands.'+ admin_sdr.pbm +'.lock = '+ admin_sdr.pbc_lock +' EXT_SAVE');
   ext_set_cfg_param('cfg.passbands.'+ admin_sdr.pbm +'.lock', admin_sdr.pbc_lock, EXT_SAVE);
}
	
function config_wfmin_cb(path, val, first)
{
   val = +val;
   var ok = (val < cfg.init.max_dB);
   if (ok) admin_int_cb(path, val, first);
   w3_show_hide('id-wfmin-error', !ok);
}

function config_wfmax_cb(path, val, first)
{
   val = +val;
   var ok = (val > cfg.init.min_dB);
   if (ok) admin_int_cb(path, val, first);
   w3_show_hide('id-wfmax-error', !ok);
}

function config_zoom_cb(path, val, first)
{
   val = +val;
   var ok = (val >= 0 && val <= 14);
   if (ok) admin_int_cb(path, val, first);
   w3_show_hide('id-zoom-error', !ok);
}

function config_ident_len_cb(path, val, first)
{
   val = +val;
   if (val < 16) val = 16;
   if (val > 64) val = 64;
   admin_int_cb(path, val, first);
}

function config_OV_counts_cb(path, val, complete, first)
{
   //console.log('config_OV_counts_cb path='+ path +' val='+ val);
   val = +val;
	var ov_counts = 1 << val;
	admin_int_cb(path, val);
	w3_set_label('S-meter OV if &ge; '+ ov_counts +' ADC OV per 64k samples', path);
	ext_send('SET ov_counts='+ ov_counts);
}

function overload_mute_cb(path, val, complete, first)
{
   //console.log('overload_mute_cb path='+ path +' val='+ val);
   val = +val;
	admin_int_cb(path, val);
	var s = 'Passband overload mute '+ val +' dBm';
	if (val >= -73) s += ' (S9+'+ (val - -73) +')';
	w3_set_label(s, path);
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
			w3_inline('w3-halign-space-between/',
            w3_inline('',
               w3_input('w3-flex-col//w3-no-styling||type="file" accept="image/*"',
                  'Photo file', 'id-photo-file', '', 'webpage_photo_file_upload'
               ),
               w3_div('id-photo-error', '')
            ),
            w3_checkbox_get_param('w3-restart w3-label-inline', 'Photo left margin', 'index_html_params.RX_PHOTO_LEFT_MARGIN', 'admin_bool_cb', true)
         ),
			w3_input('', 'Photo maximum height (pixels)', 'index_html_params.RX_PHOTO_HEIGHT', '', 'webpage_photo_height_cb')
		) +
		w3_half('', 'w3-container',
			w3_input('', 'Photo title', 'index_html_params.RX_PHOTO_TITLE', '', 'webpage_string_cb'),
			w3_input('', 'Photo description', 'index_html_params.RX_PHOTO_DESC', '', 'webpage_string_cb')
		);
		
	var s3 =
		'<hr>' +
		w3_half('w3-margin-bottom w3-text-teal', 'w3-container',
			w3_divs('/w3-center w3-tspace-8',
            w3_div('', '<b>Web server caching?</b>'),
            w3_switch('', 'Yes', 'No', 'webserver_caching', cfg.webserver_caching, 'admin_radio_YN_cb'),
            w3_text('w3-text-black w3-center',
               'Set "No" when there are caching problems in your <br>' +
               'network path, e.g. user interface icons don\'t load.'
            )
         )
		) +
		
		'<hr>' +
      w3_div('w3-container',
         w3_textarea_get_param('w3-input-any-change|width:100%',
            w3_div('',
               w3_text('w3-bold w3-text-teal', 'Additional HTML/Javascript for HTML &lt;head&gt; element (e.g. Google analytics or user customization)'),
               w3_text('w3-text-black', 'Press enter(return) key while positioned at end of text to submit data.')
            ),
            'index_html_params.HTML_HEAD', 10, 100, 'webpage_string_cb', ''
         )
		) +
		'<hr>';

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
	webpage_string_cb(path, val.trim());
	webpage_update_check_map();
}

function webpage_update_check_map()
{
	var map = kiwi_decodeURIComponent('RX_GMAP', ext_get_cfg_param('index_html_params.RX_GMAP'));
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
	
	var el = w3_el('id-photo-error');
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
	console.log('webpage_photo_file_upload2');
	console.log(file);
	console.log(fdata);

	var el = w3_el('id-photo-error');
	w3_hide(el);
	w3_remove(el, 'w3-text-red');
	w3_remove(el, 'w3-text-green');

	//kiwi_ajax_send(fdata, '/PIX?'+ key, 'webpage_photo_uploaded');
}

function webpage_title_cb(path, val)
{
	webpage_string_cb(path, val);
	w3_el('id-webpage-title-preview').innerHTML = admin_preview_status_box('RX_TITLE_2', cfg.index_html_params.RX_TITLE);
}

function webpage_owner_info_cb(path, val)
{
	webpage_string_cb(path, val);
	w3_el('id-webpage-owner-info-preview').innerHTML = admin_preview_status_box('owner_info_2', cfg.owner_info);
}

function webpage_status_cb(path, val)
{
	w3_string_set_cfg_cb(path, val);
	w3_el('id-webpage-status-preview').innerHTML = admin_preview_status_box('webpage_status_2', cfg.status_msg);
}

// because of the inline quoting issue, set value dynamically
function webpage_focus()
{
	admin_set_decoded_value('index_html_params.RX_TITLE');
	w3_el('id-webpage-title-preview').innerHTML = admin_preview_status_box('RX_TITLE_1', cfg.index_html_params.RX_TITLE);

	admin_set_decoded_value('status_msg');
	w3_el('id-webpage-status-preview').innerHTML = admin_preview_status_box('webpage_status_1', cfg.status_msg);

	admin_set_decoded_value('index_html_params.PAGE_TITLE');
	admin_set_decoded_value('index_html_params.RX_LOC');
	admin_set_decoded_value('index_html_params.RX_QRA');
	admin_set_decoded_value('index_html_params.RX_ASL');
	admin_set_decoded_value('index_html_params.RX_GMAP');
	admin_set_decoded_value('index_html_params.RX_PHOTO_HEIGHT');
	admin_set_decoded_value('index_html_params.RX_PHOTO_TITLE');
	admin_set_decoded_value('index_html_params.RX_PHOTO_DESC');

	admin_set_decoded_value('owner_info');
	w3_el('id-webpage-owner-info-preview').innerHTML = admin_preview_status_box('owner_info_1', cfg.owner_info);

	webpage_update_check_grid();
	webpage_update_check_map();
}

function webpage_string_cb(path, val)
{
	w3_string_set_cfg_cb(path, val);
	ext_send('SET reload_index_params');
}

function webpage_photo_height_cb(path, val)
{
	val = parseInt(val);
	if (isNaN(val)) {
	   // put old value back
	   val = ext_get_cfg_param(path);
		w3_set_value(path, val);
	} else {
	   val = w3_clamp(val, 0, 4000, 0);
		w3_set_value(path, val);
	   w3_num_set_cfg_cb(path, val);
	   ext_send('SET reload_index_params');
	}
}


////////////////////////////////
// public
////////////////////////////////

function kiwi_reg_html()
{
	var s1 =
		w3_div('',
         w3_div('w3-margin-T-10 w3-valign',
            '<header class="w3-container w3-yellow"><h5>' +
            'More information on <a href="http://kiwisdr.com/quickstart/index.html#id-config-kiwi-reg" target="_blank">kiwisdr.com</a><br><br>' +

            'To list your Kiwi on <a href="http://rx.kiwisdr.com" target="_blank">rx.kiwisdr.com</a> ' +
            'edit the fields below and set the "<i>Register</i>" switch to <b>Yes</b>. ' +
            'Look for a successful status result within a few minutes.<br>' +
            
            'The "<i>Location (lat, lon)</i>" field must be set properly for your Kiwi to be listed in the correct location on ' +
            '<a href="http://map.kiwisdr.com" target="_blank">map.kiwisdr.com</a>' +

            '</h5></header>'
         )
      ) +

		'<hr>' +

		w3_divs('w3-margin-bottom w3-container w3-center',
			w3_div('',
					'<b>Register on <a href="http://rx.kiwisdr.com" target="_blank">rx.kiwisdr.com</a>?</b> ' +
					w3_switch('', 'Yes', 'No', 'adm.kiwisdr_com_register', adm.kiwisdr_com_register, 'kiwisdr_com_register_cb')
			),
         w3_div('id-kiwisdr_com-reg-status-container',
            w3_div('w3-container',
               w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', 'kiwisdr.com registration status:') +
               w3_div('id-kiwisdr_com-reg-status w3-show-inline-block w3-text-black', '')
            )
         )
		);
		
      /*
		w3_half('w3-margin-bottom', 'w3-container',
			w3_div('',
					'<b>Register on <a href="http://rx.kiwisdr.com" target="_blank">rx.kiwisdr.com</a>?</b> ' +
					w3_switch('', 'Yes', 'No', 'adm.kiwisdr_com_register', adm.kiwisdr_com_register, 'kiwisdr_com_register_cb')
			),
			w3_div('',
					'<b>Register on <a href="https://sdr.hu/?top=kiwi" target="_blank">sdr.hu</a>?</b> ' +
					w3_switch('', 'Yes', 'No', 'adm.sdr_hu_register', adm.sdr_hu_register, 'sdr_hu_register_cb')
			)
		) +

		w3_half('w3-margin-bottom', 'w3-container',
		   //w3_div('w3-restart',
		   //   w3_input('', 'kiwisdr.com API key', 'adm.api_key_kiwisdr_com', '', 'w3_string_set_cfg_cb')
		   //),
		   w3_div(),
		   w3_div('w3-restart',
		      w3_input('', 'sdr.hu API key', 'adm.api_key', '', 'w3_string_set_cfg_cb', 'enter value returned from sdr.hu/register process')
		   )
		) +

		w3_half('', '',
         w3_div('id-kiwisdr_com-reg-status-container',
            w3_div('w3-container',
               w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', 'kiwisdr.com registration status:') +
               w3_div('id-kiwisdr_com-reg-status w3-show-inline-block w3-text-black', '')
            )
         ),
         w3_div('id-sdr_hu-reg-status-container',
            w3_div('w3-container',
               w3_label('w3-show-inline-block w3-margin-R-16 w3-text-teal', 'sdr.hu registration status:') +
               w3_div('id-sdr_hu-reg-status w3-show-inline-block w3-text-black', '')
            )
         )
      );
		*/
      
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
				w3_div('id-public-grid-check cl-admin-check w3-show-inline-block w3-green w3-btn w3-round-large') + ' ' +
				w3_div('id-public-grid-set cl-admin-check w3-blue w3-btn w3-round-large w3-hide', 'set from GPS'),
				'rx_grid', '', 'sdr_hu_input_grid'
			),
			w3_div('',
            w3_input('', w3_label('w3-bold', 'Location (lat, lon) ') +
               w3_div('id-public-gps-check cl-admin-check w3-show-inline-block w3-green w3-btn w3-round-large') + ' ' +
               w3_div('id-public-gps-set cl-admin-check w3-blue w3-btn w3-round-large w3-hide', 'set from GPS'),
               'rx_gps', '', 'public_check_gps_cb'
            ),
				w3_div('w3-text-black', 'Format: (nn.nnnnnn, nn.nnnnnn)')
			),
			w3_input_get('', 'Altitude (ASL meters)', 'rx_asl', 'admin_int_cb')
		) +

		'<hr>' +
		w3_half('w3-margin-bottom', 'w3-container',
         '<b>Display owner/admin email link on KiwiSDR main page?</b> ' +
         w3_switch('', 'Yes', 'No', 'contact_admin', cfg.contact_admin, 'admin_radio_YN_cb'),
		   ''
		) +

		'<hr>' +
		w3_half('w3-margin-bottom', 'w3-container',
		   w3_div('',
            w3_input_get('', 'Coverage frequency low (kHz)', 'sdr_hu_lo_kHz', 'admin_int_cb'),
				w3_div('w3-text-black',
				   'These two settings effect the frequency coverage label displayed on rx.kiwisdr.com <br>' +
				   'e.g. when set to 0 and 30000 "HF" is shown. If you\'re using a transverter <br>' +
				   'then appropriate entries will cause "2m" or "70cm" to be shown. Other labels will be <br>' +
				   'shown if you limit the range at HF due to antenna or filtering limitations.'
				)
			),
         w3_input_get('', 'Coverage frequency high (kHz)', 'sdr_hu_hi_kHz', 'admin_int_cb')
      ) +
      '<hr>';

	return w3_div('id-sdr_hu w3-text-teal w3-hide', s1 + s2);
}

function kiwisdr_com_register_cb(path, idx)
{
   idx = +idx;
   //console.log('kiwisdr_com_register_cb idx='+ idx);
   
   var text, color;
   var no_url = (cfg.server_url == '');
   var no_passwordless_channels = (adm.user_password != '' && cfg.chan_no_pwd == 0);
   var no_rx_gps = (cfg.rx_gps == '' || cfg.rx_gps == '(0.000000, 0.000000)' || cfg.rx_gps == '(0.000000%2C%200.000000)');
   //console.log('kiwisdr_com_register_cb has_u_pwd='+ (adm.user_password != '') +' chan_no_pwd='+ cfg.chan_no_pwd +' no_passwordless_channels='+ no_passwordless_channels);

   if (idx == w3_SWITCH_YES_IDX && (no_url || no_passwordless_channels || no_rx_gps)) {
      if (no_url)
         text = 'Error, you must first setup a valid Kiwi connection URL on the admin "connect" tab';
      else
      if (no_passwordless_channels)
         text = 'Error, must have at least one user channel that doesn\'t require a password (see admin "security" tab)';
      else
      if (no_rx_gps)
         text = 'Error, you must first set a valid entry in the "<i>Location (lat, lon)</i>" field';
      color = '#ffeb3b';
      w3_switch_set_value(path, w3_SWITCH_NO_IDX);    // force back to 'no'
      idx = w3_SWITCH_NO_IDX;
   } else
   if (idx == w3_SWITCH_YES_IDX) {
      text = '(waiting for kiwisdr.com response, can take several minutes in some cases)';
      color = 'hsl(180, 100%, 95%)';
   } else {    // w3_SWITCH_NO_IDX
      text = '(registration not enabled)';
      color = 'hsl(180, 100%, 95%)';
      w3_switch_set_value(path, w3_SWITCH_NO_IDX);    // for benefit of direct callers
   }
   
   w3_innerHTML('id-kiwisdr_com-reg-status', text);
   w3_color('id-kiwisdr_com-reg-status', null, color);
   admin_radio_YN_cb(path, idx);
   //console.log('kiwisdr_com_register_cb adm.kiwisdr_com_register='+ adm.kiwisdr_com_register);
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
   
   /*
   w3_innerHTML('id-sdr_hu-reg-status', text);
   w3_color('id-sdr_hu-reg-status', null, color);
   */
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
	//admin_set_decoded_value('adm.api_key');

	// The default in the factory-distributed kiwi.json is the kiwisdr.com NZ location.
	// Detect this and ask user to change it so sdr.hu/map doesn't end up with multiple SDRs
	// defined at the kiwisdr.com location.
	var gps = kiwi_decodeURIComponent('rx_gps', ext_get_cfg_param('rx_gps'));
	public_check_gps_cb('rx_gps', gps, /* first */ true);
	
	public_update_check_grid();
	public_update_check_map();
	
	w3_el('id-public-grid-set').onclick = function() {
		var val = admin.reg_status.grid;
		w3_set_value('rx_grid', val);
		w3_input_change('rx_grid', 'sdr_hu_input_grid');
	};

	w3_el('id-public-gps-set').onclick = function() {
		var val = '('+ admin.reg_status.lat +', '+ admin.reg_status.lon +')';
		w3_set_value('rx_gps', val);
		w3_input_change('rx_gps', 'public_check_gps_cb');
	};

	// only get updates while the sdr_hu tab is selected
	ext_send("SET public_update");
	sdr_hu_interval = setInterval(function() {ext_send("SET public_update");}, 5000);
	
	// display initial switch state
	kiwisdr_com_register_cb('adm.kiwisdr_com_register', adm.kiwisdr_com_register? w3_SWITCH_YES_IDX : w3_SWITCH_NO_IDX);
	sdr_hu_register_cb('adm.sdr_hu_register', adm.sdr_hu_register? w3_SWITCH_YES_IDX : w3_SWITCH_NO_IDX);
}

function sdr_hu_input_grid(path, val)
{
	w3_string_set_cfg_cb(path, val);
	public_update_check_grid();
}

function public_update_check_grid()
{
	var grid = ext_get_cfg_param('rx_grid');
	w3_el('id-public-grid-check').innerHTML = '<a href="http://www.levinecentral.com/ham/grid_square.php?Grid='+ grid +'" target="_blank">check grid</a>';
}

function public_check_gps_cb(path, val, first)
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

	if (val == '(-37.631120, 176.172210)' || val == '(-37.631120%2C%20176.172210)')
	   val = '(0.000000, 0.000000)';

	if (val == '(0.000000, 0.000000)') {
		w3_flag('rx_gps');

      // clear registration state
      kiwisdr_com_register_cb('adm.kiwisdr_com_register', w3_SWITCH_NO_IDX);
	} else {
		w3_unflag('rx_gps');
	}
	
	w3_string_set_cfg_cb(path, val, first);
	w3_set_value(path, val);
	public_update_check_map();
}

function public_update_check_map()
{
	var gps = kiwi_decodeURIComponent('rx_gps', ext_get_cfg_param('rx_gps'));
	gps = gps.substring(1, gps.length-1);		// remove parens
	w3_el('id-public-gps-check').innerHTML = '<a href="https://google.com/maps/place/'+ gps +'" target="_blank">check map</a>';
}

function sdr_hu_blur(id)
{
	kiwi_clearInterval(sdr_hu_interval);
}

function public_update(p)
{
	var i;
	var json = decodeURIComponent(p);
	//console.log('public_update='+ json);
   var obj = kiwi_JSON_parse('public_update', json);
	if (obj) admin.reg_status = obj;
	
	// rx.kiwisdr.com registration status
	if (adm.kiwisdr_com_register && admin.reg_status.kiwisdr_com != undefined && admin.reg_status.kiwisdr_com != '') {
	   w3_innerHTML('id-kiwisdr_com-reg-status', 'rx.kiwisdr.com registration: successful');
	}
	
	// sdr.hu registration status
	/*
	if (adm.sdr_hu_register && admin.reg_status.sdr_hu != undefined && admin.reg_status.sdr_hu != '') {
	   w3_innerHTML('id-sdr_hu-reg-status', admin.reg_status.sdr_hu);
	}
	*/
	
	// GPS has had a solution, show buttons
	if (admin.reg_status.lat != undefined) {
		w3_show_inline_block('id-public-grid-set');
		w3_show_inline_block('id-public-gps-set');
	}
}


////////////////////////////////
// dx:
////////////////////////////////

function dx_html()
{
   var i, s;
   dx.enabled = true;

	if (!dx.enabled)
	   return w3_div('id-dx w3-hide', w3_div('w3-container w3-margin-top', 'TODO..'));
	
	if (kiwi_isMobile())
	   return w3_div('id-dx w3-hide', w3_div('w3-container w3-margin-top', 'Not available on mobile devices.'));
	
	// one-time conversion of kiwi.config/config.js bands[] to kiwi.json configuration file
	if (isUndefined(cfg.bands) || isUndefined(cfg.band_svc) || isUndefined(cfg.dx_type)) {
      bandwidth = [30, 32][cfg.max_freq] * 1e6;
      zoom_nom = ZOOM_NOMINAL;
      bands_init();
	   console.log('BANDS: saving new cfg.bands');
      cfg_save_json('config.js', 'cfg.bands');
   } else {
	   console.log('BANDS: using stored cfg.bands');
      bands_addl_info();
   }
   
   dx.LEGEND = -1;
   dx.SAVE_NOW = true;
   dx.DX_DOW_BASE = dx.DX_DOW >> dx.DX_DOW_SFT;
   dx.spacer = w3_div('w3-font-fixed w3-circle-pad-small w3-pale-blue', '&nbsp;');
   dx.button_section = 'w3-font-fixed w3-aqua w3-circle w3-circle-pad||title="show&slash;hide section"';
   dx.button_add = 'w3-css-lime||title="duplicate this entry"';
   dx.button_del = 'w3-css-red||title="delete this entry"';
   dx.link1 = 'w3-link-darker-color w3-bold||title="more information"';
   dx.link2 = 'w3-link-darker-color w3-bold|width:175px|title="more information"';
   dx.hover_over =
      w3_inline('w3-valign',
         w3_text('w3-margin-left w3-text-black', 'Hover over'),
         w3_icon('w3-link-darker-color w3-help', 'fa-info-circle', 20),
         w3_text('w3-text-black', '&nbsp;for info')
      );

   dx.export_label = 'Export: '+ w3_icon('id-dx-export-info w3-link-darker-color w3-help' +
      '||title="Export (download) DX labels from Kiwi\nto a file on this computer.\nFiles can be in JSON or CSV format."', 'fa-info-circle', 20);
   dx.import_label = 'Import: '+ w3_icon('id-dx-import-info w3-link-darker-color w3-help' +
      '||title="Import (upload) DX labels from a file\non this computer to Kiwi.\nFiles can be in JSON or CSV format."', 'fa-info-circle', 20);
   
   // for search wrap
   s =
      w3_div('id-search-wrap-container class-overlay-container w3-hide',
         w3_div('id-search-wrap', w3_icon('', 'fa-repeat', 192))
      );
   w3_create_appendElement('id-kiwi-container', 'div', s);

   // reminder: "Nvh" means N% of the viewport (browser window) height
   var vh = '63vh';
   //var vh = '32vh';
	s =
      w3_div('id-dx w3-hide',


         // dx labels
         w3_inline('w3-margin-T-16 w3-halign-space-between/',
            w3_inline('/w3-margin-between-16 w3-valign',
               w3_button(dx.button_section, '-', 'dx_expand_cb', 0),
               w3_link(dx.link1, 'http://kiwisdr.com/quickstart/index.html#id-config-DX-list', 'DX labels'),
               w3_text('id-dx-list-saved w3-margin-left w3-padding-medium w3-text-black w3-hide', 'Changes saved')
            ),
            w3_inline('/w3-margin-between-16 w3-valign',
               w3_text('id-dx-export-label w3-padding-medium w3-padding-R-0 w3-text-teal w3-bold', dx.export_label),
               w3_inline('w3-margin-between-8',
                  w3_button('w3-blue w3-font-12px w3-padding-tiny||title="export to JSON file"', 'JSON', 'dx_export_cb', dx.DX_JSON),
                  w3_button('w3-blue w3-font-12px w3-padding-tiny||title="export to CSV file"', 'CSV', 'dx_export_cb', dx.DX_CSV)
               ),
               w3_text('id-dx-import-label w3-padding-medium w3-padding-R-0 w3-text-teal w3-bold', dx.import_label),
               w3_inline('w3-margin-between-8',
                  w3_button('w3-red w3-font-12px w3-padding-tiny||title="import from JSON file"', 'JSON', 'dx_import_cb', dx.DX_JSON),
                  w3_button('w3-red w3-font-12px w3-padding-tiny||title="import from CSV file"', 'CSV', 'dx_import_cb', dx.DX_CSV),
                  w3_input('//w3-hide w3-no-styling||type="file"', '', 'id-dx-import-form', '', 'dx_file_upload_cb')
               ),
               w3_text('w3-padding-medium w3-padding-R-0 w3-text-teal w3-bold', 'Search:'),
               w3_input('w3-text-black/w3-label-inline/w3-padding-small w3-retain-input-focus|width:75px|title="finds nearest freq"',
                  'Freq', 'dx.o.search_f', '', 'dx_search_freq_cb'),
               w3_input('w3-text-black/w3-label-inline/w3-padding-small w3-retain-input-focus w3-input-any-change|width:150px' +
                  '|title="Search ident fields.\nUse return&slash;enter key to find next. Wraps."',
                  'Ident', 'dx.o.search_i', '', 'dx_search_ident_cb'),
               w3_input('w3-text-black/w3-label-inline/w3-padding-small w3-retain-input-focus w3-input-any-change|width:150px' +
                  '|title="Search notes fields.\nUse return&slash;enter key to find next. Wraps."',
                  'Notes', 'dx.o.search_n', '', 'dx_search_notes_cb')
            )
         ),
         w3_div('id-dx-list-msg w3-margin-T-8 w3-padding-tiny w3-hide'),
         w3_div('id-dx-list-container w3-container w3-margin-top w3-margin-bottom w3-card-8 w3-round-xlarge w3-pale-blue',
            w3_div('id-dx-list-legend w3-margin-R-15'),
            w3_div('id-dx-list w3-margin-bottom w3-padding-B-8 w3-black-box w3-scroll-y|height:'+ vh +'|onscroll="dx_list_scroll();"')
         ),


         // dx type menu
         w3_inline('w3-margin-T-24 w3-halign-space-between/',
            w3_inline('/w3-margin-between-16 w3-valign',
               w3_button(dx.button_section, '+', 'dx_expand_cb', 1),
               w3_link(dx.link2, 'http://kiwisdr.com/quickstart/index.html#id-config-DX-type', 'DX type menu'),
               w3_text('w3-margin-left w3-text-black',
                  'Defines content of <b>Type</b> menu in <i>DX labels</i> section above.<br>' +
                  'Follow link at left for important info before making changes.'),
               w3_text('id-dx-type-saved w3-margin-left w3-padding-medium w3-text-black w3-hide', 'Changes saved')
            ),
            dx.hover_over
         ),
         w3_div('id-dx-type-container w3-hide w3-container w3-margin-top w3-margin-bottom w3-card-8 w3-round-xlarge w3-pale-blue',
            w3_div('id-dx-type-list-legend w3-margin-R-15'),
            w3_div('id-dx-type-list w3-margin-bottom w3-padding-B-8 w3-black-box w3-scroll-y|height:'+ vh)
         ),


         // band bars
         w3_inline('w3-margin-T-24 w3-halign-space-between/',
            w3_inline('/w3-margin-between-16 w3-valign',
               w3_button(dx.button_section, '+', 'dx_expand_cb', 2),
               w3_link(dx.link2, 'http://kiwisdr.com/quickstart/index.html#id-config-band-bars', 'Band bars'),
               w3_text('w3-margin-left w3-text-black',
                  'Defines content of band bars and <b>select band</b> menu on user page.<br>' +
                  'Follow link at left for important info before making changes.'),
               w3_text('id-band-bar-saved w3-margin-left w3-padding-medium w3-text-black w3-hide', 'Changes saved')
            ),
            dx.hover_over
         ),
         w3_div('id-band-bar-container w3-hide w3-container w3-margin-top w3-margin-bottom w3-card-8 w3-round-xlarge w3-pale-blue',
            w3_div('id-band-bar-list-legend w3-margin-R-15'),
            w3_div('id-band-bar-list w3-margin-bottom w3-padding-B-8 w3-black-box w3-scroll-y|height:'+ vh)
         ),


         // band service menu
         w3_inline('w3-margin-TB-24 w3-halign-space-between/',
            w3_inline('/w3-margin-between-16 w3-valign',
               w3_button(dx.button_section, '+', 'dx_expand_cb', 3),
               w3_link(dx.link2, 'http://kiwisdr.com/quickstart/index.html#id-config-band-svc', 'Band service menu'),
               w3_text('w3-margin-left w3-text-black',
                  'Defines content of <b>Service</b> menu in <i>Band bars</i> section above.<br>' +
                  'Follow link at left for important info before making changes.'),
               w3_text('id-band-svc-saved w3-margin-left w3-padding-medium w3-text-black w3-hide', 'Changes saved')
            ),
            dx.hover_over
         ),
         w3_div('id-band-svc-container w3-hide w3-container w3-margin-top w3-margin-bottom w3-card-8 w3-round-xlarge w3-pale-blue',
            w3_div('id-band-svc-list-legend w3-margin-R-15'),
            w3_div('id-band-svc-list w3-margin-bottom w3-padding-B-8 w3-black-box w3-scroll-y|height:'+ vh)
         )
      );

   //console.log(s);
   return s;
}

function dx_expand_cb(path, which)
{
   which = +which;
   //console.log('dx_expand_cb path='+ path +' which='+ which);
   var el = w3_el(path);
   var id = ['id-dx-list-container', 'id-dx-type-container', 'id-band-bar-container', 'id-band-svc-container'][which];
   var hide = (el.innerText == '-');
   w3_hide2(id, hide);
   el.innerText = hide? '+' : '-';
   
   if (which == 0 && !hide) dx_list_scroll();      // refresh if now visible
}

function dx_focus()
{
   if (!dx.enabled) return;
   dx.open_sched = -1;
   dx.o.last_search_idx = 0;

   w3_innerHTML('id-band-bar-list-legend', '');
   w3_innerHTML('id-band-bar-list', '');

   w3_innerHTML('id-dx-list-legend', '');
   w3_innerHTML('id-dx-list',
      w3_div('w3-show-inline-block w3-relative|top:45%;left:45%',
         w3_icon('', 'fa-refresh fa-spin', 48, 'black'),
         w3_div('id-dx-list-count w3_text_black')
      )
   );
   
	ext_send('SET GET_DX_SIZE');
	// calls dx_size() below on callback
}

// callback from "dx_size=<len>"
function dx_size(len)
{
   if (!dx.enabled) return;
   //console.log('dx_size: entries='+ len);
   w3_innerHTML('id-dx-list-count', 'loading '+ len +' entries');
   
   dx_type_render();
   band_svc_render();
   band_bar_render();
   
   // if this isn't delayed the above innerHTML set of id-dx-list-count doesn't
   // have enough time to render and be seen
   setTimeout(function() { dx_size_cb(len); }, 500);
}

function dx_blur()
{
   if (!dx.enabled) return;
   //console.log('dx_blur');
   
   // not during *_blur() calls during admin page load
   if (!admin.init) {
      dx_save('id-band-svc-saved', dx.SAVE_NOW);
      dx_save('id-band-bar-saved', dx.SAVE_NOW);
   }
}

function dx_sched_save(id, delay_ms)
{
   delay_ms = delay_ms || 1500;
   kiwi_clearTimeout(dx.cfg_save_timeout);
   dx.cfg_save_timeout = setTimeout(function() { dx_save(id); }, delay_ms);
   dx.cfg_save_sched = true;
}

function dx_save(id, now)
{
   kiwi_clearTimeout(dx.cfg_save_timeout);
   if (now == dx.SAVE_NOW || dx.cfg_save_sched == true) {
      dx.cfg_save_sched = false;
	   w3_hide2(id, false);
	   var fade = 1000;
      w3_flash_fade(id, 'cyan', 50, fade, 'white');
	   setTimeout(function() {
	      w3_hide2(id);
	   }, fade + 250);
	   
	   // only for the UI effect when called by the dx list (i.e. don't do save)
	   if (id != 'id-dx-list-saved') {
	      console.log('BANDS: saving cfg.{band_svc,bands}');
         cfg_save_json('dx_save', 'cfg.bands');
      }

      // after any admin change to dx label or band bar:
      // ask user connections to reload cfg *after* we've saved modified version
      // and get new dx list and band bars
      ext_send('SET GET_DX_LIST');
   }
}

function dx_btn(c) { return 'w3-font-fixed w3-circle w3-circle-pad-small '+ c; }


// dx: labels

// called in response to "admin_mkr=" sent from server processing "SET MARKER idx1= idx2="
function dx_render(obj)
{
   var i, j;
   
   if (dx.export_active) {
      dx_export_cb2(obj);
      return;
   }

   // prevent re-render if scrolling to bottom on add sched to last list entry
   if (dx.ignore_dx_update) {
      //console.log('dx_render: ignore_dx_update='+ dx.ignore_dx_update);
      dx.ignore_dx_update = false;
      return;
   }

   //console.log(obj);
   
   var hdr = obj[0];
   if (hdr.pe || hdr.fe) {
      var el = w3_el('id-dx-list-msg');
      w3_remove_then_add(el, 'w3-green', 'w3-red');
      el.innerHTML = 'Warning: dx.json file has '+ (hdr.pe + hdr.fe) +' labels with errors -- see Kiwi log tab for details';
      w3_show(el);
   }
   
   // show "Tn" in type menu entries that are otherwise blank but have list entry users
   var type_menu = kiwi_deep_copy(cfg.dx_type);
   for (i = 0; i < dx.DX_N_STORED; i++) {
      if (hdr.tc[i] != 0 && type_menu[i].name == '') {
         type_menu[i].name = '(T'+ i +')';    // make placeholder entry in menu, but not cfg.dx_type
         //console.log('empty type menu entry: T'+ i +' #'+ hdr.tc[i]);
      }
   }
   //console.log(type_menu);
   
   var new_len = +hdr.n;
   var cur_len = dx.o.len;
   //console.log('dx_render: n='+ new_len +' first='+ obj[1].g +' last='+ obj[obj.length-1].g);
   var s_a = dx.o.arr;
   var scroll_to_bottom = false;

   if (new_len != cur_len) {
      //console.log('$dx_render: new_len='+ new_len +' != cur_len='+ cur_len);
      
      if (new_len == (cur_len-1)) {
         s_a.pop();
         //console.log('pop');
      } else
      if (new_len == (cur_len+1)) {
         //s_a.push(w3_div('cl-dx', cur_len));
         s_a.push(w3_div('cl-dx', '&nbsp;'));
         //console.log('push');
      } else {
         //console.log('$$$ new_len NOT cur_len +/- 1');
         dx.o.last_start = 0;
         dx.o.last_end = new_len - 1;
      }

      dx.o.len = cur_len = new_len;
   }

   for (i = dx.o.last_start; i <= dx.o.last_end && i < s_a.length; i++) {
      //s_a[i] = w3_div('cl-dx', i);
      s_a[i] = w3_div('cl-dx', '&nbsp;');
   }
   
   if (cur_len != s_a.length) {
      //console.log('$$$ dx_render: FAIL cur_len='+ cur_len +' != s_a.length='+ s_a.length);
      //w3_innerHTML('id-dx-list', 'FAIL');
      dx.o.last_start = dx.o.last_end = 0;
      return;
   }

   dx.o.last_pb = [];
   dx.o.last_fd = [];

   // obj[0] is the header object, but we use its slot for the i = -1 legend hack
   for (j = 0; j < obj.length; j++) {
      i = j? obj[j].g : dx.LEGEND;
      //console.log('j='+ j +' i='+i);

      var d = null;
      var freq, mo, id, no = '';
      var pb = '', ty = 0, os, ext = '';
      var dow, begin, end, has_sched = false;
   
      // done this way so all the s_new code can be reused to construct the legend
      var h = function(psa) { return (i == dx.LEGEND)? 'w3-hide' : psa; }
      var l = function(label) { return (i == dx.LEGEND)? label : ''; }

      if (i != dx.LEGEND) {
         d = obj[j];
         freq = d.f + kiwi.freq_offset_kHz;
         mo = d.fl & dx.DX_MODE;
         ty = (d.fl & dx.DX_TYPE) >> dx.DX_TYPE_SFT;
         id = kiwi_decodeURIComponent('dx_id', d.i);
         if (d.n) no = kiwi_decodeURIComponent('dx_no', d.n);
         if (d.p) ext = kiwi_decodeURIComponent('dx_ext', d.p);
         os = d.o;

         var lo = d.lo, hi = d.hi;
         if (lo || hi) {
            if (lo == -hi) {
               pb = (Math.abs(hi)*2).toFixed(0);
            } else {
               pb = lo.toFixed(0) +', '+ hi.toFixed(0);
            }
         
            dx.o.last_pb[i] = [lo, hi];
         }

         dow = (d.fl & dx.DX_DOW) >> dx.DX_DOW_SFT;
         if (dow == 0) dow = dx.DX_DOW_BASE;
         dx.o.last_fd[i] = dow;

         begin = +(d.b), end = +(d.e);
         if (begin == 0 && end == 2400) end = 0;

         if (dow != dx.DX_DOW_BASE || begin != 0 || end != 0)
            has_sched = true;
         
         if (dx.open_sched == i) {
            has_sched = true;
            // dx.open_sched = -1 done below
         }
      }
   
      // 'path'+ '_'+i for compatibility with w3_remove_trailing_index(path, '_')

      //console.log('i='+ i +' mo='+ mo +' ty='+ ty);
      //console.log(d);
      var s_new =
         w3_inline_percent('w3-valign w3-text-black/w3-margin-T-8 w3-hspace-16 w3-margin-RE-16',
            w3_inline('w3-margin-L-8',
               (i == dx.LEGEND)? dx.spacer : w3_button(dx_btn(dx.button_add), '+', 'dx_list_add_cb', i),
               (i == dx.LEGEND)? dx.spacer : w3_button(dx_btn(dx.button_del), '-', 'dx_list_del_cb', i),
               (i == dx.LEGEND || has_sched)? dx.spacer :
                  w3_button('w3-padding-0', w3_icon('w3-pale-blue||title="add schedule time"', 'fa-clock-o', 26), 'dx_list_sched_add_cb', i)
            ), 6,
            //i.toString(), 4,     // debug
            w3_input(h('w3-padding-small||size=8'), l('Freq kHz'), 'dx.o.fr_'+i, freq, 'dx_num_cb'), 19,
            w3_select(h('w3-text-red'), l('Mode'), '', 'dx.o.fm_'+i, mo, kiwi.modes_u, 'dx_sel_cb'), 15,
            w3_input(h('w3-padding-small||size=4'), l('Passband Hz'), 'dx.o.pb_'+i, pb, 'dx_passband_cb'), 25,
            w3_select(h('w3-text-red'), l('Type'), '', 'dx.o.ft_'+i, ty, type_menu, 'dx_sel_cb'), 25,
            w3_input(h('w3-padding-small||size=2'), l('Offset Hz'), 'dx.o.o_'+i, os, 'dx_num_cb'), 18,
            w3_input(h('w3-padding-small'), l('Ident'), 'dx.o.i_'+i, id, 'dx_string_cb'), 40,
            w3_input(h('w3-padding-small'), l('Notes'), 'dx.o.n_'+i, no, 'dx_string_cb'), 40,
            w3_input(h('w3-padding-small'), l('Extension'), 'dx.o.p_'+i, ext, 'dx_string_cb'), 25
         );
      
      if (has_sched) {
         //console.log('DX dow='+ dow +'|'+ dow.toHex(2) +' f='+ freq.toFixed(2) +' b='+ d.b +' e='+ d.e +' '+ id);
         var dow_s = '';
         for (var k = 0; k < 7; k++) {
            var day = dow & (1 << (6-k));
            var fg = day? 'black' : 'white';
            var bg = day? dx.dow_colors[k] : 'darkGrey';
            dow_s += w3_button_path('w3-dummy//w3-round w3-padding-tiny'+
               (k? ' w3-margin-L-4':'') +'|color:'+ fg +'; background:'+ bg,
               'dx.o.d'+k+'_'+i, 'MTWTFSS'[k], 'dx_dow_button', k);
         }
         dow_s = w3_inline('', dow_s);

         var begin_s, end_s;
         if (begin == 0 && end == 0) {
            begin_s = end_s = '';
         } else {
            begin_s = begin.toFixed(0).leadingZeros(4);
            end_s = end.toFixed(0).leadingZeros(4);
         }

         s_new +=
            w3_inline_percent('w3-valign w3-text-black/w3-margin-T-8 w3-hspace-16 w3-margin-RE-16',
               w3_inline('w3-margin-L-8', dx.spacer, dx.spacer,
                  w3_button('w3-padding-0', w3_icon('w3-pale-blue||title="delete schedule time"', 'fa-trash-o', 26), 'dx_list_sched_del_cb', i)
               ), 6,
               dow_s, 15,
               w3_input('w3-label-inline/w3-padding-small', 'Begin', 'dx.o.b_'+i, begin_s, 'dx_sched_time_cb'), 10,
               w3_input('w3-label-inline/w3-padding-small', 'End', 'dx.o.e_'+i, end_s, 'dx_sched_time_cb'), 9
            );
         
         //console.log('i='+ i +' j='+ j +' dx.open_sched='+ dx.open_sched +' dx.o.len='+ dx.o.len);
         if (dx.open_sched == dx.o.len - 1) {
            scroll_to_bottom = true;
         }
         
         if (dx.open_sched == i) {
            dx.open_sched = -1;
         }
      }
   
      if (i == dx.LEGEND) {
         w3_innerHTML('id-dx-list-legend', s_new);
      } else {
         s_a[i] = s_new;
      }
   }

   w3_innerHTML('id-dx-list', s_a.join(''));
   dx.o.last_start = obj[1].g;
   dx.o.last_end = obj[obj.length-1].g;
   
   if (scroll_to_bottom) {
      dx.ignore_dx_update = true;
      w3_scrollDown('id-dx-list');
      //console.log('scroll_to_bottom');
   }
   
   if (dx.o.search_result) {
      //console.log('YELLOW idx='+ dx.o.search_result.idx);
      w3_add('dx.o.'+ (['fr_', 'i_', 'n_'][dx.o.search_result.which]) + dx.o.search_result.idx, 'w3-css-yellow');
      
      // keep highlight through back-to-back dx_render() calls
      setTimeout(function() { dx.o.search_result = null; }, 500);
   }
}

function dx_size_cb(len)
{
   var s_a = [];
   dx.o.len = +len;
   dx.o.arr = s_a;
   dx.o.last_start = dx.o.last_end = 0;
   for (var i = 0; i < len; i++) {
      //s_a[i] = w3_div('cl-dx', i);
      s_a[i] = w3_div('cl-dx', '&nbsp;');
   }

   w3_innerHTML('id-dx-list', s_a.join(''));
	dx_list_scroll();
}

function dx_list_scroll()
{
   // quantize scroll events a bit
   var pos = w3_scrolledPosition('id-dx-list');
   kiwi_clearTimeout(dx.scroll_timeout);
   dx.scroll_timeout = setTimeout(function() { dx_list_scroll_done(pos); }, 100);
}

function dx_list_scroll_done(pos)
{
   if (pos == -1) return;
   //console.log('dx_list_scroll '+ Math.round(pos * 100) +'%');
   var el = w3_el('id-dx-list');
   //console.log('dx_list_scroll scrollTop='+ el.scrollTop +' scrollHeight='+ el.scrollHeight +'(879*37.5='+ Math.round(879*37.5) +') clientHeight='+ el.clientHeight);
   var entries_visible = Math.round(el.clientHeight / 37.5);
   var slopL = 2, slopH = 4;
   var start = Math.round(pos * (dx.o.len - entries_visible));
   start = Math.max(0, start - slopL);
   var end = start + entries_visible;
   end = Math.min(end + slopH, dx.o.len);
   //console.log('dx_list_scroll '+ pos.toFixed(2) +' '+ start +'|'+ end +' ent='+ entries_visible);
   
   // callback to dx_render() in response to server "admin_mkr=..."
	ext_send('SET MARKER idx1='+ start +' idx2='+ end);
}

// callback from "SET DX_UPD" when update complete -- force list refresh
function dx_update_admin()
{
   var pos = w3_scrolledPosition('id-dx-list');
   console.log('dx_update_admin: pos='+ pos);
   dx_list_scroll_done(pos);
}

function dx_list_add_cb(path, idx)
{
   idx = +idx;
   //console.log('dx_list_add_cb idx='+ idx);
   dx_update_check(idx, dx.UPD_ADD);   // dx_update_check() calls dx_save()
}

function dx_list_del_cb(path, idx)
{
   idx = +idx;
   //console.log('dx_list_del_cb idx='+ idx);
   ext_send('SET DX_UPD g='+ idx +' f=-1');
   dx_save('id-dx-list-saved', dx.SAVE_NOW);
}

function dx_list_sched_add_cb(path, idx)
{
   dx.open_sched = +idx;
   dx_update_admin();      // refresh list
}

function dx_list_sched_del_cb(path, idx)
{
   dx.o.last_fd[idx] = dx.DX_DOW_BASE;
   w3_set_value('dx.o.b_'+ idx, 0);
   w3_set_value('dx.o.e_'+ idx, 2400);
   dx_update_check(idx, dx.UPD_MOD);
   dx.ignore_dx_update = false;
   dx_update_admin();      // refresh list
}

function dx_search_freq_cb(path, val, first)
{
   if (first) return;
   val = val.parseFloatWithUnits('kM', 1e-3);
   if (isNaN(val)) {
      w3_set_value(path, '');
   } else {
      //console.log('dx_search_freq_cb val='+ val);
      ext_send('SET MARKER search_freq='+ val);
   }
}

function dx_search_ident_cb(path, val, first, cb_a)
{
   if (first) return;
   //console.log('dx_search_ident_cb idx='+ dx.o.last_search_idx +' val='+ val +' from='+ cb_a[1]);
   if (cb_a[1] == 'ev') return;     // so we don't run twice
   var idx = dx.o.last_search_idx;
   if (idx >= dx.o.len) idx = 0;
	ext_send('SET MARKER idx='+ idx +' search_ident='+ encodeURIComponent(val));
}

function dx_search_notes_cb(path, val, first, cb_a)
{
   if (first) return;
   //console.log('dx_search_notes_cb idx='+ dx.o.last_search_idx +' val='+ val +' from='+ cb_a[1]);
   if (cb_a[1] == 'ev') return;     // so we don't run twice
   var idx = dx.o.last_search_idx;
   if (idx >= dx.o.len) idx = 0;
	ext_send('SET MARKER idx='+ idx +' search_notes='+ encodeURIComponent(val));
}

// callback from "SET MARKER idx= search_xxx=" above
function dx_search_pos_cb(p)
{
   var p = p.split(',');
   var which = +p[0];
   var idx = +p[1];

   if (idx < 0) {
      //console.log('dx_search_pos_cb NOT FOUND idx=-1');
      if (w3_scrollTo('id-dx-list', 0)) dx_list_scroll();
      dx_search_check_wrap(idx);
   } else {
      var idx_hi = dx.o.len - 1;
      var pos = idx_hi? (idx / idx_hi) : 0;     // avoid /0
      //console.log('dx_search_pos_cb which='+ which +' idx='+ idx +' pos='+ pos.toFixed(2));
      if (w3_scrollTo('id-dx-list', +pos, /*true*/ false)) dx_list_scroll();
      dx_search_check_wrap(idx);
      //console.log('### last_search_idx='+ dx.o.last_search_idx);
      dx.o.search_result = { which:which, idx:idx };
   }
}

function dx_search_check_wrap(result_idx)
{
   var rtn = (result_idx >= 0 && result_idx >= dx.o.last_search_idx);
   //console.log('dx_search_check_wrap result_idx='+ result_idx +' last_search_idx='+ dx.o.last_search_idx +' rtn='+ rtn);
   dx.o.last_search_idx = result_idx + 1;     
   if (rtn) return;

   var el = w3_el('id-search-wrap-container');
   el.style.opacity = 0.8;
   w3_show(el);
   var el2 = w3_el('id-search-wrap');
   el2.style.marginTop = px(w3_center_in_window(el2, 'SW'));
   setTimeout(function() {
      el.style.opacity = 0;      // CSS is setup so opacity fades
      setTimeout(function() { w3_hide(el); }, 500);
   }, 300);
}

function dx_export_cb(path, which)
{
   //console.log('dx_export_cb which='+ which);
   dx.export_which = which;
   dx.export_active = true;
   admin.long_running = true;
   w3_innerHTML('id-dx-export-label', dx.export_label + w3_icon('w3-text-teal w3-margin-left', 'fa-refresh fa-spin', 24));
   dx.export_s_a = [];
	dx_export(0);
}

function dx_export(idx1)
{
   var last_idx2 = dx.o.len - 1;
   var idx2 = Math.min(idx1 + 99, last_idx2);
   //console.log('SET MARKER idx1='+ idx1 +' idx2='+ idx2 +' last_idx2='+ last_idx2);
	ext_send('SET MARKER idx1='+ idx1 +' idx2='+ idx2);
	if (idx2 != last_idx2)
	   setTimeout(function() { dx_export(idx2+1); }, 250);
}

// called from dx_export_cb() via dx_render() when dx.export_active is true
function dx_export_cb2(obj)
{
   var last = false;
   var len = obj.length - 1;
   //console.log('dx_export_cb2 n='+ obj[0].n +' len='+ len +'('+ obj[1].g +','+ obj[len].g +')');
   //console.log(obj);
   //console.log(JSON.stringify(obj));
   var dx_dq = function(s) { return dq(kiwi_str_decode_selective_inplace(s, true)); };
   
   // create a JSON format file that exactly matches what server-side dx_save_as_json() produces
   if (dx.export_which == dx.DX_JSON) {

      obj.forEach(function(o, i) {
         if (i == 0) return;
         var gid = o.g;
         if (gid == dx.o.len - 1) last = true;
         var comma = last? '' : ',';
         var freq = o.f.toFixed(2);
         var mode = dq(kiwi.modes_u[o.fl & dx.DX_MODE]);
         var ident = dx_dq(o.i);
         var notes = o.n? dx_dq(o.n) : '""';
         
         var opt = '';
         var type = (o.fl & dx.DX_TYPE) >> dx.DX_TYPE_SFT;
         type = type? (dq('T'+ type) +':1') : '';
         var pb = (o.lo != 0 || o.hi != 0)? (dqc('lo') + o.lo +', '+  dqc('hi') + o.hi) : '';
         var off = o.o? (dqc('o') + o.o) : '';
         var ext = (o.p && o.p != '')? (dqc('p') + dx_dq(o.p)) : '';
         var dow = (o.fl & dx.DX_DOW);
         dow = (dow != 0 && dow != dx.DX_DOW)? (dqc('d0') + (dow >> dx.DX_DOW_SFT)) : '';
         var begin = o.b, end = o.e;
         var time = (begin != 0 || end != 2400)? (dqc('b0') + begin +', '+ dqc('e0') + end) : '';

         opt = w3_sbc(', ', type, pb, off, dow, time, ext);
         if (opt != '') opt = ', {'+ opt +'}';
         dx.export_s_a[gid] = '['+ freq +', '+ mode +', '+ ident +', '+ notes + opt +']'+ comma +'\n';
      });
   } else
   
   if (dx.export_which == dx.DX_CSV) {
      var re_dq_g = /\"/g;
      
      obj.forEach(function(o, i) {
         if (i == 0) {
            dx.export_s_a[0] = 'Freq kHz;"Mode";"Ident";"Notes";"Extension";"Type";PB low;PB high;Offset;"DOW";Begin;End\n';
            return;
         }
         var gid = o.g;
         if (gid == dx.o.len - 1) last = true;
         var mode = kiwi.modes_u[o.fl & dx.DX_MODE];
         var type = (o.fl & dx.DX_TYPE) >> dx.DX_TYPE_SFT;
         type = type? dq('T'+ type) : '';
         var lo = o.lo, hi = o.hi;
         if (lo == 0 && hi == 0) lo = hi = '';
         var off = o.o? o.o : '';
         
         var dow = (o.fl & dx.DX_DOW) >> dx.DX_DOW_SFT;
         dow_s = '';
         if (dow != 0 && dow != dx.DX_DOW_BASE) {
            for (var j = 0; j < 7; j++) {
               if (dow & (1 << (6-j))) dow_s += 'MTWTFSS'[j]; else dow_s += '_';
            }
            dow_s = dq(dow_s);
         }
         var begin = o.b, end = o.e;
         if (begin == 0 && end == 2400) {
            begin = end = '';
         } else {
            // to preserve leading zeros in spreadsheet number field add leading single-quote
            begin = "'"+ begin.toFixed(0).leadingZeros(4);
            end = "'"+ end.toFixed(0).leadingZeros(4);
         }

         var ident = (o.i != '')? dx_dq(o.i) : '';
         var notes = o.n? dx_dq(o.n) : '';
         var ext = o.p? dx_dq(o.p) : '';

         var s = [ o.f, dq(mode), ident, notes, ext, type, lo, hi, off, dow_s, begin, end ];
         dx.export_s_a[gid+1] = s.join(';') +'\n';    // gid+1 because export_s_a[0] has CSV column legend
      });
   }
   
   if (!last) return;
   var mime_type, file_ext;
   var wrap_begin, wrap_end;

   if (dx.export_which == dx.DX_JSON) {
      //console.log('dx_export_cb2 JSON');
      mime_type = 'application/json';
      file_ext = '.json';
      wrap_begin = '{"dx":[\n';
      wrap_end = ']}\n';
   }
   
   if (dx.export_which == dx.DX_CSV) {
      //console.log('dx_export_cb2 CSV');
      mime_type = 'text/csv';
      file_ext = '.csv';
      wrap_begin = wrap_end = '';
   }
   
   var tstamp = kiwi_host() +'_'+ new Date().toISOString().replace(/:/g, '_').replace(/\.[0-9]+Z$/, 'Z');
   var txt = new Blob([wrap_begin + dx.export_s_a.join('') + wrap_end], { type: mime_type });
   var a = document.createElement('a');
   a.style = 'display: none';
   a.href = window.URL.createObjectURL(txt);
   a.download = 'dx.'+ tstamp + file_ext;
   document.body.appendChild(a);
   //console.log('dx_export_cb2: '+ a.download);
   a.click();
   window.URL.revokeObjectURL(a.href);
   document.body.removeChild(a);

   dx.export_active = false;
   admin.long_running = false;
   w3_innerHTML('id-dx-export-label', dx.export_label);
}

function dx_import_cb(path, which)
{
   dx.current_import = which;
   
   // Don't show spinner, because if the user cancels out of file dialog no event is ever generated.
   // Process happens quickly though, even for large files. So this isn't really a problem.
   //w3_innerHTML('id-dx-import-label', dx.import_label + w3_icon('w3-text-teal w3-margin-left', 'fa-refresh fa-spin', 24));

   if (which == dx.DX_JSON) {
      //console.log('dx_import_cb JSON');
      var el = w3_el('id-dx-import-form');
	   el.setAttribute('accept', '.json,application/json');
      el.click();
   }

   if (which == dx.DX_CSV) {
      //console.log('dx_import_cb CSV');
      var el = w3_el('id-dx-import-form');
	   el.setAttribute('accept', '.csv,text/csv');
      el.click();
   }
}

// callback from id-dx-import-form.click() above
function dx_file_upload_cb(ev)
{
   //console.log('dx_file_upload_cb');
   //console.log(ev);
	ext_get_authkey(function(key) {
		dx_file_upload2(key);
	});
}

function dx_file_upload2(key)
{
   //console.log('dx_file_upload2 key='+ key);
	var browse = w3_el('id-dx-import-form');
	var file = browse.files[0];
	//console.log(file);
	var fdata = new FormData();
	fdata.append((dx.current_import == dx.DX_JSON)? 'json' : 'csv', file, file.name);
	kiwi_ajax_send(fdata, '/DX?'+ key, 'dx_file_uploaded');
}

function dx_file_uploaded(o)
{
	var e;
	var rc = isDefined(o.AJAX_error)? -1 : o.rc;
	var csv_line = 'CSV line '+ o.line +': ';
	
	//console.log('## dx_file_uploaded rc='+ rc);
	
	switch (rc) {
      case -1:
         e = 'Communication error';
         break;
      case 0:
         e = 'Successful -- Must restart to use new label data';
         break;
      case 1:
         e = 'Authentication failed';
         break;
      case 2:
         e = 'File too large';
         break;
      case 3:
         e = 'File write error';
         break;
      case 4:
         e = 'Bad upload type';
         break;
      case 10:
         e = csv_line +'expected 12 fields, got '+ o.nfields;
         break;
      case 11:
         e = csv_line +'expected frequency number in field 1';
         break;
      case 12:
         e = csv_line +'expected mode string in field 2';
         break;
      case 13:
         e = csv_line +'expected ident string in field 3';
         break;
      case 14:
         e = csv_line +'expected notes string in field 4';
         break;
      case 15:
         e = csv_line +'expected extension string in field 5';
         break;
      case 16:
         e = csv_line +'expected type string in field 6';
         break;
      case 17:
         e = csv_line +'expected passband low number in field 7';
         break;
      case 18:
         e = csv_line +'expected passband high number in field 8';
         break;
      case 19:
         e = csv_line +'expected offset freq number in field 9';
         break;
      case 20:
         e = csv_line +'expected day-of-week string in field 10';
         break;
      case 21:
         e = csv_line +'day-of-week string in field 10 is not 7 characters long (i.e. "MTWTFSS")';
         break;
      case 22:
         e = csv_line +'expected begin time number in field 11';
         break;
      case 23:
         e = csv_line +'expected end time number in field 12';
         break;
      default:
         e = 'Undefined error?';
         break;
	}

   var el = w3_el('id-dx-list-msg');
   w3_remove_then_add(el, 'w3-red w3-green', rc? 'w3-red' : 'w3-green');
   el.innerHTML = 'File upload: '+ e;
   w3_show(el);
   //w3_innerHTML('id-dx-import-label', dx.import_label);

   if (rc == 0) w3_restart_cb();
}


// dx: type menu

function dx_type_render()
{
   var i, type;
   var cbf = 'dx_type_field_cb|';
   var s_a = [];
   var icon = 'w3-link-darker-color w3-help';
   var i_psa = 'w3-margin-L-8 w3-center w3-bold|width:60px';
   var type_i = 'The code appearing in the\ntype field of the dx.json file.';
   var type_l = 'Type '+ w3_icon(icon +'||title='+ dq(type_i), 'fa-info-circle', 20);
   var name_i = 'The menu entry names. The ordering here determines\nthe ordering of entries in the menu. See instructions.';
   var name_l = 'Menu entry name '+ w3_icon(icon +'||title='+ dq(name_i), 'fa-info-circle', 20);
   var color_i = 'An HTML color specification, including\ncolor name, hex or HSL value etc.';
   var color_l = 'HTML color name '+ w3_icon(icon +'||title='+ dq(color_i), 'fa-info-circle', 20);
   var test_i = 'How the colors of the labels with\nthis type value should appear.';
   var test_l = 'Test '+ w3_icon(icon +'||title='+ dq(test_i), 'fa-info-circle', 20);

   for (var i = -1; i < cfg.dx_type.length; i++) {
      var legend = (i == dx.LEGEND);
      type = legend? {} : cfg.dx_type[i];

      // done this way so all the s_new code can be reused to construct the legend
      var h = function(psa) { return legend? 'w3-hide' : psa; }
      var l = function(label) { return legend? label : ''; }
      var i_s = legend? w3_label(i_psa, type_l) : w3_div(i_psa, 'T'+ i.toFixed(0));
      var masked = (i == cfg.dx_type.length - 1);
      var input_psa = w3_psa_mix('w3-padding-small||size=8', masked? 'w3-cursor-not-allowed':'', '', masked? 'disabled':'');
   
      var s_new =
         // + ((i >= 1)? '/w3-halign-center':''
         w3_inline_percent('w3-valign w3-text-black/w3-margin-T-8 w3-hspace-16 w3-margin-RE-16',
            w3_inline('w3-margin-L-8', i_s), 6,
            w3_input(h(input_psa), l(name_l), i+'_type.name', type.name, cbf +'0'), 13,
            w3_input(h(input_psa), l(color_l), i+'_type.color', type.color, cbf +'1'), 13,
            (legend?
               w3_label('w3-bold', test_l)
            :
               ((type.name == '' || masked)?
                  ''
               :
                  w3_inline('w3-halign-space-between/',
                     w3_div('id-'+ i +'_type.ex cl-dx-type-test'),
                     w3_div('id-'+ i +'_type.exf cl-dx-type-test cl-dx-type-filtered-test')
                  )
               )
            ), 20
         );
   
      if (legend) {
         w3_innerHTML('id-dx-type-list-legend', s_new);
      } else {
         s_a[i] = s_new;
      }
   }

   w3_innerHTML('id-dx-type-list', s_a.join(''));

   dx_color_init();
   for (var i = 0; i < (cfg.dx_type.length - 1); i++) {     // length -1 to skip DX_MASKED
      dx_type_bar_test(i);
   }
}

function dx_type_bar_test(i)
{
   type = cfg.dx_type[i];
   var el = w3_el(i +'_type.ex');
   if (!el) return;
   el.innerHTML = type.name;
   el.style.background = cfg.dx_type[i].color;
   var el = w3_el(i +'_type.exf');
   el.innerHTML = type.name;
   el.style.background = dx.stored_colors_light[i];
}

function dx_type_field_cb(path, val, first, a_cb)
{
   if (first) return;
   var i = parseInt(path);
   var obj = cfg.dx_type[i];
   console.log('dx_type_field_cb path='+ path +' val='+ val +' i='+ i +' obj='+ obj);
   //console.log(obj);
   var key = obj.key;
   var which = +(a_cb[1]);
   console.log('dx_type_field_cb key='+ key +' which='+ which);
   
   if (which == 0) {
      console.log('cfg.dx_type['+ key +'].name='+ val);
      cfg.dx_type[i].name = val;
      dx_type_render();
      dx_update_admin();      // have to rebuild dx type menus when type names change
   } else
   
   if (which == 1) {
      console.log('cfg.dx_type['+ key +'].color='+ val);
      var el = w3_el(path);
      var colorObj = w3color(val);
      if (!colorObj.valid) {
         el.style.background = 'red';
         console.log('invalid dx type color name');
         return;
      }
      el.style.background = '';     // remove any prior red
      cfg.dx_type[i].color = val;
      dx_color_init();
      dx_type_bar_test(i);
   }
   
   dx_sched_save('id-dx-type-saved');
}


// dx: band bars

function band_bar_render()
{
   var i, b1, b2;
   var s_a = [];
   var cbf = 'band_bar_float_cb|';
   var icon = 'w3-link-darker-color w3-help';
   var freq_i = 'If an entry is defining a single frequency in the select band menu,\n' +
      'and not a band bar, then set Min equal to Max.';
   var freq_l = 'Min kHz '+ w3_icon(icon +'||title='+ dq(freq_i), 'fa-info-circle', 20);
   var band_i = 'Name that appears in both the band bar\nand select band menu entry.';
   var band_l = 'Band name '+ w3_icon(icon +'||title='+ dq(band_i), 'fa-info-circle', 20);
   var svc_i = 'Service class as defined by next section.';
   var svc_l = 'Service '+ w3_icon(icon +'||title='+ dq(svc_i), 'fa-info-circle', 20);
   var itu_i = 'ITU region or visibility of band bar or frequency entry.';
   var itu_l = 'ITU / Visibility '+ w3_icon(icon +'||title='+ dq(itu_i), 'fa-info-circle', 20);
   var chan_i = 'Defines the band channel spacing (kHz).\nFor example with LW and MW bands.';
   var chan_l = 'Chan '+ w3_icon(icon +'||title='+ dq(chan_i), 'fa-info-circle', 20);
   var fms_i = 'Optional frequency and mode to set\nwhen select band entry used.';
   var fms_l = 'Band freq/mode '+ w3_icon(icon +'||title='+ dq(fms_i), 'fa-info-circle', 20);
   
   for (i = dx.LEGEND; i < cfg.bands.length; i++) {
      b1 = (i != dx.LEGEND)? cfg.bands[i] : {};

      // done this way so all the s_new code can be reused to construct the legend
      var h = function(psa) { return (i == dx.LEGEND)? 'w3-hide' : psa; }
      var l = function(label) { return (i == dx.LEGEND)? label : ''; }
      
      var svc;
      if (i == dx.LEGEND) {
         svc = null;
      } else {
         svc = band_svc_lookup(b1.svc);   // remember, b1.svc is a key e.g. 'B' = bcast
         if (svc != null) svc = svc.i;    // svc could be null if corresponding cfg.band_svc has disappeared (menu will show 'unknown')
      }
      var itu = (b1.itu > 0)? b1.itu : 0;
      
      var s_new =
         w3_inline_percent('w3-valign w3-text-black/w3-margin-T-8 w3-hspace-16 w3-margin-RE-16',
            w3_inline('w3-margin-L-8',
               (i == dx.LEGEND)? dx.spacer : w3_button(dx_btn(dx.button_add), '+', 'band_bar_add_cb', i),
               (i == dx.LEGEND)? dx.spacer : w3_button(dx_btn(dx.button_del), '-', 'band_bar_del_cb', i)
            ), 2,
            w3_input(h('w3-padding-small||size=8'), l(freq_l), i+'_bands.min', b1.min, cbf +'0'), 10,
            w3_input(h('w3-padding-small||size=8'), l('Max kHz'), i+'_bands.max', b1.max, cbf +'1'), 10,
            w3_input(h('w3-padding-small'), l(band_l), i+'_bands.n', b1.name, 'band_bar_field_cb|0'), 25,
            w3_select(h('w3-text-red'), l(svc_l), '', i+'_bands.s', svc, cfg.band_svc, 'band_bar_select_cb', 's'), 12,
            w3_select(h('w3-text-red'), l(itu_l), '', i+'_bands.i', itu, kiwi.ITU_s, 'band_bar_select_cb', 'i'), 25,
            w3_input(h('w3-padding-small||size=6'), l(chan_l), i+'_bands.c', b1.chan, cbf +'2'), 7,
            w3_input(h('w3-padding-small||size=8'), l(fms_l), i+'_bands.fms', b1.sel, 'band_bar_field_cb|1'), 15
         );
      
      if (i == dx.LEGEND) {
         w3_innerHTML('id-band-bar-list-legend', s_new);
      } else {
         s_a[i] = s_new;
      }
   }
   w3_innerHTML('id-band-bar-list', s_a.join(''));
}

function band_bar_add_cb(path, val, first)
{
   if (first) return;
   var i = +val;
   console.log('band_bar_add_cb path='+ path +' i='+ i);
   if (i < 0 || i >= cfg.bands.length) return;
   var b = cfg.bands[i];
   var add = { min:b.min, max:b.max, name:b.name, svc:b.svc, itu:b.itu, sel:b.sel, chan:b.chan };
   cfg.bands.splice(i+1, 0, add);
   dx_save('id-band-bar-saved', dx.SAVE_NOW);
   bands_addl_info();
   band_bar_render();
}

function band_bar_del_cb(path, val, first)
{
   if (first) return;
   var i = +val;
   console.log('band_bar_del_cb path='+ path +' i='+ i);
   if (i < 0 || i >= cfg.bands.length) return;
   cfg.bands.splice(i, 1);
   dx_save('id-band-bar-saved', dx.SAVE_NOW);
   bands_addl_info();
   band_bar_render();
}

function band_bar_float_cb(path, val, first, a_cb)
{
   if (first) return;
   var i = parseInt(path);
   if (i < 0 || i >= cfg.bands.length) return;
   var which = +(a_cb[1])
   var prior = cfg.bands[i][kiwi.cfg_fields[which]];

   val = parseFloat(val);
   console.log('band_bar_float_cb path='+ path +' val='+ val +' i='+ i +' which='+ which +' prior='+ prior);
   val = +(val.toFixed(3));   // NB: .toFixed() does rounding
   if (!isNumber(val)) val = prior;
   w3_set_value(path, val? val : '');  // remove any non-numeric part from field, blank zero value
   
   if (which == 0) {
      console.log('cfg.bands['+ i +'].min='+ val);
      cfg.bands[i].min = val;
   } else
   if (which == 1) {
      console.log('cfg.bands['+ i +'].max='+ val);
      cfg.bands[i].max = val;
   } else
   if (which == 2) {
      console.log('cfg.bands['+ i +'].chan='+ val);
      cfg.bands[i].chan = val;
   }
   
   dx_sched_save('id-band-bar-saved');
}

function band_bar_field_cb(path, val, first, a_cb)
{
   if (first) return;
   var i = parseInt(path);
   console.log('band_bar_field_cb path='+ path +' val='+ val +' i='+ i);
   if (i < 0 || i >= cfg.bands.length) return;
   var which = +(a_cb[1])
   if (which == 0 && val == '') {      // prevent blank menu entries
      val = '(blank)';
      w3_set_value(path, val);
   }
   if (which == 0) cfg.bands[i].name = val; else
   if (which == 1) cfg.bands[i].sel = val;
   dx_sched_save('id-band-bar-saved');
}

function band_bar_select_cb(path, val, first, which)
{
   if (first) return;
   var i = parseInt(path);
   val = +val;
   console.log('band_bar_select_cb path='+ path +' val='+ val +' i='+ i +' which='+ which);
   if (i < 0 || i >= cfg.bands.length) return;

   if (which == 's') {
      var key = cfg.band_svc[val].key;
      console.log('cfg.bands['+ i +'].svc='+ key);
      cfg.bands[i].svc = key;
   } else
   if (which == 'i') {
      console.log('cfg.bands['+ i +'].itu='+ val);
      cfg.bands[i].itu = val;
   }

   dx_sched_save('id-band-bar-saved');
}


// dx: band service menu

function band_svc_render()
{
   var i, svc;
   var cbf = 'band_svc_field_cb|';
   var s_a = [];
   var icon = 'w3-link-darker-color w3-help';
   var name_i = 'Changes made here should immediately\nbe visible in Service menu above.';
   var name_l = 'Menu entry name '+ w3_icon(icon +'||title='+ dq(name_i), 'fa-info-circle', 20);
   var color_i = 'An HTML color specification, including\ncolor name, hex or HSL value etc.';
   var color_l = 'HTML color name '+ w3_icon(icon +'||title='+ dq(color_i), 'fa-info-circle', 20);
   var lname_i = 'Optional name shown in band bar, instead of menu entry name at left,\nwhen zoomed in and band bar is wide.';
   var lname_l = 'Long name '+ w3_icon(icon +'||title='+ dq(lname_i), 'fa-info-circle', 20);
   var test_i = 'Example of how band bar will look.';
   var test_l = '&nbsp;Test '+ w3_icon(icon +'||title='+ dq(test_i), 'fa-info-circle', 20);

   for (var i = -1; i < cfg.band_svc.length; i++) {
      svc = (i != dx.LEGEND)? cfg.band_svc[i] : {};

      // done this way so all the s_new code can be reused to construct the legend
      var h = function(psa) { return (i == dx.LEGEND)? 'w3-hide' : psa; }
      var l = function(label) { return (i == dx.LEGEND)? label : ''; }
   
      var s_new =
         w3_inline_percent('w3-valign w3-text-black/w3-margin-T-8 w3-hspace-16 w3-margin-RE-16',
            w3_inline('w3-margin-L-8',
               (i == dx.LEGEND)? dx.spacer : w3_button(dx_btn(dx.button_add), '+', 'band_svc_add_cb', i),
               (i == dx.LEGEND)? dx.spacer : w3_button(dx_btn(dx.button_del), '-', 'band_svc_del_cb', i)
            ), 2,
            w3_input(h('w3-padding-small||size=8'), l(name_l), i+'_svc.name', svc.name, cbf +'0'), 13,
            w3_input(h('w3-padding-small||size=8'), l(color_l), i+'_svc.color', svc.color, cbf +'1'), 13,
            w3_input(h('w3-padding-small||size=8'), l(lname_l), i+'_svc.lname', svc.longName, cbf +'2'), 30,
            ((i == dx.LEGEND)?
               w3_label('w3-bold', test_l)
            :
               w3_div('id-'+ i +'_svc.ex cl-band-bar-test')
            ), 30
         );
   
      if (i == dx.LEGEND) {
         w3_innerHTML('id-band-svc-list-legend', s_new);
      } else {
         s_a[i] = s_new;
      }
   }

   w3_innerHTML('id-band-svc-list', s_a.join(''));

   for (var i = 0; i < cfg.band_svc.length; i++) {
      band_svc_bar_test(i);
   }
}

function band_svc_bar_test(i)
{
   svc = cfg.band_svc[i];
   var el = w3_el(i +'_svc.ex');
   el.innerHTML = (isDefined(svc.longName) && svc.longName != '')? svc.longName : svc.name;

   // derive equivalent RGB color given opacity against a white background
   var color = w3_fg_color_with_opacity_against_bk_color(svc.color, 0.2, 'white');
   el.style.background = color.toRgbString();
   el.style.color = svc.color;
}

function band_svc_new_key()
{
   var max = -1;
   cfg.band_svc.forEach(function(a, i) {
      var key = +a.key;
      if (isNumber(key) && key > max) max = key;
   });

   max++;
   console.log('band_svc_new_key key='+ max);
   return max.toFixed(0);
}

function band_svc_add_cb(path, val, first)
{
   if (first) return;
   var i = +val;
   console.log('band_svc_add_cb path='+ path +' i='+ i);
   if (i < 0 || i >= cfg.band_svc.length) return;
   var s = cfg.band_svc[i];
   console.log(s);
   var key = band_svc_new_key();
   var add = { key:key, name:s.name, color:s.color };
   if (s.longName) add.longName = s.longName;
   cfg.band_svc.splice(i+1, 0, add);
   dx_save('id-band-svc-saved', dx.SAVE_NOW);
   bands_addl_info();
   band_svc_render();
   band_bar_render();   // have to rebuild band bar service menus when services added/deleted
}

function band_svc_del_cb(path, val, first)
{
   if (first) return;
   var i = +val;
   console.log('band_svc_del_cb path='+ path +' i='+ i);
   if (i < 0 || i >= cfg.band_svc.length) return;
   cfg.band_svc.splice(i, 1);
   dx_save('id-band-svc-saved', dx.SAVE_NOW);
   bands_addl_info();
   band_svc_render();
   band_bar_render();   // have to rebuild band bar service menus when services added/deleted
}

function band_svc_field_cb(path, val, first, a_cb)
{
   if (first) return;
   var i = parseInt(path);
   var obj = cfg.band_svc[i];
   console.log('band_svc_field_cb path='+ path +' val='+ val +' i='+ i +' obj='+ obj);
   if (obj == null) return;
   console.log(obj);
   var key = obj.key;
   var which = +(a_cb[1]);
   console.log('band_svc_field_cb key='+ key +' which='+ which);
   var rebuild_band_bar_menus = false;
   
   if (which == 0) {
      if (val == '') {     // prevent blank menu entries
         val = '(blank)';
         w3_set_value(path, val);
      }
      console.log('cfg.band_svc['+ key +'].name='+ val);
      cfg.band_svc[i].name = val;
      rebuild_band_bar_menus = true;
   } else
   
   if (which == 1) {
      console.log('cfg.band_svc['+ key +'].color='+ val);
      var el = w3_el(path);
      var colorObj = w3color(val);
      if (val == '' || !colorObj.valid) {
         el.style.background = 'red';
         console.log('invalid band color name');
         return;
      }
      el.style.background = '';     // remove any prior red
      cfg.band_svc[i].color = val;
   } else
   
   if (which == 2) {
      console.log('cfg.band_svc['+ key +'].longName='+ val);
      cfg.band_svc[i].longName = val;
   }
   
   band_svc_bar_test(i);
   dx_sched_save('id-band-svc-saved');
   bands_addl_info();
   if (rebuild_band_bar_menus)
      band_bar_render();      // have to rebuild band bar service menus when service names change
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
   w3_show(id +'-container');
   w3_call(id +'_config_focus');
   ext_cur_nav = id;
}

function extensions_nav_blur(id, cb_arg)
{
   //console.log('extensions_nav_blur id='+ id);
   w3_hide(id +'-container');
   w3_call(id +'_config_blur');
}

var ext_seq = 0;

// called by extensions to register extension admin configuration
function ext_admin_config(id, nav_text, ext_html, focus_blur_cb)
{
   //console.log('ext_admin_config id='+ id +' nav_text='+ nav_text);
   // indicate we don't want a callback unless explicitly requested
   if (focus_blur_cb == undefined) focus_blur_cb = null;

	var ci = ext_seq % admin_colors.length;
	w3_el('id-extensions-nav').innerHTML +=
		w3_nav(admin_colors[ci] + ' w3-border', nav_text, id, 'extensions_nav');
	ext_seq++;
	w3_el('id-extensions-config').innerHTML += w3_div('id-'+ id +'-container w3-hide|width:95%', ext_html);
}

function ext_config_html(vars, cfg_prefix, nav_text, title_text, s)
{
   var id = vars.ext_name;
   vars.enable = ext_get_cfg_param(cfg_prefix +'.enable', true, EXT_SAVE);

	ext_admin_config(id, nav_text,
		w3_div('id-'+ id +' w3-text-teal w3-hide',
         w3_col_percent('w3-valign/',
            w3_div('w3-bold', title_text), 40,
            w3_inline('',
               w3_div('w3-bold w3-margin-R-8', 'User enabled?'),
               w3_switch('', 'Yes', 'No', cfg_prefix +'.enable', vars.enable, 'admin_radio_YN_cb'),
				   w3_div('w3-text-black w3-margin-L-32', 'Local connections exempt.')
            )
         ) +
			'<hr>' +
			(s? s:'')
		)
	);
}
