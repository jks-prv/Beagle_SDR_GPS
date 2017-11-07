// Copyright (c) 2016 John Seamons, ZL/KF6VO

/*

	Useful stuff:
	
	<element attribute="attribute-values ..." inline-style-attribute="properties ...">
	"styles" refers to any style-attribute="properties ..." combination

	style properties set one of three ways:
		1) in an element as above
		
		2) in a .css:
		selector
		{
			property: property-value;
		}
		
		3) assigned by DOM:
		el.style.property = property-value;
	
	priority (specificity)
	
		[inline-style attribute], [id], [class, pseudo-class, attribute], [elements]
		
		The "count" at each level is determined by the number of specifiers, hence the "specific-ness"
		e.g. ul#nav li.active a => 0,1,1,3 with 3 due to the ul, li and a
		This would have priority over 0,1,1,2
	
	select (menu) behavior:
	   If you set the background-color inline then border reverts to the undesirable beveled appearance!

	in w3.css:
		w3-show-block
		w3-show-inline-block
		
		w3-section: margin TB 16px
		w3-container: padding TB 0.01em LR 16px
		w3-row:after: content: ""; display: table; clear: both
		w3-col: float left, width 100%
		w3-padding: T 8px, B 16px
		w3-row-padding: LR 8px
		w3-margin: TBLR 16px

	in w3_ext.css:
		w3-show-inline
		w3-vcenter
		w3-override-(colors)

	id="foo" on...(e.g. onclick)=func(this.id)


	Notes about HTML/DOM:
	
	"Typically, the styles are merged, but when conflicts arise, the later declared style will generally win
	(unless the !important attribute is specified on one of the styles, in which casethat wins).
	Also, styles applied directly to an HTML element take precedence over CSS class styles."
	
	window.
		inner{Width,Height}		does not include tool/scroll bars
		outer{Width,Height}		includes tool/scroll bars
	
	element.
		client{Width,Height}		viewable only; no: border, scrollbar, margin; yes: padding
		offset{Width,Height}		viewable only; includes padding, border, scrollbars
	
	
	FIXME CLEANUPS:
	uniform instantiation callbacks
	uniform default/init control values
	preface internal routines/vars with w3int_...
	move some routines (esp HTML) out of kiwi_util.js into here?
	make all 'id-', 'cl-' use uniform
	
	antenna switch extension is a user of API:
	   w3_divs, w3_inline, w3_btn, w3_radio_btn(yes/no), w3_input_get_param, w3_string_set_cfg_cb,
	   w3_radio_unhighlight, w3_highlight_time

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
function w3_call(func, arg0, arg1, arg2)
{
	try {
		var f = getVarFromString(func);
		//console.log('w3_call: '+ func +'() = '+ (typeof f));
		if (typeof f == "function") {
			//var args = Array.prototype.slice.call(arguments);
			f(arg0, arg1, arg2);
		}
	} catch(ex) {
		console.log('w3_call: while in '+ func +'() this exception occured:');
		console.log(ex);
		//console.log('w3_call '+ func +'(): '+ ex.toString());
		//console.log(ex.stack);
	}
}


////////////////////////////////
// HTML
////////////////////////////////

// return document element reference either by id or name
function w3int_w3_el_id(id_name_class)
{
	var el = document.getElementById(id_name_class);
	if (el == null) {
		el = document.getElementsByName(id_name_class);		// 'name=' is deprecated
		if (el != null) el = el[0];	// use first from array
	}
	if (el == null) {
		el = document.getElementsByClassName(id_name_class);
		if (el != null) el = el[0];	// use first from array
	}
	return el;
}

// allow an element-obj or element-id to be used
// try id without, then with, leading 'id-'; then including cfg prefix as a last resort
function w3_el_id(el_id)
{
	if (typeof el_id == "string") {
		var el = w3int_w3_el_id(el_id);
		if (el == null) {
			el = w3int_w3_el_id('id-'+ el_id);
			if (el == null) {
				el_id = w3_add_toplevel(el_id);
				el = w3int_w3_el_id(el_id);
				if (el == null) {
					el = w3int_w3_el_id('id-'+ el_id);
				}
			}
		}
		return el;
	}
	return (el_id);
}

function w3_iterate_classname(cname, func)
{
	var els = document.getElementsByClassName(cname);
	if (els == null) return;
	for (var i=0; i < els.length; i++) {
		func(els[i], i);
	};
}

function w3_iterate_children(el_id, func)
{
	var el = w3_el_id(el_id);
	
	for (var i=0; i < el.children.length; i++) {
		var child_el = el.children[i];
		func(child_el);
	}
}

function w3_iterateDeep_children(el_id, func)
{
	var el = w3_el_id(el_id);
	
	for (var i=0; i < el.children.length; i++) {
		var child_el = el.children[i];
		func(child_el);
		if (child_el.hasChildNodes)
			w3_iterateDeep_children(child_el, func);
	}
}

// bounding box measured from the origin of parent
function w3_boundingBox_children(el_id, debug)
{
	var bbox = { x1:1e99, x2:0, y1:1e99, y2:0, w:0, h:0 };
	w3_iterateDeep_children(el_id, function(el) {
		if (el.nodeName != 'DIV' && el.nodeName != 'IMG')
			return;
		var position = css_style(el, 'position');
		if (position == 'static')
		   return;
		if (debug) console.log(el);
		//console.log(el.offsetParent);
		if (debug) console.log(el.nodeName +' el.oL='+ el.offsetLeft +' el.oW='+ el.offsetWidth +' el.oT='+ el.offsetTop +' el.oH='+ el.offsetHeight +' '+ position);

		bbox.x1 = Math.min(bbox.x1, el.offsetLeft);
		var x2 = el.offsetLeft + el.offsetWidth;
		bbox.x2 = Math.max(bbox.x2, x2);

		bbox.y1 = Math.min(bbox.y1, el.offsetTop);
		var y2 = el.offsetTop + el.offsetHeight;
		bbox.y2 = Math.max(bbox.y2, y2);
	});

	bbox.w = bbox.x2 - bbox.x1;
	bbox.h = bbox.y2 - bbox.y1;
	if (debug) console.log('BBOX x1='+ bbox.x1 +' x2='+ bbox.x2 +' y1='+ bbox.y1 +' y2='+ bbox.y2 +' w='+ bbox.w +' h='+ bbox.h);
	return bbox;
}

function w3_center_in_window(el_id)
{
	var el = w3_el_id(el_id);
	return window.innerHeight/2 - el.clientHeight/2;
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
function w3_unclass_class(el_id, unattr, attr)
{
	w3_unclass(el_id, unattr);
	w3_class(el_id, attr);
}

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

function w3_appendAllClass(cname, attr)
{
	w3_iterate_classname(cname, function(el) { el.className += ' '+ attr; });
}
	
function w3_setAllHref(cname, href)
{
	w3_iterate_classname(cname, function(el) { el.href = href; });
}

// Can't simply do "el.style.display =" for these since things like
// w3-hide are declared "!important" by w3.css
// Must actually remove/insert them from the class property.
function w3_show(el_id, display)
{
	var el = w3_el_id(el_id);
   var display = display? display : 'w3-show-inline-block';
	w3_unclass(el, 'w3-hide');
	w3_class(el, display);
}

function w3_show_block(el_id)
{
   w3_show(el_id, 'w3-show-block');
}

function w3_show_inline(el_id)
{
   w3_show(el_id, 'w3-show-inline-block');
}

function w3_hide(el_id)
{
	var el = w3_el_id(el_id);
	w3_unclass(el, 'w3-show-block');
	w3_unclass(el, 'w3-show-inline-block');
	w3_class(el, 'w3-hide');
}

function w3_show_hide(el_id, show, display)
{
   if (show) {
      w3_show(el_id, display || 'w3-show-inline-block');
   } else {
      w3_hide(el_id);
   }
}

function w3_visible(el_id, visible)
{
	var el = w3_el_id(el_id);
	el.style.visibility = visible? 'visible' : 'hidden';
}

// our standard for confirming (highlighting) a control action (e.g.button push)
var w3_highlight_time = 250;
var w3_highlight_color = ' w3-selection-green';

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
	w3_class(w3_el_id(path), w3_flag_color);
}

function w3_unflag(path)
{
	w3_unclass(w3_el_id(path), w3_flag_color);
}

function w3_color(el_id, color)
{
	var el = w3_el_id(el_id);
	var prev = el.style.color
	if (color != undefined && color != null) el.style.color = color;
	return prev;
}

function w3_check_restart_reboot(el_id)
{
	var el = w3_el_id(el_id);
	
	do {
		if (w3_isClass(el, 'w3-restart')) {
			w3_restart_cb();
			break;
		}
		if (w3_isClass(el, 'w3-reboot')) {
			w3_reboot_cb();
			break;
		}
		el = el.parentNode;
	} while (el);
}

function w3_set_value(path, val)
{
	var el = w3_el_id(path);
	el.value = val;
}

function w3_set_decoded_value(path, val)
{
	//console.log('w3_set_decoded_value: path='+ path +' val='+ val);
	var el = w3_el_id(path);
	el.value = decodeURIComponent(val);
}

function w3_get_value(path)
{
	var el = w3_el_id(path);
	return el.value;
}

function w3_add_toplevel(path)
{
	if (!path.startsWith('cfg.') && !path.startsWith('adm.'))
		return 'cfg.'+ path;
	return path;
}

function w3_not_toplevel(path)
{
	if (path.startsWith('cfg.') || path.startsWith('adm.'))
		return path.substr(path.indexOf('.') + 1);
	return path;
}

function w3_basename(path)
{
	var i = path.lastIndexOf('.');
	if (i >= 0) {
		return path.substr(i+1);
	}
	return path;
}

// psa = prop|style|attr
// => <div [class="[prop] [extra_prop]"] [style="[style] [extra_style]"] [attr] [extra_attr]>
function w3int_psa(psa, extra_prop, extra_style, extra_attr)
{
   var hasPSA = function(s) { return (s && s != '')? s : ''; };
   var needsSP = function(s) { return (s && s != '')? ' ' : ''; };
	var a = psa? psa.split('|') : [];

	var prop = hasPSA(a[0]);
	if (extra_prop) prop += needsSP(prop) + extra_prop;
	if (prop != '') prop = ' class='+ dq(prop);

	var style = hasPSA(a[1]);
	if (extra_style) style += needsSP(style) + extra_style;
	if (style != '') style = ' style='+ dq(style);

	var attr = hasPSA(a[2]);
	if (extra_attr) attr += needsSP(attr) + extra_attr;
	if (attr != '') attr = ' '+ attr;

	return prop + style + attr;
}

function w3_psa(psa, extra_prop, extra_style, extra_attr)
{
   return w3int_psa(psa, extra_prop, extra_style, extra_attr);
}

function w3int_init()
{
}


////////////////////////////////
// nav
////////////////////////////////

function w3int_toggle_show(id)
{
	var el = w3_el_id(id);
	if (!el) return;
	if (w3_isClass(el, 'w3-show-block')) {
		w3_unclass(el, 'w3-show-block');
		//console.log('w3int_toggle_show: hiding '+ id);
	} else {
		w3_class(el, 'w3-show-block');
		//console.log('w3int_toggle_show: showing '+ id);
	}
}

function w3int_click_anchor(next_id)
{
	var next_id_nav = 'id-nav-'+ next_id;		// to differentiate the nav anchor from the nav container
	var cur_name, cur_content_id = null;
	var next_el = null;

	w3_iterate_children(w3_el_id(next_id_nav).parentNode, function(el, i) {
	   //console.log('w3int_click_anchor consider: nodename='+ el.nodeName);
	   if (el.nodeName != 'A') return;
		//console.log('w3int_click_anchor consider: id='+ el.id +' next_id_nav='+ next_id_nav +' el.className="'+ el.className +'"');
		if (w3_isClass(el, 'w3-current')) {
			cur_name = el.id.substring(7);		// remove 'id-nav-' added by w3int_anchor(), replace with 'id-'
			cur_content_id = 'id-'+ cur_name;
		   //console.log('w3int_click_anchor: cur_content_id='+ cur_content_id);
			w3_unclass(el, 'w3-current');
		}
		if (el.id == next_id_nav) {
			next_el = el;
		}
	});

   // toggle visibility of current content
	if (cur_content_id) {
		w3int_toggle_show(cur_content_id);
		w3_call(cur_name +'_blur', cur_name);
	}

   // make new nav item current and visible / focused
	if (next_el) {
		w3_class(next_el, 'w3-current');
	}

	w3int_toggle_show(next_id);
	w3_call(next_id +'_focus', next_id);
}

function w3int_anchor(id, text, _class, isSelected)
{
	if (isSelected == true) _class += ' w3-current';
	var oc = 'onclick="w3int_click_anchor('+ q(id) +')"';
	
	// store id prefixed with 'id-nav-' so as not to collide with content container id prefixed with 'id-'
	// store with id= instead of a class property so it's easy to find with el.id in w3_iterate_classname()
	// href of "javascript:void(0)" instead of "#" so page doesn't scroll to top on click
	var s = '<a id="id-nav-'+ id +'" class="'+ _class +'" href="javascript:void(0)" '+ oc +'>'+ text +'</a> ';
	//console.log('w3int_anchor: '+ s);
	return s;
}

function w3_nav(id, text, _class, isSelected)
{
	return w3int_anchor(id, text, _class, isSelected);
}

function w3_navdef(id, text, _class)
{
	// must wait until instantiated before manipulating 
	setTimeout(function() {
		w3int_toggle_show(id);
	}, w3_highlight_time);
	
	return w3_nav(id, text, _class, true);
}

function w3_sidenav(id, text, _class, isSelected)
{
	return w3int_anchor(id, text, _class, isSelected);
}


////////////////////////////////
// labels
////////////////////////////////

/*
function w3_label(label, path, label_ext, label_prop)
{
	var s = (label || label_ext)? ('<label id="id-'+ path +'-label" class="'+ (label_prop? label_prop:'') +'"><b>'+ label +'</b>'+
		(label_ext? label_ext:'') +'</label><br>') : '';
	//console.log('LABEL: '+ s);
	return s;
}
*/

