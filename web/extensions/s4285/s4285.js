// Copyright (c) 2016 John Seamons, ZL/KF6VO

var s4285_ext_name = 's4285';		// NB: must match s4285.c:s4285_ext.name

var s4285_first_time = 1;

function s4285_main()
{
	ext_switch_to_client(s4285_ext_name, s4285_first_time, s4285_recv);		// tell server to use us (again)
	if (!s4285_first_time)
		s4285_controls_setup();
	s4285_first_time = 0;
}

var s4285_cmd_e = { CMD1:0 };

function s4285_recv(data)
{
	var firstChars = getFirstChars(data, 3);
	
	// process data sent from server/C by ext_send_data_msg()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];

		if (cmd == s4285_cmd_e.CMD1) {
			// do something ...
		} else {
			console.log('s4285_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
		}
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_encoded_msg()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (1 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('s4285_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('s4285_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				s4285_controls_setup();
				break;

			case "s4285_command_cmd2":
				var arg = parseInt(param[1]);
				console.log('s4285_recv: cmd2 arg='+ arg);
				// do something ...
				break;

			default:
				console.log('s4285_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function s4285_controls_setup()
{
	var controls_html =
	"<div id='id-s4285-controls' style='color:white; width:auto; display:block'>"+
      	's4285 audio generator'+
	"</div>";

	ext_panel_show(controls_html, null, null);
	s4285_visible(1);
	ext_mode('usb');
	ext_passband(600, 3000);
	ext_send('SET run=1');
}

function s4285_blur()
{
	console.log('### s4285_blur');
	s4285_visible(0);		// hook to be called when controls panel is closed
}

// called to display HTML for configuration parameters in admin interface
function s4285_config_html()
{
	ext_admin_config(s4285_ext_name, 's4285',
		w3_divs('id-s4285 w3-text-teal w3-hide', '',
			'<b>s4285 configuration</b>' +
			'<hr>' +
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					admin_input('int1', 's4285.int1', 'admin_num_cb'),
					admin_input('int2', 's4285.int2', 'admin_num_cb')
				), '', ''
			)
		)
	);
}

function s4285_visible(v)
{
}
