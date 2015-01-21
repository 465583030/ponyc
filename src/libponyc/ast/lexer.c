#include "lexer.h"
#include "token.h"
#include "stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <assert.h>


struct lexer_t
{
  source_t* source;
  size_t ptr;
  size_t len;

  size_t line;
  size_t pos;
  bool newline;

  char* buffer;  // Symbol text buffer
  size_t buflen; // Length of buffer currently used
  size_t alloc;  // Space allocated for buffer
};


typedef struct lexsym_t
{
  const char* symbol;
  token_id id;
} lexsym_t;

// Note that for symbols where one symbol starts with another, the longer one
// must appear first in this list.
// For example -> must appear before -
static const lexsym_t symbols[] =
{
  { "...", TK_ELLIPSIS },
  { "->", TK_ARROW },
  { "=>", TK_DBLARROW },

  { "<<", TK_LSHIFT },
  { ">>", TK_RSHIFT },

  { "==", TK_EQ },
  { "!=", TK_NE },

  { "<=", TK_LE },
  { ">=", TK_GE },

  { "{", TK_LBRACE },
  { "}", TK_RBRACE },
  { "(", TK_LPAREN },
  { ")", TK_RPAREN },
  { "[", TK_LSQUARE },
  { "]", TK_RSQUARE },
  { ",", TK_COMMA },

  { ".", TK_DOT },
  { "~", TK_TILDE },
  { ":", TK_COLON },
  { ";", TK_SEMI },
  { "=", TK_ASSIGN },

  { "+", TK_PLUS },
  { "-", TK_MINUS },
  { "*", TK_MULTIPLY },
  { "/", TK_DIVIDE },
  { "%", TK_MOD },
  { "@", TK_AT },

  { "<", TK_LT },
  { ">", TK_GT },

  { "|", TK_PIPE },
  { "&", TK_AMP },
  { "^", TK_EPHEMERAL },

  { "?", TK_QUESTION },
  { "-", TK_UNARY_MINUS },
  { "_", TK_DONTCARE },
  { "$:(", TK_TEST_SEQ_SCOPE },
  { "$(", TK_TEST_SEQ },

  { NULL, (token_id)0 }
};

static const lexsym_t keywords[] =
{
  { "compiler_intrinsic", TK_COMPILER_INTRINSIC },

  { "use", TK_USE },
  { "type", TK_TYPE },
  { "interface", TK_INTERFACE },
  { "trait", TK_TRAIT },
  { "primitive", TK_PRIMITIVE },
  { "class", TK_CLASS },
  { "actor", TK_ACTOR },
  { "object", TK_OBJECT },

  { "as", TK_AS },
  { "is", TK_IS },
  { "isnt", TK_ISNT },

  { "var", TK_VAR },
  { "let", TK_LET },
  { "new", TK_NEW },
  { "fun", TK_FUN },
  { "be", TK_BE },

  { "iso", TK_ISO },
  { "trn", TK_TRN },
  { "ref", TK_REF },
  { "val", TK_VAL },
  { "box", TK_BOX },
  { "tag", TK_TAG },

  { "this", TK_THIS },
  { "return", TK_RETURN },
  { "break", TK_BREAK },
  { "continue", TK_CONTINUE },
  { "consume", TK_CONSUME },
  { "recover", TK_RECOVER },

  { "if", TK_IF },
  { "then", TK_THEN },
  { "else", TK_ELSE },
  { "elseif", TK_ELSEIF },
  { "end", TK_END },
  { "for", TK_FOR },
  { "in", TK_IN },
  { "while", TK_WHILE },
  { "do", TK_DO },
  { "repeat", TK_REPEAT },
  { "until", TK_UNTIL },
  { "match", TK_MATCH },
  { "where", TK_WHERE },
  { "try", TK_TRY },
  { "with", TK_WITH },
  { "error", TK_ERROR },

  { "not", TK_NOT },
  { "and", TK_AND },
  { "or", TK_OR },
  { "xor", TK_XOR },

  { "true", TK_TRUE },
  { "false", TK_FALSE },

  { NULL, (token_id)0 }
};

