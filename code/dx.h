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

// Copyright (c) 2015 John Seamons, ZL/KF6VO

// signals actually heard, or on watchlist, of this particular SDR (AKA the 'DX' list)

struct dx_t {
	float freq;
	int flags;
	const char *text;
	const char *notes;
	float offset;
};

#define	AM		0x000
#define	AMN		0x001
#define	USB		0x002
#define	LSB		0x003
#define	CW		0x004
#define	CWN		0x005
#define	DATA	0x006
#define	RESV	0x007

#define	WL		0x010	// on watchlist, i.e. not actually heard yet, marked as a signal to watch for
#define	SB		0x020	// a sub-band, not a station
#define	DG		0x030	// DGPS

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
	{    20.5,	CWN,	"3SA/3SB? CHN", "FSK" },
	{    21.1,	CWN,	"RDL RUS", "FSK" },
	{    21.4,	CW,		"NPM HI", "MSK" },
	{    21.5,	CWN,	"? FSK", "11PM" },
//	{    21.6,	CWN,	"carrier <br> (10PM)" },
	{    22.2,	CWN,	"JJI JPN", "MSK" },
	{    22.4,	CWN,	"? MSK", "11PM" },
	{    22.6,	CW,		"HWU FR", "grey line, MSK" },	// huge sig on WebSDR
	{    23.4,	CW,		"DHO38 GER", "trace, MSK" },
	{    24.0,	CW,		"NAA MA", "MSK" },
	{    24.1,	CWN,	"call? JPN", "FSK" },
//	{    24.5,	CWN,	"? weak FSK" },
	{    24.8,	CWN,	"NLK WA", "MSK" },
	{    25.0,	CWN,	"3SA/3SB? CHN <br> RAB99 RUS", "FSK\\nBeta time signal, ID @xx:06:00" },
	{    25.2,	CW,		"NLM4 ND", "MSK" },
	{    26.0,	CWN,	"? FSK", "morning" },
	{    26.7,	CWN,	"? FSK", "morning" },
	{    28.0,	CW,		"3SB/3SQ? CHN", "MSK, morning" },
	{    29.7,	CW,		"? FSK", "morning, WebSDR hears it" },
	{    29.1,	USB,	"? strange FSK data bursts", "intermittent, telemetering?, center 30.1" },
	{    31.5,	CWN,	"? FSK", "2PM" },

	{    40.0,	CWN,	"JJY JPN <br> (night, pre-dawn, ID @xx:15&45:40)", "time signal" },
	{    50.0,	CWN|WL,	"RTZ RUS <br> (ID @xx:05:00-xx:06:00)", "watchlist, uncertain if active" },
	{    54.0,	CW,		"NDI JPN", "MSK, night, pre-dawn" },
	{    60.0,	CWN,	"JJY JPN <br> (night, pre-dawn, ID @xx:15&45:40)", "time signal" },
	{    66.67,	CWN|WL,	"RBU RUS", "watchlist, time signal" },
	{    68.5,	CWN,	"BPC CHN <br> (night, pre-dawn)", "time signal" },
	{    77.5,	CWN|WL,	"DCF77 GER", "watchlist, only 50 kW so unlikely, time signal" },
