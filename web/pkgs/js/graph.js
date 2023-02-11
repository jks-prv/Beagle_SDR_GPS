
function graph_init(canvas, opt)
{
   gr = {};

   // options
   gr.auto = 1;
   gr.dBm = 0;
   gr.db_units = 'dB';
   gr.speed = 1;
   gr.marker = -1;
   gr.x_tick = 0;
   gr.threshold = 0;
   gr.threshold_color = 'red';
   gr.averaging = false;
   gr.timestamp = false;
   gr.UTC = true;
   gr.avg_dB = 0;
   gr.divider = false;
   
   gr.max = 0;
   gr.min = 0;
   gr.hi = 0;
   gr.lo = 0;
   gr.goalH = 0;
   gr.goalL = 0;
   gr.scroll = 0;
   gr.scaleWidth = 60;
   gr.hysteresis = 15;
   gr.padding_tb = 0;
   gr.xi = 0;					// initial x value as grid scrolls left until panel full
   gr.secs_last = 0;
   gr.redraw_scale = false;
   gr.clr_color = 'mediumBlue';
   gr.bg_color = 'ghostWhite';
   gr.grid_color = 'lightGrey';
   gr.scale_color = 'white';
   
   gr.last_last = 0;

   gr.cv = canvas;
   gr.ct = canvas.ctx;

   gr.auto = 1;
   gr.dBm = w3_opt(opt, 'dBm', 0);
   gr.speed = w3_opt(opt, 'speed', 1);
   gr.averaging = w3_opt(opt, 'averaging', false);
   gr.timestamp = w3_opt(opt, 'timestamp', false);
   gr.UTC = w3_opt(opt, 'UTC', true);
   gr.plot_color = w3_opt(opt, 'color', 'black');
   
   if (gr.dBm) {
      gr.padding_tb = 20;
      gr.db_units = 'dBm';
   } else {
      gr.db_units = 'dB';
      gr.padding_tb = 0;
   }
   
   return gr;
}

function graph_clear(gr)
{
	var ct = gr.ct;
	ct.fillStyle = gr.clr_color;
	ct.fillRect(0,0, gr.cv.width - gr.scaleWidth, gr.cv.height);
	gr.x_tick = 0;
	gr.xi = gr.cv.width - gr.scaleWidth;
	graph_rescale(gr);
}

// FIXME: handle window resize
function graph_rescale(gr)
{
   gr.redraw_scale = true;
}

function graph_mode(gr, scale, max, min)
{
   gr.auto = (scale == 'auto')? 1:0
   if (!gr.auto) { gr.max = max; gr.min = min; }
   //console.log('gr.auto='+ gr.auto +' max='+ max +' min='+ min);
   graph_rescale(gr);
}

function graph_speed(gr, speed)
{
   if (speed < 1) speed = 1;
   gr.speed = speed;
}

function graph_marker(gr, marker)
{
   gr.marker = marker;
}

function graph_annotate(gr, color)
{
   gr.divider = color;
}

function graph_threshold(gr, threshold_dB, color)
{
   gr.threshold = threshold_dB;
   if (isString(color)) gr.threshold_color = color;
}

function graph_averaging(gr, averaging)
{
   gr.averaging = averaging;
   gr.avg_dB = 0;
}

function graph_timestamp(gr, timestamp)
{
   gr.timestamp = timestamp;
}

function graph_UTC(gr, UTC)
{
   gr.UTC = UTC;
}

