// Copyright (c) 2016 John Seamons, ZL/KF6VO

var example_ext_name = 'example';		// NB: must match example.c:example_ext.name

var example_first_time = true;

function example_main()
{
	ext_switch_to_client(example_ext_name, example_first_time, example_recv);		// tell server to use us (again)
	if (!example_first_time)
		example_controls_setup();
	example_first_time = false;
}

var example_cmd_e = { CMD1:0 };

function example_recv(data)
{
	var firstChars = getFirstChars(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];

		if (cmd == example_cmd_e.CMD1) {
			// do something ...
		} else {
			console.log('example_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
		}
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (1 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('example_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('example_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				example_controls_setup();
				break;

			case "example_command_cmd2":
				var arg = parseInt(param[1]);
				console.log('example_recv: cmd2 arg='+ arg);
				// do something ...
				break;

			default:
				console.log('example_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function example_controls_setup()
{
   var data_html =
      '<div id="id-example-data" class="scale" style="width:1024px; height:30px; background-color:white; position:relative; display:none" title="example">' +
      	'example extension HTML in ext-data-container' +
      '</div>';

	var controls_html =
	"<div id='id-example-controls' style='color:white; width:auto; display:block'>"+
      	'example extension HTML in ext-controls-container'+
	"</div>";

	ext_panel_show(controls_html, data_html, null);
	example_visible(1);
}

function example_blur()
{
	console.log('### example_blur');
	example_visible(0);		// hook to be called when controls panel is closed
}

// called to display HTML for configuration parameters in admin interface
function example_config_html()
{
	ext_admin_config(example_ext_name, 'Example',
		w3_divs('id-example w3-text-teal w3-hide', '',
			'<b>Example configuration</b>' +
			'<hr>' +
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					w3_input_get_param('int1', 'example.int1', 'w3_num_cb'),
					w3_input_get_param('int2', 'example.int2', 'w3_num_cb')
				), '', ''
			)
		)
	);
}

function example_visible(v)
{
	visible_block('id-example-data', v);
}