//	{   162.0,	CWN|WL,	"TDF FRA", "watchlist, off 01:03-05:00 UTC+2 Tuesday, time signal" },

	{   136.0,	USB|SB,	"WSPR" },
	{   224,	CWN,	"West Maitland <br> \"WMD\" AUS", "", 400 },
	{   226,	CWN,	"KeriKeri <br> \"KK\"", "", 1040 },
	{   226,	CWN,	"Ferry <br> \"FY\"", "", 1040 },
	{   230,	CWN,	"Taupo <br> \"AP\"", "", 1040 },
	{   238,	CWN,	"Kaitaia <br> \"KT\"", "", 1020 },
	{   242,	CWN,	"Paraparaumu <br> \"PP\"", "", 1030 },
	{   246,	CWN,	"Wairoa <br> \"WO\"", "", -1000 },
	{   254,	CWN|WL,	"Ashburton <br> \"AS\"", "watchlist, 1034/1039/4.136", 1040 },
	{   254,	CWN,	"Waiuku <br> \"WI\"", "", 1020 },
	{   260,	CWN,	"Norfolk Island <br> \"NF\" AUS", "", -400 },
	{   264,	CWN,	"Cloncurry <br> \"CCY\" AUS", "", 400 },
	{   269,	CWN,	"Charleville <br> \"CV\" AUS", "", 400 },
	{   272,	CWN,	"Lord Howe Island <br> \"LHI\" AUS", "", 400 },
	{   274,	CWN,	"Great Barrier Island <br> \"(dit) GB\"", "", 1010 },
	{   278,	CWN,	"Westport <br> \"WS\"", "", -1030 },
	{   278,	CWN,	"Coolangatta <br> \"CG\" AUS", "", -400 },
	{   286,	CWN,	"Cape Campbell <br> \"CC\"", "", -1020 },
	{   290,	CWN,	"Singleton <br> \"SGT\" AUS", "", 400 },
	{   293,	CWN,	"Cooma <br> \"COM\" AUS", "", 400 },
	{   294,	CW|DG,	"Brisbane QLD <br> DGPS AUS", "ID#007, differential GPS" },
//	{   294,	CW|DG,	"Darwin NT <br> DGPS AUS", "ID#014, differential GPS" },
	{   296,	CWN,	"Point Lookout <br> \"PLO\" AUS", "", -430 },
	{   297,	CW|WL,	"Exmouth WA <br> DGPS AUS", "watchlist, differential GPS" },
	{   302,	CWN,	"Bundaberg <br> \"BUD\" AUS", "", 400 },
	{   304,	CW|DG,	"Cape Flattery QLD <br> DGPS AUS", "differential GPS" },
//	{   304,	CW|DG,	"Karratha WA <br> DGPS AUS", "differential GPS" },
	{   305,	CWN,	"Griffith <br> \"GTH\" AUS", "", 400 },
	{   306,	CW|DG,	"Ingham QLD DGPS AUS", "differential GPS" },
//	{   306,	CW|DG,	"Perth WA <br> DGPS AUS", "differential GPS" },
	{   307,	CWN|DG,	"Nausori <br> \"NA\" Fiji", "", -1010 },
	{   308,	CW|DG,	"Sydney NSW <br> DGPS AUS", "ID#003, differential GPS" },
	{   310,	CWN,	"Hokitika <br> \"HK\"", "", -1020 },
	{   311,	CWN,	"Coffs Harbour <br> \"CFS\" AUS", "", 400 },
	{   313,	CW|DG,	"Gladstone QLD <br> DGPS AUS", "ID#006, differential GPS" },
	{   314,	CW|DG,	"Crib Point VIC <br> DGPS AUS", "differential GPS" },
	{   314,	CWN,	"Miranda <br> \"RD\"", "", 1020 },
	{   315,	CW|DG,	"Mackay QLD <br> DGPS AUS", "ID#004, differential GPS" },
