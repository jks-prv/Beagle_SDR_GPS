
/*
	_Extremely_ quick & dirty assembler for the language used in Andrew Holme's FORTH-like FPGA processor
	for his homemade GPS project: holmea.demon.co.uk/GPS
	
	We made slight changes to some of the pre-processing directives to make our parsing easier.
	
	There is very little bounds or error checking here and pathological input will cause the program to crash.
	The multi-pass, minimal techniques used are fine for the small programs that are assembled, but this
	assembler is completely unsuitable in its current form for any larger use.
	
	Look at the project assembler source code for examples of all the supported constructs.
	
	runtime arguments:
		-c	compare generated binary to that from "cmp.h" for debugging
		-d	enable debugging printfs
		-b	print generated binary
		-t	use input and output files "test.asm" & "test.h" instead of defaults
	
	issues:
		expression evaluation is strictly left-to-right
		really need to improve the error checking
		need runtime args instead of hardcoded stuff
*/

// Copyright (c) 2013-2019 John Seamons, ZL4VO/KF6VO

#include "asm.h"

int show_bin, gen=1, only_gen_other=0, write_coe;

#define LBUF 1024
char linebuf[LBUF];

#define	TBUF	64*1024
tokens_t tokens[TBUF], *pass1, *pass2, *pass3, *pass4;
tokens_t *tp_start, *tp_end;

typedef struct {
	const char *str;
	token_type_e ttype;
	int val;
	union {
	    u4_t flags;     // TF_* flags
	    u4_t mask;      // opcode mask for TT_OPC entries
	};
	u4_t stats;
} dict_t;

#include <cpu.h>

// dictionary of reserved symbols and tokens
dict_t dict[] = {
	{ "DEF",		TT_PRE,		PP_DEF },
	{ "DEFc",		TT_PRE,		PP_DEF,			TF_CFG_H },		// gens RX_CFG in kiwi.cfg.vh
	{ "DEFh",		TT_PRE,		PP_DEF,			TF_DOT_H },		// gens a '#define' in .h and '`define' in .vh
	{ "DEFp",		TT_PRE,		PP_DEF,			TF_DOT_VP },	// gens a 'localparam' in .vh and '#define' in .h
	{ "DEFb",		TT_PRE,		PP_DEF,			TF_DOT_VB },	// gens a 'localparam' of bit position in .vh
	{ "MACRO",		TT_PRE,		PP_MACRO },
	{ "ENDM",		TT_PRE,		PP_ENDM },
	{ "REPEAT",		TT_PRE,		PP_REPEAT },
	{ "ENDR",		TT_PRE,		PP_ENDR },
	{ "STRUCT",		TT_PRE,		PP_STRUCT },
	{ "ENDS",		TT_PRE,		PP_ENDS },
	{ "FACTOR",		TT_PRE,		PP_FACTOR },
	{ "ENDF",		TT_PRE,		PP_ENDF },
	{ "FCALL",		TT_PRE,		PP_FCALL },
	{ "#if",		TT_PRE,		PP_IF },        // supports '#if NUM|SYM (implied != 0)' and '#if SYM|NUM OPR SYM|NUM'
	{ "#else",		TT_PRE,		PP_ELSE },
	{ "#endif",		TT_PRE,		PP_ENDIF },

	{ "push",		TT_OPC,		OC_PUSH,            OCM_CONST },
	{ "nop",		TT_OPC,		OC_NOP,             OCM_NONE },
	{ "ret",		TT_OPC,		OC_NOP | OPT_RET,   OCM_NONE },
	{ "dup",		TT_OPC,		OC_DUP,             OPT_RET },
	{ "swap",		TT_OPC,		OC_SWAP,            OPT_RET },
	{ "swap16",		TT_OPC,		OC_SWAP16,          OPT_RET },
	{ "over",		TT_OPC,		OC_OVER,            OPT_RET },
	{ "pop",		TT_OPC,		OC_POP,             OPT_RET },
	{ "drop",		TT_OPC,		OC_POP,             OPT_RET },
	{ "rot",		TT_OPC,		OC_ROT,             OPT_RET },
	{ "addi",		TT_OPC,		OC_ADDI,            OPT_RET },
	{ "add",		TT_OPC,		OC_ADD,             OPT_RET },
	{ "add.cin",    TT_STATS,   OC_ADD | OPT_CIN,   OPT_RET },     // only for benefit of stats
	{ "sub",		TT_OPC,		OC_SUB,             OPT_RET },
	{ "mult",		TT_OPC,		OC_MULT,            OPT_RET },
	{ "mult20",		TT_OPC,		OC_MULT20,          OPT_RET },
	{ "and",		TT_OPC,		OC_AND,             OPT_RET },
	{ "or",			TT_OPC,		OC_OR,              OPT_RET },
	{ "xor",		TT_OPC,		OC_XOR,             OPT_RET },
	{ "not",		TT_OPC,		OC_NOT,             OPT_RET },
	{ "shl64",		TT_OPC,		OC_SHL64,           OPT_RET },
	{ "shl",		TT_OPC,		OC_SHL,             OPT_RET },
	{ "rol",		TT_OPC,		OC_ROL,             OPT_RET },
	{ "shr",		TT_OPC,		OC_SHR,             OPT_RET },
	{ "ror",		TT_OPC,		OC_ROR,             OPT_RET },
	{ "usr",		TT_OPC,		OC_USR,             OPT_RET },
	{ "rdBit0",		TT_OPC,		OC_RDBIT0,          OPT_RET },
	{ "rdBit1",		TT_OPC,		OC_RDBIT1,          OPT_RET },
	{ "rdBit2",		TT_OPC,		OC_RDBIT2,          OPT_RET },
	{ "fetch16",	TT_OPC,		OC_FETCH16,         OPT_RET },
	{ "store16",	TT_OPC,		OC_STORE16,         OPT_RET },
	{ "stk_rd",	    TT_OPC,		OC_STK_RD,          OPT_RET },
	{ "stk_wr",	    TT_OPC,		OC_STK_WR,          OPT_RET },
	{ "sp_rp",      TT_OPC,		OC_SP_RP,           OPT_RET },

	{ "r",			TT_OPC,		OC_R,               OCM_NONE },
	{ "r_from",		TT_OPC,		OC_R_FROM,          OCM_NONE },
	{ "to_r",		TT_OPC,		OC_TO_R,            OCM_NONE },
	{ "call",		TT_OPC,		OC_CALL,            OCM_ADDR },
	{ "br",			TT_OPC,		OC_BR,              OCM_ADDR },
	{ "brZ",		TT_OPC,		OC_BRZ,             OCM_ADDR },
	{ "brNZ",		TT_OPC,		OC_BRNZ,            OCM_ADDR },
	{ "loop",		TT_OPC,		OC_LOOP,            OCM_ADDR },
	{ "loop2",		TT_OPC,		OC_LOOP2,           OCM_ADDR },
	{ "to_loop",    TT_OPC,		OC_TO_LOOP,         OPT_RET },
	{ "to_loop2",   TT_OPC,		OC_TO_LOOP2,        OPT_RET },
	{ "loop_from",  TT_OPC,		OC_LOOP_FROM,       OPT_RET },
	{ "loop2_from", TT_OPC,		OC_LOOP2_FROM,      OPT_RET },
	{ "rdReg",		TT_OPC,		OC_RDREG,           OCM_IO },
	{ "rdReg2",		TT_OPC,		OC_RDREG2,          OCM_IO },
	{ "wrReg",		TT_OPC,		OC_WRREG,           OCM_IO },
	{ "wrReg2",		TT_OPC,		OC_WRREG2,          OCM_IO },
	{ "wrEvt",		TT_OPC,		OC_WREVT,           OCM_IO },
	{ "wrEvt2",		TT_OPC,		OC_WREVT2,          OCM_IO },
	
	{ "u8",			TT_DATA,	1 },
	{ "u16",		TT_DATA,	2 },
	{ "u32",		TT_DATA,	4 },
	{ "u64",		TT_DATA,	8 },
	
	{ "++",			TT_OPR,		OPR_INC,	TF_1OPR },
	{ "+",			TT_OPR,		OPR_ADD,	TF_2OPR },
	{ "--",			TT_OPR,		OPR_DEC,	TF_1OPR },
	{ "-",			TT_OPR,		OPR_SUB,	TF_2OPR },
	{ "*",			TT_OPR,		OPR_MUL,	TF_2OPR },
	{ "/",			TT_OPR,		OPR_DIV,	TF_2OPR },
	{ "<<",			TT_OPR,		OPR_SHL,	TF_2OPR },
	{ ">>",			TT_OPR,		OPR_SHR,	TF_2OPR },
	{ "&&",			TT_OPR,		OPR_LAND,	TF_2OPR },
	{ "&",			TT_OPR,		OPR_AND,	TF_2OPR },
	{ "||",			TT_OPR,		OPR_LOR,    TF_2OPR },
	{ "|",			TT_OPR,		OPR_OR,		TF_2OPR },
	{ "==",			TT_OPR,		OPR_EQ,	    TF_2OPR },
	{ "!=",			TT_OPR,		OPR_NEQ,	TF_2OPR },
	{ "~",			TT_OPR,		OPR_NOT,	TF_1OPR },      // NB: currently done postfix
	{ "max",        TT_OPR,		OPR_MAX,    TF_2OPR },
	{ "min",        TT_OPR,		OPR_MIN,    TF_2OPR },
	{ "sizeof",		TT_OPR,		OPR_SIZEOF },
	{ "#",			TT_OPR,		OPR_CONCAT },
	{ ":",			TT_OPR,		OPR_LABEL },
	{ "(",			TT_OPR,		OPR_OPEN },
	{ ")",			TT_OPR,		OPR_CLOSE },
	
	{ "<iter>",		TT_ITER,	0 },
	
	{ 0,			TT_EOL,     0 }
};

