/*
--------------------------------------------------------------------------------
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the
Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
Boston, MA  02110-1301, USA.
--------------------------------------------------------------------------------
*/

// Copyright (c) 2014 John Seamons, ZL/KF6VO

struct dx_t {
	float freq;
	int flags;
	const char *text;
	const char *notes;
	union { float carrier; float freq2; };
};

#define	AM		0x00
#define	AMN		0x01
#define	USB		0x02
#define	LSB		0x03
#define	CW		0x04
#define	CWN		0x05
#define	DATA	0x06
#define	RESV	0x07

#define	WL		0x10	// on watchlist, i.e. not actually heard yet, marked as a signal to watch for
#define	SB		0x20	// a sub-band, not a station

// fixme: read from file, database etc.
// fixme: display depending on rx time-of-day

dx_t dx[] = {
	{    11.9,	CWN,	"Alpha RUS", "navigation system" },
	{    12.65,	CWN,	"Alpha RUS", "navigation system" },
	{    14.88,	CWN,	"Alpha RUS", "navigation system" },
	{    15.25,	LSB|SB,	"Alpha", "wideband" },
	{    16.3,	CW,		"VTX1 IND", "MSK" },
	{    17.0,	CW,		"VTX2 IND", "MSK" },
	{    18.1,	CWN,	"RDL RUS", "FSK" },
	{    18.2,	CWN,	"VTX3 IND", "MSK" },
//	{    18.9,	CWN,	"RDL RUS", "FSK" },		// in there, but not definitive
//	{    19.1,	CW,		"JJI2? JPN", "5PM" },
	{    19.8,	CW,		"NWC AUS", "MSK, daytime QRM" },
	{    20.27,	CWN,	"ICV IT", "grey line, mornings before QRM, MSK" },
	{    20.5,	CWN,	"3SA/3SB? CHN <br> RAB99 RUS", "FSK\\nBeta time signal" },
	{    21.1,	CWN,	"RDL RUS", "FSK" },
	{    21.4,	CW,		"NPM HI", "MSK" },
//	{    21.5,	CWN,	"? FSK <br> (1900Z)" },
//	{    21.6,	CWN,	"carrier <br> (10PM)" },
	{    22.2,	CWN,	"JJI JPN", "MSK" },
	{    22.4,	CWN,	"? MSK", "11PM" },
	{    22.6,	CW,		"HWU FR", "grey line, MSK" },	// huge sig on WebSDR
	{    23.4,	CW,		"DHO38 GER", "trace, MSK" },
	{    24.0,	CW,		"NAA MA", "MSK" },
	{    24.1,	CWN,	"call? JPN", "FSK" },
//	{    24.5,	CWN,	"? weak FSK" },
	{    24.8,	CWN,	"NLK WA", "MSK" },
	{    25.0,	CWN,	"3SA/3SB? CHN <br> RAB99 RUS", "FSK\\nBeta time signal" },
	{    25.2,	CW,		"NLM4 ND", "MSK" },
	{    26.0,	CWN,	"? FSK", "morning" },
	{    26.7,	CWN,	"? FSK", "morning" },
	{    28.0,	CW,		"3SB/3SQ? CHN", "MSK, morning" },
	{    29.7,	CW,		"? FSK", "morning, WebSDR hears it" },
	{    29.1,	USB,	"? strange FSK data bursts", "intermittent, telemetering?, center 30.1" },
	{    31.5,	CWN,	"? FSK", "2PM" },

	{    40.0,	CWN,	"JJY JPN <br> (night, pre-dawn, ID @xx:15&45:40)", "time signal" },
//	{    50.0,	CWN,	"RTZ RUS <br> (xx:59:00-xx:05:00)"},	// Meinberg says "alive", but we've never heard it
	{    54.0,	CW,		"NDI JPN", "MSK, night, pre-dawn" },
	{    60.0,	CWN,	"JJY JPN <br> (night, pre-dawn, ID @xx:15&45:40)", "time signal" },
	{    68.5,	CW,		"BPC CHN <br> (night, pre-dawn)", "time signal" },

	{   136.0,	USB|SB,	"WSPR" },
	{   224.4,	CWN,	"West Maitland <br> \"WMD\" AUS" },
	{   227.0,	CWN,	"KeriKeri \"KK\" <br> Ferry \"FY\"" },
	{   231.0,	CWN,	"Taupo <br> \"AP\"" },
	{   239.0,	CWN,	"Kaitaia <br> \"KT\"" },
	{   241.0,	CWN,	"Paraparaumu <br> \"PP\"" },
	{   247.0,	CWN,	"Wairoa <br> \"WO\"" },
	{   255.0,	CWN,	"Waiuku <br> \"WI\"" },
	{   259.6,	CWN,	"Norfolk Island <br> \"NF\" AUS" },
	{   264.4,	CWN,	"Cloncurry <br> \"CCY\" AUS" },
	{   269.4,	CWN,	"Charleville <br> \"CV\" AUS" },
	{   272.4,	CWN,	"Lord Howe Island <br> \"LHI\" AUS" },
	{   275.0,	CWN,	"Great Barrier Island <br> \"(dit) GB\"" },
	{   277.0,	CWN,	"Westport <br> \"WS\"" },
	{   277.6,	CWN,	"Coolangatta <br> \"CG\" AUS" },
	{   285.0,	CWN,	"Cape Campbell <br> \"CC\"" },
	{   290.4,	CWN,	"Singleton <br> \"SGT\" AUS" },
	{   293.4,	CWN,	"Cooma <br> \"COM\" AUS" },
	{   294.0,	CW,		"Brisbane <br> DGPS AUS", "differential GPS" },
	{   295.57,	CWN,	"Point Lookout <br> \"PLO\" AUS" },
	{   297.0,	CW|WL,	"Exmouth <br> DGPS AUS", "watchlist, differential GPS" },
	{   302.4,	CWN,	"Bundaberg <br> \"BUD\" AUS" },
	{   304.0,	CW,		"Cape Flattery <br> DGPS AUS", "differential GPS" },
	{   305.4,	CWN,	"Griffith <br> \"GTH\" AUS" },
	{   306.0,	CWN,	"Nausori \"NA\" Fiji <br> Ingham DGPS AUS", "NDB\\ndifferential GPS", 307},
	{   308.0,	CW,		"Sydney <br> DGPS AUS", "differential GPS" },
	{   309.0,	CWN,	"Hokitika <br> \"HK\"" },
	{   311.4,	CWN,	"Coffs Harbour <br> \"CFS\" AUS" },
	{   313.0,	CW,		"Gladstone <br> DGPS AUS", "differential GPS" },
	{   314.0,	CW,		"Crib Point <br> DGPS AUS", "differential GPS" },
	{   315.0,	CW,		"Miranda \"RD\" <br> Mackay DGPS AUS", "NDB\\ndifferential GPS" },
	{   316.0,	CW,		"Weipa / Corny Point <br> DGPS AUS", "differential GPS" },
	{   318.0,	CW,		"Mallacoota <br> DGPS AUS", "differential GPS" },
	{   320.0,	CW|WL,	"Horn Island <br> DGPS AUS", "watchlist, differential GPS, buried under QRM" },
	{   323.04,	CWN,	"Chatham Islands <br> \"CI\"" },
	{   327.05,	CWN,	"Whangarei <br> \"WR\"" },
	{   329.4,	CWN,	"Narrandera <br> \"NAR\" AUS" },
	{   335.4,	CWN,	"Yass <br> \"YAS\" AUS" },
	{   338.4,	CWN,	"Mallacoota <br> \"MCO\" AUS", },
	{   341.4,	CWN,	"Tamworth <br> \"TW\" AUS" },
	{   346.0,	AMN,	"Tauranga <br> \"TG\" (spurs)" },
	{   348.97,	CWN,	"Surrey \"SY\" <br> Kaikoura \"KI\"" },
	{   353.0,	CWN,	"Rarotonga <br> \"RG\" Cook Islands" },
	{   353.4,	CWN,	"Longreach <br> \"LRE\" AUS" },
	{   354.4,	CWN,	"La Tontouta <br> \"FND\" New Caledonia" },
	{   356.93,	CWN,	"Newlands <br> \"NL\"", "", 358 },
//	{	?,		CWN,	"loud pulsing car", "", 359 },
	{   360.0,	CWN,	"Bauerfield <br> \"BA\" Vanuatu", "same tone WK spur", 361 },
	{   363.03,	CWN,	"Whakatane <br> \"WK\" (spurs)" },
	{   364.94,	CWN,	"Springfield <br> \"SF\"" },	// near carrier
	{   365.05,	CWN,	"Momi <br> \"MI\" Fiji" },		// car=364, near carrier
	{   377.4,	CWN,	"Roma <br> \"ROM\" AUS" },
	{   379.6,	CWN,	"Corowa <br> \"COR\" AUS" },
	{   380.4,	CWN,	"Sunshine Coast <br> \"SU\" AUS" },
	{   383.05,	CWN,	"Wanganui <br> \"WU\"" },
	{   383.98,	CWN,	"Malolo <br> \"AL\" Fiji" },
//	{   389.4,	CWN,	"? <br> \"?N\" AUS" },		// faded out
	{   391.0,	CWN,	"Hamilton <br> \"HN\"" },
	{   395.4,	CWN,	"Port MacQuarie <br> \"PMQ\" AUS" },
	{   400.6,	CWN,	"Armidale <br> \"ARM\" AUS" },
	{   403.6,	CWN,	"Coen <br> \"COE\" AUS" },
	{   404.0,	CWN,	"Pago Pago <br> \"TUT\" American Samoa" },
	{   406.0,	CWN,	"Nadi <br> \"VK\" Fiji" },
	{   406.6,	CWN,	"Gunnedah \"GDH\" AUS <br> Goulburn \"GLB\" AUS" },
	{   413.0,	CWN,	"Santo <br> \"SON\" Vanuatu" },
	{   444.97,	CWN,	"Thangool <br> \"(dit dit) TNG\" AUS" },
	{   474.2,	USB|SB,	"WSPR" },

	// post July 15, 2014 change to 540/1107 frequencies
//	{   531.0,	AM,		"Radio 531 PI <br> 1XPI Auckland"},		// was in the clear during 540 transition
	{   540.0,	AM,		"Rhema <br> 1XC Maketu" },
	{   576.0,	AM,		"Southern Star <br> 1XLR Hamilton" },
	{   585.0,	AM,		"Radio Ngati <br> 2XR Ruatoria" },
	{   603.0,	AM,		"Radio Waatea <br> Auckland" },
	{   630.0,	AM,		"RNZ National <br> 2YZ Napier" },
	{   657.0,	AM,		"Southern Star <br> Paengaroa" },
	{   702.0,	AM,		"Radio Live <br> 1XP Auckland" },
	{   747.0,	AM,		"Newstalk ZB <br> Rotorua" },
	{   765.0,	AM,		"Radio Kahungunu <br> 2XT Napier" },
	{   792.0,	AM,		"Radio Sport <br> 1XSR Hamilton" },
	{   819.0,	AM,		"RNZ National <br> Paengaroa" },
	{   873.0,	AM,		"LiveSPORT <br> Matapihi" },
	{  1008.0,	AM,		"Newstalk ZB <br> 1ZD Paengaroa" },
	{  1107.0,	AM,		"Radio Live <br> Maketu" },
	{  1242.0,	AM,		"One Double X <br> 1XX Whakatane" },
	{  1296.0,	AM,		"Newstalk ZB <br> 1ZH Hamilton" },
	{  1368.0,	AM,		"Village Radio <br> 1XT Tauranga" },
	{  1440.0,	AM,		"Te Reo <br> 1XK Matapihi" },
	{  1521.0,	AM,		"Good Time Oldies <br> 1XTR Matapihi" },

	{  1836.6,	USB|SB,	"WSPR" },
	{  1838.15,	USB|SB,	"PSK31" },

	{  3580.15,	USB|SB,	"PSK31" },
	{  3592.6,	USB|SB,	"WSPR" },

	{  4146.0,	USB,	"ZLM weather", "Taupo marine radio 6AM" },
	{  4177.0,	USB,	"FSK", "6PM RY testing +/- freq" },
	{  4249.0,	USB,	"STANAG" },

	{  5287.2,	USB|SB,	"WSPR" },

	{  6843.0,	USB,	"STANAG" },

	{  7035.15,	USB|SB,	"PSK31" },
	{  7038.6,	USB|SB,	"WSPR" },
	{  7080.15,	USB|SB,	"PSK31" },

	{  8144.0,	USB,	"STANAG" },
	{  8226.65,	CWN,	"6.5s beeper", "10PM" },
	{  8310.0,	CW,		"SITOR", "10PM" },
	{  8313.0,	USB,	"slot machine JPN" },
	{  8425.5,	CW,		"XSG CHN" },
	{  8433.0,	CW,		"XSG CHN" },
	{  8435.0,	CW,		"XSG CHN" },
	{  8537.0,	USB,	"FSK" },
	{  8551.0,	USB,	"MPX" },
	{  8564.0,	USB,	"FSK", "9PM" },
	{  8588.0,	USB,	"slot machine JPN" },
	{  8600.0,	CW,		"?CW", },
	{  8625.0,	USB,	"FUM Tahiti", "STANAG" },
	{  8636.0,	CW,		"HLW KOR" },
	{  8646.5,	USB,	"STANAG" },
	{  8653.0,	USB,	"FSK" },
	{  8657.0,	USB,	"FAX JPN" },
	{  8675.0,	USB,	"FSK", "6PM" },
	{  8677.0,	USB,	"STANAG", "9PM" },
	{  8681.0,	USB,	"FAX NMC USA" },
	{  8693.0,	USB,	"FSK", "10PM" },
	{  8703.5,	USB,	"slot machine JPN" },
	{  8743.0,	USB,	"HSW Bangkok?", "voice" },
	{  8764.0,	USB,	"USCG weather" },
	{  8828.0,	USB,	"VOLMET Honolulu", "5PM" },
	{  8867.0,	USB,	"Auckland ATC", "5PM" },

	{  9006.0,	USB,	"LINK-11", "10PM" },
	{  9056.0,	USB,	"STANAG", "6PM" },
	{  9073.5,	USB,	"FSK", "9PM" },
	{  9074.0,	CW,		"?CW" },
	{  9084.0,	USB,	"FSK", "5PM" },
	{  9094.0,	USB,	"STANAG", "9PM" },
	{  9111.0,	USB,	"FSK", "5PM" },
	{  9121.5,	USB,	"LINK-11", "6PM" },
	{  9139.0,	USB,	"FSK", "9PM" },
	{  9164.0,	USB,	"HLL2 KOR", "FAX" },
	{  9155.0,	AM,		"HM01 CUB", "10PM" },
	{  9240.0,	AM,		"HM01 CUB", "9PM" },
	{  9326.0,	CW,		"?CW" },
	{  9330.0,	AM,		"HM01 CUB", "9PM" },
	{  9354.0,	CW,		"?CW" },
	{  9580.0,	AM,		"R. Australia" },
	{  9780.0,	USB,	"DRM", "5PM" },
	{  9911.0,	USB,	"STANAG", "9PM" },
	{  9959.0,	USB,	"FSK", "9PM" },
	{  9981.5,	USB,	"FAX", "6PM" },

	{ 10000.0,	AM,		"WWVH <br> WWV", "time signals" },
	{ 10138.7,	USB|SB,	"WSPR" },
	{ 10152.0,	USB,	"FSK" },
	{ 10142.15,	USB|SB,	"PSK31" },
	{ 10345.0,	AM,		"HM01 CUB", "6PM" },
	{ 10373.5,	USB,	"STANAG", "5PM" },
	{ 10429.0,	USB,	"FSK", "9PM" },

	{ 11029.0,	USB,	"FAX", "10PM" },
	{ 11089.0,	USB,	"FAX", "10PM" },
	{ 11469.5,	USB,	"FSK", "10PM" },
	{ 11474.5,	USB,	"FSK", "10PM" },
	{ 11945.0,	AM,		"R. Australia" },

	{ 12145.0,	USB,	"STANAG", "12PM" },
	{ 12581.0,	USB,	"XSV CHN <br> WLO USA", "10PM" },
	{ 12612.0,	USB,	"XSQ CHN", "10PM" },
	{ 12621.5,	CW,		"SITOR", "coastal radio" },
	{ 12629.0,	CW,		"TAH TUR", "8PM" },
	{ 12636.5,	USB,	"XSG CHN", "10PM" },
	{ 12647.5,	USB,	"XSQ CHN", "10PM" },
	{ 12654.0,	CW,		"TAH TUR", "8PM" },
	{ 12700.0,	USB,	"FSK", "6PM" },
	{ 12724.5,	USB,	"STANAG", "5PM" },
	{ 12766.75,	USB,	"STANAG", "8PM" },
	{ 12785.0,	USB,	"FAX", "9PM" },
	{ 12843.0,	CW,		"HLO KOR", "coastal radio" },
	{ 12916.5,	CW,		"HLF KOR", "coastal radio" },
	{ 12923.0,	CW,		"HLW2 KOR", "coastal radio" },
	{ 12935.0,	CW,		"HLG KOR", "coastal radio" },

	{ 13630.0,	AM,		"R. Australia" },
	{ 13869.0,	USB,	"FSK", "4PM" },
	{ 13919.3,	USB,	"FAX", "12PM" },
	{ 13987.5,	USB,	"FAX", "10PM" },

	{ 14070.15,	USB|SB,	"PSK31" },
	{ 14076.0,	USB|SB,	"JT65" },
	{ 14095.6,	USB|SB,	"WSPR" },
	{ 14100.0,	CW,		"IARU/NCDXF", "propagation beacons" },
	{ 14433.0,	USB,	"FSK" },

	{ 15000.0,	AM,		"WWVH <br> WWV <br> BPM", "time signals" },
	{ 15160.0,	AM,		"R. Australia" },
	{ 15720.0,	AM,		"R. New Zealand" },
	{ 15745.0,	USB,	"STANAG", "10AM" },

	{ 16546.0,	USB,	"VMC AUS", "marine weather" },

	{ 17750.0,	AM,		"R. Australia" },
	{ 17795.0,	AM,		"R. Australia" },
	{ 17840.0,	AM,		"R. Australia" },
	{ 17860.0,	AM,		"R. Australia" },

	{ 18100.15,	USB|SB,	"PSK31" },
	{ 18104.6,	USB|SB,	"WSPR" },

	{ 19000.0,	AM,		"R. Australia" },

	{ 21094.6,	USB|SB,	"WSPR" },

	{ 16368.0,	CW,		"GPS clock spur" },
	{ 00000.0,	0,		"" },
};
