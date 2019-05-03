// Copyright (c) 2017 John Seamons, ZL/KF6VO

var colormap_ext_name = 'colormap';		// NB: must match colormap.c:colormap_ext.name

var colormap_first_time = true;

function colormap_main()
{
	ext_switch_to_client(colormap_ext_name, colormap_first_time, colormap_recv);		// tell server to use us (again)
	if (!colormap_first_time)
		colormap_controls_setup();
	colormap_first_time = false;
}

function colormap_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

		console.log('colormap_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('colormap_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('colormap_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				colormap_controls_setup();
				break;

			default:
				console.log('colormap_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function colormap_plot()
{
	var c = colormap.canvas.ctx;
	c.fillStyle = 'mediumBlue';
	c.fillRect(0, 0, 256, 256);
	
	for (var y = 0; y < 255; y++) {
	   var yn = y/255;
	   var yi = 1-yn;
	   var x;
	   var xn = yi;
	   //var emod = 1/7 * Math.exp(colormap.ef * (-2 + 4*yn));
	   //var emod = Math.exp(colormap.ef * (-2 + 4*yn));
	   var emod = Math.exp(colormap.ef * yi);
	   x = emod*y;
	   if (x < 0) x = 0;
	   if (x > 255) x = 255;
	   c.fillStyle = 'white';
	   //c.fillRect(x, 255-y, 1, 1);

      x = y + y * (1 - Math.exp(colormap.ef * xn));
      if (y==200) console.log('xn='+ xn +' eff='+ (colormap.ef * xn) +'|'+ Math.exp(colormap.ef * xn) +' x='+ x);
	   if (x < 0) x = 0;
	   if (x > 255) x = 255;
	   c.fillStyle = 'lime';
	   c.fillRect(x, 255-y, 1, 1);

	   c.fillStyle = 'yellow';
	   c.fillRect(y, 255-y, 1, 1);
	}
}

var colormap = {
   canvas: 0,
   imageData: 0,
   ef: 0
};

function colormap_controls_setup()
{
   var cmap =
      w3_div('id-colormap-data|left:0px; width:256px; height:256px; background-color:mediumBlue; overflow:hidden; position:relative;',
   		'<canvas id="id-colormap-canvas" width="256" height="256" style="position:absolute"></canvas>'
      );

	var controls_html =
		w3_div('id-colormap-controls w3-text-white',
         w3_half('', '',
            cmap,
				w3_divs('w3-container/w3-tspace-8',
               w3_div('w3-medium w3-text-aqua', '<b>Colormap definition</b>'),
					w3_slider('', 'Exp', 'colormap.ef', colormap.ef, -10, 10, 0.1, 'colormap_exp_cb'),
               w3_input('id-colormap-input1||size=10')
			   )
			)
		);

	ext_panel_show(controls_html, null, null);
	ext_set_controls_width_height(null, 330);

	colormap.canvas = w3_el('id-colormap-canvas');
	colormap.canvas.ctx = colormap.canvas.getContext("2d");
	colormap.canvas.imageData = colormap.canvas.ctx.createImageData(256, 1);
	colormap_plot();
}

function colormap_exp_cb(path, val)
{
   val = +val;
	w3_num_cb(path, val);
	w3_set_label('Exp '+ val.toFixed(2), path);
	colormap.ef = val;
	colormap_plot();
}
