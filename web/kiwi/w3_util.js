// Copyright (c) 2016 John Seamons, ZL/KF6VO

/*
	Useful stuff:

	in w3.css:
		w3-show-inline-block
		
		w3-section: margin TB 16px
		w3-container: padding TB 0.01em LR 16px
		w3-col: float left, width 100%
		w3-padding: T 8px, B 16px
		w3-row-padding: LR 8px
		w3-margin: TBLR 16px

	in w3_ext.css:
		w3-vcenter
		w3-override-(colors)

	id="foo" on...(e.g. onclick)=func(this.id)
	
*/


////////////////////////////////
// util
////////////////////////////////

function w3_strip_quotes(s)
{
	if ((typeof s == "string") && (s.indexOf('\'') != -1 || s.indexOf('\"') != -1))
		return s.replace(/'/g, '').replace(/"/g, '') + ' [quotes stripped]';
	return s;
}

// a single-argument call that silently continues if func not found
function w3_call(func, arg0, arg1)
{
	try {
		var f = getVarFromString(func);
		//console.log('w3_call: '+ func +'() = '+ (typeof f));
		if (typeof f == "function")
			f(arg0, arg1);
	} catch(ex) {
		console.log('w3_call '+ func +'()');
		console.log(ex);
		//console.log('w3_call '+ func +'(): '+ ex.toString());
		//console.log(ex.stack);
	}
}


////////////////////////////////
// HTML
////////////////////////////////

// allow an element or element-id to be used
function w3_el_id(el_id)
{
	if (typeof el_id == "string")
		return html_id(el_id);
	return (el_id);
}

function w3_field_select(el_id, focus_blur)
{
	var el = w3_el_id(el_id);
	el = (el && typeof el.select == 'function')? el : null;

	//var id = (typeof el_id == 'object')? el_id.id : el_id;
	//console.log('w3_field_select '+ id +' '+ el +' f/b='+ focus_blur +' v='+ (el? el.value:null));

	if (focus_blur && el) {
		el.focus();
		el.select();
	} else
	if (!focus_blur && el) {
		el.blur();
	}
}

// add, remove or check presence of class attribute
function w3_class(el_id, attr)
{
	var el = w3_el_id(el_id);
	if (!w3_isClass(el, attr))		// don't add it more than once
		el.className += ' '+ attr;
}

function w3_unclass(el_id, attr)
{
	var el = w3_el_id(el_id);
	el.className = el.className.replace(attr, "");		// nothing bad happens if it isn't found
}

function w3_isClass(el_id, attr)
{
	var el = w3_el_id(el_id);
	var cname = el.className;
	return (!cname || cname.indexOf(attr) == -1)? 0:1;
}

function w3_show_block(el_id)
{
	var el = w3_el_id(el_id);
	w3_unclass(el, 'w3-hide');
	w3_class(el, 'w3-show-block');
}

function w3_show_inline(el_id)
{
	var el = w3_el_id(el_id);
	w3_unclass(el, 'w3-hide');
	w3_class(el, 'w3-show-inline-block');
}

function w3_hide(el_id)
{
	var el = w3_el_id(el_id);
	w3_unclass(el, 'w3-show-block');
	w3_unclass(el, 'w3-show-inline-block');
	w3_class(el, 'w3-hide');
}

// our standard for confirming (highlighting) a control action (e.g.button push)
var w3_highlight_time = 250;
var w3_highlight_color = ' w3-override-green';

function w3_highlight(el_id)
{
	var el = w3_el_id(el_id);
	//console.log('w3_highlight '+ el.id);
	w3_class(el, w3_highlight_color);
}

function w3_unhighlight(el_id)
{
	var el = w3_el_id(el_id);
	//console.log('w3_unhighlight '+ el.id);
	w3_unclass(el, w3_highlight_color);
}

function w3_isHighlighted(el_id)
{
	var el = w3_el_id(el_id);
	return w3_isClass(el, w3_highlight_color);
}

var w3_flag_color = 'w3-override-yellow';

function w3_flag(path)
{
	w3_class(html_idname(path), w3_flag_color);
}

function w3_unflag(path)
{
	w3_unclass(html_idname(path), w3_flag_color);
}

function w3_color(el_id, color)
{
	var el = w3_el_id(el_id);
	var prev = el.style.color
	if (color != undefined && color != null) el.style.color = color;
	return prev;
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

function w3_set_value(path, val)
{
	var el = html_idname(path);
	el.value = val;
}

function w3_set_decoded_value(path, val)
{
	var el = html_idname(path);
	el.value = decodeURIComponent(val);
}

function w3_get_value(path)
{
	var el = html_idname(path);
	return el.value;
}


////////////////////////////////
// nav
////////////////////////////////

function w3_toggle(id)
{
	var el = html_idname(id);
	if (w3_isClass(el, 'w3-show-block')) {
		w3_unclass(el, 'w3-show-block');
		//console.log('w3_toggle: hiding '+ id);
	} else {
		w3_class(el, 'w3-show-block');
		//console.log('w3_toggle: showing '+ id);
	}
}

function w3_click_show(grp, next_id, focus_blur)
{
	//console.log('w3_click_show: '+ next_id);
	var cur_id = null;
	var next_el = null;
	var els = document.getElementsByClassName('grp-'+ grp);

	for (var i = 0; i < els.length; i++) {
		var el = els[i];
		//console.log('w3_click_show: consider '+ el.id +' '+ el.className);
		if (w3_isClass(el, 'w3-current')) {
			cur_id = el.id;
			w3_unclass(el, 'w3-current');
			//console.log('w3_click_show: *current*');
		}
		if (el.id == next_id)
			next_el = el;
	}

	if (cur_id) {
		w3_toggle(cur_id);
		if (focus_blur) w3_call(cur_id +'_blur', cur_id);
	}

	if (next_el)
		w3_class(next_el, 'w3-current');

	w3_toggle(next_id);
	if (focus_blur) w3_call(next_id +'_focus', next_id);
}

function w3_anchor(grp, id, text, _class, _default, focus_blur)
{
	if (_default == true) _class += 'w3-current';
	var oc = 'onclick="w3_click_show('+ q(grp) +','+ q(id) +','+ focus_blur +')"';
	var s = '<a id="'+ id +'" class="grp-'+ grp +' '+ _class +'" href="javascript:void(0)" '+ oc +'>'+ text +'</a> ';
	//console.log('w3_anchor: '+ s);
	return s;
}

function w3_nav(grp, id, text, _class, _default)
{
	var s = '<li>'+ w3_anchor(grp, id, text, _class, _default, true)  +'</li> ';
	//console.log('w3_nav: '+ s);
	return s;
}

function w3_navdef(grp, id, text, _class)
{
	// must wait until instantiated before manipulating 
	setTimeout(function() {
		w3_toggle(id);
		var el = html_idname(id);
	}, w3_highlight_time);
	
	return w3_nav(grp, id, text, _class, true);
}


////////////////////////////////
// labels
////////////////////////////////

function w3_label(label, path, label_ext)
{
	var s = label? ('<label id="id-'+ path +'-label" class=""><b>'+ label +'</b>'+
		(label_ext? label_ext:'') +'</label><br>') : '';
	return s;
}

function w3_set_label(label, path)
{
	html_idname(path +'-label').innerHTML = '<b>'+ label +'</b>';
}


////////////////////////////////
// buttons: single & radio
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
		if (w3_isHighlighted(el[i]))
			idx = i;
	}
	w3_check_restart(ev.currentTarget);

	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
		getVarFromString(save_cb)(btn_grp, idx);
	}
}

