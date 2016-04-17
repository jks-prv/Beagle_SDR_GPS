// KiwiSDR data demodulator UI
//
// Copyright (c) 2014 John Seamons, ZL/KF6VO

// TODO
// prevent decode overrun crash
// sanity check upload data
// keep demo overrun from crashing server
// final wsprstat upload interval
// move out of waterfall code
// separate data stream instead of sharing waterfall?
// multi-user

function data_init()
{
   html('data_div').innerHTML = "<b>Data mode: </b>"+
   	"<select id='data_select' onchange='data_select(1, this.value)'>"+
   		"<option selected value='0'>(select)</option>"+
      	"<option value='1'>WSPR</option>"+
      	"<option value='2'>other</option>"+
    	"</select>"+
    	"<br><hr>"+
		wspr_div()+
    	"";
}

function data_setmode(v)
{
	visible_inline('data_div', v);
	data_select(v, html('data_select').value);
}


function data_select(v, sel)
{
	if (v==0) sel=0;
	wspr_visible((sel==1)?1:0);
	//other_visible((sel==2)?1:0);
}


// WSPR --------------------------------------------------------------------------------------------

var WSPR_STATUS = 0;
var WSPR_SPOT = 1;

var WSPR_BFO = 1.5;

var psize = 25;
var psize2 = psize*2;
var mycall = "";
var mygrid = "";

var wspr_calls = ["ZL/KF6VO", "KF6VO"];
var wspr_grids = ["RF82dg", "DM65pc"];

function wspr_div()
{
	var s =
	"<div id='wspr_div' style='width:auto; display:none'>"+
		"<table>"+
			"<tr>"+
				wspr_freq_button('LF')+
				wspr_freq_button('MF')+
				wspr_freq_button('160m')+
				wspr_freq_button('80m')+
				wspr_freq_button('60m')+
				wspr_freq_button('40m')+
			"</tr><tr>"+
				wspr_freq_button('30m')+
				wspr_freq_button('20m')+
				wspr_freq_button('17m')+
				wspr_freq_button('15m')+
				wspr_freq_button('12m')+
				wspr_freq_button('10m')+
			"</tr>"+
		"</table><table>"+
			"<tr>"+
				'<td style="width:20%">'+ kiwi_button('demo', 'wspr_freq('+wspr_fbn+');') + '</td>'+
				'<td style="width:40%" id="wspr_upload_bkg"> <input id="wspr_upload" type="checkbox" value="" onclick="wspr_setupload(this.checked)">upload spots </td>'+
				'<td style="width:40%"></td>'+
			"</tr>"+
		"</table>"+

		'<svg width="'+psize2+'" height="'+psize2+'" viewbox="0 0 '+psize2+' '+psize2+'" style="float:left; margin-top:5px; background-color:white">'+
		  '<!--<path id="border" transform="translate('+psize+', '+psize+')" />-->'+
			'<circle cx="'+psize+'" cy="'+psize+'" r="'+psize+'" fill="#eeeeee" />'+
			'<path id="pie_path" style="fill:deepSkyBlue" transform="translate('+psize+', '+psize+')" />'+
		'</svg>'+
		
		'<div id="wspr_time" class="wspr_text"> </div>'+
		'<div class="wspr_text" style="clear:right">'+
			'<span style="white-space:nowrap"> My call:'+
				'<select id="wspr_call" onchange="wspr_call(this.value)">'+
					'<option selected value="0">'+ wspr_calls[0] +'</option>'+
					'<option value="1">'+ wspr_calls[1] +'</option>'+
				'</select>'+
			'</span>'+
		'</div><br>'+
		'<div id="wspr_decoding" class="wspr_text"> </div>'+
		'<div class="wspr_text">'+
			'<span style="white-space:nowrap; clear:right"> My grid:'+
				'<select id="wspr_grid" onchange="wspr_grid(this.value)">'+
					'<option selected value="0">'+ wspr_grids[0] +'</option>'+
					'<option value="1">'+ wspr_grids[1] +'</option>'+
				'</select>'+
			'</span>'+
		'</div>'+

		'<div style="background-color:lightGray; overflow:auto; width:100%; margin-top:5px; margin-bottom:0px; font-family:monospace; font-size:80%">'+
			'<pre style="display:inline"> UTC  dB   dT      Freq dF  Call        dBm</pre>'+
		'</div>'+
		'<div id="wspr_decode" style="white-space:pre; background-color:#eeeeee; overflow:scroll; height:100px; width:100%; margin-top:0px; font-family:monospace; font-size:80%"></div>'+
	"</div>";
	return s;
}