function w3_label(psa, text, path, extension)
{
   if (psa == '' && (!text || text == '') && (!extension || extension == '')) return '';
   path = path? ('id-'+ path +'-label') : '';
	var p = w3int_psa(psa, path, '', '');
	text = text? text : '';
	var s = '<label'+ p +'><b>'+ text +'</b>'+ (extension? extension : '') +'</label>';
	//console.log('LABEL: psa='+ psa +' text=<'+ text +'> text?='+ (text?1:0) +' s=<'+ s +'>');
	return s;
}

function w3_set_label(label, path)
{
	w3_el_id(path +'-label').innerHTML = '<b>'+ label +'</b>';
}


////////////////////////////////
// link
////////////////////////////////

function w3_link(psa, url, inner)
{
   if (!url.startsWith('http://') && !url.startsWith('https://'))
      url = 'http://'+ url;
	var p = w3int_psa(psa, '', '', 'href='+ dq(url) +' target="_blank"');
	var s = '<a'+ p +'>'+ inner +'</a>';
	//console.log(s);
	return s;
}


////////////////////////////////
// buttons: radio
////////////////////////////////

var w3_SELECTED = true;
var w3_NOT_SELECTED = false;

function w3_radio_unhighlight(path)
{
	w3_iterate_classname('cl-'+ path, function(el) { w3_unhighlight(el); });
}

