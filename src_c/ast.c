/* flowjs AST -- JSON serializer (uses arena from arena.c) */
#include "flowjs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void json_type(fjs_ast_kind k, fjs_buf *b) {
  static const char *names[] = {
    "Program","ExpressionStatement","BlockStatement","EmptyStatement",
    "DebuggerStatement","VariableDeclaration","VariableDeclarator",
    "FunctionDeclaration","ClassDeclaration","ReturnStatement","IfStatement",
    "ForStatement","ForInStatement","ForOfStatement","WhileStatement",
    "DoWhileStatement","SwitchStatement","SwitchCase","BreakStatement",
    "ContinueStatement","LabeledStatement","ThrowStatement","TryStatement",
    "WithStatement","ImportDeclaration","ExportNamedDeclaration",
    "ExportDefaultDeclaration","ExportAllDeclaration","Identifier",
    "NumericLiteral","StringLiteral","BooleanLiteral","NullLiteral",
    "RegExpLiteral","TemplateLiteral","ArrayExpression","ObjectExpression",
    "Property","FunctionExpression","ClassExpression",
    "ArrowFunctionExpression","UnaryExpression","UpdateExpression",
    "BinaryExpression","LogicalExpression","AssignmentExpression",
    "ConditionalExpression","CallExpression","NewExpression",
    "MemberExpression","OptionalMemberExpression","OptionalCallExpression",
    "SequenceExpression","SpreadElement","YieldExpression","AwaitExpression",
    "ParenthesizedExpression","ObjectPattern","ArrayPattern",
    "AssignmentPattern","RestElement","Decorator",
  };
  if (k < (fjs_ast_kind)(sizeof(names)/sizeof(names[0])) && names[k])
    fjs_buf_puts(b, names[k]);
  else
    fjs_buf_printf(b, "UNKNOWN_%d", (int)k);
}

static void json_esc(fjs_buf *b, fjs_slice s) {
  fjs_buf_putc(b, '"');
  for (size_t i = 0; i < s.len; i++) {
    unsigned char c = s.ptr[i];
    if (c == '"' || c == '\\') { fjs_buf_putc(b, '\\'); fjs_buf_putc(b, c); }
    else if (c < 0x20) fjs_buf_printf(b, "\\u%04x", c);
    else fjs_buf_putc(b, c);
  }
  fjs_buf_putc(b, '"');
}

static void ser(fjs_ast *n, fjs_buf *b);
static void ser_list(fjs_ast_list *l, fjs_buf *b);

