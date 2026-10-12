/*
 * flowjs lexer - ECMA-262 tokenizer written in C23.
 *
 * Produces length-prefixed slices into the original source (zero-copy),
 * cooked string values, and 1-based line/column locations.
 */
#include "flowjs.h"
#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- small helpers ------------------------------------------------------ */

void fjs_lexer_error(fjs_lexer *lx, const char *msg) {
  lx->err.kind = FJS_ERR_SYNTAX;
  __builtin_snprintf(lx->err.message, sizeof(lx->err.message), "%s:%zu:%zu: %s",
                     lx->file ? lx->file : "<input>", lx->ln, lx->col, msg);
  lx->tok.kind = FJS_TK_INVALID;
}

static bool fjs_is_digit(char c) { return c >= '0' && c <= '9'; }
static bool fjs_is_hex(char c) {
  return fjs_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool fjs_is_id_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$' ||
         (unsigned char)c >= 0x80; /* non-ascii treated as ident (approx) */
}

static bool fjs_is_id_cont(char c) { return fjs_is_id_start(c) || fjs_is_digit(c); }

static void fjs_advance(fjs_lexer *lx) {
  if (lx->p >= lx->src + lx->len) return;
  if (*lx->p == '\n') { lx->ln++; lx->col = 1; } else { lx->col++; }
  lx->p++;
}

/* span token [start, start+len); zero-copy slice */
static void fjs_span(fjs_lexer *lx, fjs_tk_kind kind, const char *start, size_t len) {
  lx->tok.kind = kind;
  lx->tok.txt = (fjs_slice){start, len};
  lx->tok.value = (fjs_slice){NULL, 0};
  lx->tok.kw = 0;
  lx->tok.template_head = false;
}

/* skip whitespace and comments; returns false on unexpected EOF inside
 * a block comment (error already raised) */
static bool fjs_skip_trivia(fjs_lexer *lx) {
  const char *end = lx->src + lx->len;
  for (;;) {
    while (lx->p < end) {
      char c = *lx->p;
      if (c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == '\n') {
        fjs_advance(lx);
      } else if (c == '\0' && lx->p == end - 1) {
        return true; /* NUL padding */
      } else {
        break;
      }
    }
    if (lx->p + 1 < end && lx->p[0] == '/' && lx->p[1] == '/') {
      while (lx->p < end && *lx->p != '\n') fjs_advance(lx);
      continue;
    }
    if (lx->p + 1 < end && lx->p[0] == '/' && lx->p[1] == '*') {
      fjs_advance(lx);
      fjs_advance(lx);
      bool closed = false;
      while (lx->p < end) {
        if (lx->p + 1 < end && lx->p[0] == '*' && lx->p[1] == '/') {
          fjs_advance(lx);
          fjs_advance(lx);
          closed = true;
          break;
        }
        fjs_advance(lx);
      }
      if (!closed) { fjs_lexer_error(lx, "unterminated block comment"); return false; }
      continue;
    }
    break;
  }
  return true;
}

/* ---- keyword table ----------------------------------------------------- */

static const struct { const char *name; fjs_kw kw; } keywords[] = {
  {"break", FJS_KW_BREAK}, {"case", FJS_KW_CASE}, {"catch", FJS_KW_CATCH},
  {"class", FJS_KW_CLASS}, {"const", FJS_KW_CONST}, {"continue", FJS_KW_CONTINUE},
  {"debugger", FJS_KW_DEBUGGER}, {"default", FJS_KW_DEFAULT0}, {"delete", FJS_KW_DELETE},
  {"do", FJS_KW_DO}, {"else", FJS_KW_ELSE}, {"export", FJS_KW_EXPORT},
  {"extends", FJS_KW_EXTENDS}, {"finally", FJS_KW_FINALLY}, {"for", FJS_KW_FOR},
  {"function", FJS_KW_FN}, {"if", FJS_KW_IF}, {"import", FJS_KW_IMPORT},
  {"in", FJS_KW_IN}, {"instanceof", FJS_KW_INSTANCEOF}, {"let", FJS_KW_LET},
  {"new", FJS_KW_NEW}, {"return", FJS_KW_RETURN}, {"super", FJS_KW_SUPER},
  {"switch", FJS_KW_SWITCH}, {"this", FJS_KW_THIS}, {"throw", FJS_KW_THROW},
  {"try", FJS_KW_TRY}, {"typeof", FJS_KW_TYPEOF}, {"var", FJS_KW_VAR},
  {"void", FJS_KW_VOID}, {"while", FJS_KW_WHILE}, {"with", FJS_KW_WITH},
  {"yield", FJS_KW_YIELD}, {"async", FJS_KW_ASYNC}, {"await", FJS_KW_AWAIT},
  {"static", FJS_KW_STATIC}, {"get", FJS_KW_GET}, {"set", FJS_KW_SET},
  {"of", FJS_KW_OF}, {"from", FJS_KW_FROM}, {"as", FJS_KW_AS},
  {"true", FJS_KW_TRUE}, {"false", FJS_KW_FALSE}, {"null", FJS_KW_NULL},
  {"enum", FJS_KW_ENUM}, {"native", FJS_KW_NATIVE},
};

