
// See Makefile for instructions about how to properly download .csv file from eibispace.de
// and also add-back ALE entries.

#include "types.h"
#include "printf.h"
#include "mem.h"
#include "str.h"
#include "mode.h"
#include "dx.h"

#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

const char *fi_s = "sked-current.csv";
const char *fo_s = "../../dx/EiBi.h";
int line = 2;

void _sys_panic(const char *str, const char *file, int line)
{
	char *buf;
	
	// errno might be overwritten if the malloc inside asprintf fails
	asprintf(&buf, "SYS_PANIC: \"%s\" (%s, line %d)", str, file, line);
	perror(buf);
	exit(-1);
}

void _panic(const char *str, bool x, const char *file, int line)
{
	printf("PANIC: \"%s\" (%s, line %d)", str, file, line);
	exit(-1);
}

#undef assert
#define assert(e) \
    if (!(e)) { \
        printf("assertion failed: \"%s\" %s line %d (%s line %d)\n", #e, __FILE__, __LINE__, fi_s, line); \
        panic("assert"); \
    }

void eibi_fgets(u1_t *bp, int size, FILE *fp)
{
    int c;
    do {
        c = fgetc(fp);
        if (c != EOF)
            *bp++ = c;
    } while (c != EOF && c != '\n');
    *bp = '\0';
}

// Makes a copy of ocp since delimiters are turned into NULLs.
// If ocp begins with delimiter a null entry is _not_ made in argv (and reflected in returned count).
// Caller must free *mbuf
int _kiwi_split(char *ocp, char **mbuf, const char *delims, char *argv[], int nargs)
{
	int n=0;
	char **ap, *tp;
	*mbuf = (char *) kiwi_imalloc("kiwi_split", strlen(ocp)+1);
	strcpy(*mbuf, ocp);
	tp = *mbuf;
	
	for (ap = argv; (*ap = strsep(&tp, delims)) != NULL;) {
		//if (**ap != '\0') {
			n++;
			if (++ap >= &argv[nargs])
				break;
		//}
	}
	
	return n;
}

int char_freq[256];

typedef struct {
    int sl;
    int ct;
    char ident[32];
} ident_dict_t;

ident_dict_t ident_dict[4096];
int ident_dict_n;

typedef struct {
    int sl;
    int ct;
    char notes[64];
} notes_dict_t;

notes_dict_t notes_dict[4096];
int notes_dict_n;

int types_n[64];
const char *types_s[64] = {
    "bcast", "util", "time", "ale", "hfdl", "milcom", "cw", "fsk", "fax", "aero", "marine", "spy", NULL
};

const char *types2_s[64] = {
    "BCST", "UTIL", "TIME", "ALE", "HFDL", "MIL", "CW_", "FSK", "FAX", "AERO", "MAR", "SPY"
};

const char * const mode_s[] = {
    "AM", "AMN", "USB", "LSB", "CW", "CWN", "FM", "IQ", "DRM",
    "USN", "LSN", "SAM", "SAU", "SAL", "SAS", "QAM", "FMN"
};

typedef enum { B_NONE, B_BCAST, B_MARINE, B_AERO } band_e;

typedef struct {
    float lo, hi;
    band_e b;
} band_t;

band_t band_f[] = {
    {     0,     0, B_NONE },
    {   153,   280, B_BCAST },  // LW
    {   525,  1710, B_BCAST },  // AM BCB
    
    // from dist.config.js, expanded to fit actual EiBi data (bcast vs aero/utility)
    {  2300,  2495, B_BCAST },
    {  3185,  3400, B_BCAST },  // 3200 => 3185
    {  3900,  4000, B_BCAST },
    {  4750,  5060, B_BCAST },
    {  5130,  5150, B_BCAST },  // prevent WBCQ and several others being classified as DX_AERO
    {  5850,  6210, B_BCAST },  // 5900,6200 => 5850,6210
    {  7200,  7800, B_BCAST },  // 7450 => 7800
    {  9300,  9900, B_BCAST },  // 9400 => 9300
    { 11550, 12150, B_BCAST },  // 11600,12100 => 11550,12150
    { 13570, 13870, B_BCAST },
    { 15000, 15800, B_BCAST },  // 15100 => 15000
    { 17480, 17900, B_BCAST },
    { 18900, 19020, B_BCAST },
    { 21450, 21850, B_BCAST },
    { 25600, 26100, B_BCAST },

    {   505,   527, B_MARINE },
    {  2172,  2190, B_MARINE },
    {  4063,  4438, B_MARINE },
    {  6200,  6525, B_MARINE },
    {  8195,  8815, B_MARINE },
    { 12230, 13200, B_MARINE },
    { 22000, 22855, B_MARINE },
    { 25070, 25121, B_MARINE },
    
    {  2850,  3155, B_AERO },
    {  3400,  3500, B_AERO },
    {  3900,  3950, B_AERO },
    {  4650,  4750, B_AERO },
    {  5450,  5730, B_AERO },
    {  6525,  6765, B_AERO },
    {  8815,  9040, B_AERO },
    { 10005, 10100, B_AERO },
    { 11175, 11400, B_AERO },
    { 13200, 13360, B_AERO },
    { 15010, 15100, B_AERO },
    { 17900, 18030, B_AERO },
    { 21924, 22000, B_AERO },
};

typedef struct {
    const char *country;
    const char *loc;
} hfdl_loc_t;

hfdl_loc_t hfdl_loc[] = {
    "KOR", "Seoul Muan",
    "ZAF", "Johannesburg",
    "CA",  "San Francisco",
    "ESP", "Canarias",
    "HI",  "Moloka'i",
    "AK",  "Barrow",
    "IRL", "Shannon",
    "NY",  "Riverhead",
    "PAN", "Albrook",
    "THA", "Hat Yai",
    "GUM", "Agana Guam",
    "BOL", "Santa Cruz",
    "RUS", "Krasnoyarsk",
    "BHR", "Al Muharraq",
    "NZL", "Auckland",
    "ISL", "Reykjavik",
};

char *qs(int n, const char *fmt, char *s)
{
    static char b[4][8];
    snprintf(b[n], 8, fmt, s);
    return b[n];
}

const char *day_s[] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
int day(char *s)
{
    int i;
    for (i = 0; i < 7; i++) {
        if (strncmp(s, day_s[i], 2) == 0)
            break;
    }
    if (i == 7) {
        printf("no day match? \"%s\"\n", s);
        panic("day");
    }
    return i;
}

//#define OPT_DEBUG_UTF_8
//#define OPT_NOTES
//#define OPT_DOW
//#define OPT_CHAR_FREQ

int main(int argc, char *argv[])
{
    int i, j, n;
    char c, *s;
    int max_ident = 0;
    int types_total = 0;
    
    FILE *fi, *fo;
    fi = fopen(fi_s, "rb");
    if (!fi) sys_panic("fi");
    fo = fopen(fo_s, "wb");
    if (!fo) sys_panic("fo");
    
    char lbuf[256];
    u1_t *lp = (u1_t *) lbuf;
    eibi_fgets(lp, sizeof(lbuf), fi);     // skip first line
    
    int same_freq_diff_ident = 0, max_same_freq_diff_ident = 0;
    bool same_freq_same_ident;
    double last_freq, max_same_freq_diff_ident_freq;
    char last_ident[256];
    char link[64];
    ssize_t bytes = readlink(fi_s, link, sizeof(link));
    if (bytes < 0) panic("readlink");
    link[bytes] = '\0';

    char abyy[8];
    bool ok = true;
    if (sscanf(link, "sked-%3s", abyy) != 1) panic("scanf abyy");
    if (abyy[0] == 'a') abyy[0] = 'A';
    else
    if (abyy[0] == 'b') abyy[0] = 'B';
    else
        ok = false;
    int yy;
    if (sscanf(&abyy[1], "%d", &yy) != 1) panic("scanf yy");
    if (yy < 24 || yy > 99) ok = false;
    if (!ok) {
        printf("ABYY fail: \"%s\" yy=%d\n", abyy, yy);
        panic("parse abyy");
    }
    
    int entries = 0;
    fprintf(fo,
        "// FILE GENERATED by pkgs/eibi_proc.cpp\n"
        "// MANUAL EDITS MAY BE OVERWRITTEN\n"
        "//\n"
        "// Derived from file: %s => %s\n\n"
        
        "const char eibi_abyy[4] = \"%s\";\n\n"

        "dx_t eibi_db[] = {\n"
        "//      freq begin   end  ext   dow type mode ident  cnty   lang   targ\n",
        fi_s, link, abyy
    );
    
    while (1) {
    
        double freq = 0;
        int begin = 9999, end = 9999;

        n = fscanf(fi, "%lf;%d-%d;", &freq, &begin, &end);
        if (n == EOF) break;
        if (n != 3) {
            printf("line %d: n=%d\n", line, n);
            panic("fscanf");
        }

        eibi_fgets(lp, sizeof(lbuf), fi);
        int sl = strlen(lbuf);
        lbuf[sl-1] = '\0';      // remove \n
        sl = strlen(lbuf);
        #define N_FIELDS 5
        char *fields[N_FIELDS+1], *rbuf;
        const char *fields_s[N_FIELDS] = {
            "dow", "country", "ident", "lang", "target" /*, "tx", "persist", "date-S", "date-E"*/
        };
        int n2 = _kiwi_split(lbuf, &rbuf, ";", fields, N_FIELDS);

        char *dow = fields[0];
        const char *country = fields[1];
        char *ident = fields[2];
        char *lang = fields[3];
        char *target = fields[4];
        int sl_iden = strlen(ident);
        
        if (strlen(country) > 3 || strlen(lang) > 3 || strlen(target) > 3) {
            printf("%.2f %04d-%04d n2=%d BAD STRLEN:", freq, begin, end, n2);
            for (i = 0; i < n2; i++) {
                printf(" %d:%s{%s}", i-1, fields_s[i], fields[i]);
            }
            printf("\n");
            panic("strlen");
        }

        // debug UTF-8 in ident field
        #if OPT_DEBUG_UTF_8
            char ident2[256];
            u1_t *ip, *ip2;
            bool dump_ident = false;
            for (ip = (u1_t *) ident, ip2 = (u1_t *) ident2; *ip != '\0'; ip++) {
                if (*ip < 0x80) {
                    *ip2++ = *ip;
                } else {
                    j = snprintf((char *) ip2, 256, "[%02x]", *ip);
                    ip2 += j;
                    dump_ident = true;
                }
            }
            *ip2 = '\0';
            if (dump_ident) printf("%d \"%s\" %lu \"%s\"\n", sl_iden, ident, strlen(ident2), ident2);
        #endif

        if (sl_iden > max_ident) max_ident = sl_iden;
    
    
        // find longest run on same freq (not counting identical adjacent idents)
        // also, set same_freq_same_ident for use later on
        same_freq_same_ident = false;
        if (freq == last_freq) {
            if (strcmp(ident, last_ident) != 0) {
                same_freq_diff_ident++;
            } else {
                same_freq_same_ident = true;
            }
        } else {
            if (same_freq_diff_ident > max_same_freq_diff_ident) {
                max_same_freq_diff_ident = same_freq_diff_ident;
                max_same_freq_diff_ident_freq = last_freq;
                printf("====> %d %.2f %s\n", max_same_freq_diff_ident, last_freq, last_ident);
            }
            same_freq_diff_ident = 1;
        }
        //printf("%d %.2f %s\n", same_freq_diff_ident, freq, ident);
        last_freq = freq;
        strcpy(last_ident, ident);
        
    #ifdef OPT_CHAR_FREQ
        for (i = 0; i < sl_iden; i++) {
            //printf("%7.1f %d %d %s\n", freq, sl_iden, (u1_t) ident[i], ident);
            char_freq[(u1_t) ident[i]]++;
        }
    #endif
        
        double f = freq;
        double r = f - floor(f);
        if (0) {
        //if (1) {
        //if (sl_iden > 24) {
        //if (n2 != 7) {
        //if (r >= 0.00001) {
        //if (begin >= end) {
            //printf("%7.1f %04d-%04d n2=%d \"%s\"", f, begin, end, n2, lbuf);
            printf("%8.2f %04d-%04d n2=%d", f, begin, end, n2);
            for (i = 0; i < n2; i++) {
                printf(" %d:%s{%s}", i-1, fields_s[i], fields[i]);
            }
            printf("\n");
        }
        
        int exp = 3;
        if (n != exp) {
            printf("line %d: expecting %d fields, got %d\n", line, exp, n);
            panic("fields");
        }


        // undo single-character country names
        assert(country[0] != '\0');
        int sl_cnt = strlen(country);
        if (sl_cnt == 1) {
            switch (country[0]) {
                case 'B': country = "BRA"; break;
                case 'D': country = "GER"; break;
                case 'E': country = "SPN"; break;
                case 'F': country = "FRA"; break;
                case 'G': country = "GBR"; break;
                case 'I': country = "ITL"; break;
                case 'J': country = "JPN"; break;
                case 'S': country = "SWE"; break;
            }
        }


        // type classification heuristic
        // FIXME: still some problems with out-of-band SWBC stations
        band_t *band;
        for (i = 0, band = band_f; i < ARRAY_LEN(band_f); i++, band++) {
            if (f >= band->lo && f <= band->hi) {
                break;
            }
        }
        if (i == ARRAY_LEN(band_f)) band = band_f;

        assert(ident[0] != '\0');
        int flags = 0, mode;
        
        // classify based on keywords in ident
        if (strstr(ident, "ALE")) { flags = DX_ALE | DX_HAS_EXT; mode = MODE_USB; } else
        if (strstr(ident, "RTTY") ||
            strstr(ident, "FSK")) { flags = DX_FSK | DX_HAS_EXT; mode = MODE_CW; } else
        // NB: detect "FSK" before "Navy"
        if (strstr(ident, "STANAG") ||
            strstr(ident, "Ny") ||
            strstr(ident, "Navy") ||
            strstr(ident, "SECURE") ||
            strstr(ident, "Air Force")) { flags = DX_MILCOM; mode = MODE_USB; } else
        if (strstr(ident, "Fax")) { flags = DX_FAX | DX_HAS_EXT; mode = MODE_USB; } else
        if (strstr(ident, "Spy") ||
            strstr(ident, "Numbers")) { flags = DX_SPY; mode = MODE_USB; } else
        if (strstr(ident, "Marine") || 
            strstr(ident, "Maritime") ||
            strstr(ident, "Coastguard") ||
            strstr(ident, "Harbor")) { flags = DX_MARINE; mode = MODE_USB; } else

        if (strstr(ident, "Volmet") ||
            strstr(ident, "Aero") ||
            strstr(ident, "Aeradio")) { flags = DX_AERO; mode = MODE_USB; } else
        
        // if " Radio" (NB leading space) then classify based on band
        if (strstr(ident, " Radio")) {
            if (band->b != B_BCAST) {
                if (band->b == B_MARINE) { flags = DX_MARINE; mode = MODE_USB; }
                else
                { flags = DX_AERO; mode = MODE_USB; }
                //if (strstr(ident, "TAH")) printf("%.2f %s flags=0x%x type=%s mode=%s\n", freq, ident, flags, types_s[DX_EiBi_FLAGS_TYPEIDX(flags)], mode_s[mode]);
            }
        }
        
        if (flags == 0) {
            // Use classifications from lang field if provided.
            // Done after above since e.g. "spy" entries can be flagged -CW or -TY 
            if (strcmp(lang, "-MX") == 0) { flags = DX_BCAST; mode = MODE_AM; } else
            if (strcmp(lang, "-HF") == 0) { flags = DX_HFDL | DX_HAS_EXT; mode = MODE_IQ; } else
            if (strcmp(lang, "-CW") == 0) { flags = DX_CW | DX_HAS_EXT; mode = MODE_CW; } else
            if (strcmp(lang, "-TY") == 0) { flags = DX_FSK | DX_HAS_EXT; mode = MODE_CW; } else
            if (strcmp(lang, "-EC") == 0) { flags = DX_BCAST; mode = MODE_AM; } else
            
            // different time stations have different passbands and only some
            // are supported by the timecode extension
            if (strcmp(lang, "-TS") == 0) {
                flags = DX_TIME;
                mode = MODE_AMN;
                if (freq <= 162) {
                    flags |= DX_HAS_EXT;
                    if (strstr(ident, "DCF77") || strstr(ident, "RBU")) { mode = MODE_CW; } else
                    mode = MODE_CWN;
                } else
                if (strstr(ident, "CHU")) { flags |= DX_HAS_EXT; } else
                if (strstr(ident, "RWM")) { mode = MODE_CWN; }
            } else

            // non CW markers appear inside bcast bands
            if (strstr(ident, "Marker")) { flags = DX_UTIL; mode = MODE_USB; } else

            // If it's in the bcast band, and not classified above, then consider it broadcast.
            if (band->b == B_BCAST || strstr(ident, "Voice of")) { flags = DX_BCAST; mode = MODE_AM; } else
        
            // If it's in the marine band, and not classified above, then consider it marine.
            if (band->b == B_MARINE) { flags = DX_MARINE; mode = MODE_USB; } else
        
            // Everything else is considered a "utility".
            // FIXME: this catches things it shouldn't
            { flags = DX_UTIL; mode = MODE_USB; }
        }

        if (!same_freq_same_ident) {
            types_n[DX_EiBi_FLAGS_TYPEIDX(flags)]++;
            types_total++;
        }


        // DOW decoding
        // FIXME: support some of the other lesser-used types?
        bool bcast = ((flags & DX_TYPE) == DX_BCAST);
        u2_t dow_mask = 0;
        if (dow[0] == '\0') {
            dow_mask = DX_DOW;
        } else {
            c = dow[0];
            bool isRange = (dow[2] == '-' && !isdigit(c));
            bool isPair = (dow[2] == ',');
            for (i = 0; (c = dow[i]) != '\0'; i++) {
                if (!isdigit(c)) break;
                dow_mask |= DX_MON >> (c - '1');
            }
            bool isMask = (i && c == '\0');
            if (!isMask) dow_mask = 0;
            if (isRange) {
                int lo = day(&dow[0]), hi = day(&dow[3]);
                //printf("lo=%d hi=%d ", lo, hi);
                for (i = lo; true;) {
                    dow_mask |= DX_MON >> i;
                    if (i == hi) break;
                    i++;
                    if (i >= 7) i = 0;
                }
                //printf("0x%x ", dow_mask);
            }
            if (isPair) {
                dow_mask |= DX_MON >> day(&dow[0]);
                dow_mask |= DX_MON >> day(&dow[3]);
            }
            if (dow_mask == 0) dow_mask = DX_DOW;
        }
        
    #ifdef OPT_DOW
        int trig = 0;
        if (1 && isMask) {      // e.g. 1245
            printf("%s%8s ", bcast? CYAN:RED, dow);
            trig = 1;
        }
        if (1 && isRange) {     // e.g. Mo-Fr
            printf("%s%8s ", bcast? YELLOW:RED, dow);
            trig = 1;
        }
        if (1 && isPair) {      // e.g. Mo,Fr
            printf("%s%8s ", bcast? GREEN:RED, dow);
            trig = 1;
        }
        if (trig) {
            const char *dow_mask_s = "1234567";
            for (i = 0; i < 7; i++)
                printf("%c", (dow_mask & (DX_MON >> i))? dow_mask_s[i] : '_');
            printf("%s", NORM);
            if (!bcast) printf(" %.2f %s", freq, ident);
            printf(" ");
        }
    #endif


        // build indexed table of station idents
        int ident_i;
        ident_dict_t *dp;
        for (ident_i = 0, dp = ident_dict; ident_i < ident_dict_n; ident_i++, dp++) {
            if (sl_iden == dp->sl && strcmp(dp->ident, ident) == 0) {
                dp->ct++;
                //printf("MATCH #%d ct=%d sl=%d %.2f %s\n", ident_i, dp->ct, sl_iden, freq, ident);
                break;
            }
        }
        if (ident_i == ident_dict_n) {
            //printf("NEW #%d ct=%d sl=%d %.2f %s\n", ident_i, dp->ct, sl_iden, freq, ident);
            strcpy(dp->ident, ident);
            dp->sl = sl_iden;
            dp->ct++;
            ident_dict_n++;
        }
    
    #ifdef OPT_NOTES
        // build indexed table of station notes
        int notes_i;
        notes_dict_t *dp2;
        for (notes_i = 0, dp2 = notes_dict; notes_i < notes_dict_n; notes_i++, dp2++) {
            //if (begin == dp2->begin && end == dp2->end) {
            if (sl_cnt == dp2->sl && strcmp(dp2->notes, country) == 0) {
                dp2->ct++;
                //printf("MATCH #%d ct=%d %.2f %s\n", notes_i, dp2->ct, freq, notes);
                break;
            }
        }
        if (notes_i == notes_dict_n) {
            //printf("NEW #%d ct=%d %.2f %s\n", notes_i, dp2->ct, country);
            strcpy(dp2->notes, country);
            dp2->sl = sl_cnt;
            dp2->ct++;
            notes_dict_n++;
        }
    #endif
        
        // output data
        if (mode >= 16) panic("mode >= 16 not currently supported by code");
        flags |= DX_ENCODE_MODE(mode);
        fprintf(fo, "  { %8.2f, %4d, %4d, %3s|0x%04x|%4s|%3s, %4d, \"%3s\", %-6s %-5s },  // %7s %s\n",
            freq, begin, end,
            (flags & DX_HAS_EXT)? "EXT":"_", dow_mask & DX_DOW,
            types2_s[DX_EiBi_FLAGS_TYPEIDX(flags)], mode_s[flags & DX_MODE],
            ident_i, country,
            qs(0, "\"%s\",", lang), qs(1, "\"%s\"", target),
            qs(2, "-%s", (char *) types_s[DX_EiBi_FLAGS_TYPEIDX(flags)]), ident);
        
        #define OUTPUT_HFDL
        #ifdef OUTPUT_HFDL
            if ((flags & DX_TYPE) == DX_HFDL) {
                char *id = strdup(ident);
                char *tp = strstr(id, " HFDL");
                *tp = '\0';
                hfdl_loc_t *h = hfdl_loc;
                for (; h < ARRAY_END(hfdl_loc); h++) {
                    if (strstr(id, h->loc))
                        break;
                }
                if (h == ARRAY_END(hfdl_loc)) panic("hfdl_loc");
                printf("[%.2f, \"USB\", \"HFDL %s\", \"%s\", {\"T1\":1, \"p\":\"hfdl,*\"}],\n",
                    freq, h->country, id);
            }
        #endif

        kiwi_ifree(rbuf, "eibi");
        line++;
        entries++;
    }
    
    fprintf(fo, "  {}  // %d entries\n};\n", entries);
    printf("\n\n");
    

    fprintf(fo, "\nconst char *eibi_ident[] = {\n");
    ident_dict_t *dp;
    int ident_size = 0;
    for (i = 0, dp = ident_dict; i < ident_dict_n; i++, dp++) {
        //printf("%4d: #%4d L%2d %s\n", i, dp->ct, dp->sl, dp->ident);
        ident_size += dp->sl + SPACE_FOR_NULL;
        fprintf(fo, "  /* %4d */ \"%s\",\n", i, dp->ident);
    }
    printf("ident_dict_n=%d ident_size=%d\n", ident_dict_n, ident_size);
    fprintf(fo, "};\n");
    

#ifdef OPT_NOTES
    fprintf(fo, "\nconst char *eibi_notes[] = {\n");
    notes_dict_t *dp2;
    int notes_size = 0;
    for (i = 0, dp2 = notes_dict; i < notes_dict_n; i++, dp2++) {
        //printf("%4d: #%4d L%2d %s\n", i, dp2->ct, dp2->sl, dp2->notes);
        fprintf(fo, "  /* %4d */ \"%s\",\n", i, dp2->notes);
        notes_size += dp2->sl + SPACE_FOR_NULL;
    }
    printf("notes_dict_n=%d notes_size=%d\n\n", notes_dict_n, notes_size);
    fprintf(fo, "};\n
#endif

    
    // counts that are added to service checkboxes in control panel
    printf("types_total=%d\n", types_total);
    fprintf(fo, "\nconst int eibi_counts[DX_N_EiBi] = {  // %d total\n", types_total);
    for (i = 0; i < DX_N_EiBi; i++) {
        fprintf(fo, "  %5d%s  // %s\n", types_n[i], (i == DX_N_EiBi-1)? " " : ",", types_s[i]);
        printf("%5d %s\n", types_n[i], types_s[i]);
    }
    fprintf(fo, "};\n");


#ifdef OPT_CHAR_FREQ
    for (i = 0; i < 16; i++) {
        printf("%02x: ", i*16);
        for (j = 0; j < 16; j++) {
            int n = i*16+j;
            int cf = char_freq[n];
            printf("%5d %c    ", cf, cf? n : ' ');
        }
        printf("\n");
    }
#endif


    printf("entries=%d max_ident=%d max_same_freq_diff_ident=%d(%.2f)\n",
        entries, max_ident, max_same_freq_diff_ident, max_same_freq_diff_ident_freq);
    
    fclose(fi);
    fclose(fo);
}