function graph_plot(gr, val_dB, opt)
{
   var plot_color = w3_opt(opt, 'color', gr.plot_color);
   var ylast = w3_opt(opt, 'line', false);
   
   var cv = gr.cv;
   var ct = gr.ct;
   var w = cv.width - gr.scaleWidth;
   var h = cv.height;
   var wi = w-gr.xi;
	var range = gr.hi - gr.lo;

	var y_dB = function(dB) {
		var norm = (dB - gr.lo) / range;
		return Math.round(h - (norm * h));
	};

	var gridC_10dB = function(dB) { return 10 * Math.ceil(dB/10); };
	var gridF_10dB = function(dB) { return 10 * Math.floor(dB/10); };

	// re-scale existing part of plot if manual range changed or in auto-range mode
	var force_recomp = false;
   if (gr.redraw_scale || gr.auto) {
   	if (gr.auto) {
			var converge_rate_dB = 0.25 / gr.speed;
			gr.goalH = (gr.goalH > (val_dB+gr.padding_tb))? (gr.goalH - converge_rate_dB) : (val_dB+gr.padding_tb);
			gr.goalL = (gr.goalL < (val_dB-gr.padding_tb))? (gr.goalL + converge_rate_dB) : (val_dB-gr.padding_tb);
		} else {
			gr.goalH = gr.max;
			gr.goalL = gr.min;
			force_recomp = true;
		}

		if (gr.goalL > gr.lo+gr.hysteresis || gr.goalL < gr.lo || force_recomp) { 
			var new_lo = gridF_10dB(gr.goalL)-5;
			var new_range = gr.hi - new_lo;

			if (new_lo <= gr.lo) {
				// shrink previous grid above with new background below
				var shrink = range / new_range;
				if (wi) {
					ct.drawImage(cv, gr.xi,0, wi,h, gr.xi,0, wi,h*shrink);
					ct.fillStyle = gr.bg_color;
					ct.fillRect(gr.xi,Math.floor(shrink*h), wi,h*(1-shrink)+1);
				}
			} else {
				// upper portion of previous grid expands to fill entire space
				var expand = new_range / range;
				if (wi) ct.drawImage(cv, gr.xi,0, wi,h*expand, gr.xi,0, wi,h);
			}

			gr.lo = new_lo;
			range = new_range;
			if (gr.auto)
				gr.redraw_scale = true;
		}
		
		if (gr.goalH > gr.hi || gr.goalH < gr.hi-gr.hysteresis || force_recomp) {
			var new_hi = gridC_10dB(gr.goalH)+5;
			var new_range = new_hi - gr.lo;
			if (new_hi >= gr.hi) {
				// shrink previous grid below with new background above
				var shrink = range / new_range;
				if (wi) {
					ct.drawImage(cv, gr.xi,0, wi,h, gr.xi,h*(1-shrink), wi,h*shrink);
					ct.fillStyle = gr.bg_color;
					ct.fillRect(gr.xi,0, wi,Math.ceil(h*(1-shrink)));
				}
			} else {
				// lower portion of previous grid expands to fill entire space
				var expand = new_range / range;
				if (wi) ct.drawImage(cv, gr.xi,h*(1-expand), wi,h*expand, gr.xi,0, wi,h);
			}
			gr.hi = new_hi;
			range = new_hi - gr.lo;
			if (gr.auto)
				gr.redraw_scale = true;
		}
	}

   if (gr.redraw_scale) {
		ct.fillStyle = gr.clr_color;
		ct.fillRect(w,0, gr.scaleWidth,h);
      ct.fillStyle = gr.scale_color;
      ct.font = '10px Verdana';
      for (var dB = gr.lo; (dB = gridC_10dB(dB)) <= gr.hi; dB++) {
         ct.fillText(dB +' '+ gr.db_units, w+2,y_dB(dB)+4, gr.scaleWidth);
      }
      gr.redraw_scale = false;
   }
   
   var plot = false;

   if (++gr.scroll >= gr.speed) {
      gr.scroll = 0;
      if (gr.xi > 0) gr.xi--;
      
      // shift entire window left 1px to make room for new line
      ct.drawImage(cv, 1,0,w-1,h, 0,0,w-1,h);

      gr.date = new Date();
      var secs = gr.date.getTime() / 1000;
      var now = Math.floor(secs / gr.marker);
      var then = Math.floor(gr.secs_last / gr.marker);

      if (gr.marker == -1 || now == then) {
			// draw dB level lines by default
         ct.fillStyle = gr.bg_color;
         ct.fillRect(w-1,0, 1,h);
         ct.fillStyle = gr.grid_color;
         for (var dB = gr.lo; (dB = gridC_10dB(dB)) <= gr.hi; dB++) {
            ct.fillRect(w-1,y_dB(dB), 1,1);
         }
      } else {
         // time to draw time marker
         var color = gr.grid_color;
         if (gr.timestamp && gr.x_tick == 0) {
            gr.time_mark = gr.date;
            gr.x_tick = 1;
            color = 'blue';
         }

         gr.secs_last = secs;
         ct.fillStyle = color;
         ct.fillRect(w-1,0, 1,h);
      }

      if (gr.x_tick == 100) {
         var tstamp = gr.UTC? (gr.time_mark.toUTCString().substr(17,8) +' UTC') : (gr.time_mark.toString().substr(16,8) +' L');
         var xt = w - 90 + ct.measureText(tstamp).width / 2;
	      w3_fillText(ct, xt, h-10, tstamp, 'black', '14px Arial', 1);
      }
      if (gr.x_tick == 300) gr.x_tick = 0;
      if (gr.x_tick) gr.x_tick++;

      if (gr.divider != false) {
         var save_color = ct.fillStyle;
            ct.fillStyle = gr.divider;
            ct.fillRect(w-1,0, 1,h);
            gr.divider = false;
         ct.fillStyle = save_color;
      }
      
      if (gr.threshold) {
         ct.fillStyle = gr.threshold_color;
         ct.fillRect(w-1,y_dB(gr.threshold), 1,1);
      }
      
      plot = true;
   }
   
   if (gr.averaging) {
      var new_dB = ((gr.avg_dB * (gr.speed-1)) + val_dB) / gr.speed;
      val_dB = gr.avg_dB = new_dB;
   }

   // if not averaging always plot, even if not shifting display
   var rv = null;
   if (!gr.averaging || plot || ylast != false) {
      var y = y_dB(val_dB);
      if (ylast != false) {
         ct.strokeStyle = plot_color;
         ct.beginPath();
         ct.moveTo(w-1, ylast);
         ct.lineTo(w, y);
         //if (plot_color == 'black') console.log((w-1) +','+ ylast +' -> '+ w +','+ y);
         rv = y;
         ct.stroke();
         
         //ct.fillStyle = plot_color;
         //var h = y - ylast;
         //if (h == 0) h = 1
         //ct.fillRect(w-1,ylast, 1,h);
      } else {
         ct.fillStyle = plot_color;
         ct.fillRect(w-1,y, 1,1);
      }
   }
   
   return rv;
}
