var sm = {
   // options
   auto: 1,
   dBm: 0,
   db_units: 'dB',
   speed: 1,
   marker: 1,
   threshold: 0,
   
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
   sm.cv = canvas;
   sm.ct = canvas.ctx;

   sm.auto = 1;
   sm.dBm = opt.dBm || 0;
   
   if (sm.dBm) {
      sm.padding_tb = 20;
      sm.db_units = 'dBm';
   } else {
      sm.db_units = 'dB';
      sm.padding_tb = 0;
   }
}

function graph_clear()
{
	var ct = sm.ct;
	ct.fillStyle = sm.clr_color;
	ct.fillRect(0,0, sm.cv.width - sm.scaleWidth, sm.cv.height);
	sm.xi = sm.cv.width - sm.scaleWidth;
	graph_rescale();
}

// FIXME: handle window resize
function graph_rescale()
{
   sm.redraw_scale = true;
}

function graph_mode(auto, max, min)
{
   sm.auto = auto;
   if (!sm.auto) { sm.max = max; sm.min = min; }
   //console.log('sm.auto='+ sm.auto +' max='+ max +' min='+ min);
   graph_rescale();
}

function graph_speed(speed)
{
   sm.speed = speed;
}

function graph_marker(marker)
{
   sm.marker = marker;
}

function graph_threshold(threshold_dB)
{
   sm.threshold = threshold_dB;
}

function graph_plot(val_dB)
{
   var cv = sm.cv;
   var ct = sm.ct;
   var w = cv.width - sm.scaleWidth;
   var h = cv.height;
   var wi = w-sm.xi;
	var range = sm.hi - sm.lo;

	var y_dB = function(dB) {
		var norm = (dB - sm.lo) / range;
		return h - (norm * h);
	};

	var gridC_10dB = function(dB) { return 10 * Math.ceil(dB/10); };
	var gridF_10dB = function(dB) { return 10 * Math.floor(dB/10); };

	// re-scale existing part of plot if manual range changed or in auto-range mode
	var force_recomp = false;
   if (sm.redraw_scale || sm.auto) {
   	if (sm.auto) {
			var converge_rate_dB = 0.25 / sm.speed;
			sm.goalH = (sm.goalH > (val_dB+sm.padding_tb))? (sm.goalH - converge_rate_dB) : (val_dB+sm.padding_tb);
			sm.goalL = (sm.goalL < (val_dB-sm.padding_tb))? (sm.goalL + converge_rate_dB) : (val_dB-sm.padding_tb);
		} else {
			sm.goalH = sm.max;
			sm.goalL = sm.min;
			force_recomp = true;
		}

		if (sm.goalL > sm.lo+sm.hysteresis || sm.goalL < sm.lo || force_recomp) { 
			var new_lo = gridF_10dB(sm.goalL)-5;
			var new_range = sm.hi - new_lo;

			if (new_lo <= sm.lo) {
				// shrink previous grid above with new background below
				var shrink = range / new_range;
				if (wi) {
					ct.drawImage(cv, sm.xi,0, wi,h, sm.xi,0, wi,h*shrink);
					ct.fillStyle = sm.bg_color;
					ct.fillRect(sm.xi,Math.floor(shrink*h), wi,h*(1-shrink)+1);
				}
			} else {
				// upper portion of previous grid expands to fill entire space
				var expand = new_range / range;
				if (wi) ct.drawImage(cv, sm.xi,0, wi,h*expand, sm.xi,0, wi,h);
			}

			sm.lo = new_lo;
			range = new_range;
			if (sm.auto)
				sm.redraw_scale = true;
		}
		
		if (sm.goalH > sm.hi || sm.goalH < sm.hi-sm.hysteresis || force_recomp) {
			var new_hi = gridC_10dB(sm.goalH)+5;
			var new_range = new_hi - sm.lo;
			if (new_hi >= sm.hi) {
				// shrink previous grid below with new background above
				var shrink = range / new_range;
				if (wi) {
					ct.drawImage(cv, sm.xi,0, wi,h, sm.xi,h*(1-shrink), wi,h*shrink);
					ct.fillStyle = sm.bg_color;
					ct.fillRect(sm.xi,0, wi,Math.ceil(h*(1-shrink)));
				}
			} else {
				// lower portion of previous grid expands to fill entire space
				var expand = new_range / range;
				if (wi) ct.drawImage(cv, sm.xi,h*(1-expand), wi,h*expand, sm.xi,0, wi,h);
			}
			sm.hi = new_hi;
			range = new_hi - sm.lo;
			if (sm.auto)
				sm.redraw_scale = true;
		}
	}

   if (sm.redraw_scale) {
		ct.fillStyle = sm.clr_color;
		ct.fillRect(w,0, sm.scaleWidth,h);
      ct.fillStyle = sm.scale_color;
      ct.font = '10px Verdana';
      for (var dB = sm.lo; (dB = gridC_10dB(dB)) <= sm.hi; dB++) {
         ct.fillText(dB +' '+ sm.db_units, w+2,y_dB(dB)+4, sm.scaleWidth);
      }
      sm.redraw_scale = false;
   }

   if (++sm.scroll >= sm.speed) {
      sm.scroll = 0;
      if (sm.xi > 0) sm.xi--;
      
      // shift entire window left 1px to make room for new line
      ct.drawImage(cv, 1,0,w-1,h, 0,0,w-1,h);

      var secs = new Date().getTime() / 1000;
      var now = Math.floor(secs / sm.marker);
      var then = Math.floor(sm.secs_last / sm.marker);

      if (now == then) {
			// draw dB level lines by default
         ct.fillStyle = sm.bg_color;
         ct.fillRect(w-1,0, 1,h);
         ct.fillStyle = sm.grid_color;
         for (var dB = sm.lo; (dB = gridC_10dB(dB)) <= sm.hi; dB++) {
            ct.fillRect(w-1,y_dB(dB), 1,1);
         }
      } else {
         // time to draw time marker
         sm.secs_last = secs;
         ct.fillStyle = sm.grid_color;
         ct.fillRect(w-1,0, 1,h);
      }

      if (sm.threshold) {
         ct.fillStyle = 'red';
         ct.fillRect(w-1,y_dB(sm.threshold), 1,1);
      }
   }

	// always plot, even if not shifting display
   ct.fillStyle = sm.plot_color;
   ct.fillRect(w-1,y_dB(val_dB), 1,1);
}