function w3int_radio_click(ev, path, cb)
{
	w3_radio_unhighlight(path);
	w3_highlight(ev.currentTarget);

	var idx = -1;
	w3_iterate_classname('cl-'+ path, function(el, i) {
		if (w3_isHighlighted(el))
			idx = i;
		//console.log('w3int_radio_click consider path='+ path +' el='+ el +' idx='+ idx);
	});

	w3_check_restart_reboot(ev.currentTarget);

	// cb is a string because can't pass an object to onclick
	if (cb) {
		w3_call(cb, path, idx, /* first */ false);
	}
}

function w3_radio_btn(text, path, isSelected, save_cb, prop)
{
	var prop = (arguments.length > 4)? arguments[4] : null;
	var _class = ' cl-'+ path + (isSelected? (' '+ w3_highlight_color) : '') + (prop? (' '+prop) : '');
	var oc = 'onclick="w3int_radio_click(event, '+ q(path) +', '+ q(save_cb) +')"';
	var s = '<button class="w3-btn w3-ext-btn'+ _class +'" '+ oc +'>'+ text +'</button>';
	//console.log(s);
	return s;
}

function w3_radio_button(psa, text, path, isSelected, cb)
{
	var onclick = cb? ('onclick="w3int_btn_click(event, '+ q(path) +', 0, '+ q(cb) +')"') : '';
	var p = w3int_psa(psa, path + (isSelected? (' '+ w3_highlight_color) : '') +' w3-btn w3-ext-btn', '', onclick);
	var s = '<button '+ p +'>'+ text +'</button>';
	//console.log(s);
	return s;
}

