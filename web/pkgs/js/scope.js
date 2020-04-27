
// NB: not re-entrant

var scope = {
   init: 0,
   run: 1,

   width: 0,
   height: 0,
   left: 0,
   margin: 0,
   
   x: 0,
   y: 0,

   prev_x: 0,
   trace_count: 0,
   trace_count_max: 6,
   
   end:     null
};

function scope_init(canvas, opt)
{
   var s = scope;

   s.ctx = canvas.getContext("2d");
   
   s.sec_per_sweep = w3_opt(opt, 'sec_per_sweep', 10);
   s.srate = w3_opt(opt, 'srate', 12000);
   s.single_shot = w3_opt(opt, 'single_shot', 0);
   s.background_color = w3_opt(opt, 'background_color', 'white');

   s.line_height = w3_opt(opt, 'line_height', 16);
   s.trace_height = w3_opt(opt, 'trace_height', 12);

   s.width = w3_opt(opt, 'width', 800);
   s.height = w3_opt(opt, 'height', 200);
   s.left = w3_opt(opt, 'left', 50);
   s.margin = w3_opt(opt, 'margin', 1);
   
   // set/increment to "zero" position of trace
   s.y_set = s.margin + s.line_height;
   s.y_inc = s.margin + s.line_height*2;
   
   s.init = 1;
   scope_clr();
}

function scope_clr()
{
   var s = scope;
   if (!s.init) return;
	
	s.trace_count = 0;
   s.x_inc = s.left + (s.single_shot? 0:64);
   s.y = s.y_set;
	s.ctx.fillStyle = s.background_color;
   s.ctx.fillRect(s.left,0, (s.width+1),s.height);
}

function scope_draw(t1, t2, t3, t4, t5, trig)
{
   var s = scope;
	if (!s.init || !s.run) return;
		
	s.x_inc += s.width/s.srate / s.sec_per_sweep;
	s.x = Math.round(s.x_inc);

	if (s.x >= s.left + s.width) {
  		s.trace_count++;
		if (s.single_shot && s.trace_count >= s.trace_count_max) {
			s.run = 0;
			return;
		}
		s.x_inc = s.left;
		s.y += s.y_inc;
		if (s.y > s.height - s.y_set) s.y = s.y_set;
		s.ctx.fillStyle = s.background_color;
		s.ctx.fillRect(s.left,s.y-s.y_set, (s.width+1),s.y_inc);
	}

   t1 = w3_obj_num(t1);
   t2 = w3_obj_num(t2);
   t3 = w3_obj_num(t3);
   t4 = w3_obj_num(t4);
   t5 = w3_obj_num(t5);
   trig = w3_obj_num(trig);
   
   var yval = function(o) { return o.num * s.trace_height };

	if (t1.num && s.x != s.prev_x) {
		s.ctx.fillStyle = 'cyan';
		s.ctx.globalAlpha = 0.3;
		s.ctx.fillRect(s.x,s.y-s.trace_height, 1,s.trace_height*2);
		s.ctx.globalAlpha = 1;
	}

	if (t2 != undefined) {
		s.ctx.fillStyle = (t2.color? t2.color : '#ff5050');		// light red
		s.ctx.fillRect(s.x,s.y-yval(t2), 1,1);
	}

	if (t3 != undefined) {
		s.ctx.fillStyle = (t3.color? t3.color : 'orange');
		s.ctx.fillRect(s.x,s.y-yval(t3), 1,1);
	}

	if (t4 != undefined) {
		s.ctx.fillStyle = (t4.color? t4.color : 'black');
		s.ctx.fillRect(s.x,s.y-Math.round(yval(t4)), 1,1);
	}

	if (t5 != undefined) {
		s.ctx.fillStyle = (t5.color? t5.color : 'lime');
		s.ctx.fillRect(s.x,s.y-yval(t5), 1,1);
	}
	
	if (trig != undefined && trig.num /* && s.x != s.prev_x */) {
	   s.ctx.fillStyle = (trig.num == 1)? 'black' : trig.color;
		s.ctx.fillRect(s.x,s.y-s.trace_height, 1,s.trace_height*2);
	}

	s.prev_x = s.x;
}
