// KiwiSDR
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
var noPasswordRequired = true;
var seriousError = false;

// wait until DOM has loaded before proceeding (browser has loaded HTML, but not necessarily images)
document.onreadystatechange = function() {
	console.log("onreadystatechange="+document.readyState);
	//if (document.readyState == "interactive") {
	if (document.readyState == "complete") {
		kiwi_bodyonload();
	}
}

function kiwi_bodyonload()
{
	admin_user = html('id-kiwi-body').getAttribute('data-type');
	if (admin_user == "undefined") admin_user = "demop";
	if (admin_user == "index") admin_user = "demop";
	console.log("admin_user="+admin_user);
	
	if (admin_user == 'demop' && noPasswordRequired) {
		visible_block('id-kiwi-msg', 0);
		visible_block('id-kiwi-container-0', 1);
		bodyonload(admin_user);
		return;
	}
	
	var p = readCookie(admin_user);
	console.log("readCookie admin_user="+admin_user+" p="+p);
	if (p && (p != "bad")) {
		kiwi_valpwd(admin_user, p);
	} else {
		html('id-kiwi-msg').innerHTML = "KiwiSDR: software-defined receiver <br>"+
			"<form name='pform' action='#' onsubmit='kiwi_valpwd(\""+admin_user+"\", this.pwd.value); return false;'>"+
				try_again+"Password: <input type='text' size=10 name='pwd' onclick='this.focus(); this.select()'>"+
  			"</form>";
		visible_block('id-kiwi-msg', 1);
		document.pform.pwd.focus();
		document.pform.pwd.select();
	}
}

function kiwi_valpwd(type, p)
{
	console.log("kiwi_valpwd type="+type+" pwd="+p);
	kiwi_ajax("/PWD?type="+type+"&pwd="+p, true);
}

var key="";

function kiwi_setpwd(p, _key)
{
	console.log("kiwi_setpwd admin_user="+admin_user+" p="+p);
	writeCookie(admin_user, p);
	if (p != "bad") {
		key = _key;
		html('id-kiwi-msg').innerHTML = "";
		visible_block('id-kiwi-msg', 0);
		visible_block('id-kiwi-container-0', 1);
		console.log("calling bodyonload(\""+admin_user+"\")..");
		bodyonload(admin_user);
	} else {
		try_again = "Try again. ";
		console.log("try again");
		kiwi_bodyonload();
	}
}

function kiwi_key()
{
	return key;
}

function kiwi_geolocate()
{
	var callback = "kiwi_geo_callback";
	kiwi_ajax('http://freegeoip.net/json/?callback='+callback, true, callback, function() { setTimeout('kiwi_geolocate();', 1000); } );
}

var geo = "";
var geojson = "";

function kiwi_geo_callback(json)
{
	console.log('kiwi_geo_callback():');
	console.log(json);
	if (window.JSON && window.JSON.stringify)
            geojson = JSON.stringify(json);
        else
				geojson = json.toString();
	
	if (json.country_code && json.country_code == "US" && json.region_name) {
		json.country_name = json.region_name + ', USA';
	}
	
	geo = "";
	if (json.city)
		geo += json.city;
	if (json.country_name)
		geo += (json.city? ', ':'')+ json.country_name;
	//console.log(geo);
	//traceA('***geo=<'+geo+'>');
}
    
function kiwi_geo()
{
	//traceA('kiwi_geo()=<'+geo+'>');
	return encodeURIComponent(geo);
}

function kiwi_geojson()
{
	//traceA('kiwi_geo()=<'+geo+'>');
	return encodeURIComponent(geojson);
}

function kiwi_plot_max(b)
{
   var t = bi[b];
   var plot_max = 1024 / (t.samplerate/t.plot_samplerate);
   return plot_max;
}

function kiwi_too_busy(rx_chans)//z
{
	kiwi_too_busy_ui();
	
	html('id-kiwi-msg').innerHTML=
	'Sorry, the KiwiSDR server is too busy right now ('+ rx_chans+((rx_chans>1)? ' users':' user') +' max). <br>' +
	'Please check <a href="http://sdr.hu" target="_self">sdr.hu</a> for more KiwiSDR receivers available world-wide.';
	visible_block('id-kiwi-msg', 1);
	visible_block('id-kiwi-container-1', 0);
}

