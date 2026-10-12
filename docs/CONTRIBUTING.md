# Contributing to flowjs

Thanks for your interest! flowjs is a young project and all help is welcome.

## Code of Conduct

Be kind, be constructive, assume good faith. This project follows the
[Contributor Covenant](https://contributor-covenant.org/) v2.1.

## Getting Started

```bash
# Clone and build
git clone https://github.com/your-org/flowjs.git
cd flowjs
npm install
npm run build
npm test          # verify your setup
```

## What to Work On

### Good first issues (requires C23 or JS knowledge)

- **Add missing fixture tests** — pick a Babel test from `test/run-babel-fixtures.js` output
  and fix the parser to handle it.
- **Improve error messages** — the lexer and parser produce basic error strings.
  Better messages (with expected/received hints) would help users.
- **Add test cases** — the `test/` directory has good coverage but more edge cases
  are always welcome.

### Medium complexity

- **Fill in transform.c** — the transform pipeline has a skeleton. Implement
  arrow function → `function` expression, `let`/`const` → `var`, template literal
  → string concatenation, or optional chaining → guard.
- **Fill in emitter.c** — the emitter stub needs to walk the AST and emit ES5 source
  code. Start with a simple `if` → `if` identity pass.
- **Benchmarking** — write a benchmark script that compares flowjs parse time against
  `@babel/parser` on real-world files.

### Advanced

- **Error recovery** — modify the parser to continue after errors, building an
  `errors` array in the output AST (like Babel's `errorRecovery: true`).
- **Source maps** — track positions through the transform pipeline and emit
  `//# sourceMappingURL` in the output.
- **WASM target** — build `flowjs.wasm` using `wasm-pack` for browser use.
- **Full Babel compatibility** — get the fixture pass rate above 60%.

## Pull Request Process

1. **Fork** the repo and create a feature branch.
2. **One change per PR.** If you're fixing multiple things, split them up.
3. **Include tests.** Every new feature or fix should come with at least one test case.
4. **Run `npm test`** before pushing.
5. **Run `npm run test:babel`** — if you're changing the parser, run the fixture
   suite to check for regressions.
6. **Open a PR** with a clear description. If it's a parser fix, include the
   input that used to fail and the expected output.

## PR review checklist

Maintainers will check:
- [ ] Does the C23 code follow `docs/HACKING.md` conventions?
- [ ] Does `npm test` pass?
- [ ] Does the Babel fixture pass rate not regress?
- [ ] Is the error message helpful if the parse fails?
- [ ] Is the test file included (if applicable)?
- [ ] Is the AST output correct (matching Babel format)?

## Commit messages

We use [Conventional Commits](https://www.conventionalcommits.org/):

```
feat(parser): support yield* delegate expressions
fix(lexer): handle unterminated template literals
docs(README): add architecture diagram
style(parser): rename parse_fn to parse_function_declaration
refactor(arena): extract block allocation to helper
test: add fixtures for optional chaining edge cases
ci: add macOS arm64 build runner
```

## Development workflow

```bash
# After changing C files
npm run build              # recompile
node test/piecewise.js     # quick feature check

# Test a single JS snippet
node -e "const f=require('./build/Release/flowjs_native'); console.log(f.parse('let x = 1;'))"

# Debug the lexer in isolation (C level)
gcc -std=c2x -Isrc_c -o /tmp/lex_test \
  src_c/arena.c src_c/buf.c src_c/lexer.c \
  -DTEST_LEXER -Wall -Wextra
/tmp/lex_test

# Run the full Babel fixture comparison
node test/run-babel-fixtures.js
```

## Questions?

Open a [Discussion](https://github.com/your-org/flowjs/discussions) or ask in
the PR comments. We're happy to help you get started.