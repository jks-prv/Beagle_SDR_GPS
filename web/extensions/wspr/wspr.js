// KiwiSDR data demodulator UI
//
// Copyright (c) 2014 John Seamons, ZL/KF6VO

// TODO
// sanity check upload data
// final wsprstat upload interval

// prefix all global vars with 'wspr_'
// ntype non-std 29 seen
// decode task shows > 100% in "-stats" task display
// update to WSJT-X merged version
// see screenshot: case where double peaks of same station, same freq didn't get filtered out?
// re-enable spot uploads

var wspr = {
   focus_interval: null,
   pie_size: 25,
};

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
var WSPR_F_DECODED =		0x4000;
var WSPR_F_IMAGE =		0x8000;

function wspr_recv(data)
{
	var firstChars = arrayBufferToStringLen(data, 3);
	
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

			case "WSPR_TIME_MSEC":
				wspr_server_time_ms = param[1] * 1000 + (+param[2]);
				wspr_local_time_epoch_ms = Date.now();
			   //console.log('WSPR_TIME_MSEC server: '+ (new Date(wspr_server_time_ms)).toUTCString() +
			   //   ' local: '+ (new Date(wspr_local_time_epoch_ms)).toUTCString());
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
				var wasScrolledDown = kiwi_isScrolledDown(o);
				o.innerHTML += s +'<br>';
				
				// only jump to bottom of updated list if it was already sitting at the bottom
				if (wasScrolledDown) o.scrollTop = o.scrollHeight;
				break;
			
			case "WSPR_UPLOAD":
				var s = decodeURIComponent(param[1]);
				wspr_upload(wspr_report_e.SPOT, s);
				break;
			
			case "WSPR_PEAKS":
				var s = decodeURIComponent(param[1]);
				var p, npk = 0;
				if (s != '') {
					p = s.split(':');
					if (p.length) npk = (p.length-1)/2;
				}
				//console.log('WSPR: '+ npk +' '+ s);
				var xscale = 2;

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
					var snr = snr_call.filterInt();
					var color;
					
					if (isNaN(snr)) {
						color = (flags & WSPR_F_IMAGE)? 'cl-wspr-image' : 'cl-wspr-call';
					} else {
						snr_call = snr.toFixed(0);
						color = (flags & WSPR_F_DECODING)? 'cl-wspr-decoding':'';
					}
					
					s +=
						w3_div('cl-wspr-mkr1|max-width:'+ (nextx-x) +'px; left:'+ (x-6) +'px; bottom:8px',
							w3_div('cl-wspr-mkr2',
								w3_div('cl-wspr-snr '+ color, snr_call)
							)
						) +
						w3_div('cl-wspr-line '+ color +'|width:1px; height:10px; position:absolute; left:'+ x +'px; bottom:0px;');
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

var wspr_spectrum_A, wspr_spectrum_B, wspr_scale_canvas;
var wccva, wccva0;

var wspr_config = { deco:1 };
var wspr_config_okay = true;
var wspr_init_band = -1;

function wspr_controls_setup()
{
   var data_html =
      time_display_html('wspr') +

      w3_div('id-wspr-peaks|width:1024px; height:30px; background-color:black; position:relative;',
      	w3_div('id-wspr-peaks-labels|width:1024px; height:30px; position:absolute;')
      ) +

   	w3_div('id-wspr-spectrum|width:1024px; height:150px; overflow:hidden; position:relative;',
			// two overlapping canvases to implement scrolling
   		'<canvas id="id-wspr-spectrum-A" width="1024" height="150" style="position:absolute">test</canvas>',
   		'<canvas id="id-wspr-spectrum-B" width="1024" height="150" style="position:absolute">test</canvas>'
   	) +
   	
      w3_div('id-wspr-scale|width:1024px; height:20px; background-color:black; position:relative;',
   		'<canvas id="id-wspr-scale-canvas" width="1024" height="20" style="position:absolute"></canvas>'
      );
   
   var call = ext_get_cfg_param_string('WSPR.callsign', '');
   if (call == undefined || call == null || call == '') {
   	call = '(not set)';
   	wspr_config_okay = false;
   }
   var grid = kiwi.WSPR_rgrid;
   if (grid == undefined || grid == null || grid == '') {
   	grid = '(not set)';
   	wspr_config_okay = false;
      wspr.grid = '';
   } else {
      wspr.grid = grid;
   }
   
   // re-define band menu if down-converter in use
   var r = ext_get_freq_range();
   if (r.lo_kHz > 32000 && r.hi_kHz > 32000) {
      var f_kHz;
      for (i = 0; i < wspr_xvtr_center_freqs.length-1; i++) {
         f_kHz = wspr_xvtr_center_freqs[i];
         if (f_kHz >= r.lo_kHz && f_kHz <= r.hi_kHz)
            break;
      }
      if (i != wspr_xvtr_center_freqs.length) {
         wspr_center_freqs = [ f_kHz ];
         wspr_freqs_u = { 0:wspr_xvtr_freqs_u[i] };
         if (wspr_init_band > 0) wspr_init_band = 0;
      }
   }

	var controls_html =
	w3_div('id-wspr-controls',
		w3_inline('w3-halign-space-between|width:83%/',
         w3_select('', '', 'band', 'wspr_init_band', wspr_init_band, wspr_freqs_u, 'wspr_band_select_cb'),
         w3_button('cl-wspr-button', 'stop', 'wspr_stop_start_cb'),
         w3_button('cl-wspr-button', 'clear', 'wspr_clear_cb'),
         w3_div('id-wspr-upload-bkg cl-upload-checkbox',
            '<input id="id-wspr-upload" type="checkbox" value="" onclick="wspr_set_upload(this.checked)"> upload spots'
         ),
         w3_div('w3-medium w3-text-aqua cl-viewer-label', '<b>WSPR<br>viewer</b>')
		),

		w3_inline('w3-halign-space-between/',
         w3_div('cl-wspr-pie|background-color:#575757',
            kiwi_pie('id-wspr-pie', wspr.pie_size, '#eeeeee', 'deepSkyBlue')
         ),
         w3_div('',
            w3_div('id-wspr-time cl-wspr-text'),
            w3_div('id-wspr-status cl-wspr-text')
         ),
         // FIXME: field validation
         w3_div('',
            w3_div('cl-wspr-text', 'BFO '+ wspr_bfo),
            w3_div('id-wspr-cf cl-wspr-text')
         ),
         w3_div('cl-wspr-text', 'reporter call '+ call),
         w3_div('id-wspr-rgrid cl-wspr-text', 'reporter grid '+ grid)
		),
		
		w3_div('|background-color:lightGray; overflow:auto; width:100%; margin-top:5px; margin-bottom:0px; font-family:monospace; font-size:100%',
			'<pre style="display:inline"> UTC  dB   dT      Freq dF  Call   Grid    km  dBm</pre>'
			//                                                   dd  cccccc GGGG ddddd  nnn (n W)
		) +
		w3_div('id-wspr-decode|white-space:pre; background-color:white; overflow:scroll; height:100px; width:100%; margin-top:0px; font-family:monospace; font-size:100%')
	);

	ext_panel_show(controls_html, data_html, null);
	ext_set_controls_width_height(null, 240);
	time_display_setup('wspr');
	//wspr_resize();
	
	wspr_spectrum_A = w3_el('id-wspr-spectrum-A');
	wspr_spectrum_A.ct = wspr_spectrum_A.getContext("2d");
	wspr_spectrum_A.im = wspr_spectrum_A.ct.createImageData(1024, 1);

	wspr_spectrum_B = w3_el('id-wspr-spectrum-B');
	wspr_spectrum_B.ct = wspr_spectrum_B.getContext("2d");
	wspr_spectrum_B.im = wspr_spectrum_B.ct.createImageData(1024, 1);
	
	wccva = wspr_spectrum_A; wccvao = wspr_spectrum_B;

	wspr_scale_canvas = w3_el('id-wspr-scale-canvas');
	wspr_scale_canvas.ct = wspr_scale_canvas.getContext("2d");

   wspr_pie_interval = setInterval(wspr_draw_pie, 1000);
   wspr_draw_pie();
   wspr_draw_scale(100);
   wspr_reset();
   wspr_upload_timeout = setTimeout(function() {wspr_upload(wspr_report_e.STATUS);}, 1000);
	
	// set band and start if URL parameter present
	var p = ext_param();
	if (p) {
		p = p.toLowerCase();
		if (typeof wspr_freqs_s[p] != 'undefined') {
			w3_set_value('wspr_init_band', wspr_freqs_s[p]);
			wspr_band_select_cb('wspr_init_band', wspr_freqs_s[p], false);
		} else {
			console.log('WSPR ext_param='+ p +' UNKNOWN');
		}
	} else {
		// if reactivating, start up on same band
		if (wspr_init_band != -1)
			wspr_freq(wspr_init_band);
	}
}

function wspr_help(show)
{
   console.log('wspr_help show='+ show);
   if (1) return false;
   if (show) {
      var s = 'And this is where WSPR help goes..';
      confirmation_show_content(s);
   }
   return true;
}

/*
function wspr_resize()
{
	var left = (window.innerWidth - 1024 - time_display_width()) / 2;
	w3_el('id-wspr-peaks').style.left = px(left);
	w3_el('id-wspr-spectrum').style.left = px(left);
	w3_el('id-wspr-scale').style.left = px(left);
}
*/

function wspr_band_select_cb(path, idx, first)
{
	//console.log('wspr_band_select_cb idx='+ idx +' path='+ path);
	idx = +idx;
	if (idx != -1 && !first) {
		wspr_init_band = idx;
		wspr_freq(idx);
	}
}

function wspr_focus()
{
   //console.log('### wspr_focus');
}

function wspr_blur()
{
	//console.log('### wspr_blur');
   ext_send('SET capture=0');
   kiwi_clearTimeout(wspr_upload_timeout);
   kiwi_clearInterval(wspr_pie_interval);
}


////////////////////////////////
// admin
////////////////////////////////

function wspr_input_grid_cb(path, val, first)
{
   //console.log('wspr_input_grid_cb val='+ val);
	w3_string_set_cfg_cb(path, val);
	
	// need this because wspr_check_GPS_update_grid() runs asynch of server sending updated value via 10s status
	kiwi.WSPR_rgrid = val;
}

var wspr_autorun_u = {
   0:'regular use', 1:'LF', 2:'MF', 3:'160m', 4:'80m_JA', 5:'80m', 6:'60m', 7:'60m_EU',
   8:'40m', 9:'30m', 10:'20m', 11:'17m', 12:'15m', 13:'12m', 14:'10m'
   //15:'hop coordinated', 16:'hop custom'
};

function wspr_config_html()
{
	ext_admin_config(wspr_ext_name, 'WSPR',
		w3_div('id-wspr w3-text-teal w3-hide',
			'<b>WSPR configuration</b>',
			'<hr>',
			w3_div('w3-show-inline-block',
            w3_col_percent('w3-container w3-restart/w3-margin-bottom',
               w3_input_get('', 'BFO Hz (multiple of 375 Hz)', 'WSPR.BFO', 'w3_num_set_cfg_cb', '', 'typically 750 Hz'), 30, ''
            ),
            w3_col_percent('w3-container w3-restart/w3-margin-bottom',
               w3_input_get('', 'Reporter callsign', 'WSPR.callsign', 'w3_string_set_cfg_cb', ''), 30, ''
            ),
            w3_col_percent('w3-container/w3-margin-bottom',
                  w3_input_get('', w3_label('w3-bold', 'Reporter grid square ') +
                     w3_div('id-wspr-grid-set cl-admin-check w3-blue w3-btn w3-round-large w3-hide', 'set from GPS'),
                     'WSPR.grid', 'wspr_input_grid_cb', '', '4 or 6-character grid square location'
                  ), 30,
                  '', 5,
                  w3_divs('w3-center w3-tspace-8',
                     w3_div('', '<b>Update grid continuously from GPS?</b>'),
                     w3_switch('', 'Yes', 'No', 'cfg.WSPR.GPS_update_grid', cfg.WSPR.GPS_update_grid, 'admin_radio_YN_cb'),
                     w3_text('w3-text-black w3-center',
                        'Useful for Kiwis in motion <br> (e.g. marine mobile)'
                     )
                  ), 35,
                  '', 5,
                  w3_divs('w3-center w3-tspace-8',
                     w3_div('', '<b>Log decodes to syslog?</b>'),
                     w3_switch('', 'Yes', 'No', 'cfg.WSPR.syslog', cfg.WSPR.syslog, 'admin_radio_YN_cb'),
                     w3_text('w3-text-black w3-center',
                        'Use with care as over time <br> filesystem can fill up.'
                     )
                  )
            ),
            '<hr>',
            w3_div('w3-container w3-restart',
               w3_div('', '<b>Autorun</b>'),
               w3_div('w3-container',
                  w3_div('w3-text-black', 'On startup automatically begins running the WSPR decoder on the selected band(s).<br>' +
                     'Channels available for regular use are reduced by one for each WSPR autorun enabled.<br>' +
                     'If Kiwi has been configured for a mix of channels with and without waterfalls then channels without waterfalls will be used first.<br><br>' +
                     
                     'Spot decodes are available in the Kiwi log (use "Log" tab above) and are listed on <a href="http://wsprnet.org/drupal/wsprnet/spots" target="_blank">wsprnet.org</a><br>' +
                     'The three fields above must be set to valid values for proper spot entry into the <a href="http://wsprnet.org/drupal/wsprnet/spots" target="_blank">wsprnet.org</a> database.'),
                  w3_div('w3-text-red w3-margin-bottom', 'Must restart the KiwiSDR server for changes to have effect.'),
                  w3_inline('id-wspr-admin-autorun/')
               )
            )
         )
		)
	);
	
	var s = '';
	for (var i=0; i < rx_chans; i++) {
	   s += w3_select_get_param('w3-margin-right|color:red', 'Autorun '+ i, 'WSPR band', 'WSPR.autorun'+ i, wspr_autorun_u, 'admin_select_cb');
	}
	w3_innerHTML('id-wspr-admin-autorun', s);
}

function wspr_config_focus()
{
   //console.log('wspr_config_focus');
   wspr_check_GPS_update_grid();
   wspr.focus_interval = setInterval(function() { wspr_check_GPS_update_grid(); }, 10000);

   var el = w3_el('id-wspr-grid-set');
	if (el) el.onclick = function() {
	   wspr.single_shot_update = true;
	   ext_send("ADM wspr_gps_info");   // NB: must be sent as ADM command
	};
}

function wspr_config_blur()
{
   //console.log('wspr_config_blur');
   kiwi_clearInterval(wspr.focus_interval);
}

function wspr_check_GPS_update_grid()
{
   //console.log('wspr_check_GPS_update_grid kiwi.WSPR_rgrid='+ kiwi.WSPR_rgrid);

   if (w3_get_value('WSPR.grid') != kiwi.WSPR_rgrid) {
      w3_set_value('WSPR.grid', kiwi.WSPR_rgrid);
      w3_input_change('WSPR.grid');
   }
   if (kiwi.GPS_fixes) w3_show_inline_block('id-wspr-grid-set');
   wspr.single_shot_update = false;
}

function wspr_gps_info_cb(o)
{
   //console.log('wspr_gps_info_cb');
   if (!cfg.WSPR.GPS_update_grid && !wspr.single_shot_update) return;
   //console.log(o);
   var wspr_gps = JSON.parse(o);
   //console.log(wspr_gps);
   kiwi.WSPR_rgrid = wspr_gps.grid;
   w3_set_value('WSPR.grid', kiwi.WSPR_rgrid);
   w3_input_change('WSPR.grid', 'wspr_input_grid_cb');
   wspr.single_shot_update = false;
}

var wspr_stop_start_state = 0;

function wspr_stop_start_cb(path, idx, first)
{
   if (wspr_stop_start_state == 0) {
      wspr_reset();
   } else {
      // startup if there is a band to start with
		if (wspr_init_band != -1) {
			wspr_freq(wspr_init_band);
	   }
   }
   
   wspr_stop_start_state ^= 1;
   w3_button_text(path, wspr_stop_start_state? 'start' : 'stop');
}

function wspr_reset()
{
	//console.log('### wspr_reset');
	ext_send('SET capture=0');
	wspr_set_status(wspr_status.IDLE);
	
	wspr_set_upload(wspr_config_okay);		// by default allow uploads unless manually unchecked
}

function wspr_clear_cb(path, idx, first)
{
	wspr_reset();
	html('id-wspr-decode').innerHTML = '';
	html('id-wspr-peaks-labels').innerHTML = '';
}

var wspr_upload_timeout, wspr_pie_interval;

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
   var rgrid = (kiwi.WSPR_rgrid)? kiwi.WSPR_rgrid : cfg.WSPR.grid;
	//console.log('wspr_upload: rcall=<'+ rcall +'> rgrid=<'+ rgrid +'>');
	var valid = wspr_rfreq && wspr_tfreq && (rcall != undefined) && (rgrid != undefined) && (rcall != null) && (rgrid != null) && (rcall != '') && (rgrid != '');

	// don't even report status if not uploading
	if (!valid || (html('id-wspr-upload').checked == false)) {
		wspr_upload_timeout = setTimeout(function() {wspr_upload(wspr_report_e.STATUS);}, 1*60*1000);	// check again in another minute
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
		kiwi_GETrequest_param(request, "tgrid", (decode[6] != 'no_grid')? decode[6] : '');
	}
	
	kiwi_GETrequest_param(request, "dbm", dbm);

	var version = "1.3 Kiwi";
	if (version.length <= 10) {
		kiwi_GETrequest_param(request, "version", version);
		kiwi_GETrequest_submit(request, false);

		//jksd show how many updates there have been
		var now = new Date();
		console.log('WSPR '+ (spot? 'SPOT':'STAT') +' '+ now.toUTCString() + (spot? (' <'+ s +'>'):''));
	}

	// report status every six minutes
	if (!spot) wspr_upload_timeout = setTimeout(function() {wspr_upload(wspr_report_e.STATUS);}, 6*60*1000);
}

