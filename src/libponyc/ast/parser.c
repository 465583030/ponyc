#include "parserapi.h"
#include "parser.h"
#include <stdio.h>

// Forward declarations
DECL(type);
DECL(rawseq);
DECL(seq);
DECL(assignment);
DECL(term);
DECL(infix);
DECL(idseqmulti);
DECL(members);


/* Precedence
 *
 * We do not support precedence of infix operators, since that leads to many
 * bugs. Instead we require the use of parentheses to disambiguiate operator
 * interactions. This is checked in the parse fix pass, so we treat all infix
 * operators as having the same precedence during parsing.
 *
 * Overall, the precedences built into the below grammar are as follows.
 *
 * Value operators:
 *  postfix (eg . call) - highest precedence, most tightly binding
 *  prefix (eg not consume)
 *  infix (eg + <<)
 *  assignment (=) - right associative
 *  sequence (consecutive expressions)
 *  tuple elements (,) - lowest precedence
 *
 * Type operators:
 *  viewpoint (->) - right associative, highest precedence
 *  infix (& |)
 *  tuple elements (,) - lowest precedence
*/


// Parse rules

// type {COMMA type}
DEF(types);
  AST_NODE(TK_TYPES);
  RULE("type", type);
  WHILE(TK_COMMA, RULE("type", type));
  DONE();

// ID COLON type [ASSIGN infix]
DEF(param);
  AST_NODE(TK_PARAM);
  TOKEN("name", TK_ID);
  SKIP(NULL, TK_COLON);
  RULE("parameter type", type);
  IF(TK_ASSIGN, RULE("default value", infix));
  DONE();

// ELLIPSIS
DEF(ellipsis);
  TOKEN(NULL, TK_ELLIPSIS);
  DONE();

// ID [COLON type] [ASSIGN type]
DEF(typeparam);
  AST_NODE(TK_TYPEPARAM);
  TOKEN("name", TK_ID);
  IF(TK_COLON, RULE("type constraint", type));
  IF(TK_ASSIGN, RULE("default type", type));
  DONE();

// param {COMMA param}
DEF(params);
  AST_NODE(TK_PARAMS);
  RULE("parameter", param, ellipsis);
  WHILE(TK_COMMA, RULE("parameter", param, ellipsis));
  DONE();

// LSQUARE typeparam {COMMA typeparam} RSQUARE
DEF(typeparams);
  AST_NODE(TK_TYPEPARAMS);
  SKIP(NULL, TK_LSQUARE, TK_LSQUARE_NEW);
  RULE("type parameter", typeparam);
  WHILE(TK_COMMA, RULE("type parameter", typeparam));
  SKIP(NULL, TK_RSQUARE);
  DONE();

// LSQUARE type {COMMA type} RSQUARE
DEF(typeargs);
  AST_NODE(TK_TYPEARGS);
  SKIP(NULL, TK_LSQUARE);
  RULE("type argument", type);
  WHILE(TK_COMMA, RULE("type argument", type));
  SKIP(NULL, TK_RSQUARE);
  DONE();

