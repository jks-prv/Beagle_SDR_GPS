// Copyright (c) 2017 John Seamons, ZL/KF6VO

var example = {
   ext_name: 'example',    // NB: must match example.c:example_ext.name
   first_time: true,
   CMD1: 0
};

function example_main()
{
	ext_switch_to_client(example.ext_name, example.first_time, example_recv);		// tell server to use us (again)
	if (!example.first_time)
		example_controls_setup();
	example.first_time = false;
}

function example_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];

		if (cmd == example.CMD1) {
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
      time_display_html('example') +

      w3_div('id-example-data scale|left:150px; width:1024px; height:200px; background-color:white; position:relative;',
      	'example extension HTML in ext-data-container'
      );

	var controls_html =
		w3_div('id-example-controls w3-text-white',
      	'example extension HTML in ext-controls-container'
      );

	ext_panel_show(controls_html, data_html, null);
	time_display_setup('example');
	example_environment_changed( {resize:1} );
}

// automatically called on changes in the environment
function example_environment_changed(changed)
{
   if (!changed.resize) return;
	var el = w3_el('id-example-data');
	var left = (window.innerWidth - 1024 - time_display_width()) / 2;
	el.style.left = px(left);
}

function example_blur()
{
   // anything that needs to be done when extension blurred (closed)
}

// called to display HTML for configuration parameters in admin interface
function example_config_html()
{
	ext_admin_config(example.ext_name, 'Example',
		w3_div('id-example w3-text-teal w3-hide',
			'<b>Example configuration</b>' +
			'<hr>' +
			w3_third('', 'w3-container',
				w3_divs('w3-margin-bottom',
					w3_input_get('', 'int1', 'example.int1', 'w3_num_cb'),
					w3_input_get('', 'int2', 'example.int2', 'w3_num_cb')
				), '', ''
			)
		)
	);
}
