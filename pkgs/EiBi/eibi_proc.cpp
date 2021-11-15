
#include "types.h"
#include "kiwi_assert.h"
#include "printf.h"
#include "mem.h"
#include "str.h"
#include "dx.h"

#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

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
int kiwi_split(char *ocp, char **mbuf, const char *delims, char *argv[], int nargs)
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

typedef struct {
    float lo, hi;
} band_t;

band_t bcast_f[] = {
    { 153, 280 },       // LW
    { 525, 1710 },      // AM BCB
    
    // from dist.config.js, expanded to fit actual EiBi data (bcast vs aero/utility)
    { 2300, 2495 },
    { 3185, 3400 },     // 3200 => 3185
    { 3900, 4000 },
    { 4750, 5060 },
    { 5850, 6210 },     // 5900,6200 => 5850,6210
    { 7200, 7800 },     // 7450 => 7800
    { 9300, 9900 },     // 9400 => 9300
    { 11550, 12150 },   // 11600,12100 => 11550,12150
    { 13570, 13870 },
    { 15000, 15800 },   // 15100 => 15000
    { 17480, 17900 },
    { 18900, 19020 },
    { 21450, 21850 },
    { 25600, 26100 }
};

band_t aero_f[] = {
    { 17900, 18020 },
};

char *qs(int n, const char *fmt, char *s)
{
    static char b[4][8];
    sprintf(b[n], fmt, s);
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
#define OPT_LONGEST_RUN
//#define OPT_DOW
//#define OPT_CHAR_FREQ

int main(int argc, char *argv[])
{
    int i, j, n;
    char c, *s;
    int max_ident = 0;
    
    FILE *fi, *fo;
    fi = fopen("sked-current.csv", "rb");
    if (!fi) sys_panic("fi");
    fo = fopen("../../init/EiBi.h", "wb");
    if (!fo) sys_panic("fo");
    
    char lbuf[256];
    u1_t *lp = (u1_t *) lbuf;
    int line = 2;
    eibi_fgets(lp, sizeof(lbuf), fi);     // skip first line
    
    int same_freq_diff_ident = 0, max_same_freq_diff_ident = 0;
    double last_freq, max_same_freq_diff_ident_freq;
    char last_ident[256];

    int entries = 0;
    fprintf(fo, "dx_t eibi_db[] = {\n");
    
    while (1) {
    
        double freq = 0;
        int begin = 9999, end = 9999;

        n = fscanf(fi, "%lf;%d-%d;", &freq, &begin, &end);
        if (n == EOF) break;

        eibi_fgets(lp, sizeof(lbuf), fi);
        int sl = strlen(lbuf);
        lbuf[sl-1] = '\0';      // remove \n
        sl = strlen(lbuf);
        #define N_FIELDS 8
        char *fields[N_FIELDS], *rbuf;
        int n2 = kiwi_split(lbuf, &rbuf, ";", fields, N_FIELDS-1);

        char *dow = fields[0];
        const char *country = fields[1];
        char *ident = fields[2];
        char *lang = fields[3];
        char *target = fields[4];
        int sl_iden = strlen(ident);

        // debug UTF-8 in ident field
        #if OPT_DEBUG_UTF_8
            char ident2[256];
            u1_t *ip, *ip2;
            bool dump_ident = false;
            for (ip = (u1_t *) ident, ip2 = (u1_t *) ident2; *ip != '\0'; ip++) {
                if (*ip < 0x80) {
                    *ip2++ = *ip;
                } else {
                    j = sprintf((char *) ip2, "[%02x]", *ip);
                    ip2 += j;
                    dump_ident = true;
                }
            }
            *ip2 = '\0';
            if (dump_ident) printf("%d \"%s\" %lu \"%s\"\n", sl_iden, ident, strlen(ident2), ident2);
        #endif

        if (sl_iden > max_ident) max_ident = sl_iden;
    
    
    #ifdef OPT_LONGEST_RUN
        // find longest run on same freq (not counting identical adjacent idents)
        if (freq == last_freq) {
            if (strcmp(ident, last_ident) != 0) {
                same_freq_diff_ident++;
            } else {
                ;
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
    #endif
        
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
            printf("%7.1f %04d-%04d n2=%d \"%s\"", f, begin, end, n2, lbuf);
            for (i = 0; i < n2; i++) {
                printf(" %d{%s}", i, fields[i]);
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
        bool in_bcast_band = false;
        for (i = 0; i < ARRAY_LEN(bcast_f); i++) {
            if (f >= bcast_f[i].lo && f <= bcast_f[i].hi) {
                in_bcast_band = true;
                break;
            }
        }

        /*
        bool in_aero_band = false;
        for (i = 0; i < ARRAY_LEN(aero_f); i++) {
            if (f >= aero_f[i].lo && f <= aero_f[i].hi) {
                in_aero_band = true;
                break;
            }
        }
        */

        assert(ident[0] != '\0');
        int type = 0;
        
        // classify based on keywords in ident
        if (strstr(ident, "ALE"))     type = DX_ALE | DX_HAS_EXT | MODE_USB; else
        if (strstr(ident, "STANAG") || strstr(ident, "Ny") || strstr(ident, "SECURE") ||
            strstr(ident, "Air Force")) type = DX_MILCOM | MODE_USB; else
        if (strstr(ident, "RTTY"))    type = DX_FSK | DX_HAS_EXT | MODE_CW; else
        if (strstr(ident, "Fax"))     type = DX_FAX | DX_HAS_EXT | MODE_USB; else
        if (strstr(ident, "Spy") || strstr(ident, "Numbers")) type = DX_SPY | MODE_USB; else
        if (strstr(ident, "Marine") || strstr(ident, "Maritime")) type = DX_MARINE | MODE_USB; else

        if (strstr(ident, "Volmet") || strstr(ident, "Aero") ||
            (!in_bcast_band && strstr(ident, " Radio"))) type = DX_AERO | MODE_USB;
        
        if (type == 0) {
            // Use classifications from lang field if provided.
            // Done after above since e.g. "spy" entries can be flagged -CW or -TY 
            if (strcmp(lang, "-MX") == 0) type = DX_BCAST | MODE_AM; else
            if (strcmp(lang, "-HF") == 0) type = DX_HFDL | DX_HAS_EXT | MODE_IQ; else
            if (strcmp(lang, "-CW") == 0) type = DX_CW | DX_HAS_EXT | MODE_CW; else
            if (strcmp(lang, "-TY") == 0) type = DX_FSK | DX_HAS_EXT | MODE_CW; else
            if (strcmp(lang, "-TS") == 0) type = DX_TIME | DX_HAS_EXT | MODE_AMN; else
            if (strcmp(lang, "-EC") == 0) type = DX_BCAST | MODE_AM; else
            
            // non CW markers appear inside bcast bands
            if (strstr(ident, "Marker")) type = DX_UTIL | MODE_USB; else

            // If it's in the bcast band, and not classified above, then consider it broadcast.
            if (in_bcast_band || strstr(ident, "Voice of")) { type = DX_BCAST | MODE_AM; } else
        
            // Everything else is considered a "utility".
            // FIXME: this catches things it shouldn't
            { type = DX_UTIL | MODE_USB; }
        }

        types_n[DX_T2I(type)]++;


        // DOW decoding
        // FIXME: support some of the other lesser-used types?
        bool bcast = ((type & DX_TYPE) == DX_BCAST);
        u2_t dow_mask = 0;
        assert(dow[0] != '\0');
        c = dow[0];
        bool isRange = (dow[2] == '-');
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
        fprintf(fo, "  { %8.2f, %4d, %4d, 0x%05x, %4d, \"%3s\", %-6s %-5s },  // %7s %s\n",
            freq, begin, end, type | dow_mask, ident_i, country,
            qs(0, "\"%s\",", lang), qs(1, "\"%s\"", target),
            qs(2, "-%s", (char *) types_s[DX_T2I(type)]), ident);

        kiwi_ifree(rbuf);
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
    fprintf(fo, "\nconst int eibi_counts[] = {\n");
    const char **sp;
    for (i = 0, sp = &types_s[0]; *sp != NULL; i++, sp++) {
        fprintf(fo, "  %5d,  // %s\n", types_n[i], *sp);
        printf("%5d %s\n", types_n[i], *sp);
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
