// Copyright (c) 2017 Peter Jennings, VE3SUN

var ibp_scan_ext_name = 'IBP_scan';		// NB: must match IBP_scan.c:ibp_scan_ext.name

var ibp_first_time = true;
var IBP_run = false;
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

/*		if (param[0] != "keepalive") {
			if (typeof param[1] != "undefined")
				console.log('ibp_recv: '+ param[0] +'='+ param[1]);
			else
				console.log('ibp_recv: '+ param[0]);
		}
*/
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
var mindb_band = [];

function ibp_controls_setup() {

   var data_html =
      time_display_html('IBP_scan') +
      '<div id="id-IBP-report" style="width:1024px; height:200px; overflow:hidden; position:relative; background-color:white;">'+
      '<canvas id="id-IBP-canvas" width="1024" height="200" style="position:absolute">no canvas?</canvas>'+
      '</div>';
    	
   var sl = [];
   WF_length = readCookie('WF_length');
   if ( !WF_length ) WF_length = 10;
   sl[WF_length] = 'selected';   
   
   var sp = [];
   WF_period = readCookie('WF_period');
   if ( !WF_period ) WF_period = 180;
   sp[WF_period] = 'selected';

	var controls_html = w3_divs('id-tc-controls w3-text-white', '',
        w3_div('w3-medium w3-text-aqua', '<b><a href="http://www.ncdxf.org/beacon/index.html" target="_blank">International Beacon Project</a> (IBP) Scanner</b>'),
	w3_col_percent('', '',  w3_div('', 'by VE3SUN'), 25,
				w3_div('', '<span id="PJ">Info: <b><a href="http://ve3sun.com/KiwiSDR/IBP.html" target="_blank">ve3sun.com/KiwiSDR/IBP</a></b></span>'), 55,
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
						'<input id="id-IBP-Autosave" type="checkbox" value="" onclick="IBP_Autosave(this.checked)" title="Save IBP scan table (PNG)"> Autosave IBP'
					)
				)
			)
		),100),
	w3_col_percent('', '',
		w3_table('w3-table-fixed',
			w3_table_row('',
				w3_table_cells('||colspan="2"',
					w3_divs('w3-margin-T-8', 'cl-annotate-checkbox', 
					'<input id="id-IBP-Save-Waterfall" type="checkbox" value="" onclick="IBP_AutosaveWF(this.checked)" title="Save main waterfall section (JPG)"> Autosave Waterfall'
				   )
				),
				
				w3_table_cells('',
					w3_divs('w3-margin-T-8', 'w3-show-inline w3-margin-right',
						'<select id="select-Length" onchange="set_Length(this.value)" title="Length of time to save">'+
						'<option value="-1" disabled selected>Length</option>'+
						'<option value="10" '+sl[10]+'>10 seconds</option>'+
						'<option value="30" '+sl[30]+'>30 seconds</option>'+
						'<option value="60" '+sl[60]+'>1 minute</option>'+
						'<option value="120" '+sl[120]+'>2 minutes</option>'+
						'</select>'
					)
				),
				w3_table_cells('',
					w3_divs('w3-margin-T-8', 'w3-show-inline w3-margin-right',
						'<select id="select-Period" onchange="set_Period(this.value)" title="Repetition time">'+
						'<option value="-1" disabled selected>Period</option>'+
						'<option value="10" '+sp[10]+'>10 seconds</option>'+
						'<option value="30" '+sp[30]+'>30 seconds</option>'+
						'<option value="60" '+sp[60]+'>1 minute</option>'+
						'<option value="180" '+sp[180]+'>3 minutes</option>'+
						'<option value="300" '+sp[300]+'>5 minutes</option>'+
						'<option value="600" '+sp[600]+'>10 minutes</option>'+
						'<option value="900" '+sp[900]+'>15 minutes</option>'+
						'<option value="3600" '+sp[3600]+'>1 hour</option>'+
						'</select>'
					)
				),
			)
		),100),
		
		);
	
	//console.log('ibp_controls_setup');
	ext_panel_show(controls_html, data_html, null);
	ext_set_controls_width_height(450, 130);
	time_display_setup('IBP_scan');
	IBP_scan_resize();
	
	// use extension parameter as beacon station call (or 'cycle' for cycle mode)
	// e.g. kiwisdr.local:8073/?ext=ibp,4u1un (upper or lowercase)
	var call = ext_param();
	if (call) 
	   {
	   var i;
      call = call.toLowerCase();
      for (i = 0; i < 18; i++) 
         {
         if (call == dx_ibp[i*2].toLowerCase())
            break;
         }
      var cycle;
      if ((cycle = (i >= 18 && call == 'cycle')) || i < 18) 
         {
         if (cycle) i = 20;
         console.log('IBP: URL set '+ call);
         w3_el_id('select-IBP').value = i;
         set_IBP(i);
         }
      }
   ibp_autosave = readCookie('IBP_PNG_Autosave');
   if ( ibp_autosave != 'true' ) ibp_autosave = false;
   document.getElementById('id-IBP-Autosave').checked = ibp_autosave;
   
   ibp_autosaveWF = readCookie('IBP_AutosaveWF');
   if ( ibp_autosaveWF != 'true' ) ibp_autosaveWF = false;
   document.getElementById('id-IBP-Save-Waterfall').checked = ibp_autosaveWF;

   var c = document.createElement('canvas');
   c.width = 16; c.height = 1;
   var ctx = c.getContext('2d');
   ctx.fillStyle = "white";
   ctx.fillRect(0, 0, 8, 1);
   ctx.fillStyle = "black";
   ctx.fillRect(8, 0, 8, 1);
   ibp_lineCanvas = c;
   
   var canv = document.getElementById('id-IBP-canvas');
   var label = '';
   if ( canv )
      {
      let canvctx = canv.getContext("2d");
      canvctx.fillStyle="#ffffff";
      canvctx.fillRect(0,0,1024,200);
      
      canvctx.font = "12px Arial";
      canvctx.fillStyle = "red";
      canvctx.textAlign = "center";
      
      for ( let i=0; i < 18; i++)
         {
         label =  dx_ibp[i*2];
         canvctx.fillText(label, 102+i*51, 16); 
         }
      var ibp_freqs = ['14.100','18.110','21.150','24.930','28.200'];
      for ( let i=0; i < 5; i++)
         {
         label =  ibp_freqs[i];
         canvctx.fillText(label, 45, 36*i+40); 
         }
        
      }
	var cookie = readCookie('mindb_band');
	if ( cookie )
	   {
	   mindb_band = JSON.parse(cookie);
	   }

   IBP_run = true;
}