static const lexsym_t abstract[] =
{
  { "x", TK_NONE }, // Needed for AST printing

  { "program", TK_PROGRAM },
  { "package", TK_PACKAGE },
  { "module", TK_MODULE },

  { "members", TK_MEMBERS },
  { "fvar", TK_FVAR },
  { "flet", TK_FLET },
  { "ffidecl", TK_FFIDECL },
  { "fficall", TK_FFICALL },

  { "types", TK_TYPES },
  { "uniontype", TK_UNIONTYPE },
  { "isecttype", TK_ISECTTYPE },
  { "tupletype", TK_TUPLETYPE },
  { "nominal", TK_NOMINAL },
  { "thistype", TK_THISTYPE },
  { "funtype", TK_FUNTYPE },

  { "iso", TK_ISO_BIND },
  { "trn", TK_TRN_BIND },
  { "ref", TK_REF_BIND },
  { "val", TK_VAL_BIND },
  { "box", TK_BOX_BIND },
  { "trn", TK_TAG_BIND },
  { "any", TK_ANY_BIND },

  { "boxgen", TK_BOX_GENERIC },
  { "taggen", TK_TAG_GENERIC },
  { "anygen", TK_ANY_GENERIC },
  { "!", TK_BORROWED },

  { "literal", TK_LITERAL },
  { "branch", TK_LITERALBRANCH },
  { "opliteral", TK_OPERATORLITERAL },

  { "typeparams", TK_TYPEPARAMS },
  { "typeparam", TK_TYPEPARAM },
  { "params", TK_PARAMS },
  { "param", TK_PARAM },
  { "typeargs", TK_TYPEARGS },
  { "positionalargs", TK_POSITIONALARGS },
  { "namedargs", TK_NAMEDARGS },
  { "namedarg", TK_NAMEDARG },

  { "seq", TK_SEQ },
  { "idseq", TK_IDSEQ },
  { "qualify", TK_QUALIFY },
  { "call", TK_CALL },
  { "tuple", TK_TUPLE },
  { "array", TK_ARRAY },
  { "cases", TK_CASES },
  { "case", TK_CASE },
  { "try", TK_TRY2 },
  { "identity", TK_IDENTITY },

  { "reference", TK_REFERENCE },
  { "packageref", TK_PACKAGEREF },
  { "typeref", TK_TYPEREF },
  { "typeparamref", TK_TYPEPARAMREF },
  { "newref", TK_NEWREF },
  { "beref", TK_BEREF },
  { "funref", TK_FUNREF },
  { "fvarref", TK_FVARREF },
  { "fletref", TK_FLETREF },
  { "varref", TK_VARREF },
  { "letref", TK_LETREF },
  { "paramref", TK_PARAMREF },
  { "newapp", TK_NEWAPP },
  { "beapp", TK_BEAPP },
  { "funapp", TK_FUNAPP },

  { "\\n", TK_NEWLINE },

  { "test", TK_TEST },
  { NULL, (token_id)0 }
};


static void lexerror(lexer_t* lexer, const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  errorv(lexer->source, lexer->line, lexer->pos, fmt, ap);
  va_end(ap);
}


static bool is_eof(lexer_t* lexer)
{
  return lexer->len == 0;
}


static bool is_symbol_char(char c)
{
  return ((c >= '!') && (c <= '.'))
    || ((c >= ':') && (c <= '@'))
    || ((c >= '[') && (c <= '^'))
    || ((c >= '{') && (c <= '~'));
}


static token_t* make_token(lexer_t* lexer, token_id id)
{
  return token_new(id, lexer->source);
}


/* Advance our input by the specified number of characters.
 * Only the first character may be a newline.
 */
static void adv(lexer_t* lexer, size_t count)
{
  assert(lexer->len >= count);

  if(count == 0)
    return;

  if(lexer->source->m[lexer->ptr] == '\n')
  {
    lexer->line++;
    lexer->pos = 0;
  }

  lexer->ptr += count;
  lexer->len -= count;
  lexer->pos += count;
}


static char look(lexer_t* lexer)
{
  if(is_eof(lexer))
    return '\0';

  return lexer->source->m[lexer->ptr];
}


// look(lexer) is equivalent to lookn(lexer, 1)
static char lookn(lexer_t* lexer, size_t chars)
{
  if(lexer->len < chars)
    return '\0';

  return lexer->source->m[lexer->ptr + chars - 1];
}


static void string_terminate(lexer_t* lexer)
{
  lexerror(lexer, "String doesn't terminate");
  lexer->ptr += lexer->len;
  lexer->len = 0;
  lexer->buflen = 0;
}


