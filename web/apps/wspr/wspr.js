// KiwiSDR data demodulator UI
//
// Copyright (c) 2014 John Seamons, ZL/KF6VO

// TODO
// prevent decode overrun crash
// sanity check upload data
// keep demo overrun from crashing server
// final wsprstat upload interval

// demo doesn't always decode same way!!!
//		implies uninitialized C variables?
// prefix all global vars with 'wspr_'
// ntype non-std 29 seen
// decode task shows > 100% in "-stats" task display
// update to WSJT-X merged version
// admin panel config
// user cookie config

var wspr_client_app_name = 'WSPR';

var wspr_canvas_width = 1024;
//var wspr_canvas_height = 150;		// not currently used

var wspr_setup = false;
var ws_wspr;

function wspr_interface()
{
	if (!wspr_setup) {
		ws_wspr = open_websocket("APP", timestamp, wspr_recv);
		// when the stream thread is established on the server it will automatically send a "SET init" to us
		
		setTimeout(function() { setInterval(function() { ws_wspr.send("SET keepalive") }, 5000) }, 5000);
		wspr_setup = true;
	} else {
		wspr_controls_setup();
	}
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

		if (param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('wspr_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('wspr_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "app_client_name_query":
				// rx_chan is set globally by waterfall code
				ws_wspr.send('SET app_client_name_reply='+ wspr_client_app_name +' rx_chan='+ rx_chan);
				break;

			case "WSPR_INIT":
				wspr_controls_setup();
				break;

			case "WSPR_TIME":
				wspr_server_time_ms = param[1] * 1000;
				wspr_local_time_epoch_ms = Date.now();
				break;

			case "WSPR_SYNC":
				console.log('WSPR: WSPR_SYNC');
				break;

			case "WSPR_STATUS":
				var status = parseInt(param[1]);
				console.log('WSPR: WSPR_STATUS='+ status);
				wspr_set_status(status);
				break;

			case "WSPR_DECODED":
				var s = decodeURIComponent(param[1]);
				console.log('WSPR: '+ s);
				var o = html('wspr-decode');
				o.innerHTML += s +'<br>';
				o.scrollTop = o.scrollHeight;
				wspr_upload(wspr_report_e.SPOT, s);
				break;
			
			case "WSPR_PEAKS":
				var s = decodeURIComponent(param[1]);
				var p = s.split(':');
				var xscale = 2;
				var npk = (p.length-1)/2;
				console.log('WSPR: '+ npk +' '+ s);

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

			case "BFO":
				WSPR_BFO = parseInt(param[1]);
				break;
				
			case "nbins":
				wspr_bins = parseInt(param[1]);
				
				// starting x position given that wspr display is centered in canvas
				// typically (1024 - 411*4)/2 = 101
				// remember that wspr_canvas is scaled to fit screen width
				wspr_startx = Math.round((wspr_canvas_width - wspr_bins*2)/2);
				break;

			case "keepalive":
				break;

			default:
				console.log('wspr_recv ?: '+ param[0] +'='+ param[1]);
				break;
		}
	}
}

var wspr_report_e = { STATUS:0, SPOT:1 };

var psize = 25;
var psize2 = psize*2;
var mycall = "";
var mygrid = "";

var wspr_calls = ["ZL/KF6VO"];
var wspr_grids = ["RF82ci"];

var wspr_spectrum_A, wspr_spectrum_B, wspr_scale_canvas;
var wccva, wccva0;

