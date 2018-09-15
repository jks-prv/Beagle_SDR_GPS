// Copyright (c) 2018 John Seamons, ZL/KF6VO

var cw = {
   ext_name: 'cw_decoder',    // NB: must match cw_decoder.cpp:cw_decoder_ext.name
   first_time: true,
   pboff: -1,

   // must set "remove_returns" since pty output lines are terminated with \r\n instead of \n alone
   // otherwise the \r overwrite logic in kiwi_output_msg() will be triggered
   console_status_msg_p: { scroll_only_at_bottom: true, process_return_nexttime: false, remove_returns: true, ncol: 135 },

};

function cw_decoder_main()
{
	ext_switch_to_client(cw.ext_name, cw.first_time, cw_decoder_recv);		// tell server to use us (again)
	if (!cw.first_time)
		cw_decoder_controls_setup();
	cw.first_time = false;
}

function cw_decoder_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0] >> 1;
		var ch = ba[0] & 1;
		var o = 1;
		var len = ba.length-1;

		console.log('cw_decoder_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('cw_decoder_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('cw_decoder_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				cw_decoder_controls_setup();
				break;

			case "cw_char":
				cw_decoder_output_char(param[1]);
				break;

			case "cw_wpm":
				w3_innerHTML('id-cw-wpm', param[1] +' WPM');
				break;

			default:
				console.log('cw_decoder_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function cw_decoder_output_char(c)
{
   cw.console_status_msg_p.s = c;      // NB: already encoded on C-side
   kiwi_output_msg('id-cw-console-msgs', 'id-cw-console-msg', cw.console_status_msg_p);
}

function cw_decoder_controls_setup()
{
   var data_html =
      time_display_html('cw') +
      
      w3_div('id-cw-data|width:'+ px(nt.lhs+1024) +'; height:200px; overflow:hidden; position:relative; background-color:black;',
         '<canvas id="id-cw-canvas" width="'+ (nt.lhs+1024) +'" height="200" style="left:0; position:absolute"></canvas>',
			w3_div('id-cw-console-msg w3-text-output w3-scroll-down w3-small w3-text-black|left:'+ px(nt.lhs) +'; width:1024px; height:200px; position:relative; overflow-x:hidden;',
			   '<pre><code id="id-cw-console-msgs"></code></pre>'
			)
      );

	var controls_html =
		w3_div('id-cw-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
            w3_col_percent('',
				   w3_div('w3-medium w3-text-aqua', '<b>CW decoder</b>'), 30,
					w3_div('', 'From Loftur Jonasson, TF3LJ / VE2LJX and <br> the <b><a href="https://github.com/df8oe/UHSDR" target="_blank">UHSDR project</a></b> &copy; 2016'), 55
				),
				w3_inline('',
               w3_button('w3-padding-smaller', 'Clear', 'cw_clear_cb', 0),
               w3_div('id-cw-wpm w3-margin-left ', '0 WPM')
            )
			)
		);
	
	ext_panel_show(controls_html, data_html, null);
	time_display_setup('cw');

	//navtex_canvas = w3_el('id-navtex-canvas');
	//navtex_canvas.ctx = navtex_canvas.getContext("2d");

	ext_set_controls_width_height(550, 90);
	
	/*
	var p = ext_param();
	console.log('Navtex p='+ p);
	var freq = parseFloat(p);
	if (freq) {
	   // select matching menu item frequency
	   var found = false;
      for (var i = 0; i < nt.n_menu; i++) {
         var menu = 'nt.menu'+ i;
         w3_select_enum(menu, function(option) {
            //console.log('CONSIDER '+ parseFloat(option.innerHTML));
            if (parseFloat(option.innerHTML) == freq) {
               navtex_pre_select_cb(menu, option.value, false);
               found = true;
            }
         });
         if (found) break;
      }
   }
   */
   
	ext_send('SET cw_start');
	cw_decoder_environment_changed();
}

function cw_decoder_environment_changed(changed)
{
   // detect passband offset change and inform C-side
   var pboff = Math.abs(ext_get_passband_center_freq() - ext_get_carrier_freq());
   if (cw.pboff != pboff) {
      //console.log('CW ENV new pboff='+ pboff);
	   ext_send('SET cw_pboff='+ pboff);
      cw.pboff = pboff;
   }
}

function cw_clear_cb(path, idx, first)
{
   if (first) return;
   cw.console_status_msg_p.s = encodeURIComponent('\f');
   kiwi_output_msg('id-cw-console-msgs', 'id-cw-console-msg', cw.console_status_msg_p);
}

function cw_decoder_blur()
{
	ext_set_data_height();     // restore default height
	ext_send('SET cw_stop');
}

function cw_decoder_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'CW decoder help') +
         '<br>This is a very early version of the decoder. Please consider it a work-in-progress. <br><br>' +
         'Use the CWN demod mode with its narrow passband to maximize the signal/noise ratio. ' +
         'The decoder doesn\'t seem to do well with weak or fading signals. ' +
         '';
      confirmation_show_content(s, 600, 150);
   }
   return true;
}