//	{   315,	CW|DG,	"Albany WA <br> DGPS AUS", "differential GPS" },
	{   316,	CW|DG,	"Corny Point SA DGPS AUS <br> Weipa QLD DGPS AUS", "ID#010, differential GPS\\ndifferential GPS" },
	{   318,	CW|DG,	"Mallacoota VIC <br> DGPS AUS", "ID#013, differential GPS" },
	{   320,	CW|WL,	"Horn Island QLD <br> DGPS AUS", "watchlist, buried under QRM, differential GPS" },
	{   322,	CWN,	"Chatham Islands <br> \"CI\"", "", 1040 },
	{   326,	CWN,	"Whangarei <br> \"WR\"", "", 1040 },
	{   329,	CWN,	"Narrandera <br> \"NAR\" AUS", "", 400 },
	{   332,	CWN,	"Ile Des Pins <br> \"IP\" New Caledonia", "20s DAID, no carrier" },
	{   335,	CWN,	"Yass <br> \"YAS\" AUS", "", 400 },
	{   338,	CWN,	"Mallacoota <br> \"MCO\" AUS", "", 400 },
	{   341,	CWN,	"Tamworth <br> \"TW\" AUS", "", 400 },
	{   346,	CWN|WL,	"Manapouri <br> \"MO\"", "watchlist, 972/1085/6.970", 1080 },
	{   346,	AMN,	"Tauranga <br> \"TG\" (spurs)", },
	{   350,	CWN,	"Surrey <br> \"SY\"", "", -1030 },
	{   350,	CWN,	"Kaikoura <br> \"KI\"", "", -1030 },
	{   352,	CWN,	"Rarotonga <br> \"RG\" Cook Islands", "", 1000 },
	{   353,	CWN,	"Longreach <br> \"LRE\" AUS", "", 400 },
	{   354,	CWN,	"La Tontouta <br> \"FND\" New Caledonia", "", 400 },
	{   358,	CWN,	"Newlands <br> \"NL\"", "", -1040 },
	{   358,	CWN|WL,	"Mosgiel <br> \"MI\"", "watchlist, 1015/1023/6.91", 1020 },
	{   359,	CWN,	"Amberley <br> \"AMB\" AUS", "", 400 },
	{   361,	CWN,	"Bauerfield <br> \"BA\" Vanuatu", "same tone WK spur", -1000 },
	{   362,	CWN,	"Whakatane <br> \"WK\" (spurs)", "", 1050 },
	{   364,	CWN,	"Momi <br> \"MI\" Fiji", "near carrier", 1050 },
	{   366,	CWN,	"Springfield <br> \"SF\"", "near carrier", -1060 },
	{   366,	CWN|WL,	"Timaru <br> \"TU\"", "watchlist, 1017/1028/7.01", 1030 },
	{   377,	CWN,	"Roma <br> \"ROM\" AUS", "", 400 },
	{   378,	CWN|WL,	"Henley <br> \"HL\"", "watchlist, 997/1040/7.01", 1040 },
	{   380,	CWN,	"Corowa <br> \"COR\" AUS", "", -400 },
	{   380,	CWN,	"Sunshine Coast <br> \"SU\" AUS", "", 400 },
	{   382,	CWN,	"Wanganui <br> \"WU\"", "", 1050 },
	{   385,	CWN,	"Malolo <br> \"AL\" Fiji", "", -1020 },
	{   386,	CWN|WL,	"Alexandra <br> \"AX\"", "watchlist, 1001/1011/7.17", 1010 },
