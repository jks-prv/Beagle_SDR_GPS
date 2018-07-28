#include "asm.h"


// debug

int curline, debug;
char *fn, *bfs, *hfs, *vfs, *cfs;

static void remove_files()
{
	char rm[256];
	sprintf(rm, "rm -f %s %s %s %s", bfs, hfs, vfs, cfs);
	system(rm);
}

void sys_panic(char *str)
{
	printf("panic\n");
	perror(str);
	remove_files();
	exit(-1);
}

void panic(char *str)
{
	errmsg(str);
	printf("\n");
	remove_files();
	exit(-1);
}

void syntax(int cond, const char *fmt, ...)
{
	if (!cond) {
	    dump_tokens("syntax", tp_start, tp_end);
        va_list ap;
        va_start(ap, fmt);
        char *buf;
		vasprintf(&buf, fmt, ap);
        va_end(ap);
		panic(buf);
	}
}

void _assert(int cond, const char *str, const char *file, int line)
{
	if(!(cond)) {
		printf("assert \"%s\" failed at %s:%d\n", str, file, line);
		panic("assert");
	}
}

void errmsg(char *str)
{
    //dump_tokens("errmsg", tp_start, tp_end);
	printf("%s:%d error: %s", fn, curline, str);
}


// string pool

#define NSTRS	8192
strs_t strs[NSTRS];
int nstrs;

#define	STRBUF	64*1024
char strbuf[STRBUF];
char *strb = strbuf;

void string_enter(char *string, char **pointer, int flags)
{
	int i;
	strs_t *s = strs;
	
	for (i=0; i<nstrs; s++, i++) {
		if (strcmp(s->str, string) == 0) { *pointer = s->str; return; }
	}

	s->str = strb; s->flags = flags; nstrs++;
	*pointer = strb; strb = stpcpy(strb, string) + 1;
}

strs_t *string_find(char *string)
{
	int i;
	strs_t *s = strs;
	
	for (i=0; i<nstrs; s++, i++) if (strcmp(s->str, string) == 0) return s;
	return 0;
}

void string_dump()
{
	int i;
	
	for (i=0; i<nstrs; i++) printf("%2d %s\n", i, strs[i].str);
}

int num_strings()
{
	return nstrs;
}


// tokens

static const char *ttype_s[] = {
    "EOL", "LABEL", "SYM", "NUM", "OPC", "PRE", "OPR", "DATA", "STRUCT", "ITER", "DEF"
};

const char *ttype(token_type_e ttype_e)
{
    return ttype_s[ttype_e];
}

void token_dump(tokens_t *tp)
{
	switch (tp->ttype) {
	
	case TT_EOL:	printf("\\n%d: ", tp->num); break;
	case TT_LABEL:	printf("%s: ", tp->str); break;
	case TT_SYM:	printf("\"%s\"%s ", tp->str, (tp->flags&TF_RET)? ".r": (tp->flags&TF_CIN)? ".cin":"");  break;
	case TT_NUM:	if (tp->flags & TF_HEX) printf("0x%x ", tp->num); else if (tp->flags & TF_FIELD) printf("%d'd%d ", tp->width, tp->num); else printf("%d ", tp->num);  break;
	case TT_OPC:	printf("[%s%s] ", tp->str, (tp->flags&TF_RET)? ".r": (tp->flags&TF_CIN)? ".cin":"");  break;
	case TT_PRE:	printf("<%s> ", tp->str);  break;
	case TT_OPR:	printf("%s ", tp->str); break;
	case TT_DATA:	printf("U%d ", tp->num*8); break;
	case TT_STRUCT:	printf("{%s} ", tp->str); break;
	case TT_ITER:	printf("<iter> "); break;
	default:		printf("UNK ttype??? "); break;
	}
}

void dump_tokens(char *pass, tokens_t *f, tokens_t *l)
{
	tokens_t *t;
	
	printf("\n--------------------------------------------------------------------------------\n");
	printf("%ld tokens at %s:\n%s ", l-f, pass, pass);
	
	for (t=f; t != l; t++) {
		token_dump(t);
		if (t->ttype == TT_EOL) printf("\n%s ", pass);
	}
	
	printf("(END)\n\n");
}

void insert(int n, tokens_t *tp, tokens_t **ep)
{
	tokens_t *t;
	
	for (t = *ep; t != (tp-n); t--) *(t+n) = *t;
	*ep = *ep+n;
}

// pullup sp..ep to start at earlier dp, adjust ep
void pullup(tokens_t *dp, tokens_t *sp, tokens_t **ep)
{
	int n = sp - dp;

	while (sp != *ep) *dp++ = *sp++;
	*ep -= n;
}


// pre-processor

preproc_t preproc[1024];

preproc_t *pre(char *str, preproc_type_e ptype)
{
	preproc_t *p;
	
	for (p=preproc; p->str; p++) {
		if (strcmp(p->str, str) == 0) {
			if ((int) ptype == -1) return p;
			if (p->ptype == ptype) return p;
		}
	}
	return 0;
}


// expressions

