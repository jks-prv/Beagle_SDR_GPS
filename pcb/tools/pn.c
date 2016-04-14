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

// Copyright (c) 2015-2016 John Seamons, ZL/KF6VO

//
// Add parts number and manufacturer fields to BOM based on value/footprint association.
// Resulting file can be uploaded to e.g. octopart.com for quotation.
//

// octopart.com BOM column assignment order: 5, 2, 7, skip, 3

#include "types.h"

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

void exit(int status);

void sys_panic(char *str)
{
	printf("panic\n");
	perror(str);
	exit(-1);
}

void panic(char *str)
{
	printf("%s\n", str);
	exit(-1);
}

#define	assert(cond) _assert(cond, # cond, __FILE__, __LINE__);

void _assert(int cond, const char *str, const char *file, int line)
{
	if(!(cond)) {
		printf("assert \"%s\" failed at %s:%d\n", str, file, line); exit(-1);
	}
}

char *strdupcat(char *s1, int sl1, char *s2, int sl2)
{
	if (!s1) return s2;
	if (!s2) return s1;
	char *s = (char *) malloc(sl1+sl2+1);
	strncpy(s, s1, sl1);
	strncat(s, s2, sl2);
	return s;
}

// PROJECT is passed from Makefile
#define	PFILE(basename)		STRINGIFY_DEFINE(PROJECT) basename

#define	KICAD_CSV_FILE	PFILE(".csv")
#define	TO_OCTO_FILE	PFILE(".bom.csv")
#define	TO_DNL_FILE		PFILE(".bom.dnl.csv")
#define	TO_ODS_FILE		PFILE(".bom.ods.csv")
#define	INSERT_FILE		PFILE(".insert.txt")

#define	FROM_OCTO_FILE	PFILE(".bom.octo.csv")
#define	DIGIKEY_FILE	PFILE(".bom.digi.csv")
#define	MOUSER_FILE		PFILE(".bom.mou.csv")
#define	NEWARK_FILE		PFILE(".bom.new.csv")
#define	AVNET_EXP_FILE	PFILE(".bom.avex.csv")

#define NBUF 1024
char linebuf[NBUF];

#define	BAD			-1
#define	SHT_FPWR	0
#define	SHT_GPS		1
#define	SHT_INJ		2
#define	SHT_BONE	3
#define	SHT_ADC		4
#define	SHT_ANT		5
#define	SHT_FIO		6
#define	SHT_SWREG	7
#define NSHEETS		8
char *sheet_name[NSHEETS+1] = { "Fpwr", "gps", "inj", "bone", "adc", "ant", "Fio", "swreg", "TOTAL" };
int sheet_count[NSHEETS+1];
float sheet_price[NSHEETS+1];


// stats needed by assembly/PCB companies
#define	SMT		0
#define	NLP		1
#define	TH		2
#define	EDGE	3
#define	MAN		4
#define	NP		5
#define	VIR		6
#define	DNL		7
#define	NPLACE	8

char *place[] = { "SMT", "no_lead", "T/H", "edge", "manual", "no_proto", "virtual", "DNL" };
int ccount[NPLACE];

// attributes
#define	NA		0
#define	FP		0x01		// fine-pitch, <= 0.5mm pin centers
#define	GP		0x02		// generic part
#define	HC		0x04		// high-cost part
#define	POL		0x08		// polarized
#define	OPL		0x10		// OPL: Open Parts Library
#define	SQ		0x20		// special quotation
#define	NATTR	6

#define X		0, NA
#define G		0, GP

char *attr_str[] = { "fine_pitch", "generic", "high_cost", "polarized", "OPL", "special quote" };
int acount[NATTR];

typedef struct {
	int place;
	char *val, *pkg, *mfg, *pn;
	int pads, attr;
	char *specs, *notes, *crit, *mark;
	int force_quan;
	char *custom_pn;
	
	char *refs;
	int quan, sheet[NSHEETS];
	float price;
	char *dist, *sku;
	int seen;
} pn_t;

int npads;

//	sheet	refs	connectors
//	ADC		4xx		rf=J1/2 extclk=J5
//	GPS		1xx		in=J3 extclk=J4
//	Bone	3xx		ZB1
//	a.ant	5xx		rf=J10/11/13 pwr=J12
//	fpga_p	01x
//	pwr		7xx
//	inj		2xx		ant=J20/21/22/23 rx1=J24/25 rx2=J26/27 gnd=J28 AC=J29

int connectors[] = {
	// J0-9
	BAD, SHT_ADC, SHT_ADC, SHT_GPS, SHT_GPS, SHT_ADC, BAD, BAD, BAD, BAD,
	
	// J10-19
	SHT_ANT, SHT_ANT, SHT_ANT, SHT_ANT, BAD, BAD, BAD, BAD, BAD, BAD,
	
	// J20-29
	SHT_INJ, SHT_INJ, SHT_INJ, SHT_INJ, SHT_INJ, SHT_INJ, SHT_INJ, SHT_INJ, SHT_INJ, SHT_INJ,
};

// fixme
//	v.reg fb resis optim?
//	fill-out generic parts
//	fill-out critical specs