//	{   389,	CWN,	"? <br> \"?N\" AUS", "", 400 },		// faded out
	{   390,	CWN,	"Hamilton <br> \"HN\"", "", 1050 },
	{   394,	CWN|WL,	"Berridale <br> \"BE\"", "watchlist, 1024/1015/7.00", 1020 },
	{   394,	CWN|WL,	"Westpoint <br> \"OT\"", "watchlist, 1015/1021/8.50", 1020 },
	{   395,	CWN,	"Port MacQuarie <br> \"PMQ\" AUS", "", 400 },
	{   401,	CWN,	"Armidale <br> \"ARM\" AUS", "", -400 },
	{   403,	CWN,	"Pago Pago <br> \"TUT\" American Samoa", "", 1000 },
	{   404,	CWN,	"Coen <br> \"COE\" AUS", "", -400 },
	{   405,	CWN,	"Nadi <br> \"VK\" Fiji", "", 1000 },
	{   407,	CWN,	"Gunnedah <br> \"GDH\" AUS", "", -400 },
	{   407,	CWN,	"Goulburn <br> \"GLB\" AUS", "", -400 },
	{   412,	CWN,	"Santo <br> \"SON\" Vanuatu", "", 1000 },
	{   446,	CWN,	"Thangool <br> \"(dit dit) TNG\" AUS", "", -1030 },
	{   474.2,	USB|SB,	"WSPR" },

	{   531.0,	AM,		"Radio 531 PI <br> 1XPI Auckland" },
	{   540.0,	AM,		"Rhema <br> 1XC Maketu" },
	{   558.0,	AMN,	"Radio Fiji One <br> Naulu Fiji", "0900Z, logged by Michael Kunze, Germany" },
	{   567.0,	AM,		"RNZ National <br> 2YA Titahi Bay" },
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

	{  1630.0,	CWN|WL,	"Taumarunui <br> \"TM\"", "watchlist, 1000/1000/4.00", 1000 },

	{  1836.6,	USB|SB,	"WSPR" },
	{  1838.15,	USB|SB,	"PSK31" },

	{  3580.15,	USB|SB,	"PSK31" },
	{  3592.6,	USB|SB,	"WSPR" },

	{  4146.0,	USB,	"ZLM NZL", "Taupo marine weather, 0600L" },
	{  4177.0,	USB,	"FSK", "6PM RY testing +/- freq" },
	{  4249.0,	USB,	"STANAG" },

	{  5287.2,	USB|SB,	"WSPR" },

	{  6843.0,	USB,	"STANAG" },

	{  7035.15,	USB|SB,	"PSK31" },
	{  7038.6,	USB|SB,	"WSPR" },
	{  7080.15,	USB|SB,	"PSK31" },
	{  7596.0,	USB,	"FSK" },

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
	{  9903.5,	USB,	"FSK", "4PM" },
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
	{ 12365.0,	USB,	"VNC AUS", "marine weather, 2200L" },
	{ 12581.0,	USB,	"XSV CHN <br> WLO USA", "10PM" },
	{ 12612.0,	USB,	"XSQ CHN", "10PM" },
	{ 12621.5,	CW,		"SITOR", "coastal radio" },
	{ 12629.0,	CW,		"TAH TUR", "8PM" },
	{ 12636.5,	USB,	"XSG CHN", "10PM" },
	{ 12647.5,	USB,	"XSQ CHN", "10PM" },
	{ 12654.0,	CW,		"TAH TUR", "8PM" },
	{ 12665.0,	USB,	"STANAG", "5PM" },
	{ 12700.0,	USB,	"FSK", "6PM" },
	{ 12724.5,	USB,	"STANAG", "5PM" },
	{ 12766.75,	USB,	"STANAG", "8PM" },
	{ 12785.0,	USB,	"FAX", "9PM" },
	{ 12843.0,	CW,		"HLO KOR", "coastal radio" },
	{ 12916.5,	CW,		"HLF KOR", "coastal radio" },
	{ 12923.0,	CW,		"HLW2 KOR", "coastal radio" },
	{ 12935.0,	CW,		"HLG KOR", "coastal radio" },
	{ 12974.0,	USB,	"MPX", "5PM" },
	{ 12983.0,	USB,	"STANAG", "5PM" },

	{ 13630.0,	AM,		"R. Australia" },
	{ 13869.0,	USB,	"FSK", "4PM" },
	{ 13919.3,	USB,	"FAX", "12PM" },
	{ 13987.5,	USB,	"FAX", "10PM" },

	{ 14070.15,	USB|SB,	"PSK31" },
	{ 14076.0,	USB|SB,	"JT65" },
	{ 14095.6,	USB|SB,	"WSPR" },
	{ 14100.0,	CW,		"IARU/NCDXF", "propagation beacons" },
	{ 14433.3,	USB,	"FSK" },
	{ 14705.0,	USB,	"STANAG", "6PM" },

	{ 15000.0,	AM,		"WWVH <br> WWV <br> BPM", "time signals" },
	{ 15160.0,	AM,		"R. Australia" },
	{ 15720.0,	AM,		"R. New Zealand" },
	{ 15745.0,	USB,	"DRM", "10AM, 4PM" },

	{ 16546.0,	USB,	"VNC AUS", "marine weather" },
	{ 16913.5,	USB,	"STANAG", "6PM" },
	{ 16958.0,	USB,	"STANAG", "6PM" },

	{ 17229.0,	USB,	"STANAG", "5PM" },
	{ 17750.0,	AM,		"R. Australia" },
	{ 17795.0,	AM,		"R. Australia" },
	{ 17840.0,	AM,		"R. Australia" },
	{ 17860.0,	AM,		"R. Australia" },

	{ 18100.15,	USB|SB,	"PSK31" },
	{ 18104.6,	USB|SB,	"WSPR" },
	{ 18599.0,	USB,	"STANAG", "6PM" },

	{ 19000.0,	AM,		"R. Australia" },

	{ 21094.6,	USB|SB,	"WSPR" },

	{ 16368.0,	CW|SB,	"GPS clock spur" },
	{ 00000.0,	0,		"" },
};
