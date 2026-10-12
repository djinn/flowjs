# flowjs

> **Babel-compatible JavaScript toolchain, written in C23, running inside Node.js via N-API.**

---

**flowjs** is a from-scratch reimplementation of the [Babel](https://babel.dev/) JavaScript compiler
pipeline in modern C (C23). It parses ES2022+ JavaScript and produces a **Babel-compatible JSON AST**
— but does it through a Node.js native addon (N-API), meaning zero JS overhead for the core parsing loop.

<p align="center">
  <a href="https://www.npmjs.com/package/flowjs"><img src="https://img.shields.io/npm/v/flowjs.svg" alt="npm version"></a>
  <img src="https://img.shields.io/badge/node-%3E%3D20-brightgreen?label=Node.js" alt="Node.js 20+">
  <img src="https://img.shields.io/badge/C-23-blue?label=C23" alt="C23">
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT License">
  <img src="https://img.shields.io/badge/build-passing-green" alt="Build Passing">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS-8A2BE2" alt="Linux | macOS">
  <img src="https://img.shields.io/badge/arch-x86_64%20%7C%20arm64-8A2BE2" alt="x86_64 | arm64">
</p>

## Why flowjs?

| Babel | flowjs |
|-------|--------|
| Written in TypeScript → JIT-compiled at runtime | Written in **C23** → compiled to native machine code at install time |
| Heap-allocates AST nodes individually → GC pressure | **Arena allocator** — all nodes bump-allocated, freed as one `free()` call |
| String content is copied between lexer and parser | **Zero-copy slices** — tokens point directly into the original source buffer |
| Requires the full Babel ecosystem for transforms | **Self-contained pipeline** — lexer → parser → AST → transform → emitter in one binary |
| Performance scales with JIT warmup | **Consistent native speed** from first parse |

## Benchmark (ARM64, Node 24)

| Fixture | flowjs | @babel/parser | Speedup |
|---------|--------|---------------|---------|
| 0.3 kB   | 0.08 ms | 0.14 ms | **2.0×** |
| 500 kB   | 53.5 ms | 72.6 ms | **1.4×** |
| **Average** | — | — | **1.2×** |

## Quick Start

```bash
npm install flowjs
```

```js
const flowjs = require('flowjs');
const ast = flowjs.parse('const x = (n) => n * n;');
console.log(JSON.stringify(ast, null, 2));
// → { "type": "Program", "body": [...] }
```

## Features

✅ Functions · Classes · Arrow functions · Async/await · Generators · `yield`
✅ Template literals · Optional chaining · Destructuring · Spread
✅ `try/catch/finally` · `switch` · Labeled statements · `for...of`/`for...in`
✅ Method shorthand · Computed keys · Keyword keys · `get`/`set` accessors
✅ Imports · Exports · Regular expressions · All operators
✅ **Babel-compatible AST output** — same node types and field structure
✅ Pre-built binaries for Linux/macOS (x86_64 + arm64)

## Architecture

```
Source → Lexer (zero-copy) → Parser (arena-allocated AST) → JSON serializer → Node.js via N-API
```

~2,350 lines of C23. No external dependencies. Single `.node` shared library.

## License

MIT — see [LICENSE](LICENSE).