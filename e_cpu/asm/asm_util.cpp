#include "asm.h"


// debug

int curline, debug;
char *fn, *bfs, *cfs, *hfs, *vfs, *efs;

static void remove_files()
{
	char rm[256];
	if (bfs || hfs || vfs || efs) {
        if (!bfs) bfs = (char *) "";
        if (!hfs) hfs = (char *) "";
        if (!vfs) vfs = (char *) "";
        if (!efs) efs = (char *) "";
	    snprintf(rm, sizeof(rm), "rm -f %s %s %s %s", bfs, hfs, vfs, efs);
	    system(rm);
	}
}

char *token(tokens_t *tp);

static void _errmsg(char *str, tokens_t *t = NULL)
{
    //dump_tokens("errmsg", tp_start, tp_end);
    if (t == NULL)
	    printf("error: %s", str);
	else {
        for (; t->ttype != TT_EOL; t++) {
            //printf("DBG %s %s ifl=%d %s:%d\n", ttype(t->ttype), token(t), t->ifl, ifiles_list[t->ifl], t->num-1);
            ;
        }
        //printf("DBG %s %s ifl=%d %s:%d\n", ttype(t->ttype), token(t), t->ifl, ifiles_list[t->ifl], t->num-1);
	    printf("%s:%d error: %s", ifiles_list[t->ifl], t->num-2, str);
	}
}

void errmsg(const char *str, tokens_t *t)
{
    _errmsg((char *) str, t);
}

void sys_panic(const char *str)
{
	printf("panic: ");
	perror(str);
	remove_files();
	exit(-1);
}

static void _panic(char *str, tokens_t *t = NULL)
{
	_errmsg(str, t);
	printf("\n");
	remove_files();
	exit(-1);
}

void panic(const char *str, tokens_t *t)
{
    _panic((char *) str, t);
}

void syntax(int cond, tokens_t *tp, const char *fmt, ...)
{
	if (!cond) {
        tokens_t *t;
        printf("\ntokens: ");

        // find first token
        for (t = tp; t->ttype != TT_EOL; t--)
            ;

        // dump until EOL
        for (t = t+1; t->ttype != TT_EOL; t++) {
            token_dump(t);
        }
        
        printf("\n");
        va_list ap;
        va_start(ap, fmt);
        char *buf;
		vasprintf(&buf, fmt, ap);
        va_end(ap);
		_panic(buf, t);
	}
}

void syntax2(int cond, const char *fmt, ...)
{
	if (!cond) {
	    dump_tokens("syntax2", tp_start, tp_end);
        va_list ap;
        va_start(ap, fmt);
        char *buf;
		vasprintf(&buf, fmt, ap);
        va_end(ap);
		_panic(buf);
	}
}

void _assert(int cond, const char *str, const char *file, int line)
{
	if(!(cond)) {
		printf("assert \"%s\" failed at %s:%d\n", str, file, line);
		panic("assert");
	}
}

int count_ones(u4_t v)
{
	int ones = 0;
    for (int bit = 31; bit >= 0; bit--)
        if (v & (1 << bit)) ones++;
	return ones;
}


// strings