#define RES_THICK_FILM	"thick film resistor"
#define S_1P_100m_50V	"1% 100mW 50V", RES_THICK_FILM
#define S_1P_125m_150V	"1% 125mW 150V", RES_THICK_FILM
#define S_1P_500m_150V	"1% 500mW 150V", RES_THICK_FILM
#define S_1P_660m_500V	"1% 660mW 500V", RES_THICK_FILM

#define CAP_CERAMIC		"ceramic capacitor"
#define S_5P_C0G_50V	"5% C0G 50V", CAP_CERAMIC
#define S_5P_C0G_100V	"5% C0G 100V", CAP_CERAMIC
#define S_20P_X7R_100V	"20% X7R 100V", CAP_CERAMIC
#define S_10P_X7R_100V	"10% X7R 100V", CAP_CERAMIC
#define S_10P_X7R_50V	"10% X7R 50V", CAP_CERAMIC
#define S_10P_X7R_16V	"10% X7R 16V", CAP_CERAMIC
#define S_10P_X5R_35V	"10% X5R 35V", CAP_CERAMIC
#define S_10P_X5R_25V	"10% X5R 25V", CAP_CERAMIC
#define S_20P_X5R_6V3	"20% X5R 6.3V", CAP_CERAMIC

pn_t pn[] = {
// resistors
{SMT, "0R",						"kiwi-SM0402",		"Panasonic",		"ERJ-2GE0R00X", G, "", "zero-ohm jumper" },
{SMT, "10R",					"kiwi-SM0402",		"Panasonic",		"ERJ-2RKF10R0X", G, S_1P_100m_50V },
{SMT, "28R7",					"kiwi-SM0402",		"Panasonic",		"ERJ-2RKF28R7X", G, S_1P_100m_50V },
{SMT, "66R5",					"kiwi-SM0402",		"Panasonic",		"ERJ-2RKF66R5X", G, S_1P_100m_50V },
{SMT, "100R",					"kiwi-SM0402",		"Panasonic",		"ERJ-2RKF1000X", G, S_1P_100m_50V },
{SMT, "5K6",					"kiwi-SM0402",		"Panasonic",		"ERJ-2RKF5601X", G, S_1P_100m_50V },
{SMT, "6K65",					"kiwi-SM0402",		"Panasonic",		"ERJ-2RKF6651X", G, S_1P_100m_50V },
{SMT, "10K",					"kiwi-SM0402",		"Panasonic",		"ERJ-2RKF1002X", G, S_1P_100m_50V },

{SMT, "0R",						"kiwi-SM0805",		"Panasonic",		"ERJ-6GEY0R00V", G, "", "zero-ohm jumper" },
{SMT, "10R/0.5W",				"kiwi-SM0805",		"Panasonic",		"ERJ-P06F10R0V", G, S_1P_500m_150V },
{SMT, "47R",					"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF47R0V", G, S_1P_125m_150V },
{SMT, "100R",					"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF1000V", G, S_1P_125m_150V },
{SMT, "220R/0.5W",				"kiwi-SM0805",		"Panasonic",		"ERJ-P06F2200V", G, S_1P_500m_150V },
{SMT, "470R",					"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF4700V", G, S_1P_125m_150V },
{SMT, "680R",					"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF6800V", G, S_1P_125m_150V },
{SMT, "1K",						"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF1001V", G, S_1P_125m_150V },
{SMT, "1K5",					"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF1501V", G, S_1P_125m_150V },
{SMT, "2K2",					"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF2201V", G, S_1P_125m_150V },
{SMT, "10K",					"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF1002V", G, S_1P_125m_150V },
{SMT, "11K",					"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF1102V", G, S_1P_125m_150V },
{SMT, "11K5",					"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF1152V", G, S_1P_125m_150V },
{SMT, "100K",					"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF1003V", G, S_1P_125m_150V },
{SMT, "1M",						"kiwi-SM0805",		"Panasonic",		"ERJ-6ENF1004V", G, S_1P_125m_150V },

{SMT, "10R/0.66W",				"kiwi-SM1206",		"Panasonic",		"ERJ-P08F10R0V", G, S_1P_660m_500V },
{SMT, "220R/0.66W",				"kiwi-SM1206",		"Panasonic",		"ERJ-P08F2200V", G, S_1P_660m_500V },

{SMT, "100R",					"kiwi-RNET_CAY16_J8", "Bourns",	"CAY16-101J8LF", 16, GP|FP, "5% 62.5mW 25V", "isolated resistor array" },

// caps
{SMT, "22p",					"kiwi-SM0402",		"Murata",	"GRM1555C1H220JA01D", G, S_5P_C0G_50V },
{SMT, "56p/100",				"kiwi-SM0402",		"Murata",	"GRM1555C2A560JA01D", G, S_5P_C0G_100V, ">= 100V" },
{SMT, "100p",					"kiwi-SM0402",		"Murata",	"GRM1555C1H101JA01D", G, S_5P_C0G_50V },
{SMT, "160p/50",				"kiwi-SM0402",		"Murata",	"GRM1555C1H161JA01D", G, S_5P_C0G_50V, ">= 50V" },
{SMT, "1n",						"kiwi-SM0402",		"Murata",	"GCM155R71H102KA37D", G, S_10P_X7R_50V },
{SMT, "10n",					"kiwi-SM0402",		"Murata",	"GRM155R71C103KA01D", G, S_10P_X7R_16V },
{SMT, "100n",					"kiwi-SM0402",		"Murata",	"GRM155R71C104KA88D", G, S_10P_X7R_16V },
{SMT, "470n",					"kiwi-SM0402",		"Murata",	"GRM155R6YA474KE01D", G, S_10P_X5R_35V },
{SMT, "1u",						"kiwi-SM0402",		"Murata",	"GRM155R61E105KA12D", G, S_10P_X5R_25V },
{SMT, "2.2u",					"kiwi-SM0402",		"Murata",	"GRM155R60J225ME15D", G, S_20P_X5R_6V3 },
{SMT, "4.7u",					"kiwi-SM0402",		"Murata",	"GRM155R60J475ME47D", G, S_20P_X5R_6V3 },

{SMT, "1n/100",					"kiwi-SM0805",		"Murata",	"GRM219R72A102KA01J", G, S_10P_X7R_100V, ">= 100V" },
{SMT, "100n/100",				"kiwi-SM0805",		"Murata",	"GRM21BR72A103KA01L", G, S_10P_X7R_100V },
{SMT, "10u/25",					"kiwi-SM0805",		"Murata",	"GRM219R61E106KA12D", G, S_10P_X5R_25V },
{SMT, "22u",					"kiwi-SM0805",		"Murata",	"GRM21BR60J226ME39L", G, S_20P_X5R_6V3, },

{SMT, "470n/100",				"kiwi-SM1206",		"AVX",		"ESD61C474K4T2A-28", G, S_10P_X7R_100V, ">= 100V" },
{SMT, "100u",					"kiwi-SM1206",		"Murata",	"GRM31CR60J107ME39L", G, S_20P_X5R_6V3 },

{SMT, "22u/25 TA",				"kiwi-CAP_D",		"Kemet",	"T491D226K025AT", G, "10% 25V 0R8ESR", "tantalum capacitor" },
{SMT, "330u/35 EL",				"kiwi-CAP_10x10",	"Nichicon",	"UWT1V331MNL1GS", G, "20% 35V 0A3RIPPLE", "aluminum electrolytic capacitor" },

// inductors
{SMT, "FB 80Z",					"kiwi-SM0805",		"TDK",		"MMZ2012D800B", G, "25% 500mA 80Z@100M", "ferrite chip" },
{SMT, "FB 600Z",				"kiwi-SM0402",		"TDK",		"MMZ1005B601C", G, "25% 200mA 600Z@100M", "ferrite chip" },
{SMT, "39nH",					"kiwi-SM0402",		"TDK",		"MLK1005S39NJ", X, "5% 200mA 2GHzSelfRes 6Q@100M", "chip inductor", ">= 2GHz self-resonance (above GPS L1)" },
{SMT, "150nH",					"kiwi-SM0402",		"TDK",		"MLG1005SR15J", G, "5% 150mA 8Q@100M", "chip inductor" },
{SMT, "270nH",					"kiwi-SM0402",		"TDK",		"MLG1005SR27J", G, "3% 100mA 8Q@100M", "chip inductor" },
{SMT, "330nH",					"kiwi-SM0402",		"TDK",		"MLG1005SR33J", G, "3% 50mA 6Q@100M", "chip inductor" },

{SMT, "1uH 3.7A",				"kiwi-INDUCTOR_4x4", "Bourns",	"SRN4018-1R0Y", G, "ferrite 30% 3.7A", "inductor; use: SMPS" },
{SMT, "100uH",					"kiwi-SM1812",		"TDK",		"B82432T1104K", X, "ferrite 10% 200mA 20Q@0.8MHz", "inductor; use: bias tee", ">= 200 mA DC" },
{SMT, "CMC 500uH 1A",			"kiwi-CMC",			"Bourns",	"SRF0905A-501Y", 4, NA, "ferrite 50% 1A", "inductor; use: bias tee", "<= 0.15R", "501Y" },
{SMT, "CMC 2mH 0.6A",			"kiwi-CMC",			"Bourns",	"SRF0905A-202Y", 4, NA, "ferrite 50% 600mA", "inductor; use: bias tee", "<= 0.42R", "202Y" },
{SMT, "20mH 1A",				"kiwi-CMC_22x22",	"Bourns",	"PM3700-80-RC", G, "ferrite 1A 0.25R", "inductor; use: bias tee", ">= 200 mA DC" },
{NLP, "SAW L1",					"kiwi-SAW",			"RFM",		"SF1186G", 4, GP, "1572.42MHz 2MHzBW", "GPS SAW filter", "", "2A" },

{SMT, "T1-6",					"kiwi-MCL_KK81",	"MCL",		"T1-6-KK81+", 6, NA, "1:1 10kHZ-150MHz ", "RF transformer" },
{SMT, "T-622",					"kiwi-MCL_KK81",	"MCL",		"T-622-KK81+", 6, NA, "1:1:1 100KHz-200MHz", "RF transformer" },

// semis
{NLP, "SE4150L",				"kiwi-QFN24",		"Skyworks",	"SE4150L-R", 24, FP, "", "GPS front-end" },
{SMT, "EEPROM 32Kx8",			"kiwi-TSSOP8",		"On Semi",	"CAT24C256YI-GT3", 8, GP, "256kb=32kx8b 100k/400k/1MHz I2C", "EEPROM" },
{SMT, "NC7SZ125",				"kiwi-SC70_5",		"Fairchild", "NC7SZ125P5X", 5, NA, "3.3V 2.6nsTpd", "clock buffer", "", "Z25" },

{NLP, "LMR10530Y 1.0V 3A",		"kiwi-WSON10",		"TI",		"LMR10530YSD/NOPB", 10, FP, "5.5Vin 3A 3MHz", "step-down voltage reg" },
{SMT, "LP5907 3.3V 250mA",		"kiwi-SOT23_5",		"TI",		"LP5907QMFX-3.3Q1", 5, NA, "3.3V 250mA 0.12Vdo", "low-noise LDO voltage reg" },
{SMT, "LP5907 1.8V 250mA",		"kiwi-SOT23_5",		"TI",		"LP5907QMFX-1.8Q1", 5, NA, "1.8V 250mA 0.12Vdo", "low-noise LDO voltage reg" },
{SMT, "LM2941",					"kiwi-TO263",		"TI",		"LM2941SX/NOPB", 6, NA, "26Vin 1.0A 1.0Vdo", "adj LDO voltage reg" },
{SMT, "TPS7A4501",				"kiwi-SOT223_6",	"TI",		"TPS7A4501DCQR", 6, NA, "20Vin 1.5A 0.3Vdo", "adj low-noise LDO voltage reg" },

{SMT, "J310",					"kiwi-SOT23_DGS",	"Fairchild", "MMBFJ310", 3, GP, "25Vds", "N-chan JFET", "", "6T", 0, "512-MMBFJ310" },
{SMT, "J271",					"kiwi-SOT23_DGS",	"Fairchild", "MMBFJ271", 3, GP, "30Vgs", "P-chan JFET", "", "62T", 0, "512-MMBFJ271" },
{SMT, "BFG35",					"kiwi-SOT223_EBEC",	"NXP",		"BFG35.115", 3, NA, "18Vceo 4GHz", "NPN wideband", "", 0, 0, "771-BFG35-T/R" },
{SMT, "BFG31",					"kiwi-SOT223_EBEC",	"NXP",		"BFG31.115", 3, NA, "15Vceo 5GHz", "PNP wideband", "", 0, 0, "771-BFG31-T/R" },
{SMT, "MMBT4401",				"kiwi-SOT23_BCE",	"On Semi",	"MMBT4401LT1G", 3, GP, "40Vceo 600mAIc", "NPN switching BJT", "", "2X", 0, "863-MMBT4401LT1G" },

{SMT, "SR 5A 40V",				"kiwi-DO214_AA",	"Vishay",	"SSB44-E3/52T", 0, POL, "40V 0.42Vf 4.0AIf ", "Schottky rectifier; use: SMPS", "<= 0.42 Vf", "S44" },
{SMT, "BR 0.5A 400V",			"kiwi-TO269_AA",	"Vishay",	"MB2S-E3/80", 4, GP, "200V 0.5AIf", "bridge rectifier", "", "2" },
{SMT, "TVS 3.3V",				"kiwi-SOD323",		"Bourns",	"CDSOD323-T03C", G, "3.3Vw 4.0Vbr 7.0Vclamp 3pF", "bi-directional TVS", "", "3C" },
{SMT, "TVS 25VAC",				"kiwi-SM0805",		"AVX",		"VC080531C650DP", G, "25VACwv 65Vclamp 0.3J", "TVS protection" },
{SMT, "PPTC 200 mA",			"kiwi-SM1812",		"Bourns",	"MF-MSMF020/60-2", G, "60Vmax 0.2Ahold 0.4Atrip 0.15SecTrip", "PTC resettable fuse" },
{SMT, "75V",					"kiwi-GDT",			"TE Conn",	"GTCS23-750M-R01-2", G, "75V 20% <0.5pF", "gas discharge tube" },

{NLP, "65.360 MHz",				"kiwi-VCXO",		"CTS",		"357LB3I065M3600", 4, GP, "3.3V 50ppm 1psTjms", "VCXO: prototype with 65.36 MHz since distributors stock this freq; for production special order 65.000 MHz from manufacturer", "<= 1ps RMS phase jitter" },
{NLP, "66.6666 MHz",			"kiwi-XO",			"Conner Winfield", "CWX823-066.6666M", 4, GP, "3.3V 50ppm 1psTjms", "XO: prototype with 66.6666 MHz since distributors stock this freq; for production special order 65.000 MHz from manufacturer", "<= 1ps RMS phase jitter" },
{NLP, "16.368 MHz",				"kiwi-TCXO",		"TXC",		"7Q-16.368MBG-T", 4, GP, "3.3V clipped sinewave tempco 0.5ppm", "TCXO", "tempco 0.5 ppm" },

// connectors
{SMT, "POWER JACK 2.1MM",		"kiwi-JACK_2_1MM_SMD", "Switchcraft", "RASM722PX", G, "2.1mm w/ locating pin", "power jack" },

// mechanical
{SMT, "RF_SHIELD 29x19",		"*",				"Laird",	"BMI-S-209-F", 12, GP, "29x19mm; 7mm high", "RF shield frame" },
{MAN, "RF_SHIELD_COVER 29x19",	"kiwi-RF_SHIELD_COVER", "Laird", "BMI-S-209-C", G, "29x19mm", "RF shield cover; manual install" },

// special quote
{NLP, "Artix-7 A35",			"kiwi-FTG256",		"Xilinx",	"XC7A35T-1FTG256C", 256, HC|SQ, "17x17 1.0mm BGA", "FPGA" },
{NLP, "LTC2248",				"kiwi-QFN32",		"Linear Tech", "LTC2248CUH#PBF", 32, FP|HC|SQ, "14-bit 65MHz 240mW", "ADC" },
{NLP, "LTC6401-20",				"kiwi-QFN16",		"Linear Tech", "LTC6401CUD-20#PBF", 16, FP|SQ, "+20dB 6.2dBNF 1.3GHz", "differential ADC driver" },

// OPL
#define USE_OPL
#ifdef USE_OPL
{EDGE, "SMA",					"kiwi-BNC_SMA_NO_VIP", "DanYang RongHua",	"SMAKE-11", 0, GP|OPL, "50 Ohm; edge mount", "SMA connector", "fits 1.6mm/63mil PCB thickness", 0, 0, "320160004" },
{EDGE, "SMA",					"kiwi-SMA_OPL_EM", "DanYang RongHua",	"SMAKE-11", 0, GP|OPL, "50 Ohm; edge mount", "SMA connector", "fits 1.6mm/63mil PCB thickness", 0, 0, "320160004" },
{TH, "HEADER 13x2",				"kiwi-BEAGLEBONE_BLACK", "Yxcon", "P125-1226A0AS116A1", 0, GP|OPL, "13 rows; 2 pins/row; 2.54mm/0.1in; 6mm tail length", "13x2 0.1 inch header connector", "", 0, 2, "320020092" },			// 13/26 pin
{TH, "TB_2",					"kiwi-TB_2P_2_54MM",	"Senger",	"GS019-2.54-02P-1", 0, GP|OPL, "2pos 2.54mm; fits 1.6mm/63mil thick PCB", "terminal block side wire entry", "", 0, 0, "320110130" },
{TH, "POWER JACK 2.1MM",		"kiwi-JACK_2_1MM_TH", "Shenzhen Best", "DC-044-A", 0, GP|OPL, "2.1mm w/ locating pin", "power jack", "", 0, 0, "320120002" },
#else
{EDGE, "SMA",					"kiwi-SMA_MOLEX_EM", "Molex",	"73251-1150", G, "50 Ohm; edge mount", "SMA connector", "fits 1.6mm/63mil PCB thickness" },
//{EDGE, "SMA",					"kiwi-BNC_SMA_NO_VIP", "Molex",	"73251-1150", G, "50 Ohm; edge mount", "SMA connector", "fits 1.6mm/63mil PCB thickness" },
//{EDGE, "SMA",					"kiwi-SMA_MOLEX_EM", "Molex",	"73251-1150", G, "50 Ohm; edge mount", "SMA connector", "fits 1.6mm/63mil PCB thickness" },
{TH, "HEADER 13x2",				"kiwi-BEAGLEBONE_BLACK", "TE Conn", "6-146253-3", G, "13 rows; 2 pins/row; 2.54mm/0.1in; 8.08mm tail length", "13x2 0.1 inch header connector", "non-std tail length 8.08mm", "", 2 },			// 13/26 pin
{TH, "TB_2",					"kiwi-TB_2P_2_54MM",	"TE Conn",	"282834-2", G, "2pos 2.54mm; fits 1.6mm/63mil thick PCB", "terminal block side wire entry", 0, 0, 0, "571-282834-2" },
#endif

// virtual
{VIR, "*",						"kiwi-E_FIELD_PROBE" },
{VIR, "*",						"kiwi-SCREW_HOLE_#8" },
{VIR, "*",						"kiwi-MTG_HOLE_3_5MM" },
{VIR, "*",						"kiwi-MTG_NPTH_3_5MM" },
{VIR, "*",						"kiwi-FIDUCIAL", },
{VIR, "*",						"kiwi-FIDUCIAL_2_SIDE", },
{VIR, "*",						"kiwi-TP_1250", },
{VIR, "*",						"kiwi-TP_600", },
{VIR, "*",						"kiwi-TP_VIA_1250", },
{VIR, "*",						"kiwi-TP_VIA_600", },
{VIR, "*",						"kiwi-KIWI_10MM", },
{VIR, "*",						"kiwi-OSHW_6MM", },
	
// DNL
{EDGE, "BNC",					"kiwi-BNC_SMA_EM",	"Amp Connex", "112640", G, "50 Ohm; edge mount", "BNC connector using SMA footprint", "fits 1.6mm/63mil PCB thickness; compatible with SMA footprint", 0, 0, "523-112640" },
{SMT, "U.FL",					"kiwi-U.FL",		"Hirose",	"U.FL-R-SMT-1", 3, GP, "", "coaxial connector" },
{TH, "RJ45",					"kiwi-RJ45_8",		"FCI",		"54602-908LF", 3, GP, "8pos; right angle; unshielded; CAT3", "RJ45 modular jack" },

{VIR, NULL}
};

