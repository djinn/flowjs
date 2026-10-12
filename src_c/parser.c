/* flowjs parser -- recursive descent ECMA-262 (subset) */
#include "flowjs.h"
#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct {
  fjs_lexer lx;
  fjs_ab b;
  fjs_token lookahead;
  bool had_lookahead;
} fjs_parser;

static fjs_token peek(fjs_parser *p) {
  if (!p->had_lookahead) {
    fjs_lexer_next(&p->lx);
    p->lookahead = p->lx.tok;
    p->had_lookahead = true;
  }
  return p->lookahead;
}
static fjs_tk_kind peek_kind(fjs_parser *p) { return peek(p).kind; }
static fjs_token consume(fjs_parser *p) {
  fjs_token t;
  if (p->had_lookahead) { t = p->lookahead; p->had_lookahead = false; }
  else { fjs_lexer_next(&p->lx); t = p->lx.tok; }
  return t;
}
static bool match_kw(fjs_parser *p, fjs_kw kw) {
  if (peek_kind(p) == FJS_TK_KW && peek(p).kw == kw) { consume(p); return true; }
  return false;
}
static bool expect_kw(fjs_parser *p, fjs_kw kw) {
  if (match_kw(p, kw)) return true;
  fjs_lexer_error(&p->lx, "expected keyword"); return false;
}
static bool match(fjs_parser *p, fjs_tk_kind k) {
  if (peek_kind(p) == k) { consume(p); return true; }
  return false;
}
static bool expect(fjs_parser *p, fjs_tk_kind k) {
  if (peek_kind(p) == k) { consume(p); return true; }
  fjs_lexer_error(&p->lx, "expected token"); return false;
}
static fjs_ast *pos(fjs_parser *p, fjs_ast_kind k, fjs_token *tok) {
  fjs_ast *n = fjs_make_ast(&p->b, k);
  if (n && tok) n->loc = (fjs_loc){tok->loc.file, tok->loc.ln, tok->loc.col};
  return n;
}
static fjs_ast *mkident(fjs_parser *p, fjs_slice txt) {
  fjs_ast *n = fjs_make_ast(&p->b, AST_IDENT);
  if (n) n->txt = txt;
  return n;
}

/* forward */
static fjs_ast *parse_expr(fjs_parser *p);
static fjs_ast *parse_stmt(fjs_parser *p);
static fjs_ast *parse_block(fjs_parser *p);
static void parse_stmts(fjs_parser *p, fjs_ast_list *list);
static fjs_ast *parse_fn(fjs_parser *p, fjs_token *kw_tok);
static fjs_ast *parse_class(fjs_parser *p, fjs_token *kw_tok);
static fjs_ast *parse_obj(fjs_parser *p, fjs_token *brace);
static fjs_ast *parse_pattern(fjs_parser *p);

/* ---- entry point -------------------------------------------------------- */

fjs_parse_result fjs_parse_source(const char *file, const char *src, size_t len) {
  fjs_parse_result res = {NULL, FJS_ERROR_OK};
  fjs_parser p;
  fjs_lexer_init(&p.lx, file, src, len);
  fjs_ab_init(&p.b);
  p.had_lookahead = false;
  p.lookahead = (fjs_token){0};
  res.root = fjs_make_ast(&p.b, AST_PROGRAM);
  if (!res.root) { res.err = p.b.err; return res; }
  parse_stmts(&p, &res.root->list);
  if (p.b.err.kind != FJS_ERR_NONE) { res.err = p.b.err; res.root = NULL; }
  else if (p.lx.err.kind != FJS_ERR_NONE) { res.err = p.lx.err; res.root = NULL; }
  return res;
}

static void parse_stmts(fjs_parser *p, fjs_ast_list *list) {
  while (peek_kind(p) != FJS_TK_EOF && peek_kind(p) != FJS_TK_RBRACE) {
    fjs_ast *s = parse_stmt(p);
    if (s) fjs_list_push(&p->b, list, s);
    else break;
    if (p->lx.err.kind != FJS_ERR_NONE) break;
  }
}

static fjs_ast *parse_block(fjs_parser *p) {
  fjs_ast *n = fjs_make_ast(&p->b, AST_BLOCK);
  if (!n) return NULL;
 if (peek_kind(p) == FJS_TK_LBRACE) consume(p);
  parse_stmts(p, &n->list);
  expect(p, FJS_TK_RBRACE);
  return n;
}

/* ---- statements --------------------------------------------------------- */

