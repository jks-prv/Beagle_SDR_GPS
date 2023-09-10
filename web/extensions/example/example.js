// Copyright (c) 2017 John Seamons, ZL4VO/KF6VO

// NB: To capitalize the name in the extension menu while using lowercase in program code
// follow the capitalization used below, e.g.
// EXAMPLE_main() _environment_changed() _focus() _blur() _help() _config_html()
// ALSO the .js and .css filenames must be capitalized and the Makefile changed.
// AND capitalize the name of this directory and fix any use of kiwi_load_js_dir()

var example = {
   //ext_name: 'EXAMPLE',
   ext_name: 'example',    // NB: must match example.cpp:example_ext.name
   first_time: true,
   CMD1: 0
};

//function EXAMPLE_main()
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
			if (isDefined(param[1]))
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
//function EXAMPLE_environment_changed(changed)
function example_environment_changed(changed)
{
   if (!changed.resize) return;
	var el = w3_el('id-example-data');
	var left = (window.innerWidth - 1024 - time_display_width()) / 2;
	el.style.left = px(left);
}

//function EXAMPLE_focus()
function example_focus()
{
}

//function EXAMPLE_blur()
function example_blur()
{
   // anything that needs to be done when extension blurred (closed)
}

//function EXAMPLE_help(show)
function example_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'Example help') +
         w3_div('w3-margin-T-8 w3-scroll-y|height:90%',
            w3_div('w3-margin-R-8',
               '...'
            )
         );
      confirmation_show_content(s, 610, 350);
      w3_el('id-confirmation-container').style.height = '100%';   // to get the w3-scroll-y above to work
   }
   return true;
}

// called to display HTML for configuration parameters in admin interface
//function EXAMPLE_config_html()
function example_config_html()
{
   ext_config_html(example, 'example', 'Example', 'Example configuration', '');
   //              +         +          +          +                        +-- admin page, extensions tab: optional page content
   //              +         +          +          +-- admin page, extensions tab: top bar title
   //              +         +          +-- admin page, extensions tab: nav sidebar text
   //              +         +-- cfg prefix, e.g. example.enable
   //              +-- vars struct (above)
}
