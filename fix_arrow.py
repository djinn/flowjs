#!/usr/bin/env python3
"""Fix arrow function block body in parser.c"""
with open('/home/ubuntu/flowjs/src_c/parser.c', 'r') as f:
    content = f.read()

old = 'if (peek_kind(p) == FJS_TK_LBRACE) { arrow_n->flag = false; arrow_n->a = parse_block(p); }'
new = 'if (peek_kind(p) == FJS_TK_LBRACE) { consume(p); arrow_n->flag = false; arrow_n->a = parse_block(p); }'

count = content.count(old)
if count > 0:
    content = content.replace(old, new)
    print(f"Fixed {count} arrow block body handler(s)")
else:
    print("Pattern not found")

with open('/home/ubuntu/flowjs/src_c/parser.c', 'w') as f:
    f.write(content)