// used when current value should come from config param
function w3_radio_btn_get_param(text, path, selected_if_val, init_val, save_cb)
{
	//console.log('w3_radio_btn_get_param: '+ path);
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	
	// set default selection of button based on current value
	var isSelected = (cur_val == selected_if_val)? w3_SELECTED : w3_NOT_SELECTED;
	return w3_radio_btn(text, path, isSelected, save_cb);
}


////////////////////////////////
// buttons: switch
////////////////////////////////

function w3_switch(text_pos, text_neg, path, isSelected, save_cb, prop)
{
	var s =
		w3_radio_btn(text_pos, path, isSelected? 1:0, save_cb, prop) +
		w3_radio_btn(text_neg, path, isSelected? 0:1, save_cb, prop);
	return s;
}


////////////////////////////////
// buttons: single
////////////////////////////////

function w3int_btn_click(ev, path, param, cb)
{
	w3_check_restart_reboot(ev.currentTarget);

	// cb is a string because can't pass an object to onclick
	if (cb) {
		w3_call(cb, path, param, /* first */ false);
	}
}

var w3int_btn_grp_uniq = 0;

// old API
function w3_btn(text, cb, prop)
{
	var path = 'id-btn-grp-'+ w3int_btn_grp_uniq.toString();
	w3int_btn_grp_uniq++;
	var prop = prop? (' '+ prop) : null;
	var _class = ' cl-'+ path + prop;
	var oc = 'onclick="w3int_btn_click(event, '+ q(path) +', 0, '+ q(cb) +')"';
	var s = '<button class="w3-btn w3-ext-btn'+ _class +'" '+ oc +'>'+ text +'</button>';
	//console.log(s);
	return s;
}