var wspr_upload_timeout, wspr_pie_timeout;

function wspr_visible(v)
{
	visible_block('wspr_div', v);
	visible_block('wsprdiv', v);
   waterfallapplet[0].app_visible(v);

	if (v) {
		//kiwi_waterfallmode(...);
		wspr_draw_pie.call(this);
   	waterfallapplet[0].wspr_draw_scale(100);
   	html('wspr_call').value = initCookie('wspr_call', '0');
   	html('wspr_grid').value = initCookie('wspr_grid', '0');

   	var upload = initCookie('wspr_upload', 'false');
   	wspr_setupload((upload == "true")? true:false);
   	
		wspr_upload_timeout = setTimeout('wspr_upload(WSPR_STATUS)', 1000);
	} else {
   	kiwi_clearTimeout(wspr_upload_timeout);
   	kiwi_clearTimeout(wspr_pie_timeout);
	   soundapplet.setparam("wspr=0");
	}
}

function wspr_setupload(upload)
{
	html('wspr_upload').checked = upload;
	writeCookie('wspr_upload', upload);
	html('wspr_upload_bkg').style.backgroundColor = upload? "white":"yellow";
}

// from WSPR-X via tcpdump: (how can 'rcall' have an un-%-escaped '/'?)
// GET /post?function=wsprstat&rcall=ZL/KF6VO&rgrid=RF82dg&rqrg=14.097100&tpct=0&tqrg=14.097100&dbm=0&version=0.8_r3058 HTTP/1.1
// GET /post?function=wspr&rcall=ZL/KF6VO&rgrid=RF82dg&rqrg=14.097100&date=140818&time=0808&sig=-25&dt=-0.2&drift=-1&tqrg=14.097018&tcall=VK6PG&tgrid=OF78&dbm=33&version=0.8_r3058 HTTP/1.1

function wspr_upload(type, s)
{
	var spot = (type == WSPR_SPOT)? 1:0;
	var rcall = readCookie('wspr_call');
	var rgrid = readCookie('wspr_grid');
	var valid = wspr_rfreq && wspr_tfreq && (rcall != null) && (rgrid != null);
	
	// don't even report status if not uploading
	if (!valid || (html('wspr_upload').checked == false)) {
		wspr_upload_timeout = setTimeout('wspr_upload(WSPR_STATUS)', 1*60*1000);
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

	if (!spot) wspr_upload_timeout = setTimeout('wspr_upload(WSPR_STATUS)', 6*60*1000);
}

var wspr_secs = 0;
var wspr_isDecoding=0;

function wspr_decoding(d)
{
	html('wspr_decoding').innerHTML = d? "decoding":"&nbsp;";
	html('wspr_decoding').style.backgroundColor = d? "lime":"white";
	wspr_isDecoding = d;
}

// from http://jsfiddle.net/gFnw9/12/
function wspr_draw_pie() {
   var d = new Date();
	html('wspr_time').innerHTML = d.toUTCString().substr(17,8)+' UTC';
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
	if (!wspr_isDecoding && (wspr_secs >= 8)) html('wsprbar').innerHTML="";
	wspr_pie_timeout = setTimeout(wspr_draw_pie, 1000);
};

// order matches button instantiation order ('demo' is last)
var wspr_freqs = [ 136, 474.2, 1836.6, 3592.6, 5287.2, 7038.6, 10138.7, 14095.6, 18104.6, 21094.6, 24924.6, 28124.6, 0 ];
var wspr_cfreqs = [ 500, 700, 100, 100, 700, 100, 200, 100, 100, 100, 100, 100, 100 ];
var wspr_rfreq=0, wspr_tfreq=0;

function wspr_freq(b)
{
	var f = wspr_freqs[b];
	var cf = wspr_cfreqs[b];
	var mode = 1;
	if (f == 0) {		// demo mode
		f = 14095.6;
		cf = 100;
		mode = 2;
		wspr_setupload(false);
	}
	wspr_rfreq = wspr_tfreq = (f + WSPR_BFO)/1000;
	kiwi_set('usb', f, MAX_ZOOM);
	setmf('usb', WSPR_BFO-0.2, WSPR_BFO+0.2);
	soundapplet.setparam("wspr="+mode);
   waterfallapplet[0].wspr_draw_scale(cf);
   
   // promptly notify band change
   kiwi_clearTimeout(wspr_upload_timeout);
   wspr_upload_timeout = setTimeout('wspr_upload(WSPR_STATUS)', 1000);
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