static void append(lexer_t* lexer, char c)
{
  if(lexer->buflen >= lexer->alloc)
  {
    size_t new_len = (lexer->alloc > 0) ? lexer->alloc << 1 : 64;
    char* new_buf = (char*)pool_alloc_size(new_len);
    memcpy(new_buf, lexer->buffer, lexer->alloc);

    if(lexer->alloc > 0)
      pool_free_size(lexer->alloc, lexer->buffer);

    lexer->buffer = new_buf;
    lexer->alloc = new_len;
  }

  lexer->buffer[lexer->buflen] = c;
  lexer->buflen++;
}


static bool append_unicode(lexer_t* lexer, size_t len)
{
  char* m = &lexer->source->m[lexer->ptr];
  uint32_t c = 0;

  if(lexer->len < len)
  {
    string_terminate(lexer);
    return false;
  }

  adv(lexer, len);

  for(size_t i = 0; i < len; i++)
  {
    c <<= 4;

    if((m[i] >= '0') && (m[i] <= '9'))
    {
      c += m[i] - '0';
    } else if((m[i] >= 'a') && (m[i] <= 'f')) {
      c += m[i] + 10 - 'a';
    } else if((m[i] >= 'A') && (m[i] <= 'F')) {
      c += m[i] + 10 - 'A';
    } else {
      lexerror(lexer, "Escape sequence contains non-hexadecimal %c", c);
      return false;
    }
  }

  // UTF-8 encoding
  if(c <= 0x7F)
  {
    append(lexer, (char)(c & 0x7F));
  } else if(c <= 0x7FF) {
    append(lexer, (char)(0xC0 | (c >> 6)));
    append(lexer, (char)(0x80 | (c & 0x3F)));
  } else if(c <= 0xFFFF) {
    append(lexer, (char)(0xE0 | (c >> 12)));
    append(lexer, (char)(0x80 | ((c >> 6) & 0x3F)));
    append(lexer, (char)(0x80 | (c & 0x3F)));
  } else if(c <= 0x10FFFF) {
    append(lexer, (char)(0xF0 | (c >> 18)));
    append(lexer, (char)(0x80 | ((c >> 12) & 0x3F)));
    append(lexer, (char)(0x80 | ((c >> 6) & 0x3F)));
    append(lexer, (char)(0x80 | (c & 0x3F)));
  } else {
    lexerror(lexer, "Escape sequence exceeds unicode range (0x10FFFF)");
    return false;
  }

  return true;
}


/**
 * Removes longest common prefix indentation from every line in a triple
 * quoted string. If the string begins with an empty line, that line is removed
 * entirely.
 */
static void normalise_string(lexer_t* lexer)
{
  if(lexer->buflen == 0)
    return;

  // Make sure we have a null terminated string.
  append(lexer, '\0');

  // If we aren't multiline, do nothing.
  if(memchr(lexer->buffer, '\n', lexer->buflen) == NULL)
    return;

  // Calculate leading whitespace.
  char* buf = lexer->buffer;
  size_t ws = lexer->buflen;
  size_t ws_this_line = 0;
  bool in_leading_ws = true;

  for(size_t i = 0; i < lexer->buflen; i++)
  {
    char c = lexer->buffer[i];

    if(in_leading_ws)
    {
      if(c == ' ' || c == '\t')
      {
        ws_this_line++;
      }
      else if((c != '\r') && (c != '\n'))
      {
        if(ws_this_line < ws)
          ws = ws_this_line;

        in_leading_ws = false;
      }
    }

    if(c == '\n')
    {
      ws_this_line = 0;
      in_leading_ws = true;
    }
  }

  // Trim leading whitespace on each line.
  if(ws > 0)
  {
    char* line_start = lexer->buffer;
    char* compacted = lexer->buffer;
    size_t rem = lexer->buflen;

    while(rem > 0)
    {
      char* line_end = strchr(line_start, '\n');
      size_t line_len =
        (line_end == NULL) ? rem : (size_t)(line_end - line_start + 1);

      if(line_start != line_end)
      {
        size_t trim = (line_len < ws) ? line_len : ws;
        memmove(compacted, line_start + trim, line_len - trim);
        compacted += line_len - trim;
      } else {
        memmove(compacted, line_start, line_len);
        compacted += line_len;
      }

      line_start += line_len;
      rem -= line_len;
    }
  }

  // Trim a leading newline if there is one.
  buf = lexer->buffer;

  if((buf[0] == '\r') && (buf[1] == '\n'))
  {
    lexer->buflen -= 2;
    memmove(&buf[0], &buf[2], lexer->buflen);
  } else if(buf[0] == '\n') {
    lexer->buflen--;
    memmove(&buf[0], &buf[1], lexer->buflen);
  }
}


