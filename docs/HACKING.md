# Hacking on flowjs — C23 Coding Guide

## Philosophy

flowjs is written in **C23 with no external dependencies** (except the Node.js N-API header).
Every line of C should be readable by a competent C programmer without reference to Babel internals.

## Coding Conventions

### Naming

| Pattern | Rule | Example |
|---------|------|---------|
| `fjs_*` (files) | All source files prefixed `fjs_*` in the `flowjs` namespace | `fjs_lexer_init()` |
| `snake_case` | Functions, variables, struct fields | `fjs_parse_result`, `source_len` |
| `UPPER_CASE` | Enums and constants | `FJS_TK_EOF`, `FJS_ERR_SYNTAX` |
| `CAMEL_CASE` | (avoid) Only for JS-facing names | `transformFile` in addon.c |

### Memory management

1. **Use the arena for AST nodes.** Never `malloc` a `fjs_ast` or `fjs_ast_list_node`.
2. **Free the arena, not individual nodes.** One call to `fjs_arena_free()` cleans up.
3. **The source buffer must outlive the AST.** The N-API addon manages this.
4. **`fjs_buf` is the only dynamic allocation.** It grows as needed. Free with `fjs_buf_free()`.

### Error handling

Errors bubble up through `fjs_error` structs:

```c
typedef struct {
  fjs_error_kind kind;   // FJS_ERR_NONE, FJS_ERR_OOM, FJS_ERR_SYNTAX, FJS_ERR_IO
  char message[512];     // human-readable error message
} fjs_error;

#define FJS_ERROR_OK ((fjs_error){FJS_ERR_NONE, {0}})
```

The lexer, parser, and builder (`fjs_ab`) all have error fields. The top-level
`fjs_parse_source()` checks all of them.

### Testing

- Unit tests go in `test/` as JS scripts
- Run them with `node test/<name>.js`
- The Babel fixture runner tests against real Babel outputs:
  ```
  node test/run-babel-fixtures.js
  ```

## Adding a new AST node type

1. Add an `AST_*` value to `fjs_ast_kind` enum in `flowjs.h`
2. Add a `case` in the `ser()` function in `ast.c` to serialize it to JSON
3. Add parsing logic in `parser.c`
4. Add the type name to `json_type()` in `ast.c`
5. Test with a JS snippet

## Adding a new keyword

1. Add a `FJS_KW_*` value to `fjs_kw` enum in `flowjs.h`
2. Add the string → keyword mapping in `lexer.c`'s `lookup_keyword()` function
3. Add parsing logic in `parser.c`
4. Add serialization in `ast.c`

## Adding a new token type

1. Add a `FJS_TK_*` value to `fjs_tk_kind` enum in `flowjs.h`
2. Add scanning logic in `lexer.c`'s main `next` function
3. Add the token to the regex-prevents-ok switch at the bottom of `next()`
4. Add parsing logic in `parser.c`

## Debugging

### C-level debugging

```bash
# Compile a standalone test binary
gcc -std=c2x -Isrc_c -o /tmp/fjs_test \
  src_c/arena.c src_c/buf.c src_c/lexer.c src_c/parser.c src_c/ast.c \
  my_test.c -Wall -Wextra
/tmp/fjs_test

# Or with GDB
gdb --args /tmp/fjs_test
```

### N-API debugging

```bash
# See the native module symbols
nm -D build/Release/flowjs_native.node | grep fjs_

# Run a single test with Node.js debug output
NODE_DEBUG=napi node test/smoke.js
```

## Key files and what they do

| File | What to find there |
|------|-------------------|
| `flowjs.h` | Core types: `fjs_slice`, `fjs_token`, `fjs_ast`, `fjs_lexer`, enums |
| `lexer.c` | The `next()` function is the heart — a 600-line character-by-character tokenizer |
| `parser.c` | `parse_stmt()` and `parse_expr()` are the main entry points. Pratt parsing in `parse_binary()` |
| `ast.c` | `ser()` function serializes each AST node kind to JSON |
| `addon.c` | N-API callback functions. Call `napi_get_cb_info()` to read JS arguments |

## C23 portability notes

- Use `-std=c2x` for GCC (C23 is still being finalized)
- Use `-std=c23` for Clang 17+
- Use `__builtin_strlen()` / `__builtin_memcmp()` for inlined libc calls
- Use `int64_t` / `uint64_t` from `<stdint.h>` for fixed-size integers
- Boolean is `bool` from `<stdbool.h>`
- Null pointer is `nullptr` (no need for `NULL` macro)