static fjs_ast *parse_stmt(fjs_parser *p) {
  fjs_token t = peek(p);
  if (peek_kind(p) == FJS_TK_SEMI) { consume(p); return pos(p, AST_EMPTY, &t); }
  if (peek_kind(p) == FJS_TK_LBRACE) { return parse_block(p); }

  if (match_kw(p, FJS_KW_VAR) || match_kw(p, FJS_KW_LET) || match_kw(p, FJS_KW_CONST)) {
    fjs_ast *n = pos(p, AST_VAR_DECL, &t);
    if (!n) return NULL;
    n->txt = t.txt;
    do {
      fjs_ast *decl = fjs_make_ast(&p->b, AST_VAR_DECLARATOR);
      if (decl) {
        if (peek_kind(p) == FJS_TK_LBRACKET || peek_kind(p) == FJS_TK_LBRACE)
          decl->a = parse_pattern(p);
        else
          decl->a = mkident(p, consume(p).txt);
        if (match(p, FJS_TK_ASSIGN)) decl->b = parse_expr(p);
        fjs_list_push(&p->b, &n->list, decl);
      }
    } while (match(p, FJS_TK_COMMA));
    if (peek_kind(p) == FJS_TK_SEMI) consume(p);
    return n;
  }

  if (match_kw(p, FJS_KW_IF)) {
    fjs_ast *n = pos(p, AST_IF, &t);
    expect(p, FJS_TK_LPAREN); n->a = parse_expr(p); expect(p, FJS_TK_RPAREN);
    n->b = parse_stmt(p);
    if (match_kw(p, FJS_KW_ELSE)) n->c = parse_stmt(p);
    return n;
  }
  if (match_kw(p, FJS_KW_WHILE)) {
    fjs_ast *n = pos(p, AST_WHILE, &t);
    expect(p, FJS_TK_LPAREN); n->a = parse_expr(p); expect(p, FJS_TK_RPAREN);
    n->b = parse_stmt(p);
    return n;
  }
  if (match_kw(p, FJS_KW_DO)) {
    fjs_ast *n = pos(p, AST_DO_WHILE, &t);
    n->a = parse_stmt(p);
    expect_kw(p, FJS_KW_WHILE); expect(p, FJS_TK_LPAREN);
    n->b = parse_expr(p); expect(p, FJS_TK_RPAREN);
    if (peek_kind(p) == FJS_TK_SEMI) consume(p);
    return n;
  }
  if (match_kw(p, FJS_KW_FOR)) {
    fjs_ast *n = pos(p, AST_FOR, &t);
    expect(p, FJS_TK_LPAREN);
    if (peek_kind(p) == FJS_TK_SEMI) { consume(p); }
    else if (match_kw(p, FJS_KW_VAR) || match_kw(p, FJS_KW_LET) || match_kw(p, FJS_KW_CONST)) {
      fjs_ast *vd = pos(p, AST_VAR_DECL, &t);
      if (vd) { vd->txt = t.txt; n->a = vd; }
      do {
        fjs_token id_tok = consume(p);
        fjs_ast *decl = fjs_make_ast(&p->b, AST_VAR_DECLARATOR);
        if (decl) { decl->a = mkident(p, id_tok.txt);
          if (match(p, FJS_TK_ASSIGN)) decl->b = parse_expr(p);
          if (vd) fjs_list_push(&p->b, &vd->list, decl); }
      } while (match(p, FJS_TK_COMMA));
      if (match_kw(p, FJS_KW_IN)) n->kind = AST_FOR_IN;
      else if (match_kw(p, FJS_KW_OF)) n->kind = AST_FOR_OF;
      else {
        /* Normal for: consume semicolon after init */
        if (peek_kind(p) == FJS_TK_SEMI) { consume(p); }
      }
    } else {
      n->a = parse_expr(p);
      if (match_kw(p, FJS_KW_IN)) n->kind = AST_FOR_IN;
      else if (match_kw(p, FJS_KW_OF)) n->kind = AST_FOR_OF;
      else {
        if (peek_kind(p) == FJS_TK_SEMI) { consume(p); }
      }
    }
    if (n->kind == AST_FOR_IN || n->kind == AST_FOR_OF) { n->b = parse_expr(p); }
    else {
      if (peek_kind(p) != FJS_TK_SEMI) n->b = parse_expr(p);
      expect(p, FJS_TK_SEMI);
      if (peek_kind(p) != FJS_TK_RPAREN) n->c = parse_expr(p);
    }
    expect(p, FJS_TK_RPAREN);
    n->d = parse_stmt(p);
    return n;
  }
  if (match_kw(p, FJS_KW_SWITCH)) {
    fjs_ast *n = pos(p, AST_SWITCH, &t);
    expect(p, FJS_TK_LPAREN); n->a = parse_expr(p); expect(p, FJS_TK_RPAREN);
    expect(p, FJS_TK_LBRACE);
    while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF) {
      if (match_kw(p, FJS_KW_CASE)) {
        fjs_ast *c = pos(p, AST_SWITCH_CASE, &t);
        c->a = parse_expr(p); expect(p, FJS_TK_COLON);
        while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF &&
               !(peek_kind(p) == FJS_TK_KW && (peek(p).kw == FJS_KW_CASE || peek(p).kw == FJS_KW_DEFAULT0))) {
          fjs_ast *s = parse_stmt(p);
          if (s) fjs_list_push(&p->b, &c->list, s);
          if (p->lx.err.kind) break;
        }
        fjs_list_push(&p->b, &n->list, c);
      } else if (match_kw(p, FJS_KW_DEFAULT0)) {
        fjs_ast *c = pos(p, AST_SWITCH_CASE, &t);
        expect(p, FJS_TK_COLON);
        while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF &&
               peek_kind(p) != FJS_TK_KW && peek(p).kw != FJS_KW_CASE && peek(p).kw != FJS_KW_DEFAULT0) {
          fjs_ast *s = parse_stmt(p);
          if (s) fjs_list_push(&p->b, &c->list, s);
        }
        fjs_list_push(&p->b, &n->list, c);
      } else parse_stmt(p);
    }
    expect(p, FJS_TK_RBRACE);
    return n;
  }
  if (match_kw(p, FJS_KW_RETURN)) {
    fjs_ast *n = pos(p, AST_RETURN, &t);
    if (peek_kind(p) != FJS_TK_SEMI && peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF)
      n->a = parse_expr(p);
    if (peek_kind(p) == FJS_TK_SEMI) consume(p);
    return n;
  }
  if (match_kw(p, FJS_KW_BREAK)) { fjs_ast *n = pos(p, AST_BREAK, &t); if (peek_kind(p)==FJS_TK_SEMI) consume(p); return n; }
  if (match_kw(p, FJS_KW_CONTINUE)) { fjs_ast *n = pos(p, AST_CONTINUE, &t); if (peek_kind(p)==FJS_TK_SEMI) consume(p); return n; }
  if (match_kw(p, FJS_KW_THROW)) {
    fjs_ast *n = pos(p, AST_THROW, &t);
    n->a = parse_expr(p);
    if (peek_kind(p) == FJS_TK_SEMI) consume(p);
    return n;
  }
  if (match_kw(p, FJS_KW_TRY)) {
    fjs_ast *n = pos(p, AST_TRY, &t);
  
    n->a = parse_block(p);
    if (match_kw(p, FJS_KW_CATCH)) {
      if (match(p, FJS_TK_LPAREN)) { consume(p); expect(p, FJS_TK_RPAREN); }
    
      n->b = parse_block(p);
    }
    if (match_kw(p, FJS_KW_FINALLY)) {
      n->c = parse_block(p);
    }
    return n;
  }
  if (match_kw(p, FJS_KW_CLASS)) return parse_class(p, &t);
  if (match_kw(p, FJS_KW_FN)) { fjs_ast *fn = parse_fn(p, &t); if (fn) fn->kind = AST_FN_DECL; return fn; }
  if (match_kw(p, FJS_KW_EXPORT)) {
    fjs_ast *n = pos(p, AST_EXPORT_NAMED, &t);
    if (match_kw(p, FJS_KW_DEFAULT0)) { n->kind = AST_EXPORT_DEFAULT; n->a = parse_stmt(p); }
    else { n->a = parse_stmt(p); }
    return n;
  }
  if (match_kw(p, FJS_KW_IMPORT)) {
    fjs_ast *n = pos(p, AST_IMPORT_DECL, &t);
    fjs_token str = consume(p);
    if (str.kind == FJS_TK_STRING) {
      fjs_ast *s = pos(p, AST_STRING_LITERAL, &str);
      if (s) { s->txt = str.value.len ? str.value : str.txt; n->a = s; }
    }
    return n;
  }

  /* Labeled: ident : stmt */
  if (peek_kind(p) == FJS_TK_IDENT) {
    size_t save_p = (size_t)(p->lx.p - p->lx.src);
    size_t save_ln = p->lx.ln, save_col = p->lx.col;
    bool saved_had = p->had_lookahead;
    fjs_token save_la = p->lookahead;
    fjs_token id_tok = consume(p);
    if (peek_kind(p) == FJS_TK_COLON) { consume(p);
      fjs_ast *n = pos(p, AST_LABELED, &t);
      if (n) { n->txt = id_tok.txt; n->a = parse_stmt(p); } return n; }
    p->had_lookahead = saved_had; p->lookahead = save_la;
    p->lx.p = p->lx.src + save_p; p->lx.ln = save_ln; p->lx.col = save_col;
  }
  /* async function decl */
  if (peek_kind(p) == FJS_TK_KW && t.kw == FJS_KW_ASYNC) {
    consume(p);
    if (peek_kind(p) == FJS_TK_KW && peek(p).kw == FJS_KW_FN) {
      fjs_token ftok = peek(p);
      consume(p); /* consume function kw */
      fjs_ast *fn = pos(p, AST_FN_DECL, &ftok);
      if (!fn) return NULL;
      fn->flag = true; /* async */
      if (peek_kind(p) == FJS_TK_IDENT) fn->txt = consume(p).txt;
      expect(p, FJS_TK_LPAREN);
      while (peek_kind(p) != FJS_TK_RPAREN && peek_kind(p) != FJS_TK_EOF) {
        if (match(p, FJS_TK_ELLIPSIS)) {
          fjs_ast *r = pos(p, AST_REST_ELEMENT, &p->lookahead);
          if (r) { r->a = mkident(p, consume(p).txt); fjs_list_push(&p->b, &fn->list, r); } break;
        }
        fjs_list_push(&p->b, &fn->list, mkident(p, consume(p).txt));
        if (peek_kind(p) != FJS_TK_RPAREN) expect(p, FJS_TK_COMMA);
      }
      expect(p, FJS_TK_RPAREN);
      if (peek_kind(p) == FJS_TK_LBRACE) {  fn->a = parse_block(p); }
      return fn;
    }
  }

  fjs_ast *expr = parse_expr(p);
  if (!expr) return NULL;
  if (peek_kind(p) == FJS_TK_SEMI) consume(p);
  fjs_ast *n = pos(p, AST_EXPR_STMT, &t);
  if (n) n->a = expr;
  return n;
}
static fjs_ast *parse_pattern(fjs_parser *p) {
  fjs_token tt = peek(p);
  fjs_tk_kind kk = peek_kind(p);
  if (kk == FJS_TK_LBRACKET) {
    consume(p);
    fjs_ast *n = pos(p, AST_ARRAY_PATTERN, &tt);
    while (peek_kind(p) != FJS_TK_RBRACKET && peek_kind(p) != FJS_TK_EOF) {
      if (match(p, FJS_TK_COMMA)) { fjs_list_push(&p->b, &n->list, NULL); continue; }
      if (match(p, FJS_TK_ELLIPSIS)) {
        fjs_ast *r = pos(p, AST_REST_ELEMENT, &p->lookahead);
        if (r) { r->a = parse_pattern(p); fjs_list_push(&p->b, &n->list, r); } break;
      }
      fjs_ast *e = parse_pattern(p);
      if (e && match(p, FJS_TK_ASSIGN)) {
        fjs_ast *a = pos(p, AST_ASSIGN_PATTERN, &tt); if (a) { a->a = e; a->b = parse_expr(p); e = a; } }
      if (e) fjs_list_push(&p->b, &n->list, e);
      if (peek_kind(p) != FJS_TK_RBRACKET) expect(p, FJS_TK_COMMA);
    }
    expect(p, FJS_TK_RBRACKET); return n;
  }
  if (kk == FJS_TK_LBRACE) {
    consume(p);
    fjs_ast *n = pos(p, AST_OBJECT_PATTERN, &tt);
    while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF) {
      if (match(p, FJS_TK_ELLIPSIS)) {
        fjs_ast *r = pos(p, AST_REST_ELEMENT, &p->lookahead);
        if (r) { r->a = parse_pattern(p); fjs_list_push(&p->b, &n->list, r); } break;
      }
      fjs_token key = consume(p);
      fjs_ast *prop = fjs_make_ast(&p->b, AST_PROPERTY);
      if (!prop) return n; prop->txt = (fjs_slice){"init",4};
      if (key.kind == FJS_TK_IDENT || key.kind == FJS_TK_KW) {
        prop->a = mkident(p, key.txt);
        if (match(p, FJS_TK_COLON)) prop->b = parse_pattern(p);
        else { prop->b = mkident(p, key.txt); prop->flag = true; }
      } else { fjs_lexer_error(&p->lx, "unexpected pattern element"); return n; }
      fjs_list_push(&p->b, &n->list, prop);
      if (peek_kind(p) != FJS_TK_RBRACE) match(p, FJS_TK_COMMA);
    }
    expect(p, FJS_TK_RBRACE); return n;
  }
  if (kk == FJS_TK_IDENT) {
    consume(p); fjs_ast *n = mkident(p, tt.txt);
    if (match(p, FJS_TK_ASSIGN)) {
      fjs_ast *a = pos(p, AST_ASSIGN_PATTERN, &tt); if (a) { a->a = n; a->b = parse_expr(p); return a; } }
    return n;
  }
  fjs_lexer_error(&p->lx, "expected pattern element");
  return NULL;
}