int def(tokens_t *tp, tokens_t **ep)
{
	tokens_t *t;
	preproc_t *p;
	strs_t *st;

	if (tp->ttype == TT_OPR && tp->num == OPR_SIZEOF) {
		tp++; syntax(tp->ttype == TT_SYM, "expected sizeof sym");
		if ((p = pre(tp->str, PT_STRUCT))) {
			if (debug) printf("sizeof %s <- %d\n", tp->str, p->size);
			pullup(tp, tp+1, ep);
			tp--; tp->ttype = TT_NUM; tp->num = p->size; tp++;
			return 1;
		}
		if (debug) printf("unknown sizeof: %s (likely fwd ref)\n", tp->str);
	}

	if (tp->ttype == TT_SYM) {
		if ((p = pre(tp->str, PT_DEF))) {
			//if (debug) printf("def: preproc %s %s\n", p->str, tp->str);
			if (debug) printf("DEF \"%s\" <- %d\n", tp->str, p->val);
			tp->ttype = TT_NUM; tp->num = p->val; return 1;
		}
		
		if ((st = string_find(tp->str)) && (st->flags & SF_DEFINED)) {
			if (debug) printf("SYM is now a defined LABEL \"%s\" <- 0x%x\n", tp->str, st->val);
			tp->ttype = TT_NUM; tp->num = st->val; return 1;
		}
	}
	
	return 0;
}

tokens_t *expr(tokens_t *tp, tokens_t **ep, int *val, int multi)
{
	tokens_t *t;
	
	def(tp, ep);
	syntax(tp->ttype == TT_NUM, "expected expr NUM, got %s", ttype(tp->ttype));
	*val = tp->num; tp++;
	while (tp->ttype != TT_EOL) {
		t = tp;
		syntax(t->ttype == TT_OPR, "expected expr operator");
		
		if (t->flags & TF_2OPR) {
			tp++; def(tp, ep);
			syntax(tp->ttype == TT_NUM, "expected expr number 2");
			switch (t->num) {
				case OPR_ADD: *val += tp->num; break;
				case OPR_SUB: *val -= tp->num; break;
				case OPR_MUL: *val *= tp->num; break;
				case OPR_DIV: *val /= tp->num; break;
				case OPR_SHL: *val <<= tp->num; break;
				case OPR_SHR: *val >>= tp->num; break;
				case OPR_AND: *val &= tp->num; break;
				case OPR_OR:  *val |= tp->num; break;
				case OPR_MAX: *val  = MAX(*val, tp->num); break;
				case OPR_MIN: *val  = MIN(*val, tp->num); break;
				default: syntax(0, "bad expr operator"); break;
			}
		} else
		if (t->flags & TF_1OPR) {
			switch (t->num) {
				case OPR_INC: *val += 1; break;
				case OPR_DEC: *val += 1; break;
				case OPR_NOT: *val = (~*val) & 0xffff; break;
				default: syntax(0, "bad expr operator 2"); break;
			}
		} else {
			syntax(0, "bad expr operator 3");
		}
		if (!multi) break;
		tp++;
	}
	
	return tp;
}

int arg_match(tokens_t *body, tokens_t *args, int nargs)
{
	int i;
	
	if (body->ttype == TT_SYM) {
		for (i=0; i<nargs; args++, i++) {
			if (strcmp(body->str, args->str) == 0) return i;
		}
	}

	return -1;
}

tokens_t mexp[1024];

int exp_macro(tokens_t **dp, tokens_t **to)
{
	int i, j;
	tokens_t *tp=*dp, *t, *params, *pt, *te;
	preproc_t *p;
	
	if (tp->ttype == TT_SYM) {
		if (!(p = pre(tp->str, PT_MACRO))) return 0;
		if (debug) printf("expand MACRO %s\n", tp->str);
		params = tp+1;

		te = mexp;
		for (t = p->body; !(t->ttype == TT_PRE && t->num == PP_ENDM); t++) {
			if ((i = arg_match(t, p->args, p->nargs)) != -1) {
				pt = params;
				for (j=0; j < i; j++) {		// have params to skip
					if (pt->ttype == TT_OPR && pt->num == OPR_PAREN) {
						pt++;
						while (!(pt->ttype == TT_OPR && pt->num == OPR_PAREN))
							pt++;
					}
					pt++;
				}
				if (pt->ttype == TT_OPR && pt->num == OPR_PAREN) {
					pt++;
					while (!(pt->ttype == TT_OPR && pt->num == OPR_PAREN))
						*te++ = *pt++;
					//te--;
				} else {
					*te++ = *pt;
				}
			} else {
				*te++ = *t;
			}
		}
		
		// find the end of the macro invocation and remove it
		for (i=0, t=tp; t->ttype != TT_EOL; t++) i++;
		i++; pullup(tp, tp+i, to);

		// insert the expanded macro
		i = (int) (te-mexp);
		te = mexp;
		insert(i, tp, to);
		for (j=0; j<i; j++) *tp++ = *te++;
		return 1;
	}
	
	return 0;
}