static const char* save_token_text(lexer_t* lexer)
{
  append(lexer, '\0');
  const char* str = stringtab(lexer->buffer);
  assert(str != NULL);
  lexer->buflen = 0;

  return str;
}


static token_t* nested_comment(lexer_t* lexer)
{
  size_t depth = 1;

  while(depth > 0)
  {
    if(lexer->len <= 1)
    {
      lexerror(lexer, "Nested comment doesn't terminate");
      lexer->ptr += lexer->len;
      lexer->len = 0;
      return make_token(lexer, TK_LEX_ERROR);
    }

    if(look(lexer) == '*' && lookn(lexer, 2) == '/')
    {
      adv(lexer, 2);
      depth--;
    }
    else if(look(lexer) == '/' && lookn(lexer, 2) == '*')
    {
      adv(lexer, 2);
      depth++;
    }
    else
    {
      adv(lexer, 1);
    }
  }

  return NULL;
}


static void line_comment(lexer_t* lexer)
{
  // We don't consume the terminating newline here, but it will be handled next
  // as whitespace
  while(!is_eof(lexer) && (look(lexer) != '\n'))
  {
    adv(lexer, 1);
  }
}


static token_t* slash(lexer_t* lexer)
{
  adv(lexer, 1);

  if(look(lexer) == '*')
  {
    adv(lexer, 1);
    return nested_comment(lexer);
  } else if(look(lexer) == '/') {
    adv(lexer, 1);
    line_comment(lexer);
    return NULL;
  }

  return make_token(lexer, TK_DIVIDE);
}


static token_t* triple_string(lexer_t* lexer)
{
  while(true)
  {
    if(is_eof(lexer))
    {
      string_terminate(lexer);
      return make_token(lexer, TK_LEX_ERROR);
    }

    char c = look(lexer);
    adv(lexer, 1);

    if((c == '\"') && (look(lexer) == '\"') && (lookn(lexer, 2) == '\"'))
    {
      adv(lexer, 2);
      normalise_string(lexer);
      token_t* t = make_token(lexer, TK_STRING);
      token_set_string(t, save_token_text(lexer));
      return t;
    }

    append(lexer, c);
  }
}


static token_t* string(lexer_t* lexer)
{
  adv(lexer, 1);  // Consume leading "
  assert(lexer->buflen == 0);

  if((look(lexer) == '\"') && (lookn(lexer, 2) == '\"'))
  {
    adv(lexer, 2);
    return triple_string(lexer);
  }

  while(true)
  {
    if(is_eof(lexer))
    {
      string_terminate(lexer);
      return make_token(lexer, TK_LEX_ERROR);
    }

    char next_char = look(lexer);

    if(next_char == '\"')
    {
      adv(lexer, 1);
      token_t* t = make_token(lexer, TK_STRING);
      token_set_string(t, save_token_text(lexer));
      return t;
    }

    if(next_char == '\\')
    {
      if(lexer->len < 2)
      {
        string_terminate(lexer);
        return make_token(lexer, TK_LEX_ERROR);
      }

      adv(lexer, 1);
      bool r = true;
      char c = look(lexer);
      adv(lexer, 1);

      switch(c)
      {
        case 'a':  append(lexer, 0x07); break;
        case 'b':  append(lexer, 0x08); break;
        case 'e':  append(lexer, 0x1B); break;
        case 'f':  append(lexer, 0x0C); break;
        case 'n':  append(lexer, 0x0A); break;
        case 'r':  append(lexer, 0x0D); break;
        case 't':  append(lexer, 0x09); break;
        case 'v':  append(lexer, 0x0B); break;
        case '\"': append(lexer, 0x22); break;
        case '\\': append(lexer, 0x5C); break;
        case '0':  append(lexer, 0x00); break;

        case 'x':  r = append_unicode(lexer, 2); break;
        case 'u':  r = append_unicode(lexer, 4); break;
        case 'U':  r = append_unicode(lexer, 6); break;

        default:
          lexerror(lexer, "Invalid escape sequence: \\%c", c);
          r = false;
          break;
      }

      if(!r)
      {
        string_terminate(lexer);
        return make_token(lexer, TK_LEX_ERROR);
      }
    } else {
      append(lexer, next_char);
      adv(lexer, 1);
    }
  }
}


