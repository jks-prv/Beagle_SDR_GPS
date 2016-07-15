// KiwiSDR utilities
//
// Copyright (c) 2014 John Seamons, ZL/KF6VO

// wait until DOM has loaded before proceeding (browser has loaded HTML, but not necessarily images)
document.onreadystatechange = function() {
	//console.log("onreadystatechange="+document.readyState);
	//if (document.readyState == "interactive") {
	if (document.readyState == "complete") {
		kiwi_bodyonload();
	}
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

function kiwi_n2h_32(ip)
{
	var s = ip.split('.');
	return (s[3]&0xff) | ((s[2]&0xff)<<8) | ((s[1]&0xff)<<16) | ((s[0]&0xff)<<24);
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
	return kiwi_h2n_32(ip,0)+'.'+kiwi_h2n_32(ip,1)+'.'+kiwi_h2n_32(ip,2)+'.'+kiwi_h2n_32(ip,3);
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


// HTML helpers

var dummy_elem = {};

// return document element reference either by id or name
function html_id(id_or_name)
{
	var i;
	var el = document.getElementById(id_or_name);
	if (el == null) {
		el = document.getElementsByName(id_or_name);		// 'name=' is deprecated
		if (el != null) el = el[0];	// use first from array
	}
	if (el == null) {
		el = document.getElementsByClassName(id_or_name);
		if (el != null) el = el[0];	// use first from array
	}
	if (el == null && id_or_name != 'id-msg-status')
		console.log('html_id: \"'+ id_or_name +'\" is null');
	return el;
}

function html_name2id(name)
{
	return 'id-'+ name;
}

function html_idname(id)
{
	return html_id(html_name2id(id));
}

function html(id_or_name)
{
	var el = html_id(id_or_name);
	var debug;
	try {
		debug = el.value;
	} catch(ex) {
		console.log("html('"+id_or_name+"')="+el+" FAILED");
		//console.log("FAILED: id_or_name="+id_or_name);
		//kiwi_trace();
	}
	if (el == null) el = dummy_elem;		// allow failures to proceed, e.g. assignments to innerHTML
	return el;
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

function kiwi_button(v, oc)
{
	return '<button class="cl-kiwi-button" type="button" value="'+ v +'" onclick="'+ oc +'">'+ v +'</button>';
}

// Get function from string, with or without scopes (by Nicolas Gauthier)
// stackoverflow.com/questions/912596/how-to-turn-a-string-into-a-javascript-function-call
// returns null if scope doesn't exist, undefined if element doesn't exist in scope
function getVarFromString(string)
{
	var scope = window;
	var scopeSplit = string.split('.');
	for (i = 0; i < scopeSplit.length - 1; i++) {
		scope = scope[scopeSplit[i]];
		if (scope == undefined) return null;
	}
	
	var r = scope[scopeSplit[scopeSplit.length - 1]];
	//console.log('getVarFromString s='+ string +' r='+ r);
	return r;
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

function kiwi_GETrequest_param(request, name, value)
{
  var input = document.createElement("input");
  input.type = "hidden";
  input.name = name;
  input.value = value;
  request.appendChild(input);

  if (kiwi_GETrequest_debug) console.log('kiwi_GETrequest_param: '+ name +'='+ value);
}

// only works on cross-domains if server sends a CORS access wildcard

var ajax_state = { DONE:4 };
var ajax_cb_called;

function kiwi_ajax(url, doEval)	// , callback, timeout, jsonp_cb, readyFunc
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

	var callback = (arguments.length > 2)? arguments[2] : null;
	var timeout = (arguments.length > 3)? arguments[3] : 0;
	var jsonp_cb = (arguments.length > 4)? arguments[4] : null;
	var retry = false;
	var retryFunc = (arguments.length > 5)? arguments[5] : null;
	
	var epoch = (new Date).getTime();

	ajax.onreadystatechange = function() {
		if (ajax.readyState != ajax_state.DONE)
			return;

		var response = ajax.responseText.toString();
		//console.log('AJAX: '+url+' RESPONSE: <'+ response +'>');
		
		if (timeout) {
			kiwi_clearTimeout(ajax_timer);
		}

		if (response.substr(0,15) == "Try again later") {
			console.log("Try again later "+ typeof(retryFunc));
			retry = true;
		}

		if (doEval && !retry) {
			if (callback) {
				// even though we aborted in timeout routine this still gets called
				// so check if still under timeout threshold
				//var elapsed = (new Date).getTime() - epoch;
				//if (elapsed < (timeout - 10)) {
				if (!ajax_cb_called) {
					callback(false, url, response);
				} else {
					//console.log("didn't call cb");
				}
			} else {
				// sometimes the JSONP form is missing
				if (response != "" && jsonp_cb && response.charAt(0) == '{') {
					console.log('AJAX JSONP response form missing for '+ jsonp_cb +'()');
					response = jsonp_cb +'('+ response +');';
				}
				
				try {
					eval(response);
				} catch(ex) {
					console.log('AJAX EVAL FAIL: '+ url +' RESPONSE: <'+ response +'> EX: '+ ex);
					console.log(ex.stack);
				}
			}
		}
		
		ajax.abort();
		delete ajax;
	}

	//console.log("AJAX url "+ url);
	ajax_cb_called = false;
	ajax.open("GET", url, true);
	
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
