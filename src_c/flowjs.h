/**
 * flowjs.c — core type and value model.
 *
 * The whole compiler (lexer → parser → AST → transformer → emitter) is a
 * single-pass, arena-allocated pipeline written in C23.  Strings are
 * length-prefixed slices so JS source can be embedded without copying.
 */
#ifndef FLOWJS_C_H
#define FLOWJS_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- type aliases ------------------------------------------------------- */

typedef int8_t   fjs_i8;
typedef int16_t  fjs_i16;
typedef int32_t  fjs_i32;
typedef int64_t  fjs_i64;
typedef uint8_t  fjs_u8;
typedef uint16_t fjs_u16;
typedef uint32_t fjs_u32;
typedef uint64_t fjs_u64;

/* ---- source slice ------------------------------------------------------- */

typedef struct {
  const char *ptr; /* not NUL-terminated in general */
  size_t len;
} fjs_slice;

#define FJS_SLICE(str)             ((fjs_slice){(str), sizeof(str) - 1})
#define FJS_SLICE_EMPTY            ((fjs_slice){NULL, 0})

static inline bool fjs_slice_eq(fjs_slice a, fjs_slice b) {
  return a.len == b.len && (a.len == 0 || __builtin_memcmp(a.ptr, b.ptr, a.len) == 0);
}

static inline fjs_slice fjs_slice_cstr(const char *s) {
  return (fjs_slice){s, __builtin_strlen(s)};
}

/* ---- string builder ----------------------------------------------------- */

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} fjs_buf;

