// Copyright (c) 2016 John Seamons, ZL/KF6VO

/*

TODO
	Would using a log display help?
	Make n-averages rolling?
		same with CMA?
	IQ displays better s/n than real -- why is that?
	bug with clearing right-of-display on gri change
	add input validation checks
	add id of stations based on master pulse detection?
	with some DSP could also id stations based on pulse phase-coding?
		need _much_ higher sample rate which means not using audio IQ stream
	noise pulse blanker?

*/

var gri_s = {
	 0:'5960 North Russia (Chayka)',
	 1:'5990 Caucasus',
	 2:'6000 China BPL Pucheng',
	 3:'6731 Anthorn UK',
	 4:'6780 China South Sea',
	 5:'7430 China North Sea',
	 6:'7950 Eastern Russia (Chayka)',
	 7:'8000 Western Russia (Chayka)',
	 8:'8390 China East Sea',
	 9:'8830 Saudi Arabia North',
	10:'8970 Wildwood USA (testing)',
	11:'9930 Korea'
};

var loran_c_default_chain1 = 7;
var loran_c_default_chain2 = 3;

// Emission delay data from Markus Vester, DF6NM
// LoranView: www.df6nm.bplaced.net/LoranView/LoranGrabber.htm

var emission_delay = {

	5960: [ { s:'M Inta', d:0 },		// LoranView (DE) shows chain reception
			  { s:'X Tumanny Pen', d:14670.15 },
			  { s:'Z Norilsk', d:45915.33 }
			],
			  
	5990: [ { s:'M Caucasian Center', d:0 },		// LoranView (DE) shows weak chain reception
			  { s:'X Caucasian West', d:16587 },
			  { s:'Y Caucasian East', d:31304 },
			  { s:'Z Caucasian North', d:46440 }
			],
			
	6000: [ { s:'M Pucheng', d:0 } ],	// LoranView (DE) shows chain reception

	6731: [ { s:'M Anthorn', d:0 },		// LoranView (DE) shows chain reception
			  { s:'Y Anthorn', d:27300.00 }
			],

	6780: [ { s:'M Hexian', d:0 },		// LoranView (DE) shows chain reception
			  { s:'X Raoping', d:14464.69 },
			  { s:'Y Chongzuo', d:26925.76 }
			],
			
	7430: [ { s:'M Rongcheng', d:0 },		// LoranView (DE) shows chain reception
			  { s:'X Xuancheng', d:13459.70 },
			  { s:'Y Helong', d:30852.32 }
			],
			
	7950: [ { s:'M Aleksandrovsk', d:0 },			// chain listed on current LoranView, but marginal reception
			  { s:'W Petropavlovsk', d:14506.50 },
			  { s:'X Ussuriisk', d:33678.00 },		// LoranView (DE) shows reception of this station at times
			  { s:'Y (ex-Tokachibuto)', d:49104.15 },	// not listed on current LoranView, 10/2015 demolished on Google Earth
			  { s:'Z Okhotsk', d:64102.05 }		// not listed on current LoranView, 10 kW, 6/2003 most recent Google Earth image
			],

	8000: [ { s:'M Bryansk', d:0 },				// LoranView (DE) shows chain reception
			  { s:'W Petrozavodsk', d:13217.21 },
			  { s:'X Slonim', d:27125.00 },
			  { s:'Y Simferopol', d:53070.25 },
			  { s:'Z Syzran', d:67941.60 }
			],
			
	8390: [ { s:'M Xuancheng', d:0 },			// LoranView (DE) shows chain reception
			  { s:'X Raoping', d:13795.52 },
			  { s:'Y Rongcheng', d:31459.70 }
			],

	8830: [ { s:'M Afif', d:0 },					// LoranView (DE) shows chain reception
			  { s:'W Salwa', d:13645.00 },
			  { s:'X (ex-Al Khamasin)', d:27265.00 },	// not listed on current LoranView, 10/2013 demolished on Google Earth
			  { s:'Y Ash Shaykh', d:42645.00 },
			  { s:'Z Al Muwassam', d:58790.00 }
			],

	8970: [ { s:'M Wildwood', d:0 } ],			// LoranView (DE) shows chain reception (when on-air)

	9930: [ { s:'M Pohang', d:0 },				// LoranView (DE) shows some reception
			  { s:'W Kwang Ju', d:11946.97 },
			  { s:'X (ex-Gesashi)', d:25565.52 },		// not listed on current LoranView, 1/2015 demolished on Google Earth
			  { s:'Y (ex-Niijima)', d:40085.64 },		// not listed on current LoranView, 1/2015 tower standing on Google Earth
			  { s:'Z Ussuriisk', d:54162.44 }
			]
};

