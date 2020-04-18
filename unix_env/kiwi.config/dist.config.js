// Default settings for a new connection
// are in "config" tab of admin interface.

var init_frequency;
var init_mode;
var init_zoom;
var init_max_dB;
var init_min_dB;

// ITU regions:
// 1 = Europe, Africa
// 2 = North & South America
// 3 = Asia / Pacific

var svc = {
//	N: { name:'MRHS NoN',					color:'yellow' },
	B: { name:'Broadcast',					color:'red' },
	U: { name:'Utility', 					color:'green' },
	A: { name:'Amateur', 					color:'blue' },
	L: { name:'Beacons',						color:'blue' },
	I: { name:'Industrial/Scientific', 	color:'orange' },
	M: { name:'Markers', 					color:'purple' },
	X: { name:'', 		         			color:'red' },
};

var bands=[];

// for band scale:
//		'*' means applies to all regions vs '1', '2' or '3'
//		first char '>' chooses which region to use when multiple regions present

// for "select band" menu:
//		only region '*', '>' or 'm' entries are used (e.g. not ISM that uses '-')
//		a region of 'm' appears in band menu only -- never in band scale
//		if selected freq/mode ('sel:') not specified goes to middle of band

// Maritime Radio Historical Society (MRHS) Night-of-Nights (NoN)
// radiomarine.org
// special event
/*
bands.push({ s:svc.N, min:426, max:500, sel:"500cwn", region:"m", name:"500 kc" });
bands.push({ s:svc.N, min:6383, max:6478, sel:"6477.5cwn", region:"m", name:"6 Mc" });
bands.push({ s:svc.N, min:8438, max:8658, sel:"8642cwn", region:"m", name:"8 Mc" });
bands.push({ s:svc.N, min:12695, max:12993, sel:"12808.5cwn", region:"m", name:"12 Mc" });
bands.push({ s:svc.N, min:16914, max:17221, sel:"17016.8cwn", region:"m", name:"16 Mc" });
bands.push({ s:svc.N, min:22477, max:22478, sel:"22477.5", region:"m", name:"22 Mc" });
*/

var down_converter_2m = false;
var down_converter_6m = false;