void fjs_buf_init(fjs_buf *b);
void fjs_buf_grow(fjs_buf *b, size_t extra);
void fjs_buf_append(fjs_buf *b, const char *data, size_t len);
void fjs_buf_putc(fjs_buf *b, char c);
void fjs_buf_puts(fjs_buf *b, const char *s);
void fjs_buf_printf(fjs_buf *b, const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
void fjs_buf_free(fjs_buf *b);

/* ---- diagnostics -------------------------------------------------------- */

typedef enum {
  FJS_ERR_NONE = 0,
  FJS_ERR_OOM,
  FJS_ERR_SYNTAX,
  FJS_ERR_IO,
} fjs_error_kind;

typedef struct {
  fjs_error_kind kind;
  char message[512];
} fjs_error;

#define FJS_ERROR_OK ((fjs_error){FJS_ERR_NONE, {0}})

static inline void fjs_error_set(fjs_error *e, fjs_error_kind k, const char *msg) {
  if (e != NULL) {
    e->kind = k;
    __builtin_snprintf(e->message, sizeof(e->message), "%s", msg);
  }
}

/* ---- source location ---------------------------------------------------- */

typedef struct {
  const char *file; /* borrowed or NULL */
  size_t ln;        /* 1-based */
  size_t col;       /* 1-based */
} fjs_loc;

/* ---- token kinds -------------------------------------------------------- */

typedef enum {
  FJS_TK_EOF = 0,
  FJS_TK_INVALID,
  FJS_TK_NUM, FJS_TK_STRING, FJS_TK_REGEX,
  FJS_TK_TEMPLATE, FJS_TK_TEMPLATE_TAIL,
  FJS_TK_IDENT, FJS_TK_PRIVATE_NAME,
  FJS_TK_KW,
  FJS_TK_LPAREN, FJS_TK_RPAREN,
  FJS_TK_LBRACE, FJS_TK_RBRACE,
  FJS_TK_LBRACKET, FJS_TK_RBRACKET,
  FJS_TK_SEMI, FJS_TK_COMMA, FJS_TK_DOT, FJS_TK_ELLIPSIS,
  FJS_TK_LT, FJS_TK_GT, FJS_TK_LE, FJS_TK_GE,
  FJS_TK_EQ, FJS_TK_NE, FJS_TK_STRICT_EQ, FJS_TK_STRICT_NE,
  FJS_TK_PLUS, FJS_TK_MINUS, FJS_TK_STAR, FJS_TK_SLASH, FJS_TK_PERCENT,
  FJS_TK_STAR_STAR,
  FJS_TK_INCR, FJS_TK_DECR,
  FJS_TK_SHL, FJS_TK_SHR, FJS_TK_USHR,
  FJS_TK_AMP, FJS_TK_PIPE, FJS_TK_CARET, FJS_TK_TILDE, FJS_TK_BANG,
  FJS_TK_AND, FJS_TK_OR, FJS_TK_NULLISH,
  FJS_TK_QUESTION, FJS_TK_COLON,
  FJS_TK_ASSIGN, FJS_TK_PLUS_ASSIGN, FJS_TK_MINUS_ASSIGN,
  FJS_TK_STAR_ASSIGN, FJS_TK_SLASH_ASSIGN, FJS_TK_PERCENT_ASSIGN,
  FJS_TK_STAR_STAR_ASSIGN, FJS_TK_SHL_ASSIGN, FJS_TK_SHR_ASSIGN,
  FJS_TK_USHR_ASSIGN, FJS_TK_AMP_ASSIGN, FJS_TK_PIPE_ASSIGN,
  FJS_TK_CARET_ASSIGN, FJS_TK_AND_ASSIGN, FJS_TK_OR_ASSIGN,
  FJS_TK_NULLISH_ASSIGN,
  FJS_TK_ARROW,
  FJS_TK_QUESTION_DOT,
  FJS_TK_OPT_CHAIN,
  FJS_TK_AT, FJS_TK_HASH,
  FJS_TK_BACKTICK,
  FJS_TK_DOLLAR,
} fjs_tk_kind;

typedef enum {
  FJS_KW_FN=1, FJS_KW_RETURN, FJS_KW_IF, FJS_KW_ELSE, FJS_KW_FOR,
  FJS_KW_WHILE, FJS_KW_DO, FJS_KW_SWITCH, FJS_KW_CASE, FJS_KW_DEFAULT0,
  FJS_KW_BREAK, FJS_KW_CONTINUE, FJS_KW_VAR, FJS_KW_LET, FJS_KW_CONST,
  FJS_KW_NEW, FJS_KW_DELETE, FJS_KW_TYPEOF, FJS_KW_VOID, FJS_KW_IN,
  FJS_KW_INSTANCEOF, FJS_KW_THIS, FJS_KW_SUPER, FJS_KW_CLASS, FJS_KW_EXTENDS,
  FJS_KW_STATIC, FJS_KW_GET, FJS_KW_SET, FJS_KW_ASYNC, FJS_KW_AWAIT,
  FJS_KW_YIELD, FJS_KW_EXPORT, FJS_KW_IMPORT, FJS_KW_FROM, FJS_KW_AS,
  FJS_KW_TRUE, FJS_KW_FALSE, FJS_KW_NULL, FJS_KW_TRY,
  FJS_KW_CATCH, FJS_KW_FINALLY, FJS_KW_THROW, FJS_KW_DEBUGGER,
  FJS_KW_ENUM, FJS_KW_WITH, FJS_KW_NATIVE,
  FJS_KW_OF,
} fjs_kw;

typedef struct {
  fjs_tk_kind kind;
  fjs_kw kw;
  fjs_slice txt;
  fjs_slice value;
  fjs_loc loc;
  bool template_head;
} fjs_token;

/* ---- lexer state -------------------------------------------------------- */

typedef struct {
  const char *file;
  const char *src;
  size_t len;
  const char *p;
  size_t ln, col;
  fjs_token tok;
  bool prev_was_regex_ok;
  fjs_error err;
} fjs_lexer;

void fjs_lexer_init(fjs_lexer *lx, const char *file, const char *src, size_t len);
void fjs_lexer_free(fjs_lexer *lx);
fjs_tk_kind fjs_lexer_next(fjs_lexer *lx);
fjs_tk_kind fjs_lexer_resume_template(fjs_lexer *lx);
bool fjs_lexer_regex_allowed(fjs_lexer *lx);

/* ---- AST types ---------------------------------------------------------- */

/* Forward declarations */
typedef struct fjs_ast_list_node fjs_ast_list_node;
typedef struct fjs_ast_list fjs_ast_list;
typedef struct fjs_ast fjs_ast;

struct fjs_ast_list_node {
  fjs_ast *item;
  fjs_ast_list_node *next;
};

struct fjs_ast_list {
  fjs_ast_list_node *head;
  fjs_ast_list_node *tail;
  size_t count;
};

typedef enum {
  AST_PROGRAM, AST_EXPR_STMT, AST_BLOCK, AST_EMPTY, AST_DEBUGGER,
  AST_VAR_DECL, AST_VAR_DECLARATOR,
  AST_FN_DECL, AST_CLASS_DECL, AST_RETURN, AST_IF, AST_FOR, AST_FOR_IN,
  AST_FOR_OF, AST_WHILE, AST_DO_WHILE, AST_SWITCH, AST_SWITCH_CASE,
  AST_BREAK, AST_CONTINUE, AST_LABELED, AST_THROW, AST_TRY, AST_WITH,
  AST_IMPORT_DECL, AST_EXPORT_NAMED, AST_EXPORT_DEFAULT, AST_EXPORT_ALL,
  AST_IDENT, AST_NUM_LITERAL, AST_STRING_LITERAL, AST_BOOL_LITERAL,
  AST_NULL_LITERAL, AST_REGEX_LITERAL, AST_TEMPLATE_LITERAL,
  AST_ARRAY_EXPR, AST_OBJECT_EXPR, AST_PROPERTY,
  AST_FN_EXPR, AST_CLASS_EXPR, AST_ARROW_FN, AST_UNARY_EXPR,
  AST_UPDATE_EXPR, AST_BINARY_EXPR, AST_LOGICAL_EXPR, AST_ASSIGN_EXPR,
  AST_COND_EXPR, AST_CALL_EXPR, AST_NEW_EXPR, AST_MEMBER_EXPR,
  AST_OPTIONAL_MEMBER, AST_OPTIONAL_CALL, AST_SEQ_EXPR,
  AST_SPREAD, AST_YIELD, AST_AWAIT, AST_PAREN,
  AST_OBJECT_PATTERN, AST_ARRAY_PATTERN, AST_ASSIGN_PATTERN,
  AST_REST_ELEMENT, AST_DECORATOR,
} fjs_ast_kind;

struct fjs_ast {
  fjs_ast_kind kind;
  fjs_loc loc;
  fjs_ast *a, *b, *c, *d;
  fjs_ast_list list;
  fjs_slice txt;
  double num;
  bool flag;
  fjs_kw kw;
};

typedef struct {
  fjs_ast *root;
  fjs_error err;
} fjs_parse_result;

/* ---- arena / builder state ---------------------------------------------- */

typedef struct fjs_arena fjs_arena;
typedef struct {
  fjs_arena *arena;
  fjs_error err;
} fjs_ab;

/* prototypes */
fjs_arena *fjs_arena_new(void);
void      *fjs_arena_alloc(fjs_arena *a, size_t sz);
void       fjs_arena_free(fjs_arena *a);
void       fjs_ab_init(fjs_ab *b);
fjs_ast   *fjs_make_ast(fjs_ab *b, fjs_ast_kind kind);
void       fjs_list_push(fjs_ab *b, fjs_ast_list *list, fjs_ast *item);
void       fjs_list_push_front(fjs_ab *b, fjs_ast_list *list, fjs_ast *item);

/* ---- number formatting -------------------------------------------------- */

void fjs_double_to_js(fjs_buf *b, double x);

/* ---- parse entry point -------------------------------------------------- */

fjs_parse_result fjs_parse_source(const char *file, const char *src, size_t len);

/* ---- transform ---------------------------------------------------------- */

typedef struct {
  bool modules;
  bool arrows;
  bool let_const;
  bool templates;
  bool optional_chaining;
  bool esm_to_cjs;
  bool emit_helpers;
} fjs_transform_opts;

extern const fjs_transform_opts FJS_TRANSFORM_DEFAULTS;
fjs_error fjs_transform(fjs_ast *root, fjs_transform_opts opts);

/* ---- emit --------------------------------------------------------------- */

typedef struct {
  bool source_maps;
  bool minify;
} fjs_emit_opts;

fjs_error fjs_emit(fjs_ast *root, fjs_emit_opts opts, fjs_buf *out);

/* ---- JSON serialization ------------------------------------------------- */

fjs_error fjs_ast_to_json(fjs_ast *root, fjs_buf *out);
fjs_error fjs_ast_to_json_pretty(fjs_ast *root, fjs_buf *out);

/* ---- napi helper -------------------------------------------------------- */

fjs_error fjs_parse(const char *file, const char *src, size_t len, fjs_ast **out_root);

#ifdef __cplusplus
}
#endif

#endif /* FLOWJS_C_H */