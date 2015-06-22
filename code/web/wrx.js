// WRX
//
// Copyright (c) 2014 John Seamons, ZL/KF6VO

var WF_SLOW = 0;
var WF_MED = 1;
var WF_FAST = 2;
var WF_OTHERS = 3;

var WF_SMALL = 0;
var WF_MEDIUM = 1;
var WF_LARGE = 2;

var WF_SPLIT = 0;
var WF_SPECTRUM = 1;
var WF_NORM = 2;
var WF_WEAK = 3;
var WF_STRONG = 4;

var VBW_OFF = 0;
var VBW_AVG = 1;
var VBW_RC = 2;
var VBW_NONLIN = 3;

var MAX_ZOOM = 10;
var SMETER_CALIBRATION = -12;

var try_again="";
var admin_user;
var noPasswordRequired = false;

function wrx_bodyonload(type)
{
	admin_user = type;
	
	if (type == 'demop' && noPasswordRequired) {
		visible_block('id-wrx-msg', 0);
		visible_block('id-wrx-container-0', 1);
		bodyonload(type);
		return;
	}
	
	var p = readCookie(type);
	//console.log("wrx_bodyonload type="+type);
	if (p && (p != "bad")) {
		//console.log("wrx_bodyonload pwd="+p);
		wrx_valpwd(type, p);
	} else {
		html('id-wrx-msg').innerHTML = "WRX prototype receiver <br>"+
			"<form name='pform' action='#' onsubmit='wrx_valpwd(\""+type+"\", this.pwd.value); return false;'>"+
				try_again+"Password: <input type='text' size=10 name='pwd' onclick='this.focus(); this.select()'>"+
  			"</form>";
		visible_block('id-wrx-msg', 1);
		document.pform.pwd.focus();
		document.pform.pwd.select();
	}
}

function wrx_valpwd(type, p)
{
	//console.log("wrx_valpwd type="+type+" pwd="+p);
	wrx_ajax("/PWD?type="+type+"&pwd="+p, true);
}

var key="";

function wrx_setpwd(p, _key)
{
	writeCookie(admin_user, p);
	if (p != "bad") {
		key = _key;
		html('id-wrx-msg').innerHTML = "";
		visible_block('id-wrx-msg', 0);
		visible_block('id-wrx-container-0', 1);
		bodyonload(admin_user);
	} else {
		try_again = "Try again. ";
		wrx_bodyonload(admin_user);
	}
}

function wrx_key()
{
	return key;
}

function wrx_geolocate()
{
	wrx_ajax('http://www.telize.com/geoip?callback=wrx_geo_callback', true);
}

var geo = "";
var geojson = "";

function wrx_geo_callback(json)
{
	//console.log(json);
	geojson = 'ci='+json.city+' re='+json.region+' co='+json.country+' c3='+json.country_code3+' cn='+json.continent_code+' isp='+json.isp;
	if (json.country && json.country == "United States" && json.region) {
		json.country = json.region + ', USA';
	}
	//geo = json.ip;
	geo = "";
	if (json.city)
		geo += json.city;
	if (json.country)
		geo += (json.city? ', ':'')+ json.country;
	//jksp
	//traceA('***geo=<'+geo+'>');
}
    
function wrx_geo()
{
	//jksp
	//traceA('wrx_geo()=<'+geo+'>');
	return encodeURIComponent(geo);
}

function wrx_geojson()
{
	//jksp
	//traceA('wrx_geo()=<'+geo+'>');
	return encodeURIComponent(geojson);
}

function wrx_plot_max(b)
{
   var t = bi[b];
   var plot_max = 1024 / (t.samplerate/t.plot_samplerate);
   return plot_max;
}

function wrx_too_busy(rx_chans)//z
{
	wrx_too_busy_ui();
	
	html('id-wrx-msg').innerHTML=
	"Sorry, the WRX server is too busy right now ("+rx_chans+((rx_chans>1)?" users":" user")+" max). <br> Please try again later.";
	visible_block('id-wrx-msg', 1);
	visible_block('id-wrx-container-1', 0);
}

function wrx_up(smeter_calib)
{
	SMETER_CALIBRATION = smeter_calib - /* bias */ 100;
	visible_block('id-wrx-msg', 0);
	visible_block('id-wrx-container-1', 1);
}

function wrx_down()
{
	html('id-wrx-msg').innerHTML=
		"Sorry, the WRX server is being used for development right now. <br> Please try between 0600 - 1800 UTC.";
	visible_block('id-wrx-msg', 1);
	visible_block('id-wrx-container-1', 0);
}

function wrx_gps()
{
	wrx_waterfallmode(WF_SPECTRUM);
}