function w3_radio_btn(btn_grp, text, _default, save_cb)
{
	var prop = (arguments.length > 4)? arguments[4] : null;
	var _class = ' cl-'+ btn_grp + (_default? (' '+ w3_highlight_color) : '') + (prop? (' '+prop) : '');
	var oc = 'onclick="w3_radio_click(event, '+ q(btn_grp) +', '+ q(save_cb) +')"';
	var s = '<button class="w3-btn w3-light-grey'+ _class +'" '+ oc +'>'+ text +'</button> ';
	//console.log(s);
	return s;
}

var w3_btn_grp_uniq = 0;

function w3_btn(text, save_cb)
{
	var s;
	var prop = (arguments.length > 2)? arguments[2] : null;
	if (prop)
		s = w3_radio_btn('w3-btn-grp-'+ w3_btn_grp_uniq.toString(), text, 0, save_cb, prop);
	else
		s = w3_radio_btn('w3-btn-grp-'+ w3_btn_grp_uniq.toString(), text, 0, save_cb);
	w3_btn_grp_uniq++;
	//console.log(s);
	return s;
}


////////////////////////////////
// input
////////////////////////////////

function w3_input_change(path, save_cb)
{
	var el = html_idname(path);
	w3_check_restart(el);
	
	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
		//el.select();
		w3_highlight(el);
		setTimeout(function() {
			w3_unhighlight(el);
		}, w3_highlight_time);
		getVarFromString(save_cb)(path, el.value);
	}
}