static token_t* character(lexer_t* lexer)
{
  adv(lexer, 1);  // Consume leading '
  __uint128_t value = 0;

  while(true)
  {
    if(is_eof(lexer))
    {
      lexerror(lexer, "Character literal doesn't terminate");
      return make_token(lexer, TK_LEX_ERROR);
    }

    char next_char = look(lexer);

    if(next_char == '\'')
    {
      adv(lexer, 1);
      token_t* t = make_token(lexer, TK_INT);
      token_set_int(t, value);
      return t;
    }

    if(next_char == '\\')
    {
      if(lexer->len < 2)
      {
        lexerror(lexer, "Character literal doesn't terminate");
        return make_token(lexer, TK_LEX_ERROR);
      }

      adv(lexer, 1);
      char c = look(lexer);
      adv(lexer, 1);

      switch(c)
      {
        case 'a':  value = (value << 8) | 0x07; break;
        case 'b':  value = (value << 8) | 0x08; break;
        case 'e':  value = (value << 8) | 0x1B; break;
        case 'f':  value = (value << 8) | 0x0C; break;
        case 'n':  value = (value << 8) | 0x0A; break;
        case 'r':  value = (value << 8) | 0x0D; break;
        case 't':  value = (value << 8) | 0x09; break;
        case 'v':  value = (value << 8) | 0x0B; break;
        case '\"': value = (value << 8) | 0x22; break;
        case '\\': value = (value << 8) | 0x5C; break;
        case '0':  value = (value << 8) | 0x00; break;

        case 'x':
          if(lexer->len < 2)
          {
            lexerror(lexer, "Character literal doesn't terminate");
            return make_token(lexer, TK_LEX_ERROR);
          }

          for(int i = 0; i < 2; i++)
          {
            c = look(lexer);

            if((c >= '0') && (c <= '9'))
              c = (char)(c - '0');
            else if((c >= 'a') && (c <= 'f'))
              c = (char)(c - 'a' + 10);
            else if((c >= 'A') && (c <= 'F'))
              c = (char)(c - 'A' + 10);
            else
            {
              lexerror(lexer, "Invalid hex value: %c", c);
              return make_token(lexer, TK_LEX_ERROR);
            }

            value = (value << 4) | c;
            adv(lexer, 1);
          }

          break;

        default:
          lexerror(lexer, "Invalid escape sequence: \\%c", c);
          return make_token(lexer, TK_LEX_ERROR);
      }
    } else {
      value = (value << 8) | next_char;
      adv(lexer, 1);
    }
  }
}


/** Add the given digit to a literal value, checking for overflow.
 * Returns true on success, false on overflow error.
 */
static bool accum(lexer_t* lexer, __uint128_t* v, int digit, uint32_t base)
{
  __uint128_t v1 = *v;
  __uint128_t v2 = v1 * base;

  if((v2 / base) != v1)
  {
    lexerror(lexer, "overflow in numeric literal");
    return false;
  }

  v2 += digit;

  if(v2 < v1)
  {
    lexerror(lexer, "overflow in numeric literal");
    return false;
  }

  *v = v2;
  return true;
}


/** Process an integral literal or integral part of a real.
 * There must be at least one digit present.
 * Return true on success, false on failure.
 * The end_on_e flag indicates that we treat e (or E) as a valid terminator
 * character, rather than part of the integer being processed.
 * The given context is used in error reporting.
 * The value read is added onto the end of any existing value in out_value.
 */
