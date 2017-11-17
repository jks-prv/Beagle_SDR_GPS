// Copyright (c) 2017 Peter Jennings, VE3SUN

var ibp_scan_ext_name = 'IBP_scan';		// NB: must match IBP_scan.c:ibp_scan_ext.name

var ibp_first_time = true;
var ibp_autosave = false;

function IBP_scan_main()
   {
   //console.log('IBP_scan_main');
    ext_switch_to_client(ibp_scan_ext_name, ibp_first_time, ibp_recv_msg);		// tell server to use us (again)
    if ( !ibp_first_time ) ibp_controls_setup();
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

var ibp_lineCanvas;

function ibp_controls_setup()
{
	var controls_html = w3_divs('id-tc-controls w3-text-white', '',
        w3_div('w3-medium w3-text-aqua', '<b>International Beacon Project (IBP) Scanner</b>'),
	w3_col_percent('', '',  w3_div('', 'by VE3SUN'), 25,
				w3_div('', 'See also <b><a href="http://ve3sun.com/KiwiSDR/index.php" target="_blank">ve3sun.com/KiwiSDR</a></b>'), 55,
				'', 10
		       ),
	w3_col_percent('', '',
		w3_table('w3-table-fixed w3-centered',
			w3_table_row('',
				w3_table_cells('',
					w3_divs('w3-margin-T-8', 'w3-show-inline w3-margin-right', IBP_select )
				),
				
				w3_table_cells('||colspan="2"',
					w3_divs('w3-margin-T-8', 'cl-annotate-checkbox w3-padding-L-16',
						'<input id="id-IBP-Annotate" type="checkbox" value="" checked> Annotate Waterfall'
					)
				),
				w3_table_cells('||colspan="2"',
					w3_divs('w3-margin-T-8', 'cl-annotate-checkbox',
						'<input id="id-IBP-Autosave" type="checkbox" value="" onclick="IBP_Autosave(this.checked)"> Autosave JPG'
					)
				),
			)
		),100)
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
   document.getElementById('id-IBP-Autosave').checked = ibp_autosave;
   
   var c = document.createElement('canvas');
   c.width = 16; c.height = 1;
   var ctx = c.getContext('2d');
   ctx.fillStyle = "white";
   ctx.fillRect(0, 0, 8, 1);
   ctx.fillStyle = "black";
   ctx.fillRect(8, 0, 8, 1);
   ibp_lineCanvas = c;
}


function IBP_scan_blur()
{
   //console.log('IBP_scan_blur');
   set_IBP(-1);
}

function IBP_Autosave(ch){
   ibp_autosave = ch;
}


var IBP_monitorSlot = -1;
var IBP_monitoring = false;
var IBP_timer;
var IBP_band = 0;
var IBP_muted = (typeof muted != "undefined")? muted : 0;
var IBP_bands = [ "IBP 20m", "IBP 17m", "IBP 15m", "IBP 12m", "IBP 10m" ];


var IBP_select = '<select id="select-IBP" onchange="set_IBP(this.value)"><option value="-2" selected="" disabled="">IBP</option><option value="-1">OFF</option>';

    if (typeof dx_ibp != "undefined") {
       for( var i=0; i<18; i++) { IBP_select += '<option value="'+i+'">'+dx_ibp[i*2]+'</option>'; }
       IBP_select += '<option value="20">Cycle</option></select>';
}

function set_IBP( v )  // called by IBP selector with slot value
{
   //console.log('set_IBP v='+ v);
    clearTimeout(IBP_timer);
    IBP_band = 0;
    IBP_monitorSlot = v;
    IBP_monitoring = false;
    select_band(IBP_bands[IBP_band]);
    if ( v < 0 )
       {
       document.getElementById('select-IBP').selectedIndex = 0;
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

var IBP_slot_done = -1;

function IBP_monitor(slot) {  
//    console.log(slot);
    if ( (slot != IBP_slot_done) && (document.getElementById('id-IBP-Annotate') && document.getElementById('id-IBP-Annotate').checked) )
      {
      IBP_slot_done = slot;
      wf_cur_canvas.ctx.strokeStyle="red";
      var al = wf_canvas_actual_line+1;
      if ( al > wf_cur_canvas.height ) al -= 2; // fixes the 1 in 200 lines that go missing 
      wf_cur_canvas.ctx.moveTo(0, al); 
      wf_cur_canvas.ctx.lineTo(wf_cur_canvas.width, al);  
      
      wf_cur_canvas.ctx.rect(0, al, wf_cur_canvas.width, 1);
      var pattern = wf_cur_canvas.ctx.createPattern(ibp_lineCanvas, 'repeat');
      wf_cur_canvas.ctx.fillStyle = pattern;
      wf_cur_canvas.ctx.fill();
      
      wf_cur_canvas.ctx.strokeStyle = "black";
      wf_cur_canvas.ctx.miterLimit = 2;
      wf_cur_canvas.ctx.lineJoin = "circle";
      wf_cur_canvas.ctx.font = "10px Arial";
      wf_cur_canvas.ctx.fillStyle = "lime";
      
      var f = get_visible_freq_range();
   
      var fb = Math.floor((f.center-14000000)/3000000);
      var sx = slot - fb -1;
      if  (sx < 0 ) sx += 18;
      var sL = dx_ibp[sx*2] +' '+ dx_ibp[sx*2+1];;
      wf_cur_canvas.ctx.lineWidth = 3;
      wf_cur_canvas.ctx.strokeText(sL,(wf_cur_canvas.width-wf_cur_canvas.ctx.measureText(sL).width)/2,wf_canvas_actual_line+12);
      wf_cur_canvas.ctx.lineWidth = 1;
      wf_cur_canvas.ctx.fillText(sL,(wf_cur_canvas.width-wf_cur_canvas.ctx.measureText(sL).width)/2,wf_canvas_actual_line+12);
      
     if ( wf_canvas_actual_line+10 >   wf_cur_canvas.height )  // overlaps end of canvas
         {
         var xcanvas = wf_canvases[1];
         if ( xcanvas )
            {
            xcanvas.ctx = xcanvas.getContext("2d");
            xcanvas.ctx.strokeStyle = "black";
            xcanvas.ctx.miterLimit = 2;
            xcanvas.ctx.lineJoin = "circle";
            xcanvas.ctx.font = "10px Arial";
            xcanvas.ctx.fillStyle = "lime";
            xcanvas.ctx.lineWidth = 3;
            xcanvas.ctx.strokeText(sL,(wf_cur_canvas.width-wf_cur_canvas.ctx.measureText(sL).width)/2,wf_canvas_actual_line-wf_cur_canvas.height+12);
            xcanvas.ctx.lineWidth = 1;
            xcanvas.ctx.fillText(sL,(wf_cur_canvas.width-wf_cur_canvas.ctx.measureText(sL).width)/2,wf_canvas_actual_line-wf_cur_canvas.height+12);
            }
         } 
       if ( sx == 17 && ibp_autosave && document.getElementById('id-IBP-Autosave').checked )
          {
          export_waterfall( 0 ) 
          }
       }
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