var wspr_cur_status = 0;
var wspr_status = { 'NONE':0, 'IDLE':1, 'SYNC':2, 'RUNNING':3, 'DECODING':4 };
var wspr_status_text = { 0:'none', 1:'idle', 2:'sync', 3:'running', 4:'decoding' };
var wspr_status_color = { 0:'white', 1:'lightSkyBlue', 2:'violet', 3:'cyan', 4:'lime' };

function wspr_set_status(status)
{
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
   kiwi_draw_pie('id-wspr-pie', wspr.pie_size, wspr_secs / 120);

   // Check for GPS-driven grid updates, i.e. those not coming from admin WSPR config changes.
   // These are delivered with the 10 second status updates if GPS-driven grid updates enabled.
   var rgrid = (kiwi.WSPR_rgrid)? kiwi.WSPR_rgrid : cfg.WSPR.grid;
   w3_innerHTML('id-wspr-rgrid', 'reporter grid<br>'+ rgrid + (cfg.WSPR.GPS_update_grid? ' (GPS)':''));
};

// order matches menu instantiation order
// see: wsprnet.org/drupal/node/7352
var wspr_center_freqs = [ 137.5, 475.7, 1838.1, 3570.1, 3594.1, 5288.7, 5366.2, 7040.1, 10140.2, 14097.1, 18106.1, 21096.1, 24926.1, 28126.1 ];

