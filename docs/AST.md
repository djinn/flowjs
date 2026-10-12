# AST Node Reference

flowjs produces a Babel-compatible AST. This document lists every node type
produced by the parser, its fields, and which JavaScript syntax it corresponds to.

## Node Type Table

| # | AST Node | JS Syntax | Fields | `a` | `b` | `c` | `d` | `list` | `txt` | `num` | `flag` |
|---|----------|-----------|--------|-----|-----|-----|-----|--------|-------|-------|--------|
| 0 | `Program` | Full script | `body` as `list` | — | — | — | — | statements | — | — | — |
| 1 | `ExpressionStatement` | `expr;` | `expression` as `a` | expr | — | — | — | — | — | — | — |
| 2 | `BlockStatement` | `{ stmts }` | `body` as `list` | — | — | — | — | statements | — | — | — |
| 3 | `EmptyStatement` | `;` | (none) | — | — | — | — | — | — | — | — |
| 4 | `DebuggerStatement` | `debugger;` | (none) | — | — | — | — | — | — | — | — |
| 5 | `VariableDeclaration` | `let x = 1` | `declarations` as `list`, `kind` as `txt` | — | — | — | — | decls | `let`/`const`/`var` | — | — |
| 6 | `VariableDeclarator` | `x = 1` | `id` as `a`, `init` as `b` | ident | init | — | — | — | — | — | — |
| 7 | `FunctionDeclaration` | `function f() {}` | `id` as `txt`, params as `list`, body as `a` | body | — | — | — | params | name | — | async |
| 8 | `ClassDeclaration` | `class A extends B {}` | `id` as `a`, super as `b`, body as `list` | id | super | — | — | methods | — | — | — |
| 9 | `ReturnStatement` | `return x;` | argument as `a` | arg | — | — | — | — | — | — | — |
| 10 | `IfStatement` | `if (t) c else a` | test/a, cons/b, alt/c | test | cons | alt | — | — | — | — | — |
| 11 | `ForStatement` | `for (i; t; u) b` | init/a, test/b, update/c, body/d | init | test | up | body | — | — | — | — |
| 12 | `ForInStatement` | `for (x in y) b` | left/a, right/b, body/d | left | right | — | body | — | — | — | — |
| 13 | `ForOfStatement` | `for (x of y) b` | left/a, right/b, body/d | left | right | — | body | — | — | — | — |
| 14 | `WhileStatement` | `while (t) b` | test/a, body/b | test | body | — | — | — | — | — | — |
| 15 | `DoWhileStatement` | `do b while (t);` | body/a, test/b | body | test | — | — | — | — | — | — |
| 16 | `SwitchStatement` | `switch(d) { cases }` | disc/a, cases/list | disc | — | — | — | cases | — | — | — |
| 17 | `SwitchCase` | `case v: stmts` | test/a, cons/list | test | — | — | — | stmts | — | — | — |
| 18 | `BreakStatement` | `break label?` | label as `a` | label | — | — | — | — | — | — | — |
| 19 | `ContinueStatement` | `continue label?` | label as `a` | label | — | — | — | — | — | — | — |
| 20 | `LabeledStatement` | `label: stmt` | label as `txt`, body as `a` | body | — | — | — | — | label | — | — |
| 21 | `ThrowStatement` | `throw e;` | argument as `a` | arg | — | — | — | — | — | — | — |
| 22 | `TryStatement` | `try b catch h finally f` | block/a, handler/b, fin/c | block | hand | fin | — | — | — | — | — |
| 23 | `WithStatement` | `with(o) b` | object/a, body/b | obj | body | — | — | — | — | — | — |
| 24 | `ImportDeclaration` | `import s from "..."` | source/a, specs/list | src | — | — | — | specs | — | — | — |
| 25 | `ExportNamedDeclaration` | `export decl` | decl/a, specs/list | decl | — | — | — | specs | — | — | — |
| 26 | `ExportDefaultDeclaration` | `export default decl` | decl/a | decl | — | — | — | — | — | — | — |
| 27 | `ExportAllDeclaration` | `export * from "..."` | source/a | src | — | — | — | — | — | — | — |
| 28 | `Identifier` | `x` | name as `txt` | — | — | — | — | — | name | — | — |
| 29 | `NumericLiteral` | `42`, `3.14` | value as `num` | — | — | — | — | — | — | value | — |
| 30 | `StringLiteral` | `"hello"` | value as `txt` | — | — | — | — | — | value | — | — |
| 31 | `BooleanLiteral` | `true`/`false` | value as `flag` | — | — | — | — | — | — | — | value |
| 32 | `NullLiteral` | `null` | (none) | — | — | — | — | — | — | — | — |
| 33 | `RegExpLiteral` | `/pattern/flags` | value as `txt` | — | — | — | — | — | pattern+flags | — | — |
| 34 | `TemplateLiteral` | `` `hello ${x}!` `` | parts as `list` | — | — | — | — | string+expr | — | — | — |
| 35 | `ArrayExpression` | `[1, 2, 3]` | elements as `list` | — | — | — | — | elements | — | — | — |
| 36 | `ObjectExpression` | `{ a: 1 }` | properties as `list` | — | — | — | — | props | — | — | — |
| 37 | `Property` | key: value | key/a, value/b, kind/txt, short/flag | key | val | — | — | — | kind | — | short |
| 38 | `FunctionExpression` | `function() {}` | id/txt, params/list, body/a | body | — | — | — | params | name | — | async/gen |
| 39 | `ClassExpression` | `class {}` | body/list | — | — | — | — | body | — | — | — |
| 40 | `ArrowFunctionExpression` | `(x) => x` | params/list, body/a, expression/flag | body | — | — | — | params | — | — | expr |
| 41 | `UnaryExpression` | `!x`, `typeof x` | op/txt, arg/a | arg | — | — | — | — | operator | — | — |
| 42 | `UpdateExpression` | `x++` | op/txt, arg/a, prefix/flag | arg | — | — | — | — | operator | — | prefix |
| 43 | `BinaryExpression` | `a + b` | op/txt, left/a, right/b | left | right | — | — | — | operator | — | — |
| 44 | `LogicalExpression` | `a && b` | op/txt, left/a, right/b | left | right | — | — | — | operator | — | — |
| 45 | `AssignmentExpression` | `a = b` | op/txt, left/a, right/b | left | right | — | — | — | operator | — | — |
| 46 | `ConditionalExpression` | `t ? c : a` | test/a, cons/b, alt/c | test | cons | alt | — | — | — | — | — |
| 47 | `CallExpression` | `f(a, b)` | callee/a, args/list | callee | — | — | — | args | — | — | — |
| 48 | `NewExpression` | `new C(a)` | callee/a, args/list | callee | — | — | — | args | — | — | — |
| 49 | `MemberExpression` | `obj.prop` | obj/a, prop/b, computed/flag | obj | prop | — | — | — | — | — | comp |
| 50 | `OptionalMemberExpression` | `obj?.prop` | obj/a, prop/b, computed/flag | obj | prop | — | — | — | — | — | comp |
| 51 | `OptionalCallExpression` | `f?.()` | callee/a, args/list | callee | — | — | — | args | — | — | — |
| 52 | `SequenceExpression` | `a, b` | expressions as `list` | — | — | — | — | exprs | — | — | — |
| 53 | `SpreadElement` | `...x` | argument as `a` | arg | — | — | — | — | — | — | — |
| 54 | `YieldExpression` | `yield x` | arg as `a`, delegate as `flag` | arg | — | — | — | — | — | — | del |
| 55 | `AwaitExpression` | `await p` | argument as `a` | arg | — | — | — | — | — | — | — |
| 56 | `ParenthesizedExpression` | `(x)` | expr as `a` | expr | — | — | — | — | — | — | — |
| 57 | `ObjectPattern` | `{x, y}` | elements as `list` | — | — | — | — | elements | — | — | — |
| 58 | `ArrayPattern` | `[a, b]` | elements as `list` | — | — | — | — | elements | — | — | — |
| 59 | `AssignmentPattern` | `x = default` | left/a, right/b | left | right | — | — | — | — | — | — |
| 60 | `RestElement` | `...args` | arg/a | arg | — | — | — | — | — | — | — |
| 61 | `Decorator` | `@expr` | expr/a | expr | — | — | — | — | — | — | — |

## Field mapping

The `fjs_ast` struct packs multiple fields into a compact 64-byte structure:

```c
struct fjs_ast {
  fjs_ast_kind kind;     // Node type enum (0–61+)
  fjs_loc loc;           // Source location (file, line, col)
  fjs_ast *a, *b, *c, *d; // Child pointers (up to 4 children)
  fjs_ast_list list;     // List of children (head/tail/count)
  fjs_slice txt;         // Text field (identifier names, operator strings, etc.)
  double num;            // Numeric literal value
  bool flag;             // Boolean flag (async, generator, computed, etc.)
  fjs_kw kw;             // Keyword enum (reserved for keyword references)
};
```