function wrx_fft()
{
	if (0) {
		wrx_waterfallmode(WF_SPECTRUM);
		setmaxdb(10);
		setVBW(VBW_NONLIN);
	} else {
		setmaxdb(-30);
	}
}

var wrx_cpu_stats_str = "";

function wrx_cpu_stats(s)
{
	//html('id-cpu-stats').innerHTML = s;
	wrx_cpu_stats_str = s;
}

var wrx_audio_stats_str = "";

function wrx_audio_stats(s)
{
	//html('id-audio-stats').innerHTML = s;
	wrx_audio_stats_str = s;
}

function wrx_config(s)
{
	html('id-config').innerHTML = s;
}


var spurs = [
// z0
[ 9215, 12000, 13822, 16368, 17693, 18425, 23030, 24576 ],

// z1
[ 15250.8, 15617, 16167, 16228, 24576 ],

// z2
[ 4612, 16166.7, 16228 ],

// z3
[  ],

// z4
[

// BCB
594, 614, 640, 686, 777, 960, 1051, 1143, 1188, 1647, 1665, 1692, 1764, 1827, 1926,

13062,

// 20m ham
13999, 14029.5, 14053, 14090.5, 14151.6, 14212.7, 14273.7, 14334.8,

15006.5,

// 16m bc
17510, 17571,

// 17m ham
18121,

// 15m bc
18915,

// 15m ham
20991, 21021.3, 21052, 21113, 21174, 21204.6, 21235, 21296, 21326.6, 21357, 21418.5

],

// z5+
[

// LW
12.3, 45.8, 58, 91.5, 99,
117, 137.2, 154, 162, 189,
216, 228.6, 234, 279, 288,
320, 333, 351,
411.5, 414, 450, 468

]

];


// utility

function arrayBufferToString(buf) {
	//http://stackoverflow.com/questions/6965107/converting-between-strings-and-arraybuffers
	return String.fromCharCode.apply(null, new Uint8Array(buf));
}

function getFirstChars(buf, num)
{
	var u8buf=new Uint8Array(buf);
	var output=String();
	num=Math.min(num,u8buf.length);
	for(i=0;i<num;i++) output+=String.fromCharCode(u8buf[i]);
	return output;
}

function wrx_n2h_32(ba, o)
{
	//traceB(ba[o]+':'+ba[o+1]+':'+ba[o+2]+':'+ba[o+3]);
	return ba[o]+(ba[o+1]<<8)+(ba[o+2]<<16)+(ba[o+3]<<24);
}

// http://stackoverflow.com/questions/2998784/how-to-output-integers-with-leading-zeros-in-javascript
Number.prototype.leadingZeros = function(size)
{
	var s = String(this);
	if (typeof(size) !== "number") size = 2;
	while (s.length < size) s = "0"+s;
	return s;
}

String.prototype.leadingZeros = function(size)
{
	var s = String(this);
	if (typeof(size) !== "number") size = 2;
	while (s.length < size) s = "0"+s;
	return s;
}

function wrx_clearTimeout(timeout)
{
   try { clearTimeout(timeout); } catch(e) {};
}

function wrx_clearInterval(timeout)
{
   try { clearInterval(timeout); } catch(e) {};
}

// from http://www.quirksmode.org/js/cookies.html
function createCookie(name,value,days) {
	if (days) {
		var date = new Date();
		date.setTime(date.getTime()+(days*24*60*60*1000));
		var expires = "; expires="+date.toGMTString();
	}
	else var expires = "";
	document.cookie = name+"="+value+expires+"; path=/";
}

function readCookie(name) {
	var nameEQ = name + "=";
	var ca = document.cookie.split(';');
	for(var i=0;i < ca.length;i++) {
		var c = ca[i];
		while (c.charAt(0)==' ') c = c.substring(1,c.length);
		if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
	}
	return null;
}

function writeCookie(cookie, value)
{
	createCookie(cookie, value, 42*365);
}

function initCookie(cookie, initValue)
{
	var v = readCookie(cookie);
	if (v == null) {
		writeCookie(cookie, initValue);
		return initValue;
	} else {
		return v;
	}
}

function updateCookie(cookie, initValue)
{
	var v = readCookie(cookie);
	if (v == null || v != initValue) {
		writeCookie(cookie, initValue);
		return null;
	} else {
		return v;
	}
}


// HTML helpers (fixme: switch to jquery at some point?)

var dummy_elem = {};

// return document element reference either by id or name
function html(id_or_name)
{
	var el = document.getElementById(id_or_name);
	if (el == null) {
		el = document.getElementsByName(id_or_name);
		if (el != null) el = el[0];	// use first from array
	}
	var debug;
	try {
		debug = el.value;
	} catch(ex) {
		console.log("html('"+id_or_name+"')="+el+" FAILED");
		//traceA("FAILED: id_or_name="+id_or_name);
		//wrx_trace();
	}
	if (el == null) el = dummy_elem;		// allow failures to proceed, e.g. assignments to innerHTML
	return el;
}