function w3_button(psa, text, cb, param)
{
	var path = 'id-btn-grp-'+ w3int_btn_grp_uniq.toString();
	w3int_btn_grp_uniq++;
	param = param? param : 0;
	var onclick = cb? ('onclick="w3int_btn_click(event, '+ q(path) +', '+ q(param) +', '+ q(cb) +')"') : '';
	var p = w3int_psa(psa, path +' w3-btn w3-round-large w3-ext-btn', '', onclick);
	var s = '<button'+ p +'>'+ text +'</button>';
	//console.log(s);
	return s;
}

function w3_button_text(text, path)
{
   var el = w3_el_id(path);
   el.innerHTML = text;
}

function w3_icon(psa, fa_icon, cb, size, color)
{
	var path = 'id-btn-grp-'+ w3int_btn_grp_uniq.toString();
	w3int_btn_grp_uniq++;
	size = size? (' font-size:'+ px(size) +';') : '';
	color = color? (' color:'+ color) : '';
	var onclick = cb? ('onclick="w3int_btn_click(event, '+ q(path) +', 0, '+ q(cb) +')"') : '';
	var p = w3int_psa(psa, path +' fa '+ fa_icon, size + color, onclick);
	var s = '<i'+ p +'></i>';
	//console.log(s);
	return s;
}


////////////////////////////////
// input
////////////////////////////////

function w3_input_change(path, save_cb)
{
	var el = w3_el_id(path);
	w3_check_restart_reboot(el);
	
	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
		//el.select();
		w3_highlight(el);
		setTimeout(function() {
			w3_unhighlight(el);
		}, w3_highlight_time);
		w3_call(save_cb, path, el.value, /* first */ false);
	}
}

function w3_input(label, path, val, save_cb, placeholder, prop, label_ext)
{
	if (val == null)
		val = '';
	else
		val = w3_strip_quotes(val);
	var oc = 'onchange="w3_input_change('+ q(path) +', '+ q(save_cb) +')" ';
	var label_s = w3_label('', label, path, label_ext);
	if (label_s != '') label_s += '<br>';
	var s =
		label_s +
		'<input id="id-'+ path +'" class="w3-input w3-border w3-hover-shadow ' +
		(prop? prop : '') +'" value=\''+ val +'\' ' +
		'type="text" '+ oc +
		(placeholder? ('placeholder="'+ placeholder +'"') : '') +'>';
	//if (label == 'Title') console.log(s);
	return s;
}

/*
function w3_input_psa(psa, label, path, val, cb, placeholder)
{
	var id = path? (' id-'+ path) : '';
	var spacing = (label != '')? (' '+ (prop? prop : 'w3-margin-T-8')) : '';
	var onchange = ' onchange="w3_input_change('+ q(path) +', '+ q(cb) +')"';
	var val = ' value='+ dq(val || '');
	var placeholder = ' placeholder='+ dq(placeholder || '');
	var p = w3int_psa(psa, 'w3-input w3-border w3-hover-shadow'+ id + spacing, '', 'type="text"');

	var s =
	   w3_divs('', '',
	      label,
		   '<input '+ p + val + placeholder + onchange'>'
		);
	//if (label == 'Title') console.log(s);
	return s;
}
*/

// used when current value should come from config param
function w3_input_get_param(label, path, save_cb, init_val, placeholder, prop, label_ext)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	cur_val = decodeURIComponent(cur_val);
	//console.log('w3_input_get_param: path='+ path +' cur_val="'+ cur_val +'" placeholder="'+ placeholder +'"');
	return w3_input(label, path, cur_val, save_cb, placeholder, prop, label_ext);
}


////////////////////////////////
// textarea
////////////////////////////////

function w3_textarea_psa(psa, label, path, val, rows, cols, cb)
{
	var id = path? (' id-'+ path) : '';
	var spacing = (label != '')? ' w3-margin-T-8' : '';
	var onchange = ' onchange="w3_input_change('+ q(path) +', '+ q(cb) +')"';
	var val = val || '';
	var p = w3int_psa(psa, 'w3-input w3-border w3-hover-shadow'+ id + spacing, '', 'rows="'+ rows +'" cols="'+ cols +'"');

	var s =
	   w3_div('',
	      label,
		   '<textarea '+ p + onchange +'>'+ val +'</textarea>'
		);
	//if (label == 'Title') console.log(s);
	return s;
}