function kiwi_up(smeter_calib)
{
	SMETER_CALIBRATION = smeter_calib - /* bias */ 100;
	if (!seriousError) {
		visible_block('id-kiwi-msg', 0);
		visible_block('id-kiwi-container-1', 1);
	}
}

function kiwi_down()
{
	html('id-kiwi-msg').innerHTML=
		//'<span style="position:relative; float:left"><a href="http://bluebison.net" target="_blank"><img id="id-left-logo" src="gfx/kiwi-with-headphones.51x67.png" /></a> ' +
		//<div id="id-left-logo-text"><a href="http://bluebison.net" target="_blank">&copy; bluebison.net</a></div>' +
		//</span><span style="position:relative; float:right">' +
		'Sorry, this KiwiSDR server is being used for development right now. <br>' +
		'Please check <a href="http://sdr.hu" target="_self">sdr.hu</a> for more KiwiSDR receivers available world-wide.' +
		//'Sorry, the KiwiSDR server is being used for development right now. <br> Please try between 0600 - 1800 UTC.' +
		//"<b>We're moving!</b> <br> This KiwiSDR receiver will be down until the antenna is relocated. <br> Thanks for your patience.<br><br>" +
		//'Until then, please try the <a href="http://websdr.ece.uvic.ca" target="_blank">KiwiSDR at the University of Victoria</a>.' +
		//'</span>';
		' ';
	visible_block('id-kiwi-msg', 1);
	visible_block('id-kiwi-container-1', 0);
}

function kiwi_fft()
{
	if (0) {
		toggle_or_set_spec(1);
		setmaxdb(10);
		setVBW(VBW_NONLIN);
	} else {
		setmaxdb(-30);
	}
}

function kiwi_innerHTML(id, s)
{
	html(id).innerHTML = s;
}

function kiwi_num(n)
{
	if (n < 1000) {
		return n.toString();
	} else
	if (n < 1000000) {
		return (n/1000).toFixed(1)+'k';
	} else
	if (n < 1000000000) {
		return (n/1000000).toFixed(1)+'m';
	} else {
		return n.toString();
	}
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

function kiwi_n2h_32(ba, o)
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

function kiwi_clearTimeout(timeout)
{
   try { clearTimeout(timeout); } catch(e) {};
}

function kiwi_clearInterval(timeout)
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
		if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length, c.length);
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
		return true;
	} else {
		return false;
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
		//kiwi_trace();
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

function kiwi_button(v, oc)
{
	return "<input type='button' value='"+v+"' onclick='"+oc+"'>";
}

// http://stackoverflow.com/questions/298745/how-do-i-send-a-cross-domain-post-request-via-javascript
function kiwi_GETrequest(id, url)
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

function kiwi_GETrequest_submit(form)
{
	document.body.appendChild(form);
	form.submit();
}

function kiwi_GETrequest_param(request, name, value)
{
  var input = document.createElement("input");
  input.type = "hidden";
  input.name = name;
  input.value = value;
  request.appendChild(input);
}

// only works on cross-domains if server sends a CORS access wildcard
function kiwi_ajax(url, doEval)	// , callback, readyFunc
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

	var callback = (arguments.length > 2)? arguments[2] : null;
	var retry = false;
	var retryFunc = (arguments.length > 3)? arguments[3] : null;

	ajax.onreadystatechange = function() {
		if (ajax.readyState == 4) {
			var response = ajax.responseText.toString();
			//console.log('AJAX: '+url+' RESPONSE: <'+ response +'>');

			if (response.substr(0,15) == "Try again later") {
				console.log("Try again later "+ typeof(retryFunc));
				retry = true;
			}

			// sometimes the callback form is missing
			if (doEval && !retry && response != "") {
				if (response.charAt(0) == '{' && callback) {
					console.log('AJAX JSON response form missing for '+ callback +'()');
					response = callback +'('+ response +');';
				}
				try {
					eval(response);
				} catch (e) {
					console.log('AJAX EVAL FAIL: '+ url +' RESPONSE: <'+ response +'>');
				}
			}
		}
	}

	ajax.open("GET", url, true);
	ajax.send(null);
	return true;
}