static fjs_kw lookup_keyword(fjs_slice s) {
  const size_t n = sizeof(keywords) / sizeof(keywords[0]);
  for (size_t i = 0; i < n; i++) {
    const char *kw = keywords[i].name;
    size_t klen = strlen(kw);
    if (klen == s.len && memcmp(s.ptr, kw, klen) == 0) return keywords[i].kw;
  }
  return 0;
}

/* ---- number scanning --------------------------------------------------- */

static void scan_number(fjs_lexer *lx) {
  const char *start = lx->p;
  const char *end = lx->src + lx->len;

  /* 0x / 0b / 0o */
  if (lx->p + 2 < end && lx->p[0] == '0') {
    char nxt = lx->p[1];
    if (nxt == 'x' || nxt == 'X') {
      lx->p += 2; lx->col += 2;
      while (lx->p < end && (fjs_is_hex(*lx->p) || *lx->p == '_')) { fjs_advance(lx); }
      fjs_span(lx, FJS_TK_NUM, start, (size_t)(lx->p - start));
      return;
    }
    if (nxt == 'b' || nxt == 'B') {
      lx->p += 2; lx->col += 2;
      while (lx->p < end && (*lx->p == '0' || *lx->p == '1' || *lx->p == '_')) { fjs_advance(lx); }
      fjs_span(lx, FJS_TK_NUM, start, (size_t)(lx->p - start));
      return;
    }
    if (nxt == 'o' || nxt == 'O') {
      lx->p += 2; lx->col += 2;
      while (lx->p < end && ((*lx->p >= '0' && *lx->p <= '7') || *lx->p == '_')) { fjs_advance(lx); }
      fjs_span(lx, FJS_TK_NUM, start, (size_t)(lx->p - start));
      return;
    }
  }

  /* decimal */
  bool dot = false, exp = false;
  while (lx->p < end) {
    char c = *lx->p;
    if (fjs_is_digit(c) || c == '_') { fjs_advance(lx); }
    else if (c == '.' && !dot && !exp && !(lx->p + 1 < end && lx->p[1] == '.')) {
      dot = true; fjs_advance(lx);
    } else if ((c == 'e' || c == 'E') && !exp) {
      exp = true; fjs_advance(lx);
      if (lx->p < end && (lx->p[0] == '+' || lx->p[0] == '-')) fjs_advance(lx);
      if (!(lx->p < end && fjs_is_digit(*lx->p))) {
        fjs_lexer_error(lx, "invalid exponent");
        return;
      }
    } else {
      break;
    }
  }
  fjs_span(lx, FJS_TK_NUM, start, (size_t)(lx->p - start));
}

/* ---- string scanning --------------------------------------------------- */