static void ser(fjs_ast *n, fjs_buf *b) {
  if (!n) { fjs_buf_puts(b, "null"); return; }
  fjs_buf_putc(b, '{');
  fjs_buf_puts(b, "\"type\":\""); json_type(n->kind, b); fjs_buf_putc(b, '"');

  if (n->loc.file) {
    fjs_buf_printf(b, ",\"loc\":{\"file\":");
    json_esc(b, fjs_slice_cstr(n->loc.file));
    fjs_buf_printf(b, ",\"line\":%zu,\"col\":%zu}", n->loc.ln, n->loc.col);
  }

#define JFIELD(nm, expr) do { fjs_buf_puts(b, ",\"" nm "\":"); expr; } while(0)
#define JFIELD_STR(nm, sl) do { fjs_buf_puts(b, ",\"" nm "\":"); json_esc(b, sl); } while(0)

  switch (n->kind) {
    case AST_PROGRAM: JFIELD("body", ser_list(&n->list, b)); break;
    case AST_EXPR_STMT: JFIELD("expression", ser(n->a, b)); break;
    case AST_BLOCK: JFIELD("body", ser_list(&n->list, b)); break;
    case AST_EMPTY: break;
    case AST_DEBUGGER: break;
    case AST_VAR_DECL:
      JFIELD("declarations", ser_list(&n->list, b));
      JFIELD_STR("kind", n->txt);
      break;
    case AST_VAR_DECLARATOR:
      JFIELD("id", ser(n->a, b));
      JFIELD("init", ser(n->b, b));
      break;
    case AST_FN_DECL: case AST_FN_EXPR: case AST_ARROW_FN:
      if (n->txt.len) JFIELD_STR("id", n->txt);
      JFIELD("params", ser_list(&n->list, b));
      JFIELD("body", ser(n->a, b));
      if (n->kind == AST_ARROW_FN)
        fjs_buf_printf(b, ",\"expression\":%s", n->flag ? "true" : "false");
      else
        fjs_buf_printf(b, ",\"generator\":%s", n->flag ? "true" : "false");
      break;
    case AST_CLASS_DECL:
      JFIELD("id", ser(n->a, b));
      JFIELD("superClass", ser(n->b, b));
      JFIELD("body", ser_list(&n->list, b));
      break;
    case AST_RETURN: JFIELD("argument", ser(n->a, b)); break;
    case AST_IF:
      JFIELD("test", ser(n->a, b));
      JFIELD("consequent", ser(n->b, b));
      JFIELD("alternate", ser(n->c, b));
      break;
    case AST_FOR: case AST_FOR_IN: case AST_FOR_OF:
      JFIELD("init", ser(n->a, b));
      JFIELD("test", ser(n->b, b));
      JFIELD("update", ser(n->c, b));
      JFIELD("body", ser(n->d, b));
      break;
    case AST_WHILE: JFIELD("test", ser(n->a, b)); JFIELD("body", ser(n->b, b)); break;
    case AST_DO_WHILE: JFIELD("body", ser(n->a, b)); JFIELD("test", ser(n->b, b)); break;
    case AST_SWITCH: JFIELD("discriminant", ser(n->a, b)); JFIELD("cases", ser_list(&n->list, b)); break;
    case AST_SWITCH_CASE: JFIELD("test", ser(n->a, b)); JFIELD("consequent", ser_list(&n->list, b)); break;
    case AST_BREAK: case AST_CONTINUE: JFIELD("label", ser(n->a, b)); break;
    case AST_LABELED: JFIELD_STR("label", n->txt); JFIELD("body", ser(n->a, b)); break;
    case AST_THROW: JFIELD("argument", ser(n->a, b)); break;
    case AST_TRY: JFIELD("block", ser(n->a, b)); JFIELD("handler", ser(n->b, b)); JFIELD("finalizer", ser(n->c, b)); break;
    case AST_WITH: JFIELD("object", ser(n->a, b)); JFIELD("body", ser(n->b, b)); break;
    case AST_IMPORT_DECL: JFIELD("source", ser(n->a, b)); JFIELD("specifiers", ser_list(&n->list, b)); break;
    case AST_EXPORT_NAMED: JFIELD("declaration", ser(n->a, b)); JFIELD("specifiers", ser_list(&n->list, b)); break;
    case AST_EXPORT_DEFAULT: JFIELD("declaration", ser(n->a, b)); break;
    case AST_EXPORT_ALL: JFIELD("source", ser(n->a, b)); break;
    case AST_IDENT: JFIELD_STR("name", n->txt); break;
    case AST_NUM_LITERAL: JFIELD("value", fjs_double_to_js(b, n->num)); break;
    case AST_STRING_LITERAL: JFIELD_STR("value", n->txt); break;
    case AST_BOOL_LITERAL: fjs_buf_printf(b, ",\"value\":%s", n->flag?"true":"false"); break;
    case AST_NULL_LITERAL: fjs_buf_puts(b, ",\"value\":null"); break;
    case AST_REGEX_LITERAL: JFIELD_STR("value", n->txt); break;
    case AST_TEMPLATE_LITERAL: JFIELD("parts", ser_list(&n->list, b)); break;
    case AST_ARRAY_EXPR: JFIELD("elements", ser_list(&n->list, b)); break;
    case AST_OBJECT_EXPR: JFIELD("properties", ser_list(&n->list, b)); break;
    case AST_PROPERTY:
      JFIELD_STR("kind", n->txt);
      JFIELD("key", ser(n->a, b));
      JFIELD("value", ser(n->b, b));
      fjs_buf_printf(b, ",\"shorthand\":%s", n->flag?"true":"false");
      break;
    case AST_UNARY_EXPR: JFIELD_STR("operator", n->txt); JFIELD("argument", ser(n->a, b)); fjs_buf_puts(b, ",\"prefix\":true"); break;
    case AST_UPDATE_EXPR: JFIELD_STR("operator", n->txt); JFIELD("argument", ser(n->a, b)); fjs_buf_printf(b, ",\"prefix\":%s", n->flag?"true":"false"); break;
    case AST_BINARY_EXPR: case AST_LOGICAL_EXPR: case AST_ASSIGN_EXPR:
      JFIELD_STR("operator", n->txt);
      JFIELD("left", ser(n->a, b));
      JFIELD("right", ser(n->b, b));
      break;
    case AST_COND_EXPR:
      JFIELD("test", ser(n->a, b));
      JFIELD("consequent", ser(n->b, b));
      JFIELD("alternate", ser(n->c, b));
      break;
    case AST_CALL_EXPR: case AST_NEW_EXPR: case AST_OPTIONAL_CALL:
      JFIELD("callee", ser(n->a, b));
      JFIELD("arguments", ser_list(&n->list, b));
      break;
    case AST_MEMBER_EXPR: case AST_OPTIONAL_MEMBER:
      JFIELD("object", ser(n->a, b));
      JFIELD("property", ser(n->b, b));
      fjs_buf_printf(b, ",\"computed\":%s", n->flag?"true":"false");
      break;
    case AST_SEQ_EXPR: JFIELD("expressions", ser_list(&n->list, b)); break;
    case AST_SPREAD: JFIELD("argument", ser(n->a, b)); break;
    case AST_YIELD: case AST_AWAIT: JFIELD("argument", ser(n->a, b)); break;
    case AST_PAREN: JFIELD("expression", ser(n->a, b)); break;
    case AST_OBJECT_PATTERN: case AST_ARRAY_PATTERN: JFIELD("elements", ser_list(&n->list, b)); break;
    case AST_ASSIGN_PATTERN: JFIELD("left", ser(n->a, b)); JFIELD("right", ser(n->b, b)); break;
    case AST_REST_ELEMENT: JFIELD("argument", ser(n->a, b)); break;
    case AST_DECORATOR: JFIELD("expression", ser(n->a, b)); break;
  }
  fjs_buf_putc(b, '}');
}

static void ser_list(fjs_ast_list *l, fjs_buf *b) {
  fjs_buf_putc(b, '[');
  bool first = true;
  for (fjs_ast_list_node *ln = l->head; ln; ln = ln->next) {
    if (!first) fjs_buf_putc(b, ',');
    first = false;
    ser(ln->item, b);
  }
  fjs_buf_putc(b, ']');
}

fjs_error fjs_ast_to_json(fjs_ast *root, fjs_buf *out) {
  ser(root, out);
  return FJS_ERROR_OK;
}

fjs_error fjs_ast_to_json_pretty(fjs_ast *root, fjs_buf *out) {
  ser(root, out);
  return FJS_ERROR_OK;
}

fjs_error fjs_parse(const char *file, const char *src, size_t len, fjs_ast **out_root) {
  (void)file; (void)src; (void)len;
  *out_root = NULL;
  return FJS_ERROR_OK;
}