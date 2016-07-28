// Copyright (c) 2016 John Seamons, ZL/KF6VO

var iq_display_ext_name = 'iq_display';		// NB: must match iq_display.c:iq_display_ext.name

var iq_display_first_time = 1;

function iq_display_main()
{
	ext_switch_to_client(iq_display_ext_name, iq_display_first_time, iq_display_recv);		// tell server to use us (again)
	iq_display_first_time = 0;
	iq_display_controls_setup();
}

var iq_display_cmd_e = { CMD1:0 };

function iq_display_recv(data)
{
	var firstChars = getFirstChars(data, 3);
	
	// process data sent from server/C by ext_send_data_msg()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];

		if (cmd == iq_display_cmd_e.CMD1) {
			// do something ...
		} else {
			console.log('iq_display_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
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
				console.log('iq_display_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('iq_display_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				iq_display_controls_setup();
				break;

			case "iq_display_command_cmd2":
				var arg = parseInt(param[1]);
				console.log('iq_display_recv: cmd2 arg='+ arg);
				// do something ...
				break;

			default:
				console.log('iq_display_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function iq_display_controls_setup()
{
   var data_html =
      '<div id="id-iq_display-data" class="scale" style="width:1024px; height:30px; background-color:white; position:relative; display:none" title="iq_display">' +
      	'iq_display extension HTML in ext-data-container' +
      '</div>';

	var controls_html =
	"<div id='id-iq_display-controls' style='color:white; width:auto; display:block'>"+
      	'iq_display extension HTML in ext-controls-container'+
	"</div>";

	ext_panel_show(controls_html, data_html, null);
	iq_display_visible(1);
}

function iq_display_blur()
{
	console.log('### iq_display_blur');
	iq_display_visible(0);		// hook to be called when controls panel is closed
}

// called to display HTML for configuration parameters in admin interface
function iq_display_config_html()
{
	ext_admin_config(iq_display_ext_name, 'IQ',
		w3_divs('id-iq_display w3-text-teal w3-hide', '',
			'<b>IQ display configuration</b>' +
			'<hr>' +
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					admin_input('int1', 'iq_display.int1', 'admin_num_cb'),
					admin_input('int2', 'iq_display.int2', 'admin_num_cb')
				), '', ''
			)
		)
	);
}

function iq_display_visible(v)
{
	visible_block('id-iq_display-data', v);
}