static void scan_string(fjs_lexer *lx, char quote) {
  const char *start = lx->p; /* includes opening quote */
  fjs_buf cooked; fjs_buf_init(&cooked);
  const char *end = lx->src + lx->len;

  fjs_advance(lx); /* skip opening quote */
  for (;;) {
    if (lx->p >= end) {
      fjs_lexer_error(lx, "unterminated string literal");
      fjs_buf_free(&cooked);
      return;
    }
    char c = *lx->p;
    if (c == quote) {
      fjs_advance(lx); /* skip closing quote */
      break;
    }
    if (c == '\n' || c == '\r') {
      fjs_lexer_error(lx, "newline in string literal");
      fjs_buf_free(&cooked);
      return;
    }
    if (c == '\\') {
      fjs_advance(lx);
      if (lx->p >= end) {
        fjs_lexer_error(lx, "unterminated escape");
        fjs_buf_free(&cooked);
        return;
      }
      char esc = *lx->p;
      switch (esc) {
        case 'n': fjs_buf_putc(&cooked, '\n'); break;
        case 't': fjs_buf_putc(&cooked, '\t'); break;
        case 'r': fjs_buf_putc(&cooked, '\r'); break;
        case 'b': fjs_buf_putc(&cooked, '\b'); break;
        case 'f': fjs_buf_putc(&cooked, '\f'); break;
        case '0': fjs_buf_putc(&cooked, '\0'); break;
        case '"': fjs_buf_putc(&cooked, '"'); break;
        case '\'': fjs_buf_putc(&cooked, '\''); break;
        case '\\': fjs_buf_putc(&cooked, '\\'); break;
        case 'x': {
          fjs_advance(lx);
          char hi = fjs_is_hex(*lx->p) ? *lx->p : 0; fjs_advance(lx);
          char lo = fjs_is_hex(*lx->p) ? *lx->p : 0;
          if (hi) { fjs_advance(lx); }
          if (lo) { fjs_advance(lx); } else { /* use hi only */ break; }
          /* produce extracted value */
          if (hi) {
            int v = fjs_is_digit(hi) ? hi - '0' : (hi - 'A' + 10) >= 10 ? hi - 'A' + 10 : hi - 'a' + 10;
            fjs_buf_putc(&cooked, (char)(v * 16 +
              (fjs_is_digit(lo) ? lo - '0' : (lo - 'A' + 10) >= 10 ? lo - 'A' + 10 : lo - 'a' + 10)));
          }
          continue;
        }
        case 'u': {
          fjs_advance(lx);
          if (*lx->p == '{') {
            fjs_advance(lx);
            unsigned cp = 0;
            while (lx->p < end && *lx->p != '}' && fjs_is_hex(*lx->p)) {
              cp = cp * 16 + (*lx->p <= '9' ? *lx->p - '0' : (*lx->p <= 'F' ? *lx->p - 'A' + 10 : *lx->p - 'a' + 10));
              fjs_advance(lx);
            }
            if (lx->p < end && *lx->p == '}') fjs_advance(lx);
            /* utf-8 encode */
            if (cp < 0x80) fjs_buf_putc(&cooked, cp);
            else if (cp < 0x800) { fjs_buf_putc(&cooked, 0xC0 | (cp >> 6)); fjs_buf_putc(&cooked, 0x80 | (cp & 0x3F)); }
            else if (cp < 0x10000) { fjs_buf_putc(&cooked, 0xE0 | (cp >> 12)); fjs_buf_putc(&cooked, 0x80 | ((cp >> 6) & 0x3F)); fjs_buf_putc(&cooked, 0x80 | (cp & 0x3F)); }
            else { fjs_buf_putc(&cooked, 0xF0 | (cp >> 18)); fjs_buf_putc(&cooked, 0x80 | ((cp >> 12) & 0x3F)); fjs_buf_putc(&cooked, 0x80 | ((cp >> 6) & 0x3F)); fjs_buf_putc(&cooked, 0x80 | (cp & 0x3F)); }
          } else {
            unsigned cp = 0;
            for (int i = 0; i < 4 && lx->p < end && fjs_is_hex(*lx->p); i++) {
              cp = cp * 16 + (*lx->p <= '9' ? *lx->p - '0' : (*lx->p <= 'F' ? *lx->p - 'A' + 10 : *lx->p - 'a' + 10));
              fjs_advance(lx);
            }
            if (cp < 0x80) fjs_buf_putc(&cooked, cp);
            else { fjs_buf_putc(&cooked, 0xC0 | (cp >> 6)); fjs_buf_putc(&cooked, 0x80 | (cp & 0x3F)); }
          }
          continue;
        }
        default: fjs_buf_putc(&cooked, esc); break;
      }
      fjs_advance(lx);
    } else {
      fjs_buf_putc(&cooked, c);
      fjs_advance(lx);
    }
  }

  lx->tok.kind = FJS_TK_STRING;
  lx->tok.txt = (fjs_slice){start, (size_t)(lx->p - start)};
  /* Transfer cooked data to a newly allocated buffer (we keep it simple:
   * value points into the bss/static heap -- but we steal from fjs_buf). */
  if (cooked.len > 0) {
    char *v = (char *)malloc(cooked.len);
    if (v) { memcpy(v, cooked.data, cooked.len); lx->tok.value = (fjs_slice){v, cooked.len}; }
  }
  fjs_buf_free(&cooked);
}

