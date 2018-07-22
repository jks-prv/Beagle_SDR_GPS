// Copyright (c) 2017 Peter Jennings, VE3SUN

var ibp_scan_ext_name = 'IBP_scan';    // NB: must match IBP_scan.c:ibp_scan_ext.name

var ibp_first_time = true;
var ibp_run = false;
var ibp_autosave = false;

function IBP_scan_main()
   {
   //console.log('IBP_scan_main');
    ext_switch_to_client(ibp_scan_ext_name, ibp_first_time, ibp_recv_msg);    // tell server to use us (again)
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

/*    if (param[0] != "keepalive") {
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
      w3_div('id-IBP-report|width:1024px; height:200px; overflow:hidden; position:relative; background-color:white;',
         '<canvas id="id-IBP-canvas" width="1024" height="200" style="position:absolute"></canvas>'
      );

   var controls_html =
      w3_div('id-tc-controls w3-text-white',
         w3_div('w3-medium w3-text-aqua',
            '<b><a href="http://www.ncdxf.org/beacon/index.html">International Beacon Project</a> (IBP) Scanner</b>'
         ),

         w3_col_percent('',
            w3_div('', 'by VE3SUN'), 25,
            w3_div('', 'Info: <b><a href="http://ve3sun.com/KiwiSDR/IBP.html" target="_blank">ve3sun.com/KiwiSDR/IBP</a></b>'), 55,
            '', 10
         ),
         
         w3_inline('w3-halign-space-between|width:90%;/',
            w3_divs('w3-margin-T-8/w3-show-inline w3-left w3-margin-right', IBP_select),
            w3_divs('w3-margin-T-8/cl-ibp-annotate-checkbox w3-padding-L-16',
               '<input id="id-IBP-Annotate" class="w3-pointer" type="checkbox" value="" checked> Annotate Waterfall'
            ),
            w3_divs('w3-margin-T-8/cl-ibp-annotate-checkbox',
               '<input id="id-IBP-Autosave" class="w3-pointer" type="checkbox" value="" onclick="IBP_Autosave(this.checked)"> Autosave PNG'
            )
         )
      );
   
   //console.log('ibp_controls_setup');
   ext_panel_show(controls_html, data_html, null);
   ext_set_controls_width_height(475, 90);
   time_display_setup('IBP_scan');
	IBP_environment_changed( {resize:1} );
   
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
         w3_el('select-IBP').value = i;
         set_IBP(i);
         }
      }
   ibp_autosave = readCookie('IBP_PNG_Autosave');
   if ( ibp_autosave != 'true' ) ibp_autosave = false;
   document.getElementById('id-IBP-Autosave').checked = ibp_autosave;
   
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
      var ctx = canv.getContext("2d");
      ctx.fillStyle="#ffffff";
      ctx.fillRect(0,0,1024,200);
//      ctx.fillRect(0,0,1024,19);
//      ctx.fillRect(0,0,75,200);
      
      ctx.font = "12px Arial";
      ctx.fillStyle = "red";
      ctx.textAlign = "center";
      
      for ( var i=0; i < 18; i++)
         {
         label =  dx_ibp[i*2];
         ctx.fillText(label, 102+i*51, 16); 
         }
      var ibp_freqs = ['14.100','18.110','21.150','24.930','28.200'];
      for ( var i=0; i < 5; i++)
         {
         label =  ibp_freqs[i];
         ctx.fillText(label, 45, 36*i+40); 
         }
        
      }
   var cookie = readCookie('mindb_band');
   if ( cookie )
      {
      mindb_band = JSON.parse(cookie);
      }

   ibp_run = true;
}


function IBP_environment_changed(changed)
{
   if (!changed.resize) return;
   var el = w3_el('id-IBP-report');
   var left = (window.innerWidth - 1024 - time_display_width()) / 2;
   el.style.left = px(left);
}

function IBP_scan_blur()
{
   console.log('IBP_scan_blur');
   set_IBP(-1);
   ibp_run = false;
}

function IBP_Autosave(ch){
   ibp_autosave = ch;
   writeCookie('IBP_PNG_Autosave', ibp_autosave);
}


var IBP_monitorBeacon = -1;
var IBP_sound = false;
var IBP_band = 0;
var IBP_muted = (typeof muted != "undefined")? muted : 0;
var IBP_bands = [ "IBP 20m", "IBP 17m", "IBP 15m", "IBP 12m", "IBP 10m" ];


var IBP_select = '<select id="select-IBP" class="w3-pointer" onchange="set_IBP(this.value)"><option value="-2" selected="" disabled="">IBP</option><option value="-1">OFF</option>';

    if (typeof dx_ibp != "undefined") {
       for( var i=0; i<18; i++) { IBP_select += '<option value="'+i+'">'+dx_ibp[i*2]+'</option>'; }
       IBP_select += '<option value="-1" disabled><b>By band:</b></option>';
       IBP_select += '<option value="20">All Bands</option>';
       for( var i=0; i<5; i++) { IBP_select += '<option value="'+(30+i)+'">'+IBP_bands[i]+'</option>'; }   
       IBP_select += '</select>';    
}

// If menu has ever been selected then we restore band to 20m on blur,
// else leave alone so e.g. zoom won't change.
var IBP_selected = false;

function set_IBP( v )  // called by IBP selector with beacon value
   {
   if (v >= 0) IBP_selected = true;
   IBP_band = 0;
   IBP_monitorBeacon = v;
   IBP_sound = false;
   if ( v >= 30 ) IBP_band = v-30;
   if ( v < 0 )
      {
      document.getElementById('select-IBP').selectedIndex = 0;
      if (IBP_selected) select_band(IBP_bands[0]);
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
      var ctx = canv.getContext("2d");
      ctx.fillStyle="black";
      ctx.fillText(leadZero(d.getUTCHours())+':'+leadZero(d.getUTCMinutes())+' UTC', 40, 16); 

      var imgURL = canv.toDataURL("image/png");
      var dlLink = document.createElement('a');
      dlLink.download = 'IBP '+d.getUTCFullYear()+(d.getUTCMonth()+1)+d.getUTCDate()+' '+leadZero(d.getUTCHours())+'h'+leadZero(d.getUTCMinutes())+'z.png';
      dlLink.href = imgURL;
      dlLink.dataset.downloadurl = ["image/png", dlLink.download, dlLink.href].join(':');
   
      document.body.appendChild(dlLink);
      dlLink.click();
      document.body.removeChild(dlLink);
      ctx.fillStyle="#ffffff";
      ctx.fillRect(0,0,75,19);
      }
   }

function Annotate_waterfall( beaconN )
   {
   wf_cur_canvas.ctx.strokeStyle="red";
   var al = wf_canvas_actual_line+1;
   if ( al > wf_cur_canvas.height ) al -= 2; // fixes the 1 in 200 lines that go missing - oops, doesn't FIXME - try setting not done and returning
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
   
   var sL = dx_ibp[beaconN*2] +' '+ dx_ibp[beaconN*2+1];;

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
   }

var IBP_oldSlot = -1;
var canvasSaved = false;

function IBP_scan_plot( oneline_image )
   {
   if (!ibp_run) return;
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
   var plot_y = 20 + 36*fb;
   
   var beaconN = (slot - fb + 18)%18;  // actual beacon transmitting
   var plot_x = 76 + 51*beaconN;
   
   if ( ibp_autosave )
      {
      if ( slot ) canvasSaved = false;
      else
         {
         if ( !canvasSaved )
            {
            ctx.fillStyle="red";
            ctx.fillRect(plot_x-1,plot_y,1,35);  // mark save time on canvas

            save_Canvas(d);
            canvasSaved = true;

            ctx.fillStyle="#ffffff";
            ctx.fillRect(plot_x-1,plot_y,1,35);  // unmark it
            }
         }     
      }

   if ( (IBP_oldSlot > -1) && (IBP_oldSlot != slot) )
      {
      if ( document.getElementById('id-IBP-Annotate') && document.getElementById('id-IBP-Annotate').checked ) Annotate_waterfall( (beaconN+17)%18 ); 
      
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
         toggle_or_set_mute();
         setTimeout(function() { toggle_or_set_mute()}, 50000 );
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
   for ( var i = 495; i < 530; i++ )
      {
      for ( var j = 0; j < 4; j++ )
        {
        subset.data[j] = oneline_image.data[4*i+j];
        ctx.putImageData(subset, plot_x+bsec, plot_y+i-495);
        }
      }
   IBP_oldSlot = slot;
   }