/* == EXPRESSIONS ========================================================== */

static fjs_ast *parse_primary(fjs_parser *p) {
  fjs_token t = consume(p);
  if (t.kind == FJS_TK_IDENT) return mkident(p, t.txt);
  if (t.kind == FJS_TK_KW) {
    if (t.kw == FJS_KW_THIS) { fjs_ast *n = mkident(p, (fjs_slice){"this",4}); if(n) n->loc = (fjs_loc){t.loc.file,t.loc.ln,t.loc.col}; return n; }
    if (t.kw == FJS_KW_SUPER) { fjs_ast *n = mkident(p, (fjs_slice){"super",5}); if(n) n->loc = (fjs_loc){t.loc.file,t.loc.ln,t.loc.col}; return n; }
    if (t.kw == FJS_KW_TRUE || t.kw == FJS_KW_FALSE) { fjs_ast *n = pos(p, AST_BOOL_LITERAL, &t); if(n) n->flag = (t.kw==FJS_KW_TRUE); return n; }
    if (t.kw == FJS_KW_NULL) return pos(p, AST_NULL_LITERAL, &t);
    if (t.kw == FJS_KW_FN) return parse_fn(p, &t);
    if (t.kw == FJS_KW_CLASS) { fjs_ast *c = parse_class(p, &t); if(c) c->kind = AST_CLASS_EXPR; return c; }
    fjs_lexer_error(&p->lx, "unexpected keyword in expression"); return NULL;
  }
  if (t.kind == FJS_TK_NUM) {
    fjs_ast *n = pos(p, AST_NUM_LITERAL, &t);
    if (n) { char buf[256]; size_t sl = t.txt.len<255?t.txt.len:255; memcpy(buf,t.txt.ptr,sl); buf[sl]=0; n->num = strtod(buf,NULL); }
    return n;
  }
  if (t.kind == FJS_TK_STRING) { fjs_ast *n = pos(p, AST_STRING_LITERAL, &t); if(n) n->txt = t.value.len ? t.value : t.txt; return n; }
  if (t.kind == FJS_TK_REGEX) return pos(p, AST_REGEX_LITERAL, &t);
  if (t.kind == FJS_TK_TEMPLATE || t.kind == FJS_TK_TEMPLATE_TAIL) {
    fjs_ast *n = pos(p, AST_TEMPLATE_LITERAL, &t);
    if (n) {
      fjs_ast *first = fjs_make_ast(&p->b, AST_STRING_LITERAL);
      if (first) { first->txt = t.value.len ? t.value : (fjs_slice){"",0}; fjs_list_push(&p->b, &n->list, first); }
      while (t.template_head) {
        fjs_ast *expr = parse_expr(p);
        if (expr) fjs_list_push(&p->b, &n->list, expr);
        /* After ${ expr }, resume scanning the template */
        p->had_lookahead = false; /* clear lookahead after resume */
        fjs_lexer_resume_template(&p->lx);
        t = p->lx.tok;
        if (t.kind == FJS_TK_TEMPLATE || t.kind == FJS_TK_TEMPLATE_TAIL) {
          fjs_ast *str = fjs_make_ast(&p->b, AST_STRING_LITERAL);
          if (str) { str->txt = t.value.len ? t.value : (fjs_slice){"",0}; fjs_list_push(&p->b, &n->list, str); }
        } else break;
      }
    }
    return n;
  }
  if (t.kind == FJS_TK_LPAREN) {
    fjs_ast *expr = parse_expr(p); expect(p, FJS_TK_RPAREN);
    fjs_ast *n = pos(p, AST_PAREN, &t); if(n) n->a = expr; return n;
  }
  if (t.kind == FJS_TK_LBRACKET) {
    fjs_ast *n = pos(p, AST_ARRAY_EXPR, &t);
    while (peek_kind(p)!=FJS_TK_RBRACKET && peek_kind(p)!=FJS_TK_EOF) {
      if (match(p, FJS_TK_COMMA)) { fjs_list_push(&p->b, &n->list, NULL); continue; }
      if (match(p, FJS_TK_ELLIPSIS)) { fjs_ast *s = pos(p, AST_SPREAD, &p->lookahead); if(s){s->a=parse_expr(p);fjs_list_push(&p->b,&n->list,s);} }
      else fjs_list_push(&p->b, &n->list, parse_expr(p));
      if (peek_kind(p)!=FJS_TK_RBRACKET) expect(p, FJS_TK_COMMA);
    }
    expect(p, FJS_TK_RBRACKET);
    return n;
  }
  if (t.kind == FJS_TK_LBRACE) return parse_obj(p, &t);
  if (t.kind == FJS_TK_ELLIPSIS) { fjs_ast *n = pos(p, AST_SPREAD, &t); if(n) n->a = parse_expr(p); return n; }
  fjs_lexer_error(&p->lx, "unexpected token in expression");
  return NULL;
}

