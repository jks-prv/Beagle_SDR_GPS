// KiwiSDR utilities
//
// Copyright (c) 2014 John Seamons, ZL/KF6VO

// browsers have added includes() only relatively recently
try {
	if (!String.prototype.includes) {
		String.prototype.includes = function(str) {
			return (this.indexof(str) >= 0);
		}
	}
} catch(ex) {
	console.log("kiwi_util: String.prototype.includes");
}

//http://stackoverflow.com/questions/646628/how-to-check-if-a-string-startswith-another-string
try {
	if (!String.prototype.startsWith) {
		String.prototype.startsWith = function(str) {
			return this.indexOf(str) == 0;
		}
	}
} catch(ex) {
	console.log("kiwi_util: String.prototype.startsWith");
}

var kiwi_iOS, kiwi_OSX, kiwi_android;
var kiwi_safari, kiwi_firefox, kiwi_chrome;

// wait until DOM has loaded before proceeding (browser has loaded HTML, but not necessarily images)
document.onreadystatechange = function() {
	//console.log("onreadystatechange="+document.readyState);
	//if (document.readyState == "interactive") {
	if (document.readyState == "complete") {
		var s = navigator.userAgent;
		console.log(s);
		//alert(s);
		kiwi_iOS = (s.includes('iPhone') || s.includes('iPad'));
		kiwi_OSX = s.includes('OS X');
		kiwi_android = s.includes('Android');
		kiwi_safari = /safari\/([0-9]+)/i.exec(s)? true:false;
		kiwi_firefox = /firefox\/([0-9]+)/i.exec(s)? true:false;
		kiwi_chrome = /chrome\/([0-9]+)/i.exec(s)? true:false;
		console.log('iOS='+ kiwi_iOS +' OSX='+ kiwi_OSX +' android='+ kiwi_android + ' safari='+ kiwi_safari +' firefox='+ kiwi_firefox +' chrome='+ kiwi_chrome);
		//alert('iOS='+ kiwi_iOS +' OSX='+ kiwi_OSX +' android='+ kiwi_android + ' safari='+ kiwi_safari +' firefox='+ kiwi_firefox +' chrome='+ kiwi_chrome);

		if (typeof kiwi_check_js_version != 'undefined') {
			// done as an AJAX because needed long before any websocket available
			kiwi_ajax("/VER", 'kiwi_version_cb');
		} else {
			kiwi_bodyonload('');
		}
	}
}

var kiwi_version_fail = false;

function kiwi_version_cb(response_obj)
{
	version_maj = response_obj.maj; version_min = response_obj.min;
	//console.log('v'+ version_maj +'.'+ version_min +': KiwiSDR server');
	var s='';
	
	kiwi_check_js_version.forEach(function(el) {
		if (el.VERSION_MAJ != version_maj || el.VERSION_MIN != version_min) {
			if (kiwi_version_fail == false) {
				s = 'Your browser is trying to use incorrect versions of the KiwiSDR Javascript files.<br>' +
					'Please clear your browser cache and try again.<br><br>' +
					'v'+ version_maj +'.'+ version_min +': KiwiSDR server<br>';
				kiwi_version_fail = true;
			}
			s += 'v'+ el.VERSION_MAJ +'.'+ el.VERSION_MIN +': '+ el.file +'<br>';
		}
		//console.log('v'+ el.VERSION_MAJ +'.'+ el.VERSION_MIN +': '+ el.file);
	});

	kiwi_bodyonload(s);
}

function q(s)
{
	return '\''+ s +'\'';
}

function dq(s)
{
	return '\"'+ s +'\"';
}

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

function kiwi_inet4_d2h(inet4_str)
{
	var s = inet4_str.split('.');
	return ((s[0]&0xff)<<24) | ((s[1]&0xff)<<16) | ((s[2]&0xff)<<8) | (s[3]&0xff);
}

function kiwi_h2n_32(ip, o)
{
	var n;
	switch (o) {
		case 0: n = ip>>24; break;
		case 1: n = ip>>16; break;
		case 2: n = ip>>8; break;
		case 3: n = ip; break;
		default: n = 0; break;
	}
	return n & 0xff;
}