static bool lex_integer(lexer_t* lexer, uint32_t base,
  __uint128_t* out_value, uint32_t* out_digit_count, bool end_on_e,
  const char* context)
{
  uint32_t digit_count = 0;

  while(!is_eof(lexer))
  {
    char c = look(lexer);
    uint32_t digit = 0;

    if(end_on_e && ((c == 'e') || (c == 'E')))
      break;

    if((c >= '0') && (c <= '9'))
      digit = c - '0';
    else if((c >= 'a') && (c <= 'z'))
      digit = c - 'a' + 10;
    else if((c >= 'A') && (c <= 'Z'))
      digit = c - 'A' + 10;
    else
      break;

    if(digit >= base)
    {
      lexerror(lexer, "Invalid character in %s: %c", context, c);
      return false;
    }

    if(!accum(lexer, out_value, digit, base))
      return false;

    adv(lexer, 1);
    digit_count++;
  }

  if(digit_count == 0)
  {
    lexerror(lexer, "No digits in %s", context);
    return false;
  }

  if(out_digit_count != NULL)
    *out_digit_count = digit_count;

  return true;
}


/** Process a real literal when the leading integral part has already been
 * handled.
 */
static token_t* real(lexer_t* lexer, __uint128_t integral_value)
{
  __uint128_t significand = integral_value;
  __int128_t e = 0;
  uint32_t mantissa_digit_count = 0;
  char c = look(lexer);
  assert(c == '.' || c == 'e' || c == 'E');

  if(c == '.')
  {
    c = lookn(lexer, 2);

    if(c < '0' || c > '9')
    {
      // Treat this as an integer token followed by a dot token
      token_t* t = make_token(lexer, TK_INT);
      token_set_int(t, integral_value);
      return t;
    }

    adv(lexer, 1);  // Consume dot

    // Read in rest of the significand
    if(!lex_integer(lexer, 10, &significand, &mantissa_digit_count, true,
      "real number mantissa"))
      return make_token(lexer, TK_LEX_ERROR);
  }

  if((look(lexer) == 'e') || (look(lexer) == 'E'))
  {
    adv(lexer, 1);  // Consume e

    bool exp_neg = false;

    if((look(lexer) == '+') || (look(lexer) == '-'))
    {
      exp_neg = (look(lexer) == '-');
      adv(lexer, 1);
    }

    __uint128_t exp_value = 0;
    if(!lex_integer(lexer, 10, &exp_value, NULL, false, "real number exponent"))
      return make_token(lexer, TK_LEX_ERROR);

    if(exp_neg)
      e = -exp_value;
    else
      e = exp_value;
  }

  e -= mantissa_digit_count;
  token_t* t = make_token(lexer, TK_FLOAT);

#ifdef PLATFORM_IS_VISUAL_STUDIO
  token_set_float(t, (double)significand * pow(10.0, e));
#else
  token_set_float(t, (double)significand * pow(10.0, (double)e));
#endif

  return t;
}


static token_t* nondecimal_number(lexer_t* lexer, int base,
  const char* context)
{
  __uint128_t value = 0;
  if(!lex_integer(lexer, base, &value, NULL, false, context))
    return make_token(lexer, TK_LEX_ERROR);

  token_t* t = make_token(lexer, TK_INT);
  token_set_int(t, value);
  return t;
}


static token_t* number(lexer_t* lexer)
{
  if(look(lexer) == '0')
  {
    switch(lookn(lexer, 2))
    {
      case 'x':
      case 'X':
        adv(lexer, 2);  // Consume 0x
        return nondecimal_number(lexer, 16, "hexadecimal number");

      case 'b':
      case 'B':
        adv(lexer, 2);  // Consume 0b
        return nondecimal_number(lexer, 2, "binary number");

      default: {}
    }
  }

  // Decimal
  __uint128_t value = 0;
  if(!lex_integer(lexer, 10, &value, NULL, true, "decimal number"))
    return make_token(lexer, TK_LEX_ERROR);

  if((look(lexer) == '.') || (look(lexer) == 'e') || (look(lexer) == 'E'))
    return real(lexer, value);

  token_t* t = make_token(lexer, TK_INT);
  token_set_int(t, value);
  return t;
}


static void read_id(lexer_t* lexer)
{
  char c;

  while(!is_eof(lexer))
  {
    c = look(lexer);

    if((c == '_') || (c == '\'') || isalnum(c))
    {
      append(lexer, c);
      adv(lexer, 1);
    } else {
      break;
    }
  }

  append(lexer, '\0');
}


