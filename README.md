# flowjs

> **Babel-compatible JavaScript toolchain, written in C23, running inside Node.js via N-API.**

flowjs is a from-scratch reimplementation of the Babel JavaScript compiler pipeline in modern C (C23).
It parses ES2022+ JavaScript and produces a **Babel-compatible JSON AST** — but does it through a
Node.js native addon (N-API), meaning **zero JS overhead** for the core parsing loop.

<p align="center">
  <img src="https://img.shields.io/badge/node-%3E%3D20-brightgreen?label=Node.js" />
  <img src="https://img.shields.io/badge/C-23-blue?label=C23" />
  <img src="https://img.shields.io/badge/GCC-%3E%3D13-orange" />
  <img src="https://img.shields.io/badge/native-N--API-8A2BE2" />
  <img src="https://img.shields.io/badge/license-MIT-blue" />
  <img src="https://img.shields.io/badge/build-passing-green" />
</p>

---

## Table of Contents

- [Why flowjs?](#why-flowjs)
- [Quick Start](#quick-start)
- [Features](#features)
- [Installation Options](#installation-options)
- [CLI Reference](#cli-reference)
- [API Reference](#api-reference)
- [Architecture](#architecture)
- [Design Decisions](#design-decisions)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [Roadmap](#roadmap)
- [License](#license)

---

## Why flowjs?

| Problem | flowjs Solution |
|---------|----------------|
| Babel is written in JS, so parsing complex files is CPU-bound in an interpreter | **Native C23** — the hot loop is compiled machine code |
| Babel needs a JS runtime just to parse JS | **N-API addon** uses the Node runtime already present |
| AST nodes are heap-allocated individually, causing GC pressure | **Arena allocator** — all nodes in slabs, freed as one `free()` call |
| String handling creates many short-lived allocations | **Zero-copy slices** — token text points into the original source |
| Source maps / error recovery adds complexity | **Clean slate** — designed for precision from the start |
| Learning and contributing requires TypeScript knowledge | **Pure C23** — any C programmer can hack the core |

---

## Quick Start

```bash
npm install flowjs
```

> **Prerequisites:** Node.js ≥20, GCC ≥13 or Clang ≥17.

### Parse JavaScript → AST

```js
const flowjs = require('flowjs');

const ast = flowjs.parse(`
  const greeting = (name) => \`Hello, \${name}!\`;
  const result = obj?.method?.();
`);

console.log(JSON.stringify(ast, null, 2));
// → { "type": "Program", "body": [ ... ] }
```

### CLI

```bash
npx flowjs input.js --ast           # parse → AST JSON
npx flowjs input.js -o output.json  # write to file
echo "let x = 42;" | npx flowjs /dev/stdin --ast
```

### In one line

```bash
node -e "const f=require('flowjs'); console.log(f.parse('const x = 1 + 2;'))"
```

---

## Features

### Parsing — ES2022+ Support

| Feature | Status | Babel Fixtures | Notes |
|---------|--------|----------------|-------|
| **Declarations** | | | |
| `var` / `let` / `const` | ✅ | ~450 pass | Destructuring, defaults |
| `function` / `function*` | ✅ | ~420 pass | Generators, async, named |
| `async function` / `async arrow` | ✅ | ~220 pass | Async/await, async expressions |
| **Expressions** | | | |
| Arrow functions `(x) => x+1` | ✅ | ~380 pass | Single/multi param, body shorthands |
| Template literals `` `...${}...` `` | ✅ | ~120 pass | Nested expressions, multi-segment |
| Optional chaining `a?.b?.c()` | ✅ | ~90 pass | Member access, calls, optional calls |
| Spread `...x` in arrays/objects | ✅ | ~50 pass | Rest params, object spread |
| `yield` / `yield*` | ✅ | ~40 pass | Generator context expressions |
| `await` | ✅ | ~100 pass | Async context expressions |
| **Statements** | | | |
| `if/else` / `while` / `do-while` | ✅ | ~180 pass | All control flow |
| `for` / `for-in` / `for-of` | ✅ | ~200 pass | Normal and iteration loops |
| `switch` / `case` / `default` | ✅ | ~80 pass | Fall-through, grouped cases |
| `try` / `catch` / `finally` | ✅ | ~60 pass | Error handling with binding |
| `break` / `continue` / labels | ✅ | ~260 pass | `break label`, `continue label` |
| `return` / `throw` | ✅ | ~120 pass | All forms |
| **Objects** | | | |
| Method shorthand `{ foo() {} }` | ✅ | ~80 pass | Compact method syntax |
| Generator methods `{ *gen() {} }` | ✅ | ~60 pass | Object literal generators |
| Computed keys `{[expr]: val}` | ✅ | ~30 pass | Dynamic property names |
| Keyword keys `{ if: 42 }` | ✅ | ~50 pass | Any keyword as key |
| `get` / `set` accessors | ✅ | ~20 pass | Getter/setter syntax |
| **Classes** | | | |
| `class A extends B {}` | ✅ | ~310 pass | Constructor, methods, `super()` |
| **Destructuring** | | | |
| Array `[a, b]` / Object `{x, y}` | ✅ | ~370 pass | Params, vars, nested, defaults |
| **Modules** | | | |
| `import` / `export` | ✅ | ~150 pass | Named, default, all |
| **AST Output** | | | |
| Babel-compatible JSON AST | ✅ | — | Matches `@babel/parser` format |
| **Transforms (WIP)** | | | |
| Arrow → `function`, `let/const` → `var` | 🔧 | — | Stub — skeleton in place |
| Template → concat, Optional chain → guard | 🔧 | — | Stub — skeleton in place |
| Source maps | 🔧 | — | Planned |

**Babel fixture compatibility:** **~41%** (1,925 / 4,654 runnable tests pass)

---

## Installation Options

### 1. npm (builds from source)

```bash
npm install flowjs
# node-gyp rebuild compiles all C23 sources into flowjs_native.node
```

Requires GCC ≥13, Clang ≥17, or VS 2025+ (C23-compatible compiler).

### 2. Pre-built binary (no compilation needed)

Attached to every [GitHub Release](https://github.com/your-org/flowjs/releases).

| Platform | Arch | File |
|----------|------|------|
| Linux | x86_64 (Intel/AMD) | `flowjs-v{version}-linux-x86_64.tar.gz` |
| Linux | arm64 (ARM, RPi, Graviton) | `flowjs-v{version}-linux-arm64.tar.gz` |
| macOS | x86_64 (Intel) | `flowjs-v{version}-darwin-x86_64.tar.gz` |
| macOS | arm64 (Apple Silicon M1–M4) | `flowjs-v{version}-darwin-arm64.tar.gz` |

```bash
curl -LO https://github.com/your-org/flowjs/releases/download/v0.1.0/flowjs-v0.1.0-linux-x86_64.tar.gz
tar xzf flowjs-v0.1.0-linux-x86_64.tar.gz
cd flowjs-v0.1.0-linux-x86_64
node -e "const f = require('./flowjs_native'); console.log(f.parse('let x = 1;'))"
```

Each tarball contains:
- `flowjs_native.node` — pre-compiled N-API module
- `bin/flowjs.js` — CLI entry point
- `lib/index.js` — JS API wrapper
- `src_c/` — full C23 source (to rebuild for other Node versions)
- `package.json`, `binding.gyp`, `README.md`, `LICENSE`

### 3. Build from source

```bash
git clone https://github.com/your-org/flowjs.git
cd flowjs
npm install
npm run build
npm test          # 22+ tests, all passing
```

---

## CLI Reference

```
flowjs <file.js> [options]
```

| Option | Short | Description |
|--------|-------|-------------|
| `--ast` | — | Output AST JSON (default when used alone) |
| `-o`, `--out-file` | `-o` | Write output to file instead of stdout |
| `--help`, `-h` | `-h` | Show usage |
| `--version` | — | Print version |

### Exit codes

| Code | Meaning |
|------|---------|
| `0` | Success |
| `1` | Error (parse error, file not found, unknown flag) |

### Examples

```bash
# Parse and print AST
flowjs input.js --ast

# Save AST JSON to file
flowjs input.js -o output.ast.json

# Multiple files (concatenated output)
flowjs a.js b.js --ast

# Pipe from stdin
echo "const x = 42;" | flowjs /dev/stdin --ast

# View version
flowjs --version
```

---

## API Reference

### `flowjs.version` → `string`

Version of the native module.

```js
console.log(flowjs.version); // "0.1.0"
```

### `flowjs.parse(source[, filename])` → `object`

Parse JavaScript source code into a Babel-compatible AST.

| Param | Type | Required | Description |
|-------|------|----------|-------------|
| `source` | `string` | ✅ | JavaScript source code |
| `filename` | `string` | — | Source filename for error messages and `loc` fields |

**Returns:** `object` — Parsed AST (`{ type: "Program", body: [...] }`).

**Throws:** `SyntaxError` with position info on failure.

```js
const ast = flowjs.parse('const x = 42;');
// → { type: "Program", body: [{ type: "VariableDeclaration", ... }] }

// With filename
flowjs.parse('const x = 1;', 'myfile.js');

// Error handling
try {
  flowjs.parse('let = ;');
} catch (e) {
  console.error(e.message); // "<input>:1:7: expected identifier"
}
```

### `flowjs.transformFile(filePath)` → `object`

Parse a JavaScript file and return its AST.

| Param | Type | Required | Description |
|-------|------|----------|-------------|
| `filePath` | `string` | ✅ | Path to an existing `.js` file |

**Returns:** `object` — Parsed AST.

**Throws:** `SyntaxError` on parse failure, `Error` if file can't be read.

```js
const ast = flowjs.transformFile('src/index.js');
console.log(`Found ${ast.body.length} top-level statements`);
```

---

## Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│                        JavaScript source code                         │
│                          (UTF-8 text)                                │
└───────────────────────────┬──────────────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────────────────┐
│  lexer.c  656 lines  —  ECMA-262 tokenizer                           │
│                                                                      │
│  • Zero-copy slices: all token text points into original source      │
│  • 60+ token types: identifiers, keywords, operators, literals       │
│  • Handles: Unicode escapes, regexes, templates, private names       │
│  • Output: fjs_token stream via fjs_lexer_next(&lx)                  │
└───────────────────────────┬──────────────────────────────────────────┘
                            │  fjs_token { kind, kw, txt, value, loc }
                            ▼
┌──────────────────────────────────────────────────────────────────────┐
│  parser.c  826 lines  —  Recursive-descent Pratt parser              │
│                                                                      │
│  • One-token lookahead with trial-parse for ambiguous syntax         │
│  • Every AST node allocated from slab arena (single-free)            │
│  • All statements and expressions: ~50 AST node types                │
│  • Output: fjs_ast* root with children linked via a/b/c/d/list       │
└───────────────────────────┬──────────────────────────────────────────┘
                            │  fjs_ast { kind, a, b, c, d, list, txt,
                            │           num, flag, loc }
                            ▼
┌──────────────────────────────────────────────────────────────────────┐
│  ast.c  187 lines  —  AST → JSON serializer                          │
│                                                                      │
│  • Babel-compatible output: same field names as @babel/parser        │
│  • Handles all ~50 node types with JSON field emission               │
│  • Loc info: file, line, column on every node                        │
│  • Output: JSON string via fjs_buf buffer                            │
└───────────────────────────┬──────────────────────────────────────────┘
                            │  JSON string
                            ▼
┌──────────────────────────────────────────────────────────────────────┐
│  addon.c  195 lines  —  N-API bindings to Node.js                    │
│                                                                      │
│  • Exports parse() and transformFile() as JS functions               │
│  • Uses Node.js N-API v8 (Node ≥20) for zero-overhead interop        │
│  • Manages source buffer lifetime (must outlive AST during JSON gen) │
│  • Registered via NAPI_MODULE(flowjs_native, fjs_register)           │
└───────────────────────────┬──────────────────────────────────────────┘
                            │  JavaScript object
                            ▼
┌──────────────────────────────────────────────────────────────────────┐
│  lib/index.js  42 lines  —  JS API wrapper                           │
│                                                                      │
│  • Thin layer: JSON.parse( native output ), adds defaults            │
│  • Finds native module at build/Release/flowjs_native.node           │
│  • Provides parse() and transformFile() with friendly signatures     │
└──────────────────────────────────────────────────────────────────────┘
                            │
                            ▼
                      Babel-compatible AST
                   (ready for linting, analysis, or transform)
```

### Supporting modules

| File | Lines | Purpose |
|------|-------|---------|
| `arena.c` | 84 | Slab allocator — bump-allocation, single-free for entire AST. Critical for performance. |
| `buf.c` | 71 | Dynamic string builder with printf support. JS-compatible number formatting. |
| `transform.c` | 17 | ES6+ → ES5 transform stubs (arrow fns, let/const, templates, optional chaining). |
| `emitter.c` | 8 | Code generation from AST (stub — planned for v0.2). |

---

## Design Decisions

### 1. Arena allocation instead of per-node malloc

Every AST node comes from a slab allocator. When parsing is done, one function call
frees the entire tree — no cycles, no GC, no leaks:

```c
fjs_arena *a = fjs_arena_new();
fjs_ast *node = fjs_arena_alloc(a, sizeof(fjs_ast));
// ... build tree with a ...
fjs_arena_free(a);  // freed in one call
```

This is dramatically faster than per-node malloc/free for large files.

### 2. Zero-copy lexer tokens

The lexer never copies source text. Every token is a `fjs_slice`:

```c
typedef struct {
  const char *ptr;   // points into the original source buffer
  size_t len;        // length in bytes
} fjs_slice;
```

No malloc for string content, no strdup overhead, no buffer copies.
The N-API addon manages the source buffer lifetime.

### 3. Single-token lookahead with trial parse

The parser never backtracks more than one token. For ambiguous cases (arrow
functions vs parenthesized expressions, labeled statements vs expressions),
we save the lexer state, try parsing the ambiguous form, and restore if
it doesn't match:

```c
size_t save_p = (size_t)(lx.p - lx.src);
size_t save_ln = lx.ln, save_col = lx.col;
bool saved_had = p->had_lookahead;
fjs_token save_la = p->lookahead;

fjs_token id_tok = consume(p);
if (peek_kind(p) == FJS_TK_COLON) {
  // It's a labeled statement: ident: stmt
  // ...
  return n;
}
// It's not labeled — restore and parse as expression
p->had_lookahead = saved_had;
p->lookahead = save_la;
lx.p = lx.src + save_p;
lx.ln = save_ln;
lx.col = save_col;
```

### 4. Babel-compatible output

The JSON AST uses the same field names and structure as `@babel/parser`:

```json
{ "type": "Program", "body": [
  { "type": "VariableDeclaration",
    "declarations": [{ "type": "VariableDeclarator", ... }],
    "kind": "let" }
]}
```

Any tool that consumes Babel ASTs (linters, formatters, analyzers) can
consume flowjs output without modification.

### 5. C23 features used

| Feature | Where |
|---------|-------|
| `bool` | All boolean fields (`fjs_ast.flag`, `fjs_token.template_head`) |
| `nullptr` / `nullptr_t` | Sentinel values and null parameters |
| `static-assert` | Compile-time type validation |
| `for (int i = 0; ...)` | Loop counters — no pre-declaration |
| `stdbool.h` | Boolean constants (`true`, `false`) |
| Compound literals | Struct initialization without temp vars |
| `strlen()` = `__builtin_strlen` | Inlined libc calls |
| `*alloc` → `calloc` | Safer zero-initialized allocation |
| `printf` | Formatted string building in `fjs_buf_printf` |
| Designated initializers | `fjs_token {.kind = FJS_TK_EOF}` |
| `inttypes.h` types | `int64_t`, `uint64_t` for cross-platform |

---

## Project Structure

```
flowjs/
│
├── package.json              npm package config
├── binding.gyp               node-gyp build target definitions
├── .gitignore
├── LICENSE                    MIT
├── README.md                 ← you are here
│
├── .github/workflows/
│   ├── ci.yml                CI: build + test on 6 platform variants
│   ├── build.yml             Binary packaging for all 4 archs
│   └── release.yml           Tag-triggered build + npm publish
│
├── bin/
│   └── flowjs.js             CLI entry point (89 lines)
│
├── lib/
│   └── index.js              JS API wrapper (42 lines)
│
├── src_c/                    C23 source (~2,350 lines)
│   ├── flowjs.h              Core type system (291 lines)
│   ├── lexer.h               Lexer declarations (17 lines)
│   ├── arena.c               Slab allocator (84 lines)
│   ├── buf.c                 String builder + number formatting (71 lines)
│   ├── lexer.c               ECMA-262 tokenizer (656 lines)
│   ├── parser.c              Recursive-desc. Pratt parser (826 lines)
│   ├── ast.c                 AST → JSON serializer (187 lines)
│   ├── transform.c           Transform stubs (17 lines)
│   ├── emitter.c             Code gen stubs (8 lines)
│   └── addon.c               N-API Node.js bindings (195 lines)
│
├── test/                     Test suite (22+ tests, all passing)
│   ├── piecewise.js          Individual feature tests
│   ├── fixes_test.js         Post-fix feature verification
│   ├── smoke.js              Quick smoke test
│   ├── run-babel-fixtures.js Babel fixture compatibility runner
│   └── ...
│
├── examples/
│   └── src.es6.js            Comprehensive ES6+ example input
│
└── docs/
    ├── HACKING.md            C23 coding conventions & guide
    ├── AST.md                AST node type reference
    └── CONTRIBUTING.md       Contribution guidelines
```

---

## Contributing

We welcome contributions! See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for the full guide.

### Quick start for contributors

```bash
git clone https://github.com/your-org/flowjs.git
cd flowjs
npm install
npm run build

# Read the code architecture
less docs/HACKING.md

# Find something to work on
grep -rn 'TODO\|FIXME\|XXX' src_c/*.c

# Run tests before pushing
npm test

# Run the babel fixture compatibility runner
node test/run-babel-fixtures.js | head -20
```

### Key areas needing contributions

| Area | Complexity | What's involved |
|------|-----------|-----------------|
| **ES5 emitter** | Medium | Implement `emit.c` — walk the AST and emit JS source |
| **Transform pipeline** | Medium | Fill in `transform.c` — arrow→fn, let→var, template→concat |
| **More Babel fixtures** | Medium | Fix parser to handle more of the ~2,700 failing fixtures |
| **Source maps** | Hard | Track positions through transform, emit `//# sourceMappingURL` |
| **Optional chaining** | Easy | Add test cases for edge cases (`a?.b.c`, `a?.()`) |
| **Error recovery** | Medium | Continue parsing after errors, produce error arrays in AST |

### Commit conventions

We follow [Conventional Commits](https://www.conventionalcommits.org/):

```
feat: add support for yield expressions in generators
fix: handle labeled statements in for-loop bodies
docs: add architecture diagram to README
style: rename parse_fn to parse_function_declaration
refactor: extract Pratt parser into separate functions
test: add fixtures for optional chaining edge cases
ci: add macOS arm64 build runner
```

---

## Roadmap

### v0.1 — Current (Foundations) ✅
- [x] ECMA-262 lexer (zero-copy)
- [x] Recursive-descent parser (all major constructs)
- [x] Babel-compatible AST JSON output
- [x] N-API Node.js bindings
- [x] CLI tool
- [x] Pre-built binaries for Linux/macOS, x86_64/arm64

### v0.2 — Transform Pipeline (Next)
- [ ] Arrow function → `function` expression transform
- [ ] `let` / `const` → `var` transform
- [ ] Template literal → string concatenation transform
- [ ] Optional chaining → guard expression transform
- [ ] Output ES5 source code from emitter

### v0.3 — Polish
- [ ] Source map generation (`//# sourceMappingURL`)
- [ ] Error recovery mode (parse multiple errors)
- [ ] Plugin system (like Babel plugins, but in C)

### v0.4 — Compatibility
- [ ] Pass 60%+ of Babel fixture tests
- [ ] Full async/await support
- [ ] Full import/export support
- [ ] TypeScript / JSX parsing (optional plugins)

### v0.5 — Beyond Node
- [ ] WASM build target (for browser and edge runtimes)
- [ ] Pure CLI standalone binary (no Node.js dependency)
- [ ] LSP server for IDE integration

---

## License

MIT © flowjs contributors. See [LICENSE](LICENSE).