var loran_c_ext_name = 'loran_c';		// NB: must match loran_c.c:loran_c_ext.name

var loran_c_first_time = true;

function loran_c_main()
{
	ext_switch_to_client(loran_c_ext_name, loran_c_first_time, loran_c_recv);		// tell server to use us (again)
	if (!loran_c_first_time)
		loran_c_controls_setup();
	loran_c_first_time = false;
}

var loran_c_cmd_e = { SCOPE_DATA:0, SCOPE_RESET:1 };
var loran_c_sidebarx = 25;
var loran_c_startx = 200;
var loran_c_hlegend = 20;
var loran_c_nbuckets = [ ];

function loran_c_recv(data)
{
	var firstChars = getFirstChars(data, 3);
	
	// process data sent from server/C by ext_send_data_msg()
	if (firstChars == "DAT") {
		var ba = new Uint8Array(data, 4);
		var cmd = ba[0];

		if (cmd == loran_c_cmd_e.SCOPE_DATA || cmd == loran_c_cmd_e.SCOPE_RESET) {
			var ch = ba[1];
			var o = 2;
			var blen = ba.length-o;
			loran_c_nbuckets[ch] = blen;
			var sx = loran_c_startx;
			var w = loran_c_scope.width;
			var h = loran_c_scope.height;
			var hlegend = loran_c_hlegend;
			var sy = (ch+1)*h/2 - hlegend;
			var yh = h/2 - hlegend;
			
			if (cmd == loran_c_cmd_e.SCOPE_RESET) {
            loran_c_scope.ct.fillStyle = 'black';
				loran_c_scope.ct.fillRect(sx, ch*h/2, w-sx, yh);
			}

			for (i=0; i < blen; i++) {
				var z = ba[o+i] / 255;
            loran_c_scope.ct.fillStyle = 'black';
            loran_c_scope.ct.fillRect(sx+i, sy, 1, -yh);
            loran_c_scope.ct.fillStyle = ch? 'violet':'cyan';
            loran_c_scope.ct.fillRect(sx+i, sy, 1, z * -yh);
         }
		} else {
			console.log('loran_c_recv: DATA UNKNOWN cmd='+ cmd +' len='+ (ba.length-1));
		}
		return;
	}
	
	// process command sent from server/C by ext_send_msg() or ext_send_encoded_msg()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (1 && param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('loran_c_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('loran_c_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				// do this here, so last settings are preserved across activations
				for (var ch=0; ch < 2; ch++) {
					for (var algo=0; algo < loran_c_nalgo; algo++) {
						loran_c_avg_param_cur[ch * loran_c_nalgo + algo] = loran_c_slider_param_init;
					}
				}
			
				loran_c_controls_setup();
				break;

			case "ms_per_bin":
				loran_c_ms_per_bin = parseFloat(param[1]);
				console.log('loran_c_ms_per_bin='+ loran_c_ms_per_bin);
				break;

			default:
				console.log('loran_c_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}

var loran_c_station_colors = [ 'red', 'yellow', 'lime', 'blue', 'grey' ];

function loran_c_draw_legend(ch, gri, gri_menu_id)
{
	var w = loran_c_scope.width;
	var h = loran_c_scope.height;
	var ssx = loran_c_sidebarx;
	var sx = loran_c_startx;
	var sy = (ch+1)*h/2;
	var hlegend = loran_c_hlegend;
	var yh = h/2 - hlegend;
	var off = 8;
	var hbar = 6;
	var htext = hlegend-hbar;
	
	loran_c_scope.ct.fillStyle = 'white';
	loran_c_scope.ct.fillText('GRI '+ gri, ssx, sy-yh/3-hlegend - off);
	var menu = html_idname(gri_menu_id).value;
	//console.log('loran_c_draw_legend: ch='+ ch +' gri='+ gri +' gri_menu_id='+ gri_menu_id +' menu='+ menu);
	if (menu != null && menu != 0) {
		loran_c_scope.ct.fillText(gri_s[menu-1].substr(5), ssx, sy-yh/3-hlegend + off);
	}

	var ed = emission_delay[gri];
	if (ed != null) {
		//console.log('gri='+ gri +' #s='+ ed.length);
		for (var i=0; i < ed.length; i++) {
			var ed0 = ed[i].d / 1000;
			var ed1;
			if (i < ed.length-1)
				ed1 = ed[i+1].d / 1000;
			else
				ed1 = gri / 100;
			//console.log(i +' '+ ed[i].s +': ed0='+ ed0 +' ed1='+ ed1);
			
			ed0 = Math.round(ed0 / loran_c_ms_per_bin);
			ed1 = Math.round(ed1 / loran_c_ms_per_bin);
			//console.log(i +': ed0='+ ed0 +' ed1='+ ed1);
         loran_c_scope.ct.fillStyle = loran_c_station_colors[i];
			loran_c_scope.ct.fillRect(sx+ed0, sy-hlegend, ed1-ed0, hbar);
			loran_c_scope.ct.fillStyle = 'white';
			loran_c_scope.ct.fillText(ed[i].s, sx+ed0, sy-3);
		}
	}
}

var loran_c_ms_per_bin;

function loran_c_update_gri(ch, path_to_menu, gri)
{
	var w = loran_c_scope.width;
	var h = loran_c_scope.height;
	var ssx = loran_c_sidebarx;
	var sx = loran_c_startx;

	loran_c_scope.ct.fillStyle = 'black';
	loran_c_scope.ct.fillRect(ssx, ch*h/2, w-ssx, h/2);

	loran_c_scope.ct.font = '12px Verdana';

	loran_c_draw_legend(ch, gri, path_to_menu);
	
	ext_send('SET gri'+ ch +'='+ gri);
}

var loran_c_nalgo = 3;
var loran_c_avg_algo_e = { CMA:0, EMA:1, IIR:2 };
var loran_c_avg_algo_s = { 0:'CMA', 1:'EMA', 2:'IIR' };

var loran_c_avg_param_s = { 0:'Averages', 1:'Decay', 2:'Exp' };
var loran_c_avg_param_init = { 0:8, 1:128, 2:1 };
var loran_c_avg_param_max = { 0:32, 1:256, 2:1 };

var loran_c_slider_algo_init = loran_c_avg_algo_e.EMA;
var loran_c_slider_param_init = Math.ceil(loran_c_avg_param_init[loran_c_slider_algo_init] / loran_c_avg_param_max[loran_c_slider_algo_init] * 100);
var loran_c_avg_param_cur = [ ];

var loran_c = {
	'gri0':0, 'gri_sel0':0, 'gain0':0, 'offset0':0, 'avg_algo0':loran_c_slider_algo_init+1, 'avg_param0':loran_c_slider_param_init,
	'gri1':0, 'gri_sel1':0, 'gain1':0, 'offset1':0, 'avg_algo1':loran_c_slider_algo_init+1, 'avg_param1':loran_c_slider_param_init
};

var loran_c_scope;

function loran_c_controls_setup()
{
   var data_html =
		'<div id="id-loran_c-data" class="scale" style="width:1024px; height:200px; background-color:black; position:relative; display:none" title="Loran-C">' +
			'<canvas id="id-loran_c-scope" width="1024" height="200" style="position:absolute">test</canvas>' +
		'</div>';

	// set GRIs from admin config if applicable
	var gri0 = ext_get_cfg_param('loran_c.gri0');
	if (gri0 == null || gri0 == undefined) {
		gri0 = parseInt(gri_s[loran_c_default_chain1]);
	}
	loran_c.gri0 = gri0;

	var gri1 = ext_get_cfg_param('loran_c.gri1');
	if (gri1 == null || gri1 == undefined) {
		gri1 = parseInt(gri_s[loran_c_default_chain2]);
	}
	loran_c.gri1 = gri1;
	//console.log('loran_c_controls_setup: gri0='+ gri0 +' gri1='+ gri1);

	var controls_html =
		w3_divs('id-loran_c-controls w3-text-white', '',
			w3_half('', '',
				w3_divs('', 'w3-medium w3-text-aqua', '<b>Loran-C viewer</b>'),
				w3_divs('', '',
					'See also <b><a href="http://www.df6nm.bplaced.net/LoranView/LoranGrabber.htm" target="_blank">LoranView</a></b> by DF6NM')
			),
			
			w3_half('', '',
				w3_divs('', 'w3-margin-T-8',
					w3_col_percent('', '',
						w3_input('GRI', 'loran_c.gri0', loran_c.gri0, 'loran_c_gri_cb'), 25
					),
					w3_select('GRI', 'select', 'loran_c.gri_sel0', 0, gri_s, 'loran_c_gri_select_cb'),
					w3_slider('Gain (auto-scale)', 'loran_c.gain0', loran_c.gain0, 0, 100, 1, 'loran_c_gain_cb'),
					w3_select('Averaging', 'select', 'loran_c.avg_algo0', loran_c.avg_algo0, loran_c_avg_algo_s, 'loran_c_avg_algo_select_cb'),
					w3_slider('?', 'loran_c.avg_param0', loran_c.avg_param0, 0, 100, 1, 'loran_c_avg_param_cb')
				),
	
				w3_divs('', 'w3-margin-T-8',
					w3_col_percent('', '',
						w3_input('GRI', 'loran_c.gri1', loran_c.gri1, 'loran_c_gri_cb'), 25
					),
					w3_select('GRI', 'select', 'loran_c.gri_sel1', 0, gri_s, 'loran_c_gri_select_cb'),
					w3_slider('Gain (auto-scale)', 'loran_c.gain1', loran_c.gain1, 0, 100, 1, 'loran_c_gain_cb'),
					w3_select('Averaging', 'select', 'loran_c.avg_algo1', loran_c.avg_algo1, loran_c_avg_algo_s, 'loran_c_avg_algo_select_cb'),
					w3_slider('?', 'loran_c.avg_param1', loran_c.avg_param1, 0, 100, 1, 'loran_c_avg_param_cb')
				)
			)
		);
	
	ext_tune(100, 'am', ext_zoom.ABS, 8);

	ext_panel_show(controls_html, data_html, null);
	
	loran_c_scope = html_id('id-loran_c-scope');
	loran_c_scope.ct = loran_c_scope.getContext("2d");
	loran_c_scope.im = loran_c_scope.ct.createImageData(1024, 1);
	loran_c_scope.addEventListener("mousedown", loran_c_mousedown, false);

	//console.log('### SET start');
	ext_send('SET start');
	loran_c_visible(1);
}

function loran_c_mousedown(evt)
{
	//console.log("MDN evt: offX="+evt.offsetX+" pageX="+evt.pageX+" clientX="+evt.clientX+" layerX="+evt.layerX );
	//console.log("MDN evt: offY="+evt.offsetY+" pageY="+evt.pageY+" clientY="+evt.clientY+" layerY="+evt.layerY );
	var y = evt.clientY? evt.clientY : (evt.offsetY? evt.offsetY : evt.layerY);
	var ch = (y < loran_c_scope.height/2)? 0:1;
	var offset = (evt.clientX? evt.clientX : (evt.offsetX? evt.offsetX : evt.layerX)) - loran_c_startx;
	if (offset < 0 || offset >= loran_c_nbuckets[ch]) return;
	//console.log('ch='+ ch +' offset='+ offset);
	ext_send('SET offset'+ ch +'='+ offset);
}

function loran_c_blur()
{
	//console.log('### loran_c_blur');
	ext_send('SET stop');
	loran_c_visible(0);		// hook to be called when controls panel is closed
}

// FIXME input validation
function loran_c_gri_cb(input_path, gri)
{
	w3_num_cb(input_path, gri);
	
	// adjust menu to match if there is a corresponding entry
	var menu_path = input_path.replace('gri', 'gri_sel');		// hack: depends on e.g. mapping gri0 -> gri_sel0
	loran_c_gri_select_cb(menu_path, 0);

	//console.log('loran_c_gri_cb: input_path='+ input_path +' gri='+ gri +' menu_path='+ menu_path +' mv='+ w3_get_value(menu_path));
	var ch = parseInt(input_path.charAt(input_path.length-1));
	loran_c_update_gri(ch, menu_path, gri);
}

function loran_c_gri_select_cb(menu_path, i)
{
	var input_path = menu_path.replace('_sel','');	 // hack: depends on e.g. mapping gri_sel0 -> gri0

	if (i == 0) {		// initial call from loran_c_controls_setup() or attempt gri match from loran_c_gri_cb()
		var gri = w3_get_value(input_path);
		var found = false;
		w3_select_enum(menu_path, function(v) {	// walk all the menu entries looking for a match
			var gri_sel = v.value - 1;
			if (gri_sel >= 0 && gri == parseInt(gri_s[gri_sel])) {
				w3_select_value(menu_path, v.value);
				found = true;
			}
		});
	
		if (!found)
			w3_select_value(menu_path, 0);	// reset menu if nothing matches
		//console.log('loran_c_gri_select_cb: input_path='+ input_path +' gri='+ gri +' menu_path='+ menu_path +' found='+ found +' mv='+ w3_get_value(menu_path));
	} else {
		i--;
		var gri = parseInt(gri_s[i]);
		//console.log('loran_c_gri_select_cb: path='+ menu_path +' i='+ i +' gri='+ gri +' input='+ input_path +' mv='+ w3_get_value(menu_path));
		w3_num_cb(input_path, gri);
	
		// update corresponding w3_input
		w3_set_value(input_path, gri);
	}

	var ch = parseInt(menu_path.charAt(menu_path.length-1));
	loran_c_update_gri(ch, menu_path, gri);
}

function loran_c_gain_cb(path, val)
{
	w3_num_cb(path, val);
	w3_set_label('Gain '+ ((val == 0)? '(auto-scale)' : val +' dB'), path);
	var ch = parseInt(path.charAt(path.length-1));
	ext_send('SET gain'+ ch +'='+ val);
}

function loran_c_avg_algo_select_cb(path, i)
{
	w3_num_cb(path, i);
	var algo = i-1;
	var ch = parseInt(path.charAt(path.length-1));
	ext_send('SET avg_algo'+ ch +'='+ algo);
	
	// update param and restore slider value 
	var param_path = path.replace('algo', 'param');
	var slider_val;
	try {
		slider_val = loran_c_avg_param_cur[ch * loran_c_nalgo + algo];
	} catch(ex) {
		slider_val = loran_c_slider_param_init;
	}
	loran_c_avg_param_cb(param_path, slider_val);
	w3_set_value(param_path, slider_val);

	var param_val = Math.ceil(slider_val * loran_c_avg_param_max[algo] / 100);
	//console.log('loran_c_avg_algo_select_cb path='+ path +' algo='+ algo +' pp='+ param_path +' sv='+ slider_val +' pv='+ param_val);
}

function loran_c_avg_param_cb(path, slider_val)
{
	w3_num_cb(path, slider_val);
	var menu_path = path.replace('param', 'algo');
	var algo = w3_get_value(menu_path) - 1;
	var ch = parseInt(path.charAt(path.length-1));
	var param_val = Math.ceil(slider_val * loran_c_avg_param_max[algo] / 100);
	param_val = param_val? param_val : 1;		// clamp at 1
	ext_send('SET avg_param'+ ch +'='+ param_val);
	loran_c_avg_param_cur[ch * loran_c_nalgo + algo] = slider_val;

	// update param label
	//console.log('loran_c_avg_param_cb: path='+ path +' sv='+ slider_val +' pv='+ param_val +' algo='+ algo);
	w3_set_label(loran_c_avg_param_s[algo] +' '+ param_val, path);
}

// called to display HTML for configuration parameters in admin interface
function loran_c_config_html()
{
	ext_admin_config(loran_c_ext_name, 'Loran-C',
		w3_divs('id-loran_c w3-text-teal w3-hide', '',
			'<b>Loran-C configuration</b>' +
			'<hr>' +
			w3_third('', 'w3-container',
				w3_divs('', 'w3-margin-bottom',
					admin_input('default GRI 0', 'loran_c.gri0', 'admin_num_cb'),
					admin_input('default GRI 1', 'loran_c.gri1', 'admin_num_cb')
				), '', ''
			)
		)
	);
}

function loran_c_visible(v)
{
	visible_block('id-loran_c-data', v);
}
