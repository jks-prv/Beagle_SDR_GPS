// Copyright (c) 2016 John Seamons, ZL/KF6VO

/*
	Useful stuff:

	in w3.css:
		w3-show-inline-block
		
		w3-section: margin top/bottom 16px
		w3-container: padding 16px (TBLR)
		w3-col: float left, width 100%
		w3-padding: V 8px, H 16px

	in w3_ext.css:
		w3-vcenter
		w3-override-(colors)

	id="foo" on...(e.g. onclick)=func(this.id)
	
*/


////////////////////////////////
// util
////////////////////////////////

function w3_class(el, attr)
{
	if (!w3_isClass(el, attr))		// don't add it more than once
		el.className += attr;
}

function w3_unclass(el, attr)
{
	el.className = el.className.replace(attr, "");		// nothing bad happens if it isn't found
}

function w3_isClass(el, attr)
{
	var cname = el.className;
	return (!cname || cname.indexOf(attr) == -1)? 0:1;
}

var w3_highlight_color = ' w3-override-green';

function w3_highlight(el)
{
	//console.log('w3_highlight '+ el.id);
	w3_class(el, w3_highlight_color);
}

function w3_unhighlight(el)
{
	//console.log('w3_unhighlight '+ el.id);
	w3_unclass(el, w3_highlight_color);
}

function w3_isHighlight(el)
{
	return w3_isClass(el, w3_highlight_color);
}

function w3_call(func, arg)
{
	try {
		var call = func +'('+ quoted(arg) +')';
		//console.log('w3_call eval: '+ call);
		eval(call);
	} catch(ex) {
		//console.log('w3_call no func: '+ call);
	}
}

function w3_check_restart(el)
{
	do {
		if (w3_isClass(el, 'w3-restart')) {
			w3_restart_cb();
			break;
		}
		el = el.parentNode;
	} while (el);
}


////////////////////////////////
// nav
////////////////////////////////

var w3_cur = 0;

function w3_toggle(id)
{
	//console.log('w3_toggle: '+ id);
	var el = html_idname(id);
	if (w3_isClass(el, ' w3-show')) {
		w3_unclass(el, ' w3-show');
	} else {
		w3_class(el, ' w3-show');
	}
}

function w3_click_show(next)
{
	//console.log('w3_click_show: '+ next);
	if (w3_cur) {
		w3_toggle(w3_cur);
		w3_call(w3_cur +'_blur', w3_cur);
	}

	w3_cur = next;
	w3_toggle(next);
	w3_call(next +'_focus', next);
}

function w3_navdef(id, text, _class)
{
	setTimeout('w3_toggle('+ quoted(id) +')', 250);
	w3_cur = id;
	return w3_nav(id, text, _class);
}

function w3_nav(id, text, _class)
{
	var oc = 'onclick="w3_click_show('+ quoted(id) +')"';
	var s = '<li><a class="'+ _class +'" href="#" '+ oc +'>'+ text +'</a></li> ';
	//console.log('w3_nav: '+ s);
	return s;
}


////////////////////////////////
// radio
////////////////////////////////

function w3_radio_unhighlight(btn_grp)
{
	var el = document.getElementsByClassName('cl-'+ btn_grp);
	for (var i = 0; i < el.length; i++) {
		w3_unhighlight(el[i]);
	}
}

function w3_radio_click(ev, btn_grp, save_cb)
{
	w3_radio_unhighlight(btn_grp);
	w3_highlight(ev.currentTarget);

	var el = document.getElementsByClassName('cl-'+ btn_grp);
	var idx = -1;
	for (var i = 0; i < el.length; i++) {
		if (w3_isHighlight(el[i]))
			idx = i;
	}
	w3_check_restart(ev.currentTarget);

	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
		//console.log('eval: '+ save_cb +'('+ quoted(btn_grp) +', '+ idx +')');
		eval(save_cb +'('+ quoted(btn_grp) +', '+ idx +')');
	}
}

function w3_radio_btn(btn_grp, text, def, save_cb)
{
	var prop = (arguments.length > 4)? arguments[4] : null;
	var _class = ' cl-'+ btn_grp + (def? w3_highlight_color : '') + (prop? (' '+prop) : '');
	var oc = 'onclick="w3_radio_click(event, '+ quoted(btn_grp) +', '+ quoted(save_cb) +')"';
	return '<button class="w3-btn w3-light-grey'+ _class +'" '+ oc +'>'+ text +'</button> ';
}


////////////////////////////////
// input
////////////////////////////////

function w3_input_change(ev, id_full, save_cb)
{
	var dot = id_full.lastIndexOf(".");
	var id = (dot == -1)? id_full : id_full.substr(dot+1);
	var el = html_idname(id);
	w3_check_restart(el);
	
	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
		//el.select();
		w3_highlight(el);
		setTimeout('w3_unhighlight(html_idname('+ quoted(id) +'))', 500);
		eval(save_cb +'('+ quoted(id_full) +', '+ quoted(el.value) +')');
	}
}

function w3_input(label, el, val, size, save_cb, placeholder)
{
	var oc = 'onchange="w3_input_change(event, '+ quoted(el) +', '+ quoted(save_cb) +')"';
	var id = el;
	var dot = el.lastIndexOf(".");
	if (dot != -1)
		id = el.substr(dot+1);
	var label_s = label? '<label class="w3-label"><b>'+ label +'</b></label>' : '';
	var s =
		'<p>' +
			label_s +
			'<input id="id-'+ id +'" class="w3-input w3-border w3-hover-shadow" value="'+ val +'"' +
			' size="'+ size +'" type="text" ' + oc +
			(placeholder? (' placeholder="'+ placeholder +'"') : '') +'>' +
		'</p>' +
	'';
	//console.log(s);
	return s;
}


////////////////////////////////
// containers
////////////////////////////////

function w3_div(prop, full)
{
	var s =
	'<div class="'+ prop +'">' +
		full +
	'</div>' +
	'';
	//console.log(s);
	return s;
}

function w3_half(prop_row, prop_col, left, right)
{
	var s =
	'<div class="w3-row '+ prop_row +'">' +
		'<div class="w3-col w3-half '+ prop_col +'">' +
			left +
		'</div>' +
		'<div class="w3-col w3-half '+ prop_col +'">' +
			right +
		'</div>' +
	'</div>' +
	'';
	//console.log(s);
	return s;
}

function w3_third(prop_row, prop_col, left, middle, right)
{
	var s =
	'<div class="w3-row '+ prop_row +'">' +
		'<div class="w3-col w3-third '+ prop_col +'">' +
			left +
		'</div>' +
		'<div class="w3-col w3-third '+ prop_col +'">' +
			middle +
		'</div>' +
		'<div class="w3-col w3-third '+ prop_col +'">' +
			right +
		'</div>' +
	'</div>' +
	'';
	//console.log(s);
	return s;
}

function w3_col_percent(prop_row, prop_col)
{
	var narg = arguments.length;
	var s = '<div class="w3-row '+ prop_row +'">';
	
	for (var i=2; i < narg; i += 2) {
		s += '<div class="w3-col '+ prop_col +'" style="width:'+ arguments[i+1] +'%">'+ arguments[i] + '</div>';
	}
	s += '</div>';
	//console.log(s);
	return s;
}