// used when current value should come from config param
function w3_textarea_get_param(psa, label, path, rows, cols, save_cb, init_val)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	cur_val = decodeURIComponent(cur_val);
	//console.log('w3_textarea_get_param: path='+ path +' cur_val="'+ cur_val +'"');
	return w3_textarea_psa(psa, label, path, cur_val, rows, cols, save_cb);
}


////////////////////////////////
// checkbox
////////////////////////////////

function w3_checkbox_change(path, save_cb)
{
	var el = w3_el_id(path);
	w3_check_restart_reboot(el);
	
	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
	   //console.log('w3_checkbox_change el='+ el +' checked='+ el.checked);
		//el.select();
		w3_highlight(el);
		setTimeout(function() {
			w3_unhighlight(el);
		}, w3_highlight_time);
		w3_call(save_cb, path, el.checked, /* first */ false);
	}
}

function w3_checkbox(psa, label, path, checked, cb, label_ext)
{
	var label_s = w3_label('', label, path, label_ext);
	var id = path? (' id-'+ path) : '';
	var onchange = ' onchange="w3_checkbox_change('+ q(path) +', '+ q(cb) +')"';
	var checked_s = checked? ' checked' : '';
	var p = w3int_psa(psa, 'w3-border w3-hover-shadow'+ id, '', 'type="checkbox"');

	if (label_s != '')
	   label_s += psa.includes('w3-label-inline')? ' ' : '<br>';
	var s = label_s + '<input '+ p + checked_s + onchange +'>'

	// run the callback after instantiation with the initial control value
	if (cb)
		setTimeout(function() {
			//console.log('w3_select: initial callback: '+ cb +'('+ q(path) +', '+ checked +')');
			w3_call(cb, path, checked, /* first */ true);
		}, 500);

	//if (label == 'Title') console.log(s);
	return s;
}

// used when current value should come from config param
function w3_checkbox_get_param(psa, label, path, save_cb, init_val)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? null : init_val);
	//console.log('w3_checkbox_get_param: path='+ path +' cur_val="'+ cur_val +'"');
	return w3_checkbox(psa, label, path, cur_val, save_cb);
}


////////////////////////////////
// select
////////////////////////////////

function w3_select_change(ev, path, save_cb)
{
	var el = ev.currentTarget;
	w3_check_restart_reboot(el);

	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
		w3_call(save_cb, path, el.value, /* first */ false);
	}
}

function w3int_select(psa, label, title, path, sel, opts_s, cb, label_ext, prop)
{
	var label_s = w3_label('', label, path, label_ext);
	var first = '';

	if (title != '') {
		first = '<option value="-1" '+ ((sel == -1)? 'selected':'') +' disabled>' + title +'</option>';
	} else {
		if (sel == -1) sel = 0;
	}
	
	if (label_s != '')
	   label_s += psa.includes('w3-label-inline')? ' ' : '<br>';
	var spacing = (label_s != '')? (' '+ (prop? prop : 'w3-margin-T-8')) : '';
	var onchange = 'onchange="w3_select_change(event, '+ q(path) +', '+ q(cb) +')"';
	var p = w3int_psa(psa, 'id-'+ path + spacing, '', onchange);

	var s = label_s +'<select '+ p +'>'+ first + opts_s +'</select>';

	// run the callback after instantiation with the initial control value
	if (cb && sel != -1)
		setTimeout(function() {
			//console.log('w3_select: initial callback: '+ cb +'('+ q(path) +', '+ sel +')');
			w3_call(cb, path, sel, /* first */ true);
		}, 500);

	//console.log(s);
	return s;
}

function w3int_select_options(sel, opts)
{
   var s = '';
   
   if (Array.isArray(opts)) {
      for (var i=0; i < opts.length; i++) {
         s += '<option value='+ dq(i) +' '+ ((i == sel)? 'selected':'') +'>'+ opts[i] +'</option>';
      }
   } else {
      // FIXME: do we need this anymore? where opts served a dual purpose by having non-integer keys?
      var keys = Object.keys(opts);
      for (var i=0; i < keys.length; i++) {
         s += '<option value='+ dq(i) +' '+ ((i == sel)? 'selected':'') +'>'+ opts[keys[i]] +'</option>';
      }
   }
   
   return s;
}

function w3_select(label, title, path, sel, opts, save_cb, label_ext, prop)
{
   var s = w3int_select_options(sel, opts);
   return w3int_select('', label, title, path, sel, s, save_cb, label_ext, prop);
}