#define BATCH_SIZE	100

#define	D_DIGIKEY	0
#define	D_MOUSER	1
#define	D_NEWARK	2
#define	D_AVEX		3
#define	D_NONE		4

// octopart		ignores lines w/ quan=0 or p/n=blank
// mouser		

int bom_items, proto_items, ccount_tot;

char *_pkg(char *pkg)
{
	if (pkg[0] == '*') return pkg; else return &pkg[5];
}

char *_place(pn_t *p)
{
	if (p->place == SMT || p->place == NLP) return "SMT";
	if (p->place == TH) return "T/H";
	if (p->place == EDGE) return "EDGE";
	if (p->place == MAN) return "MANUAL";
	if (p->place == DNL) return "DNL";
	return "UNKNOWN!";
}

int main(int argc, char *argv[])
{
	int i, j;
	FILE *fpr, *fow, *f2w, *fpw, *fiw, *fmw, *fdw[5];

	char *lb;
	char *s, *t;
	pn_t *p;
	
	if (argc >= 2) goto bom_octo;

	printf("creating BOM files \"%s\", \"%s\", \"%s\" and \"%s\" by assigning part/mfg info,\nbased on KiCAD-generated \"%s\" BOM file\n",
		TO_OCTO_FILE, TO_DNL_FILE, TO_ODS_FILE, INSERT_FILE, KICAD_CSV_FILE);
	if ((fpr = fopen(KICAD_CSV_FILE, "r")) == NULL) sys_panic("fopen " KICAD_CSV_FILE);
	if ((fow = fopen(TO_OCTO_FILE, "w")) == NULL) sys_panic("fopen " TO_OCTO_FILE);
	if ((f2w = fopen(TO_DNL_FILE, "w")) == NULL) sys_panic("fopen " TO_DNL_FILE);
	if ((fpw = fopen(TO_ODS_FILE, "w")) == NULL) sys_panic("fopen " TO_ODS_FILE);
	if ((fiw = fopen(INSERT_FILE, "w")) == NULL) sys_panic("fopen " INSERT_FILE);
	if ((fmw = fopen(MOUSER_FILE, "w")) == NULL) sys_panic("fopen " MOUSER_FILE);
	
	fprintf(fpw, "item#, quan, value, mfg, P/N, package, place, refs, SMD marking, fine pitch, polarized, generic substitution, OPL, specs, critical spec, notes\n");
	fprintf(fmw, "item#, quan, DNL, value, mfg, P/N, package, refs, mouser p/n\n");
	
	while (fgets(linebuf, NBUF, fpr)) {
		s = lb = strdup(linebuf);
		int dnl = 0;
		char *val = strsep(&s, ",");
		char *quan_s = strsep(&s, ",");
		char *refs = strsep(&s, ",");
		char *pkg = strsep(&s, ",");

		if (!strncmp(val, "DNL", 3)) {
			val += 3;
			if (*val == ' ') val++; else val = "(DNL)";
			dnl = 1;
		}

		// remove quotes around refs
		if (*refs == '"') refs++;
		t = refs + strlen(refs)-1;
		if (*t == '"') *t = 0;

		t = pkg + strlen(pkg)-1;
		if (*t == '\n') *t = 0;
		if (*pkg == 0) pkg = "(*fix pkg)";

		int quan = strtol(quan_s, NULL, 0);
		
		// accumulate BOM quantities into table due to wildcard matching
		for (p = pn; p->val; p++) {
			//printf("<%s> <%s>   <%s> <%s>\n", val, p->val, pkg, p->pkg);
			if ((!strcmp(p->val, "*") || !strcmp(p->val, val)) && (!strcmp(p->pkg, "*") || !strcmp(p->pkg, pkg))) {
				if (dnl) {
					ccount[DNL] += quan;
					//p->refs = refs;
					//p->quan = quan;
					fprintf(f2w, "DNL %s, %d, %s, %s, %s, %s\n", val, quan, p->mfg, p->pn, pkg, refs);
					proto_items++;
					fprintf(fpw, "#%d, %d, %s, %s, %s, %s, DNL, %s, %s, %s, %s, %s, %s%s, %s, %s, DO NOT LOAD; %s\n", proto_items, quan, val, p->mfg, p->pn, _pkg(pkg), refs,
						p->mark? p->mark:"", (p->attr & FP)? "FINE":"", (p->attr & POL)? "YES":"", (p->attr & GP)? "YES":"-- NO --",
						(p->attr & OPL)? "OPL ":"", (p->attr & OPL)? p->custom_pn:"",
						p->specs? p->specs:"", p->crit? p->crit:"", p->notes? p->notes:"");
					fprintf(fmw, "#%d, %d, DNL, %s, %s, %s, %s, %s, %s\n", proto_items, quan, val, p->mfg, p->pn, _pkg(p->pkg), refs, p->custom_pn? p->custom_pn :"");
				} else
				if (p->place == VIR) {
					ccount[VIR] += quan;
					fprintf(f2w, "VIR %s, %d, , , %s, %s\n", val, quan, pkg, refs);
				} else
				if (!dnl) {
					quan = p->force_quan? p->force_quan : quan;
					p->quan += quan;		// += due to wildcard matching
					ccount[p->place] += quan;
					ccount_tot += quan;
					
					for (i=0,j=1; i<NATTR; i++,j<<=1) {
						if (p->attr & j) acount[i] += quan;
					}
					
					if (p->place == NLP) {
						if (!p->pads) { printf("%s needs #pads specified\n", p->val); panic("npads"); }
						npads += quan * p->pads;
						//printf("%3d q%2d p%3d NLP %s\n", npads, quan, p->pads, p->val);
					} else
					if (p->place == SMT) {
						int pads = (!p->pads)? 2 : p->pads;
						npads += quan * pads;
						//printf("%3d q%2d p%3d SMT %s\n", npads, quan, pads, p->val);
					}
					
					// merge refs for wildcard case
					if (p->refs) {
						int sl1 = strlen(p->refs), sl2 = strlen(refs);
						p->refs[sl1] = ' ';		// okay to clobber terminating null since strdupcat uses strncpy/strncat
						p->refs = strdupcat(p->refs, sl1+1, refs, sl2);
					} else {
						p->refs = refs;
					}
				}
				break;
			}
		}
		
		// not in list
		if (!p->val) {
			if (!strcmp(val, "(DNL)")) {
				fprintf(f2w, "DNL no value, %d, , , %s, %s\n", quan, pkg, refs);
				ccount[DNL] += quan;
				proto_items++;
				fprintf(fpw, "#%d, %d, (no value), , , %s, DNL, %s, , , , YES, , , , DO NOT LOAD\n", proto_items, quan, _pkg(pkg), refs);
				fprintf(fmw, "#%d, %d, DNL, (no value), , , %s, %s,\n", proto_items, quan, _pkg(pkg), refs);
			} else {
				printf("%s%s, %d, [*no_part], , %s, %s\n", dnl? "DNL ":"", val, quan, pkg, refs);
			}
		}
	}
	
	int local_tot = 0;

	fprintf(fpw, "\n");
	for (p = pn; p->val; p++) {
		if (p->quan) {
			bom_items++; proto_items++;
			if (!(p->attr & OPL)) {
				fprintf(fow, "#%d, %d, %s, %s, %s, %s, %s\n", bom_items, p->quan, p->val, p->mfg, p->pn, p->pkg, p->refs);
				fprintf(fmw, "#%d, %d, , %s, %s, %s, %s, %s, %s\n", proto_items, p->quan, p->val, p->mfg, p->pn, _pkg(p->pkg), p->refs, p->custom_pn? p->custom_pn :"");
			}
			fprintf(fpw, "#%d, %d, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s%s, %s, %s, %s\n", proto_items, p->quan, p->val, p->mfg, p->pn, _pkg(p->pkg), _place(p), p->refs,
				p->mark? p->mark:"", (p->attr & FP)? "FINE":"", (p->attr & POL)? "YES":"", (p->attr & GP)? "YES":"-- NO --",
				(p->attr & OPL)? "OPL ":"", (p->attr & OPL)? p->custom_pn:"",
				p->specs? p->specs:"", p->crit? p->crit:"", p->notes? p->notes:"");
			local_tot += p->quan;
			
			// a file of SMT and NLP refs to compare against centroid file that use modules that have insert attribute set
			char *refs = strdup(p->refs);
			t = refs; while (*t) { if (*t == ' ') *t = '\n'; t++; }		// one ref per line
			if (p->place == SMT || p->place == NLP) fprintf(fiw, "%s\n", refs);
		}
	}

	if (ccount_tot != local_tot) {
		printf("ccount_tot %d, local_tot %d\n", ccount_tot, local_tot);
		assert(ccount_tot == local_tot);
	}
	
	// stats needed by proto assembly/PCB companies
	printf("BOM items: %d total\n", bom_items);
	
	printf("component count: %3d total", ccount_tot);
	for (i=0; i<NPLACE; i++) printf(", %d %s", ccount[i], place[i]);
	printf("\nSMT + no_lead = %d (check against centroid file count)\n", ccount[SMT]+ccount[NLP]);
	
	printf("attributes: ");
	for (i=0,j=1; i<NATTR; i++,j<<=1) printf("%s%d %s", (i>0)? ", ":"", acount[i], attr_str[i]);
	printf("\n");
	
	printf("other: %d SMT pads\n", npads);

	fclose(fpr);
	fclose(fow);
	fclose(f2w);
	fclose(fpw);
	fclose(fiw);
	fclose(fmw);
	exit(0);


bom_octo:
	printf("analyzing quotation BOM file \"%s\" using info from KiCAD BOM file \"%s\"\n",
		FROM_OCTO_FILE, KICAD_CSV_FILE);
	if ((fpr = fopen(FROM_OCTO_FILE, "r")) == NULL) sys_panic("fopen 3");

	if ((fdw[0] = fopen(DIGIKEY_FILE, "w")) == NULL) sys_panic("fopen 6");
	if ((fdw[1] = fopen(MOUSER_FILE, "w")) == NULL) sys_panic("fopen 6");
	if ((fdw[2] = fopen(NEWARK_FILE, "w")) == NULL) sys_panic("fopen 6");
	if ((fdw[3] = fopen(AVNET_EXP_FILE, "w")) == NULL) sys_panic("fopen 6");
	fdw[4] = NULL;
	
	// get part prices from octo csv file
	while (fgets(linebuf, NBUF, fpr)) {
		s = lb = strdup(linebuf);
		
		// remove commas embedded in quoted strings for benefit of strsep()
		int ql = 0;
		while (*s) {
			if (ql == 0 && *s == '"') ql = 1; else
			if (ql == 1 && *s == '"') ql = 0; else
			if (ql == 1 && *s == ',') *s = '.';
			s++;
		}
		s = lb;
		
		int quan = strtol(strsep(&s, ","), NULL, 0);
		char *part = strsep(&s, ",");
		for (i=0; i<31; i++) strsep(&s, ",");
		char *dist = strsep(&s, ",");
		int idist = D_NONE;
		if (!dist) idist = D_NONE; else
		if (!strcmp(dist, "Digi-Key")) idist = D_DIGIKEY; else
		if (!strcmp(dist, "Mouser")) idist = D_MOUSER; else
		if (!strcmp(dist, "Newark")) idist = D_NEWARK; else
		if (!strcmp(dist, "Avnet Express")) idist = D_AVEX;
		char *sku = strsep(&s, ",");
		strsep(&s, ",");
		char *price = strsep(&s, ",");
		//printf("q=%d part=%s dist=%s sku=%s $=%s\n", quan, part, dist, sku, price);
		
		for (p = pn; p->val; p++) {
			if (p->place != VIR && strlen(part) && !strcmp(p->pn, part)) {
				p->quan = quan;
				p->price = price? strtof(price, NULL) : 0;
				p->dist = dist;
				p->sku = sku;
				if (idist != D_NONE) fprintf(fdw[idist], "%s, %d, %6.4f %s,%s %s,%s \n", p->val, p->quan*BATCH_SIZE, p->price, p->mfg, p->pn, p->dist, p->sku);
				break;
			}
		}

		ccount_tot += quan;
		if (!p->val && quan) {
			printf("%2d       ? %s (no part match)\n", quan, part);
		}
	}
	
	for (i=0; i<4; i++)
		fclose(fdw[i]);
	
	fclose(fpr);
	printf("component count: %d\n", ccount_tot);
	
	if ((fpr = fopen(KICAD_CSV_FILE, "r")) == NULL) sys_panic("fopen 4");
	
	// show price breakdown by schematic sheet
	while (fgets(linebuf, NBUF, fpr)) {
		s = lb = strdup(linebuf);
		char *val = strsep(&s, ",");
		char *quan_s = strsep(&s, ",");
		char *refs = strsep(&s, ",");
		char *pkg = strsep(&s, ",");

		if (!strncmp(val, "DNL", 3)) continue;

		t = pkg + strlen(pkg)-1;
		if (*t == '\n') *t = 0;
		if (*pkg == 0) pkg = "(*fix pkg)";

		// remove quotes around refs
		if (*refs == '"') refs++;
		t = refs + strlen(refs)-1;
		if (*t == '"') *t = 0;
		s = refs;
		
		for (p = pn; p->val; p++) {
			if (strcmp(p->val, val) || (strcmp(p->pkg, "*") && strcmp(p->pkg, pkg)) || p->place == VIR) continue;
			p->seen = 1;
			
			// charge price for each reference
			do {
				char *ref = strsep(&s, " ");
				t = ref;
				while (!(*t >= '0' && *t <= '9')) t++;
				int pnum = strtol(t, NULL, 0);
				int rnum = pnum / 100;
				
				// fix sheet assignments for some refs that don't follow conventions
				if (*ref == 'J') {
					rnum = connectors[pnum];
					assert(rnum != BAD);
				}
				if (!strcmp(ref, "ZB1")) rnum = 3;
				
				assert (rnum >= 0 && rnum < NSHEETS);
				int cinc = 1;
				float pinc = p->price;
				
				// really two headers for this single library module (fixme)
				if (!strcmp(ref, "ZB1")) cinc = 2, pinc *= 2;
				
				p->sheet[rnum] += cinc;
				sheet_count[rnum] += cinc;
				sheet_count[NSHEETS] += cinc;
				sheet_price[rnum] += pinc;
				sheet_price[NSHEETS] += pinc;
			} while (s && *s);
		}
	}
	
	fclose(fpr);

	printf("%56s", "");
	for (i=0; i<=NSHEETS; i++) {
		printf("    %10s", sheet_name[i]);
	}
	printf("\n");

	for (j=1, p = pn; p->val; p++) {
		if (!p->seen) continue;
		printf("#%2d %2d %7.4f %-20.20s %-20.20s", j++, p->quan, p->price, p->val, p->pn);
		for (i=0; i<NSHEETS; i++) {
			if (p->sheet[i])
				printf("    %2d %7.4f", p->sheet[i], p->sheet[i]*p->price);
			else
				printf("%14s", "");
		}
		printf("\n");
	}
	
	printf("%56s", "");
	for (i=0; i<=NSHEETS; i++) {
		printf("    %10s", sheet_name[i]);
	}
	printf("\n%56s", "");
	for (i=0; i<=NSHEETS; i++) {
		printf("    %2d", sheet_count[i]);
		printf(" %7.4f", sheet_price[i]);
	}
	printf("\n");
	
	float tRX = sheet_price[0] + sheet_price[3] + sheet_price[4] + sheet_price[6] + sheet_price[7];
	float tGPS = sheet_price[1];
	float tANT = sheet_price[2] + sheet_price[5];
	float total = sheet_price[NSHEETS];
	printf(" RX $%5.2f %4.1f%%\n", tRX, tRX/total*100.0);
	printf("GPS $%5.2f %4.1f%%\n", tGPS, tGPS/total*100.0);
	printf("ANT $%5.2f %4.1f%%\n", tANT, tANT/total*100.0);
	
	exit(0);
}