function IBP_scan_resize()
{
	var el = w3_el_id('id-IBP-report');
	var left = (window.innerWidth - 1024 - time_display_width()) / 2;
	el.style.left = px(left);
}

function IBP_scan_blur()
{
   console.log('IBP_scan_blur');
   set_IBP(-1);
   IBP_run = false;
}

function IBP_Autosave(ch){
   ibp_autosave = ch;
   writeCookie('IBP_PNG_Autosave', ibp_autosave);
}

function IBP_AutosaveWF(ch){
   ibp_autosaveWF = ch;
   writeCookie('IBP_AutosaveWF', ibp_autosaveWF);
}

function set_Length(ch) {
   WF_length = ch;
   writeCookie('WF_length', WF_length);
}

function set_Period(ch) {
   WF_period = ch;
   writeCookie('WF_period', WF_period);
   nextAutosave -= (WF_period - ch) * 1000;
}


var IBP_monitorBeacon = -1;
var IBP_band = 0;
var IBP_muted = (typeof muted != "undefined")? muted : 0;
var IBP_bands = [ "IBP 20m", "IBP 17m", "IBP 15m", "IBP 12m", "IBP 10m" ];


var IBP_select = '<select id="select-IBP" onchange="set_IBP(this.value)"><option value="-2" selected disabled>IBP</option><option value="-1">OFF</option>';

    if (typeof dx_ibp != "undefined") {
       IBP_select += '<option value="20">All Bands</option>';
       for( let i=0; i<5; i++) { IBP_select += '<option value="'+(30+i)+'">'+IBP_bands[i]+'</option>'; }   
       IBP_select += '<option value="-1" disabled>By Beacon:</option>';
       for( let i=0; i<18; i++) { IBP_select += '<option value="'+i+'">'+dx_ibp[i*2]+'</option>'; }
       IBP_select += '</select>';    
}

