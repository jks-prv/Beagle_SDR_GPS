// Copyright (c) 2019 John Seamons, ZL/KF6VO

var sstv = {
   ext_name: 'SSTV',    // NB: must match example.c:example_ext.name
   first_time: true,
   
   w: 1024,
   h: 256,
   page: 0,
   startx: 150,
   tw: 0,
   data_canvas: 0,
   image_y: 0,
   
   CMD_DRAW: 0,
   CMD_REDRAW: 1,
   img_width: 0
};

function SSTV_main()
{
	ext_switch_to_client(sstv.ext_name, sstv.first_time, sstv_recv);		// tell server to use us (again)
	if (!sstv.first_time)
		sstv_controls_setup();
	sstv.first_time = false;
}

function sstv_recv(data)
{
   var canvas = sstv.data_canvas;
   var ct = canvas.ctx;
	var firstChars = arrayBufferToStringLen(data, 3);
	
	// process data sent from server/C by ext_send_msg_data()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];
		var o = 1;
		var len = ba.length-1;

      if (cmd == sstv.CMD_DRAW || cmd == sstv.CMD_REDRAW) {
         var imd = canvas.imd;
         for (var i = 0; i < sstv.img_width; i++) {
            imd.data[i*4+0] = ba[o];
            imd.data[i*4+1] = ba[o+1];
            imd.data[i*4+2] = ba[o+2];
            imd.data[i*4+3] = 0xff;
            o += 3;
         }
         var x;
         if (sstv.image_y < sstv.h) {
            sstv.image_y++;
         } else {
            x = sstv.startx;
            var w = sstv.w;
            ct.drawImage(canvas, x,1,w,sstv.h-1, x,0,w,sstv.h-1);   // scroll up
         }
         x = sstv.startx + (sstv.page * (320+32));
         ct.putImageData(imd, x, sstv.image_y-1, 0,0,320,1);
         if (cmd == sstv.CMD_DRAW)
            sstv_status_cb('line '+ sstv.line);
         //console.log('line '+ sstv.line);
         sstv.line++;
      } else
		   console.log('sstv_recv: DATA UNKNOWN cmd='+ cmd +' len='+ len);
		
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('sstv_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('sstv_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				sstv_controls_setup();
				break;

			case "img_width":
				sstv.img_width = param[1];
				console.log('img_width='+ sstv.img_width);
				break;

			case "new_img":
			   sstv_clear_display();
				break;

			case "redraw":
            sstv.image_y = 0;
				break;

			case "mode_name":
			   sstv.mode_name = decodeURIComponent(param[1]);
				console.log('mode_name='+ sstv.mode_name);
				sstv_mode_name_cb(sstv.mode_name);
				sstv.line = 1;
				break;

			case "status":
			   sstv.status = decodeURIComponent(param[1]);
				console.log('status='+ sstv.status);
				sstv_status_cb(sstv.status);
				break;

			case "result":
			   sstv.result = decodeURIComponent(param[1]);
				console.log('result='+ sstv.result);
				sstv_result_cb(sstv.result);
				break;

			case "fsk_id":
			   sstv.fsk_id = decodeURIComponent(param[1]);
				console.log('fsk_id='+ sstv.fsk_id);
				sstv_fsk_id_cb(sstv.fsk_id);
				break;

			default:
				console.log('sstv_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function sstv_clear_display()
{
   sstv.page = (sstv.page+1) % 3;
   var ct = sstv.data_canvas.ctx;
   ct.fillStyle = 'lightCyan';
   var x = sstv.page * (320+32);
   ct.fillRect(sstv.startx+x,0, 320,sstv.h);
   sstv.image_y = 0;
}

function sstv_controls_setup()
{
   if (kiwi_isMobile()) sstv.startx = 0;
   sstv.tw = sstv.w + sstv.startx;

   var data_html =
      time_display_html('sstv') +

      w3_div('id-sstv-data|left:0; width:'+ px(sstv.tw) +'; background-color:black; position:relative;',
   		'<canvas id="id-sstv-data-canvas" width='+ dq(sstv.tw)+' style="position:absolute;"></canvas>'
      );

	var controls_html =
		w3_div('id-test-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
            w3_col_percent('',
				   w3_div('w3-medium w3-text-aqua', '<b>SSTV decoder</b>'), 30,
					w3_div('', 'From <b><a href="http://windytan.github.io/slowrx" target="_blank">slowrx</a></b> by Oona Räisänen, OH2EIQ')
				),
				w3_inline('',
				   w3_text('', 'Alpha test. Many known bugs. No UI features yet.'),
				   w3_button('w3-margin-left w3-padding-small w3-css-yellow', 'Test image', 'sstv_test_cb')
				),
				//w3_text('w3-red', 'WARNING: Do not use with WSPR extension. Disrupts WSPR decoding process.'),
            w3_div('id-sstv-mode-name'),
            w3_div('id-sstv-status'),
            w3_div('id-sstv-result w3-hide'),
            w3_div('id-sstv-fsk-id')
			)
		);

	ext_panel_show(controls_html, data_html, null);
	ext_set_controls_width_height(560, 150);
	sstv_mode_name_cb("");
	sstv_status_cb("");
	sstv_result_cb("");
	sstv_fsk_id_cb("");
   time_display_setup('sstv');

	sstv.data_canvas = w3_el('id-sstv-data-canvas');
	sstv.data_canvas.ctx = sstv.data_canvas.getContext("2d");
	sstv.data_canvas.imd = sstv.data_canvas.ctx.createImageData(sstv.w, 1);
   sstv.data_canvas.height = sstv.h.toString();
   ext_set_data_height(sstv.h);

   var ct = sstv.data_canvas.ctx;
   ct.fillStyle = 'grey';
   ct.fillRect(sstv.startx,0, sstv.w,sstv.h);
   sstv.page = -1;

	ext_send('SET start');
}

function sstv_mode_name_cb(mode_name)
{
	w3_el('id-sstv-mode-name').innerHTML = 'Mode: ' +
	   w3_div('w3-show-inline-block w3-text-black w3-background-pale-aqua w3-padding-LR-8', mode_name);
}

function sstv_status_cb(status)
{
	w3_el('id-sstv-status').innerHTML = 'Status: ' +
	   w3_div('w3-show-inline-block w3-text-black w3-background-pale-aqua w3-padding-LR-8', status);
}

function sstv_result_cb(result)
{
	w3_el('id-sstv-result').innerHTML = 'Result: ' +
	   w3_div('w3-show-inline-block w3-text-black w3-background-pale-aqua w3-padding-LR-8', result);
}

function sstv_fsk_id_cb(fsk_id)
{
	w3_el('id-sstv-fsk-id').innerHTML = 'FSK ID: ' +
	   w3_div('w3-show-inline-block w3-text-black w3-background-pale-aqua w3-padding-LR-8', fsk_id);
}

function sstv_test_cb(path, val, first)
{
	sstv_mode_name_cb("");
	sstv_status_cb("");
	sstv_result_cb("");
	sstv_fsk_id_cb("");
	ext_send('SET test');
}

function SSTV_blur()
{
	//console.log('### SSTV_blur');
	ext_send('SET stop');
}
