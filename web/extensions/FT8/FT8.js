// Copyright (c) 2023 John Seamons, ZL/KF6VO

var ft8 = {
   ext_name: 'FT8',     // NB: must match FT8.cpp:ft8_ext.name
   first_time: true,
   mode: 0,
   mode_s: [ 'FT8', 'FT4' ],

   freq_s: {
      'FT8': [ '1840', '3573', '5357', '7074', '10136', '14074', '18100', '21074', '24915', '28074' ],
      'FT4': [ '3575.5', '7047.5', '10140', '14080', '18104', '21140', '24919', '28180' ]
   },

   // must set "remove_returns" so output lines with \r\n (instead of \n alone) don't produce double spacing
   console_status_msg_p: { scroll_only_at_bottom: true, process_return_alone: false, remove_returns: true, cols: 135 },

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

			default:
				console.log('ft8_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function ft8_output_chars(c)
{
   ft8.console_status_msg_p.s = c;      // NB: already encoded on C-side
   ft8.log_txt += kiwi_remove_escape_sequences(kiwi_decodeURIComponent('FT8', c));

   // kiwi_output_msg() does decodeURIComponent()
   kiwi_output_msg('id-ft8-console-msgs', 'id-ft8-console-msg', ft8.console_status_msg_p);
}

function ft8_controls_setup()
{
	ft8.saved_mode = ext_get_mode();
   ext_tune(null, 'usb', ext_zoom.ABS, 11, 200, 3100);

   var data_html =
      time_display_html('ft8') +
      
      w3_div('id-ft8-data|left:150px; width:1044px; height:300px; overflow:hidden; position:relative; background-color:mediumBlue;',
			w3_div('id-ft8-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|width:1024px; height:300px; position:absolute; overflow-x:hidden;',
			   '<pre><code id="id-ft8-console-msgs"></code></pre>'
			)
      );

	var controls_html =
		w3_div('id-ft8-controls w3-text-white',
			w3_divs('',
            w3_col_percent('w3-valign/',
               w3_div('',
				      w3_div('w3-medium w3-text-aqua', '<b>FT8/FT4 decoder</b>')
				   ), 40,
					w3_div('', 'From <b><a href="https://github.com/kgoba/ft8_lib/tree/update_to_0_2" target="_blank">ft8_lib</a></b> Karlis Goba &copy; 2018'), 45
				),
				w3_inline('w3-margin-T-6/w3-margin-between-16',
               w3_select_hier('id-ft8-freq w3-text-red w3-width-auto', '', 'freq', 'ft8.freq_idx', -1, ft8.freq_s, 'ft8_freq_cb'),
               w3_select('w3-text-red', '', 'mode', 'ft8.mode', ft8.mode, ft8.mode_s, 'ft8_mode_cb'),
               w3_button('w3-padding-smaller w3-css-yellow', 'Clear', 'ft8_clear_button_cb'),
               w3_button('w3-padding-smaller w3-aqua', 'Test', 'ft8_test_cb')
            )
			)
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('ft8');

   ext_set_data_height(300);
	ext_set_controls_width_height(550, 100);
   ft8_clear_button_cb();
	ext_send('SET ft8_start='+ ft8.mode);

	ft8.url_params = ext_param();
	var p = ft8.url_params.split(',');
	var do_test = 0;
   p.forEach(function(a, i) {
      if (w3_ext_param('test', a).match) {
         do_test = 1;
      }
   });
   if (do_test) ft8_test_cb();
}

function ft8_freq_cb(path, idx, first)
{
	if (first) return;
   idx = +idx;
	var freq = +w3_select_get_value('id-ft8-freq', idx);
   ext_tune(freq, 'usb', ext_zoom.ABS, 11);
	console.log('ft8_freq_cb: idx='+ idx +' freq='+ freq);
}

function ft8_mode_cb(path, idx, first)
{
	console.log('ft8_mode_cb: idx='+ idx +' first='+ first);
	if (first) return;
   idx = +idx;
   ft8.mode = idx;
	w3_set_value(path, idx);
   ext_send('SET ft8_free');
	ext_send('SET ft8_start='+ ft8.mode);
}

function ft8_clear_button_cb(path, idx, first)
{
   if (first) return;
   ft8.console_status_msg_p.s = encodeURIComponent('\f');
   kiwi_output_msg('id-ft8-console-msgs', 'id-ft8-console-msg', ft8.console_status_msg_p);
   ft8.log_txt = '';
}

function ft8_test_cb()
{
   ext_send('SET ft8_test');
}

function FT8_blur()
{
	ext_set_data_height();     // restore default height
	ext_set_mode(ft8.saved_mode);
	ext_send('SET ft8_close');
   kiwi_clearInterval(ft8.log_interval);
}

// called to display HTML for configuration parameters in admin interface
function FT8_config_html()
{
   ext_config_html(ft8, 'ft8', 'FT8', 'FT8/FT4 configuration');
}

function FT8_help(show)
{
   if (show) {
      var s =
         w3_text('w3-medium w3-bold w3-text-aqua', 'FT8/FT4 decoder help') +
         '...' +
         '';
      confirmation_show_content(s, 610, 150);
   }
   return true;
}