function wspr_controls_setup()
{
	wspr_fbn = 0;
	
   html('id-app-container').innerHTML=
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
	wspr_spectrum_A = html_id('id-wspr-spectrum-A');
	wspr_spectrum_A.ct = wspr_spectrum_A.getContext("2d");
	wspr_spectrum_A.im = wspr_spectrum_A.ct.createImageData(1024, 1);

	wspr_spectrum_B = html_id('id-wspr-spectrum-B');
	wspr_spectrum_B.ct = wspr_spectrum_B.getContext("2d");
	wspr_spectrum_B.im = wspr_spectrum_B.ct.createImageData(1024, 1);
	
	wccva = wspr_spectrum_A; wccvao = wspr_spectrum_B;

	wspr_scale_canvas = html_id('id-wspr-scale-canvas');
	wspr_scale_canvas.ct = wspr_scale_canvas.getContext("2d");

	var s =
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
					'<td>'+ kiwi_button('demo', 'wspr_freq('+ wspr_fbn +');') + '</td>' +
					'<td colspan="2" id="id-wspr-upload-bkg"> <input id="id-wspr-upload" type="checkbox" value="" onclick="wspr_set_upload(this.checked, true)"> upload spots</td>' +
					'<td></td>'+
				'</tr>' +
			'</table>', 95
		) +

		w3_col_percent('', '',
			'<svg width="'+psize2+'" height="'+psize2+'" viewbox="0 0 '+psize2+' '+psize2+'" style="float:left; margin-top:5px; background-color:#575757">'+
				'<!--<path id="border" transform="translate('+psize+', '+psize+')" />-->'+
				'<circle cx="'+psize+'" cy="'+psize+'" r="'+psize+'" fill="#eeeeee" />'+
				'<path id="pie_path" style="fill:deepSkyBlue" transform="translate('+psize+', '+psize+')" />'+
			'</svg>', 15,
		
			w3_divs('', '',
				w3_divs('id-wspr-time cl-wspr-text', '', ''),
				w3_divs('id-wspr-status cl-wspr-text', '', '')
			), 30,

			w3_divs('', '',
				w3_divs('id-reporter-call cl-wspr-text', '', 'reporter call ZL/KF6VO'),
				w3_divs('id-reporter-grid cl-wspr-text', '', 'reporter grid RF82ci')
			), 50

		/*
			w3_divs('', '',
				'<div class="cl-wspr-text" style="clear:right">'+
					'<span style="white-space:nowrap"> My call:'+
						'<select id="wspr_call" onchange="wspr_call(this.value)">'+
							'<option selected value="0">'+ wspr_calls[0] +'</option>'+
							'<option value="1">'+ wspr_calls[1] +'</option>'+
						'</select>'+
					'</span>'+
				'</div><br>' +
		
				'<div class="cl-wspr-text">'+
					'<span style="white-space:nowrap; clear:right"> My grid:'+
						'<select id="wspr_grid" onchange="wspr_grid(this.value)">'+
							'<option selected value="0">'+ wspr_grids[0] +'</option>'+
							'<option value="1">'+ wspr_grids[1] +'</option>'+
						'</select>'+
					'</span>'+
				'</div>'
			), 40
		*/
		) +

		'<div style="background-color:lightGray; overflow:auto; width:100%; margin-top:5px; margin-bottom:0px; font-family:monospace; font-size:100%">'+
			'<pre style="display:inline"> UTC  dB   dT      Freq dF  Call        dBm</pre>'+
		'</div>'+
		'<div id="wspr-decode" style="white-space:pre; background-color:#eeeeee; overflow:scroll; height:100px; width:100%; margin-top:0px; font-family:monospace; font-size:100%"></div>'+
	"</div>";

	app_panel_show(s, true, null, function() {
		console.log('### APP_PANEL_HIDE: SEND: SET capture=0');
		ws_wspr.send('SET capture=0 demo=0');
		wspr_visible(0);
	});

	wspr_visible(1);
}

function wspr_reset()
{
	console.log('### wspr_reset');
	wspr_demo = 0;
	ws_wspr.send('SET capture=0 demo=0');
	wspr_set_status(wspr_status.IDLE);
	var upload = initCookie('wspr_upload', 'false');	// set upload to cookie state
	wspr_set_upload((upload == "true")? true:false, false);  	
}

function wspr_clear()
{
	console.log('### wspr_clear');
	wspr_reset();
	var o = html('wspr-decode');
	o.innerHTML = '';
}

var wspr_upload_timeout, wspr_pie_interval;