function kiwi_ip_str(ip)
{
	var s='';
	
	if (typeof ip === 'number')
		s = kiwi_h2n_32(ip,0)+'.'+kiwi_h2n_32(ip,1)+'.'+kiwi_h2n_32(ip,2)+'.'+kiwi_h2n_32(ip,3);
	else
	if (typeof ip === 'array')
		s = ip[3] +'.'+ ip[2] +'.'+ ip[1] +'.'+ ip[0];
	else
	if (typeof ip === 'object')
		s = ip.a +'.'+ ip.b +'.'+ ip.c +'.'+ ip.d;
	
	return s;
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

Number.prototype.toHex = function()
{
	var n = Number(this);
	return '0x'+(n>>>0).toString(16);
}

Number.prototype.toUnits = function()
{
	var n = Number(this);
	if (n < 1000) {
		return n.toString();
	} else
	if (n < 1000000) {
		return (n/1000).toFixed(1)+'k';
	} else
	if (n < 1000000000) {
		return (n/1000000).toFixed(1)+'M';
	} else {
		return n.toString();
	}
}

function kiwi_clearTimeout(timeout)
{
   try { clearTimeout(timeout); } catch(e) {};
}

function kiwi_clearInterval(interval)
{
   try { clearInterval(interval); } catch(e) {};
}

// from http://www.quirksmode.org/js/cookies.html
function createCookie(name, value, days) {
	var expires = "";
	if (days) {
		var date = new Date();
		date.setTime(date.getTime() + (days*24*60*60*1000));
		expires = "; expires="+ date.toGMTString();
	}
	//console.log('createCookie <'+ name +"="+ value + expires +"; path=/" +'>');
	document.cookie = name +"="+ value + expires +"; path=/";
}

function readCookie(name) {
	var nameEQ = name + "=";
	var ca = document.cookie.split(';');
	for (var i=0; i < ca.length; i++) {
		var c = ca[i];
		while (c.charAt(0) == ' ') c = c.substring(1, c.length);
		//console.log('readCookie '+ name +' consider <'+ c +'>');
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

function deleteCookie(cookie)
{
	var v = readCookie(cookie);
	if (v == null) return;
	createCookie(cookie, 0, -1);
}

var littleEndian = (function() {
	var buffer = new ArrayBuffer(2);
	new DataView(buffer).setInt16(0, 256, true /* littleEndian */);
	// Int16Array uses the platform's endianness.
	return new Int16Array(buffer)[0] === 256;
})();

//console.log('littleEndian='+ littleEndian);


// HTML helpers

var kiwiint_dummy_elem = {};

function html(id_or_name)
{
	var el = w3_el_id(id_or_name);
	var debug;
	try {
		debug = el.value;
	} catch(ex) {
		console.log("html('"+id_or_name+"')="+el+" FAILED");
		/**/
		if (dbgUs && dbgUsFirst) {
			//console.log("FAILED: id_or_name="+id_or_name);
			kiwi_trace();
			dbgUsFirst = false;
		}
		/**/
	}
	if (el == null) el = kiwiint_dummy_elem;		// allow failures to proceed, e.g. assignments to innerHTML
	return el;
}

function px(num)
{
	return num.toString() +'px';
}

function css_style(el, prop)
{
	var style = getComputedStyle(el, null);
	//console.log('css_style id='+ el.id);
	//console.log(style);
	var val = style.getPropertyValue(prop);
	return val;
}

function css_style_num(el, prop)
{
	var v = parseInt(css_style(el, prop));
	if (isNaN(v)) v = 0;
	//console.log('css_style_num '+ prop +'='+ v);
	return v;
}

function html_LR_border_pad(el)
{
	var bL = css_style_num(el, 'border-left-width');
	var bR = css_style_num(el, 'border-right-width');
	var pL = css_style_num(el, 'padding-left');
	var pR = css_style_num(el, 'padding-right');
	//console.log('html_LR_border_pad id='+el.id+' border: '+bL+','+bR);
	//console.log('html_LR_border_pad id='+el.id+' padding: '+pL+','+pR);
	return bL+bR + pL+pR;
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

function enc(s) { return s.replace(/./gi, function(c) { return String.fromCharCode(c.charCodeAt(0) ^ 3); }); }

function ignore(ev)
{
	return cancelEvent(ev);
}

function rgb(r, g, b)
{
	return 'rgb('+ Math.floor(r) +','+ Math.floor(g) +','+ Math.floor(b) +')';
}

function visible_inline(id, v)
{
	visible_type(id, v, 'inline');
}

function visible_block(id, v)
{
	visible_type(id, v, 'block');
}

function visible_inline_block(id, v)
{
	visible_type(id, v, 'inline-block');
}

function visible_type(id, v, type)
{
	var elem = html(id);
	elem.style.display = v? type:'none';
	if (v) elem.style.visibility = 'visible';
}

function kiwi_button(v, oc, id)
{
	id = (id == undefined)? '' : 'id="'+ id +'" ';
	return '<button '+ id +'class=cl-kiwi-button type="button" value="'+ v +'" onclick="'+ oc +'">'+ v +'</button>';
}

// Get function from string, with or without scopes (by Nicolas Gauthier)
// stackoverflow.com/questions/912596/how-to-turn-a-string-into-a-javascript-function-call
// returns null if var doesn't exist in last scope, throws error if scope level missing
function getVarFromString(path)
{
	var scope = window;
	var scopeSplit = path.split('.');
	for (i = 0; i < scopeSplit.length - 1; i++) {
		var scope_name = scopeSplit[i];
		scope = scope[scope_name];
		if (typeof scope == 'undefined') {
			console.log('getVarFromString: NO SCOPE '+ path +' scope_name='+ scope_name);
			throw 'no scope';
		}
	}
	
	var r = scope[scopeSplit[scopeSplit.length - 1]];
	//console.log('getVarFromString: s='+ path +' r='+ r);
	return r;	// can be null or undefined
}

// And our extension for setting a var (i.e. object, prim) value.
// Create object at each scope level if required before final variable set.
function setVarFromString(string, val)
{
	var scope = window;
	var scopeSplit = string.split('.');
	for (i = 0; i < scopeSplit.length - 1; i++) {
		//console.log('setVarFromString '+ i +' '+ scopeSplit[i] +'='+ scope[scopeSplit[i]]);
		if (scope[scopeSplit[i]] == undefined) {
			scope[scopeSplit[i]] = { };
		}
		scope = scope[scopeSplit[i]];
	}
	
	scope[scopeSplit[scopeSplit.length - 1]] = val;
}


////////////////////////////////
// cross-domain GET
////////////////////////////////

// http://stackoverflow.com/questions/298745/how-do-i-send-a-cross-domain-post-request-via-javascript
var kiwi_GETrequest_debug = false;

function kiwi_GETrequest(id, url)
{
  // Add the iframe with a unique name
  var iframe = document.createElement("iframe");
  var uniqueString = id +'_'+ (new Date()).toUTCString().substr(17,8);
  document.body.appendChild(iframe);
  iframe.style.display = "none";
  iframe.contentWindow.name = uniqueString;

  // construct a form with hidden inputs, targeting the iframe
  var form = document.createElement("form");
  form.target = uniqueString;
  form.action = url;
  form.method = "GET";
  
  if (kiwi_GETrequest_debug) console.log('kiwi_GETrequest: '+ uniqueString);
  return form;
}

function kiwi_GETrequest_submit(form, debug)
{
	if (debug) {
		console.log('kiwi_GETrequest_submit: DEBUG, NO SUBMIT');
	} else {
		document.body.appendChild(form);
		form.submit();
		if (kiwi_GETrequest_debug) console.log('kiwi_GETrequest_submit: SUBMITTED');
	}
}

function kiwi_GETrequest_param(form, name, value)
{
  var input = document.createElement("input");
  input.type = "hidden";
  input.name = name;
  input.value = value;
  form.appendChild(input);

  if (kiwi_GETrequest_debug) console.log('kiwi_GETrequest_param: '+ name +'='+ value);
}


////////////////////////////////
// AJAX
////////////////////////////////

// only works on cross-domains if server sends a CORS access wildcard

var ajax_state = { DONE:4 };
var ajax_cb_called;

function kiwi_ajax(url, callback, timeout, retryFunc)
{
	kiwi_ajax_prim('GET', null, url, callback, timeout, retryFunc);
}

function kiwi_ajax_send(data, url, callback, timeout, retryFunc)
{
	kiwi_ajax_prim('PUT', data, url, callback, timeout, retryFunc);
}

function kiwi_ajax_prim(method, data, url, callback, timeout, retryFunc)
{
	var ajax, ajax_timer;
	
	//try {
	//	ajax = new window.XDomainRequest();
	//} catch(ex) {
		try {
			ajax = new XMLHttpRequest();
		} catch(ex) {
			try {
				ajax = new ActiveXObject("Msxml2.XMLHTTP");
			} catch(ex) {
				try {
					ajax = new ActiveXObject("Microsoft.XMLHTTP");
				} catch(ex) {
					return false;
				}
			}
		}
	//}

	if (typeof callback != 'string') {
		throw 'kiwi_ajax: callback not a string';
	}

	if (timeout == undefined) timeout = 0;
	var retry = false;
	if (retryFunc == undefined) retryFunc = null;
	
	var epoch = (new Date).getTime();

	ajax.onreadystatechange = function() {
		if (ajax.readyState != ajax_state.DONE)
			return false;

		var response = ajax.responseText.toString();
		if (response == null) response = '';
		//console.log('AJAX '+ url +' cb='+ callback);
		//console.log('AJAX '+ url +' cb='+ callback +' RESPONSE: <'+ response +'>');
		
		if (timeout) {
			kiwi_clearTimeout(ajax_timer);
		}

		if (response.startsWith() == "Try again later") {
			console.log("Try again later "+ typeof(retryFunc));
			retry = true;
		}

		if (!retry) {
			if (!response || response.length == 0) {
				console.log('AJAX zero length response: '+ url);
			} else
			if (response.startsWith('404')) {
				console.log('AJAX 404: '+ url);
			} else
			if (response.charAt(0) != '{' && response.charAt(0) != '[') {
				console.log("AJAX: response didn't begin with JSON '{' or '[' ? "+ response);
			} else {
				try {
					var obj = JSON.parse(response);
					//console.log('### AJAX JSON ###');
					//console.log(obj);
					w3_call(callback, obj);		// response can be empty
				} catch(ex) {
					console.log("AJAX response JSON.parse failed: <"+ response +'>');
					console.log(ex);
				}
			}
		}
		
		ajax.abort();
		delete ajax;
	}

	//console.log('AJAX URL '+ url);
	ajax_cb_called = false;
	ajax.open(method, url, true);
	
	/*
	if (timeout) {
		ajax_timer = setTimeout(function(ev) {
			if (callback) {
				ajax_cb_called = true;
				callback(true, url, "");
			}
			ajax.abort();
			delete ajax;
		}, timeout);
	}
	*/
	
	ajax.send(data);
	return true;
}

function kiwi_is_iOS()
{
	return kiwi_iOS;
}

function kiwi_isOSX()
{
	return kiwi_OSX;
}

function kiwi_isAndroid()
{
	return kiwi_android;
}

function kiwi_isMobile()
{
	return (kiwi_isAndroid() || kiwi_is_iOS());
}

function kiwi_isSafari()
{
	return kiwi_safari;
}

function kiwi_isFirefox()
{
	return kiwi_firefox;
}

function kiwi_isChrome()
{
	return kiwi_chrome;
}

function kiwi_scrollbar_width()
{
	if (kiwi_isOSX()) return 10;		// OSX/iOS browser scrollbars are all narrower
	return 15;
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

// from http://jsfiddle.net/gFnw9/12/
function kiwi_pie(id, size, unfilled_color, filled_color) {
	var size2 = size*2;
	var s =
		'<svg width="'+ size2 +'" height="'+ size2 +'" viewbox="0 0 '+ size2 +' '+ size2 + '">' +
			'<circle cx="'+ size +'" cy="'+ size +'" r="'+ size +'" fill="'+ unfilled_color +'" />'+
			'<path id="'+ id +'" style="fill:'+ filled_color +'" transform="translate('+ size +', '+ size +')" />'+
		'</svg>';
	return s;
}

function kiwi_draw_pie(id, size, filled) {
	var alpha = 360 * filled;
	var r = alpha * Math.PI / 180,
	x = Math.sin(r) * size,
	y = Math.cos(r) * -size,
	mid = (alpha >= 180) ? 1:0;
	
	if (alpha == 360) { mid = 1; x = -0.1; y = -size; }
	var animate = 'M 0 0 v '+ (-size) +' A '+ size +' '+ size +' 1 '+ mid +' 1 '+  x  +' '+  y  +' z';
	html(id).setAttribute('d', animate);
};

var enc = function(s) { return s.replace(/./gi, function(c) { return String.fromCharCode(c.charCodeAt(0) ^ 3); }); }

var sendmail = function (to, subject) {
	var s = "mailto:"+ enc(to) + ((typeof subject != "undefined")? ('?subject='+subject):'');
	console.log(s);
	window.location.href = s;
}


////////////////////////////////
// web sockets
////////////////////////////////

// used by common routines called from admin code
function msg_send(s)
{
	if (typeof ws_fft != 'undefined') fft_send(s); else ext_send(s);
}

var wsockets = [];

function open_websocket(stream, open_cb, open_cb_param, msg_cb, recv_cb, error_cb)
{
	if (!("WebSocket" in window) || !("CLOSING" in WebSocket)) {
		console.log('WEBSOCKET TEST');
		kiwi_serious_error("Your browser does not support WebSocket, which is required for OpenWebRX to run. <br> Please use an HTML5 compatible browser.");
		return null;
	}

	var ws_url;			// replace http:// with ws:// on the URL that includes the port number
	try {
		ws_url = window.location.origin.split("://")[1];
	} catch(ex) {
		ws_url = this.location.href.split("://")[1];
	}
	ws_url = 'ws://'+ ws_url +'/'+ timestamp +'/'+ stream;
	
	//console.log('open_websocket '+ ws_url);
	var ws = new WebSocket(ws_url);
	wsockets.push( { 'ws':ws, 'name':stream } );
	ws.up = false;
	ws.stream = stream;
	
	ws.open_cb = open_cb;
	ws.open_cb_param = open_cb_param;
	ws.msg_cb = msg_cb;
	ws.recv_cb = recv_cb;
	ws.error_cb = error_cb;

	// There is a delay between a "new WebSocket()" and it being able to perform a ws.send(),
	// so must wait for the ws.onopen() to occur before sending any init commands.
	ws.onopen = function() {
		ws.up = true;

		if (ws.open_cb)
			ws.open_cb(ws.open_cb_param);
	};

	ws.onmessage = function(evt) {
		on_ws_recv(evt, ws);
	};

	ws.onclose = function(evt) {
		console.log('WS-CLOSE: '+ ws.stream);
	};
	
	ws.binaryType = "arraybuffer";

	ws.onerror = function(evt) {
		if (ws.error_cb)
			ws.error_cb(evt, ws);
	};
	
	return ws;
}

var kiwi_flush_recv_input = true;

function recv_websocket(ws, recv_cb)
{
	ws.recv_cb = recv_cb;
	if (recv_cb == null)
		kiwi_flush_recv_input = true;
}

function on_ws_recv(evt, ws)
{
	var data = evt.data;
	
	if (!(data instanceof ArrayBuffer)) {
		console.log("on_ws_recv: not an ArrayBuffer?");
		return;
	}

	//var s = arrayBufferToString(data);
	//if (ws.stream == 'EXT') console.log('on_ws_recv: <'+ s +'>');

	var firstChars = getFirstChars(data,3);
	//divlog("on_ws_recv: "+firstChars);

	if (firstChars == "CLI") {
		//var stringData = arrayBufferToString(data);
		//if (stringData.substring(0,16) == "CLIENT DE SERVER")
			//divlog("Acknowledged WebSocket connection: "+stringData);
		return;
	} else
	
	if (firstChars == "MSG") {
		var stringData = arrayBufferToString(data);
		params = stringData.substring(4).split(" ");
	
		//if (ws.stream == 'EXT') console.log('>>> '+ ws.stream +': msg_cb='+ (typeof ws.msg_cb) +' '+ params.length +' '+ stringData);
		for (var i=0; i < params.length; i++) {
			param = params[i].split("=");
			
			if (ws.stream == 'EXT' && param[0] == 'EXT-STOP-FLUSH-INPUT') {
				//console.log('EXT-STOP-FLUSH-INPUT');
				kiwi_flush_recv_input = false;
			}
			
			if (kiwi_msg(param, ws) == false && ws.msg_cb) {
				//if (ws.stream == 'EXT') console.log('>>> '+ ws.stream + ': msg_cb='+ (typeof ws.msg_cb) +' '+ params[i]);
				ws.msg_cb(param, ws);
			}
		}
	} else {
		/*
		if (ws.stream == 'EXT' && kiwi_flush_recv_input == true) {
			var s = arrayBufferToString(data);
			console.log('>>> FLUSH '+ ws.stream + ': recv_cb='+ (typeof ws.recv_cb) +' '+ s);
		}
		*/
		if (ws.recv_cb && (ws.stream != 'EXT' || kiwi_flush_recv_input == false))
			ws.recv_cb(data, ws);
	}
}

// http://stackoverflow.com/questions/4812686/closing-websocket-correctly-html5-javascript
// FIXME: but doesn't work as expected due to problems with onbeforeunload on different browsers
window.onbeforeunload = function() {
	var i;
	
	//alert('onbeforeunload '+ wsockets.length);
	for (i=0; i < wsockets.length; i++) {
		var ws = wsockets[i].ws;
		if (ws) {
			//alert('window.onbeforeunload: ws close '+ wsockets[i].name);
			ws.onclose = function () {};
			ws.close();
		}
	}
};