/* ---- template literal scanning (entry after opening `) ----------------- */

static void scan_template(fjs_lexer *lx) {
  fjs_buf cooked; fjs_buf_init(&cooked);
  const char *start = lx->p - 1; /* includes opening backtick */
  const char *end = lx->src + lx->len;

  for (;;) {
    if (lx->p >= end) { fjs_lexer_error(lx, "unterminated template literal"); fjs_buf_free(&cooked); return; }
    char c = *lx->p;
    if (c == '`') {
      fjs_advance(lx);
      lx->tok.kind = lx->tok.kind == FJS_TK_TEMPLATE_TAIL ? FJS_TK_TEMPLATE_TAIL : FJS_TK_TEMPLATE;
      lx->tok.txt = (fjs_slice){start, (size_t)(lx->p - start)};
      if (cooked.len > 0) { char *v = (char *)malloc(cooked.len); memcpy(v, cooked.data, cooked.len); lx->tok.value = (fjs_slice){v, cooked.len}; }
      lx->tok.template_head = false;
      fjs_buf_free(&cooked);
      return;
    }
    if (c == '$' && lx->p + 1 < end && lx->p[1] == '{') {
      fjs_advance(lx); /* $ */
      fjs_advance(lx); /* { */
      lx->tok.kind = FJS_TK_TEMPLATE;
      lx->tok.txt = (fjs_slice){start, (size_t)(lx->p - start - 2)}; /* everything before ${ */
      /* subtract the 2 chars we just advanced past? Actually start through p-2 */
      lx->tok.txt = (fjs_slice){start, (size_t)((lx->p - 2) - start)};
      if (cooked.len > 0) { char *v = (char *)malloc(cooked.len); memcpy(v, cooked.data, cooked.len); lx->tok.value = (fjs_slice){v, cooked.len}; }
      lx->tok.template_head = true; /* more chunks follow */
      fjs_buf_free(&cooked);
      return;
    }
    if (c == '\\') {
      fjs_advance(lx);
      if (lx->p >= end) { fjs_lexer_error(lx, "unterminated template"); fjs_buf_free(&cooked); return; }
      char esc = *lx->p;
      switch (esc) {
        case 'n': fjs_buf_putc(&cooked, '\n'); break;
        case 't': fjs_buf_putc(&cooked, '\t'); break;
        case 'r': fjs_buf_putc(&cooked, '\r'); break;
        case '\\': fjs_buf_putc(&cooked, '\\'); break;
        case '`': fjs_buf_putc(&cooked, '`'); break;
        case '$': fjs_buf_putc(&cooked, '$'); break; /* \${ → literal */
        default: fjs_buf_putc(&cooked, esc); break;
      }
      fjs_advance(lx);
    } else {
      fjs_buf_putc(&cooked, c);
      fjs_advance(lx);
    }
  }
}

/* ---- regex scanning ---------------------------------------------------- */

static void scan_regex(fjs_lexer *lx) {
  const char *start = lx->p;
  const char *end = lx->src + lx->len;
  fjs_advance(lx); /* skip / */
  bool in_class = false;
  for (;;) {
    if (lx->p >= end) { fjs_lexer_error(lx, "unterminated regex"); return; }
    char c = *lx->p;
    if (c == '/' && !in_class) { fjs_advance(lx); break; }
    if (c == '[') in_class = true;
    else if (c == ']') in_class = false;
    else if (c == '\\') { fjs_advance(lx); if (lx->p >= end) break; }
    fjs_advance(lx);
  }
  /* flags (sequence of id_cont chars after closing /) */
  while (lx->p < end && fjs_is_id_cont(*lx->p)) fjs_advance(lx);
  lx->tok.kind = FJS_TK_REGEX;
  lx->tok.txt = (fjs_slice){start, (size_t)(lx->p - start)};
}

/* ---- public: init ------------------------------------------------------ */