#define SBUF 64
char sym[SBUF];

typedef struct {
	int addr, code;
} init_code_t;

init_code_t init_code[] = {
    #include "cmp.h"
	-1, 0
};

#define	FN_PREFIX	"kiwi"

char ifiles_list[NIFILES_LIST][32];
char *last_file;

#define NIFILES_NEST 3
char ifiles[NIFILES_NEST][32];
int lineno[NIFILES_NEST];

int main(int argc, char *argv[])
{
	int i, val;
	
	char *odir = (char *) ".";
	char *other_dir = NULL;
	char *ifs = (char *) FN_PREFIX ".asm";                  // source input
	char *ofs;                                              

	int ifn, ifl;
	FILE *ifp[NIFILES_NEST], *ofp, *efp;
	
	int bfd;
	char *lp = linebuf, *cp, *scp, *np, *sp;
	dict_t *dp;
	token_type_e ttype;
	tokens_t *tp = tokens, *tpp, *ltp = tokens, *ep0, *ep1, *ep2, *ep3, *ep4, *t, *tt, *to;
	preproc_t *pp = preproc, *p;
	strs_t *st;
	int compare_code=0, stats=0;
	char fullpath[256], basename[256];
	bool test = false;
	
	for (i=1; i < argc; i++)
        if (argv[i][0] == '-') switch (argv[i][1]) {
            case 't': ifs = (char *) "test.asm"; ofs = (char *) "test.aout.h"; bfs = (char *) "test.aout"; break;
            case 'c': compare_code=1; printf("compare mode\n"); break;
            case 'd': debug=1; gen=0; break;
            case 'b': show_bin=1; gen=0; break;
            case 'g': only_gen_other=1; break;
            case 'n': gen=0; break;
            case 's': stats=1; break;
            case 'o': i++; odir = argv[i]; break;
            case 'x': i++; other_dir = argv[i]; break;
        }
	
	if (!test) {
	    asprintf(&bfs, "%s/%s.aout", odir, FN_PREFIX);      // loaded into FPGA via SPI
	    asprintf(&ofs, "%s/ecode.aout.h", odir);            // included by simulator
	}
	
	ifn = ifl = 0; fn = ifs;
	strcpy(ifiles[ifn], ifs);
	strcpy(ifiles_list[ifl], ifs);
	if ((ifp[ifn] = fopen(ifs, "r")) == NULL) sys_panic("fopen ifs");
	if ((ofp = fopen(ofs, "w")) == NULL) sys_panic("fopen ofs");
	if ((bfd = open(bfs, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) sys_panic("open bfs");
	printf("assembling %s to: %s, %s\n", ifs, bfs, ofs);

	if (write_coe) {
        efs = (char *) "../verilog/" FN_PREFIX ".coe";    // .coe file to init BRAMs (optional during FPGA development)
		if ((efp = fopen(efs, "w")) == NULL) sys_panic("fopen efs");
		fprintf(efp, "; DEPTH = 2048\n; WIDTH = 16\nmemory_initialization_radix=16;\nmemory_initialization_vector=");
	}
	
	// pass 0: tokenize
	bool need_other_config = true;
	strcpy(basename, "./other.config");
	
	while (ifn >= 0) {

		while (fgets(lp, LBUF, ifp[ifn])) {
			lineno[ifn]++; curline = lineno[ifn];
	
			if (debug) printf("%s:%03d %s", fn, curline, lp);
			cp = lp;
			
			if (need_other_config || (sscanf(cp, "#include_other %s", basename) == 1)) {
			    if (!need_other_config && other_dir == NULL) {
			        printf("#include_other file: (ignored) %s\n", basename);
			        continue;
			    }
				ifn++; lineno[ifn] = 0; ifl++;
				if (ifn >= NIFILES_NEST) panic("too many nested include files");
				fn = ifiles[ifn]; strcpy(fn, basename); strcpy(ifiles_list[ifl], basename);
                snprintf(fullpath, sizeof(fullpath), "%s%s", other_dir? other_dir : "", fn);
                printf("#include file: %s\n", fullpath);
                if ((ifp[ifn] = fopen(fullpath, "r")) == NULL) {
                    sys_panic("fopen include file");
                }

				if (str_ends_with(fn, ".config")) {
                    tp->ttype = TT_FILE;
                    tp->str = ifiles_list[ifl];
                    tp++;
                }
                
                need_other_config = false;
                continue;
			}
	
			if (sscanf(cp, "#include %s", basename) == 1) {
				ifn++; lineno[ifn] = 0; ifl++;
				if (ifn >= NIFILES_NEST) panic("too many nested include files");
				fn = ifiles[ifn]; strcpy(fn, basename); strcpy(ifiles_list[ifl], basename);
				printf("#include file: %s\n", fn);
				if ((ifp[ifn] = fopen(fn, "r")) == NULL) {
				    sys_panic("fopen include file");
				}

				if (str_ends_with(fn, ".config")) {
                    tp->ttype = TT_FILE;
                    tp->str = ifiles_list[ifl];
                    tp++;
                }
                continue;
			}
	
			while (*cp) {
				np = cp+1;
				sp = sym;
				if (isspace(*cp)) { cp++; continue; }
				if (*cp==';' || (*cp=='/' && *np=='/')) break;	// comment
				
				if (isdigit(*cp) || (*cp == '-' && isdigit(*np))) {
					tp->ttype = TT_NUM;
					if (*np=='x') tp->flags |= TF_HEX;
					
					int num = strtol(cp, &cp, 0);
					if (*cp == '\'') {
						tp->flags |= TF_FIELD; cp++;
						syntax(*cp == 'd', tp, "expecting \'d"); cp++;
						tp->width = num;
						int max = (1 << num) - 1;
						num = strtol(cp, &cp, 0);
						syntax(num <= max, tp, "value greater than field width");
					}
					tp->num = num; tp++; continue;
				}
				
				// symbol
				scp = cp;
				if (isalpha(*cp) || *cp=='_' || *cp=='$') {
					*sp++ = *cp++;
					while (isalpha(*cp) || isdigit(*cp) || *cp=='_') *sp++ = *cp++; *sp = 0;
	
					for (dp=dict; dp->str; dp++) {		// check for reserved names
						if (strcmp(dp->str, sym) == 0) {
							if (*cp==':') panic("resv name as label");
							tp->ttype = dp->ttype;
							tp->str = (char *) dp->str;
							tp->num = dp->val;
						    tp->flags = (dp->ttype != TT_OPC)? dp->flags : 0;
							if (strncmp(cp, ".r", 2) == 0) tp->flags |= TF_RET, cp+=2;
							if (strncmp(cp, ".cin", 4) == 0) tp->flags |= TF_CIN, cp+=4;
							break;
						}
					}
	
					if (!dp->str) {
						if (*cp==':') ttype = TT_LABEL, cp++; else ttype = TT_SYM;
						tp->ttype = ttype; string_enter(sym, &(tp->str), (ttype==TT_LABEL)? SF_LABEL:0);
						if (strncmp(cp, ".r", 2) == 0) tp->flags |= TF_RET, cp+=2;
						if (strncmp(cp, ".cin", 4) == 0) tp->flags |= TF_CIN, cp+=4;
					}
	
					tp++; continue;
				}
				cp = scp;
				
				// non-symbol token
				if (*cp=='(' || *cp==')') { *sp++ = *cp++; *sp = 0; }	// just take single char
				else
					while (!isspace(*cp) && *cp!=')') *sp++ = *cp++; *sp = 0;
				for (dp=dict; dp->str; dp++) {		// check for reserved names
					if (strcmp(dp->str, sym) == 0) {
						tp->ttype = dp->ttype;
						tp->str = (char *) dp->str;
						tp->num = dp->val;
						tp->flags = (dp->ttype != TT_OPC)? dp->flags : 0;
						tp++;
						break;
					}
				}
				if (dp->str) continue;
				
				errmsg("unknown symbol: ");
				printf("\"%s\" followed by 0x%02x <%c>\n", sym, *cp, *cp);
				panic("errors detected");
				cp++;
			}
			
			if ((tp-1)->ttype != TT_EOL) {
				tp->ttype = TT_EOL; tp->num = curline+1; tp->ifl = ifl; tp++;
			} else {
				(tp-1)->num++;
			}
		
			if (debug) {
			    int seen = 0;
			    for (t=ltp; t != tp; t++) {
			        if (t->ttype != TT_EOL) {
			            token_dump(t);
			            seen = 1;
			        }
			    }
			    if (seen) printf("\n");
			}
			ltp = tp;
		}
		
		fclose(ifp[ifn]);
		ifn--;
		fn = ifiles[ifn];
		
		ifl++;
		strcpy(ifiles_list[ifl], fn);
	}

	fn = ifiles[0];
	ep0 = tp; pass1 = ep0;
	if (debug) printf("\ntokenize pass 0: %d strings %ld tokens\n\n", num_strings(), (long) (ep0-tokens));


	// pass 1: MACRO
	// NB done in-place after making a copy since pass 0 has internal references
	for (tp=tokens, to=ep0; tp != ep0;) *to++ = *tp++;
	curline=1;
	
	for (; tp != to; tp++) {

		if (tp->ttype == TT_EOL) {
			curline = tp->num;
		}

		// perform MACRO expansion
		if (tp->ttype == TT_SYM) {
			if (exp_macro(&tp, &to)) continue;
		}

		// define new MACRO
		if (tp->ttype == TT_PRE && tp->num == PP_MACRO) {
			tp++; syntax(tp->ttype == TT_SYM, tp, "expected MACRO name"); t = tp; tp++; pp->args = tp;
			for (i=0; tp->ttype != TT_EOL; i++) {
				syntax(tp->ttype == TT_SYM, tp, "expected MACRO arg"); tp++;
			}
			tp++; pp->nargs = i; pp->body = tp;
			for (i=0; !(tp->ttype == TT_PRE && tp->num == PP_ENDM); i++) {
				if (!exp_macro(&tp, &to)) tp++;		// expand MACRO within MACRO definition
			}
			pp->nbody = i; pp->str = t->str; pp->ptype = PT_MACRO;
			if (debug) printf("new MACRO %s nargs %d body %d\n", pp->str, pp->nargs, pp->nbody);
			pp++; tp+=1; continue;	// remove extra \n
		}
		
		// default: token kept
	}	

	ep1 = to; pass2 = ep1;
	if (debug) dump_tokens("pass1", pass1, ep1);


	// pass 2: IF, DEF, STRUCT/MEMBER
	#define NIFDEF_NEST 8
	static int keep[NIFDEF_NEST] = {1};
	static int ifdef_lvl=0;
	curline=1;

    tp_start = pass1; tp_end = pass2;
	for (tp=pass1, to=pass2; tp != ep1;) {
		tpp = tp+1;

		if (tp->ttype == TT_EOL) {
			curline = tp->num;
		}

		// remove #if / #endif
		if (tp->ttype == TT_PRE && tp->num == PP_IF) {
			ifdef_lvl++;
			if (ifdef_lvl >= NIFDEF_NEST) panic("too many nested #if");

			// skip in this level if skipping in previous level
			if (!keep[ifdef_lvl-1]) {
				keep[ifdef_lvl] = 0;
			} else {
			    tp = cond(tp+1, &ep1, &val);
				keep[ifdef_lvl] = (val != 0);
			}
			if (debug) printf("IF %s:%03d %s\n", fn, curline, keep[ifdef_lvl]? "YES":"NO");
		}
		
		if (tp->ttype == TT_PRE && tp->num == PP_ELSE) {
		
			// only act if not skipping in previous level
			if (keep[ifdef_lvl-1]) keep[ifdef_lvl] ^= 1;
			if (debug) printf("ELSE %s:%03d %s\n", fn, curline, keep[ifdef_lvl]? "YES":"NO");
			tp += 2; continue;
		}

		if (tp->ttype == TT_PRE && tp->num == PP_ENDIF) {
			ifdef_lvl--;
			if (ifdef_lvl < 0) panic("unbalanced #if/#endif");
			if (debug) printf("ENDIF\n");
			tp += 2; continue;
		}

		if (!keep[ifdef_lvl]) {
			tp++; continue;
		}

		// perform expansion
		if (def(tp, &ep1)) ; else

		// process STRUCT/MEMBER
		if (tp->ttype == TT_SYM) {
			if ((p = pre(tp->str, PT_NONE))) {
				if (p->ptype == PT_MEMBER) {
					if (debug) printf("MEMBER \"%s\" offset %d\n", tp->str, p->offset);
					to->ttype = TT_NUM; to->num = p->offset; to++;
				} else
				if (p->ptype == PT_STRUCT) {
					if (debug) printf("STRUCT \"%s\"\n", tp->str);
					to->ttype = TT_STRUCT; to->str = tp->str; to->num = p->size; to++;
				} else {
					syntax(0, tp, "symbol or MACRO undefined");
				}
				tp++;
			}
		}
		
		// process new DEF
		if (tp->ttype == TT_FILE) {
		    last_file = tp->str;
		    tp++; continue;
		}
		
		if (tp->ttype == TT_PRE && tp->num == PP_DEF) {
			pp->flags = tp->flags;
			tp++; syntax(tp->ttype == TT_SYM, tp, "expected DEF name");
			if (pre(tp->str, PT_NONE)) syntax(0, tp, "re-defined: %s", tp->str);
			t = tp; tp++;
			if (tp->flags & TF_FIELD) {		// fixme: not quite right wrt expr()
				pp->width = tp->width;
				pp->flags |= TF_FIELD;
			}
			tp = expr(tp, &ep1, &val, 1);
			pp->str = t->str; pp->ptype = PT_DEF; sscanf(last_file, "%*[^a-z]%[a-z]", pp->config_prefix);
			if (debug) printf("DEF \"%s\" %d (from file %s <%s>)\n", pp->str, val, last_file, pp->config_prefix);
			tp++;	// remove extra \n
			pp->val = val;
			pp++; continue;
		}
		
		// process new STRUCT
		if (tp->ttype == TT_PRE && tp->num == PP_STRUCT) {
			int size, offset;

			tp++; syntax(tp->ttype == TT_SYM, tp, "expected STRUCT name"); tt = tp; tp++; offset = 0;
			syntax(tp->ttype == TT_EOL, tp, "expecting STRUCT EOL"); tp++;
			while (!(tp->ttype == TT_PRE && tp->num == PP_ENDS)) {
				t = tp;
				syntax(tp->ttype == TT_DATA, t, "expecting STRUCT data"); size = tp->num; tp++;
				syntax(tp->ttype == TT_SYM, t, "expecting STRUCT sym"); tp++;
				tp = expr(tp, &ep1, &val, 1);
				size *= val;	// TODO alignment?
				syntax(tp->ttype == TT_EOL, t, "expecting STRUCT EOL"); tp++;
				pp->str = (t+1)->str; pp->ptype = PT_MEMBER; pp->dtype = t->str; pp->size = t->num, pp->ninst = val;
				pp->offset = offset; offset += size;
				if (debug) printf("MEMBER \"%s\" %s %d ninst %d offset %d\n", pp->str, pp->dtype, pp->size, pp->ninst, pp->offset);
				pp++;
			}
			
			pp->str = tt->str; pp->ptype = PT_STRUCT; pp->size = offset;
			if (debug) printf("STRUCT \"%s\" size %d\n", pp->str, pp->size);
			pp++; tp+=2; continue;	// remove extra \n
		}
		
		// remove MACROs
		if (tp->ttype == TT_PRE && tp->num == PP_MACRO) {
			if (debug) printf("remove MACRO %s\n", (tp+1)->str);
			while (!(tp->ttype == TT_PRE && tp->num == PP_ENDM)) tp++;
			tp += 2; continue;
		}
		
		*to++ = *tp++;
	}
	
	ep2 = to; pass3 = ep2;
	if (debug) dump_tokens("pass2", pass2, ep2);


	// pass 3: REPEAT, FACTOR
	curline=1;

    tp_start = pass2; tp_end = pass3;
	for (tp=pass2, to=pass3; tp != ep2;) {

		if (tp->ttype == TT_EOL) {
			curline = tp->num;
		}

		if (tp->ttype == TT_PRE && tp->num == PP_REPEAT) {
			tp++; tp = expr(tp, &ep2, &val, 1);
			t = tp+1;
			if (debug) printf("REPEAT %d\n", val);
			for (i=0; i < (val? val:1); i++) {	// legal for val to be zero
				tp = t;
				while (tp->ttype != TT_PRE && tp != ep2) {
					if (tp->ttype == TT_ITER) {
						if (val) { to->ttype = TT_NUM; to->num = i; to++; }
						tp++;
					} else {
						if (val) *to++ = *tp;
						tp++;
					}
				}
				syntax(tp->ttype == TT_PRE && tp->num == PP_ENDR, tp, "expecting REPEAT END");
			}
			tp+=2;		// remove extra \n
			continue;
		} else
		
		if (tp->ttype == TT_PRE && tp->num == PP_FACTOR) {
			int fcall;
			tp++; tp = expr(tp, &ep2, &val, 1);
			t = tp+1;
			if (debug) printf("FACTOR %d\n", val);
			int limit=100;
			while (val) {
				tp = t;
				while (tp->ttype != TT_PRE || tp->num != PP_ENDF) {
					while (tp->ttype != TT_PRE || tp->num != PP_FCALL) {
						tp++;
					}
					tp++; syntax(tp->ttype == TT_NUM, tp, "expecting FCALL num"); fcall = tp->num; tp++;
					if (debug) printf("\tFCALL %d\n", fcall);
					if (val >= fcall) {
						val -= fcall;
						to->ttype = TT_OPC, to->str = (char *) "call"; to->num = OC_CALL; to++;
						if (debug) { printf("\t\tval %d call ", val); token_dump(tp); printf("\n"); }
						*to++ = *tp; to->ttype = TT_EOL; to->num = curline+1; to++;	// target
						tp = t;
						continue;
					}
					tp+=2;	// also skip \n
					if (limit-- == 0) panic("FACTOR never converged");
				}
			}
			syntax(tp->ttype == TT_PRE && tp->num == PP_ENDF, tp, "expecting FACTOR END");
			tp+=2;		// remove extra \n
			continue;
		}
		
		*to++ = *tp++;
	}

	ep3 = to; pass4 = ep3;
	if (debug) dump_tokens("pass3", pass3, ep3);


	// pass 4: compiled constants (done in-place)
	curline=1;

    tp_start = pass3; tp_end = ep3;
	for (tp=pass3+1; tp != ep3-1; tp++) {   // +/- due to lookbehind / lookahead below

		if (tp->ttype == TT_EOL) {
			curline = tp->num;
		}

		def(tp, &ep3); def(tp+1, &ep3);		// fix sizeof forward references and dyn label construction
		if (tp->ttype != TT_OPR) continue;
		
		if (expr_collapse(tp-1, &ep3, &val) != NULL) {
		    tp--;
		    continue;
		}

		// concat to make numeric generated branch target: SYM # NUM -> SYM
		if (tp->num == OPR_CONCAT && (tp-1)->ttype == TT_SYM && (tp+1)->ttype == TT_NUM) {
			snprintf(sym, sizeof(sym), "%s%d", (tp-1)->str, (tp+1)->num);
			if (debug) printf("%s # %d = %s\n", (tp-1)->str, (tp+1)->num, sym);
			string_enter(sym, &(tp-1)->str, 0);
			pullup(tp, tp+2, &ep3); tp--;
			continue;
		}

		// concat to make numeric branch label: SYM # : -> LABEL
		if (tp->num == OPR_CONCAT && (tp-1)->ttype == TT_SYM && (tp+1)->ttype == TT_OPR && (tp+1)->num == OPR_LABEL) {
			tp--; if (debug) printf("%s # : = %s:\n", tp->str, tp->str); tp->ttype = TT_LABEL;
			st = string_find(tp->str); st->flags = SF_LABEL;
			pullup(tp+1, tp+3, &ep3);
			continue;
		}
	}

	ep4 = ep3; pass4 = pass3;
	if (debug) dump_tokens("pass4", pass4, ep4);


	// pass 5: allocate space and resolve (possibly forward referenced) LABELs (done in-place)
	int a=0;
	curline=1;

    tp_start = pass4; tp_end = ep4;
	for (tp=pass4; tp != ep4; tp++) {
		tokens_t *t, *tn = tp+1;
		int op, oper=0;
		
		if (a/2 >= CPU_RAM_SIZE) {
		    printf("a/2=%d CPU_RAM_SIZE=%d\n", a/2, CPU_RAM_SIZE);
		    assert(a/2 < CPU_RAM_SIZE);
		}
		
		if (tp->ttype == TT_EOL) {
			curline = tp->num;
		}

		// resolve label
		if (tp->ttype == TT_LABEL) {
			st = string_find(tp->str);
			st->flags |= SF_LABEL | SF_DEFINED; st->val = a;
			if (debug) printf("LABEL %s = %04x\n", tp->str, st->val);
		} else

		// pseudo-instruction calls		FIXME: using EOL to detect is a hack
		if ((tp-1)->ttype == TT_EOL && tp->ttype == TT_SYM /*&& (st = string_find(tp->str)) && (st->flags & SF_DEFINED)*/ ) {
			if (debug) printf("%04x pseudo-call %s\n", a, tp->str);
			a += 2;
		} else
		
		// data decl
		if (tp->ttype == TT_DATA && (tp+1)->ttype == TT_SYM) {
			if (debug) printf("%04x u%d %s\n", a, tp->num*8, (tp+1)->str);
			a += tp->num;
		} else
		
		// data decl
		if (tp->ttype == TT_DATA && (tp+1)->ttype == TT_NUM) {
			if (debug) printf("%04x u%d 0x%x\n", a, tp->num*8, (tp+1)->num);
			a += tp->num;
		} else
		
		// instruction
		if (tp->ttype == TT_OPC) {
			if (debug) printf("%04x %s\n", a, tp->str);

			// have to do this here, in a later pass, depending on when constant was resolved, use in macros etc.
			if ((strcmp(tp->str, "push") == 0) && ((tn->ttype == TT_NUM) && (tn->num >= 0x8000))) {
				#if 1
				if (debug) printf("PUSH: @%04x push %04x -> push %04x; ", a, tn->num, tn->num>>1);
				t = tn+1;
				insert(2, t, &ep4); t->ttype = TT_EOL; t->num = curline+1; t++;
				if ((tn->num & 1) == 0) {
					tn->num = tn->num >> 1;
					if (debug) printf("shl;\n");
					t->ttype = TT_OPC; t->str = (char *) "shl"; t->num = OC_SHL;
				} else {
					tn->num = tn->num & 0x7fff;
					if (debug) printf("or_0x8000_assist;\n");
					t->ttype = TT_SYM; t->str = (char *) "or_0x8000_assist"; t->num = 0;
				}
				tp += 4; a += 2;
				if (debug) printf("%04x %s\n", a, t->str);
				#else
				// until we decide to add the not16 insn
				if (debug) printf("PUSH: @%04x push %04x -> push %04x; shl; ", a, tn->num, tn->num>>1);
				int odd = tn->num & 1; tn->num = tn->num >> 1;
				t = tn+1; insert(1, t, &ep4); t->ttype = TT_EOL; t->num = curline+1;
				t++; insert(1, t, &ep4); t->ttype = TT_OPC; t->str = "shl"; t->num = OC_SHL;
				tp += 4; a += 2;
				
				if (odd) {
					if (debug) printf("addi 1; ");
					t++; insert(1, t, &ep4); t->ttype = TT_EOL; t->num = curline+1;
					t++; insert(1, t, &ep4); t->ttype = TT_OPC; t->str = "addi"; t->num = OC_ADDI + 0;
					t++; insert(1, t, &ep4); t->ttype = TT_NUM; t->num = 1;
					tp += 3; a += 2;
				}
				if (debug) printf("\n");
				#endif
			}

			a += 2;
		} else
		
		// structure
		if (tp->ttype == TT_STRUCT) {
			if (debug) printf("%04x STRUCT %s size 0x%x\n", a, tp->str, tp->num);
			a += tp->num;
		} else
		
		;
	}
	
	
	// pass 6: compile expressions involving newly resolved labels (done in-place)
	curline=1;

	for (tp=pass4; tp != ep4; tp++) {

		if (tp->ttype == TT_EOL) {
			curline = tp->num;
		}

		if (tp->ttype != TT_OPR) continue;
		def(tp-1, &ep4); def(tp+1, &ep4);

		// NUM OPR NUM -> NUM
		if ((tp-1)->ttype == TT_NUM && (tp+1)->ttype == TT_NUM) {
			if (debug) printf("(LABEL) CONST %d %s %d = ", (tp-1)->num, tp->str, (tp+1)->num);
			t = tp-1; expr(t, &ep4, &val, 0); t->num = val;
			if (debug) printf("%d\n", val);
			pullup(tp, tp+2, &ep4); tp--;
			continue;
		}
	}
	
	
	// generate header files
	FILE *cfp = NULL, *hfp = NULL, *vfp = NULL;
	char *last_prefix = NULL;
	
	if (gen || only_gen_other) {
	    if (!only_gen_other) {
            // skip writing Verilog cfg file if on read-only filesystem (e.g. NFS mounted read-only)
            cfs = (char *) "../verilog/" FN_PREFIX ".cfg.vh"; // included by verilog
            cfp = fopen(cfs, "w");
            if (cfp == NULL && errno != EROFS) sys_panic("fopen cfs");
            if (cfp) printf("generating include file: %s\n", cfs);
        }

		for (p = preproc; p->str; p++) {
			if (p->ptype != PT_DEF) continue;
            if (debug) printf("PT_DEF %s def file %s\n", p->str, p->config_prefix);
            if (only_gen_other && strcmp(p->config_prefix, "other") != 0) continue;

            if ((p->flags & TF_CFG_H) && cfp) {
                fprintf(cfp, "localparam RX_CFG = %d;\n", p->val);
                if (p->val > 0 && p->val < 14)
                    fprintf(cfp, "`define USE_WF\n");
            }
            
            if (!last_prefix || strcmp(last_prefix, p->config_prefix) != 0) {
                last_prefix = p->config_prefix;
                if (debug) printf("PT_DEF new config prefix: %s\n", p->config_prefix);
                
                if (hfp) {
                    fprintf(hfp, "\n#endif\n");
                    fclose(hfp);
                    hfp = NULL;
                }

                if (vfp) {
                    //fprintf(vfp, "\n`endif\n");
                    fclose(vfp);
                    vfp = NULL;
                    
                    // append inline file onto end of ../verilog/kiwi.gen.vh
                    if (vfs && strstr(vfs, "kiwi.gen.vh")) {
                        char *cmd_p;
                        asprintf(&cmd_p, "cat ../verilog/kiwi.gen.vh ../verilog/kiwi.inline.vh >/tmp/kiwi.gen.vh; mv /tmp/kiwi.gen.vh ../verilog");
                        system(cmd_p);
                        free(cmd_p);
                    }
                }

                asprintf(&hfs, "%s/%s.gen.h", odir, p->config_prefix);     // included by .cpp / .c
                if ((hfp = fopen(hfs, "w")) == NULL) sys_panic("fopen hfs");
                // odir should never be NFS mounted (hence potentially read-only)
                printf("generating include file: %s\n", hfs);

                // skip writing Verilog gen file if on read-only filesystem (e.g. NFS mounted read-only)
                asprintf(&vfs, "../verilog/%s.gen.vh", p->config_prefix);   // included by verilog
                vfp = fopen(vfs, "w");
                if (vfp == NULL && errno != EROFS) sys_panic("fopen vfs");
                if (vfp) printf("generating include file: %s\n", vfs);

                const char *warn = "// this file auto-generated by the e_cpu assembler -- edits will be overwritten\n\n";
                fprintf(hfp, "%s#ifndef _GEN_%s_H_\n#define _GEN_%s_H_\n\n// from assembler DEF directives:\n\n",
                    warn, p->config_prefix, p->config_prefix);
                //if (vfp) fprintf(vfp, "%s`ifndef _GEN_%s_VH_\n`define _GEN_%s_VH_\n\n// from assembler DEF directives:\n\n",
                //    warn, p->config_prefix, p->config_prefix);
                if (vfp) fprintf(vfp, "%s// from assembler DEF directives:\n\n", warn);
            }
            
            if (p->flags & TF_DOT_H) {
                fprintf(hfp, "%s#define %s    // DEFh 0x%x\n", p->val? "":"//", p->str, p->val);
                fprintf(hfp, "#define VAL_%s %d\n", p->str, p->val);
                if (vfp) fprintf(vfp, "%s`define %s%s    // DEFh 0x%x\n", p->val? "":"//", p->str, p->val? " 1":"", p->val);
            }
            if (p->flags & TF_DOT_VP) {
                fprintf(hfp, "#define %s %d    // DEFp 0x%x\n", p->str, p->val, p->val);
                if (vfp) {
                    if (p->flags & TF_FIELD)
                        fprintf(vfp, "\tlocalparam %s = %d\'d%d;    // DEFp 0x%x\n", p->str, p->width, p->val, p->val);
                    else
                        fprintf(vfp, "\tlocalparam %s = %d;    // DEFp 0x%x\n", p->str, p->val, p->val);
                    fprintf(vfp, "%s`define DEF_%s%s\n", p->val? "":"//", p->str, p->val? " 1":"");
                }
            }
            if (p->flags & TF_DOT_VB) {
                fprintf(hfp, "#define %s %d    // DEFb 0x%x\n", p->str, p->val, p->val);
                // determine bit position number
                if (p->val) for (i=0; ((1<<i) & p->val)==0; i++) ; else i=0;
                if (vfp) fprintf(vfp, "\tlocalparam %s = %d;    // DEFb: bit number for value: 0x%x\n", p->str, i, p->val);
            }
		}

        if (cfp) {
            fclose(cfp);
            cfp = NULL;
        }
        
        if (hfp) {
            fprintf(hfp, "\n#endif\n");
            fclose(hfp);
            hfp = NULL;
        }

		if (vfp) {
            fclose(vfp);
            vfp = NULL;

            // append inline file onto end of ../verilog/kiwi.gen.vh
            if (vfs && strstr(vfs, "kiwi.gen.vh")) {
                char *cmd_p;
                asprintf(&cmd_p, "cat ../verilog/kiwi.gen.vh ../verilog/kiwi.inline.vh >/tmp/kiwi.gen.vh; mv /tmp/kiwi.gen.vh ../verilog");
                system(cmd_p);
                free(cmd_p);
            }
        }
	} else {
	    if (debug) {
            for (p=preproc; p->str; p++) {
                if (p->ptype != PT_DEF) continue;
                char s[64];
                s[0] = '\0';
				if (p->flags & TF_CFG_H) strcat(s, "RX_CFG,");
				if (p->flags & TF_DOT_H) strcat(s, "DEFh,");
				if (p->flags & TF_DOT_VP) strcat(s, "DEFp,");
				if (p->flags & TF_DOT_VB) strcat(s, "DEFb,");
                printf("=> PT_DEF %s [%s] def file %s\n", p->str, s, p->config_prefix);	    
            }
        }
	}


	// pass 7: emit code
	int bad=0;
	if (debug) dump_tokens("emit", pass4, ep4);
	a = 0;
	curline=1;
	bool error = FALSE;
	char comma = ' ';

    tp_start = pass4; tp_end = ep4;
	for (tp=pass4; tp != ep4; tp++) {
		int op, oc, operand_type=0;
		u2_t val_2;
		u4_t val;
		static u2_t out;
		strs_t *st;
		init_code_t *ic;
		
		if (tp->ttype == TT_LABEL) {
			if (debug || show_bin) printf("%s:\n", tp->str);
			continue;
		}
		
		// pseudo-instruction calls
		if (tp->ttype == TT_SYM && (st = string_find(tp->str)) && (st->flags & SF_DEFINED)) {
			tp->ttype = TT_OPC; tp->num = OC_CALL + st->val;
		} // drop into emit insn code below

		// data decl (fixme: no range checking of initializer)
		if (tp->ttype == TT_DATA) {
			t = tp+1;
			if (t->ttype == TT_NUM) {
				val = t->num, operand_type=1;
			} else
			if (t->ttype == TT_SYM) {
			    st = string_find(t->str);
			    if (st) {
			        if (st->flags & SF_DEFINED) {
				        val = st->val, operand_type=2;
			        } else {
                        syntax(0, t, "symbol not defined: %s", t->str);
                    }
			    } else {
                    syntax(0, t, "symbol not found: %s", t->str);
			    }
			}
			if (debug || show_bin) printf("\t%04x u%d ", a, tp->num*8);
			if ((debug || show_bin) && operand_type==2) printf("%s ", st->str);
			if (tp->num==2) {
				assert(val <= 0xffff);
				val_2 = val & 0xffff;
				if (debug || show_bin) printf("\t%04x", val_2);
				write(bfd, &val_2, 2);
				if (write_coe) {
					fprintf(efp, "%c\n%04x", comma, val_2);
				}
				a += 2;
			} else
			if (tp->num==4) {
				if (debug || show_bin) printf("\t%08x", val);
				write(bfd, &val, 4);
				if (write_coe) {
					fprintf(efp, "%c\n%04x", comma, val >> 16);
					fprintf(efp, "%c\n%04x", comma, val & 0xffff);
				}
				a += 4;
			} else
			if (tp->num==8) {
				if (val) panic("u64 non-zero decl initializer not supported yet");
				val = 0;
				if (debug || show_bin) printf("\t%08x|%08x", /* val >> 32 */ 0, val & 0xffffffff);
				write(bfd, &val, 4);
				write(bfd, &val, 4);
				if (write_coe) {
					fprintf(efp, "%c\nno u64 yet!", comma);
				}
				a += 8;
			} else
				panic("illegal decl size");
			if (debug || show_bin) { printf("\n"); comma = ','; }
			tp++;
		} else
		
		// emit instruction
		if (tp->ttype == TT_OPC) {
			int oper;
			op = oc = tp->num; t=tp+1;
			u2_t oc8 = oc & 0xff00, oc5 = oc & 0xf001, oc_io = oc & 0xf800;
			
			// opcode & operand
			if (t->ttype == TT_NUM) {
				oper = t->num; operand_type = 1;
			} else
			if ((t->ttype == TT_SYM) && ((st = string_find(t->str)) && (st->flags & SF_DEFINED))) {
				oper = st->val; operand_type = 2;
			} else
			if (t->ttype == TT_EOL) {
				oper = 0; operand_type = 3;
			}
			
			// check operand
			switch (oc) {
				case OC_PUSH:	syntax(oper >= 0 && oper <= 0x7fff, t, "constant outside range 0..0x7fff"); break;
				case OC_ADDI:	syntax(oper >= 0 && oper <= 127, t, "constant outside range 0..127"); break;

				case OC_CALL:
				case OC_BR:
				case OC_BRZ:
				case OC_BRNZ:
				case OC_LOOP:
				case OC_LOOP2:
								syntax(!(oper&1), t, "destination address must be even");
								syntax(oper >= 0 && oper < (CPU_RAM_SIZE<<1), t, "destination address out of range");
								break;

				case OC_RDREG:
				case OC_RDREG2:
				case OC_WRREG:
				case OC_WRREG2:
				case OC_WREVT:
				case OC_WREVT2: {
								syntax(oper >= 1 && oper <= 0x7ff, t, "i/o specifier outside range 1..0x7ff: 0x%04x", oper);
								int ones = count_ones(oper);
								syntax(ones <= 2, t, "too many bits (%d) in i/o specifier 0x%04x", ones, oper);
								break;
				}

				default:		if (operand_type != 3) {
									syntax(0, t, "instruction takes no operand");
								}
								break;
			}
			
			if ((tp->flags & TF_CIN) && (oc != OC_ADD)) syntax(0, tp, "\".cin\" only valid for add instruction");
			bool rstk = (oc == OC_R || oc == OC_R_FROM || oc == OC_TO_R);
			if ((tp->flags & TF_RET) && (((oc & 0xe000) != 0x8000) || rstk)) syntax(0, tp, "\".r\" not valid for this instruction");
			
			op += oper;
			if (tp->flags & TF_RET) op |= OPT_RET;
			if (tp->flags & TF_CIN) op |= OPT_CIN;
			
			if (debug || show_bin) printf("\t%04x %04x %s%s ", a, op, tp->str, (tp->flags&TF_RET)? ".r": (tp->flags&TF_CIN)? ".cin":"");
			if (write_coe) {
				fprintf(efp, "%c\n%04x", comma, op);
				comma = ',';
			}
			if (operand_type==1) {
				if (debug || show_bin) printf("%04x", t->num);
				tp++;
			} else
			if (operand_type==2) {
				if (debug || show_bin) printf("%s", st->str);
				tp++;
			}

			if (stats) {
                for (dp=dict; dp->str; dp++) {
                    if (dp->ttype != TT_OPC) continue;
                    if ((oc & ~(dp->mask)) != dp->val) continue;
                    dp->stats++;
                    break;
                }
                if (!dp->str) {
                    printf("oc=0x%04x\n", oc);
                    panic("stats opcode");
                }
			}
			
			if (compare_code) {
				for (ic=init_code; ic->addr != -1; ic++) {
					if (ic->addr == a) {
						bad |= op != ic->code;
						if (debug || show_bin) printf("\t\t%04x %s", ic->code, (op != ic->code)? "---------------- BAD" : "ok");
						break;
					}
				}
				syntax2(ic->addr != -1, "not in init_code");
			}
			
			if (debug || show_bin) printf("\n");
			fprintf(ofp, "\t0x%04x, 0x%04x,\n", a, op);
			out = op;
			if (write(bfd, &out, 2) != 2) sys_panic("wr");
			a += 2;
		} else
		
		// allocate space for struct
		if (tp->ttype == TT_STRUCT) {
			if (debug || show_bin) printf("\t%04x STRUCT %s size 0x%x(%d)\n", a, tp->str, tp->num, tp->num);
			if (write_coe) {
				for (i=0; i<tp->num; i+=2) {
					fprintf(efp, "%c\n%04x", comma, 0);
					comma = ',';
				}
			}
			a += tp->num;
			out = 0;
			assert((tp->num & 1) == 0);
			for (i=0; i<tp->num; i+=2) write(bfd, &out, 2);
		} else
		
		if (tp->ttype == TT_EOL) {
			curline = tp->num;
			continue;
		} else
		
		{
			errmsg("found unexpected: ", tp);
			error = TRUE;
			token_dump(tp);
			printf("\n");
		}
	}
	
	fclose(ofp);
	close(bfd);
	
	if (write_coe) {
		fprintf(efp, ";\n");
		fclose(efp);
	}
	
	if (error) panic("errors detected");

	printf("used %d/%d CPU RAM (%d insns remaining)\n", a/2, CPU_RAM_SIZE, CPU_RAM_SIZE - a/2);
	
	if (bad) panic("============ generated code compare errors ============\n");
	printf("\n");

	if (stats) {
		printf("insn use statistics:\n");
		int sum = 0;
        for (i=0, dp=dict; dp->str; dp++, i++) {
            if (dp->ttype != TT_OPC) continue;
            printf("%02d: 0x%04x %4d %s\n", i, dp->val, dp->stats, dp->str);
            sum += dp->stats;
        }
		printf("sum insns = %d\n", sum);
	}

	return 0;
}
