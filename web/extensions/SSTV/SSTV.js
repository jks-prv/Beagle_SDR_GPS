// Copyright (c) 2019 John Seamons, ZL/KF6VO

var sstv = {
   ext_name: 'SSTV',    // NB: must match example.c:example_ext.name
   first_time: true,
   
   w: 3*(64+320) - 64,
   h: 256,
   iw: 320,
   isp: 64,
   iws: 320+64,
   page: 0,
   pg_sp: 64,
   startx: 22+64,
   tw: 0,
   data_canvas: 0,
   image_y: 0,
   shift_second: false,
   auto: true,
   
   CMD_DRAW: 0,
   CMD_REDRAW: 1,
   img_width: 0,
   
   // https://www.amateur-radio-wiki.net/index.php?title=SSTV_frequencies
   freqs_s: [ 3845, 3730, 7033, 7171, 7040, 14230, 21340, 28680 ]
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
		var snr = ba[1];
		var o = 2;
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
         x = Math.round(sstv.startx + (sstv.page * sstv.iws));
         ct.putImageData(imd, x, sstv.image_y-1, 0,0,sstv.iw,1);
         if (cmd == sstv.CMD_DRAW)
            sstv_status_cb('line '+ sstv.line +', SNR '+ ((snr-128).toFixed(0)) +' dB');
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
				//console.log('img_width='+ sstv.img_width);
				break;

			case "new_img":
			   var mode_name = decodeURIComponent(param[1]);
			   sstv_clear_display(mode_name);
				break;

			case "redraw":
            sstv.image_y = 0;
				break;

			case "mode_name":
			   sstv.mode_name = decodeURIComponent(param[1]);
				//console.log('mode_name='+ sstv.mode_name);
				sstv_mode_name_cb(sstv.mode_name);
				sstv.line = 1;
				break;

			case "status":
			   sstv.status = decodeURIComponent(param[1]);
				//console.log('status='+ sstv.status);
				sstv_status_cb(sstv.status);
				break;

			case "result":
			   sstv.result = decodeURIComponent(param[1]);
				//console.log('result='+ sstv.result);
				sstv_result_cb(sstv.result);
				break;

			case "fsk_id":
			   sstv.fsk_id = decodeURIComponent(param[1]);
				//console.log('fsk_id='+ sstv.fsk_id);
				sstv_fsk_id_cb(sstv.fsk_id);
				break;

			default:
				console.log('sstv_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

function sstv_clear_display(mode_name)
{
   var ct = sstv.data_canvas.ctx;
   var x = sstv.startx + (sstv.page * sstv.iws);
   var y = sstv.h/2;
   var ts = 24;
   var isp = sstv.isp;

   // clear previous marker
   if (sstv.page != -1) {
      ct.fillStyle = 'dimGray';
      ct.fillRect(x-ts,y-ts/2, ts,ts);
   }

   sstv.page = (sstv.page+1) % 3;
   x = sstv.startx + (sstv.page * sstv.iws);

   ct.fillStyle = 'yellow';
   ct.beginPath();
   ct.moveTo(x-ts, y-ts/2);
   ct.lineTo(x, y);
   ct.lineTo(x-ts, y+ts/2);
   ct.closePath();
   ct.fill();
   
   // clear previous text
   ct.fillStyle = 'dimGray';
   var font_sz = 16;
   ct.fillRect(x-isp,y-ts-font_sz, isp,ts);

   ct.font = font_sz +'px Arial';
   ct.fillStyle = 'yellow';
   var tx = x - ct.measureText(mode_name).width -4;
   ct.fillText(mode_name, tx, y - ts);

   ct.fillStyle = 'lightCyan';
   ct.fillRect(x,0, sstv.iw,sstv.h);
   sstv.image_y = 0;
   sstv.shift_second = false;
}

function sstv_controls_setup()
{
   if (kiwi_isMobile()) sstv.startx = 0;
   sstv.tw = sstv.w + sstv.startx;

   var data_html =
      time_display_html('sstv') +

      w3_div('id-sstv-data|left:0; width:'+ px(sstv.tw) +'; background-color:black; position:relative;',
   		'<canvas id="id-sstv-data-canvas" class="w3-crosshair" width='+ dq(sstv.tw)+' style="position:absolute;"></canvas>'
      );

	var controls_html =
		w3_div('id-test-controls w3-text-white',
			w3_divs('w3-container/w3-tspace-8',
            w3_col_percent('',
				   w3_div('w3-medium w3-text-aqua', '<b>SSTV decoder</b>'), 30,
					w3_div('', 'From <b><a href="http://windytan.github.io/slowrx" target="_blank">slowrx</a></b> by Oona Räisänen, OH2EIQ')
				),
				w3_inline('',
               w3_select('id-sstv-freq-menu w3-text-red', '', 'band', 'sstv.band', W3_SELECT_SHOW_TITLE, sstv.freqs_s, 'sstv_band_cb'),
               w3_checkbox('id-sstv-cbox-auto w3-margin-left w3-label-inline w3-label-not-bold', 'auto adjust', 'sstv.auto', true, 'sstv_auto_cbox_cb'),
				   w3_button('id-sstv-btn-auto w3-margin-left w3-padding-smaller', 'Undo adjust', 'sstv_auto_cb'),
				   w3_button('w3-margin-left w3-padding-smaller w3-css-yellow', 'Reset', 'sstv_reset_cb'),
				   w3_button('w3-margin-left w3-padding-smaller w3-aqua', 'Test image', 'sstv_test_cb')
				),
            w3_half('', '',
               w3_div('id-sstv-mode-name'),
               w3_div('id-sstv-fsk-id')
            ),
            w3_div('id-sstv-status'),
            w3_div('id-sstv-result w3-hide')
			)
		);

	ext_panel_show(controls_html, data_html, null);
	ext_set_controls_width_height(560, 125);
	sstv_mode_name_cb("");
	sstv_status_cb("");
	sstv_result_cb("");
	sstv_fsk_id_cb("");
   time_display_setup('sstv');

	sstv.data_canvas = w3_el('id-sstv-data-canvas');
	sstv.data_canvas.ctx = sstv.data_canvas.getContext("2d");
	sstv.data_canvas.imd = sstv.data_canvas.ctx.createImageData(sstv.w, 1);
	sstv.data_canvas.addEventListener("mousedown", sstv_mousedown, false);
	if (kiwi_isMobile())
		sstv.data_canvas.addEventListener('touchstart', sstv_touchstart, false);
   sstv.data_canvas.height = sstv.h.toString();
   ext_set_data_height(sstv.h);

   var ct = sstv.data_canvas.ctx;
   ct.fillStyle = 'black';
   ct.fillRect(0,0, sstv.tw,sstv.h);
   ct.fillStyle = 'dimGray';
   ct.fillRect(sstv.startx-sstv.isp,0, sstv.w+sstv.isp,sstv.h);
   sstv.page = -1;
   sstv.shift_second = false;

	ext_send('SET start');

   var p = ext_param();
   if (p) {
      p = p.split(',');
      var found = false;
      for (var i=0, len = p.length; i < len; i++) {
         var a = p[i];
         //console.log('SSTV: param <'+ a +'>');
         if (a == 'test') {
            sstv_test_cb();
         } else
         if (a == 'noadj') {
            sstv.auto = 1;    // gets inverted by sstv_auto_cbox_cb()
            sstv_auto_cbox_cb('id-sstv-cbox-auto', false);
         } else {
            w3_select_enum('id-sstv-freq-menu', function(option, idx) {
               //console.log('CONSIDER idx='+ idx +' <'+ option.innerHTML +'>');
               if (!found && option.innerHTML.startsWith(a)) {
                  w3_select_value('id-sstv-freq-menu', idx-1);
                  sstv_band_cb('', idx-1, false);
                  found = true;
               }
            });
         }
      }
   }
}

function sstv_auto_cbox_cb(path, checked, first)
{
   if (first) return;
   w3_checkbox_set(path, checked);
   w3_innerHTML('id-sstv-btn-auto', checked? 'Undo adjust' : 'Auto adjust');
   sstv.auto = sstv.auto? false:true;
   ext_send('SET noadj='+ (sstv.auto? 0:1));
}

function sstv_auto_cb(path, val, first)
{
   //console.log('SET '+ (sstv.auto? 'undo':'auto'));
   ext_send('SET '+ (sstv.auto? 'undo':'auto'));
}

function sstv_band_cb(path, idx, first)
{
   if (first) return;
   idx = +idx;
   var f = parseInt(sstv.freqs_s[idx]);
   //console.log('SSTV f='+ f);
   if (isNaN(f)) return;
   var lsb = (f < 10000);
   ext_tune(f, lsb? 'lsb':'usb', ext_zoom.ABS, 9);
   var margin = 200;
   var lo = lsb? -2300 : 1200;
   var hi = lsb? -1200 : 2300;
   ext_set_passband(lo - margin, hi + margin);
}

function sstv_mousedown(evt)
{
   sstv_shift(evt, true);
}

function sstv_touchstart(evt)
{
   sstv_shift(evt, false);
}

function sstv_shift(evt, requireShiftKey)
{
	var x = (evt.clientX? evt.clientX : (evt.offsetX? evt.offsetX : evt.layerX));
	var y = (evt.clientY? evt.clientY : (evt.offsetY? evt.offsetY : evt.layerY));
	x -= sstv.startx;
	//console.log('sstv_shift xy '+ x +' '+ y);
	if (x < 0 || sstv.page == -1) { sstv.shift_second = false; return; }
	var xmin = sstv.page * sstv.iws;
	var xmax = xmin + sstv.iw;
	if (x < xmin || x > xmax) { sstv.shift_second = false; return; }
	x -= xmin;
	if (!sstv.shift_second) {
	   sstv.x0 = x; sstv.y0 = y;
	   sstv.shift_second = true;
	   return;
	}
   sstv.shift_second = false;
	//console.log('sstv_shift page='+ sstv.page +' xy '+ sstv.x0 +','+ sstv.y0 +' -> '+ x +','+ y);
	ext_send('SET shift x0='+ sstv.x0 +' y0='+ sstv.y0 +' x1='+ x +' y1='+ y);
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

function sstv_reset_cb(path, val, first)
{
	sstv_mode_name_cb("");
	sstv_status_cb("");
	sstv_result_cb("");
	sstv_fsk_id_cb("");
	ext_send('SET reset');
}

function sstv_test_cb(path, val, first)
{
   // mode_name & status fields set in cpp code
	sstv_result_cb("");
	sstv_fsk_id_cb("");
	ext_send('SET test');
}

function SSTV_blur()
{
	//console.log('### SSTV_blur');
	ext_send('SET stop');
}

function SSTV_help(show)
{
   if (show) {
      var s = 
         w3_text('w3-medium w3-bold w3-text-aqua', 'SSTV decoder help') + '<br><br>' +
         'Select an entry from the SSTV band menu and wait for a signal to begin decoding.<br>' +
         'Sometimes activity is +/- the given frequencies. Try the "test image" button.<br>' +
         '<br>Supported modes:' +
         '<ul>' +
            '<li>Martin: M1 M2 M3 M4</li>' +
            '<li>Scottie: S1 S2 SDX</li>' +
            '<li>Robot: R72 R36 R24 R24-BW R12-BW R8-BW</li>' +
            '<li>Wraase: SC-2-120 SC-2-180</li>' +
            '<li>PD: PD-50 PD-90</li>' +
         '</ul>' +
         'If the image is still slanted or offset after auto adjustment you can make a manual<br>' +
         'correction. If you see what looks like an edge in the image then click in two places along<br>' +
         'the edge. The image will then auto adjust. You can repeat this procedure multiple times<br>' +
         'if necessary.<br>' +
         '';
      confirmation_show_content(s, 610, 320);
   }
   return true;
}


// called to display HTML for configuration parameters in admin interface
function SSTV_config_html()
{
	ext_admin_config(sstv.ext_name, 'SSTV',
		w3_div('id-SSTV w3-text-teal w3-hide',
			'<b>SSTV decoder configuration</b>' +
			'<hr>' +
			''
			/*
			w3_third('', 'w3-container',
				w3_divs('w3-margin-bottom',
					w3_input_get('', 'int1', 'sstv.int1', 'w3_num_cb'),
					w3_input_get('', 'int2', 'sstv.int2', 'w3_num_cb')
				), '', ''
			)
			*/
		)
	);
}