static token_t* identifier(lexer_t* lexer)
{
  read_id(lexer);

  if(!strcmp(lexer->buffer, "_"))
  {
    lexer->buflen = 0;
    return make_token(lexer, TK_DONTCARE);
  }

  for(const lexsym_t* p = keywords; p->symbol != NULL; p++)
  {
    if(!strcmp(lexer->buffer, p->symbol))
    {
      lexer->buflen = 0;
      return make_token(lexer, p->id);
    }
  }

  token_t* t = make_token(lexer, TK_ID);
  token_set_string(t, save_token_text(lexer));
  return t;
}


static token_id newline_symbols(token_id raw_token, bool newline)
{
  if(!newline)
    return raw_token;

  switch(raw_token)
  {
    case TK_LPAREN:  return TK_LPAREN_NEW;
    case TK_LSQUARE: return TK_LSQUARE_NEW;
    case TK_MINUS:   return TK_MINUS_NEW;
    default:         return raw_token;
  }
}


static token_t* symbol(lexer_t* lexer)
{
  char sym[3];

  for(size_t i = 0; i < sizeof(sym); ++i)
    sym[i] = lookn(lexer, i + 1);

  for(const lexsym_t* p = symbols; p->symbol != NULL; p++)
  {
    const char* symbol = p->symbol;

    for(int i = 0; symbol[i] == '\0' || symbol[i] == sym[i]; ++i)
    {
      if(symbol[i] == '\0')
      {
        adv(lexer, i);
        return make_token(lexer, newline_symbols(p->id, lexer->newline));
      }
    }
  }

  lexerror(lexer, "Unknown symbol: %c", sym[0]);
  return make_token(lexer, TK_LEX_ERROR);
}


lexer_t* lexer_open(source_t* source)
{
  assert(source != NULL);

  lexer_t* lexer = POOL_ALLOC(lexer_t);
  memset(lexer, 0, sizeof(lexer_t));

  lexer->source = source;
  lexer->len = source->len;
  lexer->line = 1;
  lexer->pos = 1;
  lexer->newline = true;

  return lexer;
}


void lexer_close(lexer_t* lexer)
{
  if(lexer == NULL)
    return;

  if(lexer->buffer != NULL)
    pool_free_size(lexer->alloc, lexer->buffer);

  POOL_FREE(lexer_t, lexer);
}


token_t* lexer_next(lexer_t* lexer)
{
  assert(lexer != NULL);

  token_t* t = NULL;
  size_t symbol_line;
  size_t symbol_pos;

  while(t == NULL)
  {
    symbol_line = lexer->line;
    symbol_pos = lexer->pos;

    if(is_eof(lexer))
    {
      t = make_token(lexer, TK_EOF);
      break;
    }

    char c = look(lexer);

    switch(c)
    {
      case '\n':
        lexer->newline = true;
        adv(lexer, 1);
        break;

      case '\r':
      case '\t':
      case ' ':
        adv(lexer, 1);
        break;

      case '/':
        t = slash(lexer);
        break;

      case '\"':
        t = string(lexer);
        break;

      case '\'':
        t = character(lexer);
        break;

      default:
        if(isdigit(c))
        {
          t = number(lexer);
        } else if(isalpha(c) || (c == '_')) {
          t = identifier(lexer);
        } else if(is_symbol_char(c)) {
          t = symbol(lexer);
        } else {
          lexerror(lexer, "Unrecognized character: %c", c);
          adv(lexer, 1);
        }
    }
  }

  if(lexer->newline)
    token_set_first_on_line(t);

  lexer->newline = false; // We've found a symbol, so no longer a new line
  token_set_pos(t, symbol_line, symbol_pos);
  return t;
}


const char* lexer_print(token_id id)
{
  for(const lexsym_t* p = abstract; p->symbol != NULL; p++)
  {
    if(id == p->id)
      return p->symbol;
  }

  for(const lexsym_t* p = keywords; p->symbol != NULL; p++)
  {
    if(id == p->id)
      return p->symbol;
  }

  for(const lexsym_t* p = symbols; p->symbol != NULL; p++)
  {
    if(id == p->id)
      return p->symbol;
  }

  return NULL;
}


token_id lexer_is_abstract_keyword(const char* text)
{
  for(const lexsym_t* p = abstract; p->symbol != NULL; p++)
  {
    if(!strcmp(text, p->symbol))
      return p->id;
  }

  return TK_LEX_ERROR;
}