function set_IBP( v )  // called by IBP selector with beacon value
   {
   IBP_band = 0;
   IBP_monitorBeacon = v;
   if ( v >= 30 ) IBP_band = v-30;
   if ( v >= 0 ) select_band(IBP_bands[IBP_band]);
   if ( v < 0 )
      {
      document.getElementById('select-IBP').selectedIndex = 0;
      document.getElementById('id-ext-data-container').style.display = 'none';
   	resize_waterfall_container(true);
      }
   else
      {
      document.getElementById('id-ext-data-container').style.display = 'block';
      }
   }
   
function leadZero(m) { if ( m < 10 ) m = '0'+m; return m; }

function IBP_bandchange( d, BeaconN )
   {
   if ( IBP_monitorBeacon == 20 )
      {
      var band = Math.floor(d.getTime() / 180000)%5; // 3 min per band each 15 min

      if ( band != IBP_band )
         {
         IBP_band = band;
         select_band(IBP_bands[IBP_band]);
         return IBP_band;
         }
      return false;
      }

   if ( IBP_monitorBeacon != (BeaconN+17)%18 ) return false;
   
   IBP_band++;
   IBP_band %= 5;
   select_band(IBP_bands[IBP_band]);
   return IBP_band;
   }


function save_Canvas( d )
   {
   var canv = document.getElementById('id-IBP-canvas');
   if ( canv )
      {
      let canvctx = canv.getContext("2d");
      canvctx.fillStyle="black";
      canvctx.fillText(leadZero(d.getUTCHours())+':'+leadZero(d.getUTCMinutes())+' UTC', 40, 16); 

      let imgURL = canv.toDataURL("image/png");
      let dlLink = document.createElement('a');
      dlLink.download = 'IBP '+d.getUTCFullYear()+(d.getUTCMonth()+1)+d.getUTCDate()+' '+leadZero(d.getUTCHours())+'h'+leadZero(d.getUTCMinutes())+'z.png';
      dlLink.href = imgURL;
      dlLink.dataset.downloadurl = ["image/png", dlLink.download, dlLink.href].join(':');
   
      document.body.appendChild(dlLink);
      dlLink.click();
      document.body.removeChild(dlLink);
      canvctx.fillStyle="#ffffff";
      canvctx.fillRect(0,0,75,19);
      }
   }

function Annotate_waterfall( beaconN, wftime )
   {
   wf_cur_canvas.ctx.strokeStyle="red";
   var al = wf_canvas_actual_line+1;
   if ( al > wf_cur_canvas.height ) al -= 2; // fixes the 1 in 200 lines that go missing - oops, doesn't FIXME - try setting not done and returning
//   wf_cur_canvas.ctx.moveTo(0, al); 
//   wf_cur_canvas.ctx.lineTo(wf_cur_canvas.width, al);  
   
   wf_cur_canvas.ctx.rect(5, al, wf_cur_canvas.width, 1);
   var pattern = wf_cur_canvas.ctx.createPattern(ibp_lineCanvas, 'repeat');
   wf_cur_canvas.ctx.fillStyle = pattern;
   wf_cur_canvas.ctx.fill();
   
   wf_cur_canvas.ctx.strokeStyle = "black";
   wf_cur_canvas.ctx.miterLimit = 2;
   wf_cur_canvas.ctx.lineJoin = "circle";
   wf_cur_canvas.ctx.font = "10px Arial";
   wf_cur_canvas.ctx.fillStyle = "lime";
   
   var aLabel = wftime.toISOString().replace(/T/g,' ').substr(0,19)+'z';

   var f = get_visible_freq_range();
   if ( ((f.center > 14095000) && (f.center < 14105000)) ||
        ((f.center > 18105000) && (f.center < 18115000)) ||
        ((f.center > 21145000) && (f.center < 21155000)) ||
        ((f.center > 24925000) && (f.center < 24935000)) ||
        ((f.center > 28195000) && (f.center < 28205000)) )  aLabel = dx_ibp[beaconN*2] +' '+ dx_ibp[beaconN*2+1];

   wf_cur_canvas.ctx.lineWidth = 3;
   wf_cur_canvas.ctx.strokeText(aLabel,(wf_cur_canvas.width-wf_cur_canvas.ctx.measureText(aLabel).width)/2,wf_canvas_actual_line+12);
   wf_cur_canvas.ctx.lineWidth = 1;
   wf_cur_canvas.ctx.fillText(aLabel,(wf_cur_canvas.width-wf_cur_canvas.ctx.measureText(aLabel).width)/2,wf_canvas_actual_line+12);
   
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
         xcanvas.ctx.strokeText(aLabel,(wf_cur_canvas.width-wf_cur_canvas.ctx.measureText(aLabel).width)/2,wf_canvas_actual_line-wf_cur_canvas.height+12);
         xcanvas.ctx.lineWidth = 1;
         xcanvas.ctx.fillText(aLabel,(wf_cur_canvas.width-wf_cur_canvas.ctx.measureText(aLabel).width)/2,wf_canvas_actual_line-wf_cur_canvas.height+12);
         }
      } 
   }

