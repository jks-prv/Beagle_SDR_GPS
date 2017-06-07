
/*
	_Extremely_ quick & dirty assembler for the language used in Andrew Holme's FORTH-like FPGA processor
	for his homemade GPS project: holmea.demon.co.uk/GPS
	
	We made slight changes to some of the pre-processing directives to make our parsing easier.
	
	There is very little bounds or error checking here and pathological input will cause the program to crash.
	The multi-pass, minimal techniques used are fine for the small programs that are assembled, but this
	assembler is completely unsuitable in its current form for any larger use.
	
	Look at the project assembler source code for examples of all the supported constructs.
	
	runtime arguments:
		-c	compare generated binary to that from "gps44.h" for debugging
		-d	enable debugging printfs
		-b	print generated binary
		-t	use input and output files "test.asm" & "test.h" instead of defaults
	
	issues:
		expression evaluation is strictly left-to-right
		really need to improve the error checking
		need runtime args instead of hardcoded stuff
*/

// Copyright (c) 2013-2016 John Seamons, ZL/KF6VO

#include "asm.h"

int show_bin, gen=1;

#define LBUF 1024
char linebuf[LBUF];

#define	TBUF	64*1024
tokens_t tokens[TBUF], *pass1, *pass2, *pass3, *pass4;

typedef struct {
	char *str;
	token_type_e ttype;
	int val;
	int flags;
} dict_t;

#include <cpu.h>

// dictionary of reserved symbols and tokens
dict_t dict[] = {
	{ "DEF",		TT_PRE,		PP_DEF,			0 },
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
	{ "#if",		TT_PRE,		PP_IF },			// supports '#if NUM' and '#if SYM'
	{ "#else",		TT_PRE,		PP_ELSE },
	{ "#endif",		TT_PRE,		PP_ENDIF },

	{ "push",		TT_OPC,		OC_PUSH },
	{ "nop",		TT_OPC,		OC_NOP },
	{ "ret",		TT_OPC,		OC_NOP + OPT_RET },
	{ "dup",		TT_OPC,		OC_DUP },
	{ "swap",		TT_OPC,		OC_SWAP },
	{ "swap16",		TT_OPC,		OC_SWAP16 },
	{ "over",		TT_OPC,		OC_OVER },
	{ "pop",		TT_OPC,		OC_POP },
	{ "drop",		TT_OPC,		OC_POP },
	{ "rot",		TT_OPC,		OC_ROT },
	{ "addi",		TT_OPC,		OC_ADDI },
	{ "add",		TT_OPC,		OC_ADD },
	{ "sub",		TT_OPC,		OC_SUB },
	{ "mult",		TT_OPC,		OC_MULT },
	{ "and",		TT_OPC,		OC_AND },
	{ "or",			TT_OPC,		OC_OR },
//	{ "xor",		TT_OPC,		OC_XOR },
	{ "not",		TT_OPC,		OC_NOT },
	{ "shl64",		TT_OPC,		OC_SHL64 },
	{ "shl",		TT_OPC,		OC_SHL },
	{ "shr",		TT_OPC,		OC_SHR },
	{ "rdBit",		TT_OPC,		OC_RDBIT },
	{ "rdBit2",		TT_OPC,		OC_RDBIT2 },
	{ "fetch16",	TT_OPC,		OC_FETCH16 },
	{ "store16",	TT_OPC,		OC_STORE16 },
	{ "r",			TT_OPC,		OC_R },
	{ "r_from",		TT_OPC,		OC_R_FROM },
	{ "to_r",		TT_OPC,		OC_TO_R },
	{ "call",		TT_OPC,		OC_CALL },
	{ "br",			TT_OPC,		OC_BR },
	{ "brZ",		TT_OPC,		OC_BRZ },
	{ "brNZ",		TT_OPC,		OC_BRNZ },
	{ "rdReg",		TT_OPC,		OC_RDREG },
	{ "rdReg2",		TT_OPC,		OC_RDREG2 },
	{ "wrReg",		TT_OPC,		OC_WRREG },
	{ "wrReg2",		TT_OPC,		OC_WRREG2 },
	{ "wrEvt",		TT_OPC,		OC_WREVT },
	{ "wrEvt2",		TT_OPC,		OC_WREVT2 },

	{ "sp",			TT_OPC,		OC_SP },	// STACK_CHECK
	{ "rp",			TT_OPC,		OC_RP },
	
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
	{ "&",			TT_OPR,		OPR_AND,	TF_2OPR },
	{ "|",			TT_OPR,		OPR_OR,		TF_2OPR },
	{ "~",			TT_OPR,		OPR_NOT,	TF_1OPR },
	{ "sizeof",		TT_OPR,		OPR_SIZEOF },
	{ "#",			TT_OPR,		OPR_CONCAT },
	{ ":",			TT_OPR,		OPR_LABEL },
	{ "(",			TT_OPR,		OPR_PAREN },
	{ ")",			TT_OPR,		OPR_PAREN },
	
	{ "<iter>",		TT_ITER,	0 },
	
	{ 0,			0,			0 }
};

