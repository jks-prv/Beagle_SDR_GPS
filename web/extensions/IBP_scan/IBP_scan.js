// Copyright (c) 2017 Peter Jennings, VE3SUN

var ibp_scan_ext_name = 'IBP_scan';		// NB: must match IBP_scan.c:ibp_scan_ext.name

var ibp_first_time = true;

function IBP_scan_main()
{
   //console.log('IBP_scan_main');
	ext_switch_to_client(ibp_scan_ext_name, ibp_first_time, ibp_recv_msg);		// tell server to use us (again)
	if (!ibp_first_time)
		ibp_controls_setup();
	ibp_first_time = false;
}

function ibp_recv_msg(data)
{
	// process command sent from server/C by ext_send_msg() or ext_send_msg_encoded()
	var stringData = arrayBufferToString(data);
	var params = stringData.substring(4).split(" ");

	for (var i=0; i < params.length; i++) {
		var param = params[i].split("=");

		if (param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('ibp_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('ibp_recv: '+ param[0]);
		}

		switch (param[0]) {

			case "ready":
				ibp_controls_setup();
				break;

			default:
				console.log('ibp_recv: UNKNOWN CMD '+ param[0]);
				break;
		}
	}
}



function ibp_controls_setup()
{
	var controls_html =
		w3_divs('id-tc-controls w3-text-white', '',
         w3_div('w3-medium w3-text-aqua', '<b>International Beacon Project (IBP) Scanner</b>'),
			w3_col_percent('', '',
            w3_div('', 'by VE3SUN'), 30,
				w3_div('', 'See also <b><a href="http://ve3sun.com/KiwiSDR/index.php" target="_blank">ve3sun.com/KiwiSDR</a></b>'), 50,
				'', 10
			),
			w3_divs('w3-margin-T-8', 'w3-show-inline w3-margin-right',
			   IBP_select
			)
		);
	
	//console.log('ibp_controls_setup');
	ext_panel_show(controls_html, null, null);
	ext_set_controls_width_height(450, 90);
	
	// use extension parameter as beacon station call (or 'cycle' for cycle mode)
	// e.g. kiwisdr.local:8073/?ext=ibp,4u1un (upper or lowercase)
	var call = ext_param();
	if (call) {
	   var i;
      call = call.toLowerCase();
      for (i = 0; i < 18; i++) {
         if (call == dx_ibp[i*2].toLowerCase())
            break;
      }
      var cycle;
      if ((cycle = (i >= 18 && call == 'cycle')) || i < 18) {
         if (cycle) i = 20;
         console.log('IBP: URL set '+ call);
         w3_el_id('select-IBP').value = i;
         set_IBP(i);
      }
   }
}

function IBP_scan_blur()
{
   //console.log('IBP_scan_blur');
   set_IBP(-1);
}

var IBP_monitorSlot = -1;
var IBP_monitoring = false;
var IBP_timer;
var IBP_band = 0;
var IBP_muted = (muted != undefined)? muted : 0;
var IBP_bands = [ "IBP 20m", "IBP 17m", "IBP 15m", "IBP 12m", "IBP 10m" ];

var IBP_select = '<select id="select-IBP" onchange="set_IBP(this.value)"><option value="-2" selected="" disabled="">IBP &#x025BE;</option><option value="-1">OFF</option>';
for( let i=0; i<18; i++) { IBP_select += '<option value="'+i+'">'+dx_ibp[i*2]+'</option>'; }
IBP_select += '<option value="20">Cycle</option></select>';

function set_IBP( v )  // called by IBP selector with slot value
{
   //console.log('set_IBP v='+ v);
    clearTimeout(IBP_timer);
    IBP_band = 0;
    IBP_monitorSlot = v;
    IBP_monitoring = false;
    select_band(IBP_bands[IBP_band]);
    if ( v >= 0 )
       {
       document.getElementById('select-IBP').style.color = 'lime';
       document.getElementById('select-IBP').style.backgroundColor = 'black';
       }
    else
       {
       document.getElementById('select-IBP').selectedIndex = 0;
       document.getElementById('select-IBP').style.color = 'black';
       document.getElementById('select-IBP').style.backgroundColor = 'white';
       }
}

function do_IBP()
{
   //console.log('do_IBP IBP_band='+ IBP_band);
    clearTimeout(IBP_timer);
    if ( IBP_band >= 5 )
       {
       IBP_band = 0;
       IBP_monitoring = false;
       muted = IBP_muted;
       muted ^= 1;
       toggle_mute();   // restore sound
       select_band(IBP_bands[IBP_band]);
//       demodulator_analog_replace('cwn');
       }
    else 
       {
       IBP_monitoring = true;
       select_band(IBP_bands[IBP_band]);
       muted = 1;
       toggle_mute();   // turn sound on 
       IBP_band++;
       IBP_timer = setTimeout( do_IBP, 10000 );
       }
}

function IBP_monitor(slot) 
{
  if ( !IBP_monitoring && (( IBP_monitorSlot == 20 ) || ( slot == IBP_monitorSlot )))
     {
     IBP_monitoring = true;
     IBP_muted = muted; // to restore state after 
     muted = 1;
     toggle_mute();   
     IBP_band = 1;
     IBP_timer = setTimeout( do_IBP, 8000 );
     }
}
