#!/usr/bin/env python3
"""Fix switch case parsing — double-consumption of 'case' keyword"""
with open('/home/ubuntu/flowjs/src_c/parser.c', 'r') as f:
    content = f.read()

# The bug: the inner while loop uses match_kw() which consumes 'case',
# then the outer if() also tries match_kw() which doesn't see it.
# Fix: use peek_kind()/peek().kw in the inner loop condition instead.

old_switch = """    while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF) {
      if (match_kw(p, FJS_KW_CASE)) {
        fjs_ast *c = pos(p, AST_SWITCH_CASE, &t);
        c->a = parse_expr(p); expect(p, FJS_TK_COLON);
        while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF &&
               !match_kw(p, FJS_KW_CASE) && !match_kw(p, FJS_KW_DEFAULT0)) {
          fjs_ast *s = parse_stmt(p);
          if (s) fjs_list_push(&p->b, &c->list, s);
          if (p->lx.err.kind) break;
        }
        fjs_list_push(&p->b, &n->list, c);
      } else if (match_kw(p, FJS_KW_DEFAULT0)) {
        fjs_ast *c = pos(p, AST_SWITCH_CASE, &t);
        expect(p, FJS_TK_COLON);
        while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF &&
               peek_kind(p) != FJS_TK_KW && (peek(p).kw != FJS_KW_CASE && peek(p).kw != FJS_KW_DEFAULT0)) {
          fjs_ast *s = parse_stmt(p);
          if (s) fjs_list_push(&p->b, &c->list, s);
        }
        fjs_list_push(&p->b, &n->list, c);
      } else parse_stmt(p);
    }"""

new_switch = """    while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF) {
      /* Check for 'case' or 'default' WITHOUT consuming the keyword */
      if (peek_kind(p) == FJS_TK_KW && (peek(p).kw == FJS_KW_CASE)) {
        consume(p); /* consume case */
        fjs_ast *c = pos(p, AST_SWITCH_CASE, &t);
        c->a = parse_expr(p); expect(p, FJS_TK_COLON);
        /* Inner loop: consume statements until next case/default or } */
        while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF &&
               !(peek_kind(p) == FJS_TK_KW && (peek(p).kw == FJS_KW_CASE || peek(p).kw == FJS_KW_DEFAULT0))) {
          fjs_ast *s = parse_stmt(p);
          if (s) fjs_list_push(&p->b, &c->list, s);
          if (p->lx.err.kind) break;
        }
        fjs_list_push(&p->b, &n->list, c);
      } else if (peek_kind(p) == FJS_TK_KW && peek(p).kw == FJS_KW_DEFAULT0) {
        consume(p); /* consume default */
        fjs_ast *c = pos(p, AST_SWITCH_CASE, &t);
        expect(p, FJS_TK_COLON);
        while (peek_kind(p) != FJS_TK_RBRACE && peek_kind(p) != FJS_TK_EOF &&
               !(peek_kind(p) == FJS_TK_KW && (peek(p).kw == FJS_KW_CASE || peek(p).kw == FJS_KW_DEFAULT0))) {
          fjs_ast *s = parse_stmt(p);
          if (s) fjs_list_push(&p->b, &c->list, s);
        }
        fjs_list_push(&p->b, &n->list, c);
      } else parse_stmt(p);
    }"""

if old_switch in content:
    content = content.replace(old_switch, new_switch, 1)
    print("Fixed switch parsing (double-consumption of case/default)")
else:
    print("WARNING: switch pattern not found")
    # Find what's there
    idx = content.find('match_kw(p, FJS_KW_CASE)')
    if idx > 0:
        print(f"  Found match_kw CASE at position {idx}")
        print(f"  Context: ...{content[idx-50:idx+80]}...")

with open('/home/ubuntu/flowjs/src_c/parser.c', 'w') as f:
    f.write(content)