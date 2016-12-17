// KiwiSDR data demodulator UI
//
// Copyright (c) 2014 John Seamons, ZL/KF6VO

// TODO
// sanity check upload data
// keep demo overrun from crashing server (still a problem?)
// final wsprstat upload interval

// demo doesn't always decode same way!!!
//		implies uninitialized C variables?
// prefix all global vars with 'wspr_'
// ntype non-std 29 seen
// decode task shows > 100% in "-stats" task display
// update to WSJT-X merged version
// see screenshot: case where double peaks of same station, same freq didn't get filtered out?
// re-enable spot uploads

var wspr_ext_name = 'wspr';		// NB: must match wspr.c:wspr_ext.name

var wspr_canvas_width = 1024;
//var wspr_canvas_height = 150;		// not currently used

var wspr_first_time = true;

function wspr_main()
{
	ext_switch_to_client(wspr_ext_name, wspr_first_time, wspr_recv);		// tell server to use us (again)
	if (!wspr_first_time)
		wspr_controls_setup();
	wspr_first_time = false;
}

var wspr_cmd_e = { WSPR_DATA:0 };
var wspr_cmd_d = { 0:"WSPR_DATA" };

var wspr_bins, wspr_startx;
var aw_ypos = 0, y_tstamp = -1, span = 0, over = 16;

function wspr_scroll()
{
	var aw_h = wccva.height;
	wccva.style.top=(aw_h-aw_ypos)+"px";
	wccvao.style.top=(-aw_ypos)+"px";
	aw_ypos++; if (aw_ypos>=aw_h) {
		aw_ypos=0;
		var tmp=wccvao; wccvao=wccva; wccva=tmp;
	}
}

var WSPR_F_BIN =			0x0fff;
var WSPR_F_DECODING =	0x1000;
var WSPR_F_DELETE =		0x2000;