static fjs_ast *parse_call_member(fjs_parser *p, fjs_ast *lhs) {
  fjs_token t = consume(p);
  if (t.kind == FJS_TK_DOT) {
    fjs_token id = consume(p);
    if (id.kind!=FJS_TK_IDENT && id.kind!=FJS_TK_KW) { fjs_lexer_error(&p->lx,"expected property name"); return lhs; }
    fjs_ast *n = pos(p, AST_MEMBER_EXPR, &t); if(n){n->a=lhs;n->b=mkident(p,id.txt);n->flag=false;} return n;
  }
  if (t.kind == FJS_TK_LBRACKET) {
    fjs_ast *idx = parse_expr(p); expect(p, FJS_TK_RBRACKET);
    fjs_ast *n = pos(p, AST_MEMBER_EXPR, &t); if(n){n->a=lhs;n->b=idx;n->flag=true;} return n;
  }
  if (t.kind == FJS_TK_QUESTION_DOT) {
    if (peek_kind(p) == FJS_TK_LPAREN) {
      consume(p);
      fjs_ast *n = pos(p, AST_OPTIONAL_CALL, &t); if(!n) return lhs;
      n->a = lhs;
      while (peek_kind(p)!=FJS_TK_RPAREN && peek_kind(p)!=FJS_TK_EOF) {
        fjs_list_push(&p->b,&n->list,parse_expr(p));
        if (peek_kind(p)!=FJS_TK_RPAREN) expect(p,FJS_TK_COMMA);
      }
      expect(p,FJS_TK_RPAREN); return n;
    }
    if (peek_kind(p) == FJS_TK_LBRACKET) {
      consume(p); fjs_ast *idx = parse_expr(p); expect(p, FJS_TK_RBRACKET);
      fjs_ast *n = pos(p, AST_OPTIONAL_MEMBER, &t); if(n){n->a=lhs;n->b=idx;n->flag=true;} return n;
    }
    fjs_token id = consume(p);
    if (id.kind!=FJS_TK_IDENT) { fjs_lexer_error(&p->lx,"expected ident after ?."); return lhs; }
    fjs_ast *n = pos(p, AST_OPTIONAL_MEMBER, &t); if(n){n->a=lhs;n->b=mkident(p,id.txt);n->flag=false;} return n;
  }
  if (t.kind == FJS_TK_LPAREN) {
    fjs_ast *n = pos(p, AST_CALL_EXPR, &t); if(!n) return lhs;
    n->a = lhs;
    while (peek_kind(p)!=FJS_TK_RPAREN && peek_kind(p)!=FJS_TK_EOF) {
      if (match(p, FJS_TK_ELLIPSIS)) { fjs_ast *s = pos(p, AST_SPREAD, &p->lookahead); if(s){s->a=parse_expr(p);fjs_list_push(&p->b,&n->list,s);} }
      else fjs_list_push(&p->b, &n->list, parse_expr(p));
      if (peek_kind(p)!=FJS_TK_RPAREN) expect(p, FJS_TK_COMMA);
    }
    expect(p, FJS_TK_RPAREN);
    return n;
  }
  return lhs;
}

