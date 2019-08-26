
// NB: not re-entrant

var gr = {
   // options
   auto: 1,
   dBm: 0,
   db_units: 'dB',
   speed: 1,
   marker: 1,
   threshold: 0,
   averaging: false,
   avg_dB: 0,
   divider: false,
   
   max: 0,
   min: 0,
   hi: 0,
   lo: 0,
   goalH: 0,
   goalL: 0,
   scroll: 0,
   scaleWidth: 60,
   hysteresis: 15,
   padding_tb: 0,
   xi: 0,					// initial x value as grid scrolls left until panel full
   secs_last: 0,
   redraw_scale: false,
   clr_color: 'mediumBlue',
   bg_color: 'ghostWhite',
   grid_color: 'lightGrey',
   scale_color: 'white',
   plot_color: 'black'
};

function graph_init(canvas, opt)
{
   gr.cv = canvas;
   gr.ct = canvas.ctx;

   gr.auto = 1;
   gr.dBm = opt.dBm || 0;
   gr.speed = opt.speed || 1;
   gr.averaging = opt.averaging || false;
   
   if (gr.dBm) {
      gr.padding_tb = 20;
      gr.db_units = 'dBm';
   } else {
      gr.db_units = 'dB';
      gr.padding_tb = 0;
   }
}

function graph_clear()
{
	var ct = gr.ct;
	ct.fillStyle = gr.clr_color;
	ct.fillRect(0,0, gr.cv.width - gr.scaleWidth, gr.cv.height);
	gr.xi = gr.cv.width - gr.scaleWidth;
	graph_rescale();
}

// FIXME: handle window resize
function graph_rescale()
{
   gr.redraw_scale = true;
}

function graph_mode(auto, max, min)
{
   gr.auto = auto;
   if (!gr.auto) { gr.max = max; gr.min = min; }
   //console.log('gr.auto='+ gr.auto +' max='+ max +' min='+ min);
   graph_rescale();
}

function graph_speed(speed)
{
   if (speed < 1) speed = 1;
   gr.speed = speed;
}

function graph_marker(marker)
{
   gr.marker = marker;
}

function graph_divider(color)
{
   gr.divider = color;
}

function graph_threshold(threshold_dB)
{
   gr.threshold = threshold_dB;
}

function graph_averaging(averaging)
{
   gr.averaging = averaging;
   gr.avg_dB = 0;
}

function graph_plot(val_dB)
{
   var cv = gr.cv;
   var ct = gr.ct;
   var w = cv.width - gr.scaleWidth;
   var h = cv.height;
   var wi = w-gr.xi;
	var range = gr.hi - gr.lo;

	var y_dB = function(dB) {
		var norm = (dB - gr.lo) / range;
		return h - (norm * h);
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

      var secs = new Date().getTime() / 1000;
      var now = Math.floor(secs / gr.marker);
      var then = Math.floor(gr.secs_last / gr.marker);

      if (now == then) {
			// draw dB level lines by default
         ct.fillStyle = gr.bg_color;
         ct.fillRect(w-1,0, 1,h);
         ct.fillStyle = gr.grid_color;
         for (var dB = gr.lo; (dB = gridC_10dB(dB)) <= gr.hi; dB++) {
            ct.fillRect(w-1,y_dB(dB), 1,1);
         }
      } else {
         // time to draw time marker
         gr.secs_last = secs;
         ct.fillStyle = gr.grid_color;
         ct.fillRect(w-1,0, 1,h);
      }

      if (gr.divider != false) {
         var save_color = ct.fillStyle;
            ct.fillStyle = gr.divider;
            ct.fillRect(w-1,0, 1,h);
            gr.divider = false;
         ct.fillStyle = save_color;
      }
      
      if (gr.threshold) {
         ct.fillStyle = 'red';
         ct.fillRect(w-1,y_dB(gr.threshold), 1,1);
      }
      
      plot = true;
   }
   
   if (gr.averaging) {
      var new_dB = ((gr.avg_dB * (gr.speed-1)) + val_dB) / gr.speed;
      val_dB = gr.avg_dB = new_dB;
   }

   // if not averaging always plot, even if not shifting display
   if (!gr.averaging || plot) {
      ct.fillStyle = gr.plot_color;
      ct.fillRect(w-1,y_dB(val_dB), 1,1);
   }
}