function wspr_recv(data)
{
	var firstChars = getFirstChars(data, 3);
	
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];

		if (cmd == wspr_cmd_e.WSPR_DATA) {
			var o = 2;
			var blen = ba.length-o;
			var aw_h = wccva.height;
         
         if (ba[1]) {		// 2 min interval line
         	y_tstamp = aw_ypos+12;
            wccva.ct.fillStyle="white";
            wccva.ct.fillRect(wspr_startx-over, aw_ypos, blen*2+over*2, 1);
            wspr_scroll();
         }
         
         // FFT data
			var im = wccva.im;
			for (i=0; i < blen; i++) {
				var z = ba[o+i];
				if (z>255) z=255; if (z<0) z=0;
				for (j=0; j<2; j++) {
					var k = 4*(i*2+j+wspr_startx);
					im.data[k]=color_map_r[z];
					im.data[k+1]=color_map_g[z];
					im.data[k+2]=color_map_b[z];
					im.data[k+3]=255;
				}
			}
			wccva.ct.putImageData(im,0,aw_ypos);
			
			// handle context spanning of timestamp
			if ((y_tstamp >= aw_h) && (aw_ypos == (aw_h-1))) {
				span = 1;
			}
			//console.log('aw_ypos='+ aw_ypos +' aw_h='+ aw_h +' span='+ span +' y_tstamp='+ y_tstamp);
			
			if ((y_tstamp == aw_ypos) || span) {
				wccva.ct.fillStyle="white";
				wccva.ct.font="10px Verdana";
				var d = new Date(wspr_server_time_ms + (Date.now() - wspr_local_time_epoch_ms));
				wccva.ct.fillText(d.toUTCString().substr(17,5) +' UTC', wspr_startx+blen*2+over, y_tstamp);
				if (span) {
					y_tstamp -= aw_h;
					span = 0;
				} else {
					y_tstamp = -1;
				}
			}
         
         wspr_scroll();
		} else {
			console.log('wspr_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
		}
		return;
	}
	
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (0 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('wspr_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('wspr_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				var bfo = parseInt(ext_get_cfg_param('WSPR.BFO', wspr_bfo));
				//console.log('### bfo='+ bfo);
				if (!isNaN(bfo)) {
					//console.log('### set bfo='+ bfo);
					wspr_bfo = bfo;
				}
				ext_send('SET BFO='+ wspr_bfo.toFixed(0));
				wspr_controls_setup();		// needs wspr_startx
				break;

			case "WSPR_TIME":
				wspr_server_time_ms = param[1] * 1000;
				wspr_local_time_epoch_ms = Date.now();
				break;

			case "WSPR_SYNC":
				//console.log('WSPR: WSPR_SYNC');
				break;

			case "WSPR_STATUS":
				var status = parseInt(param[1]);
				//console.log('WSPR: WSPR_STATUS='+ status);
				wspr_set_status(status);
				break;

			case "WSPR_DECODED":
				var s = decodeURIComponent(param[1]);
				//console.log('WSPR: '+ s);
				var o = html('id-wspr-decode');
				o.innerHTML += s +'<br>';
				o.scrollTop = o.scrollHeight;
				wspr_upload(wspr_report_e.SPOT, s);
				break;
			
			case "WSPR_PEAKS":
				var s = decodeURIComponent(param[1]);
				var p = s.split(':');
				var xscale = 2;
				var npk = (p.length-1)/2;
				//console.log('WSPR: '+ npk +' '+ s);

				for (var i=0; i < npk; i++) {
					var bin0 = parseInt(p[i*2]);
					var flags = bin0 & ~WSPR_F_BIN;
					bin0 &= WSPR_F_BIN;
					if (flags & WSPR_F_DELETE) continue;
					var x = wspr_startx + bin0*xscale;
					if (x > wspr_canvas_width) break;
					var nextx;
					if (i < npk-1)
						nextx = wspr_startx + parseInt(p[(i+1)*2])*xscale;
					else
						nextx = wspr_canvas_width;
					if (nextx >= wspr_canvas_width)
						nextx = wspr_canvas_width + 256;
					var snr_call = p[i*2+1];
					var snr = parseInt(snr_call);
					var color;
					if (isNaN(snr)) {
						color = 'cl-wspr-call';
					} else {
						snr_call = snr.toFixed(0)+' dB';
						color = (flags & WSPR_F_DECODING)? 'cl-wspr-decoding':'';
					}
					s +=
						'<div class="cl-wspr-mkr1" style="max-width:'+ (nextx-x) +'px; left:'+ (x-6) +'px; bottom:8px" title="">' +
							'<div class="cl-wspr-mkr2">' +
								'<div class="cl-wspr-snr '+ color +'">'+ snr_call +'<\/div>' +
							'<\/div>' +
						'<\/div>' +
						'<div class="cl-wspr-line '+ color +'" style="width:1px; height:10px; position:absolute; left:'+ x +'px; bottom:0px" title=""><\/div>';
				}
				html('id-wspr-peaks-labels').innerHTML = s;
				break;

			case "nbins":
				wspr_bins = parseInt(param[1]);
				
				// starting x position given that wspr display is centered in canvas
				// typically (1024 - 411*4)/2 = 101
				// remember that wspr_canvas is scaled to fit screen width
				wspr_startx = Math.round((wspr_canvas_width - wspr_bins*2)/2);
				break;

			default:
				console.log('wspr_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var wspr_report_e = { STATUS:0, SPOT:1 };

var pie_size = 25;
var mycall = "";
var mygrid = "";

var wspr_spectrum_A, wspr_spectrum_B, wspr_scale_canvas;
var wccva, wccva0;

var wspr_config_okay = true;

function wspr_controls_setup()
{
	wspr_fbn = 0;
	
   var data_html =
      '<div id="id-wspr-peaks" class="scale" style="width:1024px; height:30px; background-color:black; position:relative; display:none" title="WSPR">'+
      	'<div id="id-wspr-peaks-labels" style="width:1024px; height:30px; position:absolute"></div>'+
      '</div>' +

   	'<div id="id-wspr-spectrum" style="width:1024px; height:150px; overflow:hidden; position:relative; display:none">'+
			// two overlapping canvases to implement scrolling
   		'<canvas id="id-wspr-spectrum-A" width="1024" height="150" style="position:absolute">test</canvas>'+
   		'<canvas id="id-wspr-spectrum-B" width="1024" height="150" style="position:absolute">test</canvas>'+
   	'</div>' +
   	
      '<div id="id-wspr-scale" class="scale" style="width:1024px; height:20px; background-color:black; position:relative; display:none" title="WSPR">'+
   		'<canvas id="id-wspr-scale-canvas" width="1024" height="20" style="position:absolute"></canvas>'+
      '</div>';
   
   var call = ext_get_cfg_param_string('WSPR.callsign', '');
   if (call == undefined || call == null || call == '') {
   	call = '(not set)';
   	wspr_config_okay = false;
   }
   var grid = ext_get_cfg_param_string('WSPR.grid', '');
   if (grid == undefined || grid == null || grid == '') {
   	grid = '(not set)';
   	wspr_config_okay = false;
   }

	var controls_html =
	"<div id='id-wspr-controls' style='color:black; width:auto; display:block'>"+
		w3_col_percent('', '',
			'<table>' +
				'<tr>' +
					wspr_freq_button('LF')+
					wspr_freq_button('MF')+
					wspr_freq_button('160m')+
					wspr_freq_button('80m')+
					wspr_freq_button('60m')+
					wspr_freq_button('40m')+
				'</tr>' +
				'<tr>' +
					wspr_freq_button('30m')+
					wspr_freq_button('20m')+
					wspr_freq_button('17m')+
					wspr_freq_button('15m')+
					wspr_freq_button('12m')+
					wspr_freq_button('10m')+
				'</tr>' +
			'</table>' +
			
			'<table>' +
				'<tr>' +
					'<td>'+ kiwi_button('stop', 'wspr_reset();') +'</td>' +
					'<td>'+ kiwi_button('clear', 'wspr_clear();') +'</td>' +
					wspr_freq_button('demo') +
					'<td colspan="2">' +
						w3_divs('', 'id-wspr-upload-bkg cl-upload-checkbox',
							'<input id="id-wspr-upload" type="checkbox" value="" onclick="wspr_set_upload(this.checked)"> upload spots'
						) +
					'</td>' +
					'<td>'+ w3_divs('', 'w3-margin-left w3-medium w3-text-aqua w3-center cl-viewer-label', '<b>WSPR viewer</b>') +'</td>' +
					//'<td></td>' +
				'</tr>' +
			'</table>', 95
		) +

		'<table>' +
			'<tr>' +
				'<td style="width:10%">' +
					'<div class="cl-wspr-pie" style="float:left; margin-top:5px; background-color:#575757">' +
						kiwi_pie('id-wspr-pie', pie_size, '#eeeeee', 'deepSkyBlue') +
					'</div>' +
				'</td>' +
		
				'<td>' +
					w3_divs('', '',
						w3_divs('id-wspr-time cl-wspr-text', '', ''),
						w3_divs('id-wspr-status cl-wspr-text', '', '')
					) +
				'</td>' +

				// FIXME: field validation
				'<td>' +
					w3_divs('', '',
						w3_divs('cl-wspr-text', '', 'BFO '+ wspr_bfo),
						w3_divs('id-wspr-cf cl-wspr-text', ' ')
					) +
				'</td>' +

				'<td>' +
					w3_divs('cl-wspr-text', '', 'reporter call '+ call) +
				'</td>' +

				'<td>' +
					w3_divs('cl-wspr-text', '', 'reporter grid '+ grid) +
				'</td>' +
			'</tr>' +
		'</table>' +

		'<div style="background-color:lightGray; overflow:auto; width:100%; margin-top:5px; margin-bottom:0px; font-family:monospace; font-size:100%">'+
			'<pre style="display:inline"> UTC  dB   dT      Freq dF  Call        dBm</pre>'+
		'</div>'+
		'<div id="id-wspr-decode" style="white-space:pre; background-color:#eeeeee; overflow:scroll; height:100px; width:100%; margin-top:0px; font-family:monospace; font-size:100%"></div>'+
	"</div>";

	ext_panel_show(controls_html, data_html, null);

	wspr_spectrum_A = w3_el_id('id-wspr-spectrum-A');
	wspr_spectrum_A.ct = wspr_spectrum_A.getContext("2d");
	wspr_spectrum_A.im = wspr_spectrum_A.ct.createImageData(1024, 1);

	wspr_spectrum_B = w3_el_id('id-wspr-spectrum-B');
	wspr_spectrum_B.ct = wspr_spectrum_B.getContext("2d");
	wspr_spectrum_B.im = wspr_spectrum_B.ct.createImageData(1024, 1);
	
	wccva = wspr_spectrum_A; wccvao = wspr_spectrum_B;

	wspr_scale_canvas = w3_el_id('id-wspr-scale-canvas');
	wspr_scale_canvas.ct = wspr_scale_canvas.getContext("2d");

	wspr_visible(1);
}

function wspr_blur()
{
	//console.log('### wspr_blur');
	ext_send('SET capture=0 demo=0');
	wspr_visible(0);
}

function wspr_config_html()
{
	ext_admin_config(wspr_ext_name, 'WSPR',
		w3_divs('id-wspr w3-text-teal w3-hide', '',
			'<b>WSPR configuration</b>' +
			'<hr>' +
			w3_half('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					w3_input_get_param('BFO Hz (multiple of 375 Hz, i.e. 375, 750, 1125, 1500)', 'WSPR.BFO', 'w3_num_set_cfg_cb', '', 'typically 750 Hz'),
					w3_input_get_param('Reporter callsign', 'WSPR.callsign', 'w3_string_set_cfg_cb', ''),
					w3_input_get_param('Reporter grid square', 'WSPR.grid', 'w3_string_set_cfg_cb', '', '4 or 6-character grid square location')
				), ''
			)
		)
	);
}

function wspr_reset()
{
	//console.log('### wspr_reset');
	wspr_demo = 0;
	ext_send('SET capture=0 demo=0');
	wspr_set_status(wspr_status.IDLE);
	
	wspr_set_upload(wspr_config_okay);		// by default allow uploads unless manually unchecked
}

function wspr_clear()
{
	//console.log('### wspr_clear');
	wspr_reset();
	html('id-wspr-decode').innerHTML = '';
	html('id-wspr-peaks-labels').innerHTML = '';
}

var wspr_upload_timeout, wspr_pie_interval;

function wspr_visible(v)
{
	//visible_block('id-wspr-controls', v);
	visible_block('id-wspr-peaks', v);
	visible_block('id-wspr-spectrum', v);
	visible_block('id-wspr-scale', v);

	if (v) {
		wspr_pie_interval = setInterval('wspr_draw_pie()', 1000);
		wspr_draw_pie();
   	wspr_draw_scale(100);
		wspr_reset();
		wspr_upload_timeout = setTimeout('wspr_upload(wspr_report_e.STATUS)', 1000);
	} else {
   	kiwi_clearTimeout(wspr_upload_timeout);
   	kiwi_clearInterval(wspr_pie_interval);
	}
}

function wspr_draw_scale(cf)
{
	wspr_scale_canvas.ct.fillStyle="black";
	wspr_scale_canvas.ct.fillRect(0, 0, wspr_scale_canvas.width, wspr_scale_canvas.height);

	var y = 2;
	wspr_scale_canvas.ct.fillStyle="lime";
	var start = Math.round(50*(512.0/375.0));
	var width = Math.round(200*(512.0/375.0));
	wspr_scale_canvas.ct.fillRect(wspr_startx+start*2, y, width*2, 3);

	wspr_scale_canvas.ct.fillStyle="red";
	start = Math.round(150*(512.0/375.0));
	wspr_scale_canvas.ct.fillRect(wspr_startx + start*2-2, y, 5, 3);
	
	wspr_scale_canvas.ct.fillStyle="white";
	wspr_scale_canvas.ct.font="10px Verdana";
	var f;
	for (f=-150; f<=150; f+=50) {
		var bin = Math.round((f+150)*(512.0/375.0));
		var tf = f + cf;
		if (tf < 0) tf += 1000;
		var tcw = (tf < 10)? 4 : ((tf < 100)? 7:11);
		wspr_scale_canvas.ct.fillText(tf.toFixed(0), wspr_startx + (bin*2)-tcw, y+15);
	}
}

function wspr_set_upload(upload)
{
	// remove old cookie use
	deleteCookie('wspr_upload');
	
	html('id-wspr-upload').checked = upload;
	html('id-wspr-upload-bkg').style.color = upload? "white":"black";
	html('id-wspr-upload-bkg').style.backgroundColor = upload? "inherit":"yellow";
}

// from WSPR-X via tcpdump: (how can 'rcall' have an un-%-escaped '/'?)
// GET /post?function=wsprstat&rcall=ZL/KF6VO&rgrid=RF82ci&rqrg=14.097100&tpct=0&tqrg=14.097100&dbm=0&version=0.8_r3058 HTTP/1.1
// GET /post?function=wspr&rcall=ZL/KF6VO&rgrid=RF82ci&rqrg=14.097100&date=140818&time=0808&sig=-25&dt=-0.2&drift=-1&tqrg=14.097018&tcall=VK6PG&tgrid=OF78&dbm=33&version=0.8_r3058 HTTP/1.1

function wspr_upload(type, s)
{
	var spot = (type == wspr_report_e.SPOT)? 1:0;
	var rcall = ext_get_cfg_param_string('WSPR.callsign', '');
	var rgrid = ext_get_cfg_param_string('WSPR.grid', '');
	//console.log('rcall=<'+ rcall +'> rgrid=<'+ rgrid +'>');
	var valid = wspr_rfreq && wspr_tfreq && (rcall != undefined) && (rgrid != undefined) && (rcall != null) && (rgrid != null) && (rcall != '') && (rgrid != '');

	// don't even report status if not uploading
	if (!valid || (html('id-wspr-upload').checked == false)) {
		wspr_upload_timeout = setTimeout('wspr_upload(wspr_report_e.STATUS)', 1*60*1000);	// check again in another minute
		return;
	}
	
	var decode;
	if (spot) {
		decode = s.replace(/[\s]+/g, ' ').split(' ');		// remove multiple spaces before split()
	}
	
	var tqrg, dbm;
	
	var url = "http://wsprnet.org/post";
	//var url = "http://example.com/post";
	var request = kiwi_GETrequest(spot? "spot":"stat", url);
	kiwi_GETrequest_param(request, "function", spot? "wspr":"wsprstat");
	kiwi_GETrequest_param(request, "rcall", rcall);
	kiwi_GETrequest_param(request, "rgrid", rgrid);
	kiwi_GETrequest_param(request, "rqrg", wspr_rfreq.toFixed(6));
	
	if (spot) {
		var d = new Date();
		kiwi_GETrequest_param(request, "date",
			d.getUTCFullYear().toString().substr(2,2)+(d.getUTCMonth()+1).leadingZeros(2)+d.getUTCDate().leadingZeros(2));
		kiwi_GETrequest_param(request, "time", decode[0]);
		kiwi_GETrequest_param(request, "sig", decode[1]);
		kiwi_GETrequest_param(request, "dt", decode[2]);
		kiwi_GETrequest_param(request, "drift", decode[4]);
		tqrg = decode[3];
		dbm = decode[7];
	} else {
		kiwi_GETrequest_param(request, "tpct", "0");
		tqrg = wspr_tfreq.toFixed(6);
		dbm = "0";
	}
	
	kiwi_GETrequest_param(request, "tqrg", tqrg);

	if (spot) {
		kiwi_GETrequest_param(request, "tcall", decode[5]);
		kiwi_GETrequest_param(request, "tgrid", decode[6]);
	}
	
	kiwi_GETrequest_param(request, "dbm", dbm);

	var version = "1.0 Kiwi";
	if (version.length <= 10) {
		kiwi_GETrequest_param(request, "version", version);
		kiwi_GETrequest_submit(request, false);
		//jksd show how many stat updates there have been
		var now = new Date();
		console.log('WSPR STAT '+ now.toUTCString());
	}

	// report status every six minutes
	if (!spot) wspr_upload_timeout = setTimeout('wspr_upload(wspr_report_e.STATUS)', 6*60*1000);
}

var wspr_cur_status = 0;
var wspr_status = { 'NONE':0, 'IDLE':1, 'SYNC':2, 'RUNNING':3, 'DECODING':4 };
var wspr_status_text = { 0:'none', 1:'idle', 2:'sync', 3:'running', 4:'decoding' };
var wspr_status_color = { 0:'white', 1:'lightSkyBlue', 2:'violet', 3:'cyan', 4:'lime' };

function wspr_set_status(status)
{
	if (wspr_demo && wspr_cur_status == wspr_status.DECODING && status == wspr_status.IDLE) {
		wspr_reset();
	}
	
	var el = html('id-wspr-status');
	el.innerHTML = wspr_status_text[status];
	el.style.backgroundColor = wspr_status_color[status];
	
	wspr_cur_status = status;
}

var wspr_server_time_ms = 0, wspr_local_time_epoch_ms = 0;

function wspr_draw_pie() {
	var d = new Date(wspr_server_time_ms + (Date.now() - wspr_local_time_epoch_ms));
	html('id-wspr-time').innerHTML = d.toUTCString().substr(17,8) +' UTC';
   var wspr_secs = (d.getUTCMinutes()&1)*60 + d.getUTCSeconds() + 1;
   kiwi_draw_pie('id-wspr-pie', pie_size, wspr_secs / 120);
};

// order matches button instantiation order ('demo' is last)
// for BFO=1500: [ 136, 474.2, 1836.6, 3592.6, 5287.2, 7038.6, 10138.7, 14095.6, 18104.6, 21094.6, 24924.6, 28124.6, 0 ];
var wspr_center_freqs = [ 137.5, 475.7, 1838.1, 3594.1, 5288.7, 7040.1, 10140.2, 14097.1, 18106.1, 21096.1, 24926.1, 28126.1, 0 ];
var wspr_rfreq=0, wspr_tfreq=0;
var wspr_bfo = 750;
var wspr_filter_bw = 300;

var wspr_demo = 0;
var wspr_last_freq = -1;

function wspr_freq(b)
{
	var cf = wspr_center_freqs[b];
	var mode = 1;
	
	if (cf == 0) {		// demo mode
		cf = 14097.1;
		wspr_demo = 1;
		wspr_set_upload(false);		// don't upload demo decodes!
	} else {
		wspr_demo = 0;
		wspr_reset();
	}
	
	if (wspr_last_freq >= 0)
		w3_el_id('id-wspr-freq-'+ wspr_last_freq).style.backgroundColor = 'white';
	w3_el_id('id-wspr-freq-'+ b).style.backgroundColor = 'lime';
	wspr_last_freq = b;

	w3_el_id('id-wspr-cf').innerHTML = 'CF '+ cf.toFixed(1);
	var cfo = Math.round((cf - Math.floor(cf)) * 1000);
	wspr_rfreq = wspr_tfreq = cf/1000;
	var dial_freq = cf - wspr_bfo/1000;
	ext_tune(dial_freq, 'usb', ext_zoom.MAX_IN);
	ext_set_passband(wspr_bfo-wspr_filter_bw/2, wspr_bfo+wspr_filter_bw/2);
	ext_send('SET dialfreq='+ dial_freq.toFixed(2));
	ext_send('SET capture=1 demo='+ wspr_demo);
   wspr_draw_scale(cfo);
   
   // promptly notify band change
   kiwi_clearTimeout(wspr_upload_timeout);
   wspr_upload_timeout = setTimeout('wspr_upload(wspr_report_e.STATUS)', 1000);
}

var wspr_fbn = 0;

function wspr_freq_button(v)
{
	var s = '<td>'+ kiwi_button(v, 'wspr_freq('+wspr_fbn+');', 'id-wspr-freq-'+ wspr_fbn) +'</td>';
	wspr_fbn++;
	return s;
}