static fjs_ast *parse_postfix(fjs_parser *p) {
  fjs_ast *lhs = parse_primary(p);
  if (!lhs) return NULL;
  for (;;) {
    fjs_tk_kind k = peek_kind(p);
    if (k==FJS_TK_LPAREN || k==FJS_TK_DOT || k==FJS_TK_LBRACKET || k==FJS_TK_QUESTION_DOT)
      lhs = parse_call_member(p, lhs);
    else if (k==FJS_TK_INCR || k==FJS_TK_DECR) {
      fjs_token op = consume(p);
      fjs_ast *n = pos(p, AST_UPDATE_EXPR, &op); if(n){n->a=lhs;n->txt=op.txt;n->flag=false;}
      lhs = n;
    } else break;
  }
  return lhs;
}

static fjs_ast *parse_unary(fjs_parser *p) {
  fjs_tk_kind k = peek_kind(p);
  fjs_token t = peek(p);
  if (k==FJS_TK_BANG||k==FJS_TK_TILDE||k==FJS_TK_PLUS||k==FJS_TK_MINUS||k==FJS_TK_INCR||k==FJS_TK_DECR) {
    consume(p);
    bool incdec = (k==FJS_TK_INCR||k==FJS_TK_DECR);
    fjs_ast *n = pos(p, incdec?AST_UPDATE_EXPR:AST_UNARY_EXPR, &t);
    if(n){n->a=parse_unary(p);n->txt=t.txt;if(incdec)n->flag=true;}
    return n;
  }
  /* keyword unary operators */
  if (k==FJS_TK_KW) {
    if (t.kw==FJS_KW_TYPEOF||t.kw==FJS_KW_VOID||t.kw==FJS_KW_DELETE) {
      consume(p);
      fjs_ast *n = pos(p, AST_UNARY_EXPR, &t);
      if(n){n->a=parse_unary(p);n->txt=t.txt;}
      return n;
    }
    if (t.kw==FJS_KW_AWAIT) {
      consume(p);
      fjs_ast *n = pos(p, AST_AWAIT, &t);
      if(n) n->a = parse_unary(p);
      return n;
    }
    if (t.kw==FJS_KW_YIELD) {
      consume(p);
      fjs_ast *n = pos(p, AST_YIELD, &t);
      if(n) {
        /* check for yield* (delegate) */
        if (peek_kind(p)==FJS_TK_STAR) { consume(p); n->flag = true; }
        /* yield can have no argument: yield; */
        fjs_tk_kind nk = peek_kind(p);
        if (nk != FJS_TK_SEMI && nk != FJS_TK_RBRACE && nk != FJS_TK_RPAREN && nk != FJS_TK_EOF)
          n->a = parse_unary(p);
      }
      return n;
    }
  }
  return parse_postfix(p);
}