function kiwi_is_iOS()
{
	var b = navigator.userAgent.toLowerCase();
	var iOS = (b.indexOf('ios')>0 || b.indexOf('iphone')>0 || b.indexOf('ipad')>0);
	//if (iOS) console.log(b.indexOf('ios')+'='+b);
	return iOS;
}

function kiwi_isFirefox()
{
	return /firefox\/([0-9]+)/i.exec(navigator.userAgent)? true:false;
}

// https://github.com/kvz/phpjs/blob/master/functions/strings/strip_tags.js
function kiwi_strip_tags(input, allowed) {
  //  discuss at: http://phpjs.org/functions/strip_tags/
  // original by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // improved by: Luke Godfrey
  // improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  //    input by: Pul
  //    input by: Alex
  //    input by: Marc Palau
  //    input by: Brett Zamir (http://brett-zamir.me)
  //    input by: Bobby Drake
  //    input by: Evertjan Garretsen
  // bugfixed by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // bugfixed by: Onno Marsman
  // bugfixed by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // bugfixed by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // bugfixed by: Eric Nagel
  // bugfixed by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // bugfixed by: Tomasz Wesolowski
  //  revised by: Rafał Kukawski (http://blog.kukawski.pl/)
  //   example 1: strip_tags('<p>Kevin</p> <br /><b>van</b> <i>Zonneveld</i>', '<i><b>');
  //   returns 1: 'Kevin <b>van</b> <i>Zonneveld</i>'
  //   example 2: strip_tags('<p>Kevin <img src="someimage.png" onmouseover="someFunction()">van <i>Zonneveld</i></p>', '<p>');
  //   returns 2: '<p>Kevin van Zonneveld</p>'
  //   example 3: strip_tags("<a href='http://kevin.vanzonneveld.net'>Kevin van Zonneveld</a>", "<a>");
  //   returns 3: "<a href='http://kevin.vanzonneveld.net'>Kevin van Zonneveld</a>"
  //   example 4: strip_tags('1 < 5 5 > 1');
  //   returns 4: '1 < 5 5 > 1'
  //   example 5: strip_tags('1 <br/> 1');
  //   returns 5: '1  1'
  //   example 6: strip_tags('1 <br/> 1', '<br>');
  //   returns 6: '1 <br/> 1'
  //   example 7: strip_tags('1 <br/> 1', '<br><br/>');
  //   returns 7: '1 <br/> 1'

  allowed = (((allowed || '') + '')
      .toLowerCase()
      .match(/<[a-z][a-z0-9]*>/g) || [])
    .join(''); // making sure the allowed arg is a string containing only tags in lowercase (<a><b><c>)
  var tags = /<\/?([a-z][a-z0-9]*)\b[^>]*>/gi,
    commentsAndPhpTags = /<!--[\s\S]*?-->|<\?(?:php)?[\s\S]*?\?>/gi;
  return input.replace(commentsAndPhpTags, '')
    .replace(tags, function($0, $1) {
      return allowed.indexOf('<' + $1.toLowerCase() + '>') > -1 ? $0 : '';
    });
}


//debug

function kiwi_server_error(s)
{
	html('id-kiwi-msg').innerHTML =
	'Hmm, there seems to be a problem. <br> \
	The server reported the error: <span style="color:red">'+s+'</span> <br> \
	Please <a href="javascript:sendmail(\'ihpCihp-`ln\',\'server error: '+s+'\');">email me</a> the above message. Thanks!';
	visible_block('id-kiwi-msg', 1);
	visible_block('id-kiwi-container-1', 0);
	seriousError = true;
}

function kiwi_serious_error(s)
{
	html('id-kiwi-msg').innerHTML = s;
	visible_block('id-kiwi-msg', 1);
	visible_block('id-kiwi-container-1', 0);
	seriousError = true;
}

function kiwi_trace()
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
