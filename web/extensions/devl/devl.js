// Copyright (c) 2016 John Seamons, ZL/KF6VO

var devl_ext_name = 'devl';		// NB: must match devl.c:devl_ext.name

var devl_first_time = true;

function devl_main()
{
	ext_switch_to_client(devl_ext_name, devl_first_time, devl_recv);		// tell server to use us (again)
	if (!devl_first_time)
		devl_controls_setup();
	devl_first_time = false;
}

function devl_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('devl_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('devl_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('devl_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				devl_controls_setup();
				break;

			default:
				console.log('devl_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function devl_num_cb(path, val)
{
   var v;
   if (val.startsWith('0x'))
      v = parseInt(val);
   else
      v = parseFloat(val);
	if (val == '') v = 0;
	if (isNaN(v)) {
	   w3_set_value(path, 0);
	   v = 0;
	}
	console.log('devl_num_cb: path='+ path +' val='+ val +' v='+ v);
	setVarFromString(path, v);
	ext_send('SET '+ path +'='+ v);
}

function devl_controls_setup()
{
	var controls_html =
		w3_div('id-devl-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
				w3_div('w3-medium w3-text-aqua', '<b>Development controls</b>'),
			   w3_input('id-devl-input1||size=10', '', 'devl.in1', devl.in1, 'devl_num_cb'),
			   w3_input('id-devl-input2||size=10', '', 'devl.in2', devl.in2, 'devl_num_cb'),
			   w3_input('id-devl-input3||size=10', '', 'devl.in3', devl.in3, 'devl_num_cb')
			)
		);

	ext_panel_show(controls_html, null, null);
	ext_set_controls_width_height(null, 250);
}
