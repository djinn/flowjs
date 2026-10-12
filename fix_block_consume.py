#!/usr/bin/env python3
"""Fix parse_block to consume { itself, and remove all pre-consume callers."""
with open('/home/ubuntu/flowjs/src_c/parser.c', 'r') as f:
    content = f.read()

# 1. Fix parse_block: consume { at start
old_block = 'static fjs_ast *parse_block(fjs_parser *p) {\n fjs_ast *n = fjs_make_ast(&p->b, AST_BLOCK);\n if (!n) return NULL;\n parse_stmts(p, &n->list);\n expect(p, FJS_TK_RBRACE);\n return n;\n}'
new_block = 'static fjs_ast *parse_block(fjs_parser *p) {\n fjs_ast *n = fjs_make_ast(&p->b, AST_BLOCK);\n if (!n) return NULL;\n if (peek_kind(p) == FJS_TK_LBRACE) consume(p);\n parse_stmts(p, &n->list);\n expect(p, FJS_TK_RBRACE);\n return n;\n}'
content = content.replace(old_block, new_block)
print("Fixed parse_block to self-consume {")

# 2. Remove all consume(p) before parse_block(p) calls
# Pattern: consume(p); ... -> parse_block(p);
import re
patterns = [
    (r'if \(peek_kind\(p\) == FJS_TK_LBRACE\) \{ consume\(p\); return parse_block\(p\); \}', 'if (peek_kind(p) == FJS_TK_LBRACE) { return parse_block(p); }'),
    (r'if \(peek_kind\(p\) == FJS_TK_LBRACE\) \{ consume\(p\); fn->a = parse_block\(p\); \}', 'if (peek_kind(p) == FJS_TK_LBRACE) { fn->a = parse_block(p); }'),
    (r'if \(peek_kind\(p\) == FJS_TK_LBRACE\) \{ consume\(p\); arrow_n->flag = false; arrow_n->a = parse_block\(p\); \}', 'if (peek_kind(p) == FJS_TK_LBRACE) { arrow_n->flag = false; arrow_n->a = parse_block(p); }'),
    (r'if \(peek_kind\(p\) == FJS_TK_LBRACE\) \{ consume\(p\); n->a = parse_block\(p\); \}', 'if (peek_kind(p) == FJS_TK_LBRACE) { n->a = parse_block(p); }'),
    (r'if \(peek_kind\(p\) == FJS_TK_LBRACE\) \{ consume\(p\); method->a = parse_block\(p\); \}', 'if (peek_kind(p) == FJS_TK_LBRACE) { method->a = parse_block(p); }'),
    (r'if \(peek_kind\(p\) == FJS_TK_LBRACE\) \{ consume\(p\); m->a = parse_block\(p\); \}', 'if (peek_kind(p) == FJS_TK_LBRACE) { m->a = parse_block(p); }'),
    (r'if \(peek_kind\(p\) == FJS_TK_LBRACE\) \{ consume\(p\); prop->b = parse_block\(p\); \}', 'if (peek_kind(p) == FJS_TK_LBRACE) { prop->b = parse_block(p); }'),
]

for old, new in patterns:
    count = content.count(old)
    if count > 0:
        content = content.replace(old, new)
        print(f"  Replaced ({count}x): {old[:50]}...")

# 3. Remove any expect(p, FJS_TK_LBRACE) I just added before parse_block(p) calls
# Since parse_block now consumes its own {
content = content.replace('  expect(p, FJS_TK_LBRACE);\n    n->a = parse_block(p);', '\n    n->a = parse_block(p);')
content = content.replace('  expect(p, FJS_TK_LBRACE);\n      n->b = parse_block(p);', '\n      n->b = parse_block(p);')
content = content.replace('      expect(p, FJS_TK_LBRACE);\n      n->c = parse_block(p);', '\n      n->c = parse_block(p);')

# 4. Also fix class body: expect(p, FJS_TK_LBRACE) which consumes { then parse_class methods iterate
# Actually class body expects { but then the methods loop through {}. That's fine.

with open('/home/ubuntu/flowjs/src_c/parser.c', 'w') as f:
    f.write(content)
print("\nDone!")