function w3_input(label, path, val, save_cb, placeholder, prop, label_ext)
{
	if (val == null)
		val = '';
	else
		val = w3_strip_quotes(val);
	var oc = 'onchange="w3_input_change('+ q(path) +', '+ q(save_cb) +')" ';
	var label_s = w3_label(label, path, label_ext);
	var s =
		label_s +
		'<input id="id-'+ path +'" class="w3-input w3-border w3-hover-shadow ' +
		(prop? prop : '') +'" value=\''+ val +'\' ' +
		'type="text" '+ oc +
		(placeholder? ('placeholder="'+ placeholder +'"') : '') +'>' +
	'';
	//if (label == 'Title') console.log(s);
	return s;
}


////////////////////////////////
// select
////////////////////////////////

function w3_select_change(ev, path, save_cb)
{
	var el = ev.currentTarget;
	w3_check_restart(el);

	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
		getVarFromString(save_cb)(path, el.value);
	}
}

function w3_select(label, title, path, sel, opts, save_cb, label_ext)
{
	var label_s = w3_label(label, path, label_ext);
	var s =
		label_s +
		'<select id="id-'+ path +'" onchange="w3_select_change(event, '+ q(path) +', '+ q(save_cb) +')">' +
			'<option value="0" '+ ((sel == 0)? 'selected':'') +' disabled>' + title +'</option>';
			var keys = Object.keys(opts);
			for (var i=0; i < keys.length; i++) {
				s += '<option value="'+ (i+1) +'" '+ ((i+1 == sel)? 'selected':'') +'>'+ opts[keys[i]] +'</option>';
			}
	s += '</select>';

	// run the callback after instantiation with the initial control value
	if (save_cb)
		setTimeout(function() {
			//console.log('w3_select: initial callback: '+ save_cb +'('+ q(path) +', '+ sel +')');
			w3_call(save_cb, path, sel);
		}, 500);

	//console.log(s);
	return s;
}

function w3_select_enum(path, func)
{
	var sel = html_idname(path);
	//console.log('w3_select_enum '+ path +' len='+ sel.children.length);
	for (var i=0; i < sel.children.length; i++) {
		var c = sel.children[i];
		func(c);
	}
}

function w3_select_value(path, idx)
{
	html_idname(path).value = idx;
}


////////////////////////////////
// slider
////////////////////////////////

function w3_slider_change(ev, path, save_cb)
{
	var el = ev.currentTarget;
	w3_check_restart(el);
	
	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
		getVarFromString(save_cb)(path, el.value);
	}
}

function w3_slider(label, path, val, min, max, save_cb, placeholder, label_ext)
{
	if (val == null)
		val = '';
	else
		val = w3_strip_quotes(val);
	var oc = 'oninput="w3_slider_change(event, '+ q(path) +', '+ q(save_cb) +')" ';
	var label_s = w3_label(label, path, label_ext);
	var s =
		label_s +
		'<input id="id-'+ path +'" class="" value=\''+ val +'\' ' +
		'type="range" min="'+ min +'" max="'+ max +'" step="1" '+ oc +
		(placeholder? ('placeholder="'+ placeholder +'"') : '') +'>' +
	'';
	//console.log(s);
	return s;
}


////////////////////////////////
// standard callbacks
////////////////////////////////

function w3_num_cb(el, val)
{
	var v = parseFloat(val);
	if (isNaN(v)) v = 0;
	//console.log('w3_num_cb: el='+ el +' val='+ val +' v='+ v);
	setVarFromString(el, v);
}

function w3_string_cb(el, val)
{
	//console.log('w3_string_cb: el='+ el +' val='+ val);
	setVarFromString(el, val.toString());
}


////////////////////////////////
// containers
////////////////////////////////

function w3_idiv(prop)
{
	var narg = arguments.length;
	var s = '<div class="w3-idiv '+ prop +'">';
		for (var i=1; i < narg; i++) {
			s += arguments[i];
		}
	s += '</div>';
	//console.log(s);
	return s;
}

function w3_divs(prop_outer, prop_inner)
{
	var narg = arguments.length;
	var s = '<div class="'+ prop_outer +'">';
		for (var i=2; i < narg; i++) {
			s += '<div class="'+ prop_inner +'">'+ arguments[i] + '</div>';
		}
	s += '</div>';
	//console.log(s);
	return s;
}

function w3_half(prop_row, prop_col, left, right, prop_left, prop_right)
{
	if (prop_left == undefined) prop_left = '';
	if (prop_right == undefined) prop_right = '';

	var s =
	'<div class="w3-row '+ prop_row +'">' +
		'<div class="w3-col w3-half '+ prop_col + prop_left +'">' +
			left +
		'</div>' +
		'<div class="w3-col w3-half '+ prop_col + prop_right +'">' +
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