static int binprec(fjs_tk_kind k, fjs_kw kw) {
  if (k==FJS_TK_STAR_STAR) return 14;
  if (k==FJS_TK_STAR||k==FJS_TK_SLASH||k==FJS_TK_PERCENT) return 13;
  if (k==FJS_TK_PLUS||k==FJS_TK_MINUS) return 12;
  if (k==FJS_TK_SHL||k==FJS_TK_SHR||k==FJS_TK_USHR) return 11;
  if (k==FJS_TK_LT||k==FJS_TK_GT||k==FJS_TK_LE||k==FJS_TK_GE) return 10;
  if (k==FJS_TK_EQ||k==FJS_TK_NE||k==FJS_TK_STRICT_EQ||k==FJS_TK_STRICT_NE) return 9;
  if (k==FJS_TK_KW && (kw==FJS_KW_IN||kw==FJS_KW_INSTANCEOF)) return 8;
  if (k==FJS_TK_AMP) return 7;
  if (k==FJS_TK_CARET) return 6;
  if (k==FJS_TK_PIPE) return 5;
  if (k==FJS_TK_AND) return 4;
  if (k==FJS_TK_OR) return 3; /* actually || is precedence 2 in JS? No: || is 4, ?? is 2. We invert */
  if (k==FJS_TK_NULLISH) return 2;
  return 0;
}
/* Fix: JS precedence: ||=3, &&=4, ??=2. We use: &&=4, ||=3, ??=2 */

static fjs_ast *parse_binary(fjs_parser *p, int min_prec) {
  fjs_ast *lhs = parse_unary(p);
  if (!lhs) return NULL;
  for (;;) {
    fjs_tk_kind k = peek_kind(p);
    fjs_kw kw = (k==FJS_TK_KW)?peek(p).kw:0;
    int prec = binprec(k,kw);
    if (prec==0 || prec<min_prec) break;
    fjs_token op = consume(p);
    int next = prec + ((k==FJS_TK_STAR_STAR)?0:1);
    fjs_ast *rhs = parse_binary(p, next);
    if (!rhs) break;
    fjs_ast_kind nk = (k==FJS_TK_AND||k==FJS_TK_OR||k==FJS_TK_NULLISH)?AST_LOGICAL_EXPR:AST_BINARY_EXPR;
    fjs_ast *n = pos(p, nk, &op); if(n){n->a=lhs;n->b=rhs;n->txt=op.txt;}
    lhs = n;
  }
  return lhs;
}

static fjs_ast *parse_cond(fjs_parser *p) {
  fjs_ast *lhs = parse_binary(p, 0);
  if (!lhs) return NULL;
  if (match(p, FJS_TK_QUESTION)) {
    fjs_ast *n = pos(p, AST_COND_EXPR, &p->lookahead);
    if(!n) return NULL;
    n->a=lhs; n->b=parse_expr(p); expect(p,FJS_TK_COLON); n->c=parse_expr(p); return n;
  }
  return lhs;
}

static fjs_ast *parse_assign(fjs_parser *p) {
  fjs_ast *lhs = parse_cond(p);
  if (!lhs) return NULL;
  fjs_tk_kind k = peek_kind(p);
  if (k>=FJS_TK_ASSIGN && k<=FJS_TK_NULLISH_ASSIGN) {
    fjs_token op = consume(p);
    fjs_ast *n = pos(p, AST_ASSIGN_EXPR, &op);
    if(n){n->a=lhs;n->b=parse_assign(p);n->txt=op.txt;}
    return n;
  }
  return lhs;
}

/* Arrow function: id => expr  or  (params) => expr/block */
/* We do a trial parse in parse_expr by first checking for these patterns */
static fjs_ast *parse_expr(fjs_parser *p) {
  /* try single ident arrow: peek ident and if next is => */
  if (peek_kind(p) == FJS_TK_IDENT) {
    /* Save state */
    size_t save_p = (size_t)(p->lx.p - p->lx.src);
    size_t save_ln = p->lx.ln, save_col = p->lx.col;
    fjs_token save_la = p->lookahead;
    bool saved_had = p->had_lookahead;

    fjs_token id_tok = consume(p);
    if (peek_kind(p) == FJS_TK_ARROW) {
      fjs_token arrow_tok = consume(p);
      fjs_ast *arrow_n = pos(p, AST_ARROW_FN, &arrow_tok);
      if (!arrow_n) return NULL;
      fjs_list_push(&p->b, &arrow_n->list, mkident(p, id_tok.txt));
      if (peek_kind(p) == FJS_TK_LBRACE) {  arrow_n->flag = false; arrow_n->a = parse_block(p); }
      else { arrow_n->flag = true; arrow_n->a = parse_expr(p); }
      return arrow_n;
    }
    /* Not arrow, restore state */
    p->lookahead = save_la;
    p->had_lookahead = saved_had;
    p->lx.p = p->lx.src + save_p;
    p->lx.ln = save_ln;
    p->lx.col = save_col;
  }

  /* try (params) => arrow */
  if (peek_kind(p) == FJS_TK_LPAREN) {
    size_t save_p = (size_t)(p->lx.p - p->lx.src);
    size_t save_ln = p->lx.ln, save_col = p->lx.col;
    fjs_token save_la = p->lookahead;
    bool saved_had = p->had_lookahead;

    consume(p); /* ( */
    fjs_ast_list params = {0};
    bool is_arrow = false;
    while (peek_kind(p) != FJS_TK_RPAREN && peek_kind(p) != FJS_TK_EOF) {
      if (peek_kind(p) == FJS_TK_ELLIPSIS) {
        fjs_token d = consume(p); fjs_ast *r = pos(p, AST_REST_ELEMENT, &d);
        if(r){r->a=mkident(p,consume(p).txt);fjs_list_push(&p->b,&params,r);}
        break;
      }
      fjs_token param = consume(p);
      fjs_list_push(&p->b, &params, mkident(p, param.txt));
      if (peek_kind(p) != FJS_TK_RPAREN) expect(p, FJS_TK_COMMA);
    }
    if (peek_kind(p) == FJS_TK_RPAREN) {
      consume(p);
      if (peek_kind(p) == FJS_TK_ARROW) is_arrow = true;
    }

    if (is_arrow) {
      fjs_token arrow_tok = consume(p);
      fjs_ast *arrow_n = pos(p, AST_ARROW_FN, &arrow_tok);
      if (!arrow_n) return NULL;
      /* transfer params */
      for (fjs_ast_list_node *ln = params.head; ln; ln = ln->next)
        fjs_list_push(&p->b, &arrow_n->list, ln->item);
      if (peek_kind(p) == FJS_TK_LBRACE) {  arrow_n->flag = false; arrow_n->a = parse_block(p); }
      else { arrow_n->flag = true; arrow_n->a = parse_expr(p); }
      return arrow_n;
    }

    /* Not arrow: restore */
    p->lookahead = save_la;
    p->had_lookahead = saved_had;
    p->lx.p = p->lx.src + save_p;
    p->lx.ln = save_ln;
    p->lx.col = save_col;
  }

  /* Normal expression */
  return parse_assign(p);
}