void fjs_lexer_init(fjs_lexer *lx, const char *file, const char *src, size_t len) {
  *lx = (fjs_lexer){
    .file = file,
    .src = src,
    .len = len,
    .p = src,
    .ln = 1,
    .col = 1,
    .prev_was_regex_ok = false,
    .err = FJS_ERROR_OK,
    .tok = {.kind = FJS_TK_INVALID},
  };
}

void fjs_lexer_free(fjs_lexer *lx) {
  (void)lx;
  /* slices borrow from caller; nothing to free currently */
}

bool fjs_lexer_regex_allowed(fjs_lexer *lx) { return lx->prev_was_regex_ok; }

/* ---- public: next token ------------------------------------------------ */

fjs_tk_kind fjs_lexer_next(fjs_lexer *lx) {
  if (lx->err.kind != FJS_ERR_NONE) return (lx->tok.kind = FJS_TK_EOF);
  if (!fjs_skip_trivia(lx)) return (lx->tok.kind = FJS_TK_EOF);

  const char *end = lx->src + lx->len;
  if (lx->p >= end) {
    lx->tok.kind = FJS_TK_EOF;
    return FJS_TK_EOF;
  }

  char c = *lx->p;
  const char *start = lx->p;
  lx->tok.txt = (fjs_slice){NULL, 0};
  lx->tok.value = (fjs_slice){NULL, 0};
  lx->tok.kw = 0;
  lx->tok.template_head = false;
  lx->tok.loc = (fjs_loc){lx->file, lx->ln, lx->col};

  /* ---- identifier or keyword ------------------------------------------ */
  if (fjs_is_id_start(c) || c == '\\') { /* \u escapes start ident */
    bool unicode_escape = (c == '\\');
    if (unicode_escape) { fjs_advance(lx); /* skip \ */ }
    start = lx->p - (c == '\\' ? 1 : 0);
    while (lx->p < end && fjs_is_id_cont(*lx->p)) fjs_advance(lx);
    /* also slurp any following unicode escape in ident */
    while (lx->p + 5 < end && lx->p[0] == '\\' && lx->p[1] == 'u') { fjs_advance(lx); fjs_advance(lx); if (lx->p[0] == '{') { while (lx->p < end && *lx->p != '}') fjs_advance(lx); fjs_advance(lx); } else { for (int i=0; i<4 && lx->p<end; i++) fjs_advance(lx); } }
    size_t slen = (size_t)(lx->p - start);
    fjs_slice name = {start, slen};
    fjs_kw kw = lookup_keyword(name);
    if (kw) {
      lx->tok.kind = FJS_TK_KW;
      lx->tok.kw = kw;
    } else {
      lx->tok.kind = FJS_TK_IDENT;
    }
    lx->tok.txt = name;
    lx->prev_was_regex_ok = false; /* ident after ident → regex not ok */
    return lx->tok.kind;
  }

  /* ---- private name (#ident) ------------------------------------------- */
  if (c == '#') {
    /* Check if previous token allows private name (membership context) */
    fjs_advance(lx);
    if (lx->p < end && fjs_is_id_start(*lx->p)) {
      start = lx->p - 1;
      while (lx->p < end && fjs_is_id_cont(*lx->p)) fjs_advance(lx);
      lx->tok.kind = FJS_TK_PRIVATE_NAME;
      lx->tok.txt = (fjs_slice){start, (size_t)(lx->p - start)};
      lx->prev_was_regex_ok = false;
    } else {
      lx->tok.kind = FJS_TK_HASH;
      lx->tok.txt = (fjs_slice){"#", 1};
    }
    return lx->tok.kind;
  }

  /* ---- number ---------------------------------------------------------- */
  if (fjs_is_digit(c)) {
    scan_number(lx);
    lx->prev_was_regex_ok = false;
    return lx->tok.kind;
  }

  /* ---- string ----------------------------------------------------------- */
  if (c == '"' || c == '\'') {
    scan_string(lx, c);
    lx->prev_was_regex_ok = false;
    return lx->tok.kind;
  }

  /* ---- template literal ------------------------------------------------- */
  if (c == '`') {
    fjs_advance(lx);
    scan_template(lx);
    lx->prev_was_regex_ok = false;
    return lx->tok.kind;
  }

  /* ---- regex (detected by context) -------------------------------------- */
  if (c == '/' && lx->prev_was_regex_ok) {
    scan_regex(lx);
    lx->prev_was_regex_ok = false;
    return lx->tok.kind;
  }

  /* ---- punctuation / operators ----------------------------------------- */
  fjs_advance(lx);
  fjs_tk_kind result = FJS_TK_INVALID;
  switch (c) {
    case '(':
      result = FJS_TK_LPAREN;
      break;
    case ')':
      result = FJS_TK_RPAREN;
      break;
    case '{':
      result = FJS_TK_LBRACE;
      break;
    case '}':
      result = FJS_TK_RBRACE;
      break;
    case '[':
      result = FJS_TK_LBRACKET;
      break;
    case ']':
      result = FJS_TK_RBRACKET;
      break;
    case ';':
      result = FJS_TK_SEMI;
      break;
    case ',':
      result = FJS_TK_COMMA;
      break;
    case '.':
      if (lx->p < end && lx->p[0] == '.' && lx->p + 1 < end && lx->p[1] == '.') {
        fjs_advance(lx); fjs_advance(lx);
        result = FJS_TK_ELLIPSIS;
      } else if (lx->p < end && fjs_is_digit(*lx->p)) {
        /* number starting with .5 etc. */
        lx->p--; lx->col--;
        scan_number(lx);
        result = lx->tok.kind;
      } else {
        result = FJS_TK_DOT;
      }
      break;
    case '?':
      if (lx->p < end && lx->p[0] == '.') {
        fjs_advance(lx);
        result = FJS_TK_QUESTION_DOT;
      } else if (lx->p < end && lx->p[0] == '?') {
        fjs_advance(lx);
        result = FJS_TK_NULLISH;
      } else {
        result = FJS_TK_QUESTION;
      }
      break;
    case ':':
      result = FJS_TK_COLON;
      break;
    case '@':
      result = FJS_TK_AT;
      break;
    case '~':
      result = FJS_TK_TILDE;
      break;
    case '!':
      if (lx->p < end && lx->p[0] == '=') {
        fjs_advance(lx);
        result = FJS_TK_NE;
        if (lx->p < end && lx->p[0] == '=') { fjs_advance(lx); result = FJS_TK_STRICT_NE; }
      } else {
        result = FJS_TK_BANG;
      }
      break;
    case '+':
      if (lx->p < end && lx->p[0] == '+') { fjs_advance(lx); result = FJS_TK_INCR; }
      else if (lx->p < end && lx->p[0] == '=') { fjs_advance(lx); result = FJS_TK_PLUS_ASSIGN; }
      else { result = FJS_TK_PLUS; }
      break;
    case '-':
      if (lx->p < end && lx->p[0] == '-') { fjs_advance(lx); result = FJS_TK_DECR; }
      else if (lx->p < end && lx->p[0] == '=') { fjs_advance(lx); result = FJS_TK_MINUS_ASSIGN; }
      else if (lx->p < end && lx->p[0] == '>') { fjs_advance(lx); result = FJS_TK_ARROW; }
      else { result = FJS_TK_MINUS; }
      break;
    case '*':
      if (lx->p < end && lx->p[0] == '*') {
        fjs_advance(lx);
        result = lx->p < end && lx->p[0] == '=' ? (fjs_advance(lx), FJS_TK_STAR_STAR_ASSIGN) : FJS_TK_STAR_STAR;
      } else if (lx->p < end && lx->p[0] == '=') { fjs_advance(lx); result = FJS_TK_STAR_ASSIGN; }
      else { result = FJS_TK_STAR; }
      break;
    case '/':
      if (lx->p < end && lx->p[0] == '=') { fjs_advance(lx); result = FJS_TK_SLASH_ASSIGN; }
      else { /* Should have been caught by regex check above */ result = FJS_TK_SLASH; }
      break;
    case '%':
      result = lx->p < end && lx->p[0] == '=' ? (fjs_advance(lx), FJS_TK_PERCENT_ASSIGN) : FJS_TK_PERCENT;
      break;
    case '&':
      if (lx->p < end && lx->p[0] == '&') { fjs_advance(lx); result = FJS_TK_AND; }
      else if (lx->p < end && lx->p[0] == '=') { fjs_advance(lx); result = FJS_TK_AMP_ASSIGN; }
      else { result = FJS_TK_AMP; }
      break;
    case '|':
      if (lx->p < end && lx->p[0] == '|') { fjs_advance(lx); result = FJS_TK_OR; }
      else if (lx->p < end && lx->p[0] == '=') { fjs_advance(lx); result = FJS_TK_PIPE_ASSIGN; }
      else { result = FJS_TK_PIPE; }
      break;
    case '^':
      result = lx->p < end && lx->p[0] == '=' ? (fjs_advance(lx), FJS_TK_CARET_ASSIGN) : FJS_TK_CARET;
      break;
    case '<':
      if (lx->p < end && lx->p[0] == '<') {
        fjs_advance(lx);
        result = lx->p < end && lx->p[0] == '=' ? (fjs_advance(lx), FJS_TK_SHL_ASSIGN) : FJS_TK_SHL;
      } else if (lx->p < end && lx->p[0] == '=') { fjs_advance(lx); result = FJS_TK_LE; }
      else { result = FJS_TK_LT; }
      break;
    case '>':
      if (lx->p < end && lx->p[0] == '>') {
        fjs_advance(lx);
        if (lx->p < end && lx->p[0] == '>') {
          fjs_advance(lx);
          result = lx->p < end && lx->p[0] == '=' ? (fjs_advance(lx), FJS_TK_USHR_ASSIGN) : FJS_TK_USHR;
        } else {
          result = lx->p < end && lx->p[0] == '=' ? (fjs_advance(lx), FJS_TK_SHR_ASSIGN) : FJS_TK_SHR;
        }
      } else if (lx->p < end && lx->p[0] == '=') { fjs_advance(lx); result = FJS_TK_GE; }
      else { result = FJS_TK_GT; }
      break;
    case '=':
      if (lx->p < end && lx->p[0] == '=') {
        fjs_advance(lx);
        result = lx->p < end && lx->p[0] == '=' ? (fjs_advance(lx), FJS_TK_STRICT_EQ) : FJS_TK_EQ;
      } else if (lx->p < end && lx->p[0] == '>') { fjs_advance(lx); result = FJS_TK_ARROW; }
      else { result = FJS_TK_ASSIGN; }
      break;
    default:
      fjs_lexer_error(lx, "unexpected character");
      return lx->tok.kind;
  }

  lx->tok.kind = result;
  lx->tok.txt = (fjs_slice){start, (size_t)(lx->p - start)};

  /* Set prev_was_regex_ok: after these tokens, a / starts a regex */
  switch (result) {
    case FJS_TK_ASSIGN:
    case FJS_TK_LPAREN:
    case FJS_TK_LBRACKET:
    case FJS_TK_LBRACE:
    case FJS_TK_SEMI:
    case FJS_TK_COMMA:
    case FJS_TK_BANG:
    case FJS_TK_QUESTION:
    case FJS_TK_COLON:
    case FJS_TK_AND:
    case FJS_TK_OR:
    case FJS_TK_PLUS:
    case FJS_TK_MINUS:
    case FJS_TK_STAR:
    case FJS_TK_SLASH:
    case FJS_TK_PERCENT:
    case FJS_TK_TILDE:
    case FJS_TK_AMP:
    case FJS_TK_PIPE:
    case FJS_TK_CARET:
    case FJS_TK_LT:
    case FJS_TK_GT:
    case FJS_TK_LE:
    case FJS_TK_GE:
    case FJS_TK_EQ:
    case FJS_TK_NE:
    case FJS_TK_STRICT_EQ:
    case FJS_TK_STRICT_NE:
    case FJS_TK_SHL:
    case FJS_TK_SHR:
    case FJS_TK_USHR:
    case FJS_TK_NULLISH:
    case FJS_TK_KW:
      if (result == FJS_TK_KW && lx->tok.kw == FJS_KW_RETURN) lx->prev_was_regex_ok = true;
      else lx->prev_was_regex_ok = true;
      break;
    default:
      lx->prev_was_regex_ok = false;
      break;
  }

  return result;
}

/* ---- resume template after substitution -------------------------------- */

fjs_tk_kind fjs_lexer_resume_template(fjs_lexer *lx) {
  /* After ${...} the lexer is positioned on the character after }.
   * We resume scanning the template literal body. */
  lx->tok.kind = FJS_TK_TEMPLATE_TAIL; /* placeholder; scan fills it */
  fjs_skip_trivia(lx);
  scan_template(lx);
  lx->prev_was_regex_ok = false;
  return lx->tok.kind;
}