// ID [DOT ID] [typeargs] [ISO | TRN | REF | VAL | BOX | TAG]
// [EPHEMERAL | BORROWED]
DEF(nominal);
  AST_NODE(TK_NOMINAL);
  TOKEN("name", TK_ID);
  IFELSE(TK_DOT,
    TOKEN("name", TK_ID),
    AST_NODE(TK_NONE);
    REORDER(1, 0);
  );
  OPT RULE("type arguments", typeargs);
  OPT TOKEN("capability", TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  OPT TOKEN(NULL, TK_EPHEMERAL, TK_BORROWED);
  DONE();

// PIPE type
DEF(uniontype);
  INFIX_BUILD();
  AST_NODE(TK_UNIONTYPE);
  SKIP(NULL, TK_PIPE);
  RULE("type", type);
  DONE();

// AMP type
DEF(isecttype);
  INFIX_BUILD();
  AST_NODE(TK_ISECTTYPE);
  SKIP(NULL, TK_AMP);
  RULE("type", type);
  DONE();

// type {uniontype | isecttype}
DEF(infixtype);
  RULE("type", type);
  SEQ("type", uniontype, isecttype);
  DONE();

// DONTCARE
DEF(dontcare);
  TOKEN(NULL, TK_DONTCARE);
  DONE();

// (infixtype | dontcare) (COMMA (infixtype | dontcare))*
DEF(tupletype);
  INFIX_BUILD();
  TOKEN(NULL, TK_COMMA);
  MAP_ID(TK_COMMA, TK_TUPLETYPE);
  RULE("type", infixtype, dontcare);
  WHILE(TK_COMMA, RULE("type", infixtype, dontcare));
  DONE();

// (LPAREN | LPAREN_NEW) tupletype RPAREN
DEF(groupedtype);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  RULE("type", infixtype, dontcare);
  OPT_NO_DFLT RULE("type", tupletype);
  SKIP(NULL, TK_RPAREN);
  SET_FLAG(AST_IN_PARENS);
  DONE();

// THIS
DEF(thistype);
  AST_NODE(TK_THISTYPE);
  SKIP(NULL, TK_THIS);
  DONE();

// (thistype | typeexpr | nominal | dontcare)
DEF(atomtype);
  RULE("type", thistype, groupedtype, nominal);
  DONE();

// ARROW type
DEF(viewpoint);
  INFIX_BUILD();
  TOKEN(NULL, TK_ARROW);
  RULE("viewpoint", type);
  DONE();

// atomtype [viewpoint]
DEF(type);
  RULE("type", atomtype);
  OPT_NO_DFLT RULE("viewpoint", viewpoint);
  DONE();

// ID ASSIGN rawseq
DEF(namedarg);
  AST_NODE(TK_NAMEDARG);
  TOKEN("argument name", TK_ID);
  SKIP(NULL, TK_ASSIGN);
  RULE("argument value", rawseq);
  DONE();

// WHERE namedarg {COMMA namedarg}
DEF(named);
  AST_NODE(TK_NAMEDARGS);
  SKIP(NULL, TK_WHERE);
  RULE("named argument", namedarg);
  WHILE(TK_COMMA, RULE("named argument", namedarg));
  DONE();

// rawseq {COMMA rawseq}
DEF(positional);
  AST_NODE(TK_POSITIONALARGS);
  RULE("argument", rawseq);
  WHILE(TK_COMMA, RULE("argument", rawseq));
  DONE();

// LBRACE [IS types] members RBRACE
DEF(object);
  TOKEN(NULL, TK_OBJECT);
  IF(TK_IS, RULE("provided type", types));
  RULE("object member", members);
  SKIP(NULL, TK_END);
  DONE();

// (LSQUARE | LSQUARE_NEW) rawseq {COMMA rawseq} RSQUARE
DEF(array);
  AST_NODE(TK_ARRAY);
  SKIP(NULL, TK_LSQUARE, TK_LSQUARE_NEW);
  RULE("array element", rawseq);
  WHILE(TK_COMMA, RULE("array element", rawseq));
  SKIP(NULL, TK_RSQUARE);
  DONE();

// (rawseq | dontcare) (COMMA (rawseq | dontcare))*
DEF(tuple);
  INFIX_BUILD();
  TOKEN(NULL, TK_COMMA);
  MAP_ID(TK_COMMA, TK_TUPLE);
  RULE("value", rawseq, dontcare);
  WHILE(TK_COMMA, RULE("value", rawseq, dontcare));
  DONE();

// (LPAREN | LPAREN_NEW) tuple RPAREN
DEF(groupedexpr);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  RULE("value", rawseq, dontcare);
  OPT_NO_DFLT RULE("value", tuple);
  SKIP(NULL, TK_RPAREN);
  SET_FLAG(AST_IN_PARENS);
  DONE();

// THIS | TRUE | FALSE | INT | FLOAT | STRING
DEF(literal);
  TOKEN("literal", TK_THIS, TK_TRUE, TK_FALSE, TK_INT, TK_FLOAT, TK_STRING);
  DONE();

DEF(ref);
  AST_NODE(TK_REFERENCE);
  TOKEN("name", TK_ID);
  DONE();

// AT (ID | STRING) typeargs (LPAREN | LPAREN_NEW) [positional] RPAREN
// [QUESTION]
DEF(ffi);
  TOKEN(NULL, TK_AT);
  MAP_ID(TK_AT, TK_FFICALL);
  TOKEN("ffi name", TK_ID, TK_STRING);
  OPT RULE("return type", typeargs);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("ffi arguments", positional);
  OPT RULE("ffi arguments", named);
  SKIP(NULL, TK_RPAREN);
  OPT TOKEN(NULL, TK_QUESTION);
  DONE();

// ref | literal | tuple | array | object | ffi
DEF(atom);
  RULE("value", ref, literal, groupedexpr, array, object, ffi);
  DONE();

// DOT ID
DEF(dot);
  INFIX_BUILD();
  TOKEN(NULL, TK_DOT);
  TOKEN("member name", TK_ID);
  DONE();

// TILDE ID
DEF(tilde);
  INFIX_BUILD();
  TOKEN(NULL, TK_TILDE);
  TOKEN("method name", TK_ID);
  DONE();

// typeargs
DEF(qualify);
  INFIX_BUILD();
  AST_NODE(TK_QUALIFY);
  RULE("type arguments", typeargs);
  DONE();

// LPAREN [positional] [named] RPAREN
DEF(call);
  INFIX_REVERSE();
  AST_NODE(TK_CALL);
  SKIP(NULL, TK_LPAREN);
  OPT RULE("argument", positional);
  OPT RULE("argument", named);
  SKIP(NULL, TK_RPAREN);
  DONE();

// atom {dot | tilde | qualify | call}
DEF(postfix);
  RULE("value", atom);
  SEQ("postfix expression", dot, tilde, qualify, call);
  DONE();

// ID
DEF(idseqid);
  TOKEN("variable name", TK_ID);
  DONE();

// idseqid | idseqmulti
DEF(idseqelement);
  RULE("variable name", idseqid, idseqmulti);
  DONE();

// (LPAREN | TK_LPAREN_NEW) ID {COMMA ID} RPAREN
DEF(idseqmulti);
  AST_NODE(TK_IDSEQ);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  RULE("variable name", idseqelement);
  WHILE(TK_COMMA, RULE("variable name", idseqelement));
  SKIP(NULL, TK_RPAREN);
  DONE();

// ID
DEF(idseqsingle);
  AST_NODE(TK_IDSEQ);
  TOKEN("variable name", TK_ID);
  DONE();

// ID | (LPAREN | TK_LPAREN_NEW) ID {COMMA ID} RPAREN
DEF(idseq);
  RULE("variable name", idseqsingle, idseqmulti);
  DONE();

// (VAR | VAL) idseq [COLON type]
DEF(local);
  TOKEN(NULL, TK_VAR, TK_LET);
  RULE("variable name", idseq);
  IF(TK_COLON, RULE("variable type", type));
  DONE();

// ELSE seq END
DEF(elseclause);
  SKIP(NULL, TK_ELSE);
  RULE("else value", seq);
  DONE();

// ELSEIF rawseq THEN seq
DEF(elseif);
  AST_NODE(TK_IF);
  SCOPE();
  SKIP(NULL, TK_ELSEIF);
  RULE("condition expression", rawseq);
  SKIP(NULL, TK_THEN);
  RULE("then value", seq);
  OPT RULE("else clause", elseif, elseclause);
  DONE();

// IF rawseq THEN seq {elseif} [ELSE seq] END
DEF(cond);
  TOKEN(NULL, TK_IF);
  SCOPE();
  RULE("condition expression", rawseq);
  SKIP(NULL, TK_THEN);
  RULE("then value", seq);
  OPT RULE("else clause", elseif, elseclause);
  SKIP(NULL, TK_END);
  DONE();

// PIPE [infix] [WHERE rawseq] [ARROW rawseq]
DEF(caseexpr);
  AST_NODE(TK_CASE);
  SCOPE();
  SKIP(NULL, TK_PIPE);
  OPT RULE("case pattern", infix);
  IF(TK_WHERE, RULE("guard expression", rawseq));
  IF(TK_DBLARROW, RULE("case body", rawseq));
  DONE();

// {caseexpr}
DEF(cases);
  AST_NODE(TK_CASES);
  SCOPE();
  SEQ("cases", caseexpr);
  DONE();

// MATCH rawseq cases [ELSE seq] END
DEF(match);
  TOKEN(NULL, TK_MATCH);
  SCOPE();
  RULE("match expression", rawseq);
  RULE("cases", cases);
  IF(TK_ELSE, RULE("else clause", seq));
  SKIP(NULL, TK_END);
  DONE();

// WHILE rawseq DO seq [ELSE seq] END
DEF(whileloop);
  TOKEN(NULL, TK_WHILE);
  SCOPE();
  RULE("condition expression", rawseq);
  SKIP(NULL, TK_DO);
  RULE("while body", seq);
  IF(TK_ELSE, RULE("else clause", seq));
  SKIP(NULL, TK_END);
  DONE();

// REPEAT seq UNTIL seq [ELSE seq] END
DEF(repeat);
  TOKEN(NULL, TK_REPEAT);
  SCOPE();
  RULE("repeat body", seq);
  SKIP(NULL, TK_UNTIL);
  RULE("condition expression", seq);
  IF(TK_ELSE, RULE("else clause", seq));
  SKIP(NULL, TK_END);
  DONE();

// FOR idseq [COLON type] IN rawseq DO rawseq [ELSE seq] END
// =>
// (SEQ
//   (ASSIGN (LET $1) iterator)
//   (WHILE $1.has_next()
//     (SEQ (ASSIGN (LET idseq type) $1.next()) body) else))
DEF(forloop);
  TOKEN(NULL, TK_FOR);
  RULE("iterator name", idseq);
  IF(TK_COLON, RULE("iterator type", type));
  SKIP(NULL, TK_IN);
  RULE("iterator", rawseq);
  SKIP(NULL, TK_DO);
  RULE("for body", rawseq);
  IF(TK_ELSE, RULE("else clause", seq));
  SKIP(NULL, TK_END);
  DONE();

// idseq [COLON type] = rawseq
DEF(withelem);
  AST_NODE(TK_SEQ);
  RULE("with name", idseq);
  IF(TK_COLON, RULE("with type", type));
  SKIP(NULL, TK_ASSIGN);
  RULE("initialiser", rawseq);
  DONE();

// withelem {COMMA withelem}
DEF(withexpr);
  AST_NODE(TK_SEQ);
  RULE("with expression", withelem);
  WHILE(TK_COMMA, RULE("with expression", withelem));
  DONE();

// WITH withexpr DO rawseq [ELSE rawseq] END
// =>
// (SEQ
//   (ASSIGN (LET $1 initialiser))*
//   (TRY_NO_CHECK
//     (SEQ (ASSIGN (LET idseq) $1)* body)
//     (SEQ (ASSIGN (LET idseq) $1)* else)
//     (SEQ $1.dispose()*)))
DEF(with);
  TOKEN(NULL, TK_WITH);
  RULE("with expression", withexpr);
  SKIP(NULL, TK_DO);
  RULE("with body", rawseq);
  IF(TK_ELSE, RULE("else clause", rawseq));
  SKIP(NULL, TK_END);
  DONE();

// TRY seq [ELSE seq] [THEN seq] END
DEF(try_block);
  TOKEN(NULL, TK_TRY);
  RULE("try body", seq);
  IF(TK_ELSE, RULE("try else body", seq));
  IF(TK_THEN, RULE("try then body", seq));
  SKIP(NULL, TK_END);
  DONE();

// '$try_no_check' seq [ELSE seq] [THEN seq] END
DEF(test_try_block);
  TOKEN(NULL, TK_TEST_TRY_NO_CHECK);
  MAP_ID(TK_TEST_TRY_NO_CHECK, TK_TRY_NO_CHECK);
  RULE("try body", seq);
  IF(TK_ELSE, RULE("try else body", seq));
  IF(TK_THEN, RULE("try then body", seq));
  SKIP(NULL, TK_END);
  SET_FLAG(TEST_ONLY);
  DONE();

// RECOVER [CAP] rawseq END
DEF(recover);
  TOKEN(NULL, TK_RECOVER);
  SCOPE();
  OPT TOKEN("capability", TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  RULE("recover body", rawseq);
  SKIP(NULL, TK_END);
  DONE();

// CONSUME [CAP] term
DEF(consume);
  TOKEN("consume", TK_CONSUME);
  OPT TOKEN("capability", TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  RULE("expression", term);
  DONE();

// (NOT | AMP) term
DEF(prefix);
  TOKEN("prefix", TK_NOT, TK_AMP);
  RULE("expression", term);
  DONE();

// (MINUS | MINUS_NEW) term
DEF(prefixminus);
  AST_NODE(TK_UNARY_MINUS);
  SKIP(NULL, TK_MINUS, TK_MINUS_NEW);
  RULE("value", term);
  DONE();

// '$seq' '(' rawseq ')'
// For testing only, thrown out by parsefix
DEF(test_seq);
  SKIP(NULL, TK_TEST_SEQ);
  SKIP(NULL, TK_LPAREN);
  RULE("sequence", rawseq);
  SKIP(NULL, TK_RPAREN);
  SET_FLAG(TEST_ONLY);
  DONE();

// '$scope' '(' rawseq ')'
// For testing only, thrown out by parsefix
DEF(test_seq_scope);
  SKIP(NULL, TK_TEST_SEQ_SCOPE);
  SKIP(NULL, TK_LPAREN);
  RULE("sequence", rawseq);
  SKIP(NULL, TK_RPAREN);
  SET_FLAG(TEST_ONLY);
  SCOPE();
  DONE();

// local | cond | match | whileloop | repeat | forloop | with | try | recover |
// consume | prefix | prefixminus | postfix | test_SCOPE()
DEF(term);
  RULE("value", local, cond, match, whileloop, repeat, forloop, with,
    try_block, recover, consume, prefix, prefixminus, postfix, test_seq,
    test_seq_scope, test_try_block);
  DONE();

// AS type
// For tuple types, use multiple matches.
// (AS expr type) =>
// (MATCH expr
//   (CASES
//     (CASE
//       (LET (IDSEQ $1) type)
//       NONE
//       (SEQ (CONSUME BORROWED $1))))
//   (SEQ ERROR))
DEF(asop);
  INFIX_BUILD();
  TOKEN("as", TK_AS);
  RULE("type", type);
  DONE();

// BINOP term
DEF(binop);
  INFIX_BUILD();
  TOKEN("binary operator",
    TK_AND, TK_OR, TK_XOR,
    TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE, TK_MOD,
    TK_LSHIFT, TK_RSHIFT,
    TK_IS, TK_ISNT, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GE, TK_GT
    );
  RULE("value", term);
  DONE();

// term {binop | asop}
DEF(infix);
  RULE("value", term);
  SEQ("value", binop, asop);
  DONE();

// ASSIGNOP assignment
DEF(assignop);
  INFIX_REVERSE();
  TOKEN("assign operator", TK_ASSIGN);
  RULE("assign rhs", assignment);
  DONE();

// term [assignop]
DEF(assignment);
  RULE("value", infix);
  OPT_NO_DFLT RULE("value", assignop);
  DONE();

// RETURN | BREAK | CONTINUE | ERROR | COMPILER_INTRINSIC
DEF(jump);
  TOKEN("statement", TK_RETURN, TK_BREAK, TK_CONTINUE, TK_ERROR,
    TK_COMPILER_INTRINSIC);
  OPT RULE("return value", rawseq);
  DONE();

// SEMI
DEF(semi);
  INFIX_BUILD();
  TOKEN(NULL, TK_SEMI)
  IFELSE(TK_NEWLINE, SET_FLAG(LAST_ON_LINE),);
  DONE();

// Always matches, consumes no tokens
DEF(nosemi);
  IFELSE(TK_NEWLINE, , NEXT_FLAGS(MISSING_SEMI));
  DONE();

// assignment [semi]
DEF(expr);
  RULE("value", assignment);
  RULE(";", semi, nosemi);
  DONE();

// expr {expr} [jump]
DEF(longseq);
  AST_NODE(TK_SEQ);
  RULE("value", expr);
  SEQ("value", expr);
  OPT_NO_DFLT RULE("value", jump);
  NEXT_FLAGS(0);
  DONE();

// jump
DEF(jumpseq);
  AST_NODE(TK_SEQ);
  RULE("value", jump);
  DONE();

// (longseq | jumpseq)
DEF(rawseq);
  RULE("value", longseq, jumpseq);
  DONE();

// rawseq
DEF(seq);
  RULE("value", rawseq);
  SCOPE();
  DONE();

// (FUN | BE | NEW) [CAP] ID [typeparams] (LPAREN | LPAREN_NEW) [params]
// RPAREN [COLON type] [QUESTION] [ARROW rawseq]
DEF(method);
  TOKEN(NULL, TK_FUN, TK_BE, TK_NEW);
  SCOPE();
  OPT TOKEN("capability", TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  TOKEN("method name", TK_ID);
  OPT RULE("type parameters", typeparams);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("parameters", params);
  SKIP(NULL, TK_RPAREN);
  IF(TK_COLON, RULE("return type", type));
  OPT TOKEN(NULL, TK_QUESTION);
  OPT TOKEN(NULL, TK_STRING);
  IF(TK_DBLARROW, RULE("method body", rawseq));
  // Order should be:
  // cap id type_params params return_type error body docstring
  REORDER(0, 1, 2, 3, 4, 5, 7, 6);
  DONE();

// (VAR | VAL) ID [COLON type] [ASSIGN infix]
DEF(field);
  TOKEN(NULL, TK_VAR, TK_LET);
  MAP_ID(TK_VAR, TK_FVAR);
  MAP_ID(TK_LET, TK_FLET);
  TOKEN("field name", TK_ID);
  SKIP(NULL, TK_COLON);
  RULE("field type", type);
  IF(TK_ASSIGN, RULE("field value", infix));
  DONE();

// {field} {method}
DEF(members);
  AST_NODE(TK_MEMBERS);
  SEQ("field", field);
  SEQ("method", method);
  DONE();

// (TYPE | INTERFACE | TRAIT | PRIMITIVE | CLASS | ACTOR) [AT] ID [typeparams]
// [CAP] [IS types] [STRING] members
DEF(class_def);
  TOKEN("entity", TK_TYPE, TK_INTERFACE, TK_TRAIT, TK_PRIMITIVE, TK_CLASS,
    TK_ACTOR);
  SCOPE();
  OPT TOKEN(NULL, TK_AT);
  TOKEN("name", TK_ID);
  OPT RULE("type parameters", typeparams);
  OPT TOKEN("capability", TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
  IF(TK_IS, RULE("provided types", types));
  OPT TOKEN("docstring", TK_STRING);
  RULE("members", members);
  // Order should be:
  // id type_params cap provides members c_api docstring
  REORDER(1, 2, 3, 4, 6, 0, 5);
  DONE();

// STRING
DEF(use_uri);
  TOKEN(NULL, TK_STRING);
  DONE();

// AT (ID | STRING) typeparams (LPAREN | LPAREN_NEW) [params] RPAREN [QUESTION]
DEF(use_ffi);
  TOKEN(NULL, TK_AT);
  MAP_ID(TK_AT, TK_FFIDECL);
  SCOPE();
  TOKEN("ffi name", TK_ID, TK_STRING);
  RULE("return type", typeargs);
  SKIP(NULL, TK_LPAREN, TK_LPAREN_NEW);
  OPT RULE("ffi parameters", params);
  AST_NODE(TK_NONE);  // Named parameters
  SKIP(NULL, TK_RPAREN);
  OPT TOKEN(NULL, TK_QUESTION);
  DONE();

// ID ASSIGN
DEF(use_name);
  TOKEN(NULL, TK_ID);
  SKIP(NULL, TK_ASSIGN);
  DONE();

// USE [ID ASSIGN] (STRING | USE_FFI) [IF infix]
DEF(use);
  TOKEN(NULL, TK_USE);
  OPT RULE("name", use_name);
  RULE("specifier", use_uri, use_ffi);
  IF(TK_IF, RULE("use condition", infix));
  DONE();

// {use} {class}
DEF(module);
  AST_NODE(TK_MODULE);
  SCOPE();
  OPT_NO_DFLT TOKEN("package docstring", TK_STRING);
  SEQ("use command", use);
  SEQ("type, interface, trait, primitive, class or actor definition",
    class_def);
  SKIP("type, interface, trait, primitive, class, actor, member or method",
    TK_EOF);
  DONE();

// external API
ast_t* parser(source_t* source)
{
  return parse(source, module, "class, actor, primitive or trait");
}