#define SBUF 64
char sym[SBUF];

typedef struct {
	int addr, code;
} init_code_t;

init_code_t init_code[] = {

//#include "gps44.h"
#include "cmp.h"

	-1, 0
};

#define	FN_PREFIX	"kiwi"

#define NIFILES_NEST 3
char ifiles[NIFILES_NEST][32];
int lineno[NIFILES_NEST];
u4_t ocstat[256];

char *ocname[256] = {
	"nop", "dup", "swap", "swap16", "over", "pop", "rot", "addi", "add", "sub", "mult", "and", "or", "xor", "not", "0x8F",
	"shl64", "shl", "shr", "rdbit", "fetch16", "store16", "SP", "RP", "0x98", "0x99", "0x9A", "0x9B", "r", "r_from", "to_r", "0x9F",
	"push", "add_cin", "rdbit2", "call", "br", "brz", "brnz",
	"rdreg", "rdreg2", "wrreg", "wrreg2", "wrevt", "wrevt2"
};

int main(int argc, char *argv[])
{
	int i, val;

	char *ifs = FN_PREFIX ".asm";					// source input
		  bfs = FN_PREFIX ".aout";					// loaded into FPGA via SPI
	char *ofs = "ecode.aout.h";						// included by simulator
		  hfs = "../" FN_PREFIX ".gen.h";			// included by .cpp / .c
		  vfs = "../verilog/" FN_PREFIX ".gen.vh";	// included by verilog
		  cfs = "../verilog/" FN_PREFIX ".coe";		// .coe file to init BRAMs

	int ifn;
	FILE *ifp[NIFILES_NEST], *ofp, *hfp, *vfp, *cfp;
	
	int bfd;
	char *lp = linebuf, *cp, *scp, *np, *sp;
	dict_t *dp;
	token_type_e ttype;
	tokens_t *tp = tokens, *tpp, *ltp = tokens, *ep0, *ep1, *ep2, *ep3, *ep4, *t, *tt, *to;
	preproc_t *pp = preproc, *p;
	strs_t *st;
	int compare_code=0, stats=0;
	char tsbuf[256];
	
	for (i=1; argc-- > 1; i++)
	if (argv[i][0] == '-') switch (argv[i][1]) {
		case 't': ifs = "test.asm"; ofs="test.aout.h"; bfs="test.aout"; break;
		case 'c': compare_code=1; printf("compare mode\n"); break;
		case 'd': debug=1; gen=0; break;
		case 'b': show_bin=1; gen=0; break;
		case 'n': gen=0; break;
		case 's': stats=1; break;
	}

	ifn=0; fn = ifs;
	strcpy(ifiles[ifn], ifs);
	if ((ifp[ifn] = fopen(ifs, "r")) == NULL) sys_panic("fopen ifs");
	if ((ofp = fopen(ofs, "w")) == NULL) sys_panic("fopen ofs");
	if ((bfd = open(bfs, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) sys_panic("open bfs");
	printf("assembling %s to: %s, %s\n", ifs, bfs, ofs);

	if (gen) {
		if ((hfp = fopen(hfs, "w")) == NULL) sys_panic("fopen hfs");
		if ((vfp = fopen(vfs, "w")) == NULL) sys_panic("fopen vfs");
		printf("generating include files: %s, %s\n", hfs, vfs);
		const char *warn = "// this file auto-generated by the embedded processor assembler -- edits likely to be overwritten\n\n";
		fprintf(hfp, "%s", warn);
		fprintf(hfp, "%s", "#ifndef _KIWI_GEN_H_\n#define _KIWI_GEN_H_\n\n// from assembler DEF directives:\n\n");
		fprintf(vfp, "%s`ifndef _KIWI_GEN_VH_\n`define _KIWI_GEN_VH_\n\n// from assembler DEF directives:\n\n", warn);
	}
	
	if (show_bin) {
		if ((cfp = fopen(cfs, "w")) == NULL) sys_panic("fopen cfs");
		fprintf(cfp, "; DEPTH = 2048\n; WIDTH = 16\nmemory_initialization_radix=16;\nmemory_initialization_vector=");
	}

	// pass 0: tokenize	
	while (ifn >= 0) {

		while (fgets(lp, LBUF, ifp[ifn])) {
			lineno[ifn]++; curline = lineno[ifn];
	
			if (debug) printf("%s:%03d %s", fn, curline, lp);
			cp=lp;
			
			if (sscanf(cp, "#include %s", tsbuf) == 1) {
				ifn++;
				if (ifn >= NIFILES_NEST) panic("too many nested include files");
				fn = ifiles[ifn]; strcpy(fn, tsbuf);
				printf("#include file: %s\n", fn);
				if ((ifp[ifn] = fopen(fn, "r")) == NULL) sys_panic("fopen include file");
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
						syntax(*cp == 'd', "expecting \'d"); cp++;
						tp->width = num;
						num = strtol(cp, &cp, 0);
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
							tp->ttype = dp->ttype; tp->str = dp->str; tp->num = dp->val; tp->flags = dp->flags;
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
						tp->ttype = dp->ttype; tp->str = dp->str; tp->num = dp->val; tp->flags = dp->flags; tp++; break;
					}
				}
				if (dp->str) continue;
				
				errmsg("unknown symbol: ");
				printf("\"%s\" followed by 0x%02x <%c>\n", sym, *cp, *cp);
				panic("errors detected");
				cp++;
			}
			
			if ((tp-1)->ttype != TT_EOL) {
				tp->ttype = TT_EOL; tp->num = curline+1; tp++;
			} else {
				(tp-1)->num++;
			}
		
			if (debug) for (t=ltp; t!=tp; t++) token_dump(t);
			ltp = tp;
			if (debug) printf("\n");
		}
		
		fclose(ifp[ifn]);
		ifn--;
		fn = ifiles[ifn];
	}

	fn = ifiles[0];
	ep0 = tp; pass1 = ep0;
	if (debug) printf("\ntokenize pass 0: %d strings %ld tokens\n\n", num_strings(), ep0-tokens);


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
			tp++; syntax(tp->ttype == TT_SYM, "expected MACRO name"); t = tp; tp++; pp->args = tp;
			for (i=0; tp->ttype != TT_EOL; i++) {
				syntax(tp->ttype == TT_SYM, "expected MACRO arg"); tp++;
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
			} else
			if ((tp+1)->ttype == TT_NUM) {
				keep[ifdef_lvl] = (tp+1)->num;
			} else
			if ((tp+1)->ttype == TT_SYM) {
				if ((p = pre((tp+1)->str, PP_DEF))) {
					keep[ifdef_lvl] = p->val;
				} else {
					keep[ifdef_lvl] = 0;
				}
			} else panic("expected #if SYM or NUM");
			
			if (debug) printf("IF %s %d\n", (tp+1)->str, keep[ifdef_lvl]);
			tp += 3; continue;
		}
		
		if (tp->ttype == TT_PRE && tp->num == PP_ELSE) {
		
			// only act if not skipping in previous level
			if (keep[ifdef_lvl-1]) keep[ifdef_lvl] ^= 1;
			if (debug) printf("ELSE %d\n", keep[ifdef_lvl]);
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
			if ((p = pre(tp->str, (preproc_type_e) -1))) {
				if (p->ptype == PT_MEMBER) {
					if (debug) printf("MEMBER \"%s\" offset %d\n", tp->str, p->offset);
					to->ttype = TT_NUM; to->num = p->offset; to++;
				} else
				if (p->ptype == PT_STRUCT) {
					if (debug) printf("STRUCT \"%s\"\n", tp->str);
					to->ttype = TT_STRUCT; to->str = tp->str; to->num = p->size; to++;
				} else {
					syntax(0, "symbol or MACRO undefined");
				}
				tp++;
			}
		}
		
		// process new DEF
		if (tp->ttype == TT_PRE && tp->num == PP_DEF) {
			pp->flags = tp->flags;
			tp++; syntax(tp->ttype == TT_SYM, "expected DEF name");
			t = tp; tp++;
			if (tp->flags & TF_FIELD) {		// fixme: not quite right wrt expr()
				pp->width = tp->width;
				pp->flags |= TF_FIELD;
			}
			tp = expr(tp, &ep1, &val, 1);
			pp->str = t->str; pp->ptype = PT_DEF;
			if (debug) printf("DEF \"%s\" %d\n", pp->str, val);
			tp++;	// remove extra \n
			pp->val = val;
			pp++; continue;
		}
		
		// process new STRUCT
		if (tp->ttype == TT_PRE && tp->num == PP_STRUCT) {
			int size, offset;

			tp++; syntax(tp->ttype == TT_SYM, "expected STRUCT name"); tt = tp; tp++; offset = 0;
			syntax(tp->ttype == TT_EOL, "expecting STRUCT EOL"); tp++;
			while (!(tp->ttype == TT_PRE && tp->num == PP_ENDS)) {
				t = tp;
				syntax(tp->ttype == TT_DATA, "expecting STRUCT data"); size = tp->num; tp++;
				syntax(tp->ttype == TT_SYM, "expecting STRUCT sym"); tp++;
				tp = expr(tp, &ep1, &val, 1);
				size *= val;	// TODO alignment?
				syntax(tp->ttype == TT_EOL, "expecting STRUCT EOL"); tp++;
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
				syntax(tp->ttype == TT_PRE && tp->num == PP_ENDR, "expecting REPEAT END");
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
					tp++; syntax(tp->ttype == TT_NUM, "expecting FCALL num"); fcall = tp->num; tp++;
					if (debug) printf("\tFCALL %d\n", fcall);
					if (val >= fcall) {
						val -= fcall;
						to->ttype = TT_OPC, to->str = "call"; to->num = OC_CALL; to++;
						if (debug) { printf("\t\tval %d call ", val); token_dump(tp); printf("\n"); }
						*to++ = *tp; to->ttype = TT_EOL; to->num = curline+1; to++;	// target
						tp = t;
						continue;
					}
					tp+=2;	// also skip \n
					if (limit-- == 0) panic("FACTOR never converged");
				}
			}
			syntax(tp->ttype == TT_PRE && tp->num == PP_ENDF, "expecting FACTOR END");
			tp+=2;		// remove extra \n
			continue;
		}
		
		*to++ = *tp++;
	}

	ep3 = to; pass4 = ep3;
	if (debug) dump_tokens("pass3", pass3, ep3);


	// pass 4: compiled constants (done in-place)
	curline=1;

	for (tp=pass3; tp != ep3; tp++) {

		if (tp->ttype == TT_EOL) {
			curline = tp->num;
		}

		def(tp, &ep3); def(tp+1, &ep3);		// fix sizeof forward references and dyn label construction
		if (tp->ttype != TT_OPR) continue;

		// NUM OPR NUM -> NUM
		if ((tp-1)->ttype == TT_NUM && tp->flags & TF_2OPR && (tp+1)->ttype == TT_NUM) {
			if (debug) printf("CONST %d %s %d = ", (tp-1)->num, tp->str, (tp+1)->num);
			t = tp-1; expr(t, &ep3, &val, 0); t->num = val;
			if (debug) printf("%d\n", val);
			pullup(tp, tp+2, &ep3); tp--;
			continue;
		}

		// NUM OPR -> NUM
		if ((tp-1)->ttype == TT_NUM && tp->flags & TF_1OPR) {
			if (debug) printf("CONST %d %s = ", (tp-1)->num, tp->str);
			t = tp-1; expr(t, &ep3, &val, 0); t->num = val;
			if (debug) printf("%d\n", val);
			pullup(tp, tp+1, &ep3); tp--;
			continue;
		}

		// concat to make numeric generated branch target: SYM # NUM -> SYM
		if (tp->num == OPR_CONCAT && (tp-1)->ttype == TT_SYM && (tp+1)->ttype == TT_NUM) {
			sprintf(sym, "%s%d", (tp-1)->str, (tp+1)->num);
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

	for (tp=pass4; tp != ep4; tp++) {
		tokens_t *t, *tn = tp+1;
		int op, oper=0;
		
		assert (a/2 < CPU_RAM_SIZE);
		
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
					t->ttype = TT_OPC; t->str = "shl"; t->num = OC_SHL;
				} else {
					tn->num = tn->num & 0x7fff;
					if (debug) printf("or_0x8000_assist;\n");
					t->ttype = TT_SYM; t->str = "or_0x8000_assist"; t->num = 0;
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
	if (gen) {
		for (p=preproc; p->str; p++) {
			if (p->ptype == PT_DEF) {
				if (p->flags & TF_DOT_H) {
					fprintf(hfp, "%s#define %s    // DEFh 0x%x\n", p->val? "":"//", p->str, p->val);
					fprintf(hfp, "#define VAL_%s %d\n", p->str, p->val);
					fprintf(vfp, "%s`define %s    // DEFh 0x%x\n", p->val? "":"//", p->str, p->val);
				}
				if (p->flags & TF_DOT_VP) {
					fprintf(hfp, "#define %s %d    // DEFp 0x%x\n", p->str, p->val, p->val);
					if (p->flags & TF_FIELD)
						fprintf(vfp, "\tlocalparam %s = %d\'d%d;    // DEFp 0x%x\n", p->str, p->width, p->val, p->val);
					else
						fprintf(vfp, "\tlocalparam %s = %d;    // DEFp 0x%x\n", p->str, p->val, p->val);
					fprintf(vfp, "%s`define DEF_%s\n", p->val? "":"//", p->str);
				}
				if (p->flags & TF_DOT_VB) {
					fprintf(hfp, "#define %s %d    // DEFb 0x%x\n", p->str, p->val, p->val);
					// determine bit position number
					if (p->val) for (i=0; ((1<<i) & p->val)==0; i++) ; else i=0;
					fprintf(vfp, "\tlocalparam %s = %d;    // DEFb: bit number for value: 0x%x\n", p->str, i, p->val);
				}
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
			}
			if ((t->ttype == TT_SYM) && ((st = string_find(t->str)) && (st->flags & SF_DEFINED))) {
				val = st->val, operand_type=2;
			}
			if (debug || show_bin) printf("%04x u%d ", a, tp->num*8);
			if ((debug || show_bin) && operand_type==2) printf("%s ", st->str);
			if (tp->num==2) {
				assert (val <= 0xffff);
				val_2 = val & 0xffff;
				if (debug || show_bin) printf("%04x", val_2);
				write(bfd, &val_2, 2);
				if (show_bin) {
					fprintf(cfp, "%c\n%04x", comma, val_2);
				}
				a += 2;
			} else
			if (tp->num==4) {
				if (debug || show_bin) printf("%08x", val);
				write(bfd, &val, 4);
				if (show_bin) {
					fprintf(cfp, "%c\n%04x", comma, val >> 16);
					fprintf(cfp, "%c\n%04x", comma, val & 0xffff);
				}
				a += 4;
			} else
			if (tp->num==8) {
				if (val) panic("u64 non-zero decl initializer not supported yet");
				val = 0;
				if (debug || show_bin) printf("%08x|%08x", /* val >> 32 */ 0, val & 0xffffffff);
				write(bfd, &val, 4);
				write(bfd, &val, 4);
				if (show_bin) {
					fprintf(cfp, "%c\nno u64 yet!", comma);
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
				case OC_PUSH:	syntax(oper >= 0 && oper < 0x8000, "constant outside range 0..0x7fff"); break;
				case OC_ADDI:	syntax(oper >= 0 && oper < 128, "constant outside range 0..127"); break;
				case OC_CALL:
				case OC_BR:
				case OC_BRZ:
				case OC_BRNZ:
								syntax(!(oper&1), "destination address must be even");
								syntax(oper >=0 && oper < (CPU_RAM_SIZE<<1), "destination address out of range");
								break;
				case OC_RDREG:
				case OC_RDREG2:
				case OC_WRREG:
				case OC_WRREG2:
				case OC_WREVT:
				case OC_WREVT2:
								syntax(oper >=0 && oper <= 0x7ff, "i/o specifier outside range 0..0x7ff");
								break;
				default:		if (operand_type != 3) {
									syntax(0, "instruction takes no operand");
								}
								break;
			}
			
			if ((tp->flags & TF_CIN) && (oc != OC_ADD)) syntax(0, "\".cin\" only valid for add instruction");
			bool rstk = oc == OC_R || oc == OC_R_FROM || oc == OC_TO_R;
			if ((tp->flags & TF_RET) && (((oc & 0xe000) != 0x8000) || rstk)) syntax(0, "\".r\" not valid for this instruction");
			
			op += oper;
			if (tp->flags & TF_RET) op |= OPT_RET;
			if (tp->flags & TF_CIN) op |= OPT_CIN;
			
			if (debug || show_bin) printf("%04x %04x %s%s ", a, op, tp->str, (tp->flags&TF_RET)? ".r": (tp->flags&TF_CIN)? ".cin":"");
			if (show_bin) {
				fprintf(cfp, "%c\n%04x", comma, op);
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
				#define OCS_PUSH		32
				#define OCS_ADD_CIN		33
				#define OCS_RDBIT2		34
				#define OCS_CALL		35
				#define OCS_BR			36
				#define OCS_BRZ			37
				#define OCS_BRNZ		38
				#define OCS_REG_EVT		39	// 39..44
				#define N_OCS			45
				
				if (oc == OC_PUSH) {
					ocstat[OCS_PUSH]++;
				} else
				
				if (op == (OC_ADD | OPT_CIN)) {		// note op, not oc
					ocstat[OCS_ADD_CIN]++;
				} else
				
				if (oc == OC_RDBIT2) {
					ocstat[OCS_RDBIT2]++;
				} else
				
				if ((oc & 0xf001) == OC_CALL) {		// & 0xf001 due to insn extension syntax
					ocstat[OCS_CALL]++;
				} else
				
				if (oc == OC_BR) {
					ocstat[OCS_BR]++;
				} else
				
				if (oc == OC_BRZ) {
					ocstat[OCS_BRZ]++;
				} else
				
				if (oc == OC_BRNZ) {
					ocstat[OCS_BRNZ]++;
				} else
				
				if (oc >= OC_RDREG && oc <= OC_WREVT2) {
					ocstat[OCS_REG_EVT + ((oc >> 11) & 7)]++;
				} else
				
				{
					if (oc < 0x8000 || oc > 0x9f00) {
						printf("stats opcode 0x%04x\n", oc);
						panic("stats");
					}
					ocstat[(oc >> 8) & 0x1f]++;		// 0..31
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
				syntax(ic->addr != -1, "not in init_code");
			}
			
			if (debug || show_bin) printf("\n");
			fprintf(ofp, "\t0x%04x, 0x%04x,\n", a, op);
			out = op;
			if (write(bfd, &out, 2) != 2) sys_panic("wr");
			a += 2;
		} else
		
		// allocate space for struct
		if (tp->ttype == TT_STRUCT) {
			if (debug || show_bin) printf("%04x STRUCT %s size 0x%x/%d\n", a, tp->str, tp->num, tp->num);
			if (show_bin) {
				for (i=0; i<tp->num; i+=2) {
					fprintf(cfp, "%c\n%04x", comma, 0);
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
			errmsg("found unexpected: ");
			error = TRUE;
			token_dump(tp);
			printf("\n");
		}
	}
	
	fclose(ofp);
	close(bfd);
	
	if (gen) {
		fprintf(hfp, "\n#endif\n");
		fclose(hfp);
		fprintf(vfp, "\n`endif\n");
		fclose(vfp);
		
	}
	
	if (show_bin) {
		fprintf(cfp, ";\n");
		fclose(cfp);
	}
	
	if (error) panic("errors detected");

	printf("used %d/%d CPU RAM (%d insns remaining)\n", a/2, CPU_RAM_SIZE, CPU_RAM_SIZE - a/2);
	
	if (bad) panic("============ generated code compare errors ============\n");
	printf("\n");

	if (stats) {
		printf("insn use statistics:\n");
		int sum = 0;
		for (i = 0; i < N_OCS; i++) {
			printf("%02d: %4d %s\n", i, ocstat[i], ocname[i]);
			sum += ocstat[i];
		}
		printf("sum insns = %d\n", sum);
	}

	return 0;
}
