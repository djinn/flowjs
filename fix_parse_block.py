#!/usr/bin/env python3
"""Make parse_block self-sufficient (consume { itself)."""
with open('/home/ubuntu/flowjs/src_c/parser.c', 'r') as f:
    content = f.read()

old = 'static fjs_ast *parse_block(fjs_parser *p) {\n fjs_ast *n = fjs_make_ast(&p->b, AST_BLOCK);\n if (!n) return NULL;\n parse_stmts(p, &n->list);\n expect(p, FJS_TK_RBRACE);\n return n;\n}'
new = 'static fjs_ast *parse_block(fjs_parser *p) {\n fjs_ast *n = fjs_make_ast(&p->b, AST_BLOCK);\n if (!n) return NULL;\n expect(p, FJS_TK_LBRACE);\n parse_stmts(p, &n->list);\n expect(p, FJS_TK_RBRACE);\n return n;\n}'

if old in content:
    content = content.replace(old, new, 1)
    # Also fix the statement-level block handler which previously consumed the {
    # Line ~108: "if (peek_kind(p) == FJS_TK_LBRACE) { consume(p); return parse_block(p); }"
    # Now parse_block consumes it, so we shouldn't double-consume
    old2 = 'if (peek_kind(p) == FJS_TK_LBRACE) { consume(p); return parse_block(p); }'
    new2 = 'if (peek_kind(p) == FJS_TK_LBRACE) { return parse_block(p); }'
    if old2 in content:
        content = content.replace(old2, new2)
        print("Fixed parse_stmt double-consume")
    # Also fix fn body (which now calls parse_block without consuming {
    # fn body: "if (peek_kind(p) == FJS_TK_LBRACE) { consume(p); n->a = parse_block(p); }" -> remove consume
    old3 = ' if (peek_kind(p) == FJS_TK_LBRACE) { consume(p); n->a = parse_block(p); }'
    new3 = ' if (peek_kind(p) == FJS_TK_LBRACE) { n->a = parse_block(p); }'
    if old3 in content:
        content = content.replace(old3, new3)
        print("Fixed fn body double-consume")
    # Arrow block body
    old4 = 'if (peek_kind(p) == FJS_TK_LBRACE) { consume(p); arrow_n->flag = false; arrow_n->a = parse_block(p); }'
    new4 = 'if (peek_kind(p) == FJS_TK_LBRACE) { arrow_n->flag = false; arrow_n->a = parse_block(p); }'
    if old4 in content:
        content = content.replace(old4, new4)
        print("Fixed arrow body double-consume")
    print("parse_block now consumes { itself")
else:
    print("WARNING: Could not find parse_block function")
    # Find it
    import re
    match = re.search(r'static fjs_ast \*parse_block.*?\n\}$', content, re.DOTALL)
    if match:
        print(f"  Found at: {match.start()}: {match.group()[:80]}...")

with open('/home/ubuntu/flowjs/src_c/parser.c', 'w') as f:
    f.write(content)