/* == FN / CLASS / OBJECT implementations ================================= */

static fjs_ast *parse_fn(fjs_parser *p, fjs_token *kw_tok) {
  fjs_ast *n = pos(p, AST_FN_EXPR, kw_tok);
  if (!n) return NULL;
  if (match(p, FJS_TK_STAR)) n->flag = true;
  if (peek_kind(p) == FJS_TK_IDENT || (peek_kind(p) == FJS_TK_KW && (peek(p).kw == FJS_KW_GET || peek(p).kw == FJS_KW_SET))) n->txt = consume(p).txt;
  expect(p, FJS_TK_LPAREN);
  while (peek_kind(p) != FJS_TK_RPAREN && peek_kind(p) != FJS_TK_EOF) {
    if (match(p, FJS_TK_ELLIPSIS)) {
      fjs_ast *r = pos(p, AST_REST_ELEMENT, &p->lookahead);
      if (r) { r->a = mkident(p, consume(p).txt); fjs_list_push(&p->b, &n->list, r); }
      break;
    }
    /* destructuring param */
    if (peek_kind(p) == FJS_TK_LBRACKET || peek_kind(p) == FJS_TK_LBRACE) {
      fjs_ast *pat = parse_pattern(p);
      if (pat) { fjs_list_push(&p->b, &n->list, pat); }
      if (peek_kind(p) != FJS_TK_RPAREN) expect(p, FJS_TK_COMMA);
      continue;
    }
    fjs_token id_tok = consume(p);
    fjs_list_push(&p->b, &n->list, mkident(p, id_tok.txt));
    if (peek_kind(p) != FJS_TK_RPAREN) expect(p, FJS_TK_COMMA);
  }
  expect(p, FJS_TK_RPAREN);
  /* parse_block consumes the opening {, so don't require it before */
  if (peek_kind(p) == FJS_TK_LBRACE) {  n->a = parse_block(p); }
  else fjs_lexer_error(&p->lx, "expected { for function body");
  return n;
}

static fjs_ast *parse_class(fjs_parser *p, fjs_token *kw_tok) {
  fjs_ast *n = pos(p, AST_CLASS_DECL, kw_tok);
  if (!n) return NULL;
  if (peek_kind(p) == FJS_TK_IDENT) n->a = mkident(p, consume(p).txt);
  if (match_kw(p, FJS_KW_EXTENDS)) n->b = parse_expr(p);
  expect(p, FJS_TK_LBRACE);
  while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF) {
    if (match_kw(p, FJS_KW_STATIC) || match_kw(p, FJS_KW_GET) || match_kw(p, FJS_KW_SET)) {}
    if (match_kw(p, FJS_KW_FN)) {
      fjs_ast *method = parse_fn(p, kw_tok);
      if (method) fjs_list_push(&p->b, &n->list, method);
    } else if (match(p, FJS_TK_SEMI)) {}
    else if (peek_kind(p) == FJS_TK_IDENT || peek_kind(p) == FJS_TK_KW) {
      fjs_token mname = consume(p);
      fjs_ast *method = pos(p, AST_FN_EXPR, &mname);
      if (method) {
        method->txt = mname.txt;
        /* check if followed by ( for method definition */
        if (peek_kind(p) == FJS_TK_LPAREN) {
          expect(p, FJS_TK_LPAREN);
          while (peek_kind(p) != FJS_TK_RPAREN && peek_kind(p) != FJS_TK_EOF) {
            fjs_list_push(&p->b, &method->list, mkident(p, consume(p).txt));
            if (peek_kind(p) != FJS_TK_RPAREN) expect(p, FJS_TK_COMMA);
          }
          expect(p, FJS_TK_RPAREN);
          if (peek_kind(p) == FJS_TK_LBRACE) {  method->a = parse_block(p); }
        } else {
          /* just a property name, no method body */
          method->a = fjs_make_ast(&p->b, AST_EMPTY);
        }
        fjs_list_push(&p->b, &n->list, method);
      }
    } else break;
  }
  expect(p, FJS_TK_RBRACE);
  return n;
}