var wspr_freqs_s = { 'lf':0, 'mf':1, '160m':2, '80m_JA':3, '80m':4, '60m':5, '60m_EU':6, '40m':7, '30m':8, '20m':9, '17m':10, '15m':11, '12m':12, '10m':13,
                     '6m':0, '4m':0, '2m':0, '70cm':0, '440':0, '23cm':0, '1296':0 };
var wspr_freqs_u = { 0:'LF', 1:'MF', 2:'160m', 3:'80m_JA', 4:'80m', 5:'60m', 6:'60m_EU', 7:'40m', 8:'30m', 9:'20m', 10:'17m', 11:'15m', 12:'12m', 13:'10m' };

var wspr_xvtr_center_freqs = [ 50294.5, 70092.5, 144490.5, 432301.5, 1296501.5 ];
var wspr_xvtr_freqs_u = [ '6m', '4m', '2m', '70cm', '23cm' ];

var wspr_rfreq=0, wspr_tfreq=0;
var wspr_bfo = 750;
var wspr_filter_bw = 300;

//var wspr_last_freq = -1;

function wspr_freq(b)
{
	var cf = wspr_center_freqs[b];
	var mode = 1;
	
   wspr_reset();
	
	/*
	if (wspr_last_freq >= 0)
		w3_el('id-wspr-freq-'+ wspr_last_freq).style.backgroundColor = 'white';
	w3_el('id-wspr-freq-'+ b).style.backgroundColor = 'lime';
	wspr_last_freq = b;
	*/

	w3_el('id-wspr-cf').innerHTML = 'CF '+ cf.toFixed(1);
	var cfo = Math.round((cf - Math.floor(cf)) * 1000);
	wspr_rfreq = wspr_tfreq = cf/1000;
	var dial_freq_kHz = cf - wspr_bfo/1000;
	var r = ext_get_freq_range();
	var fo_kHz = dial_freq_kHz - r.offset_kHz;
	ext_tune(fo_kHz, 'usb', ext_zoom.MAX_IN);
	ext_send('SET dialfreq='+ dial_freq_kHz.toFixed(2) +' cf_offset='+ cfo);
	ext_set_passband(wspr_bfo-wspr_filter_bw/2, wspr_bfo+wspr_filter_bw/2);
	ext_tune(fo_kHz, 'usb', ext_zoom.MAX_IN);		// FIXME: temp hack so new passband gets re-centered
	ext_send('SET capture=1');
   wspr_draw_scale(cfo);
   
   // promptly notify band change
   kiwi_clearTimeout(wspr_upload_timeout);
   wspr_upload_timeout = setTimeout(function() {wspr_upload(wspr_report_e.STATUS);}, 1000);
}
