
// Add parts number and manufacturer fields to BOM based on value/footprint association.
// Resulting file can be uploaded to e.g. octopart.com for quotation.

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

#define	KICAD_CSV_FILE	"wrx.csv"
#define	TO_OCTO_FILE	"wrx.bom.csv"
#define	TO_DNL_FILE		"wrx.bom.dnl.csv"
#define	TO_PROTO_FILE	"wrx.bom.proto.csv"
#define	FROM_OCTO_FILE	"wrx.bom.octo.csv"

#define NBUF 1024
char linebuf[NBUF];

#define NSHEETS 8
char *sheet_name[NSHEETS+1] = { "Fpwr", "gps", "inj", "bone", "adc", "ant", "Fio", "swr", "TOTAL" };
int sheet_count[NSHEETS+1];
float sheet_price[NSHEETS+1];


// stats needed by proto assembly/PCB companies

#define	SMT		0
#define	NLP		1
#define	TH		2
#define	MAN		3
#define	NP		4
#define	VIR		5
#define	DNL		6
#define	NPLACE	7

char *place_str[] = { "SMT", "no_lead", "T/H", "manual", "no_proto", "virtual", "DNL" };
int ccount[NPLACE];

#define	NA		0
#define	FP		0x1		// fine-pitch, <= 0.5mm pin centers
#define	NATTR	1

char *attr_str[] = { "fine_pitch" };
int acount[NATTR];


typedef struct {
	int place;
	char *val, *pkg, *mfg, *pn;
	int pads, attr;
	int force_quan;
	char *mark;
	
	char *refs;
	int quan, sheet[NSHEETS];
	float price;
	char *dist, *sku;
	int seen;
} pn_t;

int npads;

//	sheet	refs	connectors
//	ADC		4xx		J1=rf
//	GPS		1xx		J3=in J4=extclk
//	Bone	3xx		ZB1
//	a.ant	5xx		J10=rf J11=pwr J12=pwr j13=gnd
//	fpga_p	01x
//	pwr		7xx
//	inj		2xx		J22=AC J21=ant J20=rx J23=pwr J24=gnd
int connectors[] = { 9, 4, 9, 1, 1, 9, 9, 9, 9, 9, 5, 5, 5, 5, 9, 9, 9, 9, 9, 9, 2, 2, 2, 2, 2 };

// fixme
// v.reg fb resis

