// Copyright (c) 2016 John Seamons, ZL/KF6VO

/*

TODO
	IQ displays better s/n than real -- why is that?
	with fixed gain, why do increasing # of avgs make level go down?
		shouldn't peaks be constant?
	better averaging
		averaging choices and/or controls?
	bug with clearing right-of-display on gri change
	add input validation checks
	add id of stations based on master pulse detection?
	with some DSP could also id stations based on pulse phase-coding?
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


// Emission delay data from Markus Vester, DF6NM
// LoranView: www.df6nm.bplaced.net/LoranView/LoranGrabber.htm

var emission_delay = {

	5960: [ { s:'M Inta', d:0 },		// LoranView reception
			  { s:'X Tumanny Pen', d:14670.15 },
			  { s:'Z Norilsk', d:45915.33 }
			],
			  
	5990: [ { s:'M Caucasian Center', d:0 },		// weak reception on LoranView
			  { s:'X Caucasian West', d:16587 },
			  { s:'Y Caucasian East', d:31304 },
			  { s:'Z Caucasian North', d:46440 }
			],
			
	6000: [ { s:'M Pucheng', d:0 } ],	// LoranView reception

	6731: [ { s:'M Anthorn', d:0 } ],	// LoranView reception

	6780: [ { s:'M Hexian', d:0 },
			  { s:'X Raoping', d:14464.69 },
			  { s:'Y Chongzuo', d:26925.76 }
			],
			
	7430: [ { s:'M Rongcheng', d:0 },
			  { s:'X Xuancheng', d:13459.70 },
			  { s:'Y Helong', d:30852.32 }
			],
			
	7950: [ { s:'M Aleksandrovsk', d:0 },			// chain listed on current LoranView, but no current reception
			  { s:'W Petropavlovsk', d:14506.50 },
			  { s:'X Ussuriisk', d:33678.00 },
			  { s:'Y (ex-Tokachibuto)', d:49104.15 },	// not listed on current LoranView, 10/2015 demolished on Google Earth
			  { s:'Z Okhotsk', d:64102.05 }		// not listed on current LoranView, 10 kW, 6/2003 most recent Google Earth
			],

	8000: [ { s:'M Bryansk', d:0 },
			  { s:'W Petrozavodsk', d:13217.21 },
			  { s:'X Slonim', d:27125.00 },
			  { s:'Y Simferopol', d:53070.25 },
			  { s:'Z Syzran', d:67941.60 }
			],
			
	8390: [ { s:'M Xuancheng', d:0 },
			  { s:'X Raoping', d:13795.52 },
			  { s:'Y Rongcheng', d:31459.70 }
			],

	8830: [ { s:'M Afif', d:0 },
			  { s:'W Salwa', d:13645.00 },
			  { s:'X (ex-Al Khamasin)', d:27265.00 },	// not listed on current LoranView, 10/2013 demolished on Google Earth
			  { s:'Y Ash Shaykh', d:42645.00 },
			  { s:'Z Al Muwassam', d:58790.00 }
			],

	8970: [ { s:'M Wildwood', d:0 } ],

	9930: [ { s:'M Pohang', d:0 },
			  { s:'W Kwang Ju', d:11946.97 },
			  { s:'X (ex-Gesashi)', d:25565.52 },		// not listed on current LoranView, 1/2015 demolished on Google Earth
			  { s:'Y (ex-Niijima)', d:40085.64 },		// not listed on current LoranView, 1/2015 tower standing on Google Earth
			  { s:'Z Ussuriisk', d:54162.44 }
			]
};

var loran_c_ext_name = 'loran_c';		// NB: must match loran_c.c:loran_c_ext.name

var loran_c_setup = false;
var loran_c_ws;

function loran_c_main()
{
	// only establish communication to server the first time extension is started
	if (!loran_c_setup) {
		loran_c_ws = ext_connect_server(loran_c_recv);
		loran_c_setup = true;
	} else {
		loran_c_controls_setup();
	}
}

var loran_c_cmd_e = { SCOPE_DATA:0, SCOPE_RESET:1 };
var loran_c_sidebarx = 25;
var loran_c_startx = 200;
var loran_c_hlegend = 20;

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

			// this must be included for initialization
			case "ext_init":
				ext_init(loran_c_ext_name, loran_c_ws);
				break;

			case "ready":
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

function loran_c_update_gri()
{
	var w = loran_c_scope.width;
	var h = loran_c_scope.height;
	var ssx = loran_c_sidebarx;
	var sx = loran_c_startx;

	loran_c_scope.ct.fillStyle = 'black';
	loran_c_scope.ct.fillRect(ssx, 0, w-ssx, h);

	loran_c_scope.ct.font = '12px Verdana';

	loran_c_draw_legend(0, loran_c.gri0, 'loran_c.gri0_sel');
	loran_c_draw_legend(1, loran_c.gri1, 'loran_c.gri1_sel');
	
	loran_c_ws.send('SET gri0='+ loran_c.gri0 +' gri1='+ loran_c.gri1);
}

var loran_c = {
	'gri0':0, 'gri0_sel':0, 'gain0':0, 'offset0':0,
	'gri1':0, 'gri1_sel':0, 'gain1':0, 'offset1':0
};

var loran_c_scope;

function loran_c_controls_setup()
{
   var data_html =
		'<div id="id-loran_c-data" class="scale" style="width:1024px; height:200px; background-color:black; position:relative; display:none" title="Loran C">' +
			'<canvas id="id-loran_c-scope" width="1024" height="200" style="position:absolute">test</canvas>' +
		'</div>';

	var controls_html =
		w3_divs('id-loran_c-controls w3-text-white', '',
			w3_half('', '',
				w3_divs('', 'w3-medium', '<b>Loran-C viewer</b>'),
				w3_divs('', '',
					'See also <b><a href="http://www.df6nm.bplaced.net/LoranView/LoranGrabber.htm" target="_blank">LoranView</a></b> by DF6NM')
			),
			
			w3_half('', '',
				w3_divs('', 'w3-margin-top',
					w3_col_percent('', '',
						w3_input('GRI', 'loran_c.gri0', 0, 'loran_c_gri_cb'), 25
					),
					w3_select('GRI', 'select', 'loran_c.gri0_sel', 0, gri_s, 'loran_c_gri_select_cb'),
					w3_slider('Gain (auto-scale)', 'loran_c.gain0', loran_c.gain0, 0, 100, 'loran_c_gain_cb'),
					w3_slider('Offset', 'loran_c.offset0', loran_c.offset0, 0, 100, 'loran_c_offset_cb')
				),
	
				w3_divs('', 'w3-margin-top',
					w3_col_percent('', '',
						w3_input('GRI', 'loran_c.gri1', 0, 'loran_c_gri_cb'), 25
					),
					w3_select('GRI', 'select', 'loran_c.gri1_sel', 0, gri_s, 'loran_c_gri_select_cb'),
					w3_slider('Gain (auto-scale)', 'loran_c.gain1', loran_c.gain1, 0, 100, 'loran_c_gain_cb'),
					w3_slider('Offset', 'loran_c.offset1', loran_c.offset1, 0, 100, 'loran_c_offset_cb')
				)
			)
		);
	
	ext_tune(100, 'am', zoom.abs, 8);

	ext_panel_show(controls_html, data_html, null, function() {
		//console.log('### ext_panel_show HIDE-HOOK');
		loran_c_ws.send('SET stop');
		loran_c_visible(0);		// hook to be called when controls panel is closed
	});
	
	loran_c_scope = html_id('id-loran_c-scope');
	loran_c_scope.ct = loran_c_scope.getContext("2d");
	loran_c_scope.im = loran_c_scope.ct.createImageData(1024, 1);
	//console.log('### loran_c_scope assigned');

	// set GRIs from admin config if applicable
	var gri0 = getVarFromString('cfg.loran_c.gri0');
	if (gri0 == null || gri0 == undefined) {
		gri0 = parseInt(gri_s[0]);		// default to first chain in list
	}
	loran_c.gri0 = gri0;
	w3_set_value('loran_c.gri0', gri0);

	var gri1 = getVarFromString('cfg.loran_c.gri1');
	if (gri1 == null || gri1 == undefined) {
		gri1 = parseInt(gri_s[0]);		// default to first chain in list
	}
	loran_c.gri1 = gri1;
	w3_set_value('loran_c.gri1', gri1);

	// setup menus to match GRIs
	loran_c_gri_cb('loran_c.gri0', loran_c.gri0);
	loran_c_gri_cb('loran_c.gri1', loran_c.gri1);

	loran_c_update_gri();
	//console.log('### SET start');
	loran_c_ws.send('SET start');
	loran_c_visible(1);
}

// FIXME input validation
function loran_c_gri_cb(path, val)
{
	w3_num_cb(path, val);
	
	// adjust menu to match if there is a corresponding entry
	var found = false;
	var menu_path = path +'_sel';

	w3_select_enum(menu_path, function(v) {	// walk all the menu entries looking for a match
		var gri_sel = v.value - 1;
		if (gri_sel >= 0 && val == parseInt(gri_s[gri_sel])) {
			w3_select_value(menu_path, v.value);
			found = true;
		}
	});

	if (!found)
		w3_select_value(menu_path, 0);	// reset menu if nothing matches

	loran_c_update_gri();
}

function loran_c_gri_select_cb(path, i)
{
	if (i == 0) i = 1;
	i--;
	var gri = parseInt(gri_s[i]);
	var base_path = path.split('_sel');		// hack: depends on select having path ending in '_sel'
	w3_num_cb(base_path[0], gri);

	// update corresponding w3_input
	w3_set_value(base_path[0], gri);
	loran_c_update_gri();
}

function loran_c_gain_cb(path, val)
{
	w3_num_cb(path, val);
	w3_input_label('Gain'+ ((val == 0)? ' (auto-scale)':''), path);
	loran_c_ws.send('SET gain0='+ loran_c.gain0 +' gain1='+ loran_c.gain1);
}

function loran_c_offset_cb(path, val)
{
	w3_num_cb(path, val);
	loran_c_ws.send('SET offset0='+ loran_c.offset0 +' offset1='+ loran_c.offset1);
}

// called to display HTML for configuration parameters in admin interface
function loran_c_config_html()
{
	ext_admin_config(loran_c_ext_name, 'Loran C',
		w3_divs('id-loran_c w3-text-teal w3-hide', '',
			'<b>Loran C configuration</b>' +
			'<hr>' +
			admin_input('default GRI 0', 'loran_c.gri0', 'admin_num_cb') +
			admin_input('default GRI 1', 'loran_c.gri1', 'admin_num_cb')
		)
	);
}

function loran_c_visible(v)
{
	visible_block('id-loran_c-data', v);
}
