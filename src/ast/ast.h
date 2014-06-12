#ifndef AST_H
#define AST_H

#include "lexer.h"
#include <stdbool.h>

/*

PROGRAM: {PACKAGE}
symtab: path -> PACKAGE

PACKAGE: {MODULE}
data: path
symtab: ID -> TYPE | TRAIT | CLASS | ACTOR

MODULE: {USE} {TYPE | TRAIT | CLASS | ACTOR}
data: source
symtab: ID -> PACKAGE | TYPE | TRAIT | CLASS | ACTOR

USE: PATH [ID]

TYPE: ID [TYPEPARAMS] [CAP] [TYPES] MEMBERS
TRAIT: ID [TYPEPARAMS] [CAP] [TYPES] MEMBERS
CLASS: ID [TYPEPARAMS] [CAP] [TYPES] MEMBERS
ACTOR: ID [TYPEPARAMS] [CAP] [TYPES] MEMBERS
data: typechecking state
symtab: ID -> TYPEPARAM | VAR | VAL | NEW | FUN | BE

MEMBERS: {FVAR | FLET | NEW | FUN | BE}

FVAR: ID [TYPEDEF] [SEQ]
FLET: ID [TYPEDEF] [SEQ]

NEW: NONE ID [TYPEPARAMS] [PARAMS | TYPES] NONE [QUESTION] [SEQ]
BE: NONE ID [TYPEPARAMS] [PARAMS | TYPES] NONE NONE [SEQ]
FUN: CAP ID [TYPEPARAMS] [PARAMS | TYPES] [TYPEDEF] [QUESTION] [SEQ]
symtab: ID -> TYPEPARAM | PARAM

TYPEPARAMS: {TYPEPARAM}

TYPEPARAM: ID [TYPEDEF] [SEQ]

TYPES: {TYPEDEF}

type: (UNIONTYPE | ISECTTYPE | TUPLETYPE | NOMINAL | STRUCTURAL)
TYPEDEF: type [CAP] [HAT]

CAP: {ID | ISO | TRN | REF | VAL | BOX | TAG}

UNIONTYPE: (TYPEDEF | type) (TYPEDEF | type)
ISECTTYPE: (TYPEDEF | type) (TYPEDEF | type)
TUPLETYPE: (TYPEDEF | type) (TYPEDEF | type)

NOMINAL: [ID] ID [TYPEARGS]
data: nominal_def, if it has been resolved

TYPEARGS: {TYPEDEF}

STRUCTURAL: {NEW | FUN | BE}

PARAMS: {PARAM}

PARAM: ID [TYPEDEF] [SEQ]

IDSEQ: {ID}

SEQ: {expr}
symtab: ID -> VAR | VAL

RAWSEQ: {expr}

expr
----
term: local | prefix | postfix | control | infix

CONTINUE

ERROR

BREAK: infix

RETURN: infix

infix
-----
MULTIPLY term term
DIVIDE term term
MOD term term
PLUS term term
MINUS term term
LSHIFT term term
RSHIFT term term
LT term term
LE term term
GE term term
GT term term
IS term term
EQ term term
NE term term
IS term term
ISNT term term
AND term term
XOR term term
OR term term
ASSIGN term term

local
-----
VAR: IDSEQ [TYPEDEF]
LET: IDSEQ [TYPEDEF]

prefix
------
CONSUME: term
RECOVER: term
NOT: term
MINUS: term

postfix
-------
atom
DOT postfix (ID | INT)
BANG postfix INT
QUALIFY postfix TYPEARGS
CALL postfix TUPLE

control
-------
IF: RAWSEQ SEQ [SEQ]
symtab: ID -> VAR | VAL

MATCH: RAWSEQ CASES [SEQ]

CASES: {CASE}

CASE: [SEQ] [AS] [SEQ] [SEQ]
symtab: ID -> VAR | VAL

AS: IDSEQ TYPEDEF

WHILE: RAWSEQ SEQ [SEQ]
symtab: ID -> VAR | VAL

REPEAT: RAWSEQ SEQ
symtab: ID -> VAR | VAL

FOR: IDSEQ [TYPEDEF] SEQ SEQ [SEQ]

TRY: SEQ [SEQ] [SEQ]

atom
----
TUPLE: [POSITIONALARGS] [NAMEDARGS]

ARRAY: [POSITIONALARGS] [NAMEDARGS]

OBJECT: [TYPES] MEMBERS

POSITIONALARGS: {SEQ}

NAMEDARGS: {NAMEDARG}

NAMEDARG: term SEQ

THIS
ID

INT
FLOAT
STRING

*/

typedef struct ast_t ast_t;

ast_t* ast_new(token_id id, size_t line, size_t pos, void* data);
ast_t* ast_token(token_t* t);
ast_t* ast_newid(ast_t* previous, const char* id);
ast_t* ast_dup(ast_t* ast);
void ast_attach(ast_t* ast, void* data);
void ast_scope(ast_t* ast);
void ast_setid(ast_t* ast, token_id id);

token_id ast_id(ast_t* ast);
size_t ast_line(ast_t* ast);
size_t ast_pos(ast_t* ast);
void* ast_data(ast_t* ast);
const char* ast_name(ast_t* ast);
ast_t* ast_type(ast_t* ast);

ast_t* ast_nearest(ast_t* ast, token_id id);
ast_t* ast_parent(ast_t* ast);
ast_t* ast_child(ast_t* ast);
ast_t* ast_childidx(ast_t* ast, size_t idx);
ast_t* ast_childlast(ast_t* ast);
ast_t* ast_sibling(ast_t* ast);
size_t ast_index(ast_t* ast);
size_t ast_childcount(ast_t* ast);

void* ast_get(ast_t* ast, const char* name);
bool ast_set(ast_t* ast, const char* name, void* value);
bool ast_merge(ast_t* dst, ast_t* src);

void ast_add(ast_t* parent, ast_t* child);
ast_t* ast_pop(ast_t* ast);
void ast_append(ast_t* parent, ast_t* child);
void ast_swap(ast_t* prev, ast_t* next);
void ast_reverse(ast_t* ast);
void ast_print(ast_t* ast, size_t width);
void ast_free(ast_t* ast);

void ast_error(ast_t* ast, const char* fmt, ...)
  __attribute__ ((format (printf, 2, 3)));

typedef bool (*ast_visit_t)(ast_t* ast, int verbose);

bool ast_visit(ast_t* ast, ast_visit_t pre, ast_visit_t post, int verbose);

#endif
