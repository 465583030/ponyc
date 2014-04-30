grammar pony2;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  :  (use | class_ | typedecl)*
  ;

use
  :  'use' (ID assign)? STRING
  ;

class_
  :  ('actor' | 'class' | 'trait') ID type_params? raw_cap? ('is' types)? member*
  ;

member
  :  ('var' | 'val') ID oftype? (assign seq)? // field
//  |  'new' '?'? ID type_params? params body? // constructor
//  |  'fun' raw_cap '?'? ID type_params? params oftype? body? // function
  |  'new' ID type_params? params '?'? body? // constructor
  |  'fun' raw_cap ID type_params? params oftype? '?'? body? // function
  |  'be' ID type_params? params body? // behaviour
  ;

typedecl
  :  'type' ID type_params? (':' (base_type | type_expr))?
  ;

oftype
  :  ':' type
  ;

types
  :  type (',' type)*
  ;

type
  :  (base_type | type_expr) '^'? // ephemeral types
  ;

type_expr
  :  '(' base_type (typeop base_type)* ')' cap? // ADT or tuple
  ;

typeop
  :  '|' | '&' | ',' // union, intersection, tuple
  ;

base_type
  :  ID ('.' ID)* type_args? cap? // nominal type
  |  '{' fun_type* '}' cap? // structural type
  |  typedecl // nested type definition
  ;

// could make structural types into traits by supplying bodies here
fun_type
//  :  'fun' cap '?'? ID? '(' types? ')' oftype?
  :  'fun' raw_cap ID? '(' types? ')' oftype? '?'?
  |  'be' ID? '(' types? ')' // FIX: need this?
  ;

type_params
  :  '[' param (',' param)* ']'
  ;

type_args
  :  '[' type (',' type)* ']'
  ;

cap
//  :  ':' (raw_cap | ID)
  :  (raw_cap | ID) ('.' (raw_cap | ID))*
  ;

raw_cap
//  :  ('iso' | 'trn' | 'var' | 'val' | 'box' | 'tag')
  :  ('iso' | 'trn' | 'mut' | 'imm' | 'box' | 'tag')
  ;

params
  :  '(' (param (',' param)*)? ')'
  ;

param
  :  ID oftype? (assign seq)?
  ;

body
  :  '=>' seq
  ;

seq
  :  expr (';' seq)?
  ;

expr
  :  binary
  |  'return' binary
  |  'break' binary
  |  'continue'
  |  'undef'
  ;

binary
  :  term (binop binary)?
  ;

term
  :  local
  |  control
  |  postfix
  |  unop term
  |  'consume' ID ('.' ID)*
  |  'recover' term
  ;

local
  :  ('var' | 'val') idseq oftype?
  ;

control
  :  'if' seq 'then' seq ('elseif' seq 'then' seq)* ('else' seq)? 'end'
  |  'match' seq case* ('else' seq)? 'end'
  |  'while' seq 'do' seq ('else' seq)? 'end'
  |  'do' seq 'while' seq 'end'
  |  'for' idseq oftype? 'in' seq 'do' seq ('else' seq)? 'end'
  |  'try' seq ('else' seq)? ('then' seq)? 'end'
  ;

case
  :  '|' seq? ('as' idseq oftype)? ('where' seq)? body?
  ;

postfix
  :  atom
  (  '.' (ID | INT) // member or tuple component
  |  '!' ID // partial application, syntactic sugar
  |  type_args // type arguments
  |  tuple // method arguments
  )*
  ;

atom
  :  INT
  |  FLOAT
  |  STRING
  |  ID
  |  tuple
  |  array
  |  object
  ;

idseq
  :  ID | '(' ID (',' ID)* ')'
  ;

tuple
  :  '(' positional? named? ')'
  ;

array
  :  '[' positional? named? ']'
  ;

object
  :  '{' ('is' types)? member* '}'
  ;

positional
  :  seq (',' seq)*
  ;

named
  :  'where' term assign seq (',' term assign seq)*
  ;

unop
  :  'not' | '-'
  ;

binop
  :  'and' | 'or' | 'xor' // logic
  |  '+' | '-' | '*' | '/' | '%' // arithmetic
  |  '<<' | '>>' // shift
  |  'is' | '==' | '!=' | '<' | '<=' | '>=' | '>' // comparison
  |  assign
  ;

assign
  :  '='
  ;

// Lexer

ID
  :  (LETTER | '_') (LETTER | DIGIT | '_' | '\'')*
  ;

INT
  :  DIGIT+
  |  '0' 'x' HEX+
  |  '0' 'o' OCTAL+
  |  '0' 'b' BINARY+
  ;

FLOAT
  :  DIGIT+ ('.' DIGIT+)? EXP?
  ;

LINECOMMENT
  :  '//' ~('\n' | '\r')* '\r'? '\n' {$channel=HIDDEN;}
  ;

NESTEDCOMMENT
  :  '/*' ( ('/*') => NESTEDCOMMENT | ~'*' | '*' ~'/')* '*/'
  ;

WS
  :  ' ' | '\t' | '\r' | '\n'
  ;

STRING
  :  '"' ( ESC | ~('\\'|'"') )* '"'
  ;

fragment
EXP
  :  ('e' | 'E') ('+' | '-')? DIGIT+
  ;

fragment
LETTER
  :  'a'..'z' | 'A'..'Z'
  ;

fragment
BINARY
  :  '0'..'1'
  ;

fragment
OCTAL
  :  '0'..'7'
  ;

fragment
DIGIT
  :  '0'..'9'
  ;

fragment
HEX
  :  DIGIT | 'a'..'f' | 'A'..'F'
  ;

fragment
ESC
  :  '\\' ('a' | 'b' | 'f' | 'n' | 'r' | 't' | 'v' | '\"' | '\\' | '0')
  |  HEX_ESC
  |  UNICODE_ESC
  |  UNICODE2_ESC
  ;

fragment
HEX_ESC
  :  '\\' 'x' HEX HEX
  ;

fragment
UNICODE_ESC
  :  '\\' 'u' HEX HEX HEX HEX
  ;

fragment
UNICODE2_ESC
  :  '\\' 'U' HEX HEX HEX HEX HEX HEX
  ;