function w3_select_psa(psa, label, title, path, sel, opts, save_cb, label_ext, prop)
{
   var s = w3int_select_options(sel, opts);
   return w3int_select(psa, label, title, path, sel, s, save_cb, label_ext, prop);
}

function w3_select_hier(psa, label, title, path, sel, opts, cb, label_ext, prop)
{
   var s = '';
   var idx = 0;
   var keys = Object.keys(opts);
   for (var i=0; i < keys.length; i++) {
      var key = keys[i];
      s += '<option value='+ dq(idx++) +' disabled>'+ key +'</option>';
      var o = opts[key];
      for (var j=0; j < o.length; j++) {
         s += '<option value='+ dq(idx++) +'>'+ o[j].toString() +'</option>';
      }
   }
   //console.log(s);
   
   return w3int_select(psa, label, title, path, sel, s, cb, label_ext, prop);
}

// used when current value should come from config param
function w3_select_get_param(label, title, path, opts, save_cb, init_val, label_ext)
{
	var cur_val = ext_get_cfg_param(path, (init_val == undefined)? 0 : init_val);
	return w3_select(label, title, path, cur_val, opts, save_cb, label_ext);
}

function w3_select_enum(path, func)
{
	w3_iterate_children('id-'+path, func);
}

function w3_select_value(path, idx)
{
	w3_el_id(path).value = idx;
}


////////////////////////////////
// slider
////////////////////////////////

function w3_slider_change(ev, complete, path, save_cb)
{
	var el = ev.currentTarget;
	w3_check_restart_reboot(el);
	
	// save_cb is a string because can't pass an object to onclick
	if (save_cb) {
		w3_call(save_cb, path, el.value, complete);
	}
}

function w3_slider(label, path, val, min, max, step, save_cb, placeholder, label_ext)
{
	if (val == null)
		val = '';
	else
		val = w3_strip_quotes(val);
	var oc = 'oninput="w3_slider_change(event, 0, '+ q(path) +', '+ q(save_cb) +')" ';
	// change fires when the slider is done moving
	var os = 'onchange="w3_slider_change(event, 1, '+ q(path) +', '+ q(save_cb) +')" ';
	var label_s = w3_label('', label, path, label_ext);
	var s =
		label_s +'<br>'+
		'<input id="id-'+ path +'" class="" value=\''+ val +'\' ' +
		'type="range" min="'+ min +'" max="'+ max +'" step="'+ step +'" '+ oc + os +
		(placeholder? ('placeholder="'+ placeholder +'"') : '') +'>';
	//console.log(s);
	return s;
}


////////////////////////////////
// menu
////////////////////////////////

function w3_menu(psa, cb)
{
   var id = psa.split(' |')[0];
   //console.log('w3_menu id='+ id +' psa='+ psa);

   var onclick = ' onclick="w3int_menu_onclick(event, '+ q(id) +', '+ q(cb) +')"' +
      ' oncontextmenu="w3int_menu_onclick(event, '+ q(id) +', '+ q(cb) +')"';
	var p = w3int_psa(psa, 'w3-menu w3-round-large', '', onclick);
   w3_el_id('id-w3-main-container').innerHTML += '<div'+ p +'></div>';
}

function w3_menu_items(id)
{
   //console.log('w3_menu_items id='+ id);

   var s = '';
   var idx = 0, prop, attr;

   for (var i=1; i < arguments.length; i++) {
      if (arguments[i] == '<hr>') {
         prop = 'w3-menu-item-hr';
         attr = '';
      } else {
         prop = 'w3-menu-item';
         attr = 'id='+ dq(idx);
         idx++;
      }
      s += w3_div(prop +'||'+ attr, arguments[i]);
   }
   
   //console.log(s);
   w3_el_id(id).innerHTML = s;
}

function w3_menu_popup(id, x, y)
{
   //console.log('w3_menu_popup id='+ id +' x='+ x +' y='+ y);
   var el = w3_el_id(id);
   el.style.top = px(y);
   el.style.left = px(x);
   el.style.visibility = 'visible';
   el.w3_menu_x = x;

	// close menu if escape key while menu is displayed
	w3int_menu_close_cur_id = id;
	window.addEventListener("keyup", w3int_menu_close, false);
	window.addEventListener("click", w3int_menu_close, false);
}

