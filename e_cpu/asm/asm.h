#ifndef _ASM_H_
#define _ASM_H_

#include "../types.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>

extern int curline, debug;
extern char *fn, *bfs, *hfs, *vfs, *cfs;

#define	assert(cond) _assert(cond, # cond, __FILE__, __LINE__);

void sys_panic(char *str);
void panic(char *str);
void syntax(int cond, const char *fmt, ...);
void _assert(int cond, const char *str, const char *file, int line);
void errmsg(char *str);


// string pool

typedef struct {
	char *str;
	int flags, val;
} strs_t;

#define	SF_LABEL	0x01
#define	SF_DEFINED	0x02

void string_enter(char *string, char **pointer, int flags);
strs_t *string_find(char *string);
void string_dump();
int num_strings();


// tokens

typedef enum {
	TT_EOL, TT_LABEL, TT_SYM, TT_NUM, TT_OPC, TT_PRE, TT_OPR, TT_DATA, TT_STRUCT, TT_ITER, TT_DEF
} token_type_e;

#define	TF_RET		0x0001
#define	TF_CIN		0x0002
#define	TF_HEX		0x0004
#define	TF_DOT_H	0x0008
#define	TF_DOT_VP	0x0010
#define	TF_DOT_VB	0x0020
#define	TF_FIELD	0x0040
#define	TF_2OPR		0x0080
#define	TF_1OPR		0x0100

typedef struct {
	token_type_e ttype;
	char *str;
	int num, width;
	int flags;
} tokens_t;

tokens_t *tp_start, *tp_end;

const char *ttype(token_type_e ttype_e);
void token_dump(tokens_t *tp);
void dump_tokens(char *pass, tokens_t *f, tokens_t *l);
void insert(int n, tokens_t *tp, tokens_t **ep);
void pullup(tokens_t *dp, tokens_t *sp, tokens_t **ep);


// pre-processor

// pre-processor directives
#define	PP_DEF		0
#define	PP_MACRO	1
#define	PP_ENDM		2
#define	PP_REPEAT	3
#define	PP_ENDR		4
#define	PP_STRUCT	5
#define	PP_ENDS		6
#define	PP_FACTOR	7
#define	PP_ENDF		8
#define	PP_FCALL	9
#define	PP_IF		10
#define	PP_ELSE		11
#define	PP_ENDIF	12

// pre-processor operators
#define	OPR_INC		0
#define	OPR_ADD		1
#define	OPR_DEC		2
#define	OPR_SUB		3
#define	OPR_MUL		4
#define	OPR_DIV		5
#define	OPR_SHL		6
#define	OPR_SHR		7
#define	OPR_AND		8
#define	OPR_OR		9
#define	OPR_NOT		10
#define	OPR_SIZEOF	11
#define	OPR_CONCAT	12
#define	OPR_LABEL	13
#define	OPR_PAREN	14
#define	OPR_MAX 	15
#define	OPR_MIN 	16

typedef enum {
	PT_DEF, PT_STRUCT, PT_MEMBER, PT_MACRO
} preproc_type_e;

typedef struct {
	char *str;
	preproc_type_e ptype;
	
	// DEF
	int val, width;
	int flags;
	
	// STRUCT
	int size;
	
	// MEMBER
	char *dtype;
	int ninst, offset;
	
	// MACRO
	tokens_t *args, *body;
	int nargs, nbody;
} preproc_t;

extern preproc_t preproc[1024];

preproc_t *pre(char *str, preproc_type_e ptype);


// expressions

int def(tokens_t *tp, tokens_t **ep);
tokens_t *expr(tokens_t *tp, tokens_t **ep, int *val, int multi);
int arg_match(tokens_t *body, tokens_t *args, int nargs);
int exp_macro(tokens_t **dp, tokens_t **to);

#endif