char *str_ends_with(char *s, const char *cs)
{
    int slen = strlen(cs);
    char *sp = s + strlen(s) - slen;
    return (strncmp(sp, cs, slen) == 0)? sp : NULL;
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

char *token(tokens_t *tp)
{
    char *s;
    
	switch (tp->ttype) {
	
	case TT_EOL:	asprintf(&s, "\\n (%s:%d)", ifiles_list[tp->ifl], tp->num); break;
	case TT_LABEL:	asprintf(&s, "%s:", tp->str); break;
	case TT_SYM:	asprintf(&s, "\"%s\"%s", tp->str, (tp->flags&TF_RET)? ".r": (tp->flags&TF_CIN)? ".cin":"");  break;
	case TT_NUM:	if (tp->flags & TF_HEX) asprintf(&s, "0x%x", tp->num); else if (tp->flags & TF_FIELD) asprintf(&s, "%d'd%d", tp->width, tp->num); else asprintf(&s, "%d", tp->num);  break;
	case TT_OPC:	asprintf(&s, "[%s%s]", tp->str, (tp->flags&TF_RET)? ".r": (tp->flags&TF_CIN)? ".cin":"");  break;
	case TT_PRE:	asprintf(&s, "<%s>", tp->str);  break;
	case TT_OPR:	asprintf(&s, "%s", tp->str); break;
	case TT_DATA:	asprintf(&s, "U%d", tp->num*8); break;
	case TT_STRUCT:	asprintf(&s, "{%s}", tp->str); break;
	case TT_ITER:	asprintf(&s, "<iter>"); break;
	case TT_FILE:	asprintf(&s, "file: %s", tp->str); break;
	default:		asprintf(&s, "UNK ttype???"); break;
	}
	
	return s;
}

void token_dump(tokens_t *tp)
{
    char *s = token(tp);
    printf("%s ", s);
    free(s);
}

void dump_tokens(const char *pass, tokens_t *f, tokens_t *l)
{
	tokens_t *t;
	
	printf("\n--------------------------------------------------------------------------------\n");
	printf("%ld tokens at %s:\n%s ", (long) (l-f), pass, pass);
	
	for (t=f; t != l; t++) {
		token_dump(t);
		if (t->ttype == TT_EOL) printf("\n%s ", pass);
	}
	
	printf("(END)\n\n");
}

void dump_tokens_until_eol(const char *id, tokens_t *f)
{
    printf("%s ", id);
	tokens_t *t = f;
	while (t->ttype != TT_EOL) {
        token_dump(t);
        t++;
	}
	printf("\n");
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
			if (ptype == PT_NONE) return p;
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
		tp++; syntax(tp->ttype == TT_SYM, tp, "expected sizeof sym");
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

// [NUM OPR] [NUM OPR NUM] ... -> NUM, returns value
// destructive (does pullups)
tokens_t *expr_collapse(tokens_t *t, tokens_t **ep, int *val)
{
	tokens_t *tp = t, *t1 = t+1, *t2 = t+2, *t3 = t+3;
    int e_val;

    // NUM OPR NUM -> NUM
    if (tp->ttype == TT_NUM && t1->flags & TF_2OPR && t2->ttype == TT_NUM) {
        if (debug) printf("COLLAPSE %d %s %d -> ", tp->num, t1->str, t2->num);
        expr(tp, ep, &e_val, 0); tp->num = e_val;
        if (debug) printf("%d\n", e_val);
        pullup(t1, t3, ep);
    } else

    // NUM OPR -> NUM
    if (tp->ttype == TT_NUM && t1->flags & TF_1OPR) {
        if (debug) printf("COLLAPSE %d %s -> ", tp->num, t1->str);
        expr(tp, ep, &e_val, 0); tp->num = e_val;
        if (debug) printf("%d\n", e_val);
        pullup(t1, t2, ep);
    } else {
        return NULL;
    }
    
    *val = tp->num;
    return tp;
}

// [NUM OPR] [NUM OPR NUM] ... -> nil, returns value
// destructive (does pullups)
tokens_t *cond(tokens_t *t, tokens_t **ep, int *val)
{
	tokens_t *tp = t;
	preproc_t *p;
	
	// replace all DEFs with resolved NUMs
	while (tp->ttype != TT_EOL) {
	    def(tp, ep);
        tp++;
	}
	
	// eval condition
    int e_val;
	tp = t;
	while ((tp+1)->ttype != TT_EOL) {

		// NUM OPR NUM -> NUM or NUM OPR -> NUM
		if (debug) dump_tokens_until_eol("eval:", tp);
		if (expr_collapse(tp, ep, val) != NULL) {
		    continue;
		}
		
        syntax(0, t-1, "expected \"NUM OPR NUM\" or \"NUM OPR\"");
	}
	
    if (debug) printf("COND final = %d\n", tp->num);
    *val = tp->num;
    return tp+1;
}

// [NUM OPR] [NUM OPR NUM] ... -> (no change), returns value
// not destructive (no pullups)
tokens_t *expr(tokens_t *tp, tokens_t **ep, int *val, int multi, tokens_t *ltp)
{
	tokens_t *t;
	
	def(tp, ep);
	syntax(tp->ttype == TT_NUM, tp, "expected expr NUM, got %s (%s)", ttype(tp->ttype), token(tp));
	*val = tp->num; tp++;
	while (tp->ttype != TT_EOL && (ltp == NULL || tp != ltp)) {
		t = tp;
		syntax(t->ttype == TT_OPR, t, "expected expr OPR");
		
		if (t->flags & TF_2OPR) {
			tp++; def(tp, ep);
			syntax(tp->ttype == TT_NUM, tp, "expected expr NUM");
			switch (t->num) {
				case OPR_ADD:   *val += tp->num; break;
				case OPR_SUB:   *val -= tp->num; break;
				case OPR_MUL:   *val *= tp->num; break;
				case OPR_DIV:   *val /= tp->num; break;
				case OPR_SHL:   *val <<= tp->num; break;
				case OPR_SHR:   *val >>= tp->num; break;
				case OPR_LAND:  *val  = *val && tp->num; break;
				case OPR_AND:   *val &= tp->num; break;
				case OPR_LOR:   *val  = *val || tp->num; break;
				case OPR_OR:    *val |= tp->num; break;
				case OPR_EQ:    *val  = *val == tp->num; break;
				case OPR_NEQ:   *val  = *val != tp->num; break;
				case OPR_MAX:   *val  = MAX(*val, tp->num); break;
				case OPR_MIN:   *val  = MIN(*val, tp->num); break;
				default: syntax(0, t, "unknown expr 2-arg OPR"); break;
			}
		} else
		if (t->flags & TF_1OPR) {
			switch (t->num) {
				case OPR_INC: *val += 1; break;
				case OPR_DEC: *val -= 1; break;
				case OPR_NOT: *val = (~*val) & 0xffff; break;   // NB: currently done postfix
				default: syntax(0, t, "unknown expr 1-arg OPR"); break;
			}
		} else {
			syntax(0, t, "expected expr OPR");
		}
		if (!multi) break;
		tp++;
	}
	
	return tp;
}

// [NUM OPR] [NUM OPR NUM] ... -> NUM, returns value
// destructive (does pullups)
tokens_t *expr_parens(tokens_t *t, tokens_t **ep, int *val)
{
	tokens_t *tp = t;

    syntax(tp->ttype == TT_OPR && tp->num == OPR_OPEN, tp, "expected \"(\"");
    tp++;
    
	while (!(tp->ttype == TT_OPR && tp->num == OPR_CLOSE)) {

		// NUM OPR NUM -> NUM or NUM OPR -> NUM
		if (expr_collapse(tp, ep, val) != NULL) {
		    continue;
		}
		
        syntax(0, t-1, "expected \"NUM OPR NUM\" or \"NUM OPR\"");
	}
	
    if (debug) printf("PARENS final = %d\n", tp->num);
    *val = tp->num;
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
					if (pt->ttype == TT_OPR && pt->num == OPR_OPEN) {
						pt++;
						while (!(pt->ttype == TT_OPR && pt->num == OPR_CLOSE))
							pt++;
					}
					pt++;
				}
				if (pt->ttype == TT_OPR && pt->num == OPR_OPEN) {
					pt++;
					while (!(pt->ttype == TT_OPR && pt->num == OPR_CLOSE))
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