if (down_converter_2m) {
   bands.push({ s:svc.A, min:28000, max:32000, region:'*', name:"144-148 MHz" });
} else
if (down_converter_6m) {
   bands.push({ s:svc.A, min:28000, max:30000, region:'*', name:"50-52 MHz" });
} else {
   bands.push({ s:svc.B, min:153, max:279, chan:9, region:'E', name:"LW" });
   bands.push({ s:svc.B, min:153, max:198, chan:9, region:'>', name:"LW" });	// stopped at 198 for local NDB band
   bands.push({ s:svc.B, min:530, max:1700, sel:"1000am", chan:10, region:">2", name:"MW" });
   bands.push({ s:svc.B, min:531, max:1602, sel:"1000am", chan:9, region:"3", name:"MW" });
   
   bands.push({ s:svc.B, min:2300, max:2495, region:'*', name:"120m" });
   bands.push({ s:svc.B, min:3200, max:3400, region:'*', name:"90m" });
   bands.push({ s:svc.B, min:3900, max:4000, region:'*', name:"75m" });
   bands.push({ s:svc.B, min:4750, max:5060, region:'*', name:"60m" });
   bands.push({ s:svc.B, min:5900, max:6200, region:'*', name:"49m" });
   bands.push({ s:svc.B, min:7200, max:7450, region:"13", name:"41m" });
   bands.push({ s:svc.B, min:7300, max:7450, region:">2", name:"41m" });
   bands.push({ s:svc.B, min:9400, max:9900, region:'*', name:"31m" });
   bands.push({ s:svc.B, min:11600, max:12100, region:'*', name:"25m" });
   bands.push({ s:svc.B, min:13570, max:13870, region:'*', name:"22m" });
   bands.push({ s:svc.B, min:15100, max:15800, region:'*', name:"19m" });
   bands.push({ s:svc.B, min:17480, max:17900, region:'*', name:"16m" });
   bands.push({ s:svc.B, min:18900, max:19020, region:'*', name:"15m" });
   bands.push({ s:svc.B, min:21450, max:21850, region:'*', name:"13m" });
   bands.push({ s:svc.B, min:25600, max:26100, region:'*', name:"11m" });
   
   bands.push({ s:svc.U, min:10, max:30, sel:"15.25lsb", region:'*', name:"VLF" });
   bands.push({ s:svc.U, min:31, max:100, sel:"60cwn", region:'*', name:"LF" });
   bands.push({ s:svc.U, min:200, max:472, sel:"346amn", region:">", name:"NDB" });	// local definition of NDB band
   bands.push({ s:svc.U, min:283.5, max:472, region:"E", name:"NDB" });	// don't collide w/ LW below
   bands.push({ s:svc.U, min:190, max:535, region:"U", name:"NDB" });
   bands.push({ s:svc.U, min:294, max:320, sel:"308cw", region:"m", name:"DGPS" });
   
   bands.push({ s:svc.U, min:2500, max:2500, sel:"2500amn", region:'*', name:"Time 2.5" });
   bands.push({ s:svc.U, min:3330, max:3330, sel:"3330usb", region:'*', name:"Time 3.33" });
   bands.push({ s:svc.U, min:5000, max:5000, sel:"5000amn", region:'*', name:"Time 5" });
   bands.push({ s:svc.U, min:7850, max:7850, sel:"7850usb", region:'*', name:"Time 7.85" });
   bands.push({ s:svc.U, min:10000, max:10000, sel:"10000amn", region:'*', name:"Time 10" });
   bands.push({ s:svc.U, min:14670, max:14670, sel:"14670usb", region:'*', name:"Time 14.67" });
   bands.push({ s:svc.U, min:15000, max:15000, sel:"15000amn", region:'*', name:"Time 15" });
   bands.push({ s:svc.U, min:20000, max:20000, sel:"20000amn", region:'*', name:"Time 20" });
   bands.push({ s:svc.U, min:25000, max:25000, sel:"25000amn", region:'*', name:"Time 25" });
   
   bands.push({ s:svc.A, min:135.7, max:137.8, region:'*', name:"LF" });	// 2200m
   bands.push({ s:svc.A, min:472, max:479, region:">23", name:"MF" });		// 630m
   bands.push({ s:svc.A, min:1810, max:2000, region:"1", name:"160m" });
   bands.push({ s:svc.A, min:1800, max:2000, region:">23", name:"160m" });
   bands.push({ s:svc.A, min:3500, max:3800, region:"1", name:"80m" });
   bands.push({ s:svc.A, min:3500, max:4000, region:">2", name:"80m" });
   bands.push({ s:svc.A, min:3500, max:3900, region:"3", name:"80m" });
   bands.push({ s:svc.A, min:5250, max:5450, region:'*', name:"60m" });	// many variations
   bands.push({ s:svc.A, min:7000, max:7200, region:"1", name:"40m" });
   bands.push({ s:svc.A, min:7000, max:7300, region:">23", name:"40m" });
   bands.push({ s:svc.A, min:10100, max:10150, region:">12", name:"30m" });
   bands.push({ s:svc.A, min:10100, max:10157.3, region:"3", name:"30m" });
   bands.push({ s:svc.A, min:14000, max:14350, region:'*', name:"20m" });
   bands.push({ s:svc.A, min:18068, max:18168, region:'*', name:"17m" });
   bands.push({ s:svc.A, min:21000, max:21450, region:'*', name:"15m" });
   bands.push({ s:svc.A, min:24890, max:24990, region:'*', name:"12m" });
   bands.push({ s:svc.A, min:28000, max:29700, region:'*', name:"10m" });
   
   bands.push({ s:svc.L, min:14100, max:14100, sel:"14100cwn", region:'m', name:"IBP 20m" });
   bands.push({ s:svc.L, min:18110, max:18110, sel:"18110cwn", region:'m', name:"IBP 17m" });
   bands.push({ s:svc.L, min:21150, max:21150, sel:"21150cwn", region:'m', name:"IBP 15m" });
   bands.push({ s:svc.L, min:24930, max:24930, sel:"24930cwn", region:'m', name:"IBP 12m" });
   bands.push({ s:svc.L, min:28200, max:28200, sel:"28200cwn", region:'m', name:"IBP 10m" });
   
   bands.push({ s:svc.I, min:6765, max:6795, region:"-", name:"ISM" });
   bands.push({ s:svc.I, min:13553, max:13567, region:"-", name:"ISM" });
   bands.push({ s:svc.I, min:26957, max:27283, region:"-", name:"ISM" });
   
   bands.push({ s:svc.M, min:3594.4, max:3594.4, sel:"3594.4cwn", region:'m', name:"3594" });
   bands.push({ s:svc.M, min:4558.4, max:4558.4, sel:"4558.4cwn", region:'m', name:"4558" });
   bands.push({ s:svc.M, min:5154.4, max:5154.4, sel:"5154.4cwn", region:'m', name:"5154" });
   bands.push({ s:svc.M, min:5156.8, max:5156.8, sel:"5156.8cwn", region:'m', name:"5156 L" });
   bands.push({ s:svc.M, min:5292.0, max:5292.0, sel:"5292.0cwn", region:'m', name:"5292 B" });
   bands.push({ s:svc.M, min:6928.0, max:6928.0, sel:"6928.0cwn", region:'m', name:"6928 V" });
   bands.push({ s:svc.M, min:7039.4, max:7039.4, sel:"7039.4cwn", region:'m', name:"7039" });
   bands.push({ s:svc.M, min:7509.2, max:7509.2, sel:"7509.2cwn", region:'m', name:"7509" });
   bands.push({ s:svc.M, min:8000.0, max:8000.0, sel:"8000.0cwn", region:'m', name:"8000 C" });
   bands.push({ s:svc.M, min:8495.3, max:8495.3, sel:"8495.3cwn", region:'m', name:"8495" });
   bands.push({ s:svc.M, min:10872.2, max:10872.2, sel:"10872.2cwn", region:'m', name:"10872" });
   bands.push({ s:svc.M, min:13528.4, max:13528.4, sel:"13528.4cwn", region:'m', name:"13528" });
   bands.push({ s:svc.M, min:16332.0, max:16332.0, sel:"16332.0cwn", region:'m', name:"16332" });
   bands.push({ s:svc.M, min:20048.4, max:20048.4, sel:"20048.4cwn", region:'m', name:"20048" });
}