pn_t pn[] = {
//	value						package				manuf		part number				notes, incl mouser q100 price
{SMT, "0R",						"wrx-SM0402",		"Pana",		"ERJ-2GE0R00X"},
{SMT, "10R",					"wrx-SM0402",		"Pana",		"ERJ-2RKF10R0X"},			// 1% 1/10W 0.008
{SMT, "28R7",					"wrx-SM0402",		"Pana",		"ERJ-2RKF28R7X"},			// 1% 1/10W 0.010
{SMT, "66R5",					"wrx-SM0402",		"Pana",		"ERJ-2RKF66R5X"},			// 1% 1/10W 0.010
{SMT, "100R",					"wrx-SM0402",		"Pana",		"ERJ-2RKF1000X"},			// 1% 1/10W 0.008
{SMT, "5K6",					"wrx-SM0402",		"Pana",		"ERJ-2RKF5601X"},			// PU
{SMT, "6K65",					"wrx-SM0402",		"Pana",		"ERJ-2RKF6651X"},
{SMT, "10K",					"wrx-SM0402",		"Pana",		"ERJ-2RKF1002X"},

//{SMT, "0R",						"wrx-SM0603",		"Pana",		"ERJ-3GEY0R00V"},
//{SMT, "10R",						"wrx-SM0603",		"Pana",		"ERJ-3EKF10R0V"},
//{SMT, "28R7",					"wrx-SM0603",		"Pana",		"ERJ-3EKF28R7V"},
//{SMT, "66R5",					"wrx-SM0603",		"Pana",		"ERJ-3EKF66R5V"},
//{SMT, "100R",					"wrx-SM0603",		"Pana",		"ERJ-3EKF10R1V"},
//{SMT, "5K6",						"wrx-SM0603",		"Pana",		"ERJ-3EKF5601V"},			// PU
//{SMT, "6K65",					"wrx-SM0603",		"Pana",		"ERJ-3EKF6651V"},
//{SMT, "10K",						"wrx-SM0603",		"Pana",		"ERJ-3EKF1002V"},

{SMT, "0R",						"wrx-SM0805",		"Pana",		"ERJ-6GEY0R00V"},
{SMT, "47R",					"wrx-SM0805",		"Pana",		"ERJ-6ENF47R0V"},			// 1% 1/8W 150V
{SMT, "220R/0.5W",				"wrx-SM0805",		"Pana",		"ERJ-P06F2200V"},			// 1% 1/2W 150V
{SMT, "680R",					"wrx-SM0805",		"Pana",		"ERJ-6ENF6800V"},
{SMT, "1K",						"wrx-SM0805",		"Pana",		"ERJ-6ENF1001V"},
{SMT, "1K5",					"wrx-SM0805",		"Pana",		"ERJ-6ENF1501V"},
{SMT, "2K2",					"wrx-SM0805",		"Pana",		"ERJ-6ENF2201V"},
{SMT, "10K",					"wrx-SM0805",		"Pana",		"ERJ-6ENF1002V"},
{SMT, "11K",					"wrx-SM0805",		"Pana",		"ERJ-6ENF1102V"},
{SMT, "11K5",					"wrx-SM0805",		"Pana",		"ERJ-6ENF1152V"},
{SMT, "1M",						"wrx-SM0805",		"Pana",		"ERJ-6ENF1004V"},

{SMT, "100R",					"wrx-RNET_CAY16_J8", "Bourns",	"CAY16-101J8LF", 16, FP},			// 5% 0.096

{SMT, "22p",					"wrx-SM0402",		"Murata",	"GRM1555C1H220JA01D"},
{SMT, "56p",					"wrx-SM0402",		"Murata",	"GRM1555C1H560JA01D"},
{SMT, "100p",					"wrx-SM0402",		"Murata",	"GRM1555C1H101JA01D"},
{SMT, "160p",					"wrx-SM0402",		"Murata",	"GRM1555C1H161JA01D"},
{SMT, "1n",						"wrx-SM0402",		"Murata",	"GCM155R71H102KA37D"},		// X7R 50V 10% 0.012
{SMT, "10n",					"wrx-SM0402",		"Murata",	"GRM155R71C103KA01D"},		// X7R 16V 10% 0.004
{SMT, "100n",					"wrx-SM0402",		"Murata",	"GRM155R71C104KA88D"},		// X7R 16V 10% 0.005 bypass
{SMT, "470n",					"wrx-SM0402",		"Murata",	"GRM155R6YA474KE01D"},		// X5R 35V 10% 0.020 FPGA, ADC
{SMT, "1u",						"wrx-SM0402",		"Murata",	"GRM155R61E105KA12D"},		// X5R 25V 10% 0.023 ADC
{SMT, "2.2u",					"wrx-SM0402",		"Murata",	"GRM155R60J225ME15D"},		// X5R 6.3V 20% 0.063 ADC
{SMT, "4.7u",					"wrx-SM0402",		"Murata",	"GRM155R60J475ME47D"},		// X5R 6.3V 20% FPGA

//{SMT, "1n",						"wrx-SM0603",		"Murata",	"GCM188R71H102KA37D"},		// X7R 50V 10%
//{SMT, "10n",						"wrx-SM0603",		"Murata",	"GCM188R71H103KA37D"},		// X7R 50V 10%
//{SMT, "100n",					"wrx-SM0603",		"Murata",	"GRM188R71E102KA01D"},		// X7R 25V 10%
//{SMT, "470n",					"wrx-SM0603",		"Murata",	"GRM188R71E474KA12D"},		// X7R 25V 10%
//{SMT, "1u",						"wrx-SM0603",		"Murata",	"GRM188R61E105KA12D"},		// X5R 25V 10%
//{SMT, "2.2u",					"wrx-SM0603",		"Murata",	"GRM188R60J225KE19D"},		// X5R 6.3V 10%

{SMT, "100n/100",				"wrx-SM0805",		"Vishay",	"VJ0805V104MXBPW1BC"},		// Y5V 100V 20% 0.022
{SMT, "22u",					"wrx-SM0805",		"Murata",	"GRM21BR60J226ME39L"},		// X5R 6.3V 20% SMPS
{SMT, "10u/25",					"wrx-SM0805",		"Murata",	"GRM219R61E106KA12D"},		// X5R 25V 10%

{SMT, "470n/100",				"wrx-SM1206",		"AVX",		"ESD61C474K4T2A-28"},		// X7R 100V 10% 0.091
{SMT, "100u",					"wrx-SM1206",		"Murata",	"GRM31CR60J107ME39L"},		// 6.3V X5R 20%

{SMT, "1n/100",					"wrx-SM0805",		"Vishay",	"VJ0805Y102MXBPW1BC"},		// 100V X7R 20% 0.012
{SMT, "470n/250",				"wrx-SM1812",		"TDK",		"C4532X7R2E474K230KA"},		// 250V X7R

{SMT, "22u/25 TA",				"wrx-CAP_D",		"Kemet",	"T491D226K025AT"},			// 25V Tantalum ESR 0R8
{SMT, "470u/35 EL",				"wrx-CAP_10x135",	"Nichicon",	"UCL1V471MNL1GS"},			// 35V Aluminum electrolytic 0.40

{SMT, "39nH",					"wrx-SM0402",		"TDK",		"MLK1005S39NJ"},			// self-res above L1
{SMT, "150nH",					"wrx-SM0402",		"TDK",		"MLG1005SR15J"},
{SMT, "270nH",					"wrx-SM0402",		"TDK",		"MLG1005SR27J"},
{SMT, "330nH",					"wrx-SM0402",		"TDK",		"MLG1005SR33J"},

{SMT, "1uH 3.7A",				"wrx-INDUCTOR_4x4",	"Bourns",	"SRN4018-1R0Y"},			// smps
{SMT, "100uH",					"wrx-SM1812",		"TDK",		"B82432T1104K"},			// ?

{SMT, "BR 0.5A 400V",			"wrx-TO269_AA",		"Vishay",	"MB2S-E3/80", 4},
//{SMT, "ZR 24V",					"wrx-DO214_AA",		"MCC",		"SMBJ5359B-TP"},			// 5% 5W 0.316
{SMT, "TVS 40V",				"wrx-SM0402",		"Murata",	"LXES15AAA1-100"},			// 0.123
{SMT, "SR 5A 40V",				"wrx-DO214_AA",		"Vishay",	"SSB44-E3/52T"},
{SMT, "J310",					"wrx-SOT23_DGS",	"Fairchild", "MMBFJ310", 3},
{SMT, "BFG35",					"wrx-SOT223_EBEC",	"NXP",		"BFG35.115", 3},

{SMT, "LP5907 3.3V 250mA",		"wrx-SOT23_5",		"TI",		"LP5907QMFX-3.3Q1", 5},
{SMT, "LP5907 1.8V 250mA",		"wrx-SOT23_5",		"TI",		"LP5907QMFX-1.8Q1", 5},
{NLP, "LMR10530Y 1.0V 3A",		"wrx-WSON10",		"TI",		"LMR10530YSD/NOPB", 10, FP},
{SMT, "TPS7A4501",				"wrx-SOT232_6",		"TI",		"TPS7A4501DCQR", 6},
{SMT, "LM2941",					"wrx-TO263",		"TI",		"LM2941SX/NOPB", 6},

{NLP, "Artix-7 A35",			"wrx-FTG256",		"Xilinx",	"XC7A35T-1FTG256C", 256},
{NLP, "SE4150L",				"wrx-QFN24",		"Skyworks",	"SE4150L-R", 24, FP},
{SMT, "EEPROM 32Kx8",			"wrx-TSSOP8",		"On",		"CAT24C256YI-GT3", 8},
{SMT, "NC7SZ126",				"wrx-SC70_5",		"Fairchild", "NC7SZ126P5X", 5, NA, 0, "Z26"},
{NLP, "LTC2248",				"wrx-QFN32",		"LTC",		"LTC2248CUH#PBF", 32, FP},
{NLP, "LTC6401-20",				"wrx-QFN16",	"LTC",		"LTC6401CUD-20#PBF", 16, FP},

{SMT, "FB 80Z",					"wrx-SM0805",		"TDK",		"MMZ2012D800B"},		// ant
{SMT, "FB 600Z",				"wrx-SM0402",		"TDK",		"MMZ1005B601C"},		// 200 mA 0.03 ADC, GPS
{NLP, "65.360 MHz",				"wrx-VCXO",			"CTS",		"357LB3I065M3600", 4},
{NLP, "16.368 MHz",				"wrx-TCXO",			"TXC",		"7Q-16.368MBG-T", 4},
{NLP, "SAW L1",					"wrx-SAW",			"RFM",		"SF1186G", 4},
{SMT, "75V",					"wrx-GDT",			"TE",		"GTCS23-750M-R01-2"},
{SMT, "PPTC 200 mA",			"wrx-SM1812",		"Bourns",	"MF-MSMF020/60-2"},		// hold 200ma trip 400ma/150ms/800mW 60Vmax 0.375
{SMT, "TVS 25V",				"wrx-SM0805",		"AVX",		"VC080531C650DP"},		// 65V clamp 0.17
//{SMT, "CMC 4.7 mH",				"wrx-CHOKE",		"Bourns",	"DR331-475BE", 4},			// 200mA 0.577
{SMT, "CMC 4.7 mH",				"wrx-CMC",			"Bourns",	"SRF0905A-472Y", 4, NA, 0, "472Y"},		// 400mA 0.77
{NLP, "TC4-1TX+",				"wrx-MCL_AT1521",	"MCL",		"TC4-1TX+", 5},
//{NLP, "TC4-1T+",					"wrx-FIXME",		"MCL",		"TC4-1T+", ?},
{NLP, "T1-6",					"wrx-MCL_KK81",		"MCL",		"T1-6-KK81+", 6},
{NLP, "T-622",					"wrx-MCL_KK81",		"MCL",		"T-622-KK81+", 6},

{TH, "TB_2",					"wrx-TERM_BLOCK_2",	"TE",		"282836-2"},
//{TH, "TB_3",					"wrx-TERM_BLOCK_3",	"TE",		"282836-3"},
{TH, "SMA",						"wrx-SMA_EM",		"Molex",	"73251-1150"},
{TH, "BEAGLEBONE_BLACK",		"wrx-BEAGLEBONE_BLACK", "TE",	"6-146253-3", 2},			// 13/26 pin
//{TH, "BEAGLEBONE_BLACK",		"wrx-BEAGLEBONE_BLACK", "TE",	"6-146253-1", 2},			// 11/22 pin
//{SMT, "RF_SHIELD 16x16",			"wrx-RF_SHIELD_16x16", "Laird",	"BMI-S-202-F"},
//{MAN, "RF_SHIELD_COVER 16x16",	"wrx-RF_SHIELD_COVER", "Laird", "BMI-S-202-C"},
{NP, "RF_SHIELD 29x19",			"*", "Laird",	"BMI-S-209-F", 12},
{MAN, "RF_SHIELD_COVER 29x19",	"wrx-RF_SHIELD_COVER", "Laird", "BMI-S-209-C"},
//{SMT, "RF_SHIELD 38x25",			"wrx-RF_SHIELD_38x25", "Laird",	"BMI-S-205-F"},
//{MAN, "RF_SHIELD_COVER 38x25",	"wrx-RF_SHIELD_COVER", "Laird", "BMI-S-205-C"},

// virtual
{VIR, "*",						"wrx-SCREW_HOLE_#8" },
{VIR, "*",						"wrx-FIDUCIAL", },
{VIR, "*",						"wrx-TP_TOP", },
{VIR, "*",						"wrx-TP_BOT", },
{VIR, "*",						"wrx-TP_VIA_TOP", },
{VIR, "*",						"wrx-TP_VIA_BOT", },
	
// DNL
{SMT, "J271",					"wrx-SOT23_DGS",	"Fairchild", "MMBFJ271", 3},
{SMT, "BFG31",					"wrx-SOT223_EBEC",	"NXP",		"BFG31.115", 3},
{SMT, "U.FL",					"wrx-U.FL",			"Hirose",	"U.FL-R-SMT-1", 3},
{TH, "RJ45",					"wrx-RJ45_8",		"FCI",		"54602-908LF"},

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

int bom_items, ccount_tot;

int main(int argc, char *argv[])
{
	int i, j;
	FILE *fpr, *fpw, *f2w, *fdw[5];

	char *lb;
	char *s, *t;
	pn_t *p;
	
	if (argc >= 2) goto bom_octo;

	printf("creating BOM files \"%s\" and \"%s\" by assigning part/mfg info, based on KiCAD-generated \"%s\" BOM file\n",
		TO_OCTO_FILE, TO_DNL_FILE, KICAD_CSV_FILE);
	if ((fpr = fopen(KICAD_CSV_FILE, "r")) == NULL) sys_panic("fopen 1");
	if ((fpw = fopen(TO_OCTO_FILE, "w")) == NULL) sys_panic("fopen 2");
	if ((f2w = fopen(TO_DNL_FILE, "w")) == NULL) sys_panic("fopen 5");
	
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
					fprintf(f2w, "DNL %s, %d, %s,%s, %s, %s\n", val, quan, p->mfg, p->pn, pkg, refs);
				} else
				if (p->place == VIR) {
					ccount[VIR] += quan;
					fprintf(f2w, "VIR %s, %d, ,, %s, %s\n", val, quan, pkg, refs);
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
						p->refs[sl1-1] = ' ';
						p->refs = strdupcat(p->refs, sl1, refs+1, sl2-1);
					} else {
						p->refs = refs;
					}
				}
				break;
			}
		}
		if (!p->val) {
			if (!strcmp(val, "(DNL)")) {
				fprintf(f2w, "DNL no value, %d, ,, %s, %s\n", quan, pkg, refs);			
			} else {
				printf("%s%s, %d, [*no_part],, %s, %s\n", dnl? "DNL ":"", val, quan, pkg, refs);
			}
		}
	}
	
	int local_tot = 0;
	for (p = pn; p->val; p++) {
		if (p->quan) {
			bom_items++;
			fprintf(fpw, "#%d, %d, %s, %s,%s, %s, %s\n", bom_items, p->quan, p->val, p->mfg, p->pn, p->pkg, p->refs);
			local_tot += p->quan;
		}
	}
	assert(ccount_tot == local_tot);
	
	// stats needed by proto assembly/PCB companies
	printf("BOM items: %d total\n", bom_items);
	
	printf("component count: %3d total", ccount_tot);
	for (i=0; i<NPLACE; i++) printf(", %d %s", ccount[i], place_str[i]);
	printf("\n");
	
	printf("attributes: ");
	for (i=0,j=1; i<NATTR; i++,j<<=1) printf("%d %s ", acount[i], attr_str[i]);
	printf("\n");
	
	printf("other: %d SMT pads\n", npads);

	fclose(fpr);
	fclose(fpw);
	fclose(f2w);
	exit(0);

bom_octo:
	printf("analyzing quotation BOM file \"%s\" using info from KiCAD BOM file \"%s\"\n",
		FROM_OCTO_FILE, KICAD_CSV_FILE);
	if ((fpr = fopen(FROM_OCTO_FILE, "r")) == NULL) sys_panic("fopen 3");

	if ((fdw[0] = fopen("wrx.bom.digi.csv", "w")) == NULL) sys_panic("fopen 6");
	if ((fdw[1] = fopen("wrx.bom.mou.csv", "w")) == NULL) sys_panic("fopen 6");
	if ((fdw[2] = fopen("wrx.bom.new.csv", "w")) == NULL) sys_panic("fopen 6");
	if ((fdw[3] = fopen("wrx.bom.avex.csv", "w")) == NULL) sys_panic("fopen 6");
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
				if (*ref == 'J') rnum = connectors[pnum];
				if (!strcmp(ref, "ZB1")) rnum = 3;
				
				assert (rnum >= 0 && rnum < NSHEETS);
				int cinc = 1;
				float pinc = p->price;
				
				// really two headers for this single library module
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