function wspr_visible(v)
{
	//visible_block('id-wspr-controls', v);
	visible_block('id-wspr-peaks', v);
	visible_block('id-wspr-spectrum', v);
	visible_block('id-wspr-scale', v);

	if (v) {
		wspr_pie_interval = setInterval(function() { wspr_draw_pie(); }, 1000);
		wspr_draw_pie();
   	wspr_draw_scale(100);
   	html('wspr_call').value = initCookie('wspr_call', '0');
   	html('wspr_grid').value = initCookie('wspr_grid', '0');
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

	wspr_scale_canvas.ct.fillStyle="lime";
	var start = Math.round(50*(512.0/375.0));
	var width = Math.round(200*(512.0/375.0));
	wspr_scale_canvas.ct.fillRect(wspr_startx+start*2, 0, width*2, 3);
	wspr_scale_canvas.ct.fillStyle="red";
	start = Math.round(150*(512.0/375.0));
	wspr_scale_canvas.ct.fillRect(wspr_startx + start*2-2, 0, 5, 3);
	
	wspr_scale_canvas.ct.fillStyle="white";
	wspr_scale_canvas.ct.font="10px Verdana";
	var f;
	for (f=-150; f<=150; f+=50) {
		var bin = Math.round((f+150)*(512.0/375.0));
		var tf = f + cf;
		if (tf < 0) tf += 1000;
		var tcw = (tf < 10)? 4 : ((tf < 100)? 7:11);
		wspr_scale_canvas.ct.fillText(tf.toFixed(0), wspr_startx + (bin*2)-tcw, 15);
	}
}

function wspr_set_upload(upload, update_cookie)
{
	//jks no uploading yet
	upload = false;
	
	html('id-wspr-upload').checked = upload;
	if (update_cookie) writeCookie('wspr_upload', upload);
	html('id-wspr-upload-bkg').style.backgroundColor = upload? "white":"yellow";
}

// from WSPR-X via tcpdump: (how can 'rcall' have an un-%-escaped '/'?)
// GET /post?function=wsprstat&rcall=ZL/KF6VO&rgrid=RF82ci&rqrg=14.097100&tpct=0&tqrg=14.097100&dbm=0&version=0.8_r3058 HTTP/1.1
// GET /post?function=wspr&rcall=ZL/KF6VO&rgrid=RF82ci&rqrg=14.097100&date=140818&time=0808&sig=-25&dt=-0.2&drift=-1&tqrg=14.097018&tcall=VK6PG&tgrid=OF78&dbm=33&version=0.8_r3058 HTTP/1.1

// type = SPOT: called from server WSPR_DECODED
// type = STATUS: called internally
function wspr_upload(type, s)
{
	var spot = (type == wspr_report_e.SPOT)? 1:0;
	var rcall = readCookie('wspr_call');
	var rgrid = readCookie('wspr_grid');
	var valid = wspr_rfreq && wspr_tfreq && (rcall != null) && (rgrid != null);
	
	// don't even report status if not uploading
	if (!valid || (html('id-wspr-upload').checked == false)) {
		wspr_upload_timeout = setTimeout('wspr_upload(wspr_report_e.STATUS)', 1*60*1000);
		return;
	}
	
	var decode;
	if (spot) {
		decode = s.replace(/[\s]+/g, ' ').split(' ');		// remove multiple spaces before split()
	}
	
	var tqrg, dbm;
	
	// FIXME jks
	//var url = "http://wsprnet.org/post";
	var url = "http://example.com/post";
	var request = kiwi_GETrequest(spot? "spot":"stat", url);
	kiwi_GETrequest_param(request, "function", spot? "wspr":"wsprstat");
	kiwi_GETrequest_param(request, "rcall", wspr_calls[rcall]);
	kiwi_GETrequest_param(request, "rgrid", wspr_grids[rgrid]);
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
	var version = "0.1_kiwi";
	//var version = "0.8_r3058";
	kiwi_GETrequest_param(request, "version", version);
	kiwi_GETrequest_submit(request);

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

// from http://jsfiddle.net/gFnw9/12/
function wspr_draw_pie() {
	var d = new Date(wspr_server_time_ms + (Date.now() - wspr_local_time_epoch_ms));
	html('id-wspr-time').innerHTML = d.toUTCString().substr(17,8) +' UTC';
   var wspr_secs = (d.getUTCMinutes()&1)*60 + d.getUTCSeconds() + 1;
	var alpha = 360/120 * wspr_secs;

	var r = alpha * Math.PI / 180,
	x = Math.sin(r) * psize,
	y = Math.cos(r) * -psize,
	mid = (alpha >= 180) ? 1:0,
	animate;
	
	if (alpha == 360) { mid = 1; x = -0.1; y = -psize; }
	animate = 'M 0 0 v '+ (-psize) +' A '+ psize +' '+ psize +' 1 '+ mid +' 1 '+  x  +' '+  y  +' z';
	html('pie_path').setAttribute('d', animate);
};

// order matches button instantiation order ('demo' is last)
var wspr_freqs = [ 136, 474.2, 1836.6, 3592.6, 5287.2, 7038.6, 10138.7, 14095.6, 18104.6, 21094.6, 24924.6, 28124.6, 0 ];
var wspr_cfreqs = [ 500, 700, 100, 100, 700, 100, 200, 100, 100, 100, 100, 100, 100 ];
var wspr_rfreq=0, wspr_tfreq=0;

var WSPR_BFO = 1500;

var wspr_demo = 0;

function wspr_freq(b)
{
	var f = wspr_freqs[b];
	var cf = wspr_cfreqs[b];
	var mode = 1;
	if (f == 0) {		// demo mode
		f = 14095.6;
		cf = 100;
		wspr_demo = 1;
		wspr_set_upload(false, false);		// don't upload demo decodes!
	} else {
		wspr_demo = 0;
		wspr_reset();
	}
	wspr_rfreq = wspr_tfreq = (f + (WSPR_BFO / 1000))/1000;
	tune(f, 'usb', zoom.max_in);
	setbw(f, WSPR_BFO-200, WSPR_BFO+200);
	ws_wspr.send('SET freq='+ f.toFixed(2));
	ws_wspr.send('SET capture=1 demo='+ wspr_demo);
   wspr_draw_scale(cf);
   
   // promptly notify band change
   kiwi_clearTimeout(wspr_upload_timeout);
   wspr_upload_timeout = setTimeout('wspr_upload(wspr_report_e.STATUS)', 1000);
}

var wspr_fbn=0;
function wspr_freq_button(v)
{
	var s = "<td>"+kiwi_button(v, 'wspr_freq('+wspr_fbn+');')+"</td>";
	wspr_fbn++;
	return s;
}

// fixme: wrong, don't want the receiver _user_ to change this -- put in administration panel
function wspr_call(v)
{
   writeCookie('wspr_call', v);
}

function wspr_grid(v)
{
   writeCookie('wspr_grid', v);
}