// from http://www.switchonthecode.com/tutorials/javascript-tutorial-the-scroll-wheel
function cancelEvent(ev)
{
	ev = ev ? ev : window.event;
	if (ev.stopPropagation) ev.stopPropagation();
	if (ev.preventDefault) ev.preventDefault();
	ev.cancelBubble = true;
	ev.cancel = true;
	ev.returnValue = false;
	return false;
}

function ignore(ev)
{
	return cancelEvent(ev);
}

function visible_inline(id, v)
{
	visible_type(id, v, 'inline');
}

function visible_block(id, v)
{
	visible_type(id, v, 'block');
}

function visible_type(id, v, type)
{
	var elem = html(id);
	elem.style.display = v? type:'none';
	if (v) elem.style.visibility = 'visible';
}

function wrx_button(v, oc)
{
	return "<input type='button' value='"+v+"' onclick='"+oc+"'>";
}

// http://stackoverflow.com/questions/298745/how-do-i-send-a-cross-domain-post-request-via-javascript
function wrx_GETrequest(id, url)
{
  // Add the iframe with a unique name
  var iframe = document.createElement("iframe");
  var uniqueString = id +'_'+ (new Date()).getTime().toString();
  document.body.appendChild(iframe);
  iframe.style.display = "none";
  iframe.contentWindow.name = uniqueString;

  // construct a form with hidden inputs, targeting the iframe
  var form = document.createElement("form");
  form.target = uniqueString;
  form.action = url;
  form.method = "GET";
  
  return form;
}

function wrx_GETrequest_submit(form)
{
	document.body.appendChild(form);
	form.submit();
}

function wrx_GETrequest_param(request, name, value)
{
  var input = document.createElement("input");
  input.type = "hidden";
  input.name = name;
  input.value = value;
  request.appendChild(input);
}

function wrx_ajax(url, doEval)
{
	var ajax;
	
	try {
		ajax = new XMLHttpRequest();
	}
	catch (e) {
		try {
			ajax = new ActiveXObject("Msxml2.XMLHTTP");
		}
      catch (e) {
			try {
      		ajax = new ActiveXObject("Microsoft.XMLHTTP");
      	}
			catch (e) {
				return false;
			}
		}
	}

	ajax.onreadystatechange = function() {
		if (ajax.readyState == 4) {
      	//traceA(url+'=<'+ajax.responseText+'>');
			if (doEval && ajax.responseText != "") eval(ajax.responseText);
		}
	}

	ajax.open("GET", url, true);
	ajax.send(null);
	return true;
}

function wrx_is_iOS()
{
	var b = navigator.userAgent.toLowerCase();
	var iOS = (b.indexOf('ios')>0 || b.indexOf('iphone')>0 || b.indexOf('ipad')>0);
	//if (iOS) console.log(b.indexOf('ios')+'='+b);
	return iOS;
}

function wrx_isFirefox()
{
	return /firefox\/([0-9]+)/i.exec(navigator.userAgent)? true:false;
}


//debug

function wrx_server_error(s)
{
	html('id-wrx-msg').innerHTML =
	'Hmm, there seems to be a problem. <br> \
	The server reported the error: <span style="color:red">'+s+'</span> <br> \
	Please <a href="javascript:sendmail(\'ihpCihp-`ln\',\'server error: '+s+'\');">email me</a> the above message. Thanks!';
	visible_block('id-wrx-msg', 1);
	visible_block('id-wrx-container-1', 0);
}

function wrx_serious_error(s)
{
	html('id-wrx-msg').innerHTML = s;
	visible_block('id-wrx-msg', 1);
	visible_block('id-wrx-container-1', 0);
}

function wrx_trace()
{
	try { console.trace(); } catch(ex) {}		// no console.trace() on IE
}

var tr1=0;
var tr2=0;

function trace_on()
{
	if (!tr1) html('trace').innerHTML = 'trace:';
	if (!tr2) html('trace2').innerHTML = 'trace:';
	visible_inline('tr1', 1);
	visible_inline('tr2', 1);
}

function trace(s)
{
   trace_on();
   tr1++;
   html('trace').innerHTML = 'trace: '+s;
   //traceA(s);
}

function traceA(s)
{
   trace_on();
   tr1++;
   //if (tr1==30) tr1=0;
   html('trace').innerHTML += " "+s;
}

function traceB(s)
{
   trace_on();
	tr2 += s.length;
   if (tr2>=150) tr2=0;
   html('trace2').innerHTML += " "+s;
}