static fjs_ast *parse_obj(fjs_parser *p, fjs_token *brace) {
  fjs_ast *n = pos(p, AST_OBJECT_EXPR, brace);
  if (!n) return NULL;
  while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF) {
    fjs_tk_kind kk = peek_kind(p);
    /* Spread: {...x} */
    if (match(p, FJS_TK_ELLIPSIS)) {
      fjs_ast *s = pos(p, AST_SPREAD, &p->lookahead);
      if (s) { s->a = parse_expr(p); fjs_list_push(&p->b, &n->list, s); }
      if (peek_kind(p) != FJS_TK_RBRACE) expect(p, FJS_TK_COMMA);
      continue;
    }
    /* Generator: *ident() {} */
    if (match(p, FJS_TK_STAR)) {
      fjs_token gn = consume(p);
      fjs_ast *prop = fjs_make_ast(&p->b, AST_PROPERTY);
      if (prop) { prop->txt = (fjs_slice){"init",4}; prop->a = mkident(p, gn.txt);
        fjs_ast *m = pos(p, AST_FN_EXPR, &gn); if (m) { m->txt = gn.txt; m->flag = true;
          expect(p, FJS_TK_LPAREN);
          while (peek_kind(p) != FJS_TK_RPAREN && peek_kind(p) != FJS_TK_EOF) {
            fjs_list_push(&p->b, &m->list, mkident(p, consume(p).txt));
            if (peek_kind(p) != FJS_TK_RPAREN) expect(p, FJS_TK_COMMA); }
          expect(p, FJS_TK_RPAREN);
          if (peek_kind(p) == FJS_TK_LBRACE) {  m->a = parse_block(p); }
          prop->b = m; }
        fjs_list_push(&p->b, &n->list, prop); }
      if (peek_kind(p) != FJS_TK_RBRACE) expect(p, FJS_TK_COMMA);
      continue;
    }
    /* Computed: [expr]: val */
    if (match(p, FJS_TK_LBRACKET)) {
      fjs_ast *key = parse_expr(p); expect(p, FJS_TK_RBRACKET);
      fjs_ast *prop = fjs_make_ast(&p->b, AST_PROPERTY);
      if (prop) { prop->txt = (fjs_slice){"init",4}; prop->a = key;
        if (match(p, FJS_TK_COLON)) prop->b = parse_expr(p);
        fjs_list_push(&p->b, &n->list, prop); }
      if (peek_kind(p) != FJS_TK_RBRACE) expect(p, FJS_TK_COMMA);
      continue;
    }
    /* String / num keys */
    if (kk == FJS_TK_STRING || kk == FJS_TK_NUM) {
      fjs_token key = consume(p);
      fjs_ast *prop = fjs_make_ast(&p->b, AST_PROPERTY);
      if (prop) { prop->txt = (fjs_slice){"init",4};
        if (key.kind == FJS_TK_STRING) { prop->a = pos(p, AST_STRING_LITERAL, &key); if (prop->a) prop->a->txt = key.value.len ? key.value : key.txt; }
        else { prop->a = pos(p, AST_NUM_LITERAL, &key); if (prop->a) { char buf[256]; size_t sl = key.txt.len<255?key.txt.len:255; memcpy(buf,key.txt.ptr,sl); buf[sl]=0; prop->a->num = strtod(buf,NULL); } }
        if (match(p, FJS_TK_COLON)) prop->b = parse_expr(p);
        fjs_list_push(&p->b, &n->list, prop); }
      if (peek_kind(p) != FJS_TK_RBRACE) expect(p, FJS_TK_COMMA);
      continue;
    }
    /* Keyword keys: { if: 42, true: false } */
    if (kk == FJS_TK_KW) {
      fjs_token kw_tok = consume(p);
      fjs_ast *prop = fjs_make_ast(&p->b, AST_PROPERTY);
      if (prop) { prop->txt = (fjs_slice){"init",4}; prop->a = mkident(p, kw_tok.txt);
        if (match(p, FJS_TK_COLON)) prop->b = parse_expr(p);
        fjs_list_push(&p->b, &n->list, prop); }
      if (peek_kind(p) != FJS_TK_RBRACE) expect(p, FJS_TK_COMMA);
      continue;
    }
    /* Identifier keys */
    if (kk == FJS_TK_IDENT) {
      fjs_token key = consume(p);
      fjs_ast *prop = fjs_make_ast(&p->b, AST_PROPERTY);
      if (!prop) return n; prop->txt = (fjs_slice){"init", 4};
      prop->a = mkident(p, key.txt);
      if (peek_kind(p) == FJS_TK_LPAREN) {
        fjs_ast *m = pos(p, AST_FN_EXPR, &key);
        if (m) { m->txt = key.txt; expect(p, FJS_TK_LPAREN);
          while (peek_kind(p) != FJS_TK_RPAREN && peek_kind(p) != FJS_TK_EOF) {
            fjs_list_push(&p->b, &m->list, mkident(p, consume(p).txt));
            if (peek_kind(p) != FJS_TK_RPAREN) expect(p, FJS_TK_COMMA); }
          expect(p, FJS_TK_RPAREN);
          if (peek_kind(p) == FJS_TK_LBRACE) {  m->a = parse_block(p); }
          prop->b = m; }
      } else if (match(p, FJS_TK_COLON)) { prop->b = parse_expr(p); }
      else { prop->b = mkident(p, key.txt); prop->flag = true; }
      fjs_list_push(&p->b, &n->list, prop);
      if (peek_kind(p) != FJS_TK_RBRACE) expect(p, FJS_TK_COMMA);
      continue;
    }
    /* get/set */
    if (kk == FJS_TK_KW && (peek(p).kw == FJS_KW_GET || peek(p).kw == FJS_KW_SET)) {
      fjs_token gs = consume(p); fjs_token key = consume(p);
      fjs_ast *prop = fjs_make_ast(&p->b, AST_PROPERTY);
      if (prop) { prop->txt = (key.kind == FJS_TK_KW && gs.kw == FJS_KW_GET) ? (fjs_slice){"get",3} : (fjs_slice){"set",3};
        prop->a = mkident(p, key.txt);
        expect(p, FJS_TK_LPAREN); if (gs.kw == FJS_KW_SET) { consume(p); } expect(p, FJS_TK_RPAREN);
        if (peek_kind(p) == FJS_TK_LBRACE) {  prop->b = parse_block(p); }
        fjs_list_push(&p->b, &n->list, prop); }
      if (peek_kind(p) != FJS_TK_RBRACE) expect(p, FJS_TK_COMMA);
      continue;
    }
    break;
  }
  expect(p, FJS_TK_RBRACE);
  return n;
}

