// ITU regions:
// 1 = Europe, Africa
// 2 = North & South America
// 3 = Asia / Pacific

var svc = {
	B: { name:'Broadcast',					color:'red' },
	U: { name:'Utility', 					color:'green' },
	A: { name:'Amateur', 					color:'blue' },
	I: { name:'Industrial/Scientific', 	color:'orange' },
};

var bands=[];

// for band scale:
//		'*' means applies to all regions vs '1', '2' or '3'
//		first char '>' chooses which region to use when multiple regions present

// for "select band" menu:
//		only region '*', '>' or 'm' entries are used (e.g. not ISM that uses '-')
//		a region of 'm' appears in band menu only -- never in band scale
//		if selected freq/mode ('sel:') not specified goes to middle of band

bands.push({ s:svc.B, min:153, max:279, chan:9, region:'E', name:"LW" });
bands.push({ s:svc.B, min:153, max:220, chan:9, region:'>', name:"LW" });	// stopped at 220 for local NDB band
bands.push({ s:svc.B, min:531, max:1602, sel:"1107am", chan:9, region:">E", name:"MW" });
bands.push({ s:svc.B, min:540, max:1700, sel:"1107am", chan:10, region:"U", name:"MW" });
//foo
bands.push({ s:svc.B, min:1107, max:1107, sel:"1107am", chan:10, region:">", name:"test" });

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
bands.push({ s:svc.U, min:35, max:70, sel:"60cwn", region:'m', name:"LF" });
bands.push({ s:svc.U, min:220, max:472, sel:"346amn", region:">", name:"NDB" });	// local definition of NDB band
bands.push({ s:svc.U, min:283.5, max:472, region:"E", name:"NDB" });	// don't collide w/ LW below
bands.push({ s:svc.U, min:190, max:535, region:"U", name:"NDB" });
bands.push({ s:svc.U, min:294, max:320, sel:"308cw", region:"m", name:"DGPS" });
bands.push({ s:svc.U, min:10000, max:10000, sel:"10000amn", region:'*', name:"Time 10" });
bands.push({ s:svc.U, min:15000, max:15000, sel:"15000amn", region:'*', name:"Time 15" });

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

bands.push({ s:svc.I, min:6765, max:6795, region:"-", name:"ISM" });
bands.push({ s:svc.I, min:13553, max:13567, region:"-", name:"ISM" });
bands.push({ s:svc.I, min:26957, max:27283, region:"-", name:"ISM" });