function w3int_menu_onclick(ev, id, cb)
{
   //console.log('w3int_menu_onclick id='+ id +' cb='+ cb);
   //if (ev != null) event_dump(ev, "MENU");
   var el = w3_el_id(id);
   el.style.visibility = 'hidden';

   window.removeEventListener("keyup", w3int_menu_close, false);
   window.removeEventListener("click", w3int_menu_close, false);

   if (ev != null && cb != null) {
      var _id = ev.target.id;
      var idx = +_id;
      if (_id == '' || isNaN(idx)) _id = ev.target.parentNode.id;
      idx = +_id;
      if (_id == '' || isNaN(idx)) idx = -1;
      w3_call(cb, idx, el.w3_menu_x);
   }

   // allow right-button to select menu items
	if (ev != null) {
      //console.log('w3int_menu_onclick: cancelEvent()');
      canvas_ignore_mouse_event = true;
	   return cancelEvent(ev);
	}
}

var w3int_menu_close_cur_id;

// close menu if escape key pressed or a click outside of the menu
function w3int_menu_close(evt)
{
   //event_dump(evt, 'MENU-CLOSE');
   if ((evt.type == 'keyup' && evt.key == 'Escape') ||
      (evt.type == 'click' && evt.button != 2 )) {
      //console.log('w3int_menu_close '+ evt.type +' button='+ evt.button);
      w3int_menu_onclick(null, w3int_menu_close_cur_id);
   }
}


////////////////////////////////
// standard callbacks
////////////////////////////////

function w3_num_cb(path, val)
{
	var v = parseFloat(val);
	if (isNaN(v)) v = 0;
	//console.log('w3_num_cb: path='+ path +' val='+ val +' v='+ v);
	setVarFromString(path, val);
}

function w3_string_cb(path, val)
{
	//console.log('w3_string_cb: path='+ path +' val='+ val);
	setVarFromString(path, val.toString());
}

function w3_num_set_cfg_cb(path, val, first)
{
	var v = parseFloat(val);
	if (isNaN(v)) v = 0;
	
	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, v, save);
}

function w3_string_set_cfg_cb(path, val, first)
{
	//console.log('w3_string_set_cfg_cb: path='+ path +' '+ typeof val +' "'+ val +'" first='+ first);
	
	// if first time don't save, otherwise always save
	var save = (first != undefined)? (first? false : true) : true;
	ext_set_cfg_param(path, encodeURIComponent(val.toString()), save);
}


////////////////////////////////
// tables
////////////////////////////////

function w3_table(psa)
{
	var p = w3int_psa(psa);
	var s = '<table'+ p +'>';
		for (var i=1; i < arguments.length; i++) {
			s += arguments[i];
		}
	s += '</table>';
	//console.log(s);
	return s;
}

function w3_table_heads(psa)
{
	var p = w3int_psa(psa, 'w3-table-head');
	var s = '';
	for (var i=1; i < arguments.length; i++) {
	   if (arguments[i] == null) continue;
		s += '<th'+ p +'>';
		s += arguments[i];
		s += '</th>';
	}
	//console.log(s);
	return s;
}

function w3_table_row(psa)
{
	var p = w3int_psa(psa, 'w3-table-row');
	var s = '<tr'+ p +'>';
		for (var i=1; i < arguments.length; i++) {
	      if (arguments[i] == null) continue;
			s += arguments[i];
		}
	s += '</tr>';
	//console.log(s);
	return s;
}

function w3_table_cells(psa)
{
	var p = w3int_psa(psa, 'w3-table-cell');
	var s = '';
	for (var i=1; i < arguments.length; i++) {
	   if (arguments[i] == null) continue;
		s += '<td'+ p +'>';
		s += arguments[i];
		s += '</td>';
	}
	//console.log(s);
	return s;
}


////////////////////////////////
// containers
////////////////////////////////

// deprecated
function w3_inline(prop, attr)
{
	attr = (attr == undefined)? '' : (' '+ attr);
	var s = '<div class="w3-show-inline-block '+ prop +'"'+ attr +'>';
	var narg = arguments.length;
		for (var i=2; i < narg; i++) {
			s += arguments[i];
		}
	s += '</div>';
	//console.log(s);
	return s;
}

function w3_div(psa)
{
   var p = w3int_psa(psa);
	var s = '<div'+ p +'>';
	var narg = arguments.length;
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

// Fit an enclosing box around (possibly multi-line) text by misusing a <button>.
// Trying to do this with <span> doesn't seem to work.
function w3_text(prop, text)
{
	var s = '<button type="button" class="w3-text '+ prop +'">'+ text +'</button>';
	//console.log(s);
	return s;
}

function w3_code(prop_outer, prop_inner)
{
	var narg = arguments.length;
	var s = '<pre class="'+ prop_outer +'"><code>';
		for (var i=2; i < narg; i++) {
			s += '<div class="'+ prop_inner +'">'+ arguments[i] + '</div>';
		}
	s += '</code></pre>';
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
	'</div>';
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
	'</div>';
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