var IBP_oldSlot = -1;
var canvasSaved = false;
var canvasSavedWF = false;
var nextAutosave = 0;

function IBP_scan_plot( oneline_image )  // openwebrx.js 2877
   {
   var canv = document.getElementById('id-IBP-canvas');
   if ( !canv ) return;
   var ctx = canv.getContext("2d");
   var subset = new ImageData(1,1);
   
   var d = new Date(dx_ibp_server_time_ms + (Date.now() - dx_ibp_local_time_epoch_ms));
   var msec = d.getTime();
   var bsec = Math.floor((msec % 10000)/200); // for 50 pixel slot image
   var slot = Math.floor( msec/10000 ) % 18;
		
   var f = get_visible_freq_range();
   var fb = Math.floor((f.center-14000000)/3000000);
   if ( fb < 0 ) fb = 0;
   
   var plot_y = 20 + 36*fb;
   
   var beaconN = (slot - fb + 18)%18;  // actual beacon transmitting
   var plot_x = 76 + 51*beaconN;
   
   ctx.fillStyle="#ffffff";
   ctx.fillRect(0,plot_y,75,25);
   
   ctx.font = "12px Arial";
   ctx.fillStyle = "red";
   ctx.textAlign = "center";
   ctx.fillText((f.center/1000000).toFixed(3), 45, plot_y+20); 
   
   if ( (IBP_oldSlot > -1) && (IBP_oldSlot != slot) )
      {
      if ( document.getElementById('id-IBP-Annotate') && document.getElementById('id-IBP-Annotate').checked ) Annotate_waterfall( (beaconN+17)%18, d ); 
      
   	if ( ibp_autosave )
      	{
      	if ( slot ) canvasSaved = false;  // saves occur at slot 0
         else
            {
            if ( !canvasSaved )
               {
               ctx.fillStyle="red";
               ctx.fillRect(plot_x-1,plot_y,1,35);  // mark save time on canvas
   
               save_Canvas(d);
               canvasSaved = true;
               
//   document.getElementById('PJ').innerHTML = LineCount;
//   LineCount = 0;
   
               ctx.fillStyle="#ffffff";
               ctx.fillRect(plot_x-1,plot_y,1,35);  // unmark it
               }
            }		
         }
         
   	if ( ibp_autosaveWF && ( msec >= nextAutosave ) )
         {
         export_waterfall( f.center, WF_length )
         canvasSavedWF = true;
         nextAutosave = msec-5000 + 1000 * WF_period;
         } 
         
      var new_band = IBP_bandchange( d, beaconN );
      if ( new_band !== false )  // returns new band if band changed, else false
         {
         if ( mindb_band[new_band] && (mindb != mindb_band[new_band]) ) setmindb(true,mindb_band[new_band]);
         IBP_oldSlot = -2;
         return;
         }
      }

   
   if ( IBP_oldSlot != slot )
      {
      if ( muted && (IBP_monitorBeacon == beaconN) )
         {
         toggle_mute();
         setTimeout(function() { toggle_mute()}, 50000 );
         }
      ctx.fillStyle="#000055";
      ctx.fillRect(plot_x,plot_y,50,35);
      }
   else
      {
      if ( mindb_band[fb] != mindb )
         { 
         mindb_band[fb] = mindb;
         writeCookie('mindb_band',JSON.stringify(mindb_band));
         }
      }
   for ( let i = 495; i < 530; i++ )
      {
      for ( let j = 0; j < 4; j++ )
        {
        subset.data[j] = oneline_image.data[4*i+j];
        ctx.putImageData(subset, plot_x+bsec, plot_y+i-495);
        }
      }
   IBP_oldSlot = slot;
   }
   
