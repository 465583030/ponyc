grammar pony;

options
{
  output = AST;
  k = 1;
}

// Parser

module
  :  (use | class_ | alias)*
  ;

use
  :  'use' STRING ('as' ID)?
  ;

// FIX: 'of type' not in code yet, use to constrain This for traits
class_
  :  ('actor' | 'class' | 'trait') ID type_params? mode ('of' type)? ('private' | 'infer')? ('is' types)? member*
  ;

member
  :  ('var' | 'val') ID (',' ID)* oftype // FIX: multiple declaration not in code yet
  |  ('fun' | 'msg') 'private'? 'throw'? ID type_params? mode params oftype? ('=' seq)?
  ;

alias
  :  'alias' ID type_params? oftype
  ;

types
  :  type (',' type)*
  ;

type
  :  base_type
  |  '(' base_type ('|' base_type)* ')'
  ;

base_type
  :  ID ('.' ID)? type_args? mode
  |  'fun' 'throw'? mode '(' types? ')' oftype?
  ;

type_params
  :  '[' param (',' param)* ']'
  ;

type_args
  :  '[' type (',' type)* ']'
  ;

mode
  :  ('{' base_mode ('|' base_mode)* '}')? ('->' ('this' | ID))?
  ;

base_mode
  :	 'iso' | 'var' | 'val' | 'tag' | 'this' | ID
  ;

params
  :  '(' (param (',' param)*)? ')'
  ;

// FIX: this could be used in code to allow multiple declaration, ie (a, b: I32, c, d: String)
param
  :  ID oftype? ('=' expr)?
  ;

oftype
  :  ':' type
  ;

args
  :  '(' (arg (',' arg)*)? ')'
  ;

arg
  :  expr ('->' ID)?
  ;

/* FIX: would be nice to be able to say:
a = if not match ...
a = not if ... then ... else ...
a = if ... then ... else ... + ...

those all require the clause to have a terminator that can't be part of an expression
match, try, if already have one if there is no else clause
fun, while, do, for don't
*/
expr
  :  ('var' | 'val') ID oftype? '=' expr
     // return the value of binary, not expr. a = b returns a, b = a = b is a swap
  |  binary ('=' expr)?
     // expr is in a new scope
  |  'fun' 'throw'? mode params oftype? '=' expr
     // without else clause: if e1 then (e2; None) else None
     // whole thing is a scope, each clause is a subscope
  |  'if' seq 'then' expr ('else' expr | 'end')
     // whole thing is a scope, each case seq is a scope
  |  'match' seq ('|' binary? ('as' ID oftype)? ('if' binary)? ('=' seq)?)* ('else' expr | 'end') // FIX: optional seq, else clause not yet in code
     // value is None, whole thing is a scope
  |  'while' seq 'do' expr
     // value is None, whole thing is a scope
  |  'do' seq 'while' expr
     // (var x = (e1).iterator(); while x.has_next() do (val id = x.next(); e2))
  |  'for' ID oftype? 'in' seq 'do' expr
     // only valid in a while loop, exits the loop
  |  'break'
     // only valid in a while loop, short circuits the loop
  |  'continue'
     // short circuits the function
  |  'return' expr
     // each seq is a scope
  |  'try' seq ('else' seq)? ('then' expr | 'end')
  |  'throw'
  ;

seq
  :  expr (';' expr)*
  ;

binary
  :  unary (binop unary)*
  ;

unary
  :  unop unary
  |  postfix
  ;

postfix
  :  primary
  (  '.' ID
  |  type_args
  |  args
  )*
  ;

primary
  :  'this'
  |  INT
  |  FLOAT
  |  STRING
  |  ID
  |  '(' seq ')'
  ;

unop
  :  'not' | '-'
  ;

binop
  :  'and' | 'or' | 'xor'
  |  '+' | '-' | '*' | '/' | '%'
  |  '<<' | '>>'
  |  '==' | '!=' | '<' | '<=' | '>=' | '>'
  ;

// Lexer

ID
  :  (LETTER | '_') (LETTER | DIGIT | '_' | '\'')*
  ;

INT
  :  DIGITS
  |  '0' 'x' (HEX | ',')*
  |  '0' 'b' ('0' | '1' | ',')*
  ;

FLOAT
  :  DIGITS ('.' DIGITS)? EXP?
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
  :  ('e' | 'E') ('+' | '-')? DIGITS
  ;

fragment
LETTER
  :  'a'..'z' | 'A'..'Z'
  ;

fragment
DIGITS
  :  DIGIT (DIGIT | ',